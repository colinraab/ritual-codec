# Building Ritual Codec

Ritual Codec has the following CMake switches:

| Option | Default | Purpose |
| --- | --- | --- |
| `RITUAL_CLI_SNDFILE` | `OFF` | Optional installed libsndfile backend for MP3/FLAC and extended WAV input; CLIs only. |
| `RITUAL_BUILD_CLI` | `ON` | Builds the `ritual-codec` WAV renderer and `ritual-stream` encoder/decoder. |
| `RITUAL_BUILD_VST3` | `ON` | Builds the VST3 plug-in. |
| `RITUAL_BUILD_AU` | `OFF` | Builds the Audio Unit on Apple platforms. |
| `RITUAL_BUILD_TESTS` | `ON` | Builds engine, bitstream and hardening tests; adds JUCE processor tests with plugins and packaging tests when Python is found. |

The command-line renderer and engine test have no third-party dependencies. A
CLI-only configure does not download or configure JUCE:

```sh
cmake -S . -B build/cli -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_CLI=ON \
  -DRITUAL_BUILD_VST3=OFF \
  -DRITUAL_BUILD_AU=OFF \
  -DRITUAL_BUILD_TESTS=ON
cmake --build build/cli --parallel
ctest --test-dir build/cli --output-on-failure
```

The executables are `ritual-codec` and `ritual-stream`. The Classic renderer
accepts the Makefile arguments described in the project README; see
[BITSTREAM.md](BITSTREAM.md) for `.rit` encoding and decoding.

## Linux dependencies

On Ubuntu 24.04:

```sh
sudo apt-get install cmake ninja-build pkg-config libasound2-dev \
  libfontconfig1-dev libfreetype6-dev libglu1-mesa-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxi-dev libxrandr-dev libxrender-dev xvfb xauth
```

Run GUI-dependent tests on a headless machine with `xvfb-run -a ctest ...`.
Windows builds need Visual Studio's C++ desktop tools; macOS needs Xcode and
its command-line tools. The plugin artifacts are development builds;
Developer ID signing and notarization are not configured here.

## JUCE dependency

Plugin builds use JUCE **9.0.2**, pinned to commit
`72782788ce18c2d4d760b28e0921d6ffc6431102`. If
`RITUAL_JUCE_SOURCE_DIR` is omitted, CMake retrieves that exact commit with
`FetchContent`. The configure step reads JUCE's version header and stops if the
source is not exactly 9.0.2.

For an offline build, point the override at a checked-out JUCE 9.0.2 tree:

```sh
cmake -S . -B build/plugins -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_JUCE_SOURCE_DIR=/path/to/JUCE \
  -DRITUAL_BUILD_CLI=ON \
  -DRITUAL_BUILD_VST3=ON \
  -DRITUAL_BUILD_AU=OFF \
  -DRITUAL_BUILD_TESTS=ON
cmake --build build/plugins --config Release --parallel
ctest --test-dir build/plugins -C Release --output-on-failure
```

The plug-in target is `RitualCodec`. It produces a VST3 named `Ritual Codec`
with manufacturer code `Crrb`, plug-in code `RtCd`, and bundle identifier
`net.colinraab.ritualcodec`. The target is an effect with mono and stereo bus
layouts and no MIDI input or output. JUCE's browser, cURL support, and VST2
replacement path are disabled. `COPY_PLUGIN_AFTER_BUILD` is also disabled, so
building never writes to a system or user plug-in directory.

On Apple platforms, enable AU explicitly:

```sh
cmake -S . -B build/apple -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DRITUAL_BUILD_CLI=ON \
  -DRITUAL_BUILD_VST3=ON \
  -DRITUAL_BUILD_AU=ON
cmake --build build/apple --config Release --parallel
```

`RITUAL_BUILD_AU=ON` is rejected on non-Apple hosts. AU validation requires the built component in the standard user plugin directory.
On a development machine, copy it there without overwriting another installation:

```sh
au_path="$(find build/apple -type d -name '*.component' -print -quit)"
test -n "$au_path"
plutil -lint "$au_path/Contents/Info.plist"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
component_path="$HOME/Library/Audio/Plug-Ins/Components/$(basename "$au_path")"
test ! -e "$component_path" || exit 1
ditto "$au_path" "$component_path"
auval -v aufx RtCd Crrb
```

