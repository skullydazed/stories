# Stories

Stories is a compact, file-backed documentation server written in C11. It renders Markdown on demand, redirects legacy document names using a configured directory order, and delegates static assets to CivetWeb. Disambiguation pages are ordinary, authored Markdown documents.

Stories serves HTTP behind a reverse proxy such as nginx, which handles TLS and response caching. FreeBSD is the primary deployment target, macOS the development platform, and Linux the CI environment. Development prioritizes correctness, security, and maintainability over performance.

## Requirements

- A C11 compiler, POSIX.1-2008 interfaces, and pthreads.
- CivetWeb 1.16 or newer, including development headers and file-serving support.
- MD4C 0.4.8 or newer, including development headers. Stories uses the parser API directly.
- CMake 3.20 or newer, a supported build tool such as Make or Ninja, and Python 3.8 or newer for building and tests.
- Optional pkgconf for build-time dependency discovery; CMake searches for headers and libraries directly when it is unavailable.
- Pico CSS 2.1.1 is included locally with its MIT license; there is no CDN or Node dependency.

Normal CMake builds use installed libraries and never download dependency sources. CMake uses pkgconf metadata when available and searches directly for headers and libraries otherwise. Use `CMAKE_PREFIX_PATH` when dependencies are installed outside standard prefixes. CI and release builds explicitly use [tools/build-dependencies.py](tools/build-dependencies.py) to resolve the latest upstream CivetWeb and MD4C release tags on every run and build static libraries in an isolated prefix. The release manifest records the selected tags, commits, and downloaded source checksums. CI installs CMake through the platform's package manager.

On macOS:

```sh
brew install cmake pkgconf civetweb md4c
```

Use the corresponding development packages on FreeBSD or Linux. The server requires CivetWeb's `mg_get_server_ports()` API to report the bound port.

## Build and run

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Compiler selection prefers `clang`, then `gcc`, then `cc`. Explicit compiler settings and toolchain files take precedence. Tests are enabled by default.

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

Document URLs must be extensionless: a dot in the final path component returns 404, even if a backing file such as `time.html.md` exists. The same rule applies to directory index URLs. CSS, image, and other static asset URLs may retain extensions under `/assets/`. Requests ending in `.md` or `.MD` return 404 throughout the server, including asset routes. Markdown source and legacy `.txt` files in the content root are not exposed as static content. Direct file and index resolution takes precedence over legacy-name lookup.

For unqualified legacy names, Stories searches `resolution_order` and redirects the first matching document with 301. For example, `/time` redirects to `/disambiguation/time` when that directory comes first and contains `time.md`.

A qualified name such as `/time_command` selects `command/time.md` and redirects to `/command/time`. A recognized suffix is authoritative: if the target is missing, the response is 404. Suffixes are literal configured directory names; Stories does not singularize words. If directory names contain underscores, the longest matching suffix wins. Resolution directories are unique top-level names, not nested paths; `assets` is reserved.

For EPIC, normalize directory names to singular and use:

```ini
resolution_order = disambiguation, command, function, on, ctcp, set, window, script, special
```

Markdown supports CommonMark plus tables, strikethrough, task lists, and automatic links. Stories rewrites inline and reference-style links to local `.md` documents, including percent-encoded `.md` suffixes. For example, `../function/time.md?q=x#details` in `command/time.md` becomes `/function/time?q=x#details`, and `index.md` becomes `/command/`. Rewriting does not require the destination to exist. Invalid Markdown destinations, root escapes, and targets that cannot map to extensionless document URLs fail rendering. Other link destinations pass through unchanged, including extensionless links such as `deals%` and asset URLs. Passing a link through does not make its target a supported document route.

External destinations, fragment-only links, image destinations, and embedded HTML remain as authored. Raw HTML is trusted and passes through, including active content; use valid extensionless URLs in its links. Heading IDs are not synthesized. Publishing is restricted to trusted maintainers.

