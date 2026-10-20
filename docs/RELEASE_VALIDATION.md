# Release validation

Ritual Codec 0.2.0 is an **experimental release**. The download set covers
macOS arm64 only, with the local evidence and compatibility limits below.
A successful compile is not a broader DAW compatibility claim. Additional
platforms need their own builds and host validation before binaries are offered.

| Platform / format | Current evidence | Still required |
| --- | --- | --- |
| Linux x64 CLI / VST3 | Local VM Release builds and automated tests; no DAW qualification | Real host playback, automation and sustained load |
| macOS arm64 app / AU / VST3 | 2026-10-19 local Release: 9/9 suites, AU validation, standalone and native DAW checks below | Second-machine installation; owner listening approval; broader OS/architecture qualification |
| Windows VST3 / CLI | Earlier development CI build passed | Latest build and DAW smoke test |

## macOS local candidate, 2026-10-19

The candidate is an unreleased preview built from the working tree based on
`f57311a7c272cd8fb862e35d13a0348226b77267`. Archive provenance identifies
the base revision and whether tracked source was modified. Archive source ZIPs
and checksums identify the packaged source and binaries.

- Nine Release test suites pass, including processor behavior, streaming controls,
  decoder hardening, eight packaging cases and standalone rendering. The latter
  covers six mono/stereo rate combinations, exact bypass, both modes, malformed
  input and cancellation during a six-minute render.
- Native standalone checks cover playback, pause, stop, seek, output selection,
  settings restore and Classic/RIT WAV export. Both modes played the complete
  365.688-second stereo track. Both exports contain exactly 17,553,032 frames at
  48 kHz, float32 stereo, with finite samples. RIT peak was 1.371; the effect has
  no limiter. Repeat export preserved the previous file in Trash; recovery was
  verified by restoring it and comparing its hash. Quit during an active export
  preserved the existing destination and left no partial destination file.
- The extracted standalone played three-second FLAC/AIFF fixtures and a short
  portion of a full MP3. Malformed WAV and four-channel WAV were rejected while
  preserving the previous track. Output switched between built-in and virtual
  devices; selecting none disabled Play and survived restart. Restoring speakers
  enabled playback without automatic playback. Buffers changed 512 to 256 and
  back. Physical hardware disconnect remains untested.
- REAPER 7.27: AU/VST3 Classic/RIT controls, presets, bypass, save/reopen,
  editor open/close, 44.1/48/96 kHz and 256-sample reported latency. A four-instance
  matrix ran over ten minutes at 48 kHz/32 samples with seven parameters per
  instance switching endpoints at 10 Hz. Native editors confirmed automation in
  all four format/mode combinations. Sampled FX CPU totaled about 3%; this does
  not establish zero hardware underruns. Its 61-second offline render (including
  a one-second tail) was finite with peak 0.1300 and zero clipped samples.
- Versioned arm64 standalone, AU, VST3 and CLI archives were extracted locally.
  The installed plugins match every packaged file, pass strict ad-hoc signature
  checks and AU validation, and reload in the saved four-instance REAPER project.
  Both extracted CLI programs processed the 24-second stereo fixture. The
  packaged source rebuilt app/AU/VST3/CLI without an external JUCE cache and passed
  all nine test suites. The configured JUCE tree retains 25 named license,
  NOTICE and SPDX files plus notices embedded in source. This is a technical
  inventory, not independent legal certification.
- These particular local binaries are arm64 and have a macOS 26.6 deployment
  target; tested on 26.6.2. Older macOS, Intel and universal builds require their
  own builds and qualification. CI changes were syntax-checked locally only.
- Ableton Live 12.3.1: VST3 playback at 48 kHz and save/reopen. The owner
  separately confirmed the editor opens and audio sounds good. This is narrower
  coverage than the REAPER matrix.
- No second-machine, Intel, physical device-disconnect or downloaded-Gatekeeper
  test has been completed. Automated rendering is not human listening approval.
- No public release, hosted CI run, Developer ID signing or notarization occurred.

## Version 0.2.0 distribution

The release uses the same application and DSP sources as the locally tested
candidate; subsequent changes update documentation and commit metadata. The release tag identifies the final source revision.
All four archives are rebuilt and packaged from that clean revision, with
matching source, licenses, provenance and checksums. Only macOS arm64 binaries
are offered; their minimum deployment target remains macOS 26.6.

