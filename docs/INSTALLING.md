# Installing Ritual Codec

Prebuilt downloads (when available) are on the
[Releases page](https://github.com/colinraab/ritual-codec/releases). Each
platform has separate archives:

| Archive | Contains |
| --- | --- |
| `ritual-codec-<platform>-vst3.zip` | `VST3/Ritual Codec.vst3` plug-in |
| `ritual-codec-<platform>-au.zip` | `AU/Ritual Codec.component` (macOS only) |
| `ritual-codec-<platform>-cli.zip` | `bin/ritual-codec` and `bin/ritual-stream` command-line tools |

Every archive also includes licences, third-party notices, build provenance,
SHA-256 checksums and the matching source code.

Close your DAW before installing, then rescan plug-ins after copying.

## Plug-in folders

| OS | VST3 | Audio Unit |
| --- | --- | --- |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| Windows | `C:\Program Files\Common Files\VST3\` | — |
| Linux | `~/.vst3/` | — |

Copy the whole `.vst3` or `.component` bundle (it is a folder), not its contents.

## macOS: unsigned builds

The builds are not signed with an Apple Developer ID or notarized, so macOS
Gatekeeper will block them after download. Once you have copied a bundle into
the folder above, clear the download quarantine flag for that bundle only:

```sh
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
