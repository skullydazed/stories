# Stories

Stories is a compact, file-backed documentation server written in C11. It renders Markdown on demand, gives documents extensionless URLs, supports ordered lookup of legacy names, and delegates public static assets to CivetWeb. Navigation and index pages are ordinary authored content.

Stories serves HTTP behind a reverse proxy such as nginx for TLS and caching. FreeBSD is the primary deployment platform, macOS the development platform, and Linux the CI environment.

## Documentation

The [documentation home](docs/index.md) links the complete project manual:

- [Getting started](docs/getting-started.md)
- [Configuration reference](docs/configuration.md)
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

Install a C11 compiler, CivetWeb 1.16 or newer, MD4C 0.4.8 or newer, their development headers, CMake 3.20 or newer, Make or Ninja, and Python 3.8 or newer. pkgconf is optional. Normal builds use installed libraries and do not download sources.

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

Compiler selection prefers `clang`, then `gcc`, then `cc`, while respecting explicit compiler choices and toolchain files. See [Development](docs/development.md) for static analysis, sanitizers, fuzzing, and CI.

## Run your own site

Copy [examples/stories.conf.example](examples/stories.conf.example) to `stories.conf`, create a Markdown root with `index.md`, and set a separate public asset root containing `pico.min.css`. Paths resolve relative to the configuration file. The local `stories.conf` is ignored by Git.

```sh
./build/stories --check-config
./build/stories
```

Without `--config`, Stories checks `./stories.conf`, then `/etc/stories.conf`, and fails if neither exists. See [Configuration](docs/configuration.md) for defaults and options and [Deployment](docs/deployment.md) for installation, nginx, and FreeBSD services.

## Contributing and license

Follow [CODESTYLE.md](CODESTYLE.md) and the [design contracts](docs/design.md). Publishing content is restricted to trusted maintainers because raw HTML passes through.

Stories is MIT licensed; see [LICENSE.md](LICENSE.md). The bundled Pico CSS stylesheet includes its [MIT license](assets/LICENSE.pico.md).
