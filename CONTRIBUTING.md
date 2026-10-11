# Contributing to YACHT

Outside contributions are welcome. Thomas Lothian is the sole maintainer and retains release authority. Direct commits to `main` are permitted; pull requests and reviews are optional.

Describe the change, run relevant tests from [docs/TESTING.md](docs/TESTING.md), and update user-facing documentation when behavior changes. Changes to shared conversion behavior should include Rust coverage and be considered for every native frontend. Use a concise plain-English imperative commit subject; Conventional Commits are not required. Failing checks remain visible and should be fixed, but are not required for unrelated commits or pushes.

Thomas Lothian's original YACHT code and artwork are GPL-3.0-or-later. Contributed code and assets must be legally compatible with that license, with provenance and any required third-party notices identified in the change description or pull request. The [licensing audit](docs/LICENSE_AUDIT.md) records the separate Windows binary distribution questions. Do not submit material whose licensing is unclear. No contributor license agreement is required.

The [Developer Certificate of Origin 1.1](https://developercertificate.org/) sign-off is encouraged, and maintainers normally use `git commit -s`; it is not an enforced gate. The trailer states your right to submit the work and is not a cryptographic signature. Cryptographic commit signing is optional. Official stable release tags retain the signature verification in the [code signing policy](CODE_SIGNING_POLICY.md).

Security issues belong in [private vulnerability reporting](SECURITY.md). Please follow the [Code of Conduct](CODE_OF_CONDUCT.md).
