# Bit Famine — experimental music degradation engine, v0.1

A portable C++17 streaming effect exploring short-window spectral quantization,
competing frequency bands, stale spectral shapes, coarse update clocks, amplitude
quantization and sample holding. Punch protects the start of detected attacks.

This is a working DSP prototype and command-line renderer, not a VST/AU plugin
or a finished compressed audio format. Its budget is dimensionless, not kbps.

## Listen first

`audio/00_comparison.wav` plays the same six seconds five times:

| Start | Condition |
|---|---|
| 0:00 | Source |
| 0:06.5 | Subtle |
| 0:13 | Damaged, Punch on |
| 0:19.5 | Same damaged settings, Punch off |
| 0:26 | Extreme, Punch on |

There is 0.5 seconds of silence between segments. Full 24-second renders are
also included, plus a 12-second second excerpt with a damaged render. All use
one common source attenuation of 0.5 (-6.02 dB); outputs were not individually
normalized. Float WAV files preserve overshoot instead of silently clipping it.
No rendered sample or measured 4x oversampled peak exceeded full scale here.

Music: **EDM Detection Mode — Kevin MacLeod (incompetech.com), CC BY 4.0**.
Read `ATTRIBUTION.md` when copying or sharing any audio. The source is an MP3,
so these are degradation tests on already lossy music, not pristine codec
quality evaluations. The two excerpts come from the same track.

## Build and run

Requires a C++17 compiler and Make; the engine has no third-party dependencies.

```sh
make
make test
./build/bitfamine audio/01_source.wav my_render.wav .22 .8 .78 1 .65 8 3
```

Arguments after the two paths: budget, competition, memory, punch, crunch,
clock, downsample, optional transparent (0 or 1). First five range from 0 to 1;
clock ranges 1–64 hops; downsample ranges 1–32 samples. Missing arguments use
the defaults in `Params`. Input supports little-endian RIFF PCM16 or float32,
mono/stereo, 8–192 kHz. WAVE_FORMAT_EXTENSIBLE and RF64 are not implemented.
The output is float32 WAV, same duration as input, with algorithmic delay trimmed
and the end flushed. The CLI loads files into memory; the engine itself streams.

A direct compiler invocation also works:

```sh
c++ -O3 -std=c++17 src/main.cpp -o bitfamine
```

Reproduce measurements and demos with Python 3, numpy, and scipy:

```sh
python3 tools/render_and_measure.py
```

Bundled source excerpts are already attenuated. The script only attenuates
fresh PCM16 inputs, making normal reruns idempotent. It overwrites named demos.
Only the Linux build was executed here; macOS and Windows builds are unverified.

## Embed the engine

```cpp
#include "src/bitfamine.hpp"
bf::Params parameters;
bf::Engine engine(48000, 2, parameters); // construct before audio callbacks
// For interleaved stereo floats; any nonnegative block length is accepted.
engine.process(input, output, frameCount);
// Host latency: bf::Engine::latencySamples() == 256.
```

State is fixed-size and no heap allocation, locks, or I/O occur inside process.
In-place processing is supported. One engine instance belongs to one audio
thread. Do not modify parameters concurrently; validate and apply them between
blocks. Automation is not smoothed in v0.1, so abrupt changes can click. Changes
to rate/channels require a newly prepared engine. Avoid constructing/resetting
one mid-callback. No denormal handling or hardware real-time scheduling is set up.

## What is implemented

- 256-point FFT, 64-sample hop, periodic sine analysis/synthesis windows; overlap
  normalization gives unity reconstruction in transparent mode.
- 16 logarithmic bin groups (some lowest groups are empty at this resolution).
  With 48 kHz input, bin spacing is 187.5 Hz: detailed sub-bass coding is limited.
- Shared energy-weighted allocation of complex-coefficient quantizer precision.
  More Competition lets energetic bands win more precision. Punch can add bits
  beyond that budget: this is an aesthetic allocation model, not a rate cap.
- Per-band shape memory and hop-level shape updates; fresh phase is used at
  analysis and then the complex coefficients are quantized. Band gain updates
  every hop and is currently full precision. High Memory can freeze shapes;
  at Memory=1 with no transient rescue, an initially empty shape stays empty.
- Linked stereo decisions from summed band energies, separate channel spectra.
  A stereo-linked time-domain attack detector blends toward delayed original
  PCM for several milliseconds. Per-band spectral novelty separately increases
  precision and refreshes shape. Stereo linking avoids independent attack gates,
  but distortion can still change stereo width and correlation.
- Global output sample-and-hold (Downsample) without antialias filtering, plus
  amplitude quantization (Crunch). Clock refers to shape update hops, not the
  PCM hold rate. Independent waveform clocks per frequency band are future work.
- Transparent mode bypasses all damage and attack blending but retains latency.

At 48 kHz, the engine's fixed scheduling delay is 256 samples = 5.333 ms.
This excludes host/device buffering. Degradation can reshape attack timing around
that delay; host compensation does not undo that texture. At other sample rates,
the fixed sample count implies a different delay and frequency resolution.

## Transients and limits

Punch=1 can preserve an isolated impulse exactly because it briefly selects the
original delayed attack. That is an intentional pragmatic prototype feature,
not evidence that a low-bitrate stream can preserve it for free. A true encoder
would need to transmit an attack residual/envelope or another reconstruction cue.
There is no sidechain input, dedicated low-band predictor, selectable decay
constant, synthesized attack payload, entropy coder, packed bitstream, packet
loss handling, GUI or plugin wrapper yet. Memory/clock changes are not crossfaded.

The source and listening examples have not been subjectively auditioned by the
assistant. The names describe intended severity, not verified perceptual quality.
Compare Punch on/off and listen especially to simultaneous kick/clap/hat attacks,
tails, bass weight, stereo image and perceived loudness. Listening should guide
the next tuning pass; numerical similarity is not the artistic objective.

## Measured here

See `reports/RESULTS.md` and the raw JSON files. Transparent reconstruction,
block-size invariance, all hop-phase protected impulses, silence, finite output,
parameter validation, processing allocations, and address/undefined-behavior
sanitizers were checked. The timing measurement is on this shared Linux host and
does not guarantee callback deadlines in a DAW.

## Next implementation steps

1. Audition and tune texture versus attack preservation on user-selected music.
2. Expose decay time and split spectral protection from dry attack rescue.
3. Improve low-frequency resolution with a hybrid low-band path, maintaining
   explicit alignment and measuring added group delay.
4. Add smoothed parameter automation and a VST3/AU wrapper.
5. If an actual codec is desired, quantize gains, cap and pack allocation/payload,
   replace the PCM attack rescue with encoded information, and measure true rate.

Sanitizer note: LeakSanitizer was unavailable in this environment; the successful
address/undefined-behavior run disabled leak detection.
