# Public release checklist

Target: an experimental public release of Ritual Codec.
Unchecked items are not yet verified for release, even where implementation exists.
Track evidence or relevant commits beside completed items. Making the repository public is a separate owner decision.

## Before making the repository public

- [x] Owner confirmed on 2026-09-17 that this plugin does not conflict with work obligations or scope. Recorded as owner confirmation, not an independent legal review.
- [ ] Audit the entire Git history for secrets and private information: credentials, tokens, emails, local paths, private links, logs, and accidental uploads. Rotate any exposed credentials.
- [x] Review source-license compatibility with pinned JUCE under AGPLv3. See LICENSE_ASSET_AUDIT.md; binary distribution follow-up remains below.
- [x] Inspect included font licenses and music license/provenance. See LICENSE_ASSET_AUDIT.md for evidence and scope.
- [ ] Decide whether AGPL fits future commercial plans and how outside contributions will be accepted.
- [ ] Describe the current engine accurately: an experimental degradation effect inspired by lossy codecs, not yet a compressed-bitstream encoder. Budget is an effect parameter, not a measured bitrate.
- [ ] Make the README useful to a new user: screenshot, audio demo, parameters, formats/platforms, build/install instructions, fixed latency, and limitations.
- [ ] Verify a fresh checkout builds on macOS using only documented instructions, with no workspace-specific or cached dependencies.
- [ ] Run tests and AU validation; smoke-test playback, automation, bypass, presets, session save/reopen, multiple instances, and editor open/close in a DAW.
- [ ] Exercise extreme settings, silence, transport changes, sample-rate changes, and small buffers; check levels, finite output, unexpected bursts, dropouts, and CPU usage.
- [ ] Clearly distinguish platforms that compile from platforms tested in a DAW.
- [ ] Remove unnecessary build outputs, temporary reports, binaries, and large audio assets from the repository; use Releases for distributed binaries.
- [ ] Verify CI remains manual-only to control Actions usage; review permissions and handling of untrusted contributions and secrets.
- [ ] Add contribution expectations, issue guidance, and a private security-reporting route; state experimental status and support expectations.
- [ ] Choose an initial version and provide a changelog and known compatibility issues.

- [x] Capture a clean Linux build and reproducible pre-bitstream audio baseline. See BASELINE.md and reports/baseline.json. Offline JUCE override used; Mac/AU remains unchecked.
- [ ] Implement the real experimental Ritual bitstream before initial public release (owner decision, 2026-09-17).

## Before distributing binaries

- [ ] Include complete applicable dependency license/NOTICE texts and verify corresponding-source distribution. See LICENSE_ASSET_AUDIT.md.

- [ ] Document unsigned/unnotarized builds and installation steps where applicable.
- [ ] Verify downloadable archives and installation from a clean download.
- [ ] Publish binaries separately from source, with accurate platform/architecture labels.

## Optional follow-up work

- [ ] Signed/notarized releases and a polished installer.
- [ ] Broader multi-DAW testing.

## Progress notes

- Compiler-warning cleanup and Linux regression checks completed during development.
- User confirmed local Mac tests passed before the warning cleanup; a fresh release checkout and subsequent Mac verification remain unchecked.
- GitHub Actions was changed to manual-only during development; confirm configuration at release time.

