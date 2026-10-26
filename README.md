# Stories

Stories is a compact, file-backed documentation server written in C11 with a small C++17 adapter for Inja templates. It renders Markdown on demand, gives documents extensionless URLs, supports ordered lookup of legacy names, and delegates public static assets to CivetWeb. Navigation and index pages are ordinary authored content.

Stories serves HTTP behind a reverse proxy such as nginx for TLS and caching. FreeBSD is the primary deployment platform, macOS the development platform, and Linux the CI environment.

## Design requirements

Keep the memory footprint small. Allocations need a clear purpose, an explicit owner and lifetime, and appropriate size bounds. Avoid unnecessary retention and duplication; release temporary storage promptly. Bounded working buffers are acceptable when they serve a concrete purpose and keep the implementation straightforward. Consider peak memory use and explain the tradeoff before introducing caching, preloading, or additional full-document copies.

Markdown, HTML templates, and static assets are editable site files. Their changes must appear on the next uncached request without rebuilding or restarting Stories. Site presentation belongs in editable files, outside the executable. These are requirements for future changes as well as the current implementation; see [Design contracts](docs/design.md).

## Documentation

The [documentation home](docs/index.md) links the complete project manual:

- [Getting started](docs/getting-started.md)
- [Configuration reference](docs/configuration.md)
- [Themes and page layout](docs/themes.md)
- [Writing and publishing](docs/writing.md)
- [Deployment](docs/deployment.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Development and releases](docs/development.md)
- [Design contracts](docs/design.md)

The `docs/` tree is itself a Stories site. After building, preview it with:

```sh
./build/stories --config examples/docs.conf.example
```

Open <http://127.0.0.1:8080/>. The configuration selects `docs/` and the separate `assets/` directory. Installations and release archives include the same documentation site and configuration under `share/stories/`.

## Build

Install C11 and C++17 compilers, CivetWeb 1.16 or newer, MD4C 0.4.8 or newer, their development headers, CMake 3.20 or newer, Make or Ninja, and Python 3.8 or newer. pkgconf is optional. CivetWeb and MD4C use installed libraries. From a Git checkout, CMake automatically initializes the unmodified Inja submodule at the revision recorded by Stories; the initial configuration requires Git and network access. Inja includes its nlohmann/json headers. No recursive clone or separate submodule command is needed.

On macOS:

```sh
brew install cmake pkgconf civetweb md4c
```

From the source checkout:

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Compiler selection prefers `clang`, then `gcc`, then `cc`. C++ defaults to the selected C driver's companion when available, then searches for `clang++`, `g++`, and `c++` in that order. Explicit compiler choices and toolchain files take precedence. See [Development](docs/development.md) for static analysis, sanitizers, fuzzing, and CI.

## Run your own site

Copy [examples/stories.conf.example](examples/stories.conf.example) to `stories.conf`, create a Markdown root with `index.md`, and set a separate public asset root containing `pico.min.css`, `dark_toggle.css`, `theme.css`, `site.css`, and `dark_toggle.js`. The example selects the editable [templates/page.html](templates/page.html) for the HTML layout. Paths resolve relative to the configuration file. The local `stories.conf` is ignored by Git.

```sh
./build/stories --check-config
./build/stories
```

Without `--config`, Stories checks `./stories.conf`, then `/etc/stories.conf`, and fails if neither exists. See [Configuration](docs/configuration.md) for defaults and options and [Deployment](docs/deployment.md) for installation, nginx, and FreeBSD services.

## Contributing and license

Follow [CODESTYLE.md](CODESTYLE.md) and the [design contracts](docs/design.md). Publishing content is restricted to trusted maintainers because raw HTML passes through.

Stories is MIT licensed; see [LICENSE.md](LICENSE.md). The bundled Pico CSS stylesheet includes its [MIT license](assets/LICENSE.pico.md). The [optional bundled themes](themes/README.md) include light and dark variants with upstream palette attribution and licenses.

Inja and its bundled nlohmann/json dependency are MIT licensed; their licenses ship with installed binaries. Both remain unmodified upstream source. See [Inja](https://github.com/pantor/inja) and [nlohmann/json](https://github.com/nlohmann/json).
