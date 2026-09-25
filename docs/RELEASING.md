# Releasing YACHT

The Git tag is the release version: stable `vMAJOR.MINOR.PATCH`, prerelease `vMAJOR.MINOR.PATCH-beta.N` or `-rc.N`. Embedded/package version omits the `v`. `Cargo.toml` supplies the version to the build; CI must compare it with the tag and fail on a mismatch. The current version is 2.1.1. No new release is created by this documentation change.

## Release categories

| Category | Origin and treatment |
|---|---|
| Stable | Cryptographically signed and verified tag by Thomas Lothian; production platform signing, checksums, SBOM, attestation, and GitHub Release |
| Prerelease | Tagged beta/RC; clearly marked prerelease, no production signing |
| Development | `main` build; clearly unsupported, no production signing, artifact retention 30 days; routine runnable nightly package only for macOS (ARM64) |

GitHub Releases are the canonical direct download and update source. No application component downloads or installs updates automatically. A future project website should link to GitHub releases. Native app stores, if used, are the exception.

## Target stable pipeline

1. Review the [licensing audit](LICENSE_AUDIT.md), exact bundled third-party notices, test evidence, and [support matrix](SUPPORT_MATRIX.md). Resolve the Windows package questions before calling that artifact GPL compliant or SignPath eligible.
2. Update the next version in `Cargo.toml`, run `script/sync_version.py`, and verify the generated macOS/Windows metadata. Review the changelog and release notes. Thomas Lothian signs the maintainer commit and creates a signed annotated tag; CI rejects an unsigned or unverified stable tag.
3. Each platform builds and runs its required tests independently. A platform publishes only after its own tests and signature checks pass. A failure cannot mark that platform artifact as validated or block a separate successful platform from completing.
4. **macOS (ARM64/x64):** Build `.app`, Developer ID sign, notarize, staple, verify, then ZIP. **Windows (x64/ARM64):** Build and Azure Artifact Sign shipped binaries, verify Authenticode, then portable ZIP. **Linux (x64/ARM64):** Build AppImage, GPG sign it, and generate GitHub/Sigstore artifact attestation.
5. Produce a release SBOM in SPDX or CycloneDX format, `SHA256SUMS`, a signed checksum manifest where appropriate, artifact attestations, and release notes. Use lowercase filenames like `yacht-2.1.2-macos-arm64.zip`. Publish a GitHub Release that identifies every expected platform and plainly lists any missing/failed artifact. Omit empty release-note sections.

Release notes use **Highlights**, **Changes**, **Fixes**, **Known issues**, **Supported platforms**, **Installation/update notes**, and **Verification/security information** as applicable. Keep source and build provenance linked to the tag.

## Current implementation gaps

Current scripts still emit macOS DMGs, Windows Inno installers, and Linux `.deb`/tar files. Automatic release drafting from development-signed outputs has been disabled. Azure Artifact Signing, Linux AppImage packaging, complete SBOM generation and attestation are not wired. Existing v2.1.1 artifacts are historical and should not be relabeled. Production signing credentials and a Linux GPG key must be provisioned outside the repository. See [CODE_SIGNING_POLICY.md](../CODE_SIGNING_POLICY.md) for the policy. Until these gaps are closed, do not publish a new release as fully signed/validated.
