#!/usr/bin/env python3
"""Recreate music and impulse comparisons. Requires built CLIs, scipy and numpy.
Outputs stay under build-stream/comparison; no individual loudness normalization.
"""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path
import numpy as np
from scipy.io import wavfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build-stream')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build-stream/comparison')
    args=parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    cli=args.build_dir.resolve()/'ritual-stream';classic=args.build_dir.resolve()/'ritual-codec'
    sr,source=wavfile.read(ROOT/'audio/01_source.wav')
    if source.dtype==np.int16:source=source.astype(np.float32)/32768*.5
    source=source[:sr*8].astype(np.float32)
    wavfile.write(out/'01-source.wav',sr,source)
    subprocess.run([str(classic),str(out/'01-source.wav'),str(out/'02-classic-damaged.wav'),'.22','.8','.78','1','.65','8','3'],check=True,stdout=subprocess.DEVNULL)
    files=['01-source.wav','02-classic-damaged.wav'];stats={}
    def metrics(y):
        d=np.diff(y.astype(float),axis=0);mask=np.arange(1,len(y))%128==0
        return {'peak':float(np.max(np.abs(y))),'rms_dbfs':float(20*np.log10(np.sqrt(np.mean(y.astype(float)**2))+1e-15)),
                'boundary_difference_rms':float(np.sqrt(np.mean(d[mask]**2))),
                'interior_difference_rms':float(np.sqrt(np.mean(d[~mask]**2))),
                'finite':bool(np.isfinite(y).all())}
    for f in files:
        _,v=wavfile.read(out/f);stats[f]=metrics(v)
    for version in [1,2]:
        for rate in [96,128,256]:
            name=f'rit-v{version}-{rate}';packed=out/(name+'.rit');dest=out/(name+'.wav')
            result=json.loads(subprocess.check_output([str(cli),'encode',str(out/'01-source.wav'),str(packed),str(rate),str(version)],text=True))
            subprocess.run([str(cli),'decode',str(packed),str(dest)],check=True,stdout=subprocess.DEVNULL)
            _,v=wavfile.read(dest);stats[dest.name]={**metrics(v),**result,'stream_sha256':hashlib.sha256(packed.read_bytes()).hexdigest()};files.append(dest.name)
    # One isolated impulse at every possible position in a 128-sample packet.
    impulse=np.zeros(128*1024,dtype=np.float32)
    for i in range(128):impulse[i*1024+256+i]=.8
    wavfile.write(out/'impulses.wav',48000,impulse)
    for version in [1,2]:
        packed=out/f'impulses-v{version}.rit';dest=out/f'impulses-v{version}.wav'
        subprocess.run([str(cli),'encode',str(out/'impulses.wav'),str(packed),'96',str(version)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run([str(cli),'decode',str(packed),str(dest)],check=True,stdout=subprocess.DEVNULL)
        _,v=wavfile.read(dest);ratios=[];pre=[]
        for i in range(128):
            at=i*1024+256+i;ratios.append(abs(float(v[at]))/.8);pre.append(float(np.sum(v[i*1024:at].astype(float)**2))/.64)
        stats[f'impulses-v{version}']={'minimum_peak_ratio':min(ratios),'median_peak_ratio':float(np.median(ratios)),'maximum_pre_onset_energy_ratio':max(pre),'median_pre_onset_energy_ratio':float(np.median(pre))}
    # An ordered comparison in addition to individual time-aligned clips.
    segments=[];order=[];cursor=0
    for f in files:
        _,v=wavfile.read(out/f);order.append({'file':f,'start_seconds':cursor});segments.extend([v,np.zeros((sr//2,2),np.float32)]);cursor+=len(v)/sr+.5
    combined=np.concatenate(segments[:-1]);wavfile.write(out/'00-comparison.wav',sr,combined)
    report={'sample_rate':sr,'music_seconds':len(source)/sr,'normalization':'One common source attenuation; no per-output normalization. Float32 WAV.', 'order':order,'measurements':stats,
            'limitations':'Boundary differences are diagnostics, not listening scores. Impulses do not represent all drum attacks. No subjective listening approval implied.'}
    (out/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
    text='Ritual bitstream listening comparison\n\nEight-second clips, identical source and gain; no output loudness matching.\n00-comparison.wav plays each clip in the following order, separated by 0.5 seconds:\n'
    for entry in order:text+=f"{entry['start_seconds']:5.1f}s  {entry['file']}\n"
    text+='\nListen for kick/snare impact, fuzzy block-boundary buzz, stereo image and decay texture.\nRIT v2 adds explicitly coded sparse transients. Most dense music blocks still use the v1 spectral representation.\nThe Classic file uses the existing Damaged preset; its normalized Budget is not equivalent to any RIT bitrate.\n\n'
    text+='This comparison uses the first 8 seconds of the repository excerpt (original track 60–68 seconds).\nThe .rit files are included for independent decoding.\n\n'
    text+=(ROOT/'ATTRIBUTION.md').read_text().replace('The comparison\nconcatenates five processed/source excerpts with silence between them.', 'This comparison concatenates eight source/processed excerpts with silence between them; encoding settings are listed in measurements.json.')
    (out/'README.txt').write_text(text)
    # Keep attribution beside all audio in the archive, including the experimental files.
    with zipfile.ZipFile(out/'ritual-bitstream-listening.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name in ['README.txt','measurements.json','00-comparison.wav',*files,*[f'rit-v{v}-{r}.rit' for v in [1,2] for r in [96,128,256]]]:z.write(out/name,name)
    print(out/'ritual-bitstream-listening.zip')

if __name__=='__main__':main()
