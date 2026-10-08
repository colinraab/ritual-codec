#!/usr/bin/env python3
"""Render the short before/after listening demo linked from the README.

Run after tools/fetch_demo_audio.py and a CLI build. Needs ffmpeg on PATH.
The result is a single MP3 with six-second sections separated by half a second
of silence:

  source, Subtle, Damaged, Damaged without Punch, Extreme
  and, when ritual-stream is available, the RIT bitstream at 64 kbps.

The comparison is level-matched to tools/render_and_measure.py: the source is
attenuated by 6.02 dB once and no section is normalized individually.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SECTION_SECONDS = 6
GAP_SECONDS = 0.5
# Budget, Competition, Memory, Punch, Crunch, Clock, Downsample (see README).
PRESETS = [
    ('Subtle', [.65, .4, .35, .9, .15, 2, 1]),
    ('Damaged', [.22, .8, .78, 1, .65, 8, 3]),
    ('Damaged without Punch', [.22, .8, .78, 0, .65, 8, 3]),
    ('Extreme', [.05, 1, .94, 1, .9, 16, 7]),
]
CREDIT = ('Music: "EDM Detection Mode" by Kevin MacLeod (incompetech.com), CC BY 4.0, '
          'https://creativecommons.org/licenses/by/4.0/ - excerpted, attenuated and '
          'processed with Ritual Codec. See ATTRIBUTION.md.')


def find_tool(name, candidates):
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--source', type=Path, default=ROOT / 'audio/01_source.wav')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build',
                        help='Directory containing ritual-codec (and optionally ritual-stream)')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/audio/ritual-codec-demo.mp3')
    args = parser.parse_args()

    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        parser.error('ffmpeg must be installed and available on PATH')
    if not args.source.is_file():
        parser.error(f'{args.source} not found; run tools/fetch_demo_audio.py first')
    build = args.build_dir
    codec = find_tool('ritual-codec', [build / 'ritual-codec', build / 'Release/ritual-codec.exe',
                                       build / 'ritual-codec.exe'])
    if codec is None:
        parser.error(f'ritual-codec not found in {build}; run make or a CMake CLI build')
    stream = find_tool('ritual-stream', [build / 'ritual-stream', build / 'Release/ritual-stream.exe',
                                         build / 'ritual-stream.exe'])

    with tempfile.TemporaryDirectory(prefix='ritual-demo-mp3-') as temp:
        temp = Path(temp)
        source = temp / 'source.wav'
        # Same one-time -6.02 dB attenuation as render_and_measure.py; PCM16 keeps the
        # WAV in the plain format both CLIs read without optional dependencies.
        subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(args.source),
                        '-t', str(SECTION_SECONDS + 1), '-af', 'volume=0.5',
                        '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le', str(source)], check=True)
        sections = [('Source', source)]
        for index, (label, values) in enumerate(PRESETS):
            rendered = temp / f'classic_{index}.wav'
            subprocess.run([str(codec), str(source), str(rendered), *map(str, values)],
                           check=True, stdout=subprocess.DEVNULL)
            sections.append((label, rendered))
        if stream is not None:
            encoded = temp / 'rit.rit'
            decoded = temp / 'rit.wav'
            subprocess.run([str(stream), 'encode', str(source), str(encoded), '64', '2'],
                           check=True, stdout=subprocess.DEVNULL)
            subprocess.run([str(stream), 'decode', str(encoded), str(decoded)],
                           check=True, stdout=subprocess.DEVNULL)
            sections.append(('RIT bitstream, 64 kbps', decoded))
        else:
            print('ritual-stream not found; omitting the RIT section.')

        inputs = []
        chains = []
        for index, (_, path) in enumerate(sections):
            inputs += ['-i', str(path)]
            pad = GAP_SECONDS if index < len(sections) - 1 else 0
            chains.append(f'[{index}:a]atrim=0:{SECTION_SECONDS},asetpts=PTS-STARTPTS,'
                          f'apad=pad_dur={pad}[s{index}]')
        joined = ''.join(f'[s{index}]' for index in range(len(sections)))
        graph = ';'.join(chains) + f';{joined}concat=n={len(sections)}:v=0:a=1[out]'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([ffmpeg, '-v', 'error', '-y', *inputs, '-filter_complex', graph,
                        '-map', '[out]', '-c:a', 'libmp3lame', '-b:a', '192k',
                        '-metadata', 'title=Ritual Codec demo',
                        '-metadata', 'artist=Kevin MacLeod (processed with Ritual Codec)',
                        '-metadata', f'comment={CREDIT}',
                        str(args.output)], check=True)

    step = SECTION_SECONDS + GAP_SECONDS
    print(f'Wrote {args.output}')
    for index, (label, _) in enumerate(sections):
        start = index * step
        print(f'  {int(start // 60)}:{start % 60:04.1f}  {label}')
    print(CREDIT)


if __name__ == '__main__':
    main()
