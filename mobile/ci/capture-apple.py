#!/usr/bin/env python3
"""Capture current iPhone/iPad UI and real gameplay. Shut down owned simulators."""
import argparse,json,os,signal,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];os.chdir(root)
p=argparse.ArgumentParser();p.add_argument('--stages',type=int,nargs='+',default=[1,6,38]);p.add_argument('--reuse-tested-build',action='store_true');a=p.parse_args()
assert not (a.reuse_tested_build and os.environ.get('CI')), 'CI must run native integration tests.'
out=root/'mobile/.build/uat/captures/apple';out.mkdir(parents=True,exist_ok=True)
def run(*args,**kw):return subprocess.run(list(args),check=True,**kw)
def sim(*args,**kw):return run('xcrun','simctl',*args,**kw)
available=json.loads(sim('list','devices','available','-j',capture_output=True,text=True).stdout)['devices']
devices=[d for runtime,items in available.items() if 'iOS' in runtime for d in items]
phone=next((d for d in devices if 'Pro Max' in d['name']),next(d for d in devices if 'iPhone' in d['name']))
pad=next((d for d in devices if 'iPad Pro 13' in d['name']),next(d for d in devices if 'iPad Pro' in d['name']))
derived=root/'mobile/.build/capture-ios'
for kind,device in [('iphone',phone),('ipad',pad)]:
 udid=device['udid'];owned=device['state']!='Booted'
 try:
  print('Preparing',kind,device['name'],udid,flush=True)
  if owned:sim('boot',udid)
  # Fresh hosted iOS 26 devices spend several minutes in first-boot migration.
  sim('bootstatus',udid,'-b',timeout=900)
  if not a.reuse_tested_build:
   logpath=out/f'{kind}-test.log'
   try:
    with logpath.open('w') as log:
     run('xcodebuild','-project','mobile/ios/Maglava.xcodeproj','-scheme','Maglava','-destination',f'platform=iOS Simulator,id={udid}','-destination-timeout','120','-parallel-testing-enabled','NO','-derivedDataPath',str(derived),'CODE_SIGNING_ALLOWED=NO','test',stdout=log,stderr=subprocess.STDOUT,timeout=600)
   finally:print(logpath.read_text()[-18000:],flush=True)
  if owned:
   # XCTest may use a clone; explicitly boot the selected capture device.
   status=json.loads(sim('list','devices','-j',capture_output=True,text=True).stdout)
   current=next(d for ds in status['devices'].values() for d in ds if d['udid']==udid)
   if current['state']!='Booted':sim('boot',udid)
  sim('bootstatus',udid,'-b')
  sim('status_bar',udid,'override','--time','9:41','--dataNetwork','wifi','--wifiMode','active','--wifiBars','3','--batteryState','charged','--batteryLevel','100')
  sim('install',udid,str(derived/'Build/Products/Debug-iphonesimulator/Maglava.app'))
  sim('launch','--terminate-running-process',udid,'dev.fofo.maglava');time.sleep(2)
  sim('io',udid,'screenshot',str(out/f'{kind}-home.png'))
  for level in a.stages:
   sim('launch','--terminate-running-process',udid,'dev.fofo.maglava','--level',str(level),'--media-tour')
   record=subprocess.Popen(['xcrun','simctl','io',udid,'recordVideo','--codec=h264','--force',str(out/f'{kind}-stage-{level}.mp4')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   time.sleep(4);sim('io',udid,'screenshot',str(out/f'{kind}-stage-{level}.png'));time.sleep(8)
   record.send_signal(signal.SIGINT);record.wait(timeout=20)
  (out/f'{kind}-device.json').write_text(json.dumps({'device':device['name'],'source':os.environ.get('GITHUB_SHA','local'),'method':'Native Metal screen recording; debug bot selects actual color inputs.'},indent=2)+'\n')
 finally:
  if owned:subprocess.run(['xcrun','simctl','shutdown',udid],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print('Apple gameplay, screenshots and video captured:',out)
