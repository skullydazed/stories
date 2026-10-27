# Configuration reference

Stories reads an INI configuration at startup. Use `--config PATH` to select a file explicitly, or let it check `./stories.conf` followed by `/etc/stories.conf`. An explicit path never falls back to discovery, and an existing but invalid discovery candidate prevents trying the next file.

Filesystem paths in the INI configuration resolve relative to the selected configuration file's directory. Relative log paths supplied on the command line resolve against the process working directory. Moving a configuration file can therefore change which roots its relative paths select. Configuration changes require a restart.

## A minimal configuration

```ini
[content]
root = /srv/stories/content

[assets]
root = /srv/stories/assets

[site]
template = /srv/stories/templates/page.html
```

Both directories must exist and be separate: neither may contain the other. Put an `index.md` in the content root and `pico.min.css`, `dark_toggle.css`, `theme.css`, `site.css`, and `dark_toggle.js` in the asset root. Copy the supplied `templates/page.html` to the selected template path, outside the public roots. The template is required; Stories does not use a compiled-in layout. The stylesheet is an ordinary public asset; Stories does not locate it in source or installation fallback directories.

## Settings

| Section and setting | Default | Meaning |
| --- | --- | --- |
| `[server] listen` | `127.0.0.1:8080` | One numeric IPv4 or bracketed IPv6 endpoint. Port zero selects an ephemeral port. |
| `[logging] access_log` | stdout | Append access records to this file. Parent directories must exist. |
| `[logging] error_log` | stderr | Append timestamped error diagnostics to this file. Parent directories must exist. |
| `[server] threads` | `8` | CivetWeb worker count, from 1 through 1024. |
| `[content] root` | Required | Existing Markdown root. |
| `[content] resolution_order` | Empty | Comma-separated top-level directories for legacy document lookup. |
| `[content] markdown_limit` | `8388608` | Maximum Markdown bytes per document: 8 MiB. |
| `[content] output_limit` | `33554432` | Maximum complete HTML page size: 32 MiB. |
| `[assets] root` | Required | Existing public static root, separate from content. |
| `[site] title` | `Stories` | Site name in the layout and after the page title. |
| `[site] template` | Required | HTML page template, relative to the configuration file or absolute. Checked at startup and read on each render. |
| `[navigation] Label = URL` | No entries | Navigation links, rendered in configuration order. |

Each document size limit must be positive and at most 1 GiB. The defaults are provisional caps, not measurements of a particular corpus or a guarantee that HTML always expands by less than four times. These limits apply per request; they do not cap the process's total memory use. Rendering that exceeds a limit returns a complete 500 response, without a partial successful page.

Listener examples include `127.0.0.1:8080` and `[::1]:8080`. Hostnames, multiple listeners, and TLS suffixes are unsupported. Use a reverse proxy for TLS. The selected port is reported on stdout when the server starts.

<a id="logging"></a>

## Logging

Without logging settings, access records go to stdout and error diagnostics go to stderr. Access records use CivetWeb's text format, including the client address, request, HTTP status, and bytes sent. CivetWeb supplies the decoded request URI; this is not a copy of the original encoded request target. Errors use UTC timestamps. Control bytes in records are escaped to keep each record on one line. The listener announcement remains on stdout even when access records go to a file.

```ini
[logging]
access_log = /var/log/stories/access.log
error_log = /var/log/stories/error.log
```

Each destination is independent; omit a setting to retain its standard stream. File destinations are literal paths, opened in append mode and created if necessary with mode `0644` (owner read/write, group and others read), further restricted by the process umask. Existing file permissions are preserved. Parent directories must already exist and be writable by the service account. An open failure stops startup and reports the error on stderr. Diagnostics before configuration and logging initialization also use stderr. Runtime write failures are reported to stderr without stopping request serving.

After external rotation, send SIGUSR1 to the Stories process to reopen both configured log paths. Stories keeps serving during reopening. If either open fails, both existing destinations remain active and the failure is reported to the existing error log. SIGUSR1 does not reload configuration. Restarting also opens the configured paths; Stories has no internal rotation or retention policy.

`--access-log PATH` and `--error-log PATH` override the corresponding INI settings. Relative CLI paths start at the process working directory. `--check-config` validates setting syntax, roots, and the template without opening or creating log files; it does not verify log-path writability.

## Site title and navigation

```ini
[site]
title = Project Documentation
template = templates/page.html

[navigation]
Home = /
Installation = /install
Reference = /reference/
```

Navigation destinations must be local paths beginning with one `/`, or HTTP(S) URLs. They are configuration values rather than Markdown links, so use extensionless document URLs directly. Duplicate navigation labels are rejected. Stories does not check that a linked document exists at startup.

Each page's title comes from its first Markdown heading at any level. Inline formatting is reduced to text. If that heading is empty or missing, Stories uses the extensionless document basename.

The HTML template controls the layout, stylesheet links, and navigation markup for both documents and errors. See [Themes and page layout](themes.md) for substitutions, loops, Pico customization, and light/dark mode. Template edits appear on the next uncached request, without restarting or rebuilding Stories. Changing the configured template path requires restarting, like other configuration changes.

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

Diagnostics identify the file and, when applicable, the line, section, and setting. Validate roots, configuration, and the template without starting a listener:

```sh
stories --config /etc/stories.conf --check-config
```

## Command-line options

| Option | Purpose |
| --- | --- |
| `-c PATH`, `--config PATH` | Select the configuration file. |
| `--listen ADDRESS:PORT` | Override the configured endpoint. |
| `--threads COUNT` | Override the configured worker count. |
| `--access-log PATH` | Override the access-log file; relative to the working directory. |
| `--error-log PATH` | Override the error-log file; relative to the working directory. |
| `--check-config` | Validate configuration, roots, and the page template, then exit. |
| `-h`, `--help` | Show help without requiring configuration. |
| `--version` | Show the compiled version without requiring configuration. |

[Getting started](getting-started.md) · [Troubleshooting](troubleshooting.md) · [Documentation home](index.md)
