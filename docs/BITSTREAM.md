# Ritual bitstream experiment (RIT1, versions 1 and 2)

Provisional extension: `.rit`. Alternative names considered: `.ritual` (more
explicit), `.r3` (short but suggests a version). The filename is not used to detect
the format; magic and version fields are authoritative. No extension registration
or uniqueness claim is made. This format is experimental and may change.

## First milestone

`ritual-stream` is a separate CLI encoder/decoder. The plugin also offers an opt-in CODEC: RIT mode; Classic remains the default
and existing presets keep their original sound. The decoder reconstructs audio solely from the
file. No original-audio bypass, hidden PCM side channel or encoder memory is used.

The reference encoder applies an orthonormal DCT-II independently to 128 samples
per channel, keeps the largest-magnitude coefficients, and quantizes their values
to signed 8-bit precision. Each channel transmits its RMS energy and coefficient
scale. The decoder applies the inverse transform and normalizes the reconstructed
block to the transmitted RMS. Thus attack **energy** is explicitly represented;
exact attack shape, peak and phase relationships are not guaranteed. This is not
MP3-compatible and should not be expected to duplicate the existing effect.

Non-overlapping blocks intentionally make this a simple format experiment. Expect
block-boundary discontinuities and ringing. These remain experimental artifacts,
not a solved fidelity problem. Version 2 can represent sparse attacks directly
in the time domain at the same packet budget. It does not eliminate ringing on
dense music or guarantee preservation of every drum transient.

The fixed-storage streaming API delays audio by 128 samples (2.67 ms at 48 kHz).
The plugin adds 128 samples of alignment delay, retaining its existing fixed
256-sample latency in both modes. Whole-file encode/decode functions allocate;
only the prepared Streaming process API is intended for the audio callback.
The transform remains direct O(N²), so CPU cost needs testing on target machines.

## Usage

Build with RITUAL_BUILD_CLI=ON (JUCE is unnecessary if both plugin formats are OFF):

```sh
cmake -S . -B build-stream -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRITUAL_BUILD_CLI=ON -DRITUAL_BUILD_VST3=OFF -DRITUAL_BUILD_AU=OFF
cmake --build build-stream --parallel 4
ctest --test-dir build-stream --output-on-failure
build-stream/ritual-stream encode audio/01_source.wav demo.rit 128 2
build-stream/ritual-stream decode demo.rit decoded.wav
```

