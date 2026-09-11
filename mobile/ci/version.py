#!/usr/bin/env python3
"""Set a unique UAT build in both apps; never commit generated version bumps."""
import argparse,json,os,plistlib,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=int);a=p.parse_args()
if not 3<=a.build<=2100000000: p.error('build must be 3..2100000000')
root=Path(__file__).resolve().parents[2]
info=root/'mobile/ios/Maglava/Info.plist';data=plistlib.loads(info.read_bytes());data['CFBundleVersion']=str(a.build);info.write_bytes(plistlib.dumps(data,sort_keys=False))
gradle=root/'mobile/android/app/build.gradle.kts';source=gradle.read_text();source,n=re.subn(r'versionCode = \d+',f'versionCode = {a.build}',source);assert n==1;gradle.write_text(source)
receipt={'version':data['CFBundleShortVersionString'],'build':a.build,'sha':os.environ.get('GITHUB_SHA','local')}
out=root/'mobile/.build/uat';out.mkdir(parents=True,exist_ok=True);(out/'version.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
