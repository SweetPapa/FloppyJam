#!/usr/bin/env python3
"""Sign an unsigned Xcode 26 archive with this Mac's existing SPT identity."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('archive', type=Path)
parser.add_argument('--profile', type=Path, default=Path('mobile/.build/store/MagLava-iOS.mobileprovision'))
parser.add_argument('--output', type=Path, default=None)
args = parser.parse_args()
bundle_id = 'dev.fofo.maglava'
team_id = '6Y5SZ2K5XY'
identity = 'CF531FDE8C2985AB5A3547FA481B8192DEBD5D02'
profile = plistlib.loads(subprocess.check_output(['security', 'cms', '-D', '-i', str(args.profile)]))
entitlements = profile['Entitlements']
if entitlements.get('application-identifier') != f'{team_id}.{bundle_id}':
    raise SystemExit('Provisioning profile does not match the game and SPT team.')
if entitlements.get('get-task-allow') or profile.get('ProvisionedDevices'):
    raise SystemExit('Use an App Store distribution profile, not development/ad hoc.')
if 'keychain-access-groups' in entitlements:
    entitlements['keychain-access-groups'] = [f'{team_id}.{bundle_id}']
source = args.archive / 'Products/Applications/Maglava.app'
info = plistlib.loads((source / 'Info.plist').read_bytes())
if info.get('CFBundleIdentifier') != bundle_id or int(info.get('DTSDKName', 'iphoneos0').removeprefix('iphoneos').split('.')[0]) < 26:
    raise SystemExit('Expected the game archive built with the iOS 26 SDK or later.')
if args.output is None:
    args.output = Path('mobile/.build/store') / f"MagLava-{info['CFBundleShortVersionString']}-{info['CFBundleVersion']}.ipa"
args.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='maglava-sign-') as work:
    payload = Path(work) / 'Payload'
    payload.mkdir()
    app = payload / 'Maglava.app'
    subprocess.run(['ditto', str(source), str(app)], check=True)
    shutil.copyfile(args.profile, app / 'embedded.mobileprovision')
    entitlement_file = Path(work) / 'entitlements.plist'
    entitlement_file.write_bytes(plistlib.dumps(entitlements))
    frameworks = app / 'Frameworks'
    if frameworks.exists():
        for framework in sorted(frameworks.iterdir()):
            if framework.suffix in ('.framework', '.dylib'):
                subprocess.run(['codesign', '--force', '--sign', identity, '--timestamp=none', str(framework)], check=True)
    subprocess.run(['codesign', '--force', '--sign', identity, '--entitlements', str(entitlement_file), '--generate-entitlement-der', '--timestamp=none', str(app)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(app)], check=True)
    subprocess.run(['ditto', '-c', '-k', '--keepParent', str(payload), str(args.output.resolve())], check=True)
print(f'Signed IPA: {args.output}')
