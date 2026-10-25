#ifndef STORIES_PAGE_TEMPLATE_H
#define STORIES_PAGE_TEMPLATE_H

#include "buffer.h"
#include "config.h"

#define PAGE_TEMPLATE_LIMIT (1024u * 1024u)

/* Reads, validates, and releases the template without retaining startup state. */
bool page_template_check(const struct configuration *configuration, char *error, size_t error_size);
/* Reads the current template for each expansion and releases its source before returning. Inputs are borrowed; output must already be initialized with its page budget. A non-NULL message selects an error page; content may then be NULL. Failures mark output failed, and callers must discard the incomplete page. */
bool page_template_render(const struct configuration *configuration, const char *title, const struct buffer *content, const char *message, struct buffer *output);

#endif
