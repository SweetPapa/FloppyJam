#!/usr/bin/env python3
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=int);a=p.parse_args()
root=Path(__file__).resolve().parents[2];incoming=root/'mobile/.build/uat/incoming';out=root/'mobile/.build/uat/publish';out.mkdir(parents=True,exist_ok=True)
names=['MagLava-macOS.dmg','MagLava-macOS.zip','MagLava-Windows-x64.zip','MagLava-Linux-x64.tar.gz','MagLava-Trailer.mp4','MagLava-AppPreview.mp4']
files=[]
for name in names:
 found=list(incoming.rglob(name));assert len(found)==1,(name,found);files.append(found[0])
sha=os.environ['GITHUB_SHA'];tag=f'maglava-uat-{a.build}'
checksums=out/'SHA256SUMS.txt';checksums.write_text(''.join(f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n' for f in files))
notes=out/'notes.md';notes.write_text(f'''MagLava 1.0 ({a.build}) UAT\n\n40 magnetic climbs above rising lava.\n\n- macOS: signed and notarized universal app, with the native mobile 3D presentation.\n- Windows x64: signed desktop C build.\n- Linux x64: desktop C build.\n- Mobile delivery: TestFlight and Google Play closed testing are tracked separately in the workflow; these downloads do not imply mobile approval.\n\nWindows/Linux retain the desktop C presentation and procedural music. iOS/Android/macOS share the native mobile presentation and original playlist.\n\nBuilt from `{sha}` on `uat`. Verify downloads with SHA256SUMS.txt. Production promotion is manual.\n''')
subprocess.run(['gh','release','create',tag,*map(str,files),str(checksums),'--prerelease','--title',f'MagLava 1.0 ({a.build}) — UAT','--notes-file',str(notes),'--target',sha],check=True)