The page title uses the first Markdown heading at any level, followed by the configured site title. Inline formatting is reduced to text, entities are decoded, and the result is escaped for HTML. If the heading is absent or empty, the extensionless document basename is used instead.

## Configuration

The INI grammar has named sections and one `key = value` entry per line. Leading/trailing spaces and tabs are trimmed. Empty lines and whole-line `#` or `;` comments are ignored. Values are literal: there is no quoting, escaping, interpolation, or inline-comment syntax. ASCII control bytes, including NUL and DEL, are rejected except for tabs and CR/LF used by the grammar. Unknown sections/settings, duplicate scalar settings, and duplicate navigation labels are rejected. Diagnostics identify the file, line, section, and setting where applicable, with the reason for rejection. Configuration files are limited to 1 MiB.

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

`/assets/...` maps only to the configured asset root. The page shell references `/assets/pico.min.css`, an ordinary file in that root. The example configuration points to the checkout's `assets/` directory, which includes the stylesheet. If you configure a different root, place the stylesheet there; the server does not search installed or source-tree fallback directories. Unencoded asset paths fall through to CivetWeb; encoded paths are decoded and validated before their filenames are passed to CivetWeb's file-serving API. CivetWeb handles static MIME types, HEAD, and byte ranges in both cases. Its error callback uses Stories' shared shell and response helper so errors also respect HEAD semantics. Static responses default to zero cache lifetime; configure caching at the reverse proxy.

Static HTML and XHTML files return 404, using CivetWeb's MIME classification to enforce the same policy for encoded and unencoded paths. Implicit static index files, directory listings, CGI, SSI, and compiled-in script recognizers are disabled. Static trees are deliberately public and operator-managed; CivetWeb's normal static-file and symlink behavior otherwise applies. Keep private files and symlinks to private trees out of them. Markdown access separately rejects symlinks at every component using descriptor-relative opens.

Stories completes all document responses, so missing documents never fall through to static serving. GET and HEAD are supported; other methods return 405 with `Allow: GET, HEAD`. Invalid paths return 400, missing documents 404, and filesystem/rendering failures 500. Dotfiles, dot segments, backslashes, and repeated separators are rejected. Automatic URI decoding is disabled; the HTTP adapter uses CivetWeb's decoder once and validates its full returned byte count, including embedded NULs. The resolver never decodes again. Queries remain opaque.

## Install and deploy

```sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_TESTING=OFF
cmake --build build-release
cmake --install build-release
```

CMake installs the binary, Pico stylesheet and license, example configuration, and project license. The stylesheet is installed under `/usr/local/share/stories/assets` with the prefix above. Configure `[assets] root` to that directory or copy the stylesheet into your chosen asset root. Installation does not overwrite `/etc/stories.conf`, create deployment roots, or start services. The server serves only the explicitly configured asset root.

Run as an unprivileged account with read access to content and public assets. See [examples/stories.freebsd](examples/stories.freebsd) for rc.d supervision and [examples/nginx.conf](examples/nginx.conf) for TLS and caching. Adjust paths, ownership, hostname, certificates, and cache directories. Proxy caching can delay content updates and permanent redirect changes.

Provider-neutral Linux sanitizer and fuzzing commands are in [tools/ci.sh](tools/ci.sh).

## GitHub Actions and releases

[CI](.github/workflows/ci.yml) runs on pull requests, pushes to `main`, and merge queue updates. It runs subsystem and HTTP tests on Linux, both macOS architectures, and FreeBSD in a native VM, plus Clang static analysis, ASan, UBSan, and all three libFuzzer smoke tests on Linux. Clang's analyzer checks run through clang-tidy and CMake, print findings and diagnostic paths directly in the CI log, and fail the job on findings. Dependencies are built before analysis. The platform jobs also stage an installation, create an archive, and rerun the HTTP suite against the extracted binary. Analyzer logs, test logs, fuzz findings, and binary archives are retained as workflow artifacts for seven days. Actions are pinned to commit IDs.

