# Releasing YACHT

The Git tag is the release version: stable `vMAJOR.MINOR.PATCH`, prerelease `vMAJOR.MINOR.PATCH-beta.N` or `-rc.N`. Embedded/package version omits the `v`. `Cargo.toml` supplies the version to the build; CI must compare it with the tag and fail on a mismatch. The current source version is 2.1.6. The earlier 2.1.2 release candidates are development builds: their workflow artifacts are not production signed. Production automatic-update trust remains unconfigured; a notarized direct download does not enable that channel.

## Release categories

| Category | Origin and treatment |
|---|---|
| Stable | Version-matched tag authorized by Thomas Lothian; verified platform artifacts, checksums, and GitHub Release. Local macOS distributable packaging requires signing and notarization; other platform signing remains optional until configured. |
| Prerelease | Tagged beta/RC; clearly marked prerelease, no production signing |
| Historical development | Existing `dev-vVERSION-COMMIT` GitHub prereleases; direct-download artifacts unsigned or ad-hoc signed, no production update feed. Their legacy automation remains disabled. |

The active native workflow runs weekly for newly introduced, unattempted version tags. An owner-requested manual run selects one exact tag through the same eligibility and claim checks. It requires an existing public release with a matching qualified Mac archive, honors trusted holds, verifies immutable tag identities, and skips existing or partly uploaded native targets without consuming attempts. Each Windows/Linux x64/ARM64 job builds and checks its final packages independently; only its successful run-bound artifact can reach that target's publisher. Publishers recheck readiness, tag identity and checksums before upload and never replace different bytes. Build jobs remain read-only. The workflow does not build macOS, create releases, change GitHub Latest, publish authenticated update feeds, automatically retry attempts, or run on ordinary pushes. See [weekly publication policy](../.github/WEEKLY_RELEASES.md).

Historical development releases remain direct downloads for community testing. They are not signed automatic updates. The former six-platform push/tag workflows, release initializer and automatic development publishers are retained under `.github/workflow-archive/2026-11-08/` and remain disabled. The active weekly publishers can process version-matched stable and beta/RC tags that satisfy current prerequisites; stable releases still require an annotated source tag. Missing production trust keys and installation qualification continue to block authenticated automatic updates.

GitHub Releases are the canonical direct download and update source. The candidate updater uses signed GitHub Release metadata and native adapters; see [the update release contract](updater/RELEASE-CONTRACT.md). Production keys and full installation qualification are still required. A future project website should link to GitHub releases. Native app stores, if used, are the exception.

## Target stable pipeline

1. Review the [licensing audit](LICENSE_AUDIT.md), exact bundled third-party notices, test evidence, and [support matrix](SUPPORT_MATRIX.md). Resolve the Windows package questions before calling that artifact GPL compliant or SignPath eligible.
2. Update the next version in `Cargo.toml`, run `script/sync_version.py`, and verify the generated macOS/Windows metadata. Review the changelog and release notes. Thomas Lothian creates an annotated tag; CI checks the tag's version and source commit. Cryptographic tag signing is optional.
3. Each platform builds and runs its required tests independently. A platform publishes only after its own tests and any configured signature checks pass. A failure cannot mark that platform artifact as validated or block a separate successful platform from completing.
4. Build and functionally verify each platform's current package format. Developer ID sign/notarize/staple macOS apps using default `script/package.sh`; add `--production` for authenticated update packages. When configured, Authenticode-sign Windows binaries and GPG-sign Linux packages; verify every configured signature. Otherwise label the platform artifact unsigned or ad-hoc signed and keep it out of the signed automatic-update channel.
5. Produce a release SBOM in SPDX or CycloneDX format, `SHA256SUMS`, a signed checksum manifest where appropriate, artifact attestations, and release notes. Use lowercase filenames like `yacht-2.1.5-macos-arm64.zip`. Publish a GitHub Release that identifies every expected platform and plainly lists any missing/failed artifact. Omit empty release-note sections.

Release notes use **Highlights**, **Changes**, **Fixes**, **Known issues**, **Supported platforms**, **Installation/update notes**, and **Verification/security information** as applicable. Keep source and build provenance linked to the tag.

## Current implementation gaps

Current scripts still emit Windows Inno installers and Linux `.deb`/tar files. Azure Artifact Signing, Linux AppImage packaging, complete SBOM generation and attestation are not wired. Existing v2.1.1 artifacts are historical and should not be relabeled. Production signing credentials and a Linux GPG key must be provisioned outside the repository. See [CODE_SIGNING_POLICY.md](../CODE_SIGNING_POLICY.md) for the policy. Development direct downloads can publish without those credentials; do not describe them as fully signed or eligible for automatic updates.
