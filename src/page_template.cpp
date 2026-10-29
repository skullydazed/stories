extern "C"
{
#include "page_template.h"
#include "document.h"
#include "logging.h"
}

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <inja/inja.hpp>
#include <new>
#include <ostream>
#include <streambuf>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

/* Borrows the final response buffer. A NULL destination discards validation output without allocating a second page. */
struct template_output : std::streambuf
{
	struct buffer *destination;

	explicit template_output(struct buffer *output)
	    : destination(output)
	{
	}

	std::streamsize xsputn(const char *text, std::streamsize size) override
	{
		if (destination && !buffer_append(destination, text, static_cast<size_t>(size)))
			throw std::ios_base::failure("cannot write page output");
		return size;
	}

	int_type overflow(int_type value) override
	{
		if (traits_type::eq_int_type(value, traits_type::eof()))
			return traits_type::not_eof(value);
		char byte = traits_type::to_char_type(value);
		xsputn(&byte, 1);
		return value;
	}
};

static bool template_read(const struct configuration *configuration, struct buffer *source, struct logging *logging)
{
	*source = {};
	const char *path = configuration->template_path;
	int         fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
	{
		logging_error(logging, "%s: cannot open page template: %s", path, strerror(errno));
		return false;
	}
	struct stat status;
	bool        success = false;
	if (fstat(fd, &status))
		logging_error(logging, "%s: cannot stat page template: %s", path, strerror(errno));
	else if (!S_ISREG(status.st_mode))
		logging_error(logging, "%s: page template must be a regular file", path);
	else
		success = document_read(fd, path, PAGE_TEMPLATE_LIMIT, source, logging);
	close(fd);
	if (!success)
		return false;
	if (!source->size)
	{
		logging_error(logging, "%s: page template must be nonempty", path);
		return false;
	}
	if (memchr(source->data, '\0', source->size))
	{
		logging_error(logging, "%s: page template contains a NUL byte", path);
		return false;
	}
	return true;
}

static inja::json template_data(const struct configuration *configuration, const char *title, const char *message, bool navigation, bool validating)
{
	inja::json links = inja::json::array();
	if (navigation)
	{
		for (size_t i = 0; i < configuration->navigation_count; i++)
		{
			const struct navigation_link *link = &configuration->navigation[i];
			links.push_back({{"label", link->label}, {"url", link->url}});
		}
		if (validating && links.empty())
			links.push_back({{"label", "Navigation"}, {"url", "/"}});
	}
	return {{"title", title}, {"site_title", configuration->title}, {"message", message ? message : ""}, {"error", message != nullptr}, {"navigation", std::move(links)}};
}

static bool template_expand(const struct configuration *configuration, const char *title, const struct buffer *content, const char *message, struct buffer *output, bool validating, struct logging *logging)
{
	struct buffer source = {};
	bool          success = false;
	if (!template_read(configuration, &source, logging))
		goto done;
	try
	{
		template_output sink(output);
		std::ostream    stream(&sink);
		/* ostream normally swallows streambuf exceptions. Rethrow them so an exhausted page budget stops evaluation immediately. */
		stream.exceptions(std::ios::badbit | std::ios::failbit);
		inja::Environment environment;
		environment.set_html_autoescape(true);
		environment.set_search_included_templates_in_files(false);
		/* The body remains borrowed C storage. Returning it as a JSON string would copy the complete body, and Inja would copy it again when evaluating the expression. */
		environment.add_void_callback("content", 0, [&](inja::Arguments &)
					      {
			if (content && content->size)
				stream.write(content->data, static_cast<std::streamsize>(content->size)); });
		auto layout = environment.parse(std::string_view(source.data, source.size));
		/* Inja owns the parsed template's source; release the read buffer before allocating metadata or expanding the page. */
		buffer_free(&source);
		if (validating)
		{
			/* Exercise representative document/error and empty/nonempty navigation paths. Arbitrary expressions can select other paths at request time. */
			for (unsigned combination = 0; combination < 4; combination++)
			{
				auto data = template_data(configuration, "Page", combination & 1 ? "Error" : nullptr, (combination & 2) != 0, true);
				environment.render_to(stream, layout, data);
			}
		}
		else
		{
			auto data = template_data(configuration, title, message, true, false);
			environment.render_to(stream, layout, data);
		}
		success = !output || !output->failed;
	}
	catch (const std::bad_alloc &)
	{
		logging_error(logging, "%s: cannot allocate page template state", configuration->template_path);
	}
	catch (const std::exception &failure)
	{
		/* Buffer failures already reported their cause before the stream threw. */
		if (!output || !output->failed)
			logging_error(logging, "%s: invalid Inja template: %s", configuration->template_path, failure.what());
	}
	catch (...)
	{
		logging_error(logging, "%s: page template expansion threw an unknown exception", configuration->template_path);
	}

done:
	buffer_free(&source);
	/* No exception crosses the C interface; every failure is sticky and requires the caller to discard incomplete output. */
	if (!success && output)
		output->failed = true;
	return success;
}

bool page_template_check(const struct configuration *configuration, struct logging *logging)
{
	return template_expand(configuration, nullptr, nullptr, nullptr, nullptr, true, logging);
}

bool page_template_render(const struct configuration *configuration, const char *title, const struct buffer *content, const char *message, struct buffer *output)
{
	return template_expand(configuration, title, content, message, output, false, output->logging);
}
