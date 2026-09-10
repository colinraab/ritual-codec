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
