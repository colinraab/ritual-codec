# Interface direction

The Ritual interface takes inspiration from purple ink printed on dark paper:
engraved controls, fine circuit traces, segmented meters and restrained print
texture. Decorative elements frame the controls; labels and values stay legible.

The artwork is rendered in JUCE rather than embedding a screenshot, so controls
remain live, resizable and connected to host automation. Grain and halftoning
are decorative and must not animate randomly during playback.

The existing parameter IDs, preset values, DSP and state format are retained.
Latency values come from the processor sample rate, not from reference artwork.

## Preview

Build with tests enabled, then render the editor with:

```sh
./build/ritual-codec-plugin-test --screenshot /absolute/path/preview.png
# Optional explicit dimensions for minimum-size layout review:
./build/ritual-codec-plugin-test --screenshot /absolute/path/minimum.png 820 540
```

## Numbers and spectral feedback

Numeric readouts use embedded IBM Plex Mono Regular, including editable slider
values, meters and spectrum labels. They do not depend on installed system fonts.

The sidebar shows latency-aligned input and final-output spectra (after mix,
output gain and bypass). Dim purple is input; bright lavender is output. Both
use stereo mean-square energy, avoiding cancellation from opposite-phase channels.
A 1024-point Hann-window FFT runs only on the UI thread at up to 30 updates/s.
The graph spans 40 Hz to the lesser of 20 kHz and Nyquist, and -84 to +6 dBFS.
The display smooths for readability and decays when audio stops. Its low-frequency
resolution is limited by FFT length (46.875 Hz/bin at 48 kHz).

A fixed-size single-producer/single-consumer queue transfers paired samples.
Queue overflow drops display frames; it does not block audio or change latency.
The display is an overview of spectral change, not a precision measurement tool.

A music-driven preview can be rendered with:

```sh
./build/ritual-codec-plugin-test --screenshot /absolute/path/preview.png 1000 650 /absolute/path/input.wav
```
