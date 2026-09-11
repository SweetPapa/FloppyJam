#!/usr/bin/env python3
"""Sign already-built release artifacts with the existing local SPT upload key."""
import hashlib,json,os,subprocess,plistlib,re
from pathlib import Path
root=Path(__file__).resolve().parents[3]
output=root/'mobile/.build/store'; output.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
store=Path(env.get('MAGLAVA_KEYSTORE',str(Path.home()/'code/SPT')))
alias=env.get('MAGLAVA_KEY_ALIAS','spt')
for variable,account in [('MAGLAVA_STORE_PASSWORD',f'KEY_STORE_PASSWORD__{store}'),('MAGLAVA_KEY_PASSWORD',f'KEY_PASSWORD__{store}__{alias}')]:
    if not env.get(variable): env[variable]=subprocess.check_output(['security','find-generic-password','-a',account,'-w'],text=True).strip()
sdk=Path(env.get('ANDROID_HOME',str(Path.home()/'Library/Android/sdk')))
build_tools=sdk/'build-tools/36.1.0'
import shutil
info=plistlib.loads((root/'mobile/ios/Maglava/Info.plist').read_bytes())
version=info['CFBundleShortVersionString']; build=int(info['CFBundleVersion'])
gradle=(root/'mobile/android/app/build.gradle.kts').read_text()
assert int(re.search(r'versionCode = (\d+)',gradle)[1])==build
assert re.search(r'versionName = "([^"]+)"',gradle)[1]==version
bundle=output/f'MagLava-{version}-{build}.aab'; apk=output/f'MagLava-{version}-{build}.apk' 
shutil.copyfile(root/'mobile/android/app/build/outputs/bundle/release/app-release.aab',bundle)
subprocess.run(['jarsigner','-keystore',str(store),'-storepass:env','MAGLAVA_STORE_PASSWORD','-keypass:env','MAGLAVA_KEY_PASSWORD',str(bundle),alias],env=env,check=True)
subprocess.run(['jarsigner','-verify',str(bundle)],check=True)
subprocess.run([str(build_tools/'zipalign'),'-f','-P','16','4',str(root/'mobile/android/app/build/outputs/apk/release/app-release-unsigned.apk'),str(apk)],check=True)
subprocess.run([str(build_tools/'apksigner'),'sign','--ks',str(store),'--ks-key-alias',alias,'--ks-pass','env:MAGLAVA_STORE_PASSWORD','--key-pass','env:MAGLAVA_KEY_PASSWORD',str(apk)],env=env,check=True)
subprocess.run([str(build_tools/'apksigner'),'verify','--print-certs',str(apk)],check=True)
receipt={'package':'dev.fofo.maglava','version':version,'build':build,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [bundle,apk]}}
(output/f'android-signing-{build}.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
