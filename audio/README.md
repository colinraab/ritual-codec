# Listening examples

Audio binaries are kept out of git history. Recreate the licensed excerpts:

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
