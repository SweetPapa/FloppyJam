#!/usr/bin/env python3
"""Deliver one verified UAT run. Local mode keeps all signing keys on this Mac."""
import argparse,base64,hashlib,json,os,shutil,subprocess,tempfile,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--incoming',required=True,type=Path);p.add_argument('--build',required=True,type=int);p.add_argument('--ci',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];os.chdir(root);incoming=a.incoming.resolve()
receipts=root/'mobile/.build/uat/receipts';receipts.mkdir(parents=True,exist_ok=True);(root/'mobile/.build/store').mkdir(parents=True,exist_ok=True)
def unique(name):
 files=list(incoming.rglob(name));assert len(files)==1,(name,files);return files[0]
versions=[json.loads(f.read_text()) for f in incoming.rglob('version.json')]
assert versions and all(v==versions[0] for v in versions),'Artifacts must come from one UAT build.'
version=versions[0];assert version['build']==a.build
media=unique('MagLava-AppPreview.mp4').parent
if os.environ.get('GITHUB_SHA'):assert version['sha']==os.environ['GITHUB_SHA']
def run(*cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
with tempfile.TemporaryDirectory(prefix='maglava-delivery-') as tmp:
 work=Path(tmp);env=os.environ.copy()
 def secret(name,dest,encoded=True):
  data=env.pop(name);dest.write_bytes(base64.b64decode(data) if encoded else data.encode());dest.chmod(0o600);return dest
 if a.ci:
  store=secret('MAGLAVA_ANDROID_KEYSTORE',work/'upload.keystore')
  profile=secret('MAGLAVA_IOS_PROFILE',work/'MagLava.mobileprovision')
  secret('MAGLAVA_PLAY_CREDENTIALS',work/'play.json',False);env['GOOGLE_APPLICATION_CREDENTIALS']=str(work/'play.json')
  key=secret('MAGLAVA_ASC_KEY_P8',work/'AuthKey.p8');env['ASC_KEY_PATH']=str(key)
 else:
  store=Path.home()/'code/SPT';profile=root/'mobile/.build/store/MagLava-iOS.mobileprovision'
  env.setdefault('ASC_KEY_ID','4648S8AZQV');env.setdefault('ASC_ISSUER_ID','08a7930e-9034-41cd-ab35-41afa2b19813')
  key=Path(env.get('ASC_KEY_PATH',str(Path.home()/f"Downloads/AuthKey_{env['ASC_KEY_ID']}.p8")));env['ASC_KEY_PATH']=str(key)
  for variable,account in [('MAGLAVA_STORE_PASSWORD',f'KEY_STORE_PASSWORD__{store}'),('MAGLAVA_KEY_PASSWORD',f'KEY_PASSWORD__{store}__spt')]:
   env.setdefault(variable,subprocess.check_output(['security','find-generic-password','-a',account,'-w'],text=True).strip())
 sdk=Path(env.get('ANDROID_HOME',str(Path.home()/'Library/Android/sdk')));bt=sdk/'build-tools/36.1.0';assert bt.is_dir()
 signed=root/'mobile/.build/uat/signed'/str(a.build);signed.mkdir(parents=True,exist_ok=True)
 bundle=signed/f'MagLava-1.0-{a.build}.aab';apk=signed/f'MagLava-1.0-{a.build}.apk'
 shutil.copy2(unique('MagLava-Android-unsigned.aab'),bundle)
 run('jarsigner','-keystore',store,'-storepass:env','MAGLAVA_STORE_PASSWORD','-keypass:env','MAGLAVA_KEY_PASSWORD',bundle,'spt',env=env)
 run('jarsigner','-verify',bundle)
 run(bt/'zipalign','-f','-P','16','4',unique('MagLava-Android-unsigned.apk'),apk)
 run(bt/'apksigner','sign','--ks',store,'--ks-key-alias','spt','--ks-pass','env:MAGLAVA_STORE_PASSWORD','--key-pass','env:MAGLAVA_KEY_PASSWORD',apk,env=env)
 run(bt/'apksigner','verify',apk)
 # Ensure an archive cannot escape the temporary extraction directory.
 archive=unique('Maglava-iOS-unsigned.zip')
 with zipfile.ZipFile(archive) as z:
  for entry in z.infolist():
   path=Path(entry.filename);assert not path.is_absolute() and '..' not in path.parts
 unpack=work/'archive';unpack.mkdir();run('ditto','-x','-k',archive,unpack)
 for path in unpack.rglob('*'):
  if path.is_symlink():assert path.resolve().is_relative_to(unpack.resolve())
 ios=unpack/'Maglava-iOS.xcarchive';ipa=signed/f'MagLava-1.0-{a.build}.ipa'
 import plistlib
 info=plistlib.loads((ios/'Products/Applications/Maglava.app/Info.plist').read_bytes());assert info['CFBundleVersion']==str(a.build)
 run('python3','mobile/tools/store/sign-ios.py',ios,'--profile',profile,'--output',ipa,env=env)
 receipt={'build':a.build,'sha':version['sha'],'files':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in [bundle,apk,ipa]}}
 (receipts/'signing.json').write_text(json.dumps(receipt,indent=2)+'\n')
 # Store failures are independent: still deliver the other platform, then fail honestly.
 failures=[]
 for platform in ['android','ios']:
  try:
   if platform=='android':
    run('node','mobile/ci/publish-play.mjs','--build',a.build,'--bundle',bundle,env=env)
    run('node','mobile/ci/publish-media.mjs','--directory',media,'--platform','android',env=env)
   else:
    keys=work/'private_keys';keys.mkdir();shutil.copy2(key,keys/f"AuthKey_{env['ASC_KEY_ID']}.p8");env['API_PRIVATE_KEYS_DIR']=str(keys)
    present=json.loads(subprocess.check_output(['node','mobile/ci/testflight.mjs','--build',str(a.build),'--inspect'],env=env))
    if not present:run('xcrun','altool','--upload-app','--type','ios','--file',ipa,'--apiKey',env['ASC_KEY_ID'],'--apiIssuer',env['ASC_ISSUER_ID'],'--output-format','json',env=env)
    run('node','mobile/tools/store/setup-apple.mjs',env=env)
    run('node','mobile/ci/publish-media.mjs','--directory',media,'--platform','ios',env=env)
    run('node','mobile/ci/testflight.mjs','--build',a.build,env=env)
  except subprocess.CalledProcessError as error:failures.append(f'{platform}: command failed with status {error.returncode}')
 if failures:raise SystemExit('\n'.join(failures))
