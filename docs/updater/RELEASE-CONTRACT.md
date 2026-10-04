# GitHub stable release contract (candidate schema 1)

Canonical repository: `tlolabs/yacht`. Stable tag: `vMAJOR.MINOR.PATCH`.
`Cargo.toml` workspace version is authoritative. `sync_version.py` writes both
macOS bundle versions, Windows props and Windows assembly version; packaged core
and helper use Cargo's compile-time version. `check_update_contract.py` rejects
source drift, and the verified stable tag gate invokes its production trust check.

## Assets

Every update-capable stable release needs all six verified production packages,
`tlo-update.json`, `SHA256SUMS`, signed `appcast-macos-x64.xml` and
`appcast-macos-arm64.xml`, platform verification evidence, SBOM/licenses, and
GitHub artifact attestations. Linux additionally requires a detached GPG signature
for each AppImage. Checksums and attestation do not replace platform signing.
Do not overwrite assets at a published version; fix a broken release with a newer
version. No CI/nightly/branch artifacts are eligible for this channel.

`updater/src/lib.rs` is the executable schema; structs deny unknown and duplicate
fields. JSON integers below are Unix UTC seconds. The envelope is:

```json
{"key_id":"yacht-2026-01","payload":"BASE64_EXACT_UTF8_JSON_BYTES","signature":"BASE64_ED25519_SIGNATURE"}
```

The authenticated payload has these fields:

| Field | Meaning |
|---|---|
| `schema` | Integer `1` |
| `application_id` | `com.tlolabs.yacht` |
| `repository` | `tlolabs/yacht` |
| `channel` | `stable` |
| `version`, `tag` | Strict stable SemVer and exactly `v` plus version |
| `draft`, `prerelease` | Both false |
| `published_at`, `expires_at` | UTC epoch seconds; expiry after publication, within 366 days; generator uses 180 days |
| `notes_url` | Exactly the GitHub release tag URL |
| `restart_required` | Boolean |
| `migration` | `none` or `manual`; unknown policies rejected |
| `assets` | Unique platform/arch/format entries |

Each asset has `platform` (`macos`, `windows`, `linux`), `arch` (`x64`, `arm64`),
`min_os` (three numeric components), `format`, safe basename `filename`, exact
versioned GitHub `url`, positive `size` (maximum 4 GiB), lowercase hexadecimal
`sha256`, and `native_identity`. Minimum OS is macOS version, Windows build version,
or Linux **glibc ABI** respectively. Formats are `sparkle-zip`, `inno-setup` and
`appimage`. Identity is Apple Team ID, exact Authenticode certificate subject, or
pinned GPG fingerprint respectively. These are application policy values, not
identities accepted merely because an untrusted manifest names them.

Manifest signature is Ed25519 over decoded `payload` bytes. JSON is not parsed or
re-serialized for signature verification. Key IDs select only committed public
keys. Limits apply before parsing, transport never follows arbitrary asset hosts,
and semantic version ordering uses `semver`, including `1.10.0 > 1.9.0`.

## Signing and release procedure

1. Configure public roots/identities in `config/update-trust.json`; commit only
   public material. Thomas Lothian owns and approves release/signing keys. Protect
   private keys in managed CI secrets or an ephemeral local keychain. No new paid
   service is required by this design; existing platform signing policy remains.
2. Bump the Cargo version; run sync, native generation, source contract checks,
   locked Rust/native tests. Create the authorized signed stable tag. The current
   tag checker verifies GitHub's annotated-tag signature and checked-out commit;
   signer-to-maintainer enforcement still relies on repository release controls.
3. Build all six targets from that commit. Developer ID-sign nested macOS code,
   hardened runtime, notarize/staple/verify. Use the project's Azure Artifact
   Signing policy on Windows and verify shipped native binaries and installer.
   Build real Linux AppImages, GPG-sign and attest them. Current CI does not yet
   automate all these production steps; missing credentials cannot fall back to
   unsigned production artifacts.
4. On each native runner, verify actual app identity/version/architecture and
   platform signatures. Produce `<artifact>.verification.json` binding
   `application_id`, `version`, `platform`, `arch`, `sha256`, `native_identity`, and
   `native_verified: true`. For macOS also retain codesign, stapler and Gatekeeper
   logs; for Windows Authenticode evidence; for Linux GPG fingerprint and verified
   GitHub provenance. This evidence producer is still missing for Windows/Linux;
   do not fabricate reports to satisfy the metadata gate.
5. On macOS obtain pinned Sparkle tools with `bash script/prepare_sparkle.sh`.
   Supply `RELEASE_TAG`, read-only `GH_TOKEN`, `GH_REPO`, `TLO_UPDATE_KEY_ID`,
   `TLO_UPDATE_PRIVATE_KEY` (base64 32-byte seed), and `SPARKLE_PRIVATE_KEY` in the
   approved secret environment. Sparkle receives its key through stdin, not argv.
   Run `python3 script/build_update_metadata.py <assets> --sparkle-tools
   <pinned-Sparkle-directory>/bin`. The command verifies the signed tag/version,
   all six evidence records/digests, and delegates feed signing to Sparkle. The
   generic `tlo-release` sign/verify tool can be used by other apps; it checks
   cryptography/digests, not native signing or release authority by itself.
6. Publish only those verified artifacts and metadata to the same stable GitHub
   Release, after native installation qualification. No publication was performed
   in this change. The existing native workflow still builds development artifacts;
   full production publication automation remains open.
7. The `Published update authentication` workflow discovers/downloads/verifies all
   six assets through the real latest endpoint from an older version. It has only
   read permissions. It checks authentication, **not native installation**. Record
   the separate older-to-newer installation evidence before calling a release
   qualified. An older non-latest release cannot pass this latest-channel check.

## Key rotation and recovery

Ship a bridge release trusted by the old manifest key containing old and new
public keys; confirm older clients can install it, then begin signing with the
new key. Retain the bridge and old feed compatibility for still-installed clients.
Do not silently replace a pinned key. Per-app keys limit a compromise's scope.

Use Sparkle's documented EdDSA/Developer ID rotation rules; never change both
trust anchors in the same step. Signed feeds and pre-extraction verification are
mandatory here; signed-feed failure expiry is zero, so the updater will not
silently accept an unsigned feed after a timeout. If no safe authenticated bridge
is possible (lost/revoked old signing authority), require a manually installed,
properly native-signed replacement and communicate the recovery out of band.
No Gatekeeper/Authenticode bypass, curl-to-shell recovery or unsigned emergency
feed is permitted. Test rotation with older production-equivalent clients.

## Qualification audit gate (2026-10-01)

Production remains blocked. The macOS runtime policy gap is described in
[ARCHITECTURE.md](ARCHITECTURE.md); filling in public keys alone does not close it.
Stable tags must pass both GitHub verification and local OpenPGP verification
against `config/release-maintainer.asc` (fingerprint
`F7E74ED98DB485D03F2565B96B68B73FE752FD16`). Only public key material is committed.
Release assertions have been replaced with unconditional checks, including when
Python optimization is enabled. Verification receipts are digest-bound build
outputs, not independently authenticated attestations; never accept a receipt from
an untrusted job or upload in lieu of re-verifying the native artifact.

There is still no normal production publishing workflow in this repository.
The metadata builder currently requires all six targets, while policy allows a
successful platform to publish independently. That mismatch must be resolved in
the reviewed production pipeline with an explicit supported-target list and honest
missing-platform release notes; do not remove gates or fabricate six-platform
receipts to publish a macOS test release. No new stable tag is authorized by this
qualification record.
