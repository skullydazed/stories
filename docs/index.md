# Stories

Stories serves a directory of Markdown documents as a website. It renders pages on request, gives documents extensionless URLs, and delegates CSS, images, and other public assets to CivetWeb. Navigation and index pages are authored Markdown, so the published site stays under your control.

Stories is written in C11 using CivetWeb and MD4C. It runs in the foreground behind a reverse proxy that handles TLS and caching. FreeBSD is the primary deployment platform; macOS and Linux are also supported.

## Start here

- [Getting started](getting-started.md): build Stories, preview these docs, and create your own site.
- [Configuration](configuration.md): required roots, defaults, command-line options, and validation.
- [Writing and publishing](writing.md): organize documents, link pages, publish assets, and use legacy names.
- [Deployment](deployment.md): install the server, serve this documentation, configure nginx, and run on FreeBSD.
- [Troubleshooting](troubleshooting.md): diagnose startup failures, missing pages, render errors, and stale responses.

## Work on Stories

- [Development](development.md): run tests, static analysis, sanitizers, fuzzing, and releases.
- [Design contracts](design.md): subsystem ownership, callback conventions, filesystem access, and shutdown sequencing.
- [Source repository](https://github.com/skullydazed/stories): code, issues, and CI results.

## Preview this documentation

From a checkout with dependencies installed:

```sh
cmake -B build
cmake --build build
ctest --test-dir build
./build/stories --config examples/docs.conf.example
```

Open <http://127.0.0.1:8080/>. The supplied configuration points to `docs/` and the checkout's separate `assets/` directory. No generated site, external theme download, or JavaScript build is needed.

## Publishing model

Content must come from trusted maintainers: embedded HTML passes through, including active content. Markdown files are read on each request; edits appear on the next uncached response. Stories provides no authentication, search index, generated navigation, or in-memory page cache. Use authored pages for indexes and disambiguation, and keep private files outside published roots.

Stories is MIT licensed. The repository includes [the project license](https://github.com/skullydazed/stories/blob/main/LICENSE.md) and the accompanying license for its bundled Pico CSS stylesheet.
