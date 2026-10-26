# Static assets

The example configuration uses this directory as `[assets] root`. Pico CSS is an ordinary static file served at `/assets/pico.min.css`. If you configure another asset root, copy the stylesheet there. The server serves only the configured root and does not search fallback locations.

The stylesheet link belongs to the editable `templates/page.html`, selected by `[site] template`. Change that file to select another stylesheet or load custom CSS after Pico. See [Themes and page layout](../docs/themes.md).

`pico.min.css` is Pico CSS 2.1.1, fetched from https://github.com/picocss/pico/blob/v2.1.1/css/pico.min.css and distributed with `LICENSE.pico.md`.

SHA-256: `fbc9a63fc9fc9f72d12fd7fc9806e11fa9f77ae4f9cad146b27003a1119ba3db`.
