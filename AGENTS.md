# Repository development policy

`main` is the authoritative development branch. Direct commits and pushes are permitted. Pull requests, reviews, status checks, DCO trailers, and cryptographic commit signatures are voluntary; use `git commit -s` for maintainer commits as a convention. Do not configure branch protection or repository rulesets.

Keep automated unit, integration, functional, regression, accessibility, security, dependency, and six-platform build checks. Report main, release-tag, and update-authentication failures visibly in one consolidated GitHub issue, but do not make unrelated development depend on a passing check. Resolve failures in the affected area before calling that area verified.

Build and release each supported platform independently. Publish a platform artifact only after its own build, tests, package checks, and functional verification succeed. Package signing and notarization are optional until configured; never claim an unsigned artifact is signed. Keep stable release tag verification before production signing. Preserve update signature verification and runtime security controls. Retry transient failures a bounded number of times after a relevant change, and retain the failure record when retries fail.

Eligible `main` commits publish commit-bound `dev-vVERSION-SHA` prereleases automatically. Six read-only build jobs feed six separate, contents-write publisher jobs. One source-bound initializer creates the shared draft release; each publisher verifies its own package and checksum and refuses to replace different published bytes. Documentation-only pushes do not create development releases. Development direct downloads are not signed automatic-update feed entries.

Preserve supported macOS ARM64/x64, Windows x64/ARM64, and Linux x64/ARM64 targets. Do not weaken authentication, update signatures, artifact integrity checks, or platform runtime safeguards to make CI pass.

## CI scheduling policy (owner update, 2026-11-07)

This section supersedes older instructions for automatic CI coverage and publication.

Hosted native CI runs once weekly on Sunday morning Pacific, and only for newly introduced version tags. Existing tags are baselined. The active workflow builds supported Linux and Windows targets and performs basic startup smoke checks only. It does not run macOS builds, full regression suites, automatic retries, publishing, signing, or release creation. Legacy workflows are retained but disabled. GitHub-managed security services remain separate. Runtime authentication, signature verification, and artifact-integrity safeguards must not be weakened. Local development and release verification requirements still apply before any separately authorized publication.

## Archived legacy automation

The former custom workflows are preserved, inactive, under `.github/workflow-archive/2026-11-08/`, with original paths and hashes recorded in its manifest. This archive supersedes older descriptions of automatic builds, retries, reporting, and release publishing. Do not move them back to `.github/workflows/`, enable, or dispatch them without a new explicit owner instruction. GitHub-managed security services remain separate and enabled.
