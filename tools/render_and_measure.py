#!/usr/bin/env python3
"""Run from the project root after make. Requires numpy/scipy. Sources are bundled."""
import json, subprocess, pathlib
import numpy as np
from scipy.io import wavfile
from scipy.signal import find_peaks, resample_poly
root=pathlib.Path(__file__).resolve().parents[1]
A=root/'audio'; report={}
def read(p):
 sr,x=wavfile.read(p);return sr,x.astype(np.float64)/(32768 if x.dtype==np.int16 else 1)
def save(p,sr,x):wavfile.write(p,sr,x.astype(np.float32))
# One common input attenuation, applied only once to fresh PCM16 sources.
for name in ['01_source.wav','06_source_second_excerpt.wav']:
 sr,x=wavfile.read(A/name)
 if x.dtype==np.int16:save(A/name,sr,x.astype(np.float64)/32768*.5)
settings={
 '02_subtle.wav':[.65,.4,.35,.9,.15,2,1],
 '03_damaged_punch.wav':[.22,.8,.78,1,.65,8,3],
 '04_damaged_no_punch.wav':[.22,.8,.78,0,.65,8,3],
 '05_extreme_punch.wav':[.05,1,.94,1,.9,16,7],
 '07_second_damaged_punch.wav':[.22,.8,.78,1,.65,8,3]}
for name,p in settings.items():
 src='06_source_second_excerpt.wav' if name.startswith('07') else '01_source.wav'
 result=subprocess.check_output([str(root/'build/ritual-codec'),str(A/src),str(A/name),*map(str,p)],text=True)
 report[name]={'parameters':p,'runtime':json.loads(result)}
sr,x=read(A/'01_source.wav')
# Onsets: 1 ms stereo RMS, positive log-energy differences; >= 100 ms separation.
def envelope(x):
 n=len(x)//48;return np.sqrt(np.mean(x[:n*48].reshape(n,48,2)**2,axis=(1,2)))
e=envelope(x);nov=np.maximum(0,np.diff(np.log(e+1e-5),prepend=np.log(e[0]+1e-5)))
peaks,_=find_peaks(nov,distance=100,prominence=.18)
peaks=peaks[(peaks>20)&(peaks<len(e)-50)]
# Highest 40 rises, sorted in time. Heuristic candidates, not ground-truth drum annotations.
peaks=np.sort(peaks[np.argsort(nov[peaks])[-40:]])
for name in settings:
 source='06_source_second_excerpt.wav' if name.startswith('07') else '01_source.wav'
 sr,ref=read(A/source);_,y=read(A/name)
 m={'rms_dbfs':float(20*np.log10(np.sqrt(np.mean(y*y))+1e-12)),
 'sample_peak':float(np.max(np.abs(y))),
 'oversampled_4x_peak':float(np.max(np.abs(resample_poly(y,4,1,axis=0)))),
 'rms_error_relative_db':float(10*np.log10(np.mean((y-ref)**2)/(np.mean(ref**2)+1e-15))),
 'finite':bool(np.isfinite(y).all())}
 if not name.startswith('07'):
  ratios=[];errs=[]
  for t in peaks*48:
   a=ref[t:t+480];b=y[t:t+480]
   ratios.append(10*np.log10((np.sum(b*b)+1e-15)/(np.sum(a*a)+1e-15)))
   errs.append(np.sum((b-a)**2)/(np.sum(a*a)+1e-15))
  m.update(onset_candidates=len(peaks),attack_10ms_energy_db_median=float(np.median(ratios)),attack_10ms_energy_db_p10=float(np.percentile(ratios,10)),attack_10ms_normalized_error_median=float(np.median(errs)))
 report[name]['metrics']=m
# Comparison: same first 6 seconds, five conditions separated by 0.5 s silence.
order=['01_source.wav','02_subtle.wav','03_damaged_punch.wav','04_damaged_no_punch.wav','05_extreme_punch.wav']
segments=[]
for n in order:
 _,y=read(A/n);segments.extend([y[:6*sr],np.zeros((sr//2,2))])
comparison=np.concatenate(segments[:-1]);save(A/'00_comparison.wav',sr,comparison)
report['comparison']={'order':order,'starts_seconds':[0,6.5,13,19.5,26], 'segment_seconds':6,'normalization':'Same -6.02 dB source attenuation; no individual output normalization.'}
(root/'reports/measurements.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
# Embed attribution in every standalone WAV as a standard RIFF INFO chunk.
import struct
for path in A.glob('*.wav'):
 data=path.read_bytes()
 if b'Licensed CC BY 4.0' in data:continue
 tags={'IART':'Kevin MacLeod','INAM':'EDM Detection Mode - Ritual Codec test excerpt',
 'ICMT':'Licensed CC BY 4.0 https://creativecommons.org/licenses/by/4.0/ ; source https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1500026 ; excerpted, attenuated, resampled; processed where indicated. No endorsement.'}
 info=b'INFO'
 for k,v in tags.items():
  value=v.encode()+b'\0';info+=k.encode()+struct.pack('<I',len(value))+value+(b'\0' if len(value)%2 else b'')
 data+=b'LIST'+struct.pack('<I',len(info))+info
 data=data[:4]+struct.pack('<I',len(data)-8)+data[8:];path.write_bytes(data)
