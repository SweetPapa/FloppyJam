#!/usr/bin/env python3
"""Render six current gameplay cards per platform in every supported language."""
import argparse,json,subprocess,concurrent.futures
from pathlib import Path
root=Path(__file__).resolve().parents[2];media=root/'mobile/media'
p=argparse.ArgumentParser();p.add_argument('--platform',choices=['iphone','ipad','android','all'],default='all');p.add_argument('--locales',nargs='+');p.add_argument('--workers',type=int,default=2);a=p.parse_args()
rows=json.loads((root/'mobile/store/localizations.json').read_text(encoding='utf-8'));stages=[1,6,12,20,30,38]
bundle=root/'mobile/.build/media-bundle'
# Install the shared browser once before concurrent still workers can race
# downloading/extracting/removing the same archive on a fresh CI runner.
subprocess.run(['npx','remotion','browser','ensure'],cwd=media,check=True)
subprocess.run(['npx','remotion','bundle','src/index.tsx','--out-dir',str(bundle)],cwd=media,check=True)
jobs=[]
for locale,r in rows.items():
 if a.locales and locale not in a.locales:continue
 out=media/'out/store'/locale;out.mkdir(parents=True,exist_ok=True)
 for platform in (['iphone','ipad','android'] if a.platform=='all' else [a.platform]):
  composition={'iphone':'iPhoneCard','ipad':'iPadCard','android':'AndroidCard'}[platform]
  for i,stage in enumerate(stages,1):
   image=f'captures/android/stage-{stage}.png' if platform=='android' else f'captures/apple/{platform}-stage-{stage}.png'
   props=out/f'{platform}-{i}.json';props.write_text(json.dumps({'platform':platform,'image':image,'title':r['cardTitles'][i-1],'subtitle':r['cardSubtitles'][i-1],'index':f'{i:02d}','locale':locale}),encoding='utf-8')
   jobs.append(['npx','remotion','still',str(bundle),composition,str(out/f'{platform}-{i}.png'),'--props',str(props),'--image-format','png'])
def render(cmd):subprocess.run(cmd,cwd=media,check=True,stdout=subprocess.DEVNULL)
with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:list(pool.map(render,jobs))
if a.platform in ['all','android']:
 for locale,r in rows.items():
  if a.locales and locale not in a.locales:continue
  out=media/'out/store'/locale;props=out/'feature.json';props.write_text(json.dumps({'locale':locale,'subtitle':r['subtitle'],'detail':r['cardSubtitles'][0]}),encoding='utf-8')
  render(['npx','remotion','still',str(bundle),'FeatureGraphic',str(out/'feature-graphic.png'),'--props',str(props),'--image-format','png'])
print(f'Rendered {len(jobs)} localized screenshot cards.')
