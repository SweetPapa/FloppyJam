#!/usr/bin/env python3
"""Stage recorded footage and authored audio for deterministic Remotion rendering."""
import argparse,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--captures',type=Path,default=root/'mobile/.build/uat/captures');p.add_argument('--voice',type=Path,default=root/'mobile/media/assets/narration.flac');a=p.parse_args()
out=root/'mobile/media/public';out.mkdir(exist_ok=True)
shutil.copytree(a.captures,out/'captures',dirs_exist_ok=True)
shutil.copytree(root/'v4/assets/fonts',out/'fonts',dirs_exist_ok=True)
shutil.copy2(root/'mobile/assets/Audio/magLava-game-bg-2.m4a',out/'music.m4a')
subprocess.run(['ffmpeg','-v','error','-y','-i',str(a.voice),'-af','loudnorm=I=-16:TP=-1.5:LRA=9','-ar','48000',str(out/'narration.wav')],check=True)
print('Prepared real gameplay, original music, approved AssetForge narration and licensed Barlow fonts.')
