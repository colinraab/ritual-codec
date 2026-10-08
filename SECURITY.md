# Security policy

Ritual Codec is an experimental hobby project with no security-support
commitment. The `.rit` decoder is fuzz-style tested against malformed input but
has not had an independent audit.

If you find a security problem (for example, a crafted `.rit` or WAV file that
crashes the decoder or corrupts memory), please report it privately using
GitHub's **Security → Report a vulnerability** button on this repository rather
than opening a public issue. Include the commit or version, platform, a minimal
reproduction and the impact you observed.

Please don't attach private audio or crash dumps containing unrelated data.
Fixes are made on a best-effort basis, and a fixed release or advisory will be
published by the maintainer.
