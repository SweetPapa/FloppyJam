#!/usr/bin/env python3
"""Capture real native gameplay after instrumentation, with reproducible stage selection."""
import argparse,json,os,re,shutil,signal,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];os.chdir(root)
p=argparse.ArgumentParser();p.add_argument('--stages',type=int,nargs='+',default=[1,6,38]);p.add_argument('--reuse-tested-build',action='store_true');a=p.parse_args()
assert not (a.reuse_tested_build and os.environ.get('CI')), 'CI must run native integration tests.'
out=root/'mobile/.build/uat/captures/android';out.mkdir(parents=True,exist_ok=True)
adb=shutil.which('adb') or str(Path.home()/'Library/Android/sdk/platform-tools/adb')
def run(*args,**kw):return subprocess.run([adb,*args],check=True,**kw)
# A fresh emulator shows Android's first-use full-screen tutorial over the app.
# Acknowledge that system tutorial before testing actual app touch targets.
run('shell','settings','put','secure','immersive_mode_confirmations','confirmed')
run('shell','settings','put','system','screen_off_timeout','1800000')
run('shell','wm','dismiss-keyguard')
for path in ['apk/debug/app-debug.apk','apk/androidTest/debug/app-debug-androidTest.apk']:
 run('install','-r',str(root/'mobile/android/app/build/outputs'/path))
if not a.reuse_tested_build:
 for attempt in range(2):
  result=run('shell','am','instrument','-w','dev.fofo.maglava.test/dev.fofo.maglava.SmokeRunner',capture_output=True,text=True,timeout=180)
  (out/'native-test.log').write_text(result.stdout+result.stderr)
  if 'PASS:' in result.stdout and 'FAIL:' not in result.stdout:break
  with (out/f'failure-{attempt}.png').open('wb') as f:run('exec-out','screencap','-p',stdout=f)
  windows=run('shell','dumpsys','window',capture_output=True,text=True).stdout
  (out/f'failure-{attempt}-windows.txt').write_text(windows)
  (out/f'failure-{attempt}-test.log').write_text(result.stdout+result.stderr)
  launcher_anr=re.search(r'mCurrentFocus=.*Application Not Responding: com\.android\.launcher3',windows)
  emulator=run('shell','getprop','ro.kernel.qemu',capture_output=True,text=True).stdout.strip()=='1'
  if attempt or not emulator or not launcher_anr or 'Targeted input event injection' not in result.stdout:break
  print('::warning::Emulator Quickstep launcher ANR intercepted input; restarting that launcher and rerunning native tests once. Diagnostics retained.',flush=True)
  run('shell','am','force-stop','com.android.launcher3')
  run('shell','am','force-stop','dev.fofo.maglava')
  time.sleep(3)
 assert 'PASS:' in result.stdout and 'FAIL:' not in result.stdout,result.stdout
 for name in ['maglava-home.png','maglava-game-1.png','maglava-game-2.png','maglava-complete.png']:
  with (out/name).open('wb') as f:run('exec-out','run-as','dev.fofo.maglava','cat','cache/'+name,stdout=f)
for level in a.stages:
 run('shell','am','force-stop','dev.fofo.maglava')
 run('logcat','-c')
 run('shell','am','start','-W','-n','dev.fofo.maglava/.MainActivity','--ei','level',str(level),'--ez','media_tour','true',timeout=90)
 # Launch time varies on hosted emulators. Require focused native gameplay and
 # three successful GLES frames before starting either the recording or its clock.
 deadline=time.monotonic()+60
 while time.monotonic()<deadline:
  log=run('logcat','-d','-s','MagLavaCapture:I','*:S',capture_output=True,text=True).stdout
  if f'READY stage={level}' in log:break
  time.sleep(.5)
 else:
  (out/f'stage-{level}-startup.log').write_text(run('logcat','-d',capture_output=True,text=True).stdout)
  raise RuntimeError(f'Stage {level} never rendered focused gameplay; refusing startup-screen media.')
 (out/f'stage-{level}-ready.log').write_text(log)
 remote=f'/sdcard/maglava-stage-{level}.mp4'
 record=subprocess.Popen([adb,'shell','screenrecord','--bit-rate','12000000','--time-limit','12',remote],stdout=subprocess.DEVNULL)
 time.sleep(4)
 with (out/f'stage-{level}.png').open('wb') as f:run('exec-out','screencap','-p',stdout=f)
 record.wait(timeout=30);run('pull',remote,str(out/f'stage-{level}.mp4'));run('shell','rm',remote)
(out/'capture.json').write_text(json.dumps({'source':os.environ.get('GITHUB_SHA','local'),'platform':'Android','stages':a.stages,'method':'Native GLES screen recording; debug bot selects actual color inputs.'},indent=2)+'\n')
print('Android gameplay, screenshots and video captured:',out)
