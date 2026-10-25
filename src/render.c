#include "render.h"
#include "entities.h"
#include "url.h"

#include <limits.h>
#include <md4c.h>
#include <stdio.h>
#include <string.h>

struct renderer
{
	struct buffer *output;
	const char    *source;
	unsigned       image_depth;
};

struct html_tags
{
	const char *open;
	const char *close;
};

static const struct html_tags block_tags[MD_BLOCK_TD + 1] = {
	[MD_BLOCK_DOC] = {"", ""},
	[MD_BLOCK_QUOTE] = {"<blockquote>\n", "</blockquote>\n"},
	[MD_BLOCK_UL] = {"<ul>\n", "</ul>\n"},
	[MD_BLOCK_OL] = {NULL, "</ol>\n"},
	[MD_BLOCK_LI] = {NULL, "</li>\n"},
	[MD_BLOCK_HR] = {"<hr>\n", ""},
	[MD_BLOCK_H] = {NULL, NULL},
	[MD_BLOCK_CODE] = {NULL, "</code></pre>\n"},
	[MD_BLOCK_HTML] = {"", ""},
	[MD_BLOCK_P] = {"<p>", "</p>\n"},
	[MD_BLOCK_TABLE] = {"<table>\n", "</table>\n"},
	[MD_BLOCK_THEAD] = {"<thead>\n", "</thead>\n"},
	[MD_BLOCK_TBODY] = {"<tbody>\n", "</tbody>\n"},
	[MD_BLOCK_TR] = {"<tr>\n", "</tr>\n"},
	[MD_BLOCK_TH] = {NULL, "</th>\n"},
	[MD_BLOCK_TD] = {NULL, "</td>\n"},
};

static bool attribute_decode(const MD_ATTRIBUTE *attribute, struct buffer *output)
{
	if (!attribute->size)
		return true;
	for (size_t i = 0; attribute->substr_offsets[i] < attribute->size; i++)
	{
		size_t start = attribute->substr_offsets[i];
		size_t size = attribute->substr_offsets[i + 1] - start;
		if (attribute->substr_types[i] == MD_TEXT_ENTITY)
		{
			if (!entity_decode(attribute->text + start, size, output))
				return false;
		}
		else if (attribute->substr_types[i] == MD_TEXT_NULLCHAR)
		{
			if (!buffer_puts(output, "\xef\xbf\xbd"))
				return false;
		}
		else if (!buffer_append(output, attribute->text + start, size))
			return false;
	}
	return true;
}

static bool attribute_render(struct renderer *renderer, const MD_ATTRIBUTE *attribute, bool rewrite)
{
	struct buffer decoded, destination;
	buffer_init(&decoded, renderer->output->limit);
	buffer_init(&destination, renderer->output->limit);
	bool success = attribute_decode(attribute, &decoded);
	if (success && rewrite)
		success = url_rewrite_link(decoded.data ? decoded.data : "", decoded.size, renderer->source, &destination);
	else if (success)
		success = buffer_append(&destination, decoded.data, decoded.size);
	if (success)
		success = buffer_html(renderer->output, destination.data ? destination.data : "", destination.size);
	buffer_free(&decoded);
	buffer_free(&destination);
	return success;
}

