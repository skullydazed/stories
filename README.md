# Stories - A small and safe markdown rendering app.

Stories is a compact documentation web server written in C11. It renders markdown documents into HTML on demand, with dynamic redirects and disambiguation pages to support migrating from legacy document names.

Stories serves HTTP and is designed to operate behind a reverse proxy such as nginx, which handles TLS termination and response caching.

## Requirements

* CivetWeb
* MD4C
* Meson
* Pico CSS

## Compiling

TBD

## Installation

```bash
# TBD
```

## Development

Stories is written in C11 and defines `_POSIX_C_SOURCE 200809L`. Development prioritizes correctness, security, and maintainability over performance.

## Design

TBD

## License

MIT, see [LICENSE.md](LICENSE.md) for more details.
