#include "build_config.h"
#include "config.h"
#include "logging.h"
#include "page_template.h"
#include "server.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum
{
	OPT_CHECK = 256,
	OPT_LISTEN,
	OPT_THREADS,
	OPT_VERSION,
	OPT_ACCESS_LOG,
	OPT_ERROR_LOG
};
static const struct option command_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"check-config", no_argument, NULL, OPT_CHECK},
	{"listen", required_argument, NULL, OPT_LISTEN},
	{"threads", required_argument, NULL, OPT_THREADS},
	{"access-log", required_argument, NULL, OPT_ACCESS_LOG},
	{"error-log", required_argument, NULL, OPT_ERROR_LOG},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, OPT_VERSION},
	{NULL, 0, NULL, 0},
};

static void usage(FILE *output)
{
	fputs("Usage: stories [options]\n"
	      "\n"
	      "Serve Markdown documentation over HTTP.\n"
	      "\n"
	      "Options:\n"
	      "  -c, --config PATH          Read configuration from PATH.\n"
	      "      --listen ADDRESS:PORT  Override the configured listening address.\n"
	      "      --threads COUNT        Override the configured worker count (1-1024).\n"
	      "      --access-log PATH      Append access logs to PATH (default: stdout).\n"
	      "      --error-log PATH       Append error logs to PATH (default: stderr).\n"
	      "      --check-config         Validate configuration, roots, and page template, then exit.\n"
	      "  -h, --help                 Show this help and exit.\n"
	      "      --version              Show the version and exit.\n"
	      "\n"
	      "Without --config, check ./stories.conf, then /etc/stories.conf; exit with an error if neither exists.\n"
	      "Configuration defaults: listen=127.0.0.1:8080, threads=8.\n",
	      output);
}

static bool override_log_path(char **destination, const char *path)
{
	char absolute[STORIES_PATH_LIMIT], cwd[STORIES_PATH_LIMIT];
	if (!*path)
	{
		logging_error(NULL, "log path must not be empty");
		return false;
	}
	if (*path != '/')
	{
		if (!getcwd(cwd, sizeof(cwd)))
		{
			logging_error(NULL, "cannot resolve log path: %s", strerror(errno));
			return false;
		}
		int length = snprintf(absolute, sizeof(absolute), "%s/%s", cwd, path);
		if (length < 0 || (size_t)length >= sizeof(absolute))
		{
			logging_error(NULL, "log path is too long");
			return false;
		}
		path = absolute;
	}
	if (strlen(path) >= STORIES_PATH_LIMIT)
	{
		logging_error(NULL, "log path is too long");
		return false;
	}
	char *copy = strdup(path);
	if (!copy)
	{
		logging_error(NULL, "cannot allocate log path");
		return false;
	}
	free(*destination);
	*destination = copy;
	return true;
}

int main(int argc, char **argv)
{
	const char *config_path = NULL, *listen = NULL, *threads = NULL;
	const char *access_log = NULL, *error_log = NULL;
	bool        check = false;
	int         option;
	while ((option = getopt_long(argc, argv, "c:h", command_options, NULL)) != -1)
	{
		switch (option)
		{
		case 'c':
			config_path = optarg;
			break;
		case 'h':
			usage(stdout);
			return 0;
		case OPT_VERSION:
			puts("stories " STORIES_VERSION);
			return 0;
		case OPT_CHECK:
			check = true;
			break;
		case OPT_LISTEN:
			listen = optarg;
			break;
		case OPT_ACCESS_LOG:
			access_log = optarg;
			break;
		case OPT_ERROR_LOG:
			error_log = optarg;
			break;
		case OPT_THREADS:
			threads = optarg;
			break;
		default:
			usage(stderr);
			return 1;
		}
	}
	if (optind != argc)
	{
		usage(stderr);
		return 1;
	}
	struct configuration configuration;
	char                 error[512];
	if (!configuration_load(config_path, &configuration, error, sizeof(error)))
	{
		logging_error(NULL, "%s", error);
		return 1;
	}
	struct logging  logging = {0};
	struct logging *logger = NULL;
	int             result = 1;
	if (listen)
	{
		if (!configuration_listen_valid(listen))
		{
			logging_error(logger, "invalid --listen endpoint");
			goto done;
		}
		char *copy = strdup(listen);
		if (!copy)
			goto done;
		free(configuration.listen);
		configuration.listen = copy;
	}
	if (threads)
	{
		size_t number;
		if (!configuration_unsigned(threads, 1024, &number) || !number)
		{
			logging_error(logger, "--threads must be between 1 and 1024");
			goto done;
		}
		configuration.threads = (unsigned)number;
	}
	if ((access_log && !override_log_path(&configuration.access_log_path, access_log)) || (error_log && !override_log_path(&configuration.error_log_path, error_log)))
		goto done;
	if (!check)
	{
		if (!logging_open(&logging, &configuration, error, sizeof(error)))
		{
			logging_error(NULL, "%s", error);
			goto done;
		}
		logger = &logging;
	}
	if (!page_template_check(&configuration, error, sizeof(error)))
	{
		logging_error(logger, "%s", error);
		goto done;
	}
	if (check)
	{
		if (!server_check_roots(&configuration, error, sizeof(error)))
			logging_error(logger, "%s", error);
		else
		{
			puts("Configuration OK");
			result = 0;
		}
	}
	else
		result = server_run(&configuration, logger);

done:
	logging_close(&logging);
	configuration_free(&configuration);
	return result;
}
