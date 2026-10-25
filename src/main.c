#include "build_config.h"
#include "config.h"
#include "page_template.h"
#include "server.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
	OPT_CHECK = 256,
	OPT_LISTEN,
	OPT_THREADS,
	OPT_VERSION
};
static const struct option command_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"check-config", no_argument, NULL, OPT_CHECK},
	{"listen", required_argument, NULL, OPT_LISTEN},
	{"threads", required_argument, NULL, OPT_THREADS},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, OPT_VERSION},
	{NULL, 0, NULL, 0},
};

static void usage(FILE *output)
{
	fputs("Usage: stories [options]\n"
	      "\n"
	      "Serve Markdown documentation over HTTP.\n"
	      "\n"
	      "Options:\n"
	      "  -c, --config PATH          Read configuration from PATH.\n"
	      "      --listen ADDRESS:PORT  Override the configured listening address.\n"
	      "      --threads COUNT        Override the configured worker count (1-1024).\n"
	      "      --check-config         Validate configuration, roots, and page template, then exit.\n"
	      "  -h, --help                 Show this help and exit.\n"
	      "      --version              Show the version and exit.\n"
	      "\n"
	      "Without --config, check ./stories.conf, then /etc/stories.conf; exit with an error if neither exists.\n"
	      "Configuration defaults: listen=127.0.0.1:8080, threads=8.\n",
	      output);
}

int main(int argc, char **argv)
{
	const char *config_path = NULL, *listen = NULL, *threads = NULL;
	bool        check = false;
	int         option;
	while ((option = getopt_long(argc, argv, "c:h", command_options, NULL)) != -1)
	{
		switch (option)
		{
		case 'c':
			config_path = optarg;
			break;
		case 'h':
			usage(stdout);
			return 0;
		case OPT_VERSION:
			puts("stories " STORIES_VERSION);
			return 0;
		case OPT_CHECK:
			check = true;
			break;
		case OPT_LISTEN:
			listen = optarg;
			break;
		case OPT_THREADS:
			threads = optarg;
			break;
		default:
			usage(stderr);
			return 1;
		}
	}
	if (optind != argc)
	{
		usage(stderr);
		return 1;
	}
	struct configuration configuration;
	char                 error[512];
	if (!configuration_load(config_path, &configuration, error, sizeof(error)))
	{
		fprintf(stderr, "%s\n", error);
		return 1;
	}
	int result = 1;
	if (listen)
	{
		if (!configuration_listen_valid(listen))
		{
			fprintf(stderr, "invalid --listen endpoint\n");
			goto done;
		}
		char *copy = strdup(listen);
		if (!copy)
			goto done;
		free(configuration.listen);
		configuration.listen = copy;
	}
	if (threads)
	{
		size_t number;
		if (!configuration_unsigned(threads, 1024, &number) || !number)
		{
			fprintf(stderr, "--threads must be between 1 and 1024\n");
			goto done;
		}
		configuration.threads = (unsigned)number;
	}
	if (!page_template_check(&configuration, error, sizeof(error)))
	{
		fprintf(stderr, "%s\n", error);
		goto done;
	}
	if (check)
	{
		if (!server_check_roots(&configuration, error, sizeof(error)))
			fprintf(stderr, "%s\n", error);
		else
		{
			puts("Configuration OK");
			result = 0;
		}
	}
	else
		result = server_run(&configuration);

done:
	configuration_free(&configuration);
	return result;
}
