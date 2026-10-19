# Public release checklist

Target: an experimental public release of Ritual Codec.
Unchecked items are not yet verified for release, even where implementation exists.
Track evidence or relevant commits beside completed items. Making the repository public is a separate owner decision.

## Before making the repository public

- [x] Complete the scoped Git-history audit at 6482bc6: 21 commits / 114 unique text blobs screened without credential-pattern findings; fonts/current previews verified. All nine outstanding historical PNGs retrieved through the GitHub plugin, blob hashes verified, visually reviewed and metadata checked on 2026-09-20. No findings requiring removal or credential rotation. See docs/internal/history-audit.json for scope and exclusions.
- [x] Review source-license compatibility with pinned JUCE under AGPLv3. See docs/internal/LICENSE_ASSET_AUDIT.md; binary distribution follow-up remains below.
- [x] Inspect included font licenses and music license/provenance. See LICENSE_ASSET_AUDIT.md for evidence and scope.
- [x] Decide whether AGPL fits future plans and how outside contributions will be accepted. No commercial plans; AGPL is required by the free JUCE licence. Contributions are accepted under AGPL-3.0-or-later without a CLA (see CONTRIBUTING.md).
- [x] Distinguish Classic degradation from the experimental real bitstream in README and BITSTREAM.md. Classic Budget is artistic; RIT Budget maps to a byte budget with packet kbps displayed.
- [x] Make the README useful to a new user: screenshot, audio demo, parameters, formats/platforms, build/install instructions, fixed latency, and limitations.
- [ ] Verify a fresh checkout builds on macOS using only documented instructions, with no workspace-specific or cached dependencies.
- [ ] Run tests and AU validation; smoke-test playback, automation, bypass, presets, session save/reopen, multiple instances, and editor open/close in a DAW.
- [ ] Exercise extreme settings, silence, transport changes, sample-rate changes, and small buffers; check levels, finite output, unexpected bursts, dropouts, and CPU usage.
- [x] Clearly distinguish compiled and DAW-tested platforms in docs/RELEASE_VALIDATION.md. Latest Mac/AU and Windows validation is pending.
- [x] Review tracked artifact inventory: build outputs/logs/music binaries are ignored and not tracked. Retain useful measurement reports, embedded fonts and UI previews; release archives stay outside Git.
- [x] CI remains workflow_dispatch-only, contents:read, with commit-pinned actions, nonpersistent checkout credentials, a 45-minute job limit and 7-day artifact retention. No hosted runs were triggered.
- [x] Add contribution expectations, a bug-report template, security policy and experimental support expectations.
- [x] Prepare private-reporting policy and publication-day instructions in SECURITY.md and RELEASE_VALIDATION.md. GitHub supports the feature for public repos; it is not enabled on this private repo.
- [ ] At separately approved publication, enable private vulnerability reporting, verify the report form and maintainer notifications before announcements/binaries.
- [x] Prepare existing version 0.2.0 as an unreleased experimental preview with CHANGELOG.md and known compatibility limits. No tag/release published.

- [x] Capture a clean Linux build and reproducible pre-bitstream audio baseline. See docs/internal/BASELINE.md and reports/baseline.json. Offline JUCE override used; Mac/AU remains unchecked.
- [ ] Implement the real experimental Ritual bitstream before initial public release (owner decision, 2026-09-17).
  - [x] First independent CLI encoder/decoder, versioned `.rit` packets, actual bitrate reporting, bounded parser and regression tests. See BITSTREAM.md.
  - [ ] Owner listening approval and temporal/stereo refinement; block discontinuities remain a known limitation.
  - [x] Fixed-storage streaming implementation and opt-in plugin integration. Linux tests and sanitizer evidence: reports/bitstream-streaming-validation.json. Mac/AU remains unverified.

## Before distributing binaries

- [x] Package the actual configured JUCE source, including its license/NOTICE files and SPDX inventory, beside curated project source. Linux VST3 archive verified; see docs/PACKAGING.md.
- [ ] Verify matching-source and notice completeness for each final platform/SDK configuration before binary publication. See LICENSE_ASSET_AUDIT.md.

- [x] Document unsigned/unnotarized builds and installation steps, with native macOS signature and AU validation instructions.
- [ ] Verify installation from each final downloaded archive on its target OS. Linux archive CRC/path/hash/mode checks and an extracted CLI source rebuild pass; Mac/Windows and DAW installation remain pending.
- [ ] Publish binaries separately from source, with accurate platform/architecture labels.

## Optional follow-up work

- [ ] Signed/notarized releases and a polished installer.
- [ ] Broader multi-DAW testing.

## Progress notes

- Compiler-warning cleanup and Linux regression checks completed during development.
- User confirmed local Mac tests passed before the warning cleanup; a fresh release checkout and subsequent Mac verification remain unchecked.
- GitHub Actions was changed to manual-only during development; confirm configuration at release time.

- Release hardening: six Linux CTest suites pass, including 2,400 deterministic CRC-repaired payload mutations, extreme streaming input and seven packaging tests. ASan/UBSan pass with LeakSanitizer disabled for VM compatibility. See docs/internal/release-hardening.json.

- 2026-09-20: Colin owns listening and DAW validation. Documentation review corrected stale cross-platform claims, both-CLI build instructions, Release configuration examples, ad-hoc signing terminology and the private-reporting publication sequence. Historical image review completed through the GitHub plugin with no findings; no repository visibility/settings changes or Actions runs.

- 2026-09-21: Optional CLI MP3/FLAC reader added; Linux format tests cover both CLIs. Native PCM24 WAV requires no dependency. Extended-input CLI binary distribution remains blocked by the packager until runtime/source/notice bundling is implemented; this does not block default CLI or plugin packages. Mac backend validation remains pending.

- 2026-09-21: RIT Competition, Memory, Punch and Crunch implemented in CLI and plugin, with unchanged packet format and bitrate budget. Neutral CLI encoding preserves existing bytes. Linux plugin 8/8 suites and optional-input CLI 7/7 suites pass; control tests also pass ASan/UBSan (LeakSanitizer disabled). Native AU and listening validation of these controls remain pending. No Actions runs.

- 2026-10-08: Pre-publication tidy-up. README rewritten for end users; internal audit/checklist files moved to docs/internal/; unused docs/font-options.png removed; SECURITY.md simplified; dense C++ files reformatted with the new .clang-format (CLI outputs verified byte-identical); engine header renamed from bitfamine.hpp to classic_engine.hpp (namespace classic); both CLIs reject WAV output above the 4 GiB RIFF limit. Demo MP3 still needs rendering locally (tools/make_demo_mp3.py).
