#ifndef STORIES_PAGE_TEMPLATE_H
#define STORIES_PAGE_TEMPLATE_H

#include "buffer.h"
#include "config.h"

#define PAGE_TEMPLATE_LIMIT (1024u * 1024u)

/* Reads, validates, and releases the template without retaining startup state. Failures are logged at the failed operation, including during validation. */
bool page_template_check(const struct configuration *configuration, struct logging *logging);
/* Reads the current template for each expansion and releases its source before returning. Inputs are borrowed; output must already be initialized with its page budget and logging context. A non-NULL message selects an error page; content may then be NULL. Failures are logged, mark output failed, and callers must discard the incomplete page. */
bool page_template_render(const struct configuration *configuration, const char *title, const struct buffer *content, const char *message, struct buffer *output);

#endif
