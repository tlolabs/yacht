# Repository development policy

`main` is the authoritative development branch. Direct commits and pushes are permitted. Pull requests, reviews, status checks, DCO trailers, and cryptographic commit signatures are voluntary; use `git commit -s` for maintainer commits as a convention. Do not configure repository rulesets or branch protection to require them.

Keep automated unit, integration, functional, regression, accessibility, security, dependency, and six-platform build checks. Report main and release-tag failures visibly in one consolidated GitHub issue, but do not make unrelated development depend on a passing check. Resolve failures in the affected area before calling that area verified.

Build and release each supported platform independently. Publish a platform artifact only after its own build, tests, package checks, and functional verification succeed. Package signing and notarization are optional until configured; never claim an unsigned artifact is signed. Keep stable release tag verification before production signing. Preserve update signature verification and runtime security controls. Retry transient failures a bounded number of times after a relevant change, and retain the failure record when retries fail.

Preserve supported macOS ARM64/x64, Windows x64/ARM64, and Linux x64/ARM64 targets. Do not weaken authentication, update signatures, artifact integrity checks, or platform runtime safeguards to make CI pass.
