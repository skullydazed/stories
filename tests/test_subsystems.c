#include "buffer.h"
#include "config.h"
#include "document.h"
#include "entities.h"
#include "render.h"
#include "page_template.h"
#include "url.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(expression)                                                                \
	do                                                                               \
	{                                                                                \
		if (!(expression))                                                       \
		{                                                                        \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
			exit(1);                                                         \
		}                                                                        \
	} while (0)

static struct configuration test_configuration(void)
{
	const char          *text = "[content]\nroot = docs\nresolution_order = disambiguation, command, function\n[assets]\nroot = assets\n[site]\ntemplate = page.html\ntitle = Test & <site>\n[navigation]\nHome = /\n";
	struct configuration configuration;
	char                 error[256];
	CHECK(configuration_parse(text, strlen(text), "/tmp", &configuration, error, sizeof(error)));
	return configuration;
}

static void test_buffers(void)
{
	struct buffer buffer;
	buffer_init(&buffer, 3);
	CHECK(buffer_append(&buffer, "abc", 3));
	CHECK(buffer.size == 3 && !strcmp(buffer.data, "abc"));
	CHECK(!buffer_puts(&buffer, "d"));
	CHECK(buffer.failed && buffer.size == 3);
	buffer_free(&buffer);
	buffer_init(&buffer, 64);
	CHECK(buffer_html(&buffer, "<&\"'", 4));
	CHECK(!strcmp(buffer.data, "&lt;&amp;&quot;&#39;"));
	buffer_free(&buffer);
	buffer_init(&buffer, 64);
	CHECK(buffer_url(&buffer, "/a +b", 5));
	CHECK(!strcmp(buffer.data, "/a%20%2Bb"));
	buffer_free(&buffer);
	buffer_init(&buffer, 0);
	CHECK(buffer_append(&buffer, "", 0));
	CHECK(!buffer_puts(&buffer, "a"));
	buffer_free(&buffer);
}

static void test_configuration_parser(void)
{
	struct configuration configuration = test_configuration();
	CHECK(configuration.threads == 8);
	CHECK(!configuration.access_log_path && !configuration.error_log_path);
	CHECK(configuration.directory_count == 3);
	CHECK(!strcmp(configuration.content_root, "/tmp/docs"));
	CHECK(!strcmp(configuration.template_path, "/tmp/page.html"));
	CHECK(!strcmp(configuration.navigation[0].url, "/"));
	configuration_free(&configuration);
	const char *invalid[] = {
		"",
		"[unknown]\nx = y",
		"[server]\nthreads=0",
		"[server]\nthreads=9999999999999999999999999999999999999999999",
		"[content]\nroot=a\nroot=b",
		"[content]\nroot=a\nresolution_order=command,,function",
		"[content]\nroot=a\nresolution_order=command,command",
		"[content]\nroot=a\nresolution_order=../private",
		"[content]\nroot=a\nresolution_order=assets",
		"[navigation]\nBad=javascript:alert(1)",
		"[navigation]\nBad=//example.org",
		"[navigation]\nHome=/\nHome=/other",
		"[server]\nlisten=127.0.0.1:443s",
		"[server]\nlisten=127.0.0.1:1,127.0.0.1:2",
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		char error[256];
		CHECK(!configuration_parse(invalid[i], strlen(invalid[i]), "/tmp", &configuration, error, sizeof(error)));
		CHECK(configuration.content_root == NULL && configuration.directory_count == 0 && configuration.listen == NULL);
		CHECK(error[0]);
		configuration_free(&configuration);
	}
	char        error[256];
	const char *logs = "[content]\nroot=docs\n[assets]\nroot=assets\n[site]\ntemplate=page.html\n[logging]\naccess_log=logs/access.log\nerror_log=/var/log/stories-error.log\n";
	CHECK(configuration_parse(logs, strlen(logs), "/tmp", &configuration, error, sizeof(error)));
	CHECK(!strcmp(configuration.access_log_path, "/tmp/logs/access.log"));
	CHECK(!strcmp(configuration.error_log_path, "/var/log/stories-error.log"));
	configuration_free(&configuration);
	CHECK(!configuration_parse("[site]\ntitle=x\0hidden", 21, "/tmp", &configuration, error, sizeof(error)));
	size_t number;
	CHECK(configuration_unsigned("1024", 1024, &number) && number == 1024);
	CHECK(!configuration_unsigned("1025", 1024, &number));
	CHECK(!configuration_unsigned("-1", 1024, &number));
	CHECK(configuration_listen_valid("[::1]:8080"));
	CHECK(!configuration_listen_valid("::1:8080"));
	CHECK(!configuration_listen_valid("999.999.999.999:8080"));
	CHECK(!configuration_listen_valid("dead:8080"));
	char *oversize = malloc(CONFIG_LIMIT + 1);
	CHECK(oversize);
	memset(oversize, 'x', CONFIG_LIMIT + 1);
	CHECK(!configuration_parse(oversize, CONFIG_LIMIT + 1, "/tmp", &configuration, error, sizeof(error)));
	free(oversize);
}

