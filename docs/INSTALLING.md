# Installing Ritual Codec

Prebuilt downloads (when available) are on the
[Releases page](https://github.com/colinraab/ritual-codec/releases). Each
platform has separate archives:

| Archive | Contains |
| --- | --- |
| `ritual-codec-<platform>-vst3.zip` | `VST3/Ritual Codec.vst3` plug-in |
| `ritual-codec-<platform>-au.zip` | `AU/Ritual Codec.component` (macOS only) |
| `ritual-codec-<platform>-standalone.zip` | `STANDALONE/Ritual Codec.app` (macOS) |
| `ritual-codec-<platform>-cli.zip` | `bin/ritual-codec` and `bin/ritual-stream` command-line tools |

Versioned filenames insert the version before the platform label, for example
`ritual-codec-0.2.0-preview.1-macos-arm64-standalone.zip`. The local preview is
Apple Silicon only and requires macOS 26.6 or later; other architectures and older
macOS releases have not been qualified. See [release validation](RELEASE_VALIDATION.md).

Every archive also includes licences, third-party notices, build provenance,
SHA-256 checksums and the matching source code.

## Standalone app

Extract the standalone ZIP and copy the whole `STANDALONE/Ritual Codec.app`
bundle to Applications. Open audio or drop a file onto the app, choose an output
device, and press Play. Export WAV opens a destination chooser each time and
writes the entire source as processed 32-bit float WAV audio.

## Plug-ins

Close your DAW and back up any existing Ritual Codec bundle before replacing it.
Rescan plug-ins after copying.

### Plug-in folders

| OS | VST3 | Audio Unit |
| --- | --- | --- |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| Windows | `C:\Program Files\Common Files\VST3\` | — |
| Linux | `~/.vst3/` | — |

Copy the whole `.vst3` or `.component` bundle (it is a folder), not its contents.

## macOS: ad-hoc signed builds

Mac bundles are ad-hoc signed, without an Apple Developer ID or notarization.
Downloaded Gatekeeper behavior has not been tested on a second Mac. If macOS
blocks a downloaded build you trust, clear the download quarantine flag for that
bundle only, using the path where you installed it:

```sh
xattr -dr com.apple.quarantine "/Applications/Ritual Codec.app"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Ritual Codec.vst3"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Ritual Codec.component"
```

Do the same for the CLI tools if you use them. Do not disable Gatekeeper
system-wide. For Audio Units, `auval -v aufx RtCd Crrb` checks that macOS can
load the plug-in; if a host still shows an older build, see
[BUILDING.md](BUILDING.md#if-the-au-still-looks-like-an-older-build).

## Windows: unsigned builds

Windows SmartScreen may warn about an unrecognized publisher when you run the
CLI tools. The VST3 is loaded by your DAW and normally needs no extra step.

## Usage notes

- Latency is a fixed 256 samples (5.3 ms at 48 kHz), reported to the host.
- This is a deliberate degradation effect with no output limiter. Start with
  modest monitoring levels; extreme settings can change peak level and stereo width.
- Presets and sound may change between experimental versions.

To build from source instead, see [BUILDING.md](BUILDING.md).
