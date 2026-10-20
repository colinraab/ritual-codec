# Public release checklist

Target: an experimental public release of Ritual Codec 0.2.0.
Updated 2026-10-19. Completed local checks do not imply publication or broader
platform support. [Release validation](../RELEASE_VALIDATION.md) is the source
of truth for test scope and remaining compatibility limits.

## Completed preparation

- [x] Screen the scoped Git history at `6482bc6`: 21 commits and 114 unique text
  blobs, with no credential-pattern findings. All nine outstanding historical
  PNGs were retrieved, hash-checked, visually reviewed and metadata-checked on
  2026-09-20. See [history-audit.json](history-audit.json) for exclusions.
- [x] Review source, font and music licensing. See
  [LICENSE_ASSET_AUDIT.md](LICENSE_ASSET_AUDIT.md). The project uses AGPL and accepts
  contributions under AGPL-3.0-or-later without a CLA; JUCE retains its own terms.
- [x] Distinguish artistic Classic Budget from the real experimental RIT byte
  budget. Implement the CLI encoder/decoder, bounded parser, fixed-storage
  streaming engine and plug-in controls. Format stability and sound refinement
  remain experimental.
- [x] Provide the README, current interface images, credited audio demo, build
  and install guides, contribution guidance, bug-report template and security policy.
- [x] Keep generated builds, logs and full music renders out of tracked files;
  retain useful measurement reports, embedded fonts and UI previews.
- [x] Keep CI manual-only, with read-only repository permissions, pinned actions,
  nonpersistent checkout credentials, a 45-minute limit and seven-day retention.
  The current workflow was checked locally; no hosted run was started.
- [x] Prepare the experimental 0.2.0 release documentation and Mac signing
  notice. The repository is public; a release draft is being prepared.

## Completed macOS arm64 candidate checks

These checks cover the local `0.2.0-preview.1` candidate on macOS 26.6.2.
The binaries require macOS 26.6. See the validation guide for exact fixtures,
versions, measurements and limitations.

- [x] Rebuild app, AU, VST3 and both CLIs from packaged source with bundled JUCE,
  without an external JUCE cache; pass all nine Release test suites.
- [x] Validate the AU and exercise native REAPER AU/VST3 playback, controls,
  automation, bypass, presets, editor lifecycle, session restore and multiple
  instances. Record the narrower Ableton VST3 playback/save/reopen check and
  the owner's editor/sound confirmation separately.
- [x] Exercise supported sample rates, small buffers, extreme settings and
  transport changes. Run four automated instances across both formats/modes
  for over ten minutes at 48 kHz/32 samples and verify a finite, unclipped render.
  Sampled CPU readings do not prove zero hardware underruns.
- [x] Exercise standalone transport, output selection and settings restore.
  Play the full 365.688-second track in both modes; verify exact-length float WAV
  exports, replacement recovery from Trash, cancellation, invalid input and
  recovery from no selected output. Physical device unplugging is still untested.
- [x] Extract separate app/AU/VST3/CLI archives, compare payloads, verify ad-hoc
  signatures, install the fresh plug-ins, run AU validation and reopen the test
  project. Verify matching configured JUCE sources and notice inventory.

## Remaining before distribution

- [ ] Obtain final owner listening approval, including RIT sound and stereo behavior.
- [ ] Test installation from a downloaded final archive on another Mac, including
  Gatekeeper behavior. A second Mac is currently unavailable.
- [ ] Qualify every additional advertised platform, architecture and OS version.
  Linux has local build/test evidence; current Windows DAW qualification and
  Intel/universal Mac qualification remain open.
- [ ] Review matching source, notices and final checksums for each exact published
  platform/SDK configuration. Current local inventory is technical evidence,
  not independent legal certification.
- [ ] Rerun history screening against the final release ref and record its commit.
- [x] Confirm private vulnerability reporting is enabled on the public repo
  through the GitHub API (2026-10-19).
- [ ] Verify the report form and maintainer notification delivery. Follow
  [the publication sequence](../RELEASE_VALIDATION.md#private-vulnerability-reporting).
- [ ] Publish source and binaries with accurate architecture, minimum OS and
  signing labels. Repository visibility and release publication require an
  explicit owner decision. The repository is now public; release publication
  remains separate from preparation of the draft.

## Optional follow-up

- Developer ID signing, notarization and an installer when credentials are available.
- Broader DAW coverage and a physical audio-device disconnect test.
- Portable runtime/source/notice bundling for optional libsndfile CLI packages.
  The packager currently rejects those archives; default WAV CLIs are unaffected.

## Historical evidence

Earlier Linux baselines and hardening results remain in
[BASELINE.md](BASELINE.md), [release-hardening.json](release-hardening.json) and
[bitstream-streaming-validation.json](../../reports/bitstream-streaming-validation.json).
Their platform scope and earlier test counts do not replace the current Mac results.
