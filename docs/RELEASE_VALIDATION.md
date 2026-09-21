# Release validation

Ritual Codec 0.2.0 is an **unreleased experimental preview**. A successful compile
is not a DAW compatibility claim. Do not publish binaries until the matching
platform checks below are completed and recorded against a commit.

| Platform / format | Current evidence | Still required |
| --- | --- | --- |
| Linux x64 CLI / VST3 | Local VM Release builds and automated tests; no DAW qualification | Real host playback, automation and sustained load |
| macOS AU / VST3 | Earlier development builds passed; owner reported earlier local tests | Fresh latest checkout, native AU validation and DAW smoke test |
| Windows VST3 / CLI | Earlier development CI build passed | Latest build and DAW smoke test |

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
Review historical PNGs and font metadata as well. reports/history-audit.json
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

GitHub documents private vulnerability reporting for **public repositories**.
It cannot be treated as a completed pre-publication setting on this private repo.
At the separately approved publication step, before announcements or binaries:

1. In repository **Settings → Advanced Security**, enable **Private vulnerability
   reporting** (under the Security and quality sidebar section).
2. Open **Security → Advisories** and verify **Report a vulnerability** opens
   the private report form. Verify from a non-admin account if available; do not
   submit a fake advisory merely to test the button.
3. Ensure the maintainer watches **Security alerts** (or All Activity) and has
   the intended GitHub notification delivery enabled.
4. Record the date and verification evidence here, and update SECURITY.md to
   remove its pending-status wording. If unavailable, establish a private
   contact route before announcing the project.

Source: [GitHub configuration instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository),
checked 2026-09-20. Status: pending publication; no settings or visibility changed.

## Validation ownership

As of 2026-09-20, Colin is handling listening and DAW validation. Keep those
checklist gates open until results identify the tested revision, platform and
host. Documentation and historical-preview review are separate from audio approval.
