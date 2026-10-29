# Getting started

## Requirements

Install C11 and C++17 compilers, pthreads, CivetWeb 1.16 or newer with development headers and static-file support, and MD4C 0.4.8 or newer with development headers. Building and testing also require CMake 3.20 or newer, Make or Ninja, and Python 3.8 or newer. Python is not needed to run the compiled server.

pkgconf is optional. CMake uses its metadata when available and searches directly for headers and libraries otherwise. CivetWeb and MD4C use installed libraries. Git checkouts also require Git and network access for the initial configuration: CMake automatically initializes the unmodified Inja submodule at the revision recorded by Stories. Inja includes its nlohmann/json headers. A plain clone is sufficient, and subsequent configurations reuse the available revision. Complete source release archives include Inja and do not require Git; use the uploaded source archive instead of GitHub's automatic source downloads.

On macOS:

```sh
brew install cmake pkgconf civetweb md4c
```

Use the corresponding development packages on FreeBSD or Linux. When dependencies are installed under a custom prefix, CMake's `CMAKE_PREFIX_PATH` setting identifies that prefix.

## Build and test

Run these commands from the source checkout:

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

Compiler selection prefers `clang`, then `gcc`, then `cc`. C++ defaults to the selected C driver's companion when available, then searches for `clang++`, `g++`, and `c++` in that order. Explicit compiler choices and toolchain files take precedence. Tests are enabled by default.

## Preview the project documentation

The documentation example is ready to run from the checkout:

```sh
./build/stories --config examples/docs.conf.example --check-config
./build/stories --config examples/docs.conf.example
```

Open <http://127.0.0.1:8080/>. Stop the foreground process with Ctrl-C. The example uses `docs/` as its content root and `assets/` as its static root. Paths resolve relative to the process working directory, so run these commands from the checkout root.

## Create your own site

Copy `examples/stories.conf.example` to `stories.conf` in the checkout root. The local `stories.conf` is ignored by Git. Its example roots are `docroot/` for Markdown and `assets/` for public assets; the latter already contains the Pico stylesheet used by the page layout. The example's required `[site] template` points to `templates/page.html`, which controls the HTML layout and stylesheet links.

```sh
cp examples/stories.conf.example stories.conf
mkdir -p docroot
```

Create `docroot/index.md`:

```markdown
# My documentation

Welcome to the site.
```

Adjust the site title and navigation in `stories.conf`, then validate and start:

```sh
./build/stories --check-config
./build/stories
```

Without `--config`, Stories checks `./stories.conf` and then `/etc/stories.conf`. The first existing file is authoritative; an unreadable or invalid file is fatal. Missing configuration is also fatal.

## Add a page

Create `docroot/install.md` and link to it from `index.md`:

```markdown
[Installation](install.md)
```

Stories rewrites that link to `/install`. The Markdown file remains private; requests for `/install.md` return 404. A folder needs an authored `index.md` to have an index page.

Read [Writing and publishing](writing.md) for URL rules, [Themes and page layout](themes.md) to customize the HTML and Pico styling, and [Configuration](configuration.md) for the complete settings reference. For an installed service, continue with [Deployment](deployment.md).

[Documentation home](index.md)
