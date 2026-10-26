"""Check readable text, controls, and selections in the bundled CSS palettes."""

import pathlib
import re
import unittest
import urllib.parse
import xml.etree.ElementTree as ET


ROOT = pathlib.Path(__file__).resolve().parent.parent
PICO = (ROOT / 'assets/pico.min.css').read_text()


def declarations(css):
    return dict(re.findall(r'^\s*(--[\w-]+):\s*(.+);\s*$', css, re.MULTILINE))


def palettes(path):
    css = path.read_text()
    defaults = declarations(css.split('color-scheme:', 1)[0])
    for index, match in enumerate(re.finditer(r'color-scheme:\s*(light|dark);([^}]+)', css)):
        yield f'{match[1]}-{index}', {**defaults, **declarations(match[2])}


def resolve(values, value, seen=()):
    variable = re.fullmatch(r'var\((--[\w-]+)\)', value)
    if not variable:
        return value
    name = variable[1]
    if name in seen:
        raise ValueError('cyclic CSS variable: ' + name)
    return resolve(values, values[name], (*seen, name))


def luminance(color):
    if not re.fullmatch(r'#[0-9a-fA-F]{6}', color):
        raise ValueError('expected an opaque six-digit color: ' + color)
    channels = [int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    linear = [value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4 for value in channels]
    return sum(value * weight for value, weight in zip(linear, [0.2126, 0.7152, 0.0722]))


def contrast(foreground, background):
    low, high = sorted([luminance(foreground), luminance(background)])
    return (high + 0.05) / (low + 0.05)


class ThemePalettes(unittest.TestCase):
    def test_text_and_control_contrast(self):
        pairs = [
            ('color', 'background-color'),
            ('muted-color', 'background-color'),
            ('primary', 'background-color'),
            ('primary-hover', 'background-color'),
            ('secondary', 'background-color'),
            ('secondary-hover', 'background-color'),
            ('primary-inverse', 'primary-background'),
            ('primary-inverse', 'primary-hover-background'),
            ('secondary-inverse', 'secondary-background'),
            ('secondary-inverse', 'secondary-hover-background'),
            ('contrast-inverse', 'contrast-background'),
            ('contrast-inverse', 'contrast-hover-background'),
            ('code-color', 'code-background-color'),
            ('form-element-color', 'form-element-background-color'),
            ('form-element-placeholder-color', 'form-element-background-color'),
            ('ins-color', 'background-color'),
            ('del-color', 'background-color'),
            ('mark-color', 'mark-background-color'),
        ]
        for path in sorted((ROOT / 'themes').glob('*.css')):
            for mode, values in palettes(path):
                for foreground, background in pairs:
                    with self.subTest(theme=path.name, mode=mode, foreground=foreground, background=background):
                        fg = resolve(values, values['--pico-' + foreground])
                        bg = resolve(values, values['--pico-' + background])
                        self.assertGreaterEqual(contrast(fg, bg), 4.5, f'{fg} on {bg}')

    def test_selection_is_readable_and_distinct_from_code(self):
        for path in sorted((ROOT / 'themes').glob('*.css')):
            selection = re.search(r'::selection\s*\{\s*color:\s*([^;]+);', path.read_text())
            for mode, values in palettes(path):
                with self.subTest(theme=path.name, mode=mode):
                    self.assertIsNotNone(selection, 'set a foreground for selected text and code')
                    foreground = resolve(values, selection[1])
                    background = resolve(values, values['--pico-text-selection-color'])
                    code_background = resolve(values, values['--pico-code-background-color'])
                    self.assertGreaterEqual(contrast(foreground, background), 4.5)
                    self.assertNotEqual(background, code_background)

    def test_checkbox_icons_contrast_with_normal_and_hover_fill(self):
        for path in sorted((ROOT / 'themes').glob('*.css')):
            for mode, values in palettes(path):
                for icon in ['--pico-icon-checkbox', '--pico-icon-minus']:
                    with self.subTest(theme=path.name, mode=mode, icon=icon):
                        fallback = re.search(re.escape(icon) + r':([^;]+);', PICO)[1]
                        image = values.get(icon, fallback)
                        svg = urllib.parse.unquote(re.fullmatch(r'url\("(.+)"\)', image)[1].split(',', 1)[1])
                        stroke = ET.fromstring(svg).attrib['stroke']
                        if stroke.startswith('rgb('):
                            stroke = '#%02x%02x%02x' % tuple(map(int, re.findall(r'\d+', stroke)))
                        for fill in ['--pico-primary-background', '--pico-primary-hover-background']:
                            self.assertGreaterEqual(contrast(stroke, resolve(values, values[fill])), 3)

    def test_system_and_explicit_dark_palettes_match(self):
        for path in sorted((ROOT / 'themes').glob('*.css')):
            with self.subTest(theme=path.name):
                modes = list(palettes(path))
                self.assertEqual(len(modes), 3)
                self.assertEqual(modes[1][1], modes[2][1])


if __name__ == '__main__':
    unittest.main()
