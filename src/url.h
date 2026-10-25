#ifndef STORIES_URL_H
#define STORIES_URL_H

#include "buffer.h"

/* The HTTP adapter decodes request paths once and supplies the full byte count.
 * This function validates that span and never performs another decoding pass. */
bool url_path_valid(const char *path, size_t size);
bool url_markdown_extension(const char *path, size_t size);

/* Input and source are borrowed. source is a docroot-relative Markdown filename.
 * Appends a rewritten destination, or the original for non-Markdown/external URLs.
 * False means malformed input, a root escape, or output allocation/limit failure. */
bool url_rewrite_link(const char *destination, size_t size, const char *source, struct buffer *output);
bool url_document_location(const char *source, struct buffer *output);

#endif
