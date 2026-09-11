#!/usr/bin/env python3
"""Create a streaming-friendly trailer and poster from the Remotion master."""
import json,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent/'out'
source=out/'MagLava-Trailer.mp4'
analysis=subprocess.run(['ffmpeg','-hide_banner','-i',str(source),'-af','loudnorm=I=-16:TP=-1.5:LRA=9:print_format=json','-f','null','-'],capture_output=True,text=True,check=True)
values,_=json.JSONDecoder().raw_decode(analysis.stderr[analysis.stderr.rfind('{'):])
filters='loudnorm=I=-16:TP=-1.5:LRA=9:linear=true:'+':'.join(f'{key}={values[value]}' for key,value in [('measured_I','input_i'),('measured_TP','input_tp'),('measured_LRA','input_lra'),('measured_thresh','input_thresh'),('offset','target_offset')])
subprocess.run(['ffmpeg','-v','error','-y','-i',str(source),'-vf','scale=1280:720','-c:v','libx264','-crf','23','-preset','medium','-pix_fmt','yuv420p','-af',filters,'-c:a','aac','-b:a','128k','-ar','48000','-movflags','+faststart',str(out/'MagLava-Trailer-Web.mp4')],check=True)
subprocess.run(['ffmpeg','-v','error','-y','-ss','5','-i',str(source),'-frames:v','1','-vf','scale=1280:720','-q:v','3',str(out/'MagLava-Trailer-Poster.jpg')],check=True)
(out/'audio-mastering.json').write_text(json.dumps({'source':source.name,'web_target_lufs':-16,'web_true_peak_db':-1.5,'analysis':values},indent=2)+'\n')
print('Created web trailer, poster and audio mastering receipt.')
