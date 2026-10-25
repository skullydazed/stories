#include "config.h"
#include "buffer.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum option_kind
{
	OPTION_STRING,
	OPTION_PATH,
	OPTION_THREADS,
	OPTION_LIMIT,
	OPTION_ORDER
};
struct option_definition
{
	const char      *section;
	const char      *name;
	enum option_kind kind;
	size_t           offset;
};

static const struct option_definition options[] = {
	{"server", "listen", OPTION_STRING, offsetof(struct configuration, listen)},
	{"server", "threads", OPTION_THREADS, offsetof(struct configuration, threads)},
	{"content", "root", OPTION_PATH, offsetof(struct configuration, content_root)},
	{"content", "resolution_order", OPTION_ORDER, 0},
	{"content", "markdown_limit", OPTION_LIMIT, offsetof(struct configuration, markdown_limit)},
	{"content", "output_limit", OPTION_LIMIT, offsetof(struct configuration, output_limit)},
	{"site", "title", OPTION_STRING, offsetof(struct configuration, title)},
	{"assets", "root", OPTION_PATH, offsetof(struct configuration, asset_root)},
};

static char *trim(char *text)
{
	while (*text == ' ' || *text == '\t' || *text == '\r')
		text++;
	size_t size = strlen(text);
	while (size && (text[size - 1] == ' ' || text[size - 1] == '\t' || text[size - 1] == '\r'))
		text[--size] = '\0';
	return text;
}

bool configuration_unsigned(const char *text, size_t maximum, size_t *value)
{
	size_t result = 0;
	if (!*text)
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9' || (size_t)(*text - '0') > maximum || result > (maximum - (size_t)(*text - '0')) / 10)
			return false;
		result = result * 10 + (size_t)(*text - '0');
	}
	*value = result;
	return true;
}

bool configuration_listen_valid(const char *listen)
{
	/* One numeric HTTP endpoint only: no TLS suffixes, multiple listeners, or hostnames. */
	const char *port = strrchr(listen, ':');
	size_t      value;
	if (!port || port == listen || !configuration_unsigned(port + 1, 65535, &value))
		return false;
	char          host[INET6_ADDRSTRLEN];
	unsigned char address[sizeof(struct in6_addr)];
	const char   *start = listen;
	size_t        length = (size_t)(port - listen);
	int           family = AF_INET;
	if (*start == '[')
	{
		if (length < 3 || port[-1] != ']')
			return false;
		start++;
		length -= 2;
		family = AF_INET6;
	}
	if (!length || length >= sizeof(host))
		return false;
	memcpy(host, start, length);
	host[length] = '\0';
	return inet_pton(family, host, address) == 1;
}

static char *path_from(const char *base, const char *value)
{
	struct buffer path;
	buffer_init(&path, STORIES_PATH_LIMIT - 1);
	if (*value != '/')
	{
		buffer_puts(&path, base);
		buffer_puts(&path, "/");
	}
	buffer_puts(&path, value);
	/* CivetWeb's rewrite setting uses commas and equals as syntax. */
	if (path.failed || strchr(path.data, ',') || strchr(path.data, '='))
	{
		buffer_free(&path);
		return NULL;
	}
	return path.data;
}

void configuration_free(struct configuration *configuration)
{
	free(configuration->listen);
	free(configuration->content_root);
	free(configuration->asset_root);
	free(configuration->title);
	for (size_t i = 0; i < configuration->directory_count; i++)
		free(configuration->directories[i]);
	for (size_t i = 0; i < configuration->navigation_count; i++)
	{
		free(configuration->navigation[i].label);
		free(configuration->navigation[i].url);
	}
	*configuration = (struct configuration){0};
}

static bool directory_order(struct configuration *configuration, char *value)
{
	char *part = value;
	while (part)
	{
		char *next = strchr(part, ',');
		if (next)
			*next++ = '\0';
		part = trim(part);
		if (!*part || configuration->directory_count == CONFIG_ITEMS || strlen(part) > 128 || !strcmp(part, ".") || !strcmp(part, "..") || !strcmp(part, "assets") || !strcmp(part, "_stories"))
			return false;
		for (char *p = part; *p; p++)
			if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
				return false;
		for (size_t i = 0; i < configuration->directory_count; i++)
			if (!strcmp(part, configuration->directories[i]))
				return false;
		char *copy = strdup(part);
		if (!copy)
			return false;
		configuration->directories[configuration->directory_count++] = copy;
		part = next;
	}
	return true;
}