`auval` is supplied by Apple's developer tools. The generated AU is not Developer ID signed or notarized;
those are release-process steps outside this build.

## Build directories and packaging

JUCE places plug-in bundles below `RitualCodec_artefacts`; multi-config
generators add a `Release` (or other configuration) directory. The build also
writes `RitualCodecBuildInfo.txt` with the resolved JUCE version and source
description.

The release helper creates one ZIP per artifact type and copies the license,
audio attribution, source provenance JSON, and this build guide into each ZIP:

```sh
python3 tools/package_release.py \
  --build-dir build/apple \
  --output-dir dist \
  --platform macos-universal
```

Pass `--kind vst3` (or `cli`, `au`, repeatable) to package only selected outputs.

The resulting files are named `ritual-codec-macos-universal-cli.zip`,
`ritual-codec-macos-universal-vst3.zip`, and
`ritual-codec-macos-universal-au.zip` when those formats were built. The helper
fails if a requested artifact is missing and never copies a plug-in into a host
directory.

## Continuous integration

`.github/workflows/build.yml` runs only when started with `workflow_dispatch`;
pushes and pull requests do not start builds. Its selectable matrix
covers Windows VST3 plus CLI, macOS universal VST3/AU plus CLI, and Linux VST3
plus CLI. Each job uploads separate CLI, VST3, and (on macOS) AU ZIP artifacts.
No job publishes a release. The macOS job validates the AU bundle with
`plutil` and stages it at the host's expected
`~/Library/Audio/Plug-Ins/Components` path before running `auval`.

### Download and extraction

Artifacts are uploaded as the original ZIP file (`archive: false`), without
GitHub adding another ZIP with the same name. Unzip once to reach `AU/`,
`VST3/`, or `bin/`, plus the licenses and matching project source.

macOS packages use Apple's `ditto` ZIP writer. Packaging extracts them again
with `ditto`, checks file bytes, permissions and symlinks, and verifies plugin
signatures. The CI bundles are ad-hoc signed, not Developer ID signed or notarized.

Older runs used nested ZIPs. If Archive Utility cannot expand one, extract the
inner ZIP into a fresh folder using Terminal (adjust the downloaded filename):

```sh
ditto -x -k "$HOME/Downloads/ritual-codec-macos-universal-au 2.zip" "$HOME/Downloads/Ritual-AU"
```

Copy `AU/Ritual Codec.component` into
`~/Library/Audio/Plug-Ins/Components`, then restart your host to rescan.


## Local AU iteration without Actions minutes

Actions runs only via **Run workflow**, with a platform selector. Pushes and pull
requests do not start builds. Manual hosted runs still consume minutes on a private
repository; leave them unused when conserving the allowance.

On your Mac, install Apple's command-line developer tools (`xcode-select --install`)
and CMake/Ninja (`brew install cmake ninja`, assuming Homebrew is installed).
Clone this repository, or pull updates in your existing checkout, then run:

```sh
cmake -S . -B build/local-au -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_AU=ON -DRITUAL_BUILD_VST3=OFF \
  -DRITUAL_BUILD_CLI=OFF -DRITUAL_BUILD_TESTS=ON
cmake --build build/local-au --parallel 4
ctest --test-dir build/local-au --output-on-failure
```

The first configure fetches the pinned JUCE source. This builds for the native
architecture of your terminal; on Apple Silicon use a native terminal rather than
Rosetta. Subsequent builds reuse compiled files. No Apple Developer subscription
is needed for a local ad-hoc signed build.

Quit Ableton before installing. The following replaces the user-level Ritual Codec
installation with this build:

```sh
ritual_au="build/local-au/RitualCodec_artefacts/Release/AU/Ritual Codec.component"
codesign --force --sign - --timestamp=none "$ritual_au"
codesign --verify --strict "$ritual_au"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
ditto "$ritual_au" "$HOME/Library/Audio/Plug-Ins/Components/Ritual Codec.component"
killall -u "$(id -un)" AudioComponentRegistrar || true
auval -v aufx RtCd Crrb
```

