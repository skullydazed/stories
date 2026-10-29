#ifndef STORIES_RENDER_H
#define STORIES_RENDER_H

#include "buffer.h"
#include "config.h"

/* All input is borrowed. On failure output may contain an incomplete document: discard it, never send it as a successful HTTP response. Failures are logged where detected; logging and source (or the error title) must remain valid while output is used. */
bool render_document(const struct configuration *configuration, const char *source, const char *markdown, size_t size, struct buffer *output, struct logging *logging);
bool render_error(const struct configuration *configuration, const char *title, const char *message, struct buffer *output, struct logging *logging);

#endif