static bool navigation_add(struct configuration *configuration, const char *label, const char *url)
{
	if (!*label || configuration->navigation_count == CONFIG_ITEMS || !*url || (url[0] != '/' && strncmp(url, "https://", 8) && strncmp(url, "http://", 7)) || !strncmp(url, "//", 2))
		return false;
	for (size_t i = 0; i < configuration->navigation_count; i++)
		if (!strcmp(label, configuration->navigation[i].label))
			return false;
	char *label_copy = strdup(label);
	char *url_copy = strdup(url);
	if (!label_copy || !url_copy)
	{
		free(label_copy);
		free(url_copy);
		return false;
	}
	configuration->navigation[configuration->navigation_count++] = (struct navigation_link){label_copy, url_copy};
	return true;
}

bool configuration_parse(const char *data, size_t size, const char *base_directory, struct configuration *output, char *error, size_t error_size)
{
	struct configuration configuration = {.threads = 8, .markdown_limit = 8u * 1024u * 1024u, .output_limit = 32u * 1024u * 1024u};
	bool                 seen[sizeof(options) / sizeof(options[0])] = {false};
	char                 section[32] = "";
	char                *copy = NULL;
	size_t               line_number = 0;
	const char          *reason = "invalid configuration";
	*output = (struct configuration){0};
	if (size > CONFIG_LIMIT || memchr(data, '\0', size))
	{
		reason = "configuration is too large or contains NUL";
		goto fail;
	}
	for (size_t i = 0; i < size; i++)
		if ((unsigned char)data[i] < 32 && data[i] != '\n' && data[i] != '\r' && data[i] != '\t')
			goto fail;
	copy = malloc(size + 1);
	configuration.listen = strdup("127.0.0.1:8080");
	configuration.title = strdup("Stories");
	if (!copy || !configuration.listen || !configuration.title)
	{
		reason = "out of memory";
		goto fail;
	}
	memcpy(copy, data, size);
	copy[size] = '\0';
	char *line = copy;
	while (line)
	{
		char *next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		line_number++;
		line = trim(line);
		if (!*line || *line == '#' || *line == ';')
			goto next_line;
		if (*line == '[')
		{
			size_t length = strlen(line);
			if (length < 3 || line[length - 1] != ']' || length - 2 >= sizeof(section))
				goto fail;
			line[length - 1] = '\0';
			strcpy(section, line + 1);
			if (strcmp(section, "server") && strcmp(section, "content") && strcmp(section, "site") && strcmp(section, "navigation") && strcmp(section, "assets"))
				goto fail;
			goto next_line;
		}
		char *value = strchr(line, '=');
		if (!value)
			goto fail;
		*value++ = '\0';
		line = trim(line);
		value = trim(value);
		if (!*value)
			goto fail;
		if (!strcmp(section, "navigation"))
		{
			if (!navigation_add(&configuration, line, value))
				goto fail;
			goto next_line;
		}
		size_t index;
		for (index = 0; index < sizeof(options) / sizeof(options[0]); index++)
			if (!strcmp(section, options[index].section) && !strcmp(line, options[index].name))
				break;
		if (index == sizeof(options) / sizeof(options[0]) || seen[index])
			goto fail;
		seen[index] = true;
		const struct option_definition *option = &options[index];
		char                           *field = (char *)&configuration + option->offset;
		if (option->kind == OPTION_STRING || option->kind == OPTION_PATH)
		{
			char *replacement = option->kind == OPTION_PATH ? path_from(base_directory, value) : strdup(value);
			if (!replacement)
				goto fail;
			free(*(char **)field);
			*(char **)field = replacement;
		}
		else if (option->kind == OPTION_ORDER)
		{
			if (!directory_order(&configuration, value))
				goto fail;
		}
		else
		{
			size_t number;
			size_t maximum = option->kind == OPTION_THREADS ? 1024 : 1024u * 1024u * 1024u;
			if (!configuration_unsigned(value, maximum, &number) || !number)
				goto fail;
			if (option->kind == OPTION_THREADS)
				configuration.threads = (unsigned)number;
			else
				*(size_t *)field = number;
		}
	next_line:
		line = next;
	}
	if (!configuration.content_root || !configuration.asset_root || !configuration_listen_valid(configuration.listen))
	{
		reason = "content.root and assets.root are required; server.listen must be a numeric HTTP endpoint";
		goto fail;
	}
	free(copy);
	*output = configuration;
	if (error_size)
		error[0] = '\0';
	return true;

fail:
	snprintf(error, error_size, "line %zu: %s", line_number, reason);
	free(copy);
	configuration_free(&configuration);
	return false;
}

