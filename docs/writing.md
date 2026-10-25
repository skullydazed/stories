# Writing and publishing

The content root is a tree of trusted Markdown documents. It is independent of the static asset root. Stories renders the requested document on demand; there is no site-generation step.

## Organize documents

| Content file | Public URL |
| --- | --- |
| `index.md` | `/` |
| `install.md` | `/install` |
| `reference/index.md` | `/reference/` |
| `reference/configuration.md` | `/reference/configuration` |

An existing directory index gives `/reference` and `/reference/index` a 301 redirect to `/reference/`. `/index` redirects to `/`. A directory without `index.md` returns 404. Stories does not generate directory listings, indexes, search results, or navigation trees.

Document URLs must be extensionless. A dot in the final path component is rejected even if a backing file such as `install.html.md` exists. This also applies to directory index URLs. Requests ending in `.md` or `.MD` return 404, including beneath `/assets/`.

Author index pages with links to their child documents. Keep public names stable when editing or reorganizing content; external bookmarks use those URLs.

## Link pages

Write local Markdown links relative to the source document:

```markdown
[Installation](install.md)
[Reference](reference/index.md)
```

From `reference/configuration.md`, `../install.md` becomes `/install`, and `index.md` becomes `/reference/`. Rewriting retains query strings and fragments. A destination does not have to exist when rendered, so a valid link can still lead to a 404.

Local `.md` links, including percent-encoded `.md` suffixes, are normalized against the source path. A malformed Markdown destination, a root escape, or a name that cannot form an extensionless document URL fails the whole render with 500. No partial successful document is sent.

External URLs, fragment-only links, non-Markdown links, image destinations, and embedded HTML remain as authored. For example, extensionless `deals%` is passed through; a malformed escape in a `.md` destination is rejected. A link passing through does not make its destination a supported route.

## Markdown and page titles

Stories supports CommonMark plus tables, strikethrough, task lists, and automatic links. The first heading supplies the page title. Use a descriptive first heading and ordinary Markdown links for navigation.

Heading IDs are not generated. A link to a heading fragment needs an explicit authored anchor, for example:

```markdown
<a id="installation"></a>
## Installation

See [Installation](#installation).
```

Raw HTML passes through as trusted content, including scripts and other active content. The renderer is not an HTML sanitizer. Restrict publishing access to trusted maintainers.

## Publish assets

The layout requests `/assets/pico.min.css`. Copy the bundled stylesheet into the configured asset root, or use the installed `share/stories/assets` directory as that root. There is no stylesheet search outside the configured directory.

Asset URLs retain extensions. A file `images/logo.png` beneath the asset root is served as `/assets/images/logo.png`. Image destinations are not rewritten, so use an explicit asset URL:

```markdown
![Project logo](/assets/images/logo.png)
```

CivetWeb handles MIME types, HEAD, byte ranges, and conditional static requests. HTML and XHTML files return 404; implicit index files, directory listings, and script recognition are disabled. Static trees are public, and their symlinks follow CivetWeb's behavior. Keep secrets and symlinks to private trees outside them.

## Support legacy names

Configure `resolution_order` when migrating a corpus that previously used unqualified names. With `disambiguation, command, function`:

- A direct `time.md` takes precedence over the directory search.
- Otherwise `/time` redirects to the first matching `time.md` in those directories.
- `/time_command` redirects specifically to `/command/time` when that file exists.
- An authored `disambiguation/time.md` can explain the alternatives and link to them.

For EPIC-style content, singular directory names such as `command`, `function`, `on`, `ctcp`, `set`, `window`, `script`, and `special` can be listed in the desired order. Stories does not singularize names or generate disambiguation pages.

## Update a running site

Publish complete files with an atomic replacement inside the existing content root. Stories opens and reads documents on each request, so edits, additions, and deletions appear on the next uncached response. Replacing the root directory itself requires a restart because the server retains its descriptor.

Reverse-proxy and client caches may continue serving earlier content or redirects. Account for their freshness policy when publishing. Configuration changes, including navigation changes, require a server restart.

[Configuration](configuration.md) · [Deployment](deployment.md) · [Documentation home](index.md)
