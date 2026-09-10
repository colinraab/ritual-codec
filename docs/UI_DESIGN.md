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

## Display lettering and panel clearance

Display labels now use embedded Cormorant Garamond, retaining IBM Plex Mono
for numbers. Cormorant keeps the printed, literary character of the reference;
Cinzel would emphasize engraved capitals, while Space Grotesk would move the
interface toward a technical instrument. Cormorant is the current choice.
Section headings have taller dedicated bands and inset text. Control rows have
10 px vertical clearance and additional horizontal panel padding so value boxes
remain clear of the surrounding rules, including at the minimum window size.

## Native control polish

The working panels are now untextured, with paper grain reserved for the frame
and header. Single panel rules and a quieter section motif establish hierarchy.
Cormorant is used across display and parameter lettering, with IBM Plex Mono for
numeric and utility labels. The analyzer begins directly below the meters instead
of leaving a height-dependent gap. Horizontal controls use compact bounds so their
readouts stay near the tracks; rotary drawing areas are capped for consistent scale.
All changes use JUCE's native drawing and the existing cached paper texture.

Knobs use sparse ticks, a clear active arc and pointer, and a small center cap.
Sliders, presets, and bypass share restrained surfaces and border treatments;
preset labels use embedded Cormorant. The functional controls no longer carry
stipple or star ornaments, keeping the printed motif in the surrounding frame.
