# Development

Stories uses procedural, subsystem-oriented C11 with explicit ownership and lifecycle transitions. Read the [design contracts](design.md) before changing request handling or subsystem interfaces. The repository's [code style guide](https://github.com/skullydazed/stories/blob/main/CODESTYLE.md) defines Allman braces, tabs, declaration alignment, and contract documentation. There is no artificial line-length limit.

## Build and test

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

CMake prefers `clang`, then `gcc`, then `cc`. C++ defaults to the selected C driver's companion when available, then searches for `clang++`, `g++`, and `c++` in that order. Explicit compiler choices and toolchain files take precedence. CivetWeb and MD4C use installed development headers and libraries. See [Getting started](getting-started.md) for prerequisites.

Inja is an unmodified upstream Git submodule at `inja/`, including its upstream nlohmann/json headers. CMake initializes it automatically and checks out the revision recorded by Stories, so a plain `git clone` is sufficient. The initial configuration needs Git and network access; subsequent configurations reuse the available revision. The revision is tracked by Git's submodule entry; `git -C inja describe --tags --always` identifies it. Source release archives include the submodule's contents and can configure without Git. GitHub's automatic source downloads are not complete source distributions for this purpose.

The C subsystem suite exercises configuration, buffers, filesystem resolution, URL rewriting, and rendering. Python standard-library tests check packaging arguments, compiler selection, and HTTP/process behavior against the built binary. HTTP tests use temporary roots, ephemeral ports, readiness deadlines, and bounded cleanup. They cover redirects, HEAD bodies on the wire, static ranges, size limits, failed startup, and shutdown under traffic.

For failure output or a smaller set of checks:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build --label-regex unit --output-on-failure
```

## Static analysis

With `clang-tidy` installed:

```sh
cmake -B build-analysis -G Ninja -DSTORIES_WARNINGS_AS_ERRORS=ON -DCMAKE_C_CLANG_TIDY=clang-tidy -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-analysis --clean-first --parallel 2
```

The repository's `.clang-tidy` configuration enables Clang's analyzer checks and treats findings as errors. The optional Annex K API-advice checker is excluded for POSIX portability. CMake supplies the same flags and include paths as compilation. Diagnostics and path notes print directly in the build and CI logs. A clean rebuild ensures cached object files do not skip analysis.

## Sanitizers

```sh
cmake -B build-sanitize -DSTORIES_SANITIZERS=ON -DSTORIES_WARNINGS_AS_ERRORS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

This enables AddressSanitizer and UndefinedBehaviorSanitizer for application code. CMake checks that the selected compiler can provide both runtimes.

## Fuzzing

Three opt-in libFuzzer harnesses exercise the URL resolver, link rewriter, and config parser using arbitrary byte spans and semantic invariants. They use a separately instrumented core with ASan and UBSan; the production server does not link libFuzzer's main or coverage runtime.

```sh
cmake -B build-fuzz -DSTORIES_FUZZING=ON
cmake --build build-fuzz
ctest --test-dir build-fuzz --label-regex fuzz --output-on-failure
```

Apple Clang may lack libFuzzer. On macOS, install a full LLVM toolchain with `brew install llvm` and select it when configuring that build:

```sh
cmake -B build-fuzz -DSTORIES_FUZZING=ON -DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang"
```

Unless C++ is explicitly selected through `CMAKE_CXX_COMPILER`, `CXX`, or a toolchain file, CMake prefers the matching `clang++` beside the selected C compiler. The fuzzing configuration checks both drivers, including the C++ driver's libFuzzer link runtime. Explicit compiler choices remain authoritative; select a toolchain in a fresh build directory.

Smoke tests run for 30 seconds per target. Checked-in seeds remain immutable; temporary working corpora and crash artifacts live in the build tree. For a longer session:

```sh
mkdir -p build-fuzz/corpus-url build-fuzz/fuzz-artifacts/url_resolver
./build-fuzz/fuzz_url_resolver build-fuzz/corpus-url fuzz/corpus/url_resolver -max_total_time=3600 -artifact_prefix=build-fuzz/fuzz-artifacts/url_resolver/
```

Pass a saved artifact as the sole positional input to reproduce it. Use libFuzzer's `-minimize_crash=1 -exact_artifact_path=...` options to minimize a finding, or `-merge=1 NEW_CORPUS OLD_CORPUS` to reduce a corpus. Add minimized findings to deterministic regression tests and seeds.

The provider-neutral `tools/ci.sh` runs the sanitizer suites and fuzz smoke tests. It expects the dependencies and a full Clang toolchain to be installed. CI builds the latest upstream release tags of CivetWeb and MD4C first, in an isolated prefix, and records the versions, commits, and source checksums in release archives.

## CI and releases

CI runs on pull requests, pushes to `main`, and merge queue updates. It tests Linux x86-64, macOS Intel and Apple Silicon, and FreeBSD amd64 in a VM. Separate Linux jobs run Clang static analysis and sanitizers/fuzzing. Platform jobs stage installations and test extracted release binaries. Test logs, fuzz findings, and binary archives are retained as workflow artifacts for seven days.

To create a release, select **Actions → Release → Run workflow** on `main`, then choose **patch**, **minor**, or **major**. Python Semantic Release calculates the version from Git release history. The workflow stamps the build version, tests all platforms, and creates the tag and GitHub Release after successful builds. It uploads complete source and binary archives with checksums. No version edits, bump commits, or manual tagging are needed. Release runs are serialized.

The `VERSION` file contains `0.0.0` for ordinary development builds. Release builds stamp it automatically. An existing release is not overwritten; inspect an incomplete draft if publication fails after the draft is created.

## Work on these docs

The documentation is an authored Markdown site. Preview it with:

```sh
./build/stories --config examples/docs.conf.example
```

Keep local page links inside `docs/` and use `.md` destinations so they work in the repository and are rewritten when served. Links to source files outside the content root should use their repository URLs. Those files are not local document routes. Heading fragments need explicit anchors because Stories does not synthesize heading IDs.

Before publishing, visit new pages through Stories, follow their links, and check the server log for rendering failures. See [Writing and publishing](writing.md) for the content rules.

[Design contracts](design.md) · [Getting started](getting-started.md) · [Documentation home](index.md)
