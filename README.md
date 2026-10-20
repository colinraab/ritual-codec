# Ritual Codec

An experimental, low-latency audio effect that degrades music on purpose:
frequency bands compete for a scarce precision budget, old timbres linger in
memory, and coarse digital clocks turn tails into texture, while **Punch** keeps
drum attacks hitting hard.

Available as a **VST3** and **Audio Unit** plug-in, a **standalone file player
and WAV exporter**, and two command-line tools. Free and open source under the AGPL.

![Current Ritual Codec plug-in in Classic mode, with the Damaged preset and live spectrum display](docs/plugin-preview.png)

> **Experimental preview (0.2.0).** The sound, presets and `.rit` file format
> may change between versions. See the [changelog](CHANGELOG.md) and
> [what has been tested](docs/RELEASE_VALIDATION.md).

## Listen

[**▶ Before/after demo (MP3, 38 s)**](docs/audio/ritual-codec-demo.mp3)

| Time | Section |
| --- | --- |
| 0:00 | Original |
| 0:06 | Subtle |
| 0:13 | Damaged |
| 0:19 | Damaged without Punch |
| 0:26 | Extreme |
| 0:32 | RIT bitstream mode, 64 kbps |

Music: "EDM Detection Mode" by Kevin MacLeod ([incompetech.com](https://incompetech.com)),
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); excerpted,
attenuated and processed. See [ATTRIBUTION.md](ATTRIBUTION.md).

## Two modes

The **CODEC** button in the editor header switches between:

- **Classic** (default): a spectral degradation effect. Budget is an artistic
  control, not a real bitrate.
- **RIT**: audio really is encoded into small packets at a fixed byte budget
  and decoded again, and the plug-in shows the actual packet bitrate. It sounds
  different from Classic, with more blocky, codec-like artifacts. See
  [the bitstream notes](docs/BITSTREAM.md).

Both modes have a fixed latency of 256 samples (5.3 ms at 48 kHz), which is
reported to the host so Mix and Bypass stay aligned.

## Standalone app

![Ritual Codec standalone app playing audio, with matching transport controls and WAV export](docs/standalone-preview.png)

The standalone app opens or accepts a dropped mono/stereo audio file (8–192 kHz).
Use Play/Pause, Stop and the seek bar to preview it through the same controls as
the plug-in. Audio output selects the playback device; no microphone is needed.
Effect settings, the last file and the output device are restored on launch.

Export WAV opens a save dialog each time and renders the entire source with a
snapshot of the current settings. The 32-bit floating-point WAV keeps the source
sample rate, channel count and duration, with the effect latency removed. Preview
is resampled to the output device's rate, so use a matching device rate when
comparing it with the export. There is no limiter or normalization. Existing
exports are moved to Trash after a replacement finishes; source files are protected.

WAV, AIFF, FLAC, Ogg and MP3 input use JUCE's available readers. Multichannel files
are rejected. Export runs in the background; closing the app cancels an active
render. This first app exports WAV; `.rit` files still use `ritual-stream`.
See [building the standalone app](docs/BUILDING.md#standalone-file-app).

## Controls

| Control | Classic | RIT |
| --- | --- | --- |
| Budget | Precision available to the spectral quantizers | Packet size (bitrate) |
| Competition | How strongly loud bands steal precision from quieter ones | Favors strong components over quiet ones |
| Memory | How long previous spectral shapes persist | Retains spectral shapes in the encoded audio |
| Punch | Extra attack precision plus a brief blend toward the original attack | Preserves encoded attacks; no original PCM blend |
| Crunch | Coarse time-domain amplitude quantization | Coarsens encoded coefficients |
| Clock | Number of 64-sample hops between spectral-shape refreshes | — |
| Downsample | Sample-and-hold interval, deliberately without anti-aliasing | — |
| Mix | Latency-aligned dry/wet balance | ✓ |
| Output | Output gain in dB | ✓ |
| Bypass | Latency-aligned unprocessed signal | ✓ |

The **Subtle**, **Damaged**, **No Punch** and **Extreme** presets (Classic mode)
are good starting points. There is no output limiter: extreme settings can
change peak level, stereo width and tonal balance, so start with moderate
monitoring levels.

## Install

When prebuilt downloads are available, get the archive for your platform from
the [Releases page](https://github.com/colinraab/ritual-codec/releases) and follow
[the install guide](docs/INSTALLING.md). Version 0.2.0 is currently unreleased.
The locally tested Mac preview is Apple Silicon only and requires macOS 26.6
or later. It is ad-hoc signed, without Developer ID signing or notarization.

### Build from source

You need CMake 3.22+ and a C++17 compiler. Plug-in builds download JUCE 9.0.2
at a pinned commit. CLI-only builds have no dependencies at all.

```sh
# VST3 plug-in, CLIs and tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# Add the Audio Unit and standalone app on macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_AU=ON -DRITUAL_BUILD_STANDALONE=ON
cmake --build build --config Release

# CLI tools only (no JUCE)
cmake -S . -B build-cli -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_VST3=OFF -DRITUAL_BUILD_AU=OFF -DRITUAL_BUILD_STANDALONE=OFF
cmake --build build-cli --config Release
```

Builds are not copied into your system plug-in folders automatically. See
[BUILDING.md](docs/BUILDING.md) for Linux packages, where built files end up,
offline JUCE builds and optional MP3/FLAC input.

## Command-line tools

**`ritual-codec`** renders the Classic effect offline (`make` also builds it):

```sh
ritual-codec input.wav output.wav [budget competition memory punch crunch clock downsample]
ritual-codec input.wav damaged.wav .22 .8 .78 1 .65 8 3
```

Budget, Competition, Memory, Punch and Crunch take values from 0 to 1, Clock
from 1 to 64 and Downsample from 1 to 32.

**`ritual-stream`** encodes real `.rit` files and decodes them back to audio:

```sh
ritual-stream encode input.wav song.rit 96 --punch 0.8 --crunch 0.2
ritual-stream decode song.rit decoded.wav
```

Both tools read 16/24-bit PCM or 32-bit float WAV (mono or stereo) and write
32-bit float WAV with the processing delay removed. Building with
`-DRITUAL_CLI_SNDFILE=ON` adds MP3 and FLAC input through libsndfile.

## How it works

The Classic engine uses a 256-point FFT with 64-sample hops, 16 logarithmic
bands that share a precision budget (linked across stereo channels),
normalized magnitude memory, complex coefficient quantization and time-domain
degradation. It runs with fixed storage and never allocates in the audio
callback. The RIT path uses 128-sample DCT blocks with an explicit byte budget
per packet, and a time-domain packet type for sharp transients. See
[PROTOTYPE.md](docs/PROTOTYPE.md) for the original engine design and
measurements, and [BITSTREAM.md](docs/BITSTREAM.md) for the `.rit` format.

| Path | Contents |
| --- | --- |
| `src/classic_engine.hpp` | Classic DSP engine (header-only, no dependencies) |
| `src/rit_stream.hpp` | `.rit` encoder, decoder and streaming implementation |
| `src/main.cpp`, `src/rit_main.cpp` | The two command-line tools |
| `plugin/` | JUCE processor, editor, presets and spectrum display |
| `standalone/` | File player, background WAV renderer and app icon |
| `tests/` | Engine, bitstream, fuzz-style decoder, plug-in and packaging tests |
| `tools/` | Demo audio, measurement and release packaging scripts |
| `reports/` | Measurement results referenced by the docs |

## Status

The current macOS arm64 candidate passes nine automated suites, AU validation,
standalone playback/export checks, REAPER AU/VST3 testing and a narrower Ableton
VST3 check. Linux has local build/test evidence; Windows has an earlier development
build. Second-machine installation, broader compatibility and final listening
approval remain open. See [release validation](docs/RELEASE_VALIDATION.md) for the
exact scope and the [documentation index](docs/README.md) for the other guides.

Ideas for later: independent band clocks, a dedicated low-band path, separate
attack and decay controls, and further tuning of the RIT bitstream.

Bug reports and ideas are welcome; please read [CONTRIBUTING.md](CONTRIBUTING.md)
first, and report security issues as described in [SECURITY.md](SECURITY.md).

## License

Ritual Codec is licensed under the
[GNU Affero General Public License v3.0 or later](LICENSE). Plug-in builds use
JUCE under its AGPLv3 option. The bundled IBM Plex Mono and Cormorant Garamond
fonts are under the SIL Open Font License, and the demo music is CC BY 4.0. See
[LICENSE-NOTES.md](LICENSE-NOTES.md) and [ATTRIBUTION.md](ATTRIBUTION.md) for details.
