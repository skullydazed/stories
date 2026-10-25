# Mustach

`mustach.c`, `mustach.h`, and `LICENSE.txt` come from [Mustach](https://gitlab.com/jobol/mustach), release 1.2.10, commit `6bbfd33244ad08cd5debec8af1786d9e8e29dd2e`. Mustach is licensed under the BSD Zero Clause License (0BSD). The header and license are unmodified.

The core has a local section-replay fix: section entries save their opening and closing delimiters, including their lengths, and their standalone-line classification. Replaying the body restores that state and clears deferred whitespace from before the opening tag, which belongs outside the repeated body. Delimiter changes in the final iteration remain in effect after the section. Snapshots use the existing bounded section stack and require no heap allocations. The HTTP regression covers multiple navigation entries, nested sections, different delimiter lengths, mixed inline/standalone tags, indentation, CRLF line endings, and GET/HEAD parity.

Only the core is compiled. Stories supplies callbacks for its page values, navigation, HTML escaping, and bounded output; no JSON library or command-line tool is needed. Keep upstream files out of project formatting changes.