Input: PCM16/24 or IEEE float32 mono/stereo WAV, 8–192 kHz, finite samples within
+/-4. Output: float32 WAV, original rate/channel count/sample count. The reference
codec caps decoded audio at 64 Mi samples across channels; CLI input files are
capped at 512 MiB. Outputs may exceed unity; no clipping/limiter is applied.
Optional MP3/FLAC and extended WAV input is available with `RITUAL_CLI_SNDFILE=ON`;
see [BUILDING.md](BUILDING.md#mp3-flac-and-24-bit-wav-input).
The CLI prints measured total-file kbps. A requested kbps value is rounded down
to whole bytes per packet; file-header overhead and final-frame padding affect
total-file bitrate. Values too small for framing are rejected. Very high rates
can spend bytes on padding once all coefficients fit; v1 is not a competitive
compression format. Quantization remains lossy at every rate.

Audio metadata is not stored in v1 and WAV tags are not copied through this CLI.
Keep ATTRIBUTION.md with demo `.rit` files and decoded WAVs when sharing them.
The CLI packager includes both the Classic renderer and experimental ritual-stream.
See PACKAGING.md for archive validation and corresponding-source contents.

## Byte layout

All multi-byte integers are unsigned little-endian. Sample count means frames
per channel. No native structs or floating-point values are serialized.

| File header offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII RIT1 |
| 4 | 2 | Version 1 or 2 |
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

1. Version 1: coefficient count (1 byte, 1–128). Version 2: see mode byte below.
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

## Version 2 transient representation

Version 2 retains the RIT1 magic, framing, checksum, record widths and energy
metadata. The high bit of each channel's count byte selects sparse time-domain
samples. The lower seven bits give the count; in spectral mode a zero count
means 128 coefficients. Sparse mode requires a count from 1 to 127.

For sparse blocks, record indices identify sample positions instead of DCT bins.
Unsent samples are zero. Values use the same scale and quantizer; reconstructed
samples are then normalized to the transmitted RMS without an inverse transform.
The encoder selects this mode only when the largest samples that fit the packet
capture at least 95% of the nonzero block energy, using no more than 32
samples. This restriction avoids introducing holes into dense high-rate blocks. This is an encoder choice, not
an additional decoder side channel. Each stereo channel chooses independently.

The CLI defaults to version 2; pass a final `1` to produce legacy streams. The
whole-file C++ encoder retains version 1 as its default for source compatibility;
pass `2` explicitly. The decoder supports both versions. Neither is a stable
public interoperability commitment yet.

## Plugin experiment

Click CODEC: CLASSIC in the header to enable CODEC: RIT. Budget controls packet
size, and the header reports packet kbps at the current sample rate. Mix, Output
and Bypass remain active. Competition, Memory, Punch, Crunch, Clock, Downsample
and Classic presets are disabled in this mode because they are not yet part of
the encoded algorithm. In particular, sparse transient protection is automatic;
Punch does not secretly mix original attacks into the decoded stream.

Old version-1 plugin states migrate with RIT mode off. New states retain the
mode. Switching warms up the new path and crossfades while maintaining latency.
The in-plugin codec encodes and decodes packets in memory; it does not save a
`.rit` file from the DAW. Use the CLI for files.

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

The streaming regression suite covers exact offline/streaming agreement across
8/44.1/48/96/192 kHz and mono/stereo, arbitrary callback sizes, in-place processing,
reset, a full spectral packet, malformed sparse counts, allocation guards, and
bitrate changes that preserve queued audio. Plugin tests cover independent file
reconstruction after the switch settles, 256-sample latency, host-block-size
independence, repeated automation/switching and legacy state migration.

See `reports/bitstream-streaming-validation.json`. A shared Linux VM run of the
192 kbps stereo streaming core at 48 kHz averaged 35.5 microseconds per 128-sample
callback (maximum observed 799 microseconds; 2667 microseconds of audio per block).
This excludes the parallel Classic engine and UI, and is not a realtime deadline
guarantee. Mac/AU and DAW validation remain outstanding.

## Reproduce the listening comparison

With both CLIs built and Python numpy/scipy installed:

```sh
python3 tools/compare_bitstreams.py --build-dir build-stream
```

The build directory receives an attributed listening ZIP with source, Classic
Damaged, and v1/v2 at 96, 128 and 256 kbps. It contains individual eight-second
WAVs, an ordered comparison WAV, encoded files and measurements. All outputs use
the same source attenuation, without per-output loudness matching.

`reports/bitstream-comparison.json` records the measured comparison. Across all
128 isolated impulse positions at 48 kHz/96 kbps mono, v1 preserves approximately
41.3% of the peak at its original position. V2 preserves approximately 100.04%
(the small excess comes from RMS quantization), with zero pre-onset energy in
that test. This is not a claim about every real drum transient. Dense music still
has pronounced block boundaries; listening approval remains outstanding.

## Next milestones before replacing the plugin engine

- Owner listening approval of the comparison; decide whether to retain block discontinuities.
- Better temporal attack preservation and stereo handling within explicit bit costs.
- Map the existing artistic controls onto encoded parameters and decoder behavior.
- Broader CPU measurements and small-buffer stress tests on target machines.
- Decoder fuzzing beyond smoke tests; safe packet-loss/corruption behavior.
- Mac/AU and multi-DAW validation of the opt-in plugin mode.
