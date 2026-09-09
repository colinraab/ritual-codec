#!/usr/bin/env python3
"""Fetch and verify the CC BY 4.0 music used for the reproducible listening test."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://incompetech.com/music/royalty-free/mp3-royaltyfree/EDM%20Detection%20Mode.mp3'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='Use an already downloaded original MP3')
    parser.add_argument('--force', action='store_true', help='Replace existing source excerpt WAVs')
    args = parser.parse_args()
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        parser.error('ffmpeg must be installed and available on PATH')
    names = ['01_source.wav', '06_source_second_excerpt.wav']
    output = ROOT / 'audio'
    output.mkdir(exist_ok=True)
    if not args.force and any((output / name).exists() for name in names):
        parser.error('Source excerpts already exist; pass --force to regenerate')
    expected = json.loads((ROOT / 'reports/source_provenance.json').read_text())['original_mp3_sha256']
    with tempfile.TemporaryDirectory(prefix='ritual-demo-') as temp:
        source = args.source or Path(temp) / 'source.mp3'
        if args.source is None:
            print('Downloading EDM Detection Mode by Kevin MacLeod (CC BY 4.0)...')
            request = urllib.request.Request(URL, headers={'User-Agent': 'RitualCodecDemo/0.2'})
            with urllib.request.urlopen(request, timeout=60) as response, source.open('wb') as stream:
                shutil.copyfileobj(response, stream)
        actual = hashlib.sha256(source.read_bytes()).hexdigest()
        if actual != expected:
            parser.error('Original MP3 checksum differs from the validated source; inspect it before updating provenance')
        for name, start, duration in zip(names, [60, 180], [24, 12]):
            staged = Path(temp) / name
            subprocess.run([ffmpeg, '-v', 'error', '-y', '-ss', str(start), '-t', str(duration),
                            '-i', str(source), '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le',
                            str(staged)], check=True)
        for name in names:
            shutil.copyfile(Path(temp) / name, output / name)
    print('Created two source excerpts. Run tools/render_and_measure.py after building the CLI.')
    print('Credit: EDM Detection Mode — Kevin MacLeod (incompetech.com). CC BY 4.0.')
    print('https://creativecommons.org/licenses/by/4.0/ — see ATTRIBUTION.md for changes and provenance.')


if __name__ == '__main__':
    main()
