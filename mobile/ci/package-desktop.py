#!/usr/bin/env python3
import json,platform,struct,subprocess,tarfile,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=root/'mobile/.build/uat/desktop';out.mkdir(parents=True,exist_ok=True)
windows=platform.system()=='Windows';exe=root/'mobile/.build/desktop'/('Release/maglava.exe' if windows else 'maglava')
assert exe.is_file()
license=root/'v4/assets/fonts/OFL.txt'
if windows:
 # Verify actual PE imports, not embedded strings from optional graphics drivers.
 data=exe.read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
 assert data[pe:pe+4]==b'PE\0\0'
 count=struct.unpack_from('<H',data,pe+6)[0];optional=pe+24
 size=struct.unpack_from('<H',data,pe+20)[0]
 def offset(rva):
  for i in range(count):
   virtual_size,address,raw_size,raw=struct.unpack_from('<IIII',data,optional+size+i*40+8)
   if address<=rva<address+max(virtual_size,raw_size):return raw+rva-address
  raise ValueError('Invalid PE import address')
 directory=112 if struct.unpack_from('<H',data,optional)[0]==0x20b else 96
 imports=offset(struct.unpack_from('<I',data,optional+directory+8)[0]);dlls=[]
 while any(data[imports:imports+20]):
  name=offset(struct.unpack_from('<I',data,imports+12)[0])
  dlls.append(data[name:data.index(b'\0',name)].decode('ascii'));imports+=20
 assert not any(d.upper().startswith(('VCRUNTIME','MSVCP','MSVCR')) for d in dlls),dlls
 print('Windows dependencies:',', '.join(dlls))
 subprocess.run(['pwsh','-NoProfile','-Command',f"$ErrorActionPreference='Stop'; if ((Get-AuthenticodeSignature -LiteralPath '{exe}').Status -ne 'Valid') {{ throw 'Invalid MagLava signature' }}"],check=True)
 with zipfile.ZipFile(out/'MagLava-Windows-x64.zip','w',zipfile.ZIP_DEFLATED) as z:
  z.write(exe,'MagLava/maglava.exe');z.write(license,'MagLava/licenses/OFL.txt')
else:
 with tarfile.open(out/'MagLava-Linux-x64.tar.gz','w:gz') as z:
  z.add(exe,arcname='MagLava/maglava');z.add(license,arcname='MagLava/licenses/OFL.txt')
