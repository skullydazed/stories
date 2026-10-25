#include "buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void buffer_init(struct buffer *buffer, size_t limit)
{
	*buffer = (struct buffer){.limit = limit};
}

void buffer_free(struct buffer *buffer)
{
	free(buffer->data);
	*buffer = (struct buffer){0};
}

bool buffer_append(struct buffer *buffer, const void *data, size_t size)
{
	size_t needed;
	size_t capacity;
	char  *allocation;

	/* Check before addition: input sizes can originate at a parser boundary. */
	if (buffer->failed || size > buffer->limit - buffer->size || size >= SIZE_MAX - buffer->size)
	{
		buffer->failed = true;
		return false;
	}
	needed = buffer->size + size + 1;
	if (needed > buffer->capacity)
	{
		capacity = buffer->capacity ? buffer->capacity : 256;
		while (capacity < needed)
		{
			if (capacity > SIZE_MAX / 2)
			{
				capacity = needed;
				break;
			}
			capacity *= 2;
		}
		if (buffer->limit != SIZE_MAX && capacity > buffer->limit + 1)
			capacity = buffer->limit + 1;
		allocation = realloc(buffer->data, capacity);
		if (!allocation)
		{
			buffer->failed = true;
			return false;
		}
		buffer->data = allocation;
		buffer->capacity = capacity;
	}
	if (size)
		memcpy(buffer->data + buffer->size, data, size);
	buffer->size += size;
	buffer->data[buffer->size] = '\0';
	return true;
}

bool buffer_puts(struct buffer *buffer, const char *text)
{
	return buffer_append(buffer, text, strlen(text));
}

bool buffer_html(struct buffer *buffer, const char *text, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		const char *escaped = NULL;
		switch (text[i])
		{
		case '&':
			escaped = "&amp;";
			break;
		case '<':
			escaped = "&lt;";
			break;
		case '>':
			escaped = "&gt;";
			break;
		case '"':
			escaped = "&quot;";
			break;
		case '\'':
			escaped = "&#39;";
			break;
		case '\0':
			escaped = "\xef\xbf\xbd";
			break;
		default:
			break;
		}
		if (!(escaped ? buffer_puts(buffer, escaped) : buffer_append(buffer, text + i, 1)))
			return false;
	}
	return true;
}

bool buffer_url(struct buffer *buffer, const char *text, size_t size)
{
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < size; i++)
	{
		unsigned char c = (unsigned char)text[i];
		bool          plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("/-._~", c);
		if (plain && c)
		{
			if (!buffer_append(buffer, &c, 1))
				return false;
		}
		else
		{
			char encoded[] = {'%', hex[c >> 4], hex[c & 15]};
			if (!buffer_append(buffer, encoded, sizeof(encoded)))
				return false;
		}
	}
	return true;
}
