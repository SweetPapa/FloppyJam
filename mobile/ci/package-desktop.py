#!/usr/bin/env python3
import json,platform,subprocess,tarfile,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=root/'mobile/.build/uat/desktop';out.mkdir(parents=True,exist_ok=True)
windows=platform.system()=='Windows';exe=root/'mobile/.build/desktop'/('Release/maglava.exe' if windows else 'maglava')
assert exe.is_file()
license=root/'v4/assets/fonts/OFL.txt'
if windows:
 subprocess.run(['powershell','-NoProfile','-Command',f"if ((Get-AuthenticodeSignature '{exe}').Status -ne 'Valid') {{ throw 'Invalid MagLava signature' }}"],check=True)
 with zipfile.ZipFile(out/'MagLava-Windows-x64.zip','w',zipfile.ZIP_DEFLATED) as z:
  z.write(exe,'MagLava/maglava.exe');z.write(license,'MagLava/licenses/OFL.txt')
else:
 with tarfile.open(out/'MagLava-Linux-x64.tar.gz','w:gz') as z:
  z.add(exe,arcname='MagLava/maglava');z.add(license,arcname='MagLava/licenses/OFL.txt')
