#include "server.h"
#include "document.h"
#include "logging.h"
#include "render.h"
#include "url.h"

#include <civetweb.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct server
{
	const struct configuration *configuration;
	int                         content_fd;
	struct mg_context          *context;
	struct logging             *logging;
};

static bool path_contains(const char *parent, const char *child)
{
	size_t size = strlen(parent);
	return !strncmp(parent, child, size) && (size == 1 || child[size] == '/' || child[size] == '\0');
}

static bool resolve_roots(const struct configuration *configuration, char *content, char *assets, char *error, size_t error_size)
{
	if (!realpath(configuration->content_root, content) || !realpath(configuration->asset_root, assets))
	{
		snprintf(error, error_size, "cannot resolve content/asset roots: %s", strerror(errno));
		return false;
	}
	if (path_contains(assets, content) || path_contains(content, assets))
	{
		snprintf(error, error_size, "content and asset roots must be separate, nonoverlapping directories");
		return false;
	}
	const char *paths[] = {content, assets};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
	{
		int fd = open(paths[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		if (fd < 0)
		{
			snprintf(error, error_size, "cannot open root %s: %s", paths[i], strerror(errno));
			return false;
		}
		close(fd);
	}
	return true;
}

bool server_check_roots(const struct configuration *configuration, char *error, size_t error_size)
{
	char content[STORIES_PATH_LIMIT], assets[STORIES_PATH_LIMIT];
	return resolve_roots(configuration, content, assets, error, error_size);
}

static int send_response(struct mg_connection *connection, int status, const char *reason, const char *extra_headers, const struct buffer *body)
{
	const struct mg_request_info *request = mg_get_request_info(connection);
	char                          date[64];
	time_t                        now = time(NULL);
	struct tm                     utc;
	gmtime_r(&now, &utc);
	strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &utc);
	if (mg_printf(connection, "HTTP/1.1 %d %s\r\nDate: %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n%s\r\n", status, reason, date, body->size, extra_headers ? extra_headers : "") < 0)
		return status;
	if (strcmp(request->request_method, "HEAD"))
	{
		size_t sent = 0;
		while (sent < body->size)
		{
			size_t chunk = body->size - sent;
			if (chunk > 65536)
				chunk = 65536;
			int written = mg_write(connection, body->data + sent, chunk);
			if (written <= 0)
				break;
			sent += (size_t)written;
		}
	}
	return status;
}

static int send_error(struct mg_connection *connection, const struct server *server, int status, const char *title, const char *message)
{
	struct buffer body;
	if (!render_error(server->configuration, title, message, &body))
	{
		logging_error(server->logging, "cannot render error page for status %d", status);
		buffer_free(&body);
		buffer_init(&body, 64);
		buffer_puts(&body, "Request failed.\n");
	}
	int result = send_response(connection, status, title, status == 405 ? "Allow: GET, HEAD\r\n" : NULL, &body);
	buffer_free(&body);
	return result;
}

static int request_begin(struct mg_connection *connection)
{
	const struct mg_request_info *request = mg_get_request_info(connection);
	struct server                *server = request->user_data;
	const struct configuration   *configuration = server->configuration;
	if (strcmp(request->request_method, "GET") && strcmp(request->request_method, "HEAD"))
		return send_error(connection, server, 405, "Method Not Allowed", "Use GET or HEAD.");

	/* CivetWeb has decoded this path, but has not removed dot segments from
	 * local_uri_raw. Validate before using its cleaned path for static serving.
	 * The front proxy rejects %00; direct requests retain CivetWeb's truncation. */
	const char *path = request->local_uri_raw;
	if (!path || !url_path_valid(path, strlen(path)))
		return send_error(connection, server, 400, "Bad Request", "Invalid document path.");
	size_t path_size = strlen(path);
	bool   asset = !strncmp(path, "/assets/", 8);
	if (url_markdown_extension(path, path_size) || (!asset && url_path_has_extension(path, path_size)))
		return send_error(connection, server, 404, "Not Found", "No document exists at this URL.");
	if (asset)
	{
		/* Match CivetWeb's own MIME classification before an asset can
		 * serve a file. HTML pages belong to the Markdown renderer. */
		const char *mime = mg_get_builtin_mime_type(path);
		if (!strcmp(mime, "text/html") || !strcmp(mime, "application/xhtml+xml"))
			return send_error(connection, server, 404, "Not Found", "No document exists at this URL.");
		return 0;
	}
	if (!strcmp(path, "/assets"))
		return send_error(connection, server, 404, "Not Found", "No document exists at this URL.");

	struct document_route route;
	document_resolve(server->content_fd, configuration, path, path_size, &route);
	int result;
	if (route.status == ROUTE_REDIRECT)
	{
		struct buffer headers, body;
		buffer_init(&headers, 32768);
		buffer_init(&body, 0);
		buffer_puts(&headers, "Location: ");
		buffer_append(&headers, route.location.data, route.location.size);
		if (request->query_string && *request->query_string)
		{
			for (const unsigned char *p = (const unsigned char *)request->query_string; *p; p++)
				if (*p < 32 || *p == 127)
					headers.failed = true;
			buffer_puts(&headers, "?");
			buffer_puts(&headers, request->query_string);
		}
		buffer_puts(&headers, "\r\n");
		result = headers.failed ? send_error(connection, server, 400, "Bad Request", "Invalid redirect query.") : send_response(connection, 301, "Moved Permanently", headers.data, &body);
		buffer_free(&headers);
		buffer_free(&body);
	}
	else if (route.status == ROUTE_DOCUMENT)
	{
		struct buffer markdown, html = {0};
		bool          rendered = document_read(route.fd, configuration->markdown_limit, &markdown) && render_document(configuration, route.source, markdown.data, markdown.size, &html);
		buffer_free(&markdown);
		if (!rendered)
		{
			buffer_free(&html);
			logging_error(server->logging, "cannot read/render document: %s (I/O error, invalid link/template, or configured limit)", route.source);
			result = send_error(connection, server, 500, "Internal Server Error", "The document could not be rendered.");
		}
		else
			result = send_response(connection, 200, "OK", NULL, &html);
		buffer_free(&html);
	}
	else if (route.status == ROUTE_INVALID)
		result = send_error(connection, server, 400, "Bad Request", "Invalid document path.");
	else if (route.status == ROUTE_FAILURE)
	{
		logging_error(server->logging, "cannot resolve document: %s", path);
		result = send_error(connection, server, 500, "Internal Server Error", "The document could not be opened.");
	}
	else
		result = send_error(connection, server, 404, "Not Found", "No document exists at this URL.");
	document_route_free(&route);
	return result;
}

static int log_message(const struct mg_connection *connection, const char *message)
{
	struct server *server = connection ? mg_get_user_data(mg_get_context(connection)) : NULL;
	logging_error(server ? server->logging : NULL, "civetweb: %s", message);
	return 1;
}

static int log_access(const struct mg_connection *connection, const char *message)
{
	struct server *server = mg_get_user_data(mg_get_context(connection));
	logging_access(server->logging, message);
	/* Suppress CivetWeb's file writer because Stories owns the destination. */
	return 1;
}

static int http_error(struct mg_connection *connection, int status, const char *message)
{
	const struct mg_request_info *request = mg_get_request_info(connection);
	struct server                *server = mg_get_user_data(mg_get_context(connection));
	if (server && status >= 500)
		logging_error(server->logging, "civetweb HTTP %d: %s", status, message ? message : "request failed");
	/* Parsing errors can precede a usable request. Leave those to CivetWeb.
	 * For established requests, this also corrects CivetWeb 1.16's default HEAD
	 * error body while using the same bounded shell as document errors. */
	if (!request || !request->request_method || !server)
		return 1;
	const char *reason = mg_get_response_code_text(connection, status);
	send_error(connection, server, status, reason ? reason : "Request Failed", "The requested resource could not be served.");
	/* This callback's convention is the inverse of begin_request: zero means handled. */
	return 0;
}

int server_run(const struct configuration *configuration, struct logging *logging)
{
	struct server server = {.configuration = configuration, .content_fd = -1, .logging = logging};
	char          content[STORIES_PATH_LIMIT], assets[STORIES_PATH_LIMIT];
	char          error[512], threads[32];
	struct buffer rewrites;
	bool          initialized = false;
	int           result = 1;
	sigset_t      signals, old_signals;
	bool          signals_blocked = false;
	buffer_init(&rewrites, STORIES_PATH_LIMIT * 3);
	if (!resolve_roots(configuration, content, assets, error, sizeof(error)))
	{
		logging_error(logging, "%s", error);
		goto done;
	}
	if (strchr(assets, ',') || strchr(assets, '='))
	{
		logging_error(logging, "asset paths cannot contain comma or equals (CivetWeb rewrite delimiters)");
		goto done;
	}
	buffer_puts(&rewrites, "/assets/=");
	buffer_puts(&rewrites, assets);
	buffer_puts(&rewrites, "/");
	if (rewrites.failed)
	{
		logging_error(logging, "cannot build asset rewrite mapping");
		goto done;
	}
	server.content_fd = open(content, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (server.content_fd < 0)
	{
		logging_error(logging, "cannot open content root: %s", strerror(errno));
		goto done;
	}
	/* Block before creating workers so all of them inherit the signal mask.
	 * Only the main thread consumes termination signals and performs shutdown. */
	sigemptyset(&signals);
	/* Darwin's sigaddset macro triggers GCC's sign-conversion warning; use the POSIX function. */
	(sigaddset)(&signals, SIGINT);
	(sigaddset)(&signals, SIGTERM);
	(sigaddset)(&signals, SIGUSR1);
	int signal_error = pthread_sigmask(SIG_BLOCK, &signals, &old_signals);
	if (signal_error)
	{
		logging_error(logging, "cannot block server signals: %s", strerror(signal_error));
		goto done;
	}
	signals_blocked = true;
	struct sigaction ignore_pipe = {.sa_handler = SIG_IGN};
	sigemptyset(&ignore_pipe.sa_mask);
	if (sigaction(SIGPIPE, &ignore_pipe, NULL))
	{
		logging_error(logging, "cannot ignore SIGPIPE: %s", strerror(errno));
		goto done;
	}
	if (!(mg_init_library(MG_FEATURES_FILES) & MG_FEATURES_FILES))
	{
		logging_error(logging, "cannot initialize CivetWeb");
		goto done;
	}
	initialized = true;
	snprintf(threads, sizeof(threads), "%u", configuration->threads);
	const char *settings[80];
	size_t      count = 0;
	const char *required[] = {
		"listening_ports",
		configuration->listen,
		"num_threads",
		threads,
		"document_root",
		assets,
		"url_rewrite_patterns",
		rewrites.data,
		"enable_directory_listing",
		"no",
		"index_files",
		"",
		"enable_keep_alive",
		"no",
		"request_timeout_ms",
		"5000",
		"max_request_size",
		"16384",
		"decode_url",
		"yes",
		"decode_query_string",
		"no",
		"static_file_max_age",
		"0",
	};
	for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); i++)
		settings[count++] = required[i];
	/* Installed CivetWeb builds expose different optional script features.
	 * Disable every compiled-in script recognizer, without requiring those features. */
	static const char      *disabled_patterns[] = {"cgi_pattern", "cgi2_pattern", "cgi3_pattern", "cgi4_pattern", "ssi_pattern", "lua_script_pattern", "lua_server_page_pattern", "duktape_script_pattern"};
	const struct mg_option *valid = mg_get_valid_options();
	for (size_t i = 0; i < sizeof(disabled_patterns) / sizeof(disabled_patterns[0]); i++)
		for (const struct mg_option *option = valid; option->name; option++)
			if (!strcmp(option->name, disabled_patterns[i]))
			{
				settings[count++] = disabled_patterns[i];
				settings[count++] = "";
				break;
			}
	settings[count] = NULL;
	const struct mg_callbacks callbacks = {.begin_request = request_begin, .log_message = log_message, .log_access = log_access, .http_error = http_error};
	server.context = mg_start(&callbacks, &server, settings);
	if (!server.context)
	{
		logging_error(logging, "cannot start HTTP listener");
		goto done;
	}
	struct mg_server_port port;
	if (mg_get_server_ports(server.context, 1, &port) != 1)
	{
		logging_error(logging, "cannot determine HTTP listener port");
		goto done;
	}
	printf("Listening on %s (port %d)\n", configuration->listen, port.port);
	fflush(stdout);
	int signal_number;
	for (;;)
	{
		signal_error = sigwait(&signals, &signal_number);
		if (signal_error)
		{
			logging_error(logging, "cannot wait for signal: %s", strerror(signal_error));
			break;
		}
		if (signal_number != SIGUSR1)
		{
			result = 0;
			break;
		}
		if (!logging_reopen(logging, error, sizeof(error)))
			logging_error(logging, "%s; keeping existing logs", error);
		else
			logging_error(logging, "log files reopened");
	}

done:
	if (server.context)
		mg_stop(server.context);
	if (initialized)
		mg_exit_library();
	if (signals_blocked)
		pthread_sigmask(SIG_SETMASK, &old_signals, NULL);
	if (server.content_fd >= 0)
		close(server.content_fd);
	buffer_free(&rewrites);
	return result;
}
