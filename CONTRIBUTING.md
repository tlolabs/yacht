# Contributing to YACHT

Outside contributions are welcome. Thomas Lothian is the sole maintainer and retains final review, merge, signing, and release authority.

Before opening a pull request, describe the change, run relevant tests from [docs/TESTING.md](docs/TESTING.md), and update user-facing documentation when behavior changes. Changes to shared conversion behavior should include Rust coverage and be considered for every native frontend. Use a concise plain-English imperative commit subject; Conventional Commits are not required.

Thomas Lothian's original YACHT code and artwork are GPL-3.0-or-later. Contributed code and assets must be legally compatible with that license, with provenance and any required third-party notices identified in the pull request. The [licensing audit](docs/LICENSE_AUDIT.md) records the separate Windows binary distribution questions. Do not submit material whose licensing is unclear. No contributor license agreement is required.

Use the [Developer Certificate of Origin 1.1](https://developercertificate.org/). Add a `Signed-off-by: Your Name <your@email.example>` trailer to each contributed commit, for example with `git commit -s`. This is a statement about your right to submit the work, not a cryptographic signature. Cryptographically signed commits are recommended but are not mandatory for outside contributors. The maintainer signs maintainer commits; official stable release tags must be cryptographically signed.

Security issues belong in [private vulnerability reporting](SECURITY.md). Please follow the [Code of Conduct](CODE_OF_CONDUCT.md).
