# Building and distribution

## Requirements

- macOS 14+ to run the app.
- Xcode 26.6 / Swift 6.3.3 for the current verified build; CI pins that stable toolchain.
- Python 3 for regression comparisons and optional Xcode project regeneration; Python is not required by the shipped native app.
- No third-party Swift package dependencies.

The local scripts use `/Applications/Xcode.app/Contents/Developer` when `DEVELOPER_DIR` is unset, without changing the machine-wide xcode-select setting.

## Build, run and test

```sh
./script/build_and_run.sh --verify
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
swift test
python3 script/verify_compatibility.py
xcodebuild -project Yacht.xcodeproj -scheme Yacht -derivedDataPath build \
  -destination 'platform=macOS,arch=arm64' -resultBundlePath build/UITests.xcresult test
```

Use a fresh result-bundle path for each run. UI tests require an unlocked, logged-in desktop and Xcode automation permission. Test-runner hardened runtime is disabled for ad-hoc XCTest hosting; the production signing script enables hardened runtime when using a Developer ID identity.

`script/generate_project.py` regenerates the checked-in Xcode project from app/UI-test Swift files. CI checks it produces no diff. `Package.swift` is authoritative for core and CLI targets.

## Package

```sh
./script/package.sh
```

Builds an optimized universal ARM64/x86_64 app and CLI, verifies architecture and code signatures, and produces:

- `dist/Y.A.C.H.T..app`
- `dist/yacht`
- `release/Y.A.C.H.T.-macos-universal.dmg`
- `release/Y.A.C.H.T.-macos-universal.zip`
- `release/yacht`
- `release/SHA256SUMS`

The DMG includes an Applications shortcut, CLI, README and license. App identity remains `com.local.yacht.csvhtmltranslator`; the current native version is 2.0.1 (build 3). No icon asset existed in the Python repository, so the native build currently uses the system application icon.

## Signing and notarization

Default local and CI builds use ad-hoc signatures and are **not notarized**. They are testable development artifacts, not trusted public distribution packages. No valid signing identity or repository signing secret was provided during the rewrite.

When a Developer ID Application certificate and private key are installed in Keychain, run:

```sh
SIGNING_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
NOTARY_PROFILE='yacht-notary' ./script/package.sh
```

Create the notary profile with `xcrun notarytool store-credentials yacht-notary` using your own Apple developer account/API credentials. Never commit credentials. The packaging script signs the app/CLI with hardened runtime and timestamps, submits and staples the app, builds/signs/notarizes/staples the DMG, verifies the app signature and both CPU architectures, then writes checksums. `NOTARY_PROFILE` requires Developer ID signing.

CI deliberately does not claim notarization or require credentials to produce development builds. If automated trusted releases are desired, add a dedicated signing workflow using repository/environment secrets and an ephemeral keychain. Public release assets should accurately identify their signing state.

## GitHub Actions

`Native macOS` runs for pushes to main, pull requests, manual runs and version tags. It verifies the project, runs core tests, CLI/Python differential tests and macOS UI tests, builds universal packages, and uploads downloadable artifacts with 30-day retention. UI test result bundles are uploaded even on failure. CI uses the standard public `macos-26` runner, not a paid larger runner.

Tagged releases publish only after all required verification/package steps pass. Update `Support/Info.plist`, commit/push on main, then create a new native version tag (for example `v2.0.0`) when manually accepted. The current rewrite does not create a release tag or imply acceptance.

The legacy Python workflow remains available for manual cross-platform builds and `v1*` tags. Its preserved original is under `legacy-python/.github/`. Native release tags should be `v2*` or later to avoid mixing legacy and native assets.

## Updating and future distribution

There was no automatic update mechanism to preserve. Updates are currently downloaded from GitHub. A future updater and sandbox/bookmark work for a Mac App Store edition are tracked as [updater](https://github.com/tlolabs/yacht/issues/1) and [sandbox](https://github.com/tlolabs/yacht/issues/3) follow-up issues rather than adding an unconfigured service to this rewrite.
