#include "entities.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct named_entity
{
	const char *name;
	const char *value;
};

static const struct named_entity named_entities[] = {
#include "entities.inc"
};

static int entity_compare(const void *key, const void *entry)
{
	return strcmp(key, ((const struct named_entity *)entry)->name);
}

static bool codepoint_append(uint32_t point, struct buffer *output)
{
	unsigned char utf8[4];
	size_t        size;
	if (!point || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
		point = 0xfffd;
	if (point < 0x80)
	{
		utf8[0] = (unsigned char)point;
		size = 1;
	}
	else if (point < 0x800)
	{
		utf8[0] = (unsigned char)(0xc0 | (point >> 6));
		utf8[1] = (unsigned char)(0x80 | (point & 63));
		size = 2;
	}
	else if (point < 0x10000)
	{
		utf8[0] = (unsigned char)(0xe0 | (point >> 12));
		utf8[1] = (unsigned char)(0x80 | ((point >> 6) & 63));
		utf8[2] = (unsigned char)(0x80 | (point & 63));
		size = 3;
	}
	else
	{
		utf8[0] = (unsigned char)(0xf0 | (point >> 18));
		utf8[1] = (unsigned char)(0x80 | ((point >> 12) & 63));
		utf8[2] = (unsigned char)(0x80 | ((point >> 6) & 63));
		utf8[3] = (unsigned char)(0x80 | (point & 63));
		size = 4;
	}
	return buffer_append(output, utf8, size);
}

bool entity_decode(const char *text, size_t size, struct buffer *output)
{
	if (size < 3 || text[0] != '&' || text[size - 1] != ';')
		return buffer_append(output, text, size);
	if (text[1] == '#')
	{
		size_t   position = 2;
		unsigned base = 10;
		uint32_t point = 0;
		if (position < size - 1 && (text[position] == 'x' || text[position] == 'X'))
		{
			position++;
			base = 16;
		}
		if (position == size - 1)
			return buffer_append(output, text, size);
		for (; position < size - 1; position++)
		{
			unsigned char c = (unsigned char)text[position];
			unsigned      digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else
				return buffer_append(output, text, size);
			if (digit >= base)
				return buffer_append(output, text, size);
			if (point <= 0x10ffff)
				point = point * base + digit;
		}
		return codepoint_append(point, output);
	}
	char name[64];
	if (size - 1 >= sizeof(name))
		return buffer_append(output, text, size);
	memcpy(name, text + 1, size - 1);
	name[size - 1] = '\0';
	const struct named_entity *entity = bsearch(name, named_entities, sizeof(named_entities) / sizeof(named_entities[0]), sizeof(named_entities[0]), entity_compare);
	return entity ? buffer_puts(output, entity->value) : buffer_append(output, text, size);
}
