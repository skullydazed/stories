# Stories

Stories is a compact, file-backed documentation server written in C11. It renders Markdown on demand, redirects legacy document names using a configured directory order, and delegates static assets to CivetWeb. Disambiguation pages are ordinary, authored Markdown documents.

Stories serves HTTP behind a reverse proxy such as nginx, which handles TLS and response caching. FreeBSD is the primary deployment target, macOS the development platform, and Linux the CI environment. Development prioritizes correctness, security, and maintainability over performance.

## Requirements

- A C11 compiler, POSIX.1-2008 interfaces, and pthreads.
- CivetWeb 1.16 or newer, including development headers and file-serving support.
- MD4C 0.4.8 or newer, including development headers. Stories uses the parser API directly.
- Meson 1.3 or newer, Ninja, and Python 3.8 or newer for building and tests.
- Pico CSS 2.1.1 is included locally with its MIT license; there is no CDN or Node dependency.

The build uses installed libraries and never downloads dependency sources. Meson uses pkg-config when available, with a compiler library lookup fallback. Set `PKG_CONFIG_PATH`, `CFLAGS`, or `LDFLAGS` when libraries are outside the compiler's search paths.

On macOS:

```sh
brew install meson pkgconf civetweb md4c
```

Use the corresponding development packages on FreeBSD or Linux. The server requires CivetWeb's `mg_get_server_ports()` API to report the bound port.

## Build and run

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Copy [stories.conf.example](stories.conf.example) to `stories.conf`, create separate Markdown and asset roots, and adjust paths and navigation links. The local `stories.conf` is ignored by Git. Relative filesystem paths resolve against the selected configuration file's directory, independently of the process working directory.

```sh
./build/stories --config /path/to/stories.conf --check-config
./build/stories --config /path/to/stories.conf
```

Without `--config`, Stories checks `./stories.conf`, then `/etc/stories.conf`, and exits with an error if neither exists. The first existing file is authoritative: an unreadable or invalid file is fatal. An explicit `--config` never falls back to discovery.

`--listen ADDRESS:PORT` and `--threads COUNT` override those settings. `--help` and `--version` do not require a configuration. The default endpoint is `127.0.0.1:8080`, with eight worker threads. Use one numeric IPv4 or bracketed IPv6 address and a port; port zero requests an ephemeral port, reported on stdout. Multiple listeners and TLS suffixes are unsupported.

The server stays in the foreground. SIGINT and SIGTERM stop the listener and workers before application resources are released. Configuration changes require a restart. Markdown and asset edits, additions, and deletions appear on the next uncached request; Stories has no in-memory content index or response cache.

## Content and URLs

| File beneath the Markdown root | Public URL |
| --- | --- |
| `index.md` | `/` |
| `command/index.md` | `/command/` |
| `command/time.md` | `/command/time` |
| `disambiguation/time.md` | `/disambiguation/time` |

When `command/index.md` exists, `/command` and `/command/index` redirect to `/command/` with 301. `/index` redirects to `/`. A directory without `index.md` returns 404. Stories never generates directory listings, navigation trees, search results, or disambiguation pages. Navigation links must point to documents that you author.

Requests ending in `.md` or `.MD` return 404 throughout the server, including asset routes. Markdown source and legacy `.txt` files are not exposed as static content. Direct file and index resolution takes precedence over legacy-name lookup.

For unqualified legacy names, Stories searches `resolution_order` and redirects the first matching document with 301. For example, `/time` redirects to `/disambiguation/time` when that directory comes first and contains `time.md`.

A qualified name such as `/time_command` selects `command/time.md` and redirects to `/command/time`. A recognized suffix is authoritative: if the target is missing, the response is 404. Suffixes are literal configured directory names; Stories does not singularize words. If directory names contain underscores, the longest matching suffix wins. Resolution directories are unique top-level names, not nested paths; `assets` and `_stories` are reserved.

For EPIC, normalize directory names to singular and use:

```ini
resolution_order = disambiguation, command, function, on, ctcp, set, window, script, special
```

Markdown supports CommonMark plus tables, strikethrough, task lists, and automatic links. Stories rewrites inline and reference-style links to local `.md` documents. For example, `../function/time.md?q=x#details` in `command/time.md` becomes `/function/time?q=x#details`, and `index.md` becomes `/command/`. Rewriting does not require the destination to exist. Attempts to escape above the content root or malformed local destinations fail rendering.

External destinations, fragment-only links, image destinations, and embedded HTML remain as authored. Raw HTML is trusted and passes through, including active content; use valid extensionless URLs in its links. Heading IDs are not synthesized. Publishing is restricted to trusted maintainers.

## Configuration

The INI grammar has named sections and one `key = value` entry per line. Leading/trailing spaces and tabs are trimmed. Empty lines and whole-line `#` or `;` comments are ignored. Values are literal: there is no quoting, escaping, interpolation, or inline-comment syntax. NUL/control bytes, unknown sections/settings, duplicate scalar settings, and duplicate navigation labels are rejected. Configuration files are limited to 1 MiB.

| Setting | Default or requirement |
| --- | --- |
| `[server] listen` | `127.0.0.1:8080` |
| `[server] threads` | `8`; range 1–1024 |
| `[content] root` | Required existing directory |
| `[content] resolution_order` | Optional comma-separated ordered directory names |
| `[content] markdown_limit` | `8388608` bytes |
| `[content] output_limit` | `33554432` bytes |
| `[site] title` | `Stories` |
| `[navigation] Label = URL` | Optional, in file order; local paths or HTTP(S) URLs |
| `[assets] root` | Required existing directory, separate from Markdown |

