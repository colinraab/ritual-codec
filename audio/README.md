# Listening examples

Full-length source excerpts and renders are kept out of git history. Recreate the
licensed excerpts:

```sh
python3 tools/fetch_demo_audio.py
python3 -m pip install -r requirements-dev.txt
make
python3 tools/render_and_measure.py
```

Requires ffmpeg on PATH. The download script verifies the original MP3 SHA-256
against reports/source_provenance.json, extracts two clips, and preserves the
source attribution. render_and_measure.py makes comparison presets and embeds
credits in every WAV. See ../ATTRIBUTION.md for the CC BY 4.0 license.

The 32-second comparison uses the same six seconds at 0:00 (source), 0:06.5
(subtle), 0:13 (damaged with Punch), 0:19.5 (damaged without Punch), and 0:26
(extreme with Punch). Playback files are 48 kHz stereo float WAV.

## README demo clip

The short MP3 linked from the README (`docs/audio/ritual-codec-demo.mp3`) is the
only audio committed to the repository. Regenerate it after fetching the source
excerpts and building the CLIs with CMake:

```sh
python3 tools/fetch_demo_audio.py
cmake -S . -B build-cli -DCMAKE_BUILD_TYPE=Release -DRITUAL_BUILD_VST3=OFF
cmake --build build-cli
python3 tools/make_demo_mp3.py --build-dir build-cli
```

It needs only ffmpeg (no numpy/scipy) and embeds the CC BY 4.0 credit in the MP3's
comment tag.
