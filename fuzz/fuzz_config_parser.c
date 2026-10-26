#include "config.h"

#include <assert.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct configuration configuration;
	char                 error[128];
	bool                 parsed = configuration_parse((const char *)data, size, "/fixture", &configuration, error, sizeof(error));
	if (parsed)
	{
		assert(!error[0]);
		assert(configuration.content_root && configuration.content_root[0] == '/');
		assert(configuration.asset_root && configuration.asset_root[0] == '/');
		assert(configuration.template_path && configuration.template_path[0] == '/');
		assert(configuration.directory_count <= CONFIG_ITEMS);
		assert(configuration.navigation_count <= CONFIG_ITEMS);
		assert(configuration.threads && configuration.threads <= 1024);
	}
	else
	{
		assert(error[0]);
		assert(!configuration.listen && !configuration.content_root && !configuration.asset_root);
		assert(!configuration.template_path);
		assert(!configuration.directory_count && !configuration.navigation_count);
	}
	configuration_free(&configuration);
	return 0;
}
