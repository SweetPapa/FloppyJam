#!/usr/bin/env python3
"""Subset official Noto CJK Sans 2.004 sources to the committed game catalog.
Usage: subset_fonts.py SOURCE_DIRECTORY (Japanese.otf and Chinese.otf).
Fonts: https://github.com/notofonts/noto-cjk/tree/Sans2.004/Sans/OTF
Install fonttools to regenerate; normal builds use the committed subsets.
"""
import json,sys
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont
root=Path(__file__).resolve().parents[1]
d=json.loads((root/'i18n/catalog.json').read_text())
text=''.join(v for row in d['strings'].values() for v in row)+''.join(d['names'])+''.join(chr(c) for c in range(32,127))+'×←↑→↓'
for name in ['Japanese','Chinese']:
 font=TTFont(Path(sys.argv[1])/(name+'.otf'))
 options=subset.Options();options.layout_features=['*'];options.notdef_glyph=True;options.notdef_outline=True
 worker=subset.Subsetter(options=options);worker.populate(text=text);worker.subset(font)
 # Distinct family names for modified OFL font subsets.
 family='MagLava Sans '+name
 for record in font['name'].names:
  if record.nameID in (1,3,4,6):record.string=(family.replace(' ','') if record.nameID==6 else family).encode(record.getEncoding())
 if 'CFF ' in font:
  cff=font['CFF '].cff;cff.fontNames=[family.replace(' ','')]
  cff.topDictIndex[0].FamilyName=family;cff.topDictIndex[0].FullName=family
 out=root/'assets/fonts'/('MagLavaSans'+name+'.otf');font.save(out)
 print(out.name,out.stat().st_size)
