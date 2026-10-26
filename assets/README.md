# Static assets

The example configuration uses this directory as `[assets] root`. If you configure another asset root, copy `pico.min.css`, `dark_toggle.css`, `theme.css`, `site.css`, and `dark_toggle.js` there. The server serves only the configured root and does not search fallback locations.

The editable `templates/page.html`, selected by `[site] template`, loads `/assets/pico.min.css`, `/assets/dark_toggle.css`, `/assets/theme.css`, and `/assets/site.css`, in that order. `dark_toggle.css` styles the switch. Copy one of the self-contained [bundled themes](../themes/README.md) to `theme.css`; put independent site customization in `site.css`. Both files contain only a comment by default, preserving Pico's colors. `dark_toggle.js` powers the System/Light/Dark control and remembers the visitor's choice. See [Themes and page layout](../docs/themes.md).

`pico.min.css` is Pico CSS 2.1.1, fetched from https://github.com/picocss/pico/blob/v2.1.1/css/pico.min.css and distributed with `LICENSE.pico.md`.

SHA-256: `fbc9a63fc9fc9f72d12fd7fc9806e11fa9f77ae4f9cad146b27003a1119ba3db`.
