#ifndef STORIES_CONFIG_H
#define STORIES_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define CONFIG_LIMIT (1024u * 1024u)
#define CONFIG_ITEMS 128
#define STORIES_PATH_LIMIT 4096

struct navigation_link
{
	char *label;
	char *url;
};

/* Owns all strings. Treat as immutable once handed to the server. */
struct configuration
{
	char                  *listen;
	unsigned               threads;
	char                  *content_root;
	char                  *asset_root;
	char                  *title;
	char                  *template_path;
	char                  *access_log_path;
	char                  *error_log_path;
	char                  *directories[CONFIG_ITEMS];
	size_t                 directory_count;
	struct navigation_link navigation[CONFIG_ITEMS];
	size_t                 navigation_count;
	size_t                 markdown_limit;
	size_t                 output_limit;
};

/* On failure, output is empty and error contains a human-readable diagnostic.
 * Parsing has no filesystem effects and preserves paths as supplied. */
bool configuration_parse(const char *data, size_t size, struct configuration *output, char *error, size_t error_size);
bool configuration_load(const char *explicit_path, struct configuration *output, char *error, size_t error_size);
/* The executable supplies fixed default paths. Tests supply isolated files to
 * exercise the identical discovery policy without touching /etc. */
bool configuration_load_search(const char *explicit_path, const char *const *search_paths, size_t search_count, struct configuration *output, char *error, size_t error_size);
void configuration_free(struct configuration *configuration);
bool configuration_listen_valid(const char *listen);
bool configuration_unsigned(const char *text, size_t maximum, size_t *value);

#endif
