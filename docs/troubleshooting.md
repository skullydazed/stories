# Troubleshooting

Start with the server's diagnostic output and a direct backend request. Access logs default to stdout and errors to stderr; `[logging] access_log` and `error_log` select append-mode files. See [Logging](configuration.md#logging) for CLI overrides and SIGUSR1 reopening. A reverse proxy can cache a response that differs from the current document or configuration.

## The server will not start

Run the same binary and configuration without starting a listener:

```sh
stories --config /path/to/stories.conf --check-config
```

If discovery reports no configuration, provide `--config` or place `stories.conf` in the working directory or at `/etc/stories.conf`. An explicit file never falls back to discovery. An existing but invalid discovery candidate is fatal.

For a configuration diagnostic, inspect the named file, line, section, and setting. Values have no quoting or inline-comment syntax. A value followed by `# a comment` includes those bytes in the value; put comments on their own lines instead.

Both roots must be existing directories, readable by the process account, and disjoint. Relative paths start at the process working directory. Check the directory from which Stories was started, or use absolute paths. Configuration validation does not require every navigation target or configured resolution directory to exist.

`[site] template` is required, including in existing configurations when upgrading. It must select a readable, nonempty regular file containing valid Inja markup, limited to 1 MiB with no NUL bytes. The supplied layout is `templates/page.html` in a checkout or `share/stories/templates/page.html` in an installation. Template diagnostics name the selected file; see [Themes and page layout](themes.md) for supported values, loops, and conditionals.

If the listener fails, check that its numeric address is available and its port is free. Stories supports one HTTP endpoint, with bracketed IPv6 syntax when applicable. You can use `--listen 127.0.0.1:0` to request an ephemeral port; startup reports the selected port on stdout.

## A page returns 404

Check the mapping between the public URL and the Markdown tree:

- `/` needs `index.md` at the content root.
- `/guide` normally needs `guide.md`, or redirects to `/guide/` when `guide/index.md` exists.
- `/guide/` needs `guide/index.md`.
- `/guide.md` and extension-bearing document URLs return 404.

Documents must be regular files. Symlinks are rejected at every Markdown path component, and hidden components are not published. Check directory traversal and file read permissions for the process account. A linked destination may be missing even when its referring page renders successfully.

For legacy names, inspect `resolution_order`. Direct document and index lookup takes precedence. A recognized qualified suffix selects its directory authoritatively; a missing qualified target does not fall through to another directory.

## A page returns 400 or 405

Malformed decoded paths, including dot segments, repeated separators, backslashes, and control bytes visible to Stories, return 400. The nginx example rejects `%00` in the original request URI, including query parameters. Direct backend requests retain CivetWeb's native NUL truncation behavior. GET and HEAD are supported; other methods return 405 with `Allow: GET, HEAD`.

## A page returns 500

A 500 can result from a document read failure, allocation failure, an exceeded Markdown/output limit, or an invalid local Markdown link. Stories discards incomplete output rather than returning a partial successful page.

Check recent link edits for malformed percent escapes in `.md` destinations, attempts to escape the content root with `..`, and names such as `page.html.md` that cannot form an extensionless document URL. Also check the file's source size and configured `output_limit`, which applies to the complete rendered page. See [Configuration](configuration.md) and [Writing and publishing](writing.md).

## The layout has no stylesheet

The supplied template links `/assets/pico.min.css`, `/assets/dark_toggle.css`, `/assets/theme.css`, and `/assets/site.css`; custom templates may select different stylesheets. Request that stylesheet URL directly. The file must be present in the configured asset root. The checkout includes these files under `assets/`, and installations include them under `share/stories/assets`. If the Color scheme control is missing, also check that `/assets/dark_toggle.js` loads and JavaScript is enabled.

Stories does not search a source-tree or installed fallback directory. A missing stylesheet returns 404 without preventing startup. If other assets fail, verify their `/assets/` URL and the configured root. HTML/XHTML files, Markdown sources, directory listings, and implicit static index pages are deliberately unavailable.

## Changes are not visible

Document and asset edits appear on the next uncached request. Compare the backend's loopback response with the public proxy response. The supplied nginx example caches 200 and 301 responses for 60 seconds, so edits and redirect changes can remain stale during that interval.

Configuration and navigation changes require a restart. HTML template edits appear on the next uncached request, without restarting or rebuilding Stories. If the entire content-root directory was replaced, restart as well: the server holds an open descriptor for that root. Atomic replacement of individual files inside the existing root is supported.

## Build or analysis failures

Check that the CivetWeb and MD4C development headers and libraries are installed. CMake checks the required interfaces during configuration. For dependencies under a custom prefix, use `CMAKE_PREFIX_PATH`.

Sanitizer and fuzz configurations need matching compiler runtimes. Apple Clang can build the normal server while lacking libFuzzer; select a full LLVM Clang installation for fuzz builds. Clang analyzer findings print with their path notes in the build and CI logs and fail the analysis build.

[Deployment](deployment.md) · [Development](development.md) · [Documentation home](index.md)
