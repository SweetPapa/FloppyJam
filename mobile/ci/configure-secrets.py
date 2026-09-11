#!/usr/bin/env python3
"""After explicit approval, transfer only MagLava signing credentials to UAT secrets.
The environment's deployment branch policy must allow only `uat`.
Never writes private key material into the repository or prints credential values.
"""
import argparse,base64,os,secrets,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--apply',action='store_true');a=p.parse_args()
repo='SweetPapa/FloppyJam';environment='maglava-uat'
names=['MAGLAVA_IOS_CERT_P12','MAGLAVA_IOS_CERT_PASSWORD','MAGLAVA_ASC_KEY_P8','MAGLAVA_ASC_KEY_ID','MAGLAVA_ASC_ISSUER_ID','MAGLAVA_IOS_PROFILE','MAGLAVA_PLAY_CREDENTIALS','MAGLAVA_ANDROID_KEYSTORE','MAGLAVA_STORE_PASSWORD','MAGLAVA_KEY_PASSWORD','MAGLAVA_REVIEW_PHONE']
if not a.apply:
 print('Destination:',repo,'environment:',environment);print('\n'.join(names));raise SystemExit(0)
root=Path(__file__).resolve().parents[2];os.chdir(root)
phone=os.environ['MAGLAVA_REVIEW_PHONE']
def put(name,data):
 subprocess.run(['gh','secret','set',name,'-R',repo,'--env',environment],input=data if isinstance(data,bytes) else data.encode(),check=True,stdout=subprocess.DEVNULL)
 print('Configured',name)
with tempfile.TemporaryDirectory(prefix='maglava-ci-') as temp:
 password=secrets.token_urlsafe(32);env=os.environ.copy();env['MAGLAVA_P12_PASSWORD']=password
 p12=Path(temp)/'distribution.p12';exporter=Path(temp)/'export-distribution'
 subprocess.run(['swiftc','mobile/ci/export-distribution.swift','-o',str(exporter)],check=True)
 subprocess.run([str(exporter),str(p12)],env=env,check=True,timeout=120)
 put('MAGLAVA_IOS_CERT_P12',base64.b64encode(p12.read_bytes()));put('MAGLAVA_IOS_CERT_PASSWORD',password)
put('MAGLAVA_ASC_KEY_P8',base64.b64encode((Path.home()/'Downloads/AuthKey_4648S8AZQV.p8').read_bytes()))
put('MAGLAVA_ASC_KEY_ID','4648S8AZQV');put('MAGLAVA_ASC_ISSUER_ID','08a7930e-9034-41cd-ab35-41afa2b19813')
put('MAGLAVA_IOS_PROFILE',base64.b64encode(Path('mobile/.build/store/MagLava-iOS.mobileprovision').read_bytes()))
put('MAGLAVA_PLAY_CREDENTIALS',(Path.home()/'secrets/fofo-play-publisher.json').read_bytes())
store=Path.home()/'code/SPT';put('MAGLAVA_ANDROID_KEYSTORE',base64.b64encode(store.read_bytes()))
for name,account in [('MAGLAVA_STORE_PASSWORD',f'KEY_STORE_PASSWORD__{store}'),('MAGLAVA_KEY_PASSWORD',f'KEY_PASSWORD__{store}__spt')]:
 put(name,subprocess.check_output(['security','find-generic-password','-a',account,'-w']).strip())
put('MAGLAVA_REVIEW_PHONE',phone)
