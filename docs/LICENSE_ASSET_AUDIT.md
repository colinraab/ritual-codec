# License and asset audit

Date: 2026-09-17. Source revision: `19496a18328ac6eb73cb188ea69b407fcc85dab9`.
Scope: current tracked files, configured dependencies, asset metadata, upstream
license evidence, and packaging code. This is a technical license inventory and
release-readiness review, not legal certification or a full-history secrets audit.

## Findings

| Component | Evidence and conclusion |
| --- | --- |
| Project code and documentation | Root LICENSE is AGPLv3; LICENSE-NOTES.md grants AGPL-3.0-or-later for project-owned material. Compatible with using JUCE under AGPLv3. |
| JUCE 9.0.2 | CMake pins `72782788ce18c2d4d760b28e0921d6ffc6431102`. Its LICENSE.md explicitly offers AGPLv3 or commercial licensing. Its SPDX inventory specifies AGPL-3.0-only for modules. Combined open-source plugin distributions therefore use version 3; the project's later-version option does not relicense JUCE. |
| IBM Plex Mono Regular | TTF name-table copyright identifies IBM, and license fields identify OFL 1.1. assets/fonts/OFL.txt contains the copyright, reserved name Plex, and license. Embedding/bundling is permitted subject to retaining the notices; font remains OFL. |
| Cormorant Garamond | TTF name-table copyright identifies the Cormorant Project Authors, and license fields identify OFL 1.1. CormorantGaramond-OFL.txt is present. Added the missing entry in top-level LICENSE-NOTES.md. |
| Music | Kevin MacLeod's current catalog lists EDM Detection Mode, ISRC USUAN1500026. The author's licensing page's renderCC function generates CC BY 4.0 credits. Cached original MP3 matches recorded SHA-256 `746f7b1d4dad9c949a67bc063e0ca79da34452c69cd98f4ab9c16a712e92e951`. ATTRIBUTION.md records source, author, license, excerpts, gain/resampling and processing changes, and no endorsement. |
| Audio distribution | Git tracks audio/README.md, not music binaries. The fetcher supports a checksum-verified original; the renderer embeds credits into WAVs. Include attribution with separately distributed audio, and retain the music's CC license rather than applying AGPL to it. |
| UI images | Tracked PNGs are UI/font previews. The plugin embeds fonts and draws circuit/texture graphics in code; no reference-book-cover raster is bundled in the tracked asset tree. This inventory does not independently establish rights in every historical input or contribution. |

Font source URLs are recorded in assets/fonts/README.md. Their embedded metadata
and included license texts were inspected; upstream byte-for-byte identity of
the fonts was not independently established.

## Dependency and binary-release follow-up

JUCE's SPDX inventory includes permissively licensed dependencies such as
HarfBuzz (MIT variant), SheenBidi (Apache-2.0), PNG/JPEG libraries, FLAC/Ogg/Vorbis
(BSD), VST3 SDK (MIT), and AudioUnitSDK (Apache-2.0). The full inventory also lists
components not necessarily compiled in our configuration (AAX, ASIO, examples,
Android and browser tools). Do not treat every inventory entry as a linked dependency.

The current packager includes both font licenses, project source, the main JUCE
license, an upstream source link and, when found, JUCE.spdx.json. It does **not**
assemble a complete set of dependency copyright/license/NOTICE texts. An SPDX
identifier alone is not a substitute for those texts. Before binary release:

- [ ] Include the applicable third-party license and NOTICE texts, with copyright statements.
- [ ] Provide matching corresponding source, including dependencies, using a verified AGPL-compliant distribution arrangement. Bundling the pinned JUCE source with retained notices is a straightforward option; do not assume an upstream link alone completes this work.
- [ ] Verify the final archives contain those materials for each platform and SDK configuration.

No blocking incompatibility was identified for making the current source available
under the documented open-source licenses. Binary release readiness remains open
until the above distribution tasks are verified. Commercial proprietary licensing
and future contributor agreements were not decided by this audit.

## Sources checked

- [Pinned JUCE license](https://github.com/juce-framework/JUCE/blob/72782788ce18c2d4d760b28e0921d6ffc6431102/LICENSE.md)
- [Pinned JUCE dependency inventory](https://github.com/juce-framework/JUCE/blob/72782788ce18c2d4d760b28e0921d6ffc6431102/JUCE.spdx.json)
- [Official OFL license](https://openfontlicense.org/open-font-license-official-text/)
- [Author's music catalog](https://incompetech.com/music/royalty-free/pieces.json)
- [Author's credit generator](https://incompetech.com/music/royalty-free/licenses/)
- [CC BY 4.0 terms](https://creativecommons.org/licenses/by/4.0/)
