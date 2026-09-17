#!/usr/bin/env python3
"""Capture repeat-render hashes, audio metrics and timing for the pre-bitstream DSP.

Run from a clean checkout after building build/ritual-codec and fetching demo
audio. Requires requirements-dev.txt. Regenerates audio examples and measurements.
Pass the source revision explicitly so a source archive works without Git.
"""
import argparse
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((ROOT / 'audio').glob('*.wav'))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'reports/baseline.json')
    args = parser.parse_args()
    runs = []
    snapshots = []
    for _ in range(2):
        subprocess.run([sys.executable, str(ROOT / 'tools/render_and_measure.py')],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        runs.append(json.loads((ROOT / 'reports/measurements.json').read_text()))
        snapshots.append(hashes())
    if snapshots[0] != snapshots[1]:
        raise RuntimeError('Repeat renders differed; baseline is not deterministic')
    import numpy
    import scipy
    result = {
        'revision': args.revision,
        'platform': platform.platform(),
        'machine': platform.machine(),
        'python': platform.python_version(),
        'numpy': numpy.__version__, 'scipy': scipy.__version__,
        'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
        'ffmpeg': subprocess.check_output(['ffmpeg', '-version'], text=True).splitlines()[0],
        'repeat_render_bytes_identical': True,
        'audio_sha256': snapshots[0],
        'runs': runs,
        'limitations': [
            'Hashes are a same-toolchain reference, not a cross-platform bit-exact promise.',
            'Timing is offline engine processing elapsed time in a shared VM, not DAW CPU or a realtime deadline guarantee.',
            'Transient metrics use heuristic onset candidates, not annotated drum hits.',
            'CLI trims the 256-sample delay; plugin/engine tests separately check streaming latency.',
            'No AU, macOS, Windows or DAW-host validation is implied.'
        ]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Saved', args.output)


if __name__ == '__main__':
    main()
