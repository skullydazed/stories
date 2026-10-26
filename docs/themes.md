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
	<link rel="stylesheet" href="/assets/theme.css">
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

## Custom CSS and bundled themes

The bundled stylesheet is Pico CSS 2.1.1. It is an ordinary file under the configured asset root, served at `/assets/pico.min.css` by the supplied template.

To change its color palette, select a stylesheet with [Pico's theme picker](https://picocss.com/docs/version-picker) and put it in the asset root. Replace `pico.min.css`, or give it another name and update the template's stylesheet link. Preserve the accompanying license when distributing it.

The supplied template loads styles in this order: `/assets/pico.min.css`, `/assets/dark_toggle.css`, `/assets/theme.css`, then `/assets/site.css`. `dark_toggle.css` styles the switch, `theme.css` selects the color theme, and `site.css` holds independent site customization. Both `theme.css` and `site.css` contain only a comment by default, preserving Pico's colors. Put site-specific colors, typography, or spacing in `site.css`; changing the theme does not replace those customizations. Pico's [CSS variables](https://picocss.com/docs/css-variables) provide the customization points. For example:

```css
:root {
	--pico-font-family: system-ui, sans-serif;
	--pico-border-radius: 0.25rem;
}
```

The supplied template also loads `/assets/dark_toggle.js` and provides a switch with ☀️ and 🌑 icons, styled by `/assets/dark_toggle.css`. It starts in System mode, following the visitor's operating system preference, including later changes. The first click selects the opposite of the current system preference; subsequent clicks toggle only between Light and Dark. These explicit modes set `data-theme` on the page's `<html>` element and override that preference. The choice is remembered in browser local storage when available; switching still works for the current page if storage is blocked. Without JavaScript, the control stays hidden and CSS follows the system preference. See [Pico's color schemes](https://picocss.com/docs/color-schemes).

Stories bundles six self-contained CSS themes. Each includes both variants and works with the same control:

| File | Light variant | Dark variant |
| --- | --- | --- |
| `solarized.css` | Solarized Light | Solarized Dark |
| `dracula.css` | Alucard, Dracula's official light palette | Dracula |
| `tomorrow.css` | Tomorrow | Tomorrow Night |
| `catppuccin.css` | Catppuccin Latte | Catppuccin Mocha |
| `80s-terminal.css` | White on blue, like WordPerfect | Green on black, like a TTY |
| `epic.css` | EPIC site's off-white backdrop, white page, and blue links | Navy with logo-inspired orange and lavender accents |

The community themes use their upstream palettes; the terminal theme is an original design with monospace text. Its Light variant intentionally has a blue background. The EPIC theme follows the [existing site's](https://epicsol.org/) Arial typography and paper layout in Light mode, with an original Dark palette using orange `#f38d2d`, lavender `#8a90ba`, and navy shades sampled from its [anniversary logo](https://epicsol.org/lib/tpl/dokuwiki/images/logo.png). It does not load remote images or stylesheets. Palette sources and licenses are included in the [bundled theme directory](https://github.com/skullydazed/stories/tree/main/themes).

The bundled themes map text and controls to readable colors from those palettes. Some text roles differ from editor defaults: Solarized uses stronger base tones, and Catppuccin Latte uses mauve primary controls. Low-contrast insertion/deletion accents use normal text color with the existing underline/strikethrough. Selected text and checkbox icons have explicit contrasting foregrounds. Typography and corner defaults in the terminal and EPIC themes remain overridable using the `:root` example above.

To activate a theme in the source checkout with the example asset root:

```sh
cp themes/solarized.css assets/theme.css
```

For an installed release, copy the chosen file from `share/stories/themes/` into your configured asset root as `theme.css`. Retain `pico.min.css`, `dark_toggle.css`, `site.css`, and `dark_toggle.js` in the asset root. No configuration edit or restart is needed. Replacing `theme.css` with an empty file or a CSS comment restores Pico's default palette while retaining `site.css` customizations. A custom page template must include these stylesheet links and the switcher markup/script to provide the same behavior.

CSS and template edits appear on the next uncached request. Changing configuration values, including the selected template path, requires a restart. A reverse proxy or browser cache can delay visible changes; verify against the backend when diagnosing stale styling.

[Configuration](configuration.md) · [Deployment](deployment.md) · [Documentation home](index.md)
