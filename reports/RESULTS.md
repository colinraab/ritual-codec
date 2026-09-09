# Validation results — v0.1

Engine delay: **256 samples / 5.333 ms at 48 kHz**, excluding audio hardware and host buffering.

| Render | Speed vs real time | Median onset energy change, dB | Median onset normalized squared error | 4x oversampled peak |
|---|---:|---:|---:|---:|
| 02_subtle.wav | 48.5x | -0.001 | 0.00006 | 0.501 |
| 03_damaged_punch.wav | 50.6x | -0.021 | 0.00431 | 0.504 |
| 04_damaged_no_punch.wav | 50.3x | -0.138 | 0.08297 | 0.523 |
| 05_extreme_punch.wav | 55.6x | -0.070 | 0.01814 | 0.497 |
| 07_second_damaged_punch.wav | 46.4x | nan | nan | 0.475 |

Onset measurements use the 40 strongest qualifying positive log-RMS rises in the
first excerpt, separated by at least 100 ms, and compare the following 10 ms.
These are heuristic candidates, not manually annotated drum hits. The second
excerpt has no onset annotation here (NaN in table). No output was independently
level-normalized. Punch on/off changes both allocation protection and PCM attack
blending, so this comparison does not isolate one mechanism.

Transparent maximum absolute error: 1.78814e-07.
Minimum protected isolated-impulse peak ratio over all 64 hop offsets:
1. Allocations inside processing:
0.

Other passed gates: mono/stereo at 44.1/48/96 kHz, exact equality between 1- and
127-frame host blocks, silence at extreme settings, finite/bounded randomized
outputs, invalid parameter rejection, AddressSanitizer and UndefinedBehaviorSanitizer.

The optimized test observed a maximum 64-frame stereo process call of
908.48 microseconds in 1,000 calls. This is a
small shared-host timing sample, not a hard real-time upper bound. The audio
render timings exclude file I/O and include the DSP loop. g++ 13.3, -O3, C++17,
x86_64 Linux. Sanitizers were run separately from speed measurement.

Limitations: one already-MP3-encoded track, two excerpts; no live DAW/device test,
subjective listening panel, bitrate measurement, or compression-ratio claim.
The full-Punch impulse result benefits from direct access to the original PCM.

LeakSanitizer could not inspect processes in this environment. Address/undefined
behavior checks passed with leak detection disabled; no leak-check claim is made.
