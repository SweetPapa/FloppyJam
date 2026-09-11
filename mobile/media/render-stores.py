#!/usr/bin/env python3
import argparse,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];media=root/'mobile/media'
p=argparse.ArgumentParser();p.add_argument('--platform',choices=['iphone','ipad','android','all'],default='all');a=p.parse_args()
out=media/'out/store';out.mkdir(parents=True,exist_ok=True)
for platform in (['iphone','ipad','android'] if a.platform=='all' else [a.platform]):
 composition={'iphone':'iPhoneCard','ipad':'iPadCard','android':'AndroidCard'}[platform]
 for i,(stage,title,subtitle) in enumerate([(1,'Catch a color. Carry your swing.','Four colors. One way up.'),(6,'Keep climbing. Stay alive.','Ghosts, lasers and rising lava.'),(9,'Forty stages. One more try.','Checkpoints. Stars. Personal bests.')],1):
  image=f'captures/android/stage-{stage}.png' if platform=='android' else f'captures/apple/{platform}-stage-{stage}.png'
  props=out/f'{platform}-{i}.json';props.write_text(json.dumps({'platform':platform,'image':image,'title':title,'subtitle':subtitle,'index':f'{i:02d}'}))
  subprocess.run(['npx','remotion','still','src/index.tsx',composition,str(out/f'{platform}-{i}.png'),'--props',str(props),'--image-format','png'],cwd=media,check=True)
if a.platform in ['all','android']:
 subprocess.run(['npx','remotion','still','src/index.tsx','FeatureGraphic',str(out/'feature-graphic.png'),'--image-format','png'],cwd=media,check=True)
