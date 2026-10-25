# Design contracts

## Subsystem boundaries

`configuration` owns parsed strings, directory order, and navigation entries. Parsing accepts a byte span and base directory, performs no filesystem access, and returns either a fully usable configuration or an empty object. Loading implements file discovery and bounded reading around that parser. The executable supplies fixed defaults; the same search routine accepts isolated paths in tests.

`document` owns descriptor-relative lookup and reading. Resolution receives an already decoded path, immutable configuration, and a borrowed content-root descriptor. Every result is initialized, owns any returned descriptor and location buffer, and must be released even after failure. Outcomes distinguish a document, redirect, invalid request, missing document, and internal failure.

`url` owns path validation, canonical document URLs, and link rewriting. Rewriting is independent of destination existence. It resolves local destinations against the source filename, normalizes segments within the root, strips the source extension, maps indexes to directory URLs, percent-encodes the resulting path, and preserves queries/fragments. External and non-Markdown destinations remain unchanged. A filename ending in another `.md` after extension removal has no supported public URL.

`renderer` owns per-parse state and HTML emission. MD4C supplies borrowed, length-delimited strings, which are never assumed to be NUL-terminated or retained after callbacks. Parser-identified entities are decoded using a generated HTML5 lookup before escaping attributes or rewriting links. Rich image labels emit escaped text into `alt` attributes; ordinary raw HTML events remain verbatim elsewhere because publishers are trusted.

`server` adapts these subsystems to CivetWeb. A `begin_request` callback completes document responses or returns zero for the static prefixes. CivetWeb owns connections and worker threads. Static MIME, range, and successful HEAD behavior belongs to CivetWeb, with listings and script recognition disabled. An `http_error` callback sends established-request errors through the shared shell and response helper, correcting CivetWeb 1.16's default HEAD error body. That callback uses the opposite return convention: zero means the error was handled.

## Ownership and sequencing

The main thread loads configuration, applies scalar CLI overrides, and either checks it or enters `server_run`. Configuration remains immutable while serving and is freed after the server returns. Startup validates disjoint roots, opens the Markdown root, builds static mappings, blocks termination signals before worker creation, initializes CivetWeb file support, and starts the listener.

Workers inherit blocked SIGINT/SIGTERM. The main thread uses `sigwait`, so cleanup never runs in an asynchronous signal handler. On termination, `mg_stop` joins workers before application-owned descriptors or configuration are released. Partial startup failures unwind only acquired resources. SIGPIPE is ignored so a disconnected reader cannot terminate the process during a write.

Each request owns its route descriptor, source bytes, rendering state, and output. It reads from the descriptor opened during resolution, avoiding a second path lookup. Read failures, callback cancellation, allocation failures, and limit failures discard partial output and produce an independent error response. HEAD renders identically to GET to preserve status and content length but sends no body. Error pages have a separate bounded budget so an output-limit failure can still produce an error.

Buffers own allocations, check arithmetic before growing, and retain a sticky failure flag. Callers borrow data only until mutation or release. Appends do not support overlapping input and destination allocations.

## Filesystem and HTTP policy

Markdown paths reject NUL/control bytes, hidden components, backslashes, repeated separators, and dot segments. Each component is opened with `O_NOFOLLOW`; intermediate components must be directories and the final descriptor must identify a regular file. `O_NONBLOCK` prevents FIFOs from blocking before validation. Missing, inaccessible, or forbidden content is absent; resource and other I/O failures remain internal errors.

CivetWeb decodes incoming paths once. Stories validates `local_uri_raw`, which retains dot segments, rather than trusting the cleaned `local_uri` or applying another decoder. Resolver fuzzing therefore begins at the decoded-path boundary. CivetWeb's decoder remains an external library contract.

Only `/assets/` and `/_stories/` permit static fallback. Their roots contain public files and must not overlap content. Asset symlinks follow CivetWeb's behavior. Script recognition is disabled even when the installed library includes scripting support. Stories completes all other routes, including 404s.

Configuration is startup state; content is read on every request. Publish complete files by atomic replacement to avoid partially rewritten documents. Replacing files inside the content root is supported; replacing the root itself requires restart because the server retains its descriptor. Proxy and client caches may continue serving cached responses.

## Verification boundaries

C tests exercise production subsystem contracts. Python tests drive the built binary using temporary roots and ephemeral ports. Readiness and shutdown deadlines prevent indefinite waits. Raw socket checks verify HEAD bodies independently of client-library behavior.

Fuzz harnesses use the production resolver, link rewriter, and parser, checking semantic invariants alongside sanitizers. Resolver setup opens an immutable fixture root once. Inputs do not write files, start servers, or open network connections. A separately instrumented core keeps libFuzzer's main and coverage runtime out of the server binary. Checked-in seeds remain immutable; working corpora and artifacts live in the build tree.
