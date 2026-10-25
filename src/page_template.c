#include "page_template.h"
#include "document.h"

#include <errno.h>
#include <fcntl.h>
#include <mustach.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct page_template
{
	struct buffer source;
};

enum section_kind
{
	SECTION_NAVIGATION,
	SECTION_ERROR
};

struct template_section
{
	enum section_kind kind;
	size_t            index;
};

struct template_values
{
	const struct configuration *configuration;
	const char                 *title;
	const struct buffer        *content;
	const char                 *message;
	struct buffer              *output;
	struct template_section     sections[MUSTACH_MAX_DEPTH];
	size_t                      depth;
	bool                        validating;
	bool                        validation_navigation;
	char                        unknown[128];
};

static int unknown_value(struct template_values *values, const char *name)
{
	snprintf(values->unknown, sizeof(values->unknown), "unknown template name: %.96s", name);
	return MUSTACH_ERROR_UNDEFINED_TAG;
}

static int value_get(void *closure, const char *name, struct mustach_sbuf *value)
{
	struct template_values *values = closure;
	if (!strcmp(name, "title"))
		value->value = values->title;
	else if (!strcmp(name, "site_title"))
		value->value = values->configuration->title;
	else if (!strcmp(name, "message"))
		value->value = values->message ? values->message : "";
	else if (!strcmp(name, "content"))
	{
		value->value = values->content ? values->content->data : "";
		value->length = values->content ? values->content->size : 0;
	}
	else if (!strcmp(name, "label") || !strcmp(name, "url"))
	{
		/* Look through enclosing boolean sections to the current navigation item. */
		size_t depth = values->depth;
		while (depth && values->sections[depth - 1].kind != SECTION_NAVIGATION)
			depth--;
		if (!depth)
			return unknown_value(values, name);
		const struct navigation_link *link = values->validating ? &(const struct navigation_link){"Navigation", "/"} : &values->configuration->navigation[values->sections[depth - 1].index];
		value->value = !strcmp(name, "label") ? link->label : link->url;
	}
	else
		return unknown_value(values, name);
	return MUSTACH_OK;
}

static int section_enter(void *closure, const char *name)
{
	struct template_values *values = closure;
	struct template_section section = {0};
	if (!strcmp(name, "navigation"))
	{
		if (!(values->validating ? values->validation_navigation : values->configuration->navigation_count != 0))
			return 0;
		section.kind = SECTION_NAVIGATION;
	}
	else if (!strcmp(name, "error"))
	{
		if (!values->message)
			return 0;
		section.kind = SECTION_ERROR;
	}
	else
		return unknown_value(values, name);
	if (values->depth == MUSTACH_MAX_DEPTH)
		return MUSTACH_ERROR_TOO_DEEP;
	values->sections[values->depth++] = section;
	return 1;
}

static int section_next(void *closure)
{
	struct template_values  *values = closure;
	struct template_section *section = &values->sections[values->depth - 1];
	return section->kind == SECTION_NAVIGATION && !values->validating && ++section->index < values->configuration->navigation_count;
}

static int section_leave(void *closure)
{
	struct template_values *values = closure;
	values->depth--;
	return MUSTACH_OK;
}

static int partial_get(void *closure, const char *name, struct mustach_sbuf *value)
{
	(void)value;
	return unknown_value(closure, name);
}

static int output_emit(void *closure, const char *text, size_t size, int escape, FILE *file)
{
	(void)file;
	struct template_values *values = closure;
	if (values->validating)
		return MUSTACH_OK;
	return (escape ? buffer_html(values->output, text, size) : buffer_append(values->output, text, size)) ? MUSTACH_OK : MUSTACH_ERROR_SYSTEM;
}

static int template_expand(const struct page_template *layout, struct template_values *values)
{
	static const struct mustach_itf callbacks = {
		.enter = section_enter,
		.next = section_next,
		.leave = section_leave,
		.partial = partial_get,
		.emit = output_emit,
		.get = value_get,
	};
	/* Custom emit/get callbacks keep Mustach on our bounded buffer interface; no FILE stream, temporary file, or unbounded memory rendering is used. */
	return mustach_file(layout->source.data, layout->source.size, &callbacks, values, Mustach_With_NoExtensions, NULL);
}

static bool template_read(const struct configuration *configuration, struct page_template *layout, char *error, size_t error_size)
{
	*layout = (struct page_template){0};
	const char *path = configuration->template_path;
	int         fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
	{
		snprintf(error, error_size, "%s: cannot open page template: %s", path, strerror(errno));
		return false;
	}
	struct stat status;
	bool        success = !fstat(fd, &status) && S_ISREG(status.st_mode);
	if (!success)
		snprintf(error, error_size, "%s: page template must be a regular file", path);
	else if (!(success = document_read(fd, PAGE_TEMPLATE_LIMIT, &layout->source)))
		snprintf(error, error_size, "%s: cannot read page template or exceeds 1 MiB", path);
	close(fd);
	if (!success)
		goto fail;
	if (!layout->source.size || memchr(layout->source.data, '\0', layout->source.size))
	{
		snprintf(error, error_size, "%s: page template must be nonempty and contain no NUL bytes", path);
		goto fail;
	}
	return true;

fail:
	buffer_free(&layout->source);
	return false;
}

bool page_template_check(const struct configuration *configuration, char *error, size_t error_size)
{
	struct page_template layout;
	if (!template_read(configuration, &layout, error, error_size))
		return false;
	/* Visit document and error branches, with and without navigation. Discarding output validates syntax without making startup depend on output_limit. */
	for (unsigned combination = 0; combination < 4; combination++)
	{
		struct template_values values = {.configuration = configuration, .title = "Page", .message = combination & 1 ? "Error" : NULL, .validating = true, .validation_navigation = (combination & 2) != 0};
		int                    result = template_expand(&layout, &values);
		if (result < 0)
		{
			if (*values.unknown)
				snprintf(error, error_size, "%s: %s", configuration->template_path, values.unknown);
			else
				snprintf(error, error_size, "%s: invalid Mustache template (error %d)", configuration->template_path, result);
			goto fail;
		}
	}
	if (error_size)
		error[0] = '\0';
	buffer_free(&layout.source);
	return true;

fail:
	buffer_free(&layout.source);
	return false;
}

bool page_template_render(const struct configuration *configuration, const char *title, const struct buffer *content, const char *message, struct buffer *output)
{
	struct page_template layout;
	char                 error[512];
	if (!template_read(configuration, &layout, error, sizeof(error)))
	{
		output->failed = true;
		return false;
	}
	struct template_values values = {.configuration = configuration, .title = title, .content = content, .message = message, .output = output};
	if (template_expand(&layout, &values) < 0)
		output->failed = true;
	buffer_free(&layout.source);
	return !output->failed;
}
