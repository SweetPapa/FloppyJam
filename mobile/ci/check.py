#!/usr/bin/env python3
"""Fast release invariants: matching versions/assets and isolated version generation."""
import hashlib,json,plistlib,re,shutil,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
info=plistlib.loads((root/'mobile/ios/Maglava/Info.plist').read_bytes())
gradle=(root/'mobile/android/app/build.gradle.kts').read_text()
assert re.search(r'versionCode = (\d+)',gradle)[1]==info['CFBundleVersion']
assert re.search(r'versionName = "([^"]+)"',gradle)[1]==info['CFBundleShortVersionString']
for row in json.loads((root/'mobile/assets/Audio/soundtrack.json').read_text()):
 data=(root/'mobile/assets/Audio'/row['file']).read_bytes();assert hashlib.sha256(data).hexdigest()==row['sha256']
 assert 90<row['duration_seconds']<180
voice=json.loads((root/'mobile/media/assets/narration.json').read_text())
assert hashlib.sha256((root/'mobile/media/narration.json').read_bytes()).hexdigest()==voice['request_sha256']
assert hashlib.sha256((root/'mobile/media/assets/narration.flac').read_bytes()).hexdigest()==voice['audio_sha256']
subprocess.run(['node','--input-type=module','-e',"import './mobile/ci/store-localizations.mjs'"],cwd=root,check=True)
for path in (root/'mobile/ci').glob('*.py'):compile(path.read_text(),str(path),'exec')
for path in (root/'mobile/ci').glob('*.mjs'):subprocess.run(['node','--check',str(path)],check=True)
with tempfile.TemporaryDirectory() as tmp:
 test=Path(tmp)
 for name in ['mobile/ci/version.py','mobile/ios/Maglava/Info.plist','mobile/android/app/build.gradle.kts']:
  dest=test/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,dest)
 subprocess.run(['python3',str(test/'mobile/ci/version.py'),'--build','1001'],check=True,capture_output=True)
 assert plistlib.loads((test/'mobile/ios/Maglava/Info.plist').read_bytes())['CFBundleVersion']=='1001'
 assert 'versionCode = 1001' in (test/'mobile/android/app/build.gradle.kts').read_text()
 assert subprocess.run(['python3',str(test/'mobile/ci/version.py'),'--build','2'],capture_output=True).returncode!=0
print('Matching platform versions, full soundtrack hashes, release scripts and isolated version generation passed.')
