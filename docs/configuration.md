# Configuration reference

Stories reads an INI configuration at startup. Use `--config PATH` to select a file explicitly, or let it check `./stories.conf` followed by `/etc/stories.conf`. An explicit path never falls back to discovery, and an existing but invalid discovery candidate prevents trying the next file.

All filesystem paths resolve relative to the selected configuration file's directory. Moving a configuration file can therefore change which roots its relative paths select. Configuration changes require a restart.

## A minimal configuration

```ini
[content]
root = /srv/stories/content

[assets]
root = /srv/stories/assets
```

Both directories must exist and be separate: neither may contain the other. Put an `index.md` in the content root and `pico.min.css` in the asset root. The stylesheet is an ordinary public asset; Stories does not locate it in source or installation fallback directories.

## Settings

| Section and setting | Default | Meaning |
| --- | --- | --- |
| `[server] listen` | `127.0.0.1:8080` | One numeric IPv4 or bracketed IPv6 endpoint. Port zero selects an ephemeral port. |
| `[server] threads` | `8` | CivetWeb worker count, from 1 through 1024. |
| `[content] root` | Required | Existing Markdown root. |
| `[content] resolution_order` | Empty | Comma-separated top-level directories for legacy document lookup. |
| `[content] markdown_limit` | `8388608` | Maximum Markdown bytes per document: 8 MiB. |
| `[content] output_limit` | `33554432` | Maximum complete HTML page size: 32 MiB. |
| `[assets] root` | Required | Existing public static root, separate from content. |
| `[site] title` | `Stories` | Site name in the layout and after the page title. |
| `[navigation] Label = URL` | No entries | Navigation links, rendered in configuration order. |

Each document size limit must be positive and at most 1 GiB. The defaults are provisional caps, not measurements of a particular corpus or a guarantee that HTML always expands by less than four times. These limits apply per request; they do not cap the process's total memory use. Rendering that exceeds a limit returns a complete 500 response, without a partial successful page.

Listener examples include `127.0.0.1:8080` and `[::1]:8080`. Hostnames, multiple listeners, and TLS suffixes are unsupported. Use a reverse proxy for TLS. The selected port is reported on stdout when the server starts.

## Site title and navigation

```ini
[site]
title = Project Documentation

[navigation]
Home = /
Installation = /install
Reference = /reference/
```

Navigation destinations must be local paths beginning with one `/`, or HTTP(S) URLs. They are configuration values rather than Markdown links, so use extensionless document URLs directly. Duplicate navigation labels are rejected. Stories does not check that a linked document exists at startup.

Each page's title comes from its first Markdown heading at any level. Inline formatting is reduced to text. If that heading is empty or missing, Stories uses the extensionless document basename.

## Legacy lookup

```ini
[content]
root = /srv/stories/content
resolution_order = disambiguation, command, function
```

For `/time`, Stories first tries direct document and index resolution, then searches those directories in order. A match redirects to its canonical URL. `/time_command` selects `command/time.md` when `command` is a configured resolution directory. A recognized suffix is authoritative: a missing target returns 404.

Resolution directories are unique top-level names. Their names may contain letters, digits, underscores, and hyphens; they are limited to 128 bytes, and `assets` is reserved. They need not exist at startup. The longest configured suffix wins when names contain underscores. See [Writing and publishing](writing.md) for a complete routing example.

## Grammar and validation

Each setting occupies one `key = value` line inside a named section. Leading and trailing spaces and tabs are trimmed. Blank lines and whole-line `#` or `;` comments are ignored. Values are literal: there is no quoting, escaping, interpolation, or inline-comment syntax.

Unknown sections or settings, duplicate scalar settings, and disallowed control bytes are rejected. Tabs and the CR/LF bytes used by the grammar are accepted; NUL, DEL, and other ASCII control bytes are not. Files are limited to 1 MiB. There can be at most 128 navigation entries and 128 resolution directories.

Configured root paths cannot contain commas or equals signs because CivetWeb reserves them in rewrite mappings. Avoid those characters in configured root paths.

Diagnostics identify the file and, when applicable, the line, section, and setting. Validate roots and configuration without starting a listener:

```sh
stories --config /etc/stories.conf --check-config
```

## Command-line options

| Option | Purpose |
| --- | --- |
| `-c PATH`, `--config PATH` | Select the configuration file. |
| `--listen ADDRESS:PORT` | Override the configured endpoint. |
| `--threads COUNT` | Override the configured worker count. |
| `--check-config` | Validate configuration and roots, then exit. |
| `-h`, `--help` | Show help without requiring configuration. |
| `--version` | Show the compiled version without requiring configuration. |

[Getting started](getting-started.md) · [Troubleshooting](troubleshooting.md) · [Documentation home](index.md)
