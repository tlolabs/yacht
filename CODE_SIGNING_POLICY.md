# Code signing policy

Git commits need no cryptographic signature; see [CONTRIBUTING.md](CONTRIBUTING.md). This policy covers release tags and distributed artifacts. Release-tag signing, package signing, and notarization are optional until the corresponding credentials and verification are configured. Stable release tags must still match the project version and checked-out commit.

An **official stable release** is a GitHub Release for a SemVer tag `vMAJOR.MINOR.PATCH` authorized by Thomas Lothian, with artifacts built from that tagged commit and accompanied by checksums and release provenance. A passing CI run alone is a development build. Tags with a prerelease suffix are prereleases; `main`/nightly outputs are development builds. Unsigned artifacts must be labeled as such and must never be described as signed or notarized.

Thomas Lothian retains release authority. Before using production credentials, verify the stable tag and configured signer when signing is enabled. Do not use a redundant GitHub Environment human approval gate; a signing provider may independently require interactive approval. CI builds and tests artifacts, records their provenance, signs when the required provider and credentials are configured, and verifies the result before publication. Each platform is verified independently; a failure prevents only that platform's artifact from being presented as validated.

| Platform | Intended signed method | Verification when configured |
|---|---|---|
| macOS (ARM64/x64) | Apple Developer ID signing, Apple notarization, staple the app, then ZIP | `codesign` and `spctl`/stapler checks on the distributed app |
| Windows (x64/ARM64) | Azure Artifact Signing and Authenticode; portable ZIP | `signtool verify /pa` for shipped executables and DLLs |
| Linux (x64/ARM64) | GPG detached signature for AppImage plus GitHub/Sigstore artifact attestation | GPG signature, SHA-256, and attestation verification |

These methods are the **target signed-distribution policy**, not a claim that all current release automation implements them. The current workflow produces development packages by default. Windows currently has an optional local certificate-thumbprint signing path, and Linux uses `.deb`/tar rather than AppImage; these require migration before a new release can claim the target signed formats. The Windows package licensing questions are recorded in [docs/LICENSE_AUDIT.md](docs/LICENSE_AUDIT.md). Do not label an unsigned artifact as signed.

Signing keys, certificates, provider credentials, and notarization credentials belong in managed CI secrets or an ephemeral local keychain/store. They must never be committed or exposed in logs. Published releases should carry a standard `SHA256SUMS` manifest, an SBOM, and GitHub artifact attestations where supported. The Git tag identifies the release source and the GitHub Release is the canonical download source. Signed automatic updates remain disabled until their separate pinned trust configuration is complete. Source code, build scripts, checksums, and available signature/attestation instructions are linked from the release.

This project is **not currently using SignPath**. If that changes, this policy will name the provider and satisfy its specific approval and disclosure requirements. [SignPath Foundation's current terms](https://signpath.org/terms.html) are assessed in the local readiness report; the project does not claim eligibility.
