# Public release checklist

Target: an experimental public release of Ritual Codec.
Unchecked items are not yet verified for release, even where implementation exists.
Track evidence or relevant commits beside completed items. Making the repository public is a separate owner decision.

## Before making the repository public

- [x] Owner confirmed on 2026-09-17 that this plugin does not conflict with work obligations or scope. Recorded as owner confirmation, not an independent legal review.
- [ ] Finish the Git-history audit: 21 commits / 114 unique text blobs screened without credential-pattern findings; current fonts/previews verified. Nine historical PNG revisions still need binary review from a full clone. See reports/history-audit.json. No credential rotation indicated by the completed checks.
- [x] Review source-license compatibility with pinned JUCE under AGPLv3. See LICENSE_ASSET_AUDIT.md; binary distribution follow-up remains below.
- [x] Inspect included font licenses and music license/provenance. See LICENSE_ASSET_AUDIT.md for evidence and scope.
- [ ] Decide whether AGPL fits future commercial plans and how outside contributions will be accepted.
- [x] Distinguish Classic degradation from the experimental real bitstream in README and BITSTREAM.md. Classic Budget is artistic; RIT Budget maps to a byte budget with packet kbps displayed.
- [x] Make the README useful to a new user: screenshot, audio demo, parameters, formats/platforms, build/install instructions, fixed latency, and limitations.
- [ ] Verify a fresh checkout builds on macOS using only documented instructions, with no workspace-specific or cached dependencies.
- [ ] Run tests and AU validation; smoke-test playback, automation, bypass, presets, session save/reopen, multiple instances, and editor open/close in a DAW.
- [ ] Exercise extreme settings, silence, transport changes, sample-rate changes, and small buffers; check levels, finite output, unexpected bursts, dropouts, and CPU usage.
- [x] Clearly distinguish compiled and DAW-tested platforms in docs/RELEASE_VALIDATION.md. Latest Mac/AU and Windows validation is pending.
- [x] Review tracked artifact inventory: build outputs/logs/music binaries are ignored and not tracked. Retain useful measurement reports, embedded fonts and UI previews; release archives stay outside Git.
- [x] CI remains workflow_dispatch-only, contents:read, with commit-pinned actions, nonpersistent checkout credentials, a 45-minute job limit and 7-day artifact retention. No hosted runs were triggered.
- [x] Add contribution expectations, a bug-report template, security policy and experimental support expectations.
- [ ] Owner: enable GitHub private vulnerability reporting and verify the submission route before making the repository public.
- [x] Prepare existing version 0.2.0 as an unreleased experimental preview with CHANGELOG.md and known compatibility limits. No tag/release published.

- [x] Capture a clean Linux build and reproducible pre-bitstream audio baseline. See BASELINE.md and reports/baseline.json. Offline JUCE override used; Mac/AU remains unchecked.
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

- Release hardening: six Linux CTest suites pass, including 2,400 deterministic CRC-repaired payload mutations, extreme streaming input and seven packaging tests. ASan/UBSan pass with LeakSanitizer disabled for VM compatibility. See reports/release-hardening.json.
