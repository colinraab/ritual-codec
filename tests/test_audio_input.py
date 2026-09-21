# SPDX-License-Identifier: AGPL-3.0-or-later
"""CLI input regression; FFmpeg generates compressed fixtures when available."""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import wave


def run(args, ok=True):
    p = subprocess.run([str(a) for a in args], capture_output=True, text=True)
    if (p.returncode == 0) != ok:
        raise AssertionError(f'{args}: {p.returncode}: {p.stderr}')
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('build', type=Path)
    ap.add_argument('--extended', action='store_true')
    args = ap.parse_args()
    suffix = '.exe' if __import__('os').name == 'nt' else ''
    stream = args.build / ('ritual-stream' + suffix)
    classic = args.build / ('ritual-codec' + suffix)
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        # Exactly representable 24-bit edge cases, including sign extension.
        values = [-8388608, -8388607, -1, 0, 1, 4194304, 8388607] * 100
        for channels in (1, 2):
            wav = d / f'input space {channels}.wav'
            with wave.open(str(wav), 'wb') as w:
                w.setparams((channels, 3, 48000, 0, 'NONE', 'not compressed'))
                w.writeframes(b''.join((v & 0xffffff).to_bytes(3, 'little') for v in values))
            result = d / 'out.rit'
            run([stream, 'encode', wav, result, 128, 2])
            original = result.read_bytes()
            # Compare bitstream against a float WAV containing the exact same PCM.
            samples = b''.join(struct.pack('<f', v / 8388608) for v in values)
            fmt = struct.pack('<HHIIHH', 3, channels, 48000, 48000*channels*4, channels*4, 32)
            body = b'WAVEfmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(samples)) + samples
            floatwav = d / 'float.wav'
            floatwav.write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)
            run([stream, 'encode', floatwav, result, 128, 2])
            assert result.read_bytes() == original, '24-bit PCM differs from equivalent float input'
            run([classic, wav, d / 'classic.wav'])
            before = wav.read_bytes()
            run([stream, 'encode', wav, wav], ok=False)
            run([classic, wav, wav], ok=False)
            assert wav.read_bytes() == before
            if args.extended and shutil.which('ffmpeg'):
                for ext, codec in [('flac', 'flac'), ('mp3', 'libmp3lame')]:
                    compressed = d / ('input space.' + ext)
                    run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-c:a', codec, compressed])
                    run([stream, 'encode', compressed, result, 128, 2])
                    if ext == 'flac':
                        assert result.read_bytes() == original, 'FLAC changed lossless samples'
                    run([stream, 'decode', result, d / 'decoded.wav'])
                    run([classic, compressed, d / 'classic.wav'])
        bad = d / 'bad.wav'
        bad.write_bytes(b'RIFF\xff\xff\xff\xffWAVE')
        run([stream, 'encode', bad, d / 'bad.rit'], ok=False)
        run([classic, bad, d / 'bad-out.wav'], ok=False)
    print('Audio input regressions passed')
    if args.extended and not shutil.which('ffmpeg'):
        print('MP3/FLAC fixtures skipped: FFmpeg is not installed')


if __name__ == '__main__':
    main()
