#!/usr/bin/env python3
"""Export opaque Pomodoro PNGs to RGB565 without alpha darkening; --check verifies."""
import argparse
import re
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PAIRS = {
    'pomodoro-scene': 'folotoy_pomodoro_scene',
    'pomodoro-scene-focus': 'folotoy_pomodoro_scene_focus',
    'pomodoro-scene-forest-rest': 'folotoy_pomodoro_scene_forest',
    'pomodoro-scene-forest-focus': 'folotoy_pomodoro_scene_forest_focus',
    'pomodoro-scene-night-rest': 'folotoy_pomodoro_scene_night',
    'pomodoro-scene-night-focus': 'folotoy_pomodoro_scene_night_focus',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for source, symbol in PAIRS.items():
        with Image.open(ROOT / 'assets/images/pixel-clock' / (source + '.png')) as image:
            if image.size != (230, 107):
                raise ValueError(f'{source}: expected 230x107')
            if image.convert('RGBA').getchannel('A').getextrema() != (255, 255):
                raise ValueError(f'{source}: opaque PNG required')
            pixels = image.convert('RGB').getdata()
            data = b''.join((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, 'little')
                            for r, g, b in pixels)
        path = ROOT / 'main/assets' / (symbol + '.c')
        text = path.read_text()
        pattern = r'(uint8_t\s+' + symbol + r'_map\[\]\s*=\s*\{)(.*?)(\};)'
        match = re.search(pattern, text, re.S)
        if match is None:
            raise ValueError(f'{symbol}: missing image array')
        actual = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', match[2]))
        for field, value in [('w', 230), ('h', 107), ('stride', 460)]:
            if not re.search(r'\.' + field + r'\s*=\s*' + str(value) + r'\s*,', text):
                raise ValueError(f'{symbol}: wrong {field}')
        if args.check:
            if actual != data:
                raise ValueError(f'{source}: regenerate C asset')
        else:
            body = '\n' + ''.join('    ' + ','.join(f'0x{v:02x}' for v in data[i:i+32]) + ',\n'
                                   for i in range(0, len(data), 32))
            path.write_text((text[:match.start(2)] + body + text[match.end(2):]).rstrip() + '\n')
        print(f'{source}: PASS (230x107, {len(data)} bytes)')


if __name__ == '__main__':
    main()
