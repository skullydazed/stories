#ifndef STORIES_BUFFER_H
#define STORIES_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

struct logging;

/* A buffer owns its allocation. Failure is sticky; size excludes the trailing NUL. Callers may borrow data until the next append or buffer_free(). Logging and context are optional borrowed state and must remain valid while the buffer is used. */
struct buffer
{
	char           *data;
	size_t          size;
	size_t          capacity;
	size_t          limit;
	bool            failed;
	struct logging *logging;
	const char     *context;
};

void buffer_init(struct buffer *buffer, size_t limit);
/* Logs the first allocation or limit failure with the supplied context. NULL logging selects stderr; a NULL context keeps the buffer silent. */
void buffer_init_logged(struct buffer *buffer, size_t limit, struct logging *logging, const char *context);
void buffer_free(struct buffer *buffer);
bool buffer_append(struct buffer *buffer, const void *data, size_t size);
bool buffer_puts(struct buffer *buffer, const char *text);
bool buffer_html(struct buffer *buffer, const char *text, size_t size);
bool buffer_url(struct buffer *buffer, const char *text, size_t size);

#endif
