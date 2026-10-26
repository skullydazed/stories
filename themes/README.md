# Bundled themes

Copy one CSS file to `theme.css` in your configured `[assets] root`. The supplied page template already loads it after Pico CSS and the switch styles, before `site.css`. Keep independent site customization in `site.css`; replacing the theme does not overwrite it. Each file is self-contained and includes both variants, attribution, and its license.

From the source checkout, with the example asset root:

```sh
cp themes/solarized.css assets/theme.css
```

An installation or release archive includes these files under `share/stories/themes/`. Copy the selected file into your own asset root as `theme.css`. Keep `pico.min.css`, `dark_toggle.css`, `site.css`, and `theme.js` in that root too. An uncached reload applies the new theme without restarting Stories.

| File | Light | Dark | Palette source |
| --- | --- | --- | --- |
| `solarized.css` | Solarized Light | Solarized Dark | [Solarized](https://ethanschoonover.com/solarized/) |
| `dracula.css` | Alucard | Dracula | [Official Dracula palettes](https://github.com/dracula/dracula-theme#color-palette-oss) |
| `tomorrow.css` | Tomorrow | Tomorrow Night | [Tomorrow Theme](https://github.com/chriskempson/tomorrow-theme) |
| `catppuccin.css` | Latte | Mocha | [Catppuccin palette](https://catppuccin.com/palette/) |
| `80s-terminal.css` | White on blue, inspired by WordPerfect | Green on black, inspired by a TTY | Original Stories design |

The ☀️/🌑 switch starts in System mode, following the visitor's operating system preference. The first click selects the opposite of the current system preference. Subsequent clicks toggle only between Light and Dark, overriding the system preference. The selection is remembered in browser local storage when available. Without JavaScript, themes still follow the system preference. The terminal theme deliberately uses a blue background in Light mode.

To return to Pico's default colors, replace `theme.css` with an empty file or a CSS comment. Your `site.css` customizations remain in effect. To customize a bundled theme, edit the palette variables in the copied file; each mode has its own palette, and the dark palette appears in both the system-preference rule and the explicit Dark rule.