static void test_configuration_diagnostics(void)
{
	static const struct
	{
		const char *input;
		const char *diagnostic;
	} cases[] = {
		{"[logging]\naccess_log=\n", "line 2: [logging] access_log: value must not be empty"},
		{"[logging]\nerror_log=a\nerror_log=b\n", "line 3: [logging] error_log: duplicate setting"},
		{"[server]\nthreads=0\n", "line 2: [server] threads: expected an integer from 1 to 1024"},
		{"[server]\nthreads=8\nthreads=4\n", "line 3: [server] threads: duplicate setting"},
		{"[server]\nthreds=8\n", "line 2: [server] threds: unknown setting"},
		{"[server]\nlisten=localhost:8080\n", "line 2: [server] listen: expected a numeric IPv4 or bracketed IPv6 address and port"},
		{"[unknown]\n", "line 1: [unknown]: unknown section"},
		{"[server\n", "line 1: expected a section header"},
		{"[", "line 1: expected a section header"},
		{"[]", "line 1: expected a section header"},
		{"[server]tail", "line 1: expected a section header"},
		{"[server]\nthreads\n", "line 2: [server]: expected key = value"},
		{"threads=4\n", "line 1: setting appears before a section header"},
		{"[site]\n=value\n", "line 2: [site]: setting name must not be empty"},
		{"[site]\ntitle=\n", "line 2: [site] title: value must not be empty"},
		{"[assets]\nroot=a,b\n", "line 2: [assets] root: path must not contain ',' or '='"},
		{"[content]\nresolution_order=command,,function\n", "line 2: [content] resolution_order: directory names must not be empty"},
		{"[content]\nresolution_order=command,command\n", "line 2: [content] resolution_order: duplicate directory"},
		{"[content]\nresolution_order=assets\n", "line 2: [content] resolution_order: directory name 'assets' is reserved"},
		{"[content]\noutput_limit=-1\n", "line 2: [content] output_limit: expected a byte limit"},
		{"[navigation]\nHome=//example.org\n", "line 2: [navigation] Home: URL must be a local path"},
		{"[navigation]\nHome=/\nHome=/other\n", "line 3: [navigation] Home: duplicate navigation label"},
		{"", "missing required setting [content] root"},
		{"[content]\nroot=docs\n", "missing required setting [assets] root"},
		{"[content]\nroot=docs\n[assets]\nroot=assets\n", "missing required setting [site] template"},
	};
	struct configuration configuration;
	char                 error[512];
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		CHECK(!configuration_parse(cases[i].input, strlen(cases[i].input), "/tmp", &configuration, error, sizeof(error)));
		CHECK(strstr(error, cases[i].diagnostic));
		CHECK(!configuration.listen && !configuration.content_root && !configuration.asset_root);
		configuration_free(&configuration);
	}
	char  control[] = "[content]\nroot=docs\n[assets]\nroot=assets\n[site]\ntitle=before?after\n";
	char *marker = strchr(control, '?');
	CHECK(marker);
	for (unsigned byte = 0; byte <= 127; byte++)
	{
		if (!((byte < 32 && byte != '\n' && byte != '\r' && byte != '\t') || byte == 127))
			continue;
		*marker = (char)byte;
		CHECK(!configuration_parse(control, sizeof(control) - 1, "/tmp", &configuration, error, sizeof(error)));
		char diagnostic[80];
		snprintf(diagnostic, sizeof(diagnostic), "line 6: disallowed control byte 0x%02X", byte);
		CHECK(strstr(error, diagnostic));
		configuration_free(&configuration);
	}
	const char *valid = "[content]\r\nroot = docs\r\n[assets]\r\nroot = assets\r\n[site]\r\ntemplate = page.html\r\ntitle = Tab\tvalue\r\n";
	CHECK(configuration_parse(valid, strlen(valid), "/tmp", &configuration, error, sizeof(error)));
	configuration_free(&configuration);
}

