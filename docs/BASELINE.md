# Pre-bitstream baseline

Captured 2026-09-17 against source revision
`19496a18328ac6eb73cb188ea69b407fcc85dab9`.

## Build provenance and limits

All tracked project files were verified against GitHub's Git blob hashes and
copied into a separate source directory. The build used a fresh CMake/Ninja
Release directory, GCC 13.3.0, Linux x86-64, CLI + VST3 + tests, and the documented
RITUAL_JUCE_SOURCE_DIR override pointing to the existing JUCE 9.0.2 dependency.
No prior project objects or build cache were reused. The VM's installed Linux
development dependencies were reused. This verifies a clean project build with
an offline dependency override, not fresh network dependency retrieval, a clean
OS installation, or a macOS/AU build.

Build succeeded. CTest passed both suites (engine and plugin processor).
Compiler warnings were confined to external ALSA headers: zero-size arrays and
shadowing declarations. No project-source warning appeared in the build log.

## Results

- Fixed streaming latency: 256 samples, 5.333 ms at 48 kHz.
- Transparent reconstruction maximum absolute error: 1.78814e-7.
- Protected impulse minimum peak ratio: 1.0 across all 64 hop offsets tested.
- Processing heap allocations observed by the existing engine test: zero.
- Largest observed 64-sample call: 316.934 microseconds. This is a finite-run
  observation in a shared VM, not a worst-case execution-time guarantee.
- Five music renders plus sources/comparison were identical byte-for-byte on two
  successive runs. SHA-256 hashes and full metrics are in reports/baseline.json.
- First-run music processing took approximately 1.66–2.14% of audio duration.
  These are offline engine timings, not DAW CPU readings or realtime guarantees.
- Median 10 ms onset-energy difference for Damaged with Punch: approximately
  -0.021 dB; without Punch: -0.138 dB. Onsets are heuristic candidates on one track,
  not annotated drum hits or a general perceptual-quality claim.

The CLI trims processing delay; the existing streaming engine and processor tests
check delayed reconstruction/bypass separately. reports/baseline-engine.json
records engine test measurements. Source audio was recreated from the cached
original MP3 after verifying its recorded SHA-256, using the documented --source
option. No arbitrary test music was substituted.

## Reproduce

To reproduce the old engine after future codec changes, check out the source
revision above in a separate worktree and copy tools/capture_baseline.py from
the commit containing this report into it. The helper does not change DSP code.
Install the build dependencies listed in BUILDING.md, ffmpeg, and requirements-dev.txt.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_CLI=ON -DRITUAL_BUILD_VST3=ON \
  -DRITUAL_BUILD_AU=OFF -DRITUAL_BUILD_TESTS=ON \
  -DRITUAL_JUCE_SOURCE_DIR=/path/to/JUCE-9.0.2
cmake --build build --parallel 4
xvfb-run -a ctest --test-dir build --output-on-failure
python3 tools/fetch_demo_audio.py
python3 tools/capture_baseline.py --revision 19496a18328ac6eb73cb188ea69b407fcc85dab9
```

The helper regenerates audio examples and reports/measurements.json, performs two
renders, checks repeatability and writes reports/baseline.json. Use a separate
checkout to preserve this recorded baseline. Source WAVs are attenuated only once
by the existing renderer. Music attribution is embedded in generated WAVs.

Binary WAVs remain outside Git and can be regenerated from the pinned source,
original MP3 checksum, parameters and tool versions. Hash equality across different
compilers, CPUs or ffmpeg versions is not promised; compare numerical metrics and
listen when changing toolchains. Fresh Mac/AU and actual DAW validation remain open.
