#ifndef STORIES_BUFFER_H
#define STORIES_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

/* A buffer owns its allocation. Failure is sticky; size excludes the trailing NUL.
 * Callers may borrow data until the next append or buffer_free(). */
struct buffer
{
	char  *data;
	size_t size;
	size_t capacity;
	size_t limit;
	bool   failed;
};

void buffer_init(struct buffer *buffer, size_t limit);
void buffer_free(struct buffer *buffer);
bool buffer_append(struct buffer *buffer, const void *data, size_t size);
bool buffer_puts(struct buffer *buffer, const char *text);
bool buffer_html(struct buffer *buffer, const char *text, size_t size);
bool buffer_url(struct buffer *buffer, const char *text, size_t size);

#endif
