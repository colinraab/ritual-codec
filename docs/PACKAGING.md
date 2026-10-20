# Packaging an experimental release

Packaging does not publish a release. Build and test from a clean source tree
first; do not modify project or JUCE sources between building and packaging.
Python 3.9+ is required. The packager does not rebuild binaries or certify that an
old binary matches newly edited source files.

```sh
python3 tools/package_release.py --build-dir build --output-dir dist \
  --platform linux-x64 --kind cli --kind vst3
```

Use `macos-arm64`, `macos-universal`, or `windows-x64` only when they describe the
actual build. The label does not select or verify a CPU architecture. `--kind au`
and `--kind standalone` are available for a macOS build. Omit `--kind` to package CMake-enabled targets.
Use a dedicated build directory for the configuration being released: multiple
Debug/Release copies are rejected rather than guessed.

Each CLI archive includes **both** `ritual-codec` and `ritual-stream`. A missing
executable is an error. Plugin bundles are packaged separately by format. The standalone archive contains
`STANDALONE/Ritual Codec.app`. Pass `--version 0.2.0` to include
a version label in filenames and provenance. The label does not change the app version. Every
archive contains documentation, font/project notices, `SHA256SUMS.txt`, build
provenance and a project source ZIP drawn from explicit source directories and
file lists. Local logs, audio renders, caches and root-level scratch files are
excluded. Review source directories before packaging; the allowlist is not a
secret scanner.

Plugin and standalone source ZIPs additionally include the actual configured JUCE tree under
`ritual-codec/JUCE/`, preserving upstream sources, embedded copyright statements,
license/NOTICE files and SPDX inventory. Git metadata and generated build trees
are excluded. The top-level JUCE license and inventory are also copied beside
the archive documentation. A local source override is identified as such; it is
not silently claimed to be an unmodified upstream commit. Absolute local paths
are omitted from the distributed provenance.

After extracting the inner source ZIP, configure from `ritual-codec/` with:

```sh
cmake -S . -B build -DRITUAL_JUCE_SOURCE_DIR=./JUCE
```

Use the format/platform options in BUILDING.md. CLI-only sources do not include
JUCE and must be configured with both plugin formats OFF. Toolchains and system
libraries are not bundled; see BUILDING.md for prerequisites. Review any extra
SDKs or local dependency modifications before redistribution. Inclusion of source
and notices is a technical packaging measure, not independent legal certification.

Verification checks archive CRCs, paths, hashes, file types and Unix permission
metadata; extracted regular files are compared against the staging tree. On
macOS, native `ditto` extraction additionally checks symlinks, permissions and
completed plugin and app signatures with `codesign --verify --strict`. Ad-hoc signing
is not notarization. The Windows verification checks bytes and paths; Windows
permissions are not represented as Unix execute bits.

On macOS, replaced archives and temporary staging/extraction folders are moved
to Trash. Packaging requires permission to use the current user’s Trash.

Run `python3 tests/test_packaging.py` to exercise isolated fixture builds. These
checks do not replace installing the final archive in a DAW. See
RELEASE_VALIDATION.md for platform qualification and release gates.