static int block_enter(MD_BLOCKTYPE type, void *detail, void *userdata)
{
	struct renderer *renderer = userdata;
	struct buffer   *output = renderer->output;
	char             number[64];
	if ((unsigned)type > MD_BLOCK_TD)
		return 1;
	if (block_tags[type].open)
		return !buffer_puts(output, block_tags[type].open);
	switch (type)
	{
	case MD_BLOCK_OL:
	{
		const MD_BLOCK_OL_DETAIL *list = detail;
		if (list->start == 1)
			buffer_puts(output, "<ol>\n");
		else
		{
			snprintf(number, sizeof(number), "<ol start=\"%u\">\n", list->start);
			buffer_puts(output, number);
		}
		break;
	}
	case MD_BLOCK_LI:
	{
		const MD_BLOCK_LI_DETAIL *item = detail;
		buffer_puts(output, "<li>");
		if (item->is_task)
			buffer_puts(output, item->task_mark == ' ' ? "<input type=\"checkbox\" disabled> " : "<input type=\"checkbox\" disabled checked> ");
		break;
	}
	case MD_BLOCK_H:
	{
		const MD_BLOCK_H_DETAIL *heading = detail;
		snprintf(number, sizeof(number), "<h%u>", heading->level);
		buffer_puts(output, number);
		break;
	}
	case MD_BLOCK_CODE:
	{
		const MD_BLOCK_CODE_DETAIL *code = detail;
		buffer_puts(output, "<pre><code");
		if (code->lang.size)
		{
			buffer_puts(output, " class=\"language-");
			if (!attribute_render(renderer, &code->lang, false))
				return 1;
			buffer_puts(output, "\"");
		}
		buffer_puts(output, ">");
		break;
	}
	case MD_BLOCK_TH:
	case MD_BLOCK_TD:
	{
		const MD_BLOCK_TD_DETAIL *cell = detail;
		buffer_puts(output, type == MD_BLOCK_TH ? "<th" : "<td");
		static const char *alignment[] = {"", " style=\"text-align:left\"", " style=\"text-align:center\"", " style=\"text-align:right\""};
		if ((unsigned)cell->align < sizeof(alignment) / sizeof(alignment[0]))
			buffer_puts(output, alignment[cell->align]);
		buffer_puts(output, ">");
		break;
	}
	default:
		return 1;
	}
	return output->failed;
}

static int block_leave(MD_BLOCKTYPE type, void *detail, void *userdata)
{
	struct renderer *renderer = userdata;
	if ((unsigned)type > MD_BLOCK_TD)
		return 1;
	if (type == MD_BLOCK_H)
	{
		char tag[32];
		snprintf(tag, sizeof(tag), "</h%u>\n", ((const MD_BLOCK_H_DETAIL *)detail)->level);
		return !buffer_puts(renderer->output, tag);
	}
	return !buffer_puts(renderer->output, block_tags[type].close);
}

static int span_enter(MD_SPANTYPE type, void *detail, void *userdata)
{
	struct renderer *renderer = userdata;
	bool             inside_image = renderer->image_depth != 0;
	if (type == MD_SPAN_IMG)
		renderer->image_depth++;
	/* Rich image labels become an alt attribute, so only their text may be emitted. */
	if (inside_image)
		return 0;
	switch (type)
	{
	case MD_SPAN_EM:
		return !buffer_puts(renderer->output, "<em>");
	case MD_SPAN_STRONG:
		return !buffer_puts(renderer->output, "<strong>");
	case MD_SPAN_CODE:
		return !buffer_puts(renderer->output, "<code>");
	case MD_SPAN_DEL:
		return !buffer_puts(renderer->output, "<del>");
	case MD_SPAN_A:
	{
		const MD_SPAN_A_DETAIL *link = detail;
		buffer_puts(renderer->output, "<a href=\"");
		if (!attribute_render(renderer, &link->href, true))
			return 1;
		buffer_puts(renderer->output, "\"");
		if (link->title.text)
		{
			buffer_puts(renderer->output, " title=\"");
			if (!attribute_render(renderer, &link->title, false))
				return 1;
			buffer_puts(renderer->output, "\"");
		}
		return !buffer_puts(renderer->output, ">");
	}
	case MD_SPAN_IMG:
	{
		const MD_SPAN_IMG_DETAIL *image = detail;
		buffer_puts(renderer->output, "<img src=\"");
		if (!attribute_render(renderer, &image->src, false))
			return 1;
		return !buffer_puts(renderer->output, "\" alt=\"");
	}
	default:
		return 1;
	}
}

