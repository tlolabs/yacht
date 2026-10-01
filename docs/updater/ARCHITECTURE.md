# Authenticated desktop updates: YACHT implementation and adoption decision

Status: implementation candidate, 2026-09-30. **No application/platform has been
qualified by an older-to-newer installation in this work.** Do not advertise this
change as a completed fleet migration or production-ready automatic updater.

## Decision

Use an application-independent Rust library, `updater/` (`tlo-updater`), for
signed stable GitHub Release metadata, semantic versions, target selection,
bounded transport, digest verification, update policy and shared tests. The crate
has its own version and no dependency on YACHT's conversion engine or AVID Core.
`crates/yacht-update` supplies YACHT's compiled identity, public trust roots,
installed version, actual Windows OS/Linux glibc version, private state and process
lock. It is a JSON-speaking process adapter for native UI, not an installer.

The protocol builds on ATIV's existing base64/Ed25519 envelope and streamed
SHA-256 verification. It adds signed application/repository identity, exact
versioned URLs, expiry, native signer identity and rollback checks. Existing
ATIV/EnCAP schemas are not silently reinterpreted as this schema.

Use Sparkle 2.9.6 for macOS scheduling, dialogs, authenticated installation and
relaunch. Signed appcasts and release archives use Sparkle's own tooling. This
intentional platform implementation avoids rewriting mature installation code;
all metadata is prepared from the same release version and artifacts. An additional
macOS feed policy requires a newer stable numeric version and the exact versioned
YACHT URL for the running architecture; Rust uses `semver`. macOS termination
passes through the application delegate so active imports/exports/batches veto
termination and pending update restarts require confirmation before discarding work.

**Production trust gap:** pinned Sparkle 2.9.6 verifies its signed feed/archive,
bundle identity and code-signature validity, but a valid EdDSA archive may rotate
the Developer ID identity or use ad-hoc signing. It does not independently enforce
YACHT's pinned team and notarization at installation. Its public delegates have
no post-extraction artifact-path validation/veto hook. Release-time codesign,
stapler and Gatekeeper checks are not a substitute for that runtime requirement.
Do not qualify or enable production updates until an enforceable, reviewed native
validation integration is implemented and tested. No platform protection is disabled.

Use YACHT's existing interactive Inno Setup package for Windows, with Authenticode
validation and an exact expected certificate subject in addition to the signed
SHA-256. The UI downloads and verifies, then reveals the installer and asks the
user to export, quit and run it. Downloads must match both the version and signed
digest offered to the user; a changed release requires checking and confirming again. It never replaces running portable files, kills
processes or launches a downloaded artifact. This is a conservative assisted
installation, not a qualified silent updater. `CloseApplications=no` prevents
Inno Setup from closing user work automatically. Settings remain in LocalAppData.
A full installer-driven restart transaction still requires Windows qualification.

Linux currently has no deployable YACHT AppImage. The shared contract supports
AppImages and verifies their signed metadata/downloads. GTK exposes periodic and
manual checking with a clear manual fallback; it does not replace `.deb`/tar
installations or execute AppImages. Packaging, pinned GPG plus GitHub provenance
verification, and an installation adapter remain implementation work. Do not
mistake their absence for a credential-only blocker.

## Evaluated alternatives

- [Sparkle](https://sparkle-project.org/documentation/) is already used by ATIV and
  EnCAP. Its signed-feed option and verification before extraction fit this task.
- [Velopack](https://docs.velopack.io/packaging/operating-systems/windows) offers
  managed Windows updates, but its documented handling of locked installation
  directories can kill processes. Adopting its layout/lifecycle needs deliberate
  work, so the existing Inno installer is the smaller safe first step for YACHT.
- [AppImageUpdate](https://docs.appimage.org/packaging-guide/optional/updates.html)
  offers established delta/update machinery. Transport integrity is insufficient
  to establish our pinned GPG and GitHub provenance policies. A future adapter
  must stage to a separate file, authenticate before any replacement, retain a
  recovery copy, and be tested on real AppImages. No custom replacement loop is
  introduced here.

## GitHub and request policy

Normal clients fetch `/releases/latest/download/tlo-update.json`, not the REST API.
GitHub's latest-release routing is the stable discovery mechanism; the signed
payload independently requires stable version/channel and rejects draft/prerelease
metadata. Immutable `/releases/download/vVERSION/FILENAME` asset URLs must match
the pinned repository exactly. HTTPS redirects are restricted to GitHub's release
asset hosts, with five redirects maximum, bounded response sizes and timeouts.
No tokens, user IDs, file paths, conversion data or analytics are sent. The shared
client and Sparkle use a fixed `TLO-Updater/1` user agent; Sparkle profiling and
JavaScript are disabled. GitHub still receives ordinary HTTP connection data.

Background checks occur after a successful-check interval of 24 hours and a failed
attempt interval of one hour. Frontends wake the helper at startup and hourly;
the process lock/state prevents repeated launch checks. Manual checks bypass the
interval. Opt-out persists locally. Failures leave conversion usable. There is no
automatic artifact download or unattended installation. Sparkle manages its own
persistent schedule and opt-out under the same 24-hour policy.

## Trust chain and failure behavior

Installed signed application -> compiled public key allowlist/application ID/repo
-> Ed25519 signature over exact manifest bytes -> stable SemVer/expiry/target/OS/
signer/URL validation -> bounded download -> signed size and SHA-256 -> native
platform identity verification -> established native installer/user coordination.

GitHub account access or a modified unsigned checksum file is insufficient to
create an accepted update. A valid signature with another application's ID,
repository or signer is rejected. Signing-key compromise remains a trust-root
compromise; see the recovery procedure. An attacker can withhold updates; local
highest-seen state and signed expiry limit replay without pretending to solve
availability or a compromised local administrator.

Shared downloads use same-directory temporary files and persist without clobbering
existing files only after verification/fsync. Interrupted/corrupt downloads leave
no published staged artifact. Failed staging directories are removed; completed
Windows downloads are retained for seven days and cleaned on later downloads. The helper never chmods/executes a download.
Installation rollback is owned by platform adapters; it is **not yet implemented
or qualified on Linux or tested on Windows**. macOS relies on Sparkle and still
needs a signed older/newer installation test. State corruption produces an error
rather than silently discarding the remembered version floor.

## Adoption and concurrency

Another app can depend on the `tlo-updater` package at a pinned reviewed Git commit
(or move this standalone package to an independent repository without its API
changing). It supplies `Policy`, keeps `VerifiedUpdate` opaque, and delegates
installation behind its own native adapter. Do not copy this library or add an
AVID Core dependency. Public keys must be per-application; do not reuse the test
fixture seeds or another application's private key.

During this work, other active chats began creating different `tlo-updater` crates
in FILLR and EWAF and modifying ATIV/EnCAP. Those are concurrent uncommitted work,
not established shared dependencies. Fleet adoption is pending consolidation into
one source and wire contract. This candidate must not be published as the shared
standard until that coordination is complete. Their files and installed feeds
have not been overwritten by this chat.