bool configuration_load_search(const char *explicit_path, const char *const *search_paths, size_t search_count, struct configuration *output, char *error, size_t error_size)
{
	const char   *selected = explicit_path;
	struct buffer input;
	FILE         *file = NULL;
	char          absolute[STORIES_PATH_LIMIT];
	char          base[STORIES_PATH_LIMIT];
	bool          result = false;
	*output = (struct configuration){0};
	buffer_init(&input, CONFIG_LIMIT);
	if (!selected)
	{
		for (size_t i = 0; i < search_count; i++)
		{
			struct stat status;
			/* lstat selects even a broken symlink: a selected but unusable file is fatal. */
			if (!lstat(search_paths[i], &status))
			{
				selected = search_paths[i];
				break;
			}
			if (errno != ENOENT && errno != ENOTDIR)
			{
				snprintf(error, error_size, "%s: %s", search_paths[i], strerror(errno));
				goto done;
			}
		}
	}
	if (!selected)
	{
		snprintf(error, error_size, "no configuration found in ./stories.conf or /etc/stories.conf");
		goto done;
	}
	/* Preserve the selected file's directory even when the file itself is a symlink. */
	if (*selected == '/')
	{
		if (strlen(selected) >= sizeof(absolute))
			goto path_error;
		strcpy(absolute, selected);
	}
	else
	{
		char cwd[STORIES_PATH_LIMIT];
		if (!getcwd(cwd, sizeof(cwd)) || snprintf(absolute, sizeof(absolute), "%s/%s", cwd, selected) >= (int)sizeof(absolute))
			goto path_error;
	}
	strcpy(base, absolute);
	char *slash = strrchr(base, '/');
	if (slash == base)
		slash[1] = '\0';
	else
		*slash = '\0';
	/* Open nonblocking before checking the type: a FIFO must fail validation
	 * immediately, not wait indefinitely for a writer. */
	int descriptor = open(absolute, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (descriptor < 0)
	{
		snprintf(error, error_size, "%s: %s", selected, strerror(errno));
		goto done;
	}
	struct stat status;
	if (fstat(descriptor, &status) || !S_ISREG(status.st_mode))
	{
		close(descriptor);
		snprintf(error, error_size, "%s: configuration must be a regular file", selected);
		goto done;
	}
	file = fdopen(descriptor, "rb");
	if (!file)
	{
		close(descriptor);
		snprintf(error, error_size, "%s: cannot create configuration stream", selected);
		goto done;
	}
	char   block[4096];
	size_t count;
	while ((count = fread(block, 1, sizeof(block), file)))
		if (!buffer_append(&input, block, count))
			break;
	if (ferror(file) || input.failed)
	{
		snprintf(error, error_size, "%s: configuration read failed or exceeds 1 MiB", selected);
		goto done;
	}
	result = configuration_parse(input.data ? input.data : "", input.size, base, output, error, error_size);
	goto done;

path_error:
	snprintf(error, error_size, "configuration path is too long or inaccessible");
done:
	if (file)
		fclose(file);
	buffer_free(&input);
	return result;
}

bool configuration_load(const char *explicit_path, struct configuration *output, char *error, size_t error_size)
{
	static const char *const candidates[] = {"./stories.conf", "/etc/stories.conf"};
	return configuration_load_search(explicit_path, candidates, sizeof(candidates) / sizeof(candidates[0]), output, error, error_size);
}
