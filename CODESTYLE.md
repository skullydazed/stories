# Code style

Stories uses procedural, subsystem-oriented C with explicit state, ownership, and lifecycle transitions. Correctness, security, and maintainability take priority over performance. These conventions reflect the project's EPIC6 influences, while the design should fit CivetWeb and MD4C's APIs and execution models.

The checked-in [.clang-format](.clang-format) defines mechanical formatting. [docs/design.md](docs/design.md) documents the current subsystem contracts and lifecycle. This file describes how to write and extend the code.

## Formatting

- Use Allman braces for function definitions, control-flow blocks, structures, and enums.
- Indent with tabs, displayed at eight columns. Use spaces for readable alignment within related declaration groups.
- Keep each statement on its own line. Single-statement guards may omit braces; use braces for blocks containing multiple statements.
- Use readable declaration alignment within related groups, without extending alignment across unrelated operations or scopes.
- There is no line-length limit. Do not wrap code, comments, or Markdown paragraphs to an arbitrary column count. Break lines where structure or meaning calls for it.
- Keep related operations together and separate distinct steps with blank lines.

```c
struct document_route
{
	enum route_status status;
	int               fd;
	struct buffer     location;
};

void document_route_free(struct document_route *route)
{
	if (route->fd >= 0)
		close(route->fd);
	buffer_free(&route->location);
	*route = (struct document_route){.fd = -1};
}
```

Format changed C and C++ files with `clang-format -i`. CI enforces the project's formatting with:

```sh
clang-format --dry-run --Werror src/*.c src/*.cpp src/*.h tests/test_subsystems.c fuzz/*.c
```

Keep third-party assets and generated files out of routine formatting changes.

The page template adapter uses C++17 for Inja. Keep that boundary small and procedural, use the same brace and indentation conventions, and catch all C++ exceptions before returning to C. Library-owned template and JSON objects remain local to an expansion; rendered Markdown is borrowed through the output callback rather than copied into JSON. Third-party source stays unmodified.

## Subsystems, names, and interfaces

Organize code around concrete responsibilities such as configuration, document resolution, rendering, and HTTP integration. Give functions and state names that identify their domain and operation: `document_resolve`, `configuration_free`, and `struct document_route`.

Keep implementation helpers and immutable lookup tables `static`. Expose only the types and operations other subsystems need. Keep structure definitions private when callers can work through a small interface; expose fields when callers actually need to construct or inspect that state. Use explicit `struct` and `enum` tags for application types, while retaining library-defined types at library boundaries.

Use small shared utilities to enforce consistent policies for allocation, escaping, URL handling, and diagnostics. Keep dependencies visible through arguments and includes. Include a source file's own header first, followed by its project dependencies and system/library headers. Headers must include the declarations their interfaces require and use `STORIES_` include guards.

Use tables for registries and simple mappings, and callbacks for library integration and event dispatch. Use direct conditionals and switches for ordinary control flow. Introduce an abstraction when it makes a concrete responsibility or policy easier to maintain.

## State, ownership, and lifecycle

Make ownership and lifetime clear at every subsystem boundary. Document whether inputs are borrowed, whether outputs own allocations or descriptors, what remains valid after failure, and which function releases each resource. A callback's `userdata` should lead directly to concrete, typed state.

Keep configuration immutable while serving. Keep request and renderer state local to each request. Immutable tables may have static storage; mutable application state belongs in explicit state structures passed to the operations that use it.

Initialize resource owners to safe states before acquiring resources. Use explicit sentinels such as `-1` for unopened descriptors. Cleanup must handle partial initialization and restore a predictable state. A local `goto` cleanup path is appropriate when it makes acquisition and release sequencing easier to trace.

Keep startup, request processing, and shutdown transitions explicit. Stop CivetWeb and wait for workers before releasing anything callbacks can reference. Handle termination through the established main-thread signal path; keep library operations and resource cleanup out of asynchronous signal handlers.

## Input, errors, and defensive checks

Treat external and parser-supplied data as byte spans until the relevant contract establishes that it is a usable C string. Keep lengths explicit. Validate the complete returned length after decoding, including embedded NULs, before calling `strlen` or performing C-string routing. Decode at the designated boundary once, with the correct URL or form semantics.

Use `size_t` for byte counts and allocation sizes, and preserve signed system-call results until errors have been checked. Check arithmetic before adding or multiplying sizes. Check bounds before copying, and check allocation and I/O results. Preserve `errno` when cleanup would otherwise hide the error being reported.

Distinguish expected outcomes such as a missing document from malformed input, resource exhaustion, and internal failures. Use typed outcomes when callers need to distinguish those cases. Return failure through the subsystem interface and let the responsible boundary choose the diagnostic or HTTP response.

Use the shared bounded buffers and escaping helpers. Propagate failures and discard incomplete output. A failed render must not become a partial successful HTTP response. Keep filesystem lookup and use tied to the same validated descriptor where that is part of the subsystem's safety contract.

Explain why defensive checks exist, especially when their removal would reopen an overflow, lifetime, traversal, or sequencing problem. Comments should document contracts, invariants, ownership, callback conventions, and non-obvious decisions. Update comments when behavior changes.

## CivetWeb and MD4C

Use the libraries' supported public interfaces and keep adaptation code at the integration boundary. Respect their resource ownership and callback contracts. Copy or retain library data only according to its documented lifetime; never assume parser callback strings are NUL-terminated.

Document callback return conventions where they are easy to confuse. In CivetWeb, `begin_request` returns zero to delegate, while `http_error` returns zero when the error has been handled. MD4C rendering callbacks return zero on success and negative values on failure: supported older versions can silently drop positive error returns. Preserve renderer failures in the output buffer's sticky flag as well, including failures from temporary buffers and link rewriting. Keep failure propagation visible through those boundaries.

Let CivetWeb handle connections, worker threads, and static-file response mechanics. Let MD4C parse Markdown and expose rendering events. Keep Stories' URL, filesystem, rendering, and configuration policies explicit around those interfaces.

## Portability and verification

Write C11 with `_POSIX_C_SOURCE=200809L`. Isolate additional platform feature definitions in the build configuration. FreeBSD is the primary deployment platform, macOS the development platform, and Linux the CI environment. Check required library interfaces in CMake and give clear errors for explicitly requested unsupported features.

Use C tests for subsystem contracts and Python's standard library for HTTP and process integration tests. Test observable behavior, ownership transitions, and failure cases through production entrypoints. Use isolated temporary resources and bounded readiness/shutdown waits. Check HEAD bodies on the wire rather than relying solely on a client library that suppresses them.

Keep the resolver, link rewriter, and config parser usable by deterministic libFuzzer harnesses. Exercise arbitrary byte inputs and semantic invariants alongside ASan and UBSan. Add regression coverage at the boundary where a bug occurs: HTTP decoding bugs need HTTP tests even when the decoded-path resolver already has a fuzz harness. Keep working corpora and artifacts in the build tree, and add minimized findings to regression tests and seeds.

Run the checks appropriate to the change. See [docs/development.md](docs/development.md) for build, integration, sanitizer, and fuzzing commands.
