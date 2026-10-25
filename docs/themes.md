# Themes and page layout

Stories reads the page's HTML layout from a file selected by `[site] template`. The supplied [page template](https://github.com/skullydazed/stories/blob/main/templates/page.html) uses Pico CSS, but the template controls the entire document: metadata, stylesheet links, header, navigation, content placement, and error messages. Editing it requires no C changes or rebuild.

```ini
[site]
title = My Documentation
template = templates/page.html
```

The path resolves relative to the configuration file, independently of the working directory. Keep templates outside the public asset and Markdown roots. There is no compiled-in layout or source-tree fallback. Existing configurations need a `template` setting when upgrading to this version.

Stories checks the template before starting its listener, then reads and parses it again for each document or error render. Edits appear on the next uncached request, without restarting or rebuilding the server. Template bytes and parsed state are request-owned and released after expansion; there is no application template cache. `--check-config` checks the template as well as configuration and roots. Missing files, nonregular files, empty templates, NUL bytes, and malformed Inja syntax fail validation. Startup also renders representative document/error paths with and without navigation; unknown names fail when evaluated, including in branches selected later by a request. Template files are limited to 1 MiB. Publish complete templates by atomic replacement to avoid readers seeing a partial edit.

## Template values

Templates use [Inja](https://github.com/pantor/inja), with Jinja-style loops, conditionals, comparisons, logical expressions, local variables, and comments. Ordinary `{{name}}` string substitutions are HTML-escaped, including both quote characters. Use these substitutions in HTML text or quoted HTML attributes, rather than JavaScript, CSS, or URL-building expressions.

| Value or expression | Meaning |
| --- | --- |
| `{{title}}` | The first Markdown heading's plain text, with a filename fallback; the HTTP error title on error pages. |
| `{{site_title}}` | The configured site title. |
| `{{content()}}` | Write rendered Markdown HTML without escaping. Empty on error pages. This function writes directly to the response rather than returning a string. |
| `{% for link in navigation %}…{% endfor %}` | Repeat the enclosed markup for each configured navigation entry, in order. |
| `{{link.label}}`, `{{link.url}}` | The current navigation entry's label and destination. |
| `{% if navigation %}…{% else %}…{% endif %}` | Select markup based on whether navigation is nonempty. |
| `{% if error %}…{% else %}…{% endif %}` | Select error or document markup. `error` is a boolean. |
| `{{message}}` | The error explanation; empty on document pages. |

For example:

```html
<!doctype html>
<html lang="en">
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>{{title}} — {{site_title}}</title>
	<link rel="stylesheet" href="/assets/pico.min.css">
	<link rel="stylesheet" href="/assets/site.css">
</head>
<body>
	<header class="container">
		<a href="/">{{site_title}}</a>
		<nav>
			{% for link in navigation %}
			<a href="{{link.url}}">{{link.label}}</a>
			{% endfor %}
		</nav>
	</header>
	<main class="container">
		{% if error %}
		<h1>{{title}}</h1>
		<p>{{message}}</p>
		{% else %}
{{content()}}
		{% endif %}
	</main>
</body>
</html>
```

The template supplies navigation and error markup as well as the page shell. No application-generated header or footer is added around it. Expressions such as `{% if length(navigation) > 1 and not error %}` work directly in the template. Inja also supports `{% set count = length(navigation) %}`, `loop.index1`, and `{# comments #}`. These local assignments do not change Stories' configuration. Filesystem includes are not loaded automatically.

Use `{{content()}}` only where the rendered body should be written. Calling it in a condition or assignment still writes the body; it does not provide a string for filtering or comparison. Substituted values and rendered Markdown are not reparsed as templates, so literal Inja syntax in a document stays literal. Previous Mustache layouts need conversion to this syntax; triple braces and Mustache section tags are not supported.

Template authors are trusted, just like Markdown authors. Inja allocates syntax trees, JSON metadata, and expression results internally; the response budget does not cap all of those allocations. Avoid unnecessarily large temporary values such as `range()` arrays. Its arithmetic does not guard every error: avoid signed integer overflow and remainder by zero. These upstream limitations are accepted for trusted templates; Stories carries no dependency patches. See [Design contracts](design.md) for the memory lifecycle.

The configured `output_limit` applies to the complete expanded document, including repeated content and navigation. An expansion failure discards the incomplete document and returns an independent error response. Error pages use the same template with a separate 256 KiB budget; if that also fails, Stories sends a short fallback message.

If a template becomes unreadable or invalid while the server is running, affected document requests return 500; Stories does not serve a retained older layout. Correcting the template restores rendering on the next uncached request. Static assets continue to use CivetWeb's handler.

## Change the Pico theme

The bundled stylesheet is Pico CSS 2.1.1. It is an ordinary file under the configured asset root, served at `/assets/pico.min.css` by the supplied template.

To change its color palette, select a stylesheet with [Pico's theme picker](https://picocss.com/docs/version-picker) and put it in the asset root. Replace `pico.min.css`, or give it another name and update the template's stylesheet link. Preserve the accompanying license when distributing it.

For custom colors, typography, or spacing, create `site.css` in the asset root and load it after Pico, as shown above. Pico's [CSS variables](https://picocss.com/docs/css-variables) provide the customization points. For example:

```css
:root {
	--pico-font-family: system-ui, sans-serif;
	--pico-border-radius: 0.25rem;
}
```

Pico follows the visitor's system preference for light or dark mode. To force a mode, change the template's opening element to `<html lang="en" data-theme="light">` or `<html lang="en" data-theme="dark">`. See [Pico's color schemes](https://picocss.com/docs/color-schemes).

CSS and template edits appear on the next uncached request. Changing configuration values, including the selected template path, requires a restart. A reverse proxy or browser cache can delay visible changes; verify against the backend when diagnosing stale styling.

[Configuration](configuration.md) · [Deployment](deployment.md) · [Documentation home](index.md)
