#ifndef STORIES_ENTITIES_H
#define STORIES_ENTITIES_H

#include "buffer.h"

/* Decode one parser-identified entity; unknown entities stay literal. */
bool entity_decode(const char *text, size_t size, struct buffer *output);

#endif
