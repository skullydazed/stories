#ifndef STORIES_DOCUMENT_H
#define STORIES_DOCUMENT_H

#include "buffer.h"
#include "config.h"

enum route_status
{
	ROUTE_DOCUMENT = 200,
	ROUTE_REDIRECT = 301,
	ROUTE_INVALID = 400,
	ROUTE_MISSING = 404,
	ROUTE_FAILURE = 500
};

/* Owns fd and location. The caller must document_route_free() every result,
 * including failures. source is a docroot-relative path, never a URL. */
struct document_route
{
	enum route_status status;
	int               fd;
	char              source[STORIES_PATH_LIMIT];
	struct buffer     location;
};

void document_resolve(int root_fd, const struct configuration *configuration, const char *path, size_t size, struct document_route *route);
void document_route_free(struct document_route *route);
bool document_read(int fd, size_t limit, struct buffer *output);

#endif
