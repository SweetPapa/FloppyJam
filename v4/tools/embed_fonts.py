#!/usr/bin/env python3
"""Embed the two OFL fonts so the desktop executable stays portable."""
import pathlib
import sys
root = pathlib.Path(__file__).resolve().parents[1]
with open(sys.argv[1], 'w') as out:
    out.write('/* Generated font data. See assets/fonts/OFL.txt. */\n')
    for key, filename in [('medium', 'Barlow-Medium.ttf'), ('bold', 'Barlow-SemiBold.ttf')]:
        data = (root / 'assets' / 'fonts' / filename).read_bytes()
        out.write(f'static const unsigned char font_{key}[] = {{\n')
        for i in range(0, len(data), 24):
            out.write(','.join(str(b) for b in data[i:i + 24]) + ',\n')
        out.write('};\n')