static int span_leave(MD_SPANTYPE type, void *detail, void *userdata)
{
	struct renderer *renderer = userdata;
	if (type == MD_SPAN_IMG)
		renderer->image_depth--;
	if (renderer->image_depth)
		return 0;
	switch (type)
	{
	case MD_SPAN_EM:
		return !buffer_puts(renderer->output, "</em>");
	case MD_SPAN_STRONG:
		return !buffer_puts(renderer->output, "</strong>");
	case MD_SPAN_CODE:
		return !buffer_puts(renderer->output, "</code>");
	case MD_SPAN_DEL:
		return !buffer_puts(renderer->output, "</del>");
	case MD_SPAN_A:
		return !buffer_puts(renderer->output, "</a>");
	case MD_SPAN_IMG:
	{
		const MD_SPAN_IMG_DETAIL *image = detail;
		buffer_puts(renderer->output, "\"");
		if (image->title.text)
		{
			buffer_puts(renderer->output, " title=\"");
			if (!attribute_render(renderer, &image->title, false))
				return 1;
			buffer_puts(renderer->output, "\"");
		}
		return !buffer_puts(renderer->output, ">");
	}
	default:
		return 1;
	}
}

static int text_render(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
	struct renderer *renderer = userdata;
	if (type == MD_TEXT_HTML && !renderer->image_depth)
		return !buffer_append(renderer->output, text, size);
	if (type == MD_TEXT_BR)
		return !buffer_puts(renderer->output, renderer->image_depth ? " " : "<br>\n");
	if (type == MD_TEXT_SOFTBR)
		return !buffer_puts(renderer->output, renderer->image_depth ? " " : "\n");
	if (type == MD_TEXT_NULLCHAR)
		return !buffer_puts(renderer->output, "\xef\xbf\xbd");
	if (type == MD_TEXT_ENTITY)
	{
		struct buffer decoded;
		buffer_init(&decoded, renderer->output->limit);
		bool success = entity_decode(text, size, &decoded) && buffer_html(renderer->output, decoded.data ? decoded.data : "", decoded.size);
		buffer_free(&decoded);
		return !success;
	}
	return !buffer_html(renderer->output, text, size);
}

static bool page_open(const struct configuration *configuration, const char *title, struct buffer *output)
{
	buffer_puts(output, "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>");
	buffer_html(output, title, strlen(title));
	buffer_puts(output, " — ");
	buffer_html(output, configuration->title, strlen(configuration->title));
	buffer_puts(output, "</title><link rel=\"stylesheet\" href=\"/_stories/pico.min.css\"></head><body><header class=\"container\"><nav><ul><li><a href=\"/\"><strong>");
	buffer_html(output, configuration->title, strlen(configuration->title));
	buffer_puts(output, "</strong></a></li></ul><ul>");
	for (size_t i = 0; i < configuration->navigation_count; i++)
	{
		const struct navigation_link *link = &configuration->navigation[i];
		buffer_puts(output, "<li><a href=\"");
		buffer_html(output, link->url, strlen(link->url));
		buffer_puts(output, "\">");
		buffer_html(output, link->label, strlen(link->label));
		buffer_puts(output, "</a></li>");
	}
	return buffer_puts(output, "</ul></nav></header><main class=\"container\">\n");
}

bool render_document(const struct configuration *configuration, const char *source, const char *markdown, size_t size, struct buffer *output)
{
	buffer_init(output, configuration->output_limit);
	if (size > UINT_MAX || !page_open(configuration, source, output))
		return false;
	struct renderer renderer = {.output = output, .source = source};
	const MD_PARSER parser = {
		.abi_version = 0,
		.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS,
		.enter_block = block_enter,
		.leave_block = block_leave,
		.enter_span = span_enter,
		.leave_span = span_leave,
		.text = text_render,
	};
	if (md_parse(markdown, (MD_SIZE)size, &parser, &renderer))
		return false;
	return buffer_puts(output, "\n</main></body></html>\n");
}

bool render_error(const struct configuration *configuration, const char *title, const char *message, struct buffer *output)
{
	/* Error pages have a separate small budget so an output-limit failure can
	 * still produce a complete error response instead of recursively failing. */
	buffer_init(output, 256u * 1024u);
	return page_open(configuration, title, output) && buffer_puts(output, "<h1>") && buffer_html(output, title, strlen(title)) && buffer_puts(output, "</h1><p>") && buffer_html(output, message, strlen(message)) && buffer_puts(output, "</p></main></body></html>\n");
}
