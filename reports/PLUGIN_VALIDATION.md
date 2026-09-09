# Plugin validation — 0.2.0 development snapshot

Validated on Linux x86-64 with GCC 13.3, CMake 3.28.3 and JUCE 9.0.2
(commit `72782788ce18c2d4d760b28e0921d6ffc6431102`).

## Passed locally

- CLI-only configure/build and engine tests, without JUCE.
- Release VST3 bundle build and JUCE module-info generation.
- CTest: engine and plugin integration suites, 2/2 passed.
- Mono/stereo processing at 44.1, 48 and 96 kHz.
- Exactly 256 samples of aligned dry audio, parameter bypass and host bypass.
- Processing across variable host block sizes, including single-sample calls.
- Core equivalence, automation consistency, state roundtrip and invalid-state rejection.
- Reset, unsupported-rate fallback and nonfinite-input handling.
- Editor construction and layout at 820×540 and 1200×800; nine controls and preset updates.
- Rendered interface preview inspected for visible controls and readable values.

The fixed algorithm delay is 5.333 ms at 48 kHz, excluding host/device buffers.
The older RESULTS.md and JSON measurements describe the v0.1 engine and music
renders; they are not measurements of native plugin CPU use or DAW behavior.

## Still to validate

- Native DAW loading on macOS and Windows.
- DAW automation, session restoration, live performance and listening evaluation.
- Developer ID signing/notarization and any release publication.

GitHub Actions is configured to produce separate CLI, VST3 and AU downloads.
Repository access was restored after reconnecting GitHub. Platform CI results
are tracked in GitHub Actions. These are development artifacts, not a tested
public release.

## CI log review — run 34404462322

All three platform jobs passed, including the engine/processor suites and
Apple's AU validation (`AU VALIDATION SUCCEEDED`). The AU download matched
GitHub's SHA-256 digest and both outer and inner ZIP CRC checks passed. The
archive used standard Deflate compression and preserved the AU executable bit;
Archive Utility's reported failure was not reproduced on this Linux reviewer.
The identical outer/inner filenames are removed by direct artifact upload.

Follow-up changes add native macOS ZIP creation/extraction and signature checks,
require the JUCE license in plugin packages, update deprecated Actions runtimes,
and clean up editor conversions, shadowed names and misleading indentation.
Remaining DSP warnings mainly concern signed indexing into fixed arrays and
exact float comparisons; these warrant separate cleanup and are not evidence
of a failing audio test. JUCE's configure-time Make jobserver warning affects
build parallelism, not the produced plugin.
