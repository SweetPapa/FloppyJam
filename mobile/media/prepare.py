#!/usr/bin/env python3
"""Stage recorded footage and authored audio for deterministic Remotion rendering."""
import argparse,shutil,hashlib,urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--captures',type=Path,default=root/'mobile/.build/uat/captures');a=p.parse_args()
out=root/'mobile/media/public';out.mkdir(exist_ok=True)
shutil.copytree(a.captures,out/'captures',dirs_exist_ok=True)
shutil.copytree(root/'v4/assets/fonts',out/'fonts',dirs_exist_ok=True)
shutil.copy2(root/'mobile/assets/Audio/magLava-game-bg-2.m4a',out/'music.m4a')
# Full CJK coverage for localized marketing captions; pinned official OFL fonts.
for name,folder,file,digest in [
 ('Japanese','Japanese','NotoSansCJKjp-Regular.otf','68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5'),
 ('Chinese','SimplifiedChinese','NotoSansCJKsc-Regular.otf','2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b')]:
 dest=out/'fonts'/('Noto'+name+'.otf')
 if not dest.exists():
  cached=root/'mobile/.build/uat/i18n-fonts'/(name+'.otf')
  data=cached.read_bytes() if cached.exists() else urllib.request.urlopen(f'https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTF/{folder}/{file}').read()
  assert hashlib.sha256(data).hexdigest()==digest,'Unexpected font download'
  dest.write_bytes(data)
 assert hashlib.sha256(dest.read_bytes()).hexdigest()==digest,'Unexpected font data'
print('Prepared real gameplay, original music and licensed fonts. No narration.')
