# Ritual Codec

**Unreleased experimental preview (0.2.0).** See the [changelog](CHANGELOG.md)
and [release validation matrix](docs/RELEASE_VALIDATION.md) for tested scope.

An experimental, low-latency music degradation effect. Frequency bands compete
for spectral precision, old timbres linger, and coarse digital clocks turn tails
into texture. **Punch** protects attacks for rhythmic material.

The repository contains a dependency-free C++ DSP engine, WAV command-line tool,
and a JUCE 9.0.2 plugin project targeting **VST3** and **Audio Unit (AU)**. It is
currently an effect prototype; its Budget remains an artistic control, not kbps.
A separate experimental `ritual-stream` CLI now encodes and decodes actual `.rit`
files with an explicit byte budget. See [the bitstream experiment](docs/BITSTREAM.md)
for commands, the versioned format, tests and limitations. The header's CODEC button enables this experimental path in the plugin. Classic
remains the default; RIT has a different sound and currently supports Budget, Mix,
Output and Bypass. Both modes retain 256 samples of latency.

![Ritual Codec plugin interface](docs/plugin-preview.png)

## Build only what you need

Requires CMake 3.22+ and a C++17 compiler. Plugin builds fetch JUCE at a pinned
revision; CLI-only builds do not download or link JUCE.

```sh
# CLI and engine tests only
cmake -S . -B build-cli -DRITUAL_BUILD_VST3=OFF -DRITUAL_BUILD_AU=OFF
cmake --build build-cli --config Release
ctest --test-dir build-cli -C Release --output-on-failure

# VST3 plugin (and CLI)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRITUAL_BUILD_VST3=ON
cmake --build build --config Release

# Add Audio Unit on macOS
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_VST3=ON -DRITUAL_BUILD_AU=ON
cmake --build build-mac --config Release
```

See [build and installation instructions](docs/BUILDING.md) for dependencies,
platform details, artifact paths, and CI download steps. Installing the CLI and
plugin are independent; plugins include the engine internally. No automatic
copy into system plugin directories is performed during ordinary builds.

The sidebar overlays the original and processed spectra to show spectral change.
Numeric controls use bundled IBM Plex Mono.

## Controls

These descriptions apply to Classic mode. In RIT mode, Budget selects packet
size; only Budget, Mix, Output and Bypass are active. The header CODEC button
switches modes and RIT displays its actual packet bitrate.

| Control | Behavior |
|---|---|
| Budget | More/less precision available to the spectral quantizers |
| Competition | How strongly energetic bands take precision from other bands |
| Memory | Persistence of previous normalized spectral shapes |
| Punch | Extra attack precision and a brief blend toward the original attack |
| Crunch | Coarse time-domain amplitude quantization |
| Clock | Number of 64-sample hops between ordinary shape refreshes |
| Downsample | Time-domain sample-and-hold interval, deliberately without an antialias filter |
| Mix | Latency-aligned dry/wet balance (plugin) |
| Output | Output gain in dB (plugin) |
| Bypass | Latency-aligned unprocessed signal (plugin) |

Subtle, Damaged, No Punch, and Extreme presets provide starting points. At the
extremes, artifacts can change stereo width, peak levels, and tonal balance.
Continuous automation is smoothed by the plugin; the integer clocks remain
intentional stepped changes. The prototype makes no promise of transparent
reconstruction at maximum Budget. Transparent mode in the CLI is a diagnostic
pass-through using the transform path.

## Command-line rendering

The original Make build remains available and uses no JUCE:

```sh
make
./build/ritual-codec input.wav output.wav .22 .8 .78 1 .65 8 3
```

Positional controls are Budget, Competition, Memory, Punch, Crunch, Clock,
Downsample, then optional Transparent (0 or 1). The first five are 0–1,
Clock is 1–64, Downsample is 1–32. Input: PCM16 or IEEE float32 RIFF WAV,
mono/stereo; output: float32 WAV with engine delay trimmed and tail flushed.
WAVE_FORMAT_EXTENSIBLE and RF64 are not supported by this minimal CLI reader.

## Listen and reproduce the prototype measurements

```sh
python3 tools/fetch_demo_audio.py
python3 -m pip install -r requirements-dev.txt
make
python3 tools/render_and_measure.py
```

The fetch step requires ffmpeg. It downloads and verifies Kevin MacLeod's
**EDM Detection Mode** under CC BY 4.0 and extracts two short sections. The render
script makes five comparison conditions with one common source attenuation.
See [audio instructions](audio/README.md) and [attribution](ATTRIBUTION.md).
Audio and generated binaries are not stored in git history.

The original v0.1 measurements are in [reports/RESULTS.md](reports/RESULTS.md).
They characterize the DSP engine on one already-MP3-encoded track, not native
plugin performance or a comprehensive listening evaluation. The engine reports
**256 samples of delay: 5.333 ms at 48 kHz**, excluding host/device buffering.
The plugin reports this latency to the host and aligns dry audio and bypass.

## Architecture

- `src/bitfamine.hpp`: fixed-storage DSP core; historical internal filename/name.
- `src/main.cpp`: portable WAV reader/writer and offline command-line harness.
- `plugin/`: JUCE processor, parameter/state handling, editor and presets.
- `tests/`: engine and plugin integration checks.
- `.github/workflows/`: platform builds and separately packaged artifacts.
- `tools/`: licensed demo retrieval, rendering and packaging.

The engine uses a 256-point FFT with 64-sample hops, linked stereo allocation,
16 logarithmic bin groups, normalized magnitude memory, complex coefficient
quantization and time-domain degradation. No memory allocation occurs in its
sample-processing method. The plugin wraps it with bounded buffers and
latency-aligned mixing. See [prototype design notes](docs/PROTOTYPE.md) for the
original algorithm and measurement details; those notes describe v0.1.

## Status and next steps

Linux VST3 and CLI builds and the engine/processor/editor tests pass locally.
Linux, Windows and macOS CI builds and tests pass, including Apple AU validation.
See [validation details](reports/PLUGIN_VALIDATION.md).

The artistic goal is intentional spectral breakup with preserved rhythmic impact.
The interface and sound are experimental. Listen before relying on presets in
productions; future algorithm versions will need explicit preset compatibility.

Possible next steps: independent band clocks, a dedicated low-band path, separate
attack-rescue and decay controls, and artistic controls for the opt-in bitstream
after listening validation. The Classic plugin Punch implementation
uses original PCM during attacks, so its preservation is not free compression.

## License

Source and documentation: **AGPL-3.0-or-later**, using JUCE's AGPL open-source
option. See [LICENSE](LICENSE) and [licensing notes](LICENSE-NOTES.md).
The CC BY 4.0 music retains its separate attribution and license. This project
is not affiliated with JUCE or the music's creator.

## Contributing and security

See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
Please keep private audio and security reports out of public issues.
