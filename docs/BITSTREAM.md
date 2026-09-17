# Ritual bitstream experiment (RIT1, version 1)

Provisional extension: `.rit`. Alternative names considered: `.ritual` (more
explicit), `.r3` (short but suggests a version). The filename is not used to detect
the format; magic and version fields are authoritative. No extension registration
or uniqueness claim is made. This format is experimental and may change.

## First milestone

`ritual-stream` is a separate CLI encoder/decoder. It does not replace the plugin
engine or alter existing presets. The decoder reconstructs audio solely from the
file. No original-audio bypass, hidden PCM side channel or encoder memory is used.

The reference encoder applies an orthonormal DCT-II independently to 128 samples
per channel, keeps the largest-magnitude coefficients, and quantizes their values
to signed 8-bit precision. Each channel transmits its RMS energy and coefficient
scale. The decoder applies the inverse transform and normalizes the reconstructed
block to the transmitted RMS. Thus attack **energy** is explicitly represented;
exact attack shape, peak and phase relationships are not guaranteed. This is not
MP3-compatible and should not be expected to duplicate the existing effect.

Non-overlapping blocks intentionally make this a simple format experiment. Expect
possible block-boundary discontinuities and ringing. Decide by listening whether
to retain those artifacts or adopt a lapped transform before plugin integration.
128 samples is 2.67 ms at 48 kHz, but no realtime streaming API or measured plugin
latency is claimed yet. The current whole-file implementation allocates memory
and uses a direct O(N²) transform; do not call it on an audio callback.

## Usage

Build with RITUAL_BUILD_CLI=ON (JUCE is unnecessary if both plugin formats are OFF):

```sh
cmake -S . -B build-stream -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_CLI=ON -DRITUAL_BUILD_VST3=OFF -DRITUAL_BUILD_AU=OFF
cmake --build build-stream --parallel 4
ctest --test-dir build-stream --output-on-failure
build-stream/ritual-stream encode audio/01_source.wav demo.rit 128
build-stream/ritual-stream decode demo.rit decoded.wav
```

Input: PCM16 or IEEE float32 mono/stereo WAV, 8–192 kHz, finite samples within
+/-4. Output: float32 WAV, original rate/channel count/sample count. The reference
codec caps decoded audio at 64 Mi samples across channels; CLI input files are
capped at 512 MiB. Outputs may exceed unity; no clipping/limiter is applied.
The CLI prints measured total-file kbps. A requested kbps value is rounded down
to whole bytes per packet; file-header overhead and final-frame padding affect
total-file bitrate. Values too small for framing are rejected. Very high rates
can spend bytes on padding once all coefficients fit; v1 is not a competitive
compression format. Quantization remains lossy at every rate.

Audio metadata is not stored in v1 and WAV tags are not copied through this CLI.
Keep ATTRIBUTION.md with demo `.rit` files and decoded WAVs when sharing them.
The package_release.py CLI packager still packages the established renderer only;
experimental ritual-stream release packaging is pending.

## Byte layout

All multi-byte integers are unsigned little-endian. Sample count means frames
per channel. No native structs or floating-point values are serialized.

| File header offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII RIT1 |
| 4 | 2 | Version 1 |
| 6 | 1 | Channels: 1 or 2 |
| 7 | 1 | log2(frame samples): 7 |
| 8 | 4 | Sample rate |
| 12 | 8 | Original sample count |
| 20 | 2 | Fixed packet byte length, including packet header |
| 22 | 2 | Reserved flags: zero |

Exactly ceil(sample count / 128) packets follow. Final input frame is zero-padded;
the decoder trims it to the recorded sample count. Trailing bytes are rejected.

Each packet begins with a 2-byte length equal to the file header's packet length,
then a 4-byte CRC-32 of the payload (reflected polynomial 0xEDB88320, initial and
final XOR 0xFFFFFFFF). Length must be between 6 + 7*channels and 4096 bytes.
The checksum detects corruption; it provides no authentication.

Each channel's payload is concatenated in channel order:

1. Coefficient count (1 byte, 1–128).
2. RMS (2 bytes): integer * 4 / 65535.
3. Maximum coefficient scale (2 bytes): integer * 64 / 65535.
4. Count coefficient records, 15 bits each, packed least-significant-bit first:
   7-bit index followed by an 8-bit biased signed value. Decode q = value - 127;
   value 255 is invalid. Coefficient = q * scale / 127. Duplicate indices are invalid.
5. Zero bits to the next byte boundary.

After all channels, remaining bytes to the fixed packet length are zero.
Unsent coefficients are zero. DCT basis is sqrt((k==0?1:2)/128) *
cos(pi * (n+0.5) * k / 128). The inverse sums coefficient[k] * basis[k][n],
then scales to the transmitted RMS (zero RMS produces zero output). A zero-energy
spectrum with nonzero RMS is rejected. Decoder bounds, padding and CRC are checked.
Malformed packets currently cause an error; musical concealment is future work.

## Validation and initial measurements

Linux Release tests cover mono/stereo at 44.1/48/96 kHz, partial/complete frames,
repeat encode/decode, RMS preservation, silence, impulse positions, input extrema,
truncation, bad versions, CRC errors, a malformed payload with recomputed CRC,
and random invalid files. Address/undefined-behavior sanitizer checks pass with
LeakSanitizer disabled because it cannot operate under this VM's tracing setup.
Strict conversion/shadow compiler checks pass for the new sources.

The existing 24-second CC music excerpt was encoded/decoded at requested
96/128/256/512 kbps. reports/bitstream-v1.json records actual sizes, file hashes,
peaks and onset-energy metrics. All outputs were finite. At 96 kbps the file was
288,024 bytes and median 10 ms onset-energy change was approximately -0.021 dB.
This does not establish perceptual quality, exact transient shape or MP3-like sound.
No subjective listening approval has been recorded.

## Next milestones before replacing the plugin engine

- Listening comparison against the saved pre-bitstream baseline; decide transform/overlap.
- Better temporal attack preservation and stereo handling within explicit bit costs.
- Map the existing artistic controls onto encoded parameters and decoder behavior.
- Fixed-storage streaming encoder/decoder, measured CPU and latency, no callback allocation.
- Decoder fuzzing beyond smoke tests; safe packet-loss/corruption behavior.
- Plugin integration, preset compatibility strategy and Mac/AU tests.