If discovery is not immediate, wait a few seconds and rerun `auval`. Open Ableton,
enable Audio Units in its Plug-Ins settings, and rescan if needed. For later
iterations, `git pull`, rerun the build and tests, then repeat the installation
commands with Ableton closed. Reconfigure when build options change.

## Release qualification

The CLI archive includes both `ritual-codec` and `ritual-stream`. See
[PACKAGING.md](PACKAGING.md) for source/notice bundling and archive verification,
and [RELEASE_VALIDATION.md](RELEASE_VALIDATION.md) for the platform matrix and
local DAW checklist. No public release or notarization is implied by a build.

## MP3, FLAC and 24-bit WAV input

Both CLIs accept conventional RIFF PCM16/24 and float32 WAV without extra libraries.
For MP3, FLAC, WAVE_FORMAT_EXTENSIBLE and other libsndfile-supported input, install
libsndfile and explicitly enable the optional backend. On macOS with Homebrew:

```sh
brew install libsndfile
cmake -S . -B build-stream -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix libsndfile)" \
  -DRITUAL_BUILD_CLI=ON -DRITUAL_BUILD_VST3=OFF -DRITUAL_BUILD_AU=OFF \
  -DRITUAL_CLI_SNDFILE=ON
cmake --build build-stream --parallel 4
ctest --test-dir build-stream --output-on-failure
./build-stream/ritual-stream encode "input.mp3" output.rit 128 2
./build-stream/ritual-stream encode "input.flac" output.rit 128 2
./build-stream/ritual-stream decode output.rit decoded.wav
./build-stream/ritual-codec "input.flac" classic.wav .22 .8 .78 1 .65 8 3
```

Linux needs the libsndfile development package (typically `libsndfile1-dev`);
Windows needs an installed matching-architecture libsndfile development library,
headers and runtime DLLs. Use `CMAKE_PREFIX_PATH` for its installation prefix.
MP3 requires libsndfile 1.1+ built with MPEG decoding; format availability follows
the installed library. Missing libraries fail configure when the option is ON.
The dependency-free Make build remains WAV-only.

Input is mono/stereo; the extended backend accepts 8–192 kHz without downmixing,
resampling or loudness normalization. File size is capped at 512 MiB and decoded
samples at 64 Mi across channels (about 11.65 minutes of stereo at 48 kHz).
RIT rejects nonfinite input and samples outside +/-4. MP3 decoding adds no recovery
of detail already lost in that source. Tags/artwork are not carried into `.rit`.
Output remains `.rit` for encoding and float32 WAV for decoding/Classic rendering;
this option adds input formats, not MP3/FLAC output.

The AU/VST3 do not link this optional dependency. Extended-input CLIs need their
installed runtime libraries; the release packager deliberately rejects their CLI
archives until portable library/source/notice bundling is implemented. Plugin-only
packages and default WAV-only CLI packages continue to work. See LICENSE-NOTES.md.

CTest always checks 24-bit sign/scale conversion, both CLIs, malformed input and
same-file rejection. In an extended build it also checks MP3/FLAC if FFmpeg is
available to generate fixtures; FFmpeg is a test dependency, not a CLI runtime dependency.

## If the AU still looks like an older build

The new mode selector reads **CODEC: CLASSIC** in the top-right header; click it
for **CODEC: RIT**. The rest of the layout remains familiar. Building does not
install the AU. Pull the latest source, rebuild, quit the host, and repeat the
sign/copy/registrar/AU-validation steps under Local AU iteration above. Then reopen
the host and create a fresh instance. Check for a second older copy in the system
`/Library/Audio/Plug-Ins/Components` folder if the user-level update is not loaded.
Do not delete another installation without confirming which copy the host uses.

A WAV-only executable given MP3/FLAC input reports the build option and suggests
converting to WAV. Enabling the option without installed headers/library fails
CMake configuration. Removing libsndfile after linking an extended executable can
instead cause an OS loader error before the CLI starts; reinstall the dependency
or rebuild with the option OFF. The application cannot intercept loader errors.
