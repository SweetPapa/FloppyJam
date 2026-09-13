#!/usr/bin/env python3
"""Package current marketing assets and localized copy, excluding old renders."""
import argparse,hashlib,json,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=root/'mobile/media/out'
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=int);a=p.parse_args()
rows=json.loads((root/'mobile/store/localizations.json').read_text(encoding='utf-8'))
copy=out/'press-copy';copy.mkdir(exist_ok=True)
for locale,r in rows.items():
 text=f"# MagLava — {locale}\n\n{r['subtitle']}\n\n{r['fullDescription']}\n\n## Social post\n\n{r['socialPost']}\n\n## Links\n\nhttps://maglava.io\nhttps://testflight.apple.com/join/syeM6ybj\nhttps://play.google.com/apps/testing/dev.fofo.maglava\n"
 (copy/f'{locale}.md').write_text(text,encoding='utf-8')
(copy/'README.md').write_text(f'''# MagLava press kit — build {a.build}

40 stages; seven languages; iOS/iPadOS, Android, macOS, Windows and Linux.
The current installation instructions and beta availability are at https://maglava.io.

All screenshots and footage are from actual native gameplay. The automated
capture player selects real color inputs; it does not bypass hazards. Screenshot
headlines are localized into all seven game languages. The captured game UI is
English. Videos use the original MagLava soundtrack, with no narration or added
spoken audio. The landscape and portrait gameplay edits are 36 seconds; the
App Store preview is 28 seconds. Playback is at the recorded speed.

Use these supplied assets to cover or promote MagLava. Please credit MagLava /
Sweet Papa Technologies. Contact: fterry@sweetpapatechnologies.com.

Mobile and macOS use native 3D graphics and the original four-song playlist.
Windows and Linux use the desktop presentation and procedural music.
Production store promotion is separate from beta access.
''',encoding='utf-8')
files=list(copy.glob('*.md'))+[out/name for name in ['MagLava-Trailer.mp4','MagLava-Gameplay-Portrait.mp4','MagLava-AppPreview.mp4','MagLava-Trailer-Poster.jpg']]
for locale in rows:files.extend(sorted((out/'store'/locale).glob('*.png')))
assert len([f for f in files if f.suffix=='.png'])==133,'126 screenshots and seven feature graphics required'
manifest={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
archive=out/f'MagLava-Press-Kit-{a.build}.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for f in files:z.write(f,str(f.relative_to(out)))
 z.writestr('SHA256SUMS.json',json.dumps(manifest,indent=2)+'\n')
print(archive,archive.stat().st_size)
