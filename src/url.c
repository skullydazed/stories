#include "url.h"
#include "config.h"
#include "logging.h"

#include <ctype.h>
#include <string.h>

bool url_markdown_extension(const char *path, size_t size)
{
	return size >= 3 && path[size - 3] == '.' && (path[size - 2] == 'm' || path[size - 2] == 'M') && (path[size - 1] == 'd' || path[size - 1] == 'D');
}

bool url_path_has_extension(const char *path, size_t size)
{
	while (size && path[size - 1] == '/')
		size--;
	while (size && path[size - 1] != '/')
	{
		if (path[--size] == '.')
			return true;
	}
	return false;
}

bool url_path_valid(const char *path, size_t size)
{
	if (!size || size >= STORIES_PATH_LIMIT || path[0] != '/')
		return false;
	size_t segment = 1;
	for (size_t i = 0; i <= size; i++)
	{
		if (i < size)
		{
			unsigned char c = (unsigned char)path[i];
			if (c < 32 || c == 127 || c == '\\' || c == '?' || c == '#')
				return false;
		}
		if (i && (i == size || path[i] == '/'))
		{
			size_t length = i - segment;
			if ((length == 1 && path[segment] == '.') || (length == 2 && !memcmp(path + segment, "..", 2)) || (!length && i != size))
				return false;
			/* Keep dotfiles (including .htpasswd) outside both published namespaces. */
			if (length && path[segment] == '.')
				return false;
			segment = i + 1;
		}
	}
	return true;
}

/* source is a borrowed byte span and need not be NUL-terminated. */
static bool document_location(const char *source, size_t length, struct buffer *output)
{
	if (length < 3 || memcmp(source + length - 3, ".md", 3))
		return false;
	length -= 3;
	if ((length == 5 && !memcmp(source, "index", 5)) || (length >= 6 && !memcmp(source + length - 6, "/index", 6)))
		length -= 5;
	if (url_path_has_extension(source, length))
		return false;
	return buffer_puts(output, "/") && buffer_url(output, source, length);
}

bool url_document_location(const char *source, struct buffer *output)
{
	return document_location(source, strlen(source), output);
}

static bool external_url(const char *text, size_t size)
{
	if (size >= 2 && text[0] == '/' && text[1] == '/')
		return true;
	if (!size || !isalpha((unsigned char)text[0]))
		return false;
	for (size_t i = 1; i < size; i++)
	{
		if (text[i] == ':')
			return true;
		if (!(isalnum((unsigned char)text[i]) || text[i] == '+' || text[i] == '-' || text[i] == '.'))
			return false;
	}
	return false;
}

static int hex_digit(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool markdown_link_path(const char *path, size_t size)
{
	static const char extension[] = ".md";
	/* Inspect only the decoded suffix. Malformed escapes elsewhere must not
	 * reject links that Stories would otherwise leave unchanged. */
	for (size_t i = sizeof(extension) - 1; i > 0; i--)
	{
		if (!size)
			return false;
		unsigned char c = (unsigned char)path[--size];
		if (size >= 2 && path[size - 2] == '%')
		{
			int high = hex_digit((unsigned char)path[size - 1]);
			int low = hex_digit(c);
			if (high >= 0 && low >= 0)
			{
				c = (unsigned char)(high * 16 + low);
				size -= 2;
			}
		}
		if (c != (unsigned char)extension[i - 1])
			return false;
	}
	return true;
}

static bool link_failure(const char *destination, size_t size, const char *source, struct buffer *output, const char *reason)
{
	/* Bound the preview so it leaves room for the rejection reason. Plain utility buffers remain silent, as they are for allocation failures. */
	if (output->context)
		logging_error(output->logging, "%s: invalid Markdown link \"%.*s%s\": %s", source, (int)(size > 512 ? 512 : size), destination, size > 512 ? "..." : "", reason);
	return false;
}

bool url_rewrite_link(const char *destination, size_t size, const char *source, struct buffer *output)
{
	char   decoded[STORIES_PATH_LIMIT];
	char   joined[STORIES_PATH_LIMIT];
	char   normalized[STORIES_PATH_LIMIT];
	size_t offsets[STORIES_PATH_LIMIT / 2];
	size_t decoded_size = 0, joined_size = 0, normalized_size = 0, depth = 0;
	size_t path_size = 0;
	if (memchr(destination, '\0', size))
		return link_failure(destination, size, source, output, "destination contains a NUL byte");
	if (!size || destination[0] == '#' || destination[0] == '?' || external_url(destination, size))
		return buffer_append(output, destination, size);
	while (path_size < size && destination[path_size] != '?' && destination[path_size] != '#')
		path_size++;
	if (!markdown_link_path(destination, path_size))
		return buffer_append(output, destination, size);
	if (path_size >= sizeof(decoded))
		return link_failure(destination, size, source, output, "path exceeds the 4095-byte limit");
	for (size_t i = 0; i < path_size; i++)
	{
		unsigned char c = (unsigned char)destination[i];
		if (c == '%')
		{
			if (i + 2 >= path_size || hex_digit((unsigned char)destination[i + 1]) < 0 || hex_digit((unsigned char)destination[i + 2]) < 0)
				return link_failure(destination, size, source, output, "malformed percent escape");
			c = (unsigned char)(hex_digit((unsigned char)destination[i + 1]) * 16 + hex_digit((unsigned char)destination[i + 2]));
			i += 2;
		}
		if (!c)
			return link_failure(destination, size, source, output, "decoded path contains a NUL byte");
		if (c < 32 || c == 127)
			return link_failure(destination, size, source, output, "decoded path contains a control byte");
		if (c == '\\')
			return link_failure(destination, size, source, output, "decoded path contains a backslash");
		decoded[decoded_size++] = (char)c;
	}
	decoded[decoded_size] = '\0';
	if (decoded[0] != '/')
	{
		const char *slash = strrchr(source, '/');
		joined_size = slash ? (size_t)(slash - source) + 1 : 0;
		if (joined_size >= sizeof(joined))
			return link_failure(destination, size, source, output, "source directory exceeds the 4095-byte limit");
		memcpy(joined, source, joined_size);
	}
	if (decoded_size >= sizeof(joined) - joined_size)
		return link_failure(destination, size, source, output, "resolved path exceeds the 4095-byte limit");
	memcpy(joined + joined_size, decoded, decoded_size + 1);
	joined_size += decoded_size;
	for (size_t i = 0; i < joined_size;)
	{
		while (i < joined_size && joined[i] == '/')
			i++;
		size_t start = i;
		while (i < joined_size && joined[i] != '/')
			i++;
		size_t length = i - start;
		if (!length || (length == 1 && joined[start] == '.'))
			continue;
		if (length == 2 && !memcmp(joined + start, "..", 2))
		{
			if (!depth)
				return link_failure(destination, size, source, output, "path escapes the content root");
			normalized_size = offsets[--depth];
			continue;
		}
		if (depth == sizeof(offsets) / sizeof(offsets[0]))
			return link_failure(destination, size, source, output, "too many path components");
		offsets[depth++] = normalized_size;
		if (normalized_size)
			normalized[normalized_size++] = '/';
		memcpy(normalized + normalized_size, joined + start, length);
		normalized_size += length;
	}
	normalized[normalized_size] = '\0';
	if (!document_location(normalized, normalized_size, output))
	{
		if (!output->failed)
			return link_failure(destination, size, source, output, "filename retains an extension after removing .md");
		return false;
	}
	return buffer_append(output, destination + path_size, size - path_size);
}
