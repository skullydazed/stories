#include "config.h"
#include "document.h"
#include "url.h"

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The only mutable global is harness-owned setup, never production state.
 * Open the immutable fixture tree once, not once per generated input. */
static int root_fd = -1;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	(void)argc;
	(void)argv;
	root_fd = open(TEST_FIXTURE_DIR, O_RDONLY | O_DIRECTORY);
	if (root_fd < 0)
		abort();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct configuration configuration = {.directory_count = 3};
	configuration.directories[0] = "disambiguation";
	configuration.directories[1] = "command";
	configuration.directories[2] = "function";
	struct document_route route;
	document_resolve(root_fd, &configuration, (const char *)data, size, &route);
	if (route.status == ROUTE_DOCUMENT || route.status == ROUTE_REDIRECT)
	{
		assert(route.fd >= 0);
		assert(route.source[0] && route.source[0] != '/');
		assert(!strstr(route.source, ".."));
		assert(route.location.size && route.location.data[0] == '/');
		assert(!url_markdown_extension(route.location.data, route.location.size));
		/* Fixture names need no escaping, so canonical locations resolve directly. */
		struct document_route canonical;
		document_resolve(root_fd, &configuration, route.location.data, route.location.size, &canonical);
		assert(canonical.status == ROUTE_DOCUMENT);
		assert(!strcmp(route.source, canonical.source));
		document_route_free(&canonical);
	}
	document_route_free(&route);
	return 0;
}
