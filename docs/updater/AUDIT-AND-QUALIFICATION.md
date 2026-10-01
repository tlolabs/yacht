# Audit, migration and qualification ledger

Audit date: 2026-09-30. Scope: local TLO Labs repositories under
`/Users/tlothian/Documents/Projects`, with implementation in the YACHT checkout.
Integration/qualification checkouts are alternate copies, not additional apps.
Web projects, AVID Core (engine, no desktop updater) and CLI-only distributions
are not automatically assigned a desktop installation adapter.

| Application | Baseline observed | Migration status in this chat |
|---|---|---|
| YACHT | No updater; Cargo version; native-macos.yml; macOS ZIP, Windows ZIP/Inno, Linux deb/tar | Candidate shared verifier, native controls, Sparkle and release tooling implemented; keys absent; manual bootstrap required |
| ATIV | `crates/ativ-update`, Ed25519 base64 payload, stable/development feeds; Sparkle; portable Windows helper; AppImage replacement; native/application/signing workflows | Existing channel preserved. Its schema/channel and key migration need a bridge; separate active chat is editing it |
| EnCAP | SparkleBridge; `encap-release` object payload schema, appcasts; local public/private update key filenames; build-platforms.yml | Key contents not read or copied. Preserve existing keys/feed/assets; separate active chat is editing it |
| EWAF | README says manual downloads; Cargo version, native.yml, ZIP/portable/deb baseline | Concurrent chat is now adding a different tlo-updater crate and platform adapters; not modified here |
| FILLR | Rust/native frontends; build.yml, ZIP/portable/AppImage; local Developer ID/notarization scripts | Concurrent chat is now adding a different tlo-updater crate and platform adapters; not modified here |
| Hyper Jump | Python/PyInstaller app/DMG; release-macos.yml | No updater found in initial scan; adoption not implemented |
| text2qti fork | Python/CLI README; no .github directory in this checkout | Desktop distribution scope and adoption not established; not modified |

The initial updater scan found ATIV/EnCAP implementations. Subsequent reads found
new uncommitted updater implementations appearing in EWAF/FILLR and confirmed
four parallel active chats. Consolidation is required before a common library or
wire contract can be called standardized. Do not replace their work or add new
copies while they are changing.

Public YACHT latest release observed: `v2.1.1`, neither draft nor prerelease.
It contains platform packages/checksums but no `tlo-update.json`/appcasts and no
AppImages. GitHub's YACHT Actions secret-name listing returned no configured
repository secrets. This does not prove no organization/local signing credentials
exist. No production key was generated, extracted, rotated or uploaded here.
The committed candidate trust file intentionally contains no public keys or
signing identities; the production contract gate fails closed.

## Qualification boundary

| Application/platform | End-to-end older -> newer discover/authenticate/download/install |
|---|---|
| YACHT macOS ARM64 | **Not qualified**; local native build succeeded; signed feed/install test still required |
| YACHT macOS x64 | **Not qualified**; no Intel installation test |
| YACHT Windows x64/ARM64 | **Not qualified**; native build/Authenticode/installer/settings/restart tests required |
| YACHT Linux x64/ARM64 | **Not qualified**; AppImage packaging, GPG/provenance adapter and installation machinery remain |
| ATIV, EnCAP, EWAF, FILLR, Hyper Jump, text2qti | **Not qualified by this chat**; concurrent work is not evidence of successful installation |

Source-level tests cover no/same/older/newer version, malformed version, prerelease,
draft, wrong platform/architecture/OS, invalid signature/untrusted key, identity,
expiry/rollback, URL/tag/duplicate target mismatch, corruption/truncation/overflow,
interrupted transport, private state persistence and check backoff. A local mock
release server exercises discovery, signature verification and actual streaming,
HTTP 503 and truncated bodies. Failed downloads do not clobber existing files.
The CLI roundtrip signs/verifies all six fixture artifacts and rejects substituted
bytes/version mismatch. Fixture keys are deliberately public test data, never
production roots. These tests do not install an executable or qualify native
rollback/interrupted installation.