Document limits must be positive and at most 1 GiB each. Exceeding either produces a complete 500 error response with no partial successful document. Roots are validated at startup; configured resolution directories may be added later. Asset paths cannot contain comma or equals because CivetWeb's rewrite mapping reserves those characters. Document and asset roots may not overlap.

## Static assets and HTTP

`/assets/...` maps to the configured asset root. `/_stories/...` maps to installed application assets, including Pico CSS. Unencoded asset paths fall through to CivetWeb; encoded paths are decoded and validated before their filenames are passed to CivetWeb's file-serving API. CivetWeb handles static MIME types, HEAD, and byte ranges in both cases. Its error callback uses Stories' shared shell and response helper so errors also respect HEAD semantics. Static responses default to zero cache lifetime; configure caching at the reverse proxy.

Directory listings, CGI, SSI, and compiled-in script recognizers are disabled. Static trees are deliberately public and operator-managed; CivetWeb's normal static-file and symlink behavior applies. Keep private files and symlinks to private trees out of them. Markdown access separately rejects symlinks at every component using descriptor-relative opens.

Stories completes all document responses, so missing documents never fall through to static serving. GET and HEAD are supported; other methods return 405 with `Allow: GET, HEAD`. Invalid paths return 400, missing documents 404, and filesystem/rendering failures 500. Dotfiles, dot segments, backslashes, and repeated separators are rejected. Automatic URI decoding is disabled; the HTTP adapter uses CivetWeb's decoder once and validates its full returned byte count, including embedded NULs. The resolver never decodes again. Queries remain opaque.

## Install and deploy

```sh
meson setup build-release --buildtype=release --prefix=/usr/local -Dtests=false
meson compile -C build-release
meson install -C build-release
```

Meson installs the binary, Pico stylesheet and license, example configuration, and project license. It does not install or overwrite `/etc/stories.conf`, create deployment roots, start services, or publish content. Installed application assets take precedence; an uninstalled development binary can use the checkout's `assets/` directory.

Run as an unprivileged account with read access to content and public assets. See [examples/stories.freebsd](examples/stories.freebsd) for rc.d supervision and [examples/nginx.conf](examples/nginx.conf) for TLS and caching. Adjust paths, ownership, hostname, certificates, and cache directories. Proxy caching can delay content updates and permanent redirect changes.

FreeBSD release verification should include a native build, `meson test`, a staged installation, asset serving, and SIGTERM under load. Provider-neutral Linux CI commands are in [tools/ci.sh](tools/ci.sh).

## Tests and fuzzing

`meson test` runs C subsystem tests and a Python standard-library HTTP suite against the built binary. Tests use a small checked-in corpus, isolated temporary roots, ephemeral ports, readiness polling, and bounded cleanup. They do not require or modify the ignored EPIC corpus. Discovery tests use the production search routine with isolated candidate files rather than changing `/etc`.

The HTTP suite checks redirects, indexes, GET/HEAD wire parity, static delegation and ranges, script disabling, root isolation, live updates, source/output limits, CLI behavior, failed startup, and shutdown under concurrent traffic.

```sh
CC=clang meson setup build-sanitize -Db_sanitize=address,undefined -Db_lundef=false -Dwerror=true
meson compile -C build-sanitize
meson test -C build-sanitize --print-errorlogs
```

Three opt-in libFuzzer targets exercise the production resolver, link rewriter, and config parser. They consume arbitrary byte spans with deterministic fixtures and ASan/UBSan instrumentation in the tested application code. The resolver consumes CivetWeb-decoded paths; link rewriting separately exercises percent-encoded destinations.

```sh
CC=clang meson setup build-fuzz -Dfuzzing=true -Db_sanitize=address,undefined -Db_lundef=false
meson compile -C build-fuzz
meson test -C build-fuzz --suite fuzz --print-errorlogs
```

Apple Clang may lack libFuzzer. On macOS install LLVM with `brew install llvm` and use `CC="$(brew --prefix llvm)/bin/clang"` when creating the fuzz build. Meson checks runtime support and fails clearly if explicitly requested fuzzing is unavailable. Normal builds do not require libFuzzer.

Smoke tests run for 30 seconds per target, never modify checked-in seeds, and save crash artifacts beneath the build directory. For a longer session:

```sh
mkdir -p build-fuzz/corpus-url build-fuzz/fuzz-artifacts/url_resolver
./build-fuzz/fuzz_url_resolver build-fuzz/corpus-url fuzz/corpus/url_resolver -max_total_time=3600 -artifact_prefix=build-fuzz/fuzz-artifacts/url_resolver/
```

Pass an artifact as the sole positional input to reproduce it. Use libFuzzer's `-minimize_crash=1 -exact_artifact_path=...` options to minimize a finding, and `-merge=1 NEW_CORPUS OLD_CORPUS` to minimize a corpus. Add minimized findings to the seeds and deterministic regression tests when fixing them.

## C conventions and lifecycle

Stories uses procedural, subsystem-oriented C with concrete names, private helpers, direct control flow, Allman braces, tabs, and no artificial line-length limit. [CODESTYLE.md](CODESTYLE.md) defines coding conventions. [docs/design.md](docs/design.md) describes ownership, callback contracts, filesystem policy, lifecycle sequencing, and test boundaries. `.clang-format` records formatting conventions.

The runtime has no mutable application globals. Process-level code owns configuration and server resources; each request owns its document and output buffers. Small shared utilities enforce bounds, escaping, and URL policy. Tables represent registries and mappings; library callbacks provide request and Markdown event dispatch.

## License

MIT; see [LICENSE.md](LICENSE.md). Pico CSS has its accompanying [MIT license](assets/LICENSE.pico.md). The HTML entity lookup is generated at build time from Python's standard-library HTML5 entity data.
