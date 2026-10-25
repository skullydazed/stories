#ifndef STORIES_RENDER_H
#define STORIES_RENDER_H

#include "buffer.h"
#include "config.h"

/* All input is borrowed. On failure output may contain an incomplete document:
 * discard it, never send it as a successful HTTP response. */
bool render_document(const struct configuration *configuration, const char *source, const char *markdown, size_t size, struct buffer *output);
bool render_error(const struct configuration *configuration, const char *title, const char *message, struct buffer *output);

#endif
