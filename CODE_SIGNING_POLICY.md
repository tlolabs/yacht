# Code signing policy

An **official stable release** is a GitHub Release for a SemVer tag `vMAJOR.MINOR.PATCH` created and cryptographically signed by Thomas Lothian, with artifacts built from that tagged commit and accompanied by checksums and release provenance. A passing CI run alone is a development build. Tags with a prerelease suffix are prereleases; `main`/nightly outputs are unsupported development builds. Prerelease and development builds do not receive production signing.

Thomas Lothian is the sole release and signing approver. The release workflow must verify the stable tag before using production credentials. It must not use a redundant GitHub Environment human approval gate; a signing provider may independently require interactive approval. CI builds and tests artifacts, records their provenance, signs when the required provider and credentials are configured, and verifies the result before publication. A failed platform gate prevents that platform's artifact from being presented as validated.

| Platform | Intended production method | Required verification |
|---|---|---|
| macOS (ARM64/x64) | Apple Developer ID signing, Apple notarization, staple the app, then ZIP | `codesign` and `spctl`/stapler checks on the distributed app |
| Windows (x64/ARM64) | Azure Artifact Signing and Authenticode; portable ZIP | `signtool verify /pa` for shipped executables and DLLs |
| Linux (x64/ARM64) | GPG detached signature for AppImage plus GitHub/Sigstore artifact attestation | GPG signature, SHA-256, and attestation verification |

These methods are the **target policy**, not a claim that all current release automation implements them. The current workflow produces development packages by default. Windows currently has an optional local certificate-thumbprint signing path, and Linux uses `.deb`/tar rather than AppImage; these require migration before a new fully compliant stable release. The Windows package licensing questions are recorded in [docs/LICENSE_AUDIT.md](docs/LICENSE_AUDIT.md). Do not label an unsigned artifact as officially signed.

Signing keys, certificates, provider credentials, and notarization credentials belong in managed CI secrets or an ephemeral local keychain/store. They must never be committed or exposed in logs. Published releases should carry a standard `SHA256SUMS` manifest, an SBOM, and GitHub artifact attestations where supported. The signed Git tag is the release authorization and the GitHub Release is the canonical download source. Source code, build scripts, checksums, and signature/attestation instructions are linked from the release.

This project is **not currently using SignPath**. If that changes, this policy will name the provider and satisfy its specific approval and disclosure requirements. [SignPath Foundation's current terms](https://signpath.org/terms.html) are assessed in the local readiness report; the project does not claim eligibility.