static void test_links(void)
{
	static const struct
	{
		const char *input;
		const char *source;
		const char *expected;
	} cases[] = {
		{"../function/time.md#details", "command/time.md", "/function/time#details"},
		{"../function/time.md?a=1&b=2#details", "command/time.md", "/function/time?a=1&b=2#details"},
		{"./index.md", "command/time.md", "/command/"},
		{"../index.md", "command/time.md", "/"},
		{"plain.md", "command/index.md", "/command/plain"},
		{"/command/index.md", "index.md", "/command/"},
		{"a%20b.md", "command/time.md", "/command/a%20b"},
		{"a%2Bb.md", "command/time.md", "/command/a%2Bb"},
		{"time%2emd#x", "command/plain.md", "/command/time#x"},
		{"time.%6d%64#x", "command/plain.md", "/command/time#x"},
		{"time%2e%6d%64#x", "command/plain.md", "/command/time#x"},
		{"deals%", "command/time.md", "deals%"},
		{"deals%GG", "command/time.md", "deals%GG"},
		{"deals%?next=time.md#discount", "command/time.md", "deals%?next=time.md#discount"},
		{"/assets/sale%.png", "command/time.md", "/assets/sale%.png"},
		{"https://example.org/readme.md?q=x", "index.md", "https://example.org/readme.md?q=x"},
		{"//example.org/readme.md", "index.md", "//example.org/readme.md"},
		{"mailto:person@example.org", "index.md", "mailto:person@example.org"},
		{"#readme.md", "index.md", "#readme.md"},
		{"?readme.md", "index.md", "?readme.md"},
		{"../assets/image.png", "command/time.md", "../assets/image.png"},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		struct buffer output;
		buffer_init(&output, 16384);
		CHECK(url_rewrite_link(cases[i].input, strlen(cases[i].input), cases[i].source, &output));
		CHECK(!strcmp(output.data, cases[i].expected));
		buffer_free(&output);
	}
	const char *bad[] = {"../../outside.md", "file%00.md", "%GG.md", "file\\name.md", "file\n.md", "deals%%2e%6d%64", "file.html.md", "file.css.md"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		struct buffer output;
		buffer_init(&output, 16384);
		CHECK(!url_rewrite_link(bad[i], strlen(bad[i]), "index.md", &output));
		buffer_free(&output);
	}
	const char *invalid[] = {"", "relative", "//host", "/../secret", "/command/../secret", "/command//time", "/.hidden", "/bad\\path", "/bad?query"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		CHECK(!url_path_valid(invalid[i], strlen(invalid[i])));
	CHECK(!url_path_valid("/abc\0hidden", 11));
	CHECK(url_path_valid("/command/time", 13));
	CHECK(url_path_valid("/command/", 9));
	CHECK(!url_path_has_extension("/command/time", strlen("/command/time")));
	CHECK(!url_path_has_extension("/command/", strlen("/command/")));
	CHECK(url_path_has_extension("/command/time.html", strlen("/command/time.html")));
	CHECK(url_path_has_extension("/command.html/", strlen("/command.html/")));
}

static void write_configuration(const char *path, const char *title)
{
	FILE *file = fopen(path, "wb");
	CHECK(file);
	CHECK(fprintf(file, "[content]\nroot=docs\n[assets]\nroot=assets\n[site]\ntemplate=page.html\ntitle=%s\n", title) > 0);
	CHECK(!fclose(file));
}

static void test_configuration_discovery(void)
{
	char directory[] = "/tmp/stories-config-XXXXXX";
	CHECK(mkdtemp(directory));
	char local[256], system[256], explicit_path[256];
	snprintf(local, sizeof(local), "%s/local.conf", directory);
	snprintf(system, sizeof(system), "%s/system.conf", directory);
	snprintf(explicit_path, sizeof(explicit_path), "%s/explicit.conf", directory);
	const char          *search[] = {local, system};
	struct configuration configuration;
	char                 error[512];
	CHECK(!configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	write_configuration(system, "system");
	CHECK(configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	CHECK(!strcmp(configuration.title, "system"));
	configuration_free(&configuration);
	write_configuration(local, "local");
	CHECK(configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	CHECK(!strcmp(configuration.title, "local"));
	configuration_free(&configuration);
	CHECK(!configuration_load_search(explicit_path, search, 2, &configuration, error, sizeof(error)));
	write_configuration(explicit_path, "explicit");
	CHECK(configuration_load_search(explicit_path, search, 2, &configuration, error, sizeof(error)));
	CHECK(!strcmp(configuration.title, "explicit"));
	configuration_free(&configuration);
	FILE *file = fopen(local, "wb");
	CHECK(file && fputs("[invalid]\n", file) >= 0 && !fclose(file));
	CHECK(!configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	CHECK(!unlink(local));
	CHECK(!symlink("missing-target", local));
	CHECK(!configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	CHECK(!unlink(local));
	CHECK(!mkfifo(local, 0600));
	CHECK(!configuration_load_search(NULL, search, 2, &configuration, error, sizeof(error)));
	CHECK(!unlink(local));
	const char   tail[] = "[content]\nroot=docs\n[assets]\nroot=assets\n[site]\ntemplate=page.html\ntitle=block boundary\n";
	const size_t sizes[] = {4095, 4096, 4097, 8192, 8193};
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		FILE *boundary_file = fopen(explicit_path, "wb");
		CHECK(boundary_file);
		/* Put the settings at EOF so a dropped partial block cannot pass. */
		for (size_t padding = sizeof(tail) - 1; padding < sizes[i]; padding++)
			CHECK(fputc('\n', boundary_file) != EOF);
		CHECK(fwrite(tail, 1, sizeof(tail) - 1, boundary_file) == sizeof(tail) - 1);
		CHECK(!fclose(boundary_file));
		CHECK(configuration_load_search(explicit_path, search, 2, &configuration, error, sizeof(error)));
		CHECK(!strcmp(configuration.title, "block boundary"));
		configuration_free(&configuration);
	}
	CHECK(!unlink(system));
	CHECK(!unlink(explicit_path));
	CHECK(!rmdir(directory));
}

static void test_resolver(void)
{
	struct configuration configuration = test_configuration();
	int                  root = open(TEST_FIXTURE_DIR, O_RDONLY | O_DIRECTORY);
	CHECK(root >= 0);
	static const struct
	{
		const char       *path;
		enum route_status status;
		const char       *source;
		const char       *location;
	} cases[] = {
		{"/", ROUTE_DOCUMENT, "index.md", "/"},
		{"/index", ROUTE_REDIRECT, "index.md", "/"},
		{"/command/time", ROUTE_DOCUMENT, "command/time.md", "/command/time"},
		{"/time", ROUTE_REDIRECT, "disambiguation/time.md", "/disambiguation/time"},
		{"/time_command", ROUTE_REDIRECT, "command/time.md", "/command/time"},
		{"/plain_function", ROUTE_MISSING, "", ""},
		{"/command", ROUTE_REDIRECT, "command/index.md", "/command/"},
		{"/command/index", ROUTE_REDIRECT, "command/index.md", "/command/"},
		{"/command/", ROUTE_DOCUMENT, "command/index.md", "/command/"},
		{"/function/", ROUTE_MISSING, "", ""},
		{"/command/time.md", ROUTE_MISSING, "", ""},
		{"/command/time.html", ROUTE_MISSING, "", ""},
		{"/command/time.txt", ROUTE_MISSING, "", ""},
		{"/missing", ROUTE_MISSING, "", ""},
		{"/command/../index", ROUTE_INVALID, "", ""},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		struct document_route route;
		document_resolve(root, &configuration, cases[i].path, strlen(cases[i].path), &route);
		CHECK(route.status == cases[i].status);
		if (route.status == ROUTE_DOCUMENT || route.status == ROUTE_REDIRECT)
		{
			CHECK(route.fd >= 0);
			CHECK(!strcmp(route.source, cases[i].source));
			CHECK(!strcmp(route.location.data, cases[i].location));
			struct buffer content;
			CHECK(document_read(route.fd, 65536, &content));
			CHECK(content.size);
			buffer_free(&content);
		}
		document_route_free(&route);
	}
	close(root);
	configuration_free(&configuration);
}

static void test_template(struct configuration *configuration)
{
	free(configuration->template_path);
	configuration->template_path = strdup(TEST_FIXTURE_DIR "/../../templates/page.html");
	CHECK(configuration->template_path);
	char error[512];
	CHECK(page_template_check(configuration, error, sizeof(error)));
}

static void test_renderer(void)
{
	struct configuration configuration = test_configuration();
	test_template(&configuration);
	const char   *markdown = "# Heading\n\n[inline](../function/time.md?a=1&b=2#x)\n\n[reference][r]\n\n[r]: index.md \"Title &amp; quote\"\n\n[encoded](time&#46;md)\n\n![*rich* <b>label</b>](../assets/image.png \"image title\")\n\n~~gone~~\n\n- [x] done\n\n| A | B |\n|---|---|\n| one | two |\n\n```c\n<code>&\n```\n\nhttps://example.org\n\n<a href=\"raw.md\">raw HTML</a>\n\n&copy; &#x1F600; &NotEqualTilde;\n";
	struct buffer output;
	CHECK(render_document(&configuration, "command/time.md", markdown, strlen(markdown), &output));
	const char *expected[] = {"<h1>Heading</h1>", "href=\"/function/time?a=1&amp;b=2#x\"", "href=\"/command/\"", "href=\"/command/time\"", "title=\"Title &amp; quote\"", "src=\"../assets/image.png\"", "alt=\"rich &lt;b&gt;label&lt;/b&gt;\"", "<del>gone</del>", "disabled checked", "<table>", "language-c", "&lt;code&gt;&amp;", "href=\"https://example.org\"", "<a href=\"raw.md\">", "Test &amp; &lt;site&gt;", "\xc2\xa9"};
	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
		CHECK(strstr(output.data, expected[i]));
	buffer_free(&output);
	const char *invalid_links[] = {
		"before [x](../../outside.md) after",
		"before [a](%GG.md) after",
		"before [reference][target] after\n\n[target]: ../../outside.md\n",
		"before [bad](file%00.md) after",
	};
	for (size_t i = 0; i < sizeof(invalid_links) / sizeof(invalid_links[0]); i++)
	{
		CHECK(!render_document(&configuration, "command/time.md", invalid_links[i], strlen(invalid_links[i]), &output));
		CHECK(output.failed);
		buffer_free(&output);
	}
	const char *percent_label = "[100%](x)";
	CHECK(render_document(&configuration, "command/time.md", percent_label, strlen(percent_label), &output));
	CHECK(strstr(output.data, "<a href=\"x\">100%</a>"));
	buffer_free(&output);
	configuration.output_limit = 32;
	CHECK(!render_document(&configuration, "index.md", "# too big", 9, &output));
	buffer_free(&output);
	CHECK(render_error(&configuration, "Error", "<escaped>", &output));
	CHECK(strstr(output.data, "&lt;escaped&gt;"));
	buffer_free(&output);
	configuration_free(&configuration);
}

static void test_renderer_titles(void)
{
	struct configuration configuration = test_configuration();
	test_template(&configuration);
	static const struct
	{
		const char *markdown;
		const char *title;
	} cases[] = {
		{"## First heading\n\n# Later heading\n", "First heading"},
		{"Setext heading\n==============\n", "Setext heading"},
		{"First line\nsecond line\n===========\n", "First line second line"},
		{"# *Hello* **world** `code` [link](time.md) &amp; ![image](/assets/image.png)\n", "Hello world code link &amp; image"},
		{"# <em>Heading</em>\n", "Heading"},
		{"# `</title><script>bad</script>`\n", "&lt;/title&gt;&lt;script&gt;bad&lt;/script&gt;"},
		{"# &#169; &amp; &#0;\n", "\xc2\xa9 &amp; \xef\xbf\xbd"},
		{"Body without a heading.\n", "plain"},
		{"#\n\n# Later heading\n", "plain"},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		struct buffer output;
		CHECK(render_document(&configuration, "command/plain.md", cases[i].markdown, strlen(cases[i].markdown), &output));
		char expected[512];
		snprintf(expected, sizeof(expected), "<title>%s — Test &amp; &lt;site&gt;</title>", cases[i].title);
		CHECK(strstr(output.data, expected));
		buffer_free(&output);
	}
	configuration_free(&configuration);
}

int main(void)
{
	test_buffers();
	test_configuration_parser();
	test_configuration_diagnostics();
	test_configuration_discovery();
	test_links();
	test_resolver();
	test_renderer();
	test_renderer_titles();
	puts("subsystem tests passed");
	return 0;
}