[Release](.github/workflows/release.yml) runs from **Actions → Release → Run workflow** on `main`. Select **patch**, **minor**, or **major** in the **Release type** dropdown. Python Semantic Release reads the Git release history and calculates the version number for that release type. The workflow supplies that version to CMake, builds and tests every platform, then creates the tag and publishes the release with its archives and SHA-256 checksums. No version edits, bump commits, or manual tagging are required. Release runs are serialized so concurrent requests cannot allocate the same version.

Every platform builds the same resolved commit; only the final publication job has release write permission. An existing release is never overwritten; if publication fails after creating a draft, inspect that draft before rerunning. `VERSION` contains `0.0.0` for ordinary development builds and is stamped automatically during release builds.

| Archive target | Build platform and runtime baseline |
| --- | --- |
| `linux-x86_64` | Ubuntu 22.04, glibc 2.35 or newer; not a musl binary |
| `macos-arm64` | macOS 15, Apple Silicon |
| `macos-x86_64` | macOS 15, Intel |
| `freebsd14-amd64` | FreeBSD 14.4, amd64; built and tested inside FreeBSD |

Each `stories-VERSION-TARGET.tar.gz` contains `bin/stories`, the README, and the normal `share/` installation tree with assets, examples, and licenses, including the linked dependencies' licenses and source checksums. CivetWeb and MD4C are statically linked; only platform system libraries are required at runtime. TLS remains the reverse proxy's responsibility. The macOS binaries are not Developer ID signed or notarized.

Extract the archive, copy `share/stories/examples/stories.conf.example` to your chosen configuration path, and set the content and asset roots. For an extracted archive, point the asset root at its `share/stories/assets` directory. Paths still resolve relative to the configuration file, and the binary does not locate assets automatically. No configuration, content roots, or services are created by extraction.

## Tests and fuzzing

`ctest --test-dir build` runs C subsystem tests and a Python standard-library HTTP suite against the built binary. Tests use a small checked-in corpus, isolated temporary roots, ephemeral ports, readiness polling, and bounded cleanup. They do not require or modify the ignored EPIC corpus. Discovery tests use the production search routine with isolated candidate files rather than changing `/etc`.

The HTTP suite checks redirects, indexes, GET/HEAD wire parity, static delegation and ranges, script disabling, root isolation, live updates, source/output limits, CLI behavior, failed startup, and shutdown under concurrent traffic.

With `clang-tidy` installed, run the same Clang static-analyzer checks locally:

```sh
cmake -B build-analysis -G Ninja -DSTORIES_WARNINGS_AS_ERRORS=ON -DCMAKE_C_CLANG_TIDY=clang-tidy
cmake --build build-analysis --clean-first --parallel 2
```

Only the `clang-analyzer-*` checks are enabled. The optional Annex K API-advice checker is excluded to preserve POSIX portability. CMake supplies the compiler flags and include paths, and the clean rebuild ensures cached objects do not skip analysis. Diagnostics are native compiler-style text; CI also saves them in `build-analysis/analysis.log`.

```sh
cmake -B build-sanitize -DSTORIES_SANITIZERS=ON -DSTORIES_WARNINGS_AS_ERRORS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Three opt-in libFuzzer targets exercise the production resolver, link rewriter, and config parser. They consume arbitrary byte spans with deterministic fixtures and ASan/UBSan instrumentation in the tested application code. The resolver consumes CivetWeb-decoded paths; link rewriting separately exercises percent-encoded destinations.

```sh
cmake -B build-fuzz -DSTORIES_FUZZING=ON
cmake --build build-fuzz
ctest --test-dir build-fuzz --label-regex fuzz --output-on-failure
```

Apple Clang may lack libFuzzer. On macOS install LLVM with `brew install llvm` and add `-DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang"` when configuring the fuzz build. CMake checks runtime support and fails clearly if explicitly requested fuzzing is unavailable. Normal builds do not require libFuzzer.

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
