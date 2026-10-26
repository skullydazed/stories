#include "url.h"
#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static const char *sources[] = {"index.md", "command/time.md", "command/index.md", "nested/directory/page.md"};
	const char        *source = sources[size ? data[0] % 4 : 0];
	if (size)
	{
		data++;
		size--;
	}
	struct buffer output;
	buffer_init(&output, 32768);
	bool rewritten = url_rewrite_link((const char *)data, size, source, &output);
	if (rewritten)
	{
		assert(!output.failed);
		assert(!memchr(output.data, '\0', output.size));
		const char *suffix = memchr(data, '?', size);
		const char *fragment = memchr(data, '#', size);
		if (!suffix || (fragment && fragment < suffix))
			suffix = fragment;
		if (suffix)
		{
			size_t suffix_size = size - (size_t)(suffix - (const char *)data);
			assert(output.size >= suffix_size);
			assert(!memcmp(output.data + output.size - suffix_size, suffix, suffix_size));
		}
		/* Rewritten local Markdown links are root-relative and have no dot segments. */
		if (output.size && output.data[0] == '/' && (output.size < 2 || output.data[1] != '/') && (output.size != size || memcmp(output.data, data, size)))
		{
			size_t path_size = strcspn(output.data, "?#");
			assert(path_size && !url_markdown_extension(output.data, path_size));
			for (size_t i = 0; i + 4 <= path_size; i++)
				assert(memcmp(output.data + i, "/../", 4));
		}
	}
	buffer_free(&output);
	return 0;
}