## Historical first-implementation verification (2026-09-30)

- Rust workspace format/clippy (warnings denied) and all existing core/ABI tests passed.
- 19 updater tests passed: 14 protocol/policy tests, four mock HTTP tests and one
  six-target release-tool signing/verification roundtrip.
- macOS ARM64 build and development ZIP packaging passed with Sparkle embedded,
  nested ad-hoc signatures verified, correct app/feed versions, profiling disabled,
  and no production public key present.
- Eight existing native macOS UI tests passed, zero skipped/failed, on macOS
  26.7.1 ARM64. Local result: `build/updater-native-tests.xcresult` (ignored build output).
- Platform/version/dependency/compliance checks and Python/shell syntax checks passed.
- Production contract gate failed as expected because manifest public keys are absent.
- Windows/Linux native UI, package installation and all signed older/newer update
  transactions were not run. No test above changes this ledger's unqualified status.

## Qualification follow-up (2026-10-01)

**Not qualified. No new commit, push, tag or release.** Changes remain in the
working tree based on `ed110ba27c9b7384bddf99e0925f73a33527d0e9`. The requested
commit/release gates have not passed. The old eight-test result above belongs to
the initial implementation and must not be substituted for this modified tree.
Exact machine-readable evidence and artifact hashes are in [qualification.json](qualification.json).

Audit repairs include unconditional Python release gates (also under `-O`),
Windows offered-version/digest binding and installer ProductName checks, failed
and aged download cleanup, stable macOS version/URL/architecture filtering,
normal-termination protection for active work and pending update confirmation,
accessible unconfigured settings, pinned maintainer tag verification, actual
Swift lockfile inventory, verified Sparkle tool extraction and executable mode,
and removal of force-quit behavior from the development build helper. No existing
test was removed, disabled or weakened.

| Gate | Result for the modified tree |
|---|---|
| Rust workspace, all targets | 35 passed, 0 failed/ignored; benchmarks ran; doc tests contain 0 examples |
| Swift package | 29 passed, 0 failed/skipped (25 existing + 4 update/lifecycle tests) |
| Python release/package policy | 6 passed in normal mode and 6 passed under `-O`; native verification commands mocked |
| Real Sparkle tool fixture | Signed-feed roundtrip and modified-feed rejection passed with an intentionally public test seed; no keychain/production key used |
| CLI compatibility | 60 seeded CSV cases plus CLI safety/batch checks passed |
| C ABI integration | Unicode, preview/export, settings/presets, batch, cancellation and concurrent requests passed |
| Static/quality checks | Format, clippy with warnings denied, Xcode analysis, Actionlint, ShellCheck, Python syntax, generated-file determinism, platform/compliance/dependency checks passed |
| License inventory | 155 resolved Rust registry packages have license declarations; no missing declarations; existing Windows redistribution questions remain unresolved |
| Native UI suite | 10 planned; **not passed**. One contended run canceled. Two runs failed before tests with “Timed out while enabling automation mode,” including a retry after restarting the idle user XCTest service |
| macOS ARM64/x64 build | Normal Release packaging path succeeded with ad-hoc signatures; architecture and nested signatures verified; x64 was cross-built, not natively qualified |
| Packaged ARM64 manual smoke | Launch, displayed 2.1.1, native update fallback, HTML source, disabled accessible update setting, normal quit passed |
| Production packaging | `--notarize --arch arm64` failed at missing updater signing identity/public key before signing/notarization |
| Source hygiene | 56 changed/untracked files reviewed for intended scope; no high-confidence private-key/token patterns found; build/release artifacts remain ignored |

The baseline Codex Security review accounted for 49 original candidate files,
with zero reportable vulnerabilities and no identified unauthenticated-metadata
installation bypass. It is static evidence for the original snapshot, not a
certificate for subsequent fixes or native installation. Rust verifies Ed25519
before parsing trusted metadata; SHA-256 supplies integrity bound by that signature.

