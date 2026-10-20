# Changelog

## 0.2.0 — unreleased experimental preview

- Optional standalone file app with audio preview, seeking, output-device selection,
  settings restoration and background 32-bit float WAV export.
- Matching standalone transport and audio settings, embedded fonts, and a noisy
  waveform app icon. Removed the decorative header subtitle and circle emblem.
- Separate macOS standalone ZIP, versioned archive labels, matching app/icon
  source, and manual CI packaging.
- Hardened missing-output handling, normalized WAV overwrite confirmation and
  export cancellation; regression tests cover malformed input and long renders.

- Both CLIs accept native PCM24 WAV; optional installed libsndfile adds MP3/FLAC
  and extended WAV input. Outputs remain .rit or float32 WAV.

- Native JUCE 9.0.2 VST3 and AU effect with a textured interface and stereo
  input/output spectrum comparison.
- Classic spectral degradation controls, transient protection, latency-aligned
  mix/bypass and fixed 256-sample plugin latency.
- Independent .rit encoder/decoder CLI supporting experimental RIT1 versions 1
  and 2; version 2 can encode sparse attacks in the time domain.
- Opt-in RIT plugin mode with allocation-free streaming and actual packet kbps.
  Classic is the default. Existing version-1 plugin states migrate to Classic.
- RIT mode supports Budget, Competition, Memory, Punch, Crunch, Mix, Output and
  Bypass. Clock, Downsample and Classic presets remain inactive. Named encoder
  controls are available in ritual-stream. Existing RIT sessions can change sound
  because previously ignored knobs are now active; Classic and decoded old files
  are unchanged. The plugin does not record .rit files in the DAW.
- Bitstreams remain lossy at all rates; block-edge distortion, changed peaks and
  stereo-image changes are possible. Format stability is not promised yet.
- Local macOS arm64 Release tests and native AU validation passed. Standalone,
  REAPER AU/VST3 and Ableton VST3 checks are recorded in RELEASE_VALIDATION.md.
  A second Mac, Intel/universal builds and latest Windows/Linux host qualification
  remain open. macOS bundles are ad-hoc signed, without Developer ID or notarization.

No public release has been published from this changelog entry.
