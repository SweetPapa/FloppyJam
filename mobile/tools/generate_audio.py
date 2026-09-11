#!/usr/bin/env python3
"""Small, deterministic native-playback cues; no external packages."""
import math
import pathlib
import struct
import wave
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'mobile/assets/Audio'
out.mkdir(parents=True,exist_ok=True)
for name,notes,duration in [('swing',[400,760],.12),('attach',[660,990],.16),('checkpoint',[523,659,784],.42),('death',[180,90],.32),('complete',[523,659,784,1046],.65)]:
    rate=22050
    samples=[]
    for i in range(int(rate*duration)):
        t=i/rate; part=t/duration*len(notes); freq=notes[min(len(notes)-1,int(part))]
        envelope=min(1,t/.008)*max(0,1-t/duration)**1.4
        samples.append(int(8500*envelope*(math.sin(2*math.pi*freq*t)+.15*math.sin(4*math.pi*freq*t))))
    with wave.open(str(out/(name+'.wav')),'wb') as f:
        f.setparams((1,2,rate,0,'NONE','not compressed')); f.writeframes(struct.pack('<'+'h'*len(samples),*samples))

# Authored music is imported separately with import_soundtrack.py.