The available local Developer ID identity and notarization profile are valid,
but the repository has no configured Actions signing secrets, self-hosted runner
or production publication workflow. The trust configuration is empty. More
fundamentally, the pinned Sparkle integration lacks enforceable pinned-team and
notarization validation at runtime; see [architecture](ARCHITECTURE.md). Its public
delegates do not offer a post-extraction validation/veto hook. This is an
implementation blocker, not merely a missing credential. The six-target metadata
builder also conflicts with the policy allowing independent platform publication.
These gaps were not hidden by fabricated verification receipts or relaxed gates.

VERSION A: **none established**. Historical `v2.1.1` has no updater and cannot
perform this acceptance test. The locally built candidate also has no configured
production update trust and is not an eligible bridge build.
VERSION B: **none published**. No new release tag or GitHub Release exists.
Production signing, notarization, stapling, Gatekeeper acceptance, published
release discovery/download authentication, actual updater installation/relaunch,
data preservation and post-update regression testing were **not completed**.

The exact path **VERSION A → GitHub discovery → authenticated download →
installation → relaunch → VERSION B → passing regression tests did not occur**.
No platform is end-to-end qualified. Windows native tools/runners and Linux GTK
runtime were not exercised here; their signing/packaging/licensing/installation
blockers remain. Xcode emitted non-stripping warnings for already-signed Sparkle
and XCTest binaries; no application compiler warning or Rust lint failure remains.
System developer mode was reported disabled; no system security settings were
changed to work around the UI harness failure. Logs/results are retained under
`build/qualification-*` and are ignored generated output.

## Required acceptance run

On disposable native machines for each architecture, use genuine older/newer
production-equivalent signed builds, with the same app ID and representative
settings/presets. Capture exact versions, commit, OS/arch, release URL, artifact
hash, expected and observed signer, discovery/authentication/download results,
installation result, restart version, settings/data preservation and rollback.
Test permission denial, download loss, disk-full, service outage, installer
interruption and active/unsaved work. Reject downgrade, another app/arch, invalid
signatures, and mismatched packaged versions. Do not publish bogus GitHub stable
releases: mock transport is for protocol tests; use a separate fixture application
and signing identity for native integration tests. Production feed changes require
real stable releases and real post-publication verification.

## Troubleshooting

- “Updates are not configured”: expected in this candidate; provision reviewed
  public trust roots and signed packages. Do not disable verification to proceed.
- Expired metadata: maintainers must publish authenticated fresh metadata within
  the validity policy. Correct a bad local clock. Preserve the rollback floor.
- Network/GitHub error: conversion stays available; automatic retry is hourly,
  successful-check interval daily. Manual checks are available in native menus.
- OS/identity/architecture mismatch: use the correct signed package; never rewrite
  downloaded metadata or lower the minimum OS gate.
- Windows: installer is revealed after verification. Export work, quit, then run
  the interactive installer. Portable folder layouts are not overwritten in place.
- Linux: automatic replacement is unavailable in the current deb/tar packages.
  Install from official GitHub Releases or through the system package manager.
- Sparkle: inspect local macOS Console logs. Source/development app bundles without
  stamped keys do not start the updater. Packaging stamps policy before signing.

No release was published and no production installation was changed by this work.

## Subsequent shared-UI migration validation (2026-10-01)

The native macOS UI harness blocker above was resolved on a fresh run: all 10 UI
tests passed with zero failures/skips in `build/AvaloniaMigration-NativeUITests.xcresult`.
The migration also passed 29 Swift tests and rebuilt both native Mac packages.
These results supersede only the earlier UI-harness/build status; production trust,
release signing/publication and OLD→NEW updater qualification remain blocked.
The internal Avalonia Mac reference is explicitly ineligible as a production
bridge/update artifact. No production release or tag was created.
