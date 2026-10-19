# Licensing

Ritual Codec source code and documentation are offered under the GNU Affero
General Public License, version 3 or (at your option) any later version
(SPDX: AGPL-3.0-or-later). See LICENSE for the full terms.
Copyright (c) 2026 Colin Raab and contributors.

The project uses JUCE 9.0.2 through its AGPLv3 open-source licensing option.
JUCE is fetched at its pinned upstream revision and is not vendored in this
repository. Its copyright and third-party notices remain applicable; see the
upstream LICENSE.md and JUCE.spdx.json. Plugin packages must retain the relevant
notices and provide corresponding source, including the pinned dependency and
build instructions. Bundled third-party components retain their own licenses.

This project has no commercial JUCE license bundled with it. The fact that the
CLI does not depend on JUCE does not change this project's chosen source license.

The audio examples are third-party music under CC BY 4.0, not AGPL. See
ATTRIBUTION.md; attribution is also embedded in generated comparison WAVs and in
the README demo MP3 (docs/audio/).
No ownership of third-party music or endorsement by its creator is claimed.

IBM Plex Mono is bundled under the SIL Open Font License 1.1. See
`assets/fonts/OFL.txt`; the font retains its separate license.

Cormorant Garamond is bundled under the SIL Open Font License 1.1. See
`assets/fonts/CormorantGaramond-OFL.txt`; it also retains its separate license.

The project's AGPL-3.0-or-later grant covers our own code. The pinned JUCE
dependency declares AGPL-3.0-only as its open-source option, so combined plugin
distributions use AGPL version 3 with the dependency's separate notices retained.
It does not grant a later-version license for JUCE itself.

## Optional CLI audio reader

`RITUAL_CLI_SNDFILE=ON` links the separately installed libsndfile library in the
CLIs only. No libsndfile code or binaries are vendored here. Its upstream header
carries LGPL-2.1-or-later terms; codec dependencies have their own notices and
source obligations. Consult the installed distribution and upstream licensing
before redistributing those libraries. The automatic release packager currently
rejects extended-input CLI archives because it does not bundle this dependency,
its matching source or notices. Default WAV-only builds and plugins are unaffected.
Upstream: https://libsndfile.github.io/libsndfile/
