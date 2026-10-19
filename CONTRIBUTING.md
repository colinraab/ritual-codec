# Contributing to Ritual Codec

Ritual is experimental. Discuss substantial DSP, format, parameter-ID or preset
changes in an issue before implementing them. Keep Classic sessions compatible;
version incompatible bitstream changes explicitly. Do not silently change the
meaning of existing host automation parameters.

Contributions must be your own work or include the applicable source, copyright
and license notices. Project-owned contributions use the repository's
AGPL-3.0-or-later license; third-party assets retain their own licenses. Do not
submit employer-owned or confidential material. No CLA is currently required.

Use small, focused pull requests explaining the problem, resulting behavior and
validation. Run the CLI/engine tests locally; plugin changes also need the plugin
processor tests and a DAW smoke test on an available platform. Include the OS,
compiler, sample rate and buffer size for audio issues. See docs/BUILDING.md.

Format C++ changes with the repository's `.clang-format` (for example
`clang-format -i src/main.cpp`); JUCE-style spacing and Allman braces throughout.

Audio callbacks must not allocate, lock, access files or wait. Preserve bounded
parser behavior, explicit packet budgets, latency alignment and session loading.
Include regression tests for meaningful behavioral or safety fixes.

GitHub Actions is deliberately manual-only. Do not add automatic build triggers
or run hosted builds without coordinating with the maintainer. Never include
credentials, private project audio, build directories or diagnostic dumps in a PR.
For security concerns, read SECURITY.md instead of opening a public issue.

There is no response-time or production-support guarantee for this prototype.