Second-machine installation and downloaded Gatekeeper behavior remain untested.
A second Mac is unavailable. Developer ID signing and notarization are absent.
These limits are disclosed in the README, install guide and release notes; they
remain follow-up work for broader qualification. Final owner listening approval
is separate from automated checks and the recorded host tests.

## Local gates

Use docs/BUILDING.md to configure and build. Run `ctest --test-dir <build-dir>
--output-on-failure` (with `-C Release` for multi-configuration generators).
Python packaging tests run when CMake finds Python; otherwise run
`python3 tests/test_packaging.py` explicitly. No GitHub Actions invocation is
needed. CLI builds have no JUCE dependency.

For a source-history screening pass, use a full clone with all intended refs:

```sh
git fetch --all --tags
python3 tools/audit_history.py --repo . > history-review.json
```

Review findings manually; exit zero means no pattern matches, not certification.
Review historical PNGs and font metadata as well. docs/internal/history-audit.json
records the original text/font audit and the 2026-09-20 completion of all nine
outstanding historical preview reviews through the GitHub plugin. No visible
private information or PNG text/EXIF metadata was found; decoded blob hashes
matched the historical inventory. Re-run screening against the final release ref.
Do not rewrite history or rotate credentials unless an actual finding warrants it.

## Mac / DAW smoke test

Close the DAW before installing the newly built component. Follow the local AU
build/install instructions in BUILDING.md. Then run:

```sh
auval -v aufx RtCd Crrb
```

In Ableton or another host, record OS/architecture, DAW version, plugin format,
commit, sample rate and buffer size. Use a test session and initially modest
monitoring gain: this is an intentional degradation effect without a limiter.

- Exercise Classic and RIT with music, silence and isolated drum hits at 44.1,
  48 and 96 kHz and available 32/64/128/512-sample buffers.
- Automate Budget, Competition, Memory, Punch, Crunch, Mix and Output in both
  modes; repeatedly switch modes and toggle host and
  plugin bypass. Check for dropouts, unexpected level bursts and alignment.
- Change transport state, loop, stop/start and render offline. Confirm the host
  reports 256 samples of latency in both modes.
- Save/reopen a session in each mode, including an older Classic session. Check
  parameter values, automation and disabled Clock/Downsample controls in RIT mode.
- Open/close and resize multiple editors, instantiate several plugins, and
  monitor sustained CPU usage and audio dropouts for at least ten minutes.
- Test mono and stereo tracks and opposite-phase stereo input. Compare rendered
  and live behavior without assuming bit-identical output across platforms.

## Distribution gates

Run the packager for the exact build being tested. Extract the final archive to a
clean location and install from that extraction, not from the build tree. Verify
both CLI executables, bundle structure, executable permissions, included source,
licenses and notices. On macOS verify the completed bundle's signature and run
AU validation after extraction. Ad-hoc signing is not Developer ID notarization;
never describe these builds as notarized or disable system-wide security controls.

License strategy and final publication remain owner decisions. Nothing in this
workflow publishes a release or changes repository visibility.

## Private vulnerability reporting

The repository is public. On 2026-10-19, the GitHub API confirmed private
vulnerability reporting is enabled (`enabled: true`). Before announcements,
the maintainer should also verify the report form and notification delivery:

1. Keep **Private vulnerability reporting** enabled in repository
   **Settings → Advanced Security**.
2. Open **Security → Advisories** and verify **Report a vulnerability** opens
   the private report form. Verify from a non-admin account if available; do not
   submit a fake advisory merely to test the button.
3. Ensure the maintainer watches **Security alerts** (or All Activity) and has
   the intended GitHub notification delivery enabled.
4. Record the date and verification evidence here. SECURITY.md already points
   reporters to this button. If unavailable, add a private contact route to
   SECURITY.md before announcing the project.

Source: [GitHub configuration instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository),
checked 2026-09-20. Reporting enabled status verified 2026-10-19; the report form
and notification delivery have not been independently verified.

## Validation ownership

Colin owns final listening approval. Local agent-operated checks above identify
the tested host and scope; second-machine and distribution gates remain separate.
Documentation and historical-preview review are separate from audio approval.
