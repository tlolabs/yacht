# Testing

Run commands from the repository root. A configured CI job is evidence of a test gate, not evidence that it passed on a particular commit. The [verification record](VERIFICATION.md) and [feature parity matrix](FEATURE-PARITY.md) contain the last recorded runs and their limits.

## Shared core

```sh
cargo fmt --all --check
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
python3 script/verify_compatibility.py
python3 script/test_update_release.py
python3 -O script/test_update_release.py
```

The compatibility script creates its own CSV data and compares decoded output. `script/test_native_binding.py` exercises the compiled C ABI; supply the platform's built `yacht_ffi` library path.

## Native targets

- **macOS (ARM64/x64):** Build the Rust library, run `swift test`, then the Xcode `Yacht` scheme's UI tests on a logged-in desktop. The workflow runs each architecture on a native runner. See [building](BUILDING.md#macos) for commands.
- **Windows (x64/ARM64):** Run `platform/avalonia/Tests/NativeIntegration.csproj`, the native binding test, and `platform/windows/test_ui.ps1` on a Windows host with WebView2. The workflow uses native runners for both targets.
- **Linux (x64/ARM64):** Run the native binding test and `script/test_avalonia_ui.py` against the published executable in an Xvfb/DBus session with the documented native WebKitGTK/X11 dependencies installed. The workflow uses Ubuntu 24.04 native runners.

CI should verify generated project/version files, declared minimum OS versions, dependency inventory drift, package contents, and checksums. Dependency-license concerns are reported as warnings; structural errors in licensing files and project SPDX declarations are hard failures.

Before an official stable release, Thomas Lothian should exercise real CSV/preset files, screen readers, keyboard navigation, high contrast and scaling, native dialogs, file manager integration, and install/uninstall on the supported hosts. Record the actual host and result; CI build success does not imply personal hands-on testing.

## Update authentication and installation

`cargo test -p tlo-updater --locked` runs the authenticated manifest/download tests,
local mock release endpoint, outage/interrupted-download tests and release-tool
roundtrip. `python3 script/check_update_contract.py` validates source versions and
security settings; `--production` additionally requires real pinned public trust.
The latter intentionally fails in the current unconfigured candidate.

`Published update authentication` checks real GitHub metadata/downloads without
publishing or installing. Follow the [native qualification ledger](updater/AUDIT-AND-QUALIFICATION.md)
for older-to-newer installation, rollback and data-preservation acceptance.
Passing unit, UI or build tests alone never qualifies an updater installation.

The release-gate fixture suite covers identity/version/key/feed mismatches,
disabled signature flags, wrong architecture, missing hardened runtime, native
signature/notarization/Gatekeeper failures, and Python optimization. These are
mocked policy regressions, not evidence of real notarization or installation.
Swift tests cover stable feed URL/architecture/version selection and active-work
termination/confirmation. Native UI tests also exercise the unconfigured updater
fallback and its disabled, accessible settings control. Never run two desktop UI
suites simultaneously against the same login session; preserve failed/interrupted
runs and rerun after the conflicting session ends.

`./script/prepare_sparkle.sh` authenticates the pinned upstream Sparkle tool
archive. `python3 script/test_sparkle_tools.py build/sparkle/2.9.6/bin` signs and
verifies an offline appcast with a public fixture seed and rejects tampering,
without accessing the login keychain or contacting a release feed.

## Shared Avalonia presentation

Build `platform/avalonia/Tests/NativeIntegration.csproj` with .NET 10.0.401,
copy the host's `yacht_ffi` library beside the test output in `bin/Release/net10.0`,
and run `dotnet .../NativeIntegration.dll`. This preserves the earlier C# ABI
assertions and adds view-model, persistence/migration, validation, cancellation,
batch and lifecycle tests. No graphical session is needed.

`python3 script/test_avalonia_ui.py <published-YachtApp-executable>` isolates user
data and runs the real shared window, native WebView, clipboard, preset, export,
settings and batch smoke path. Windows can also use `platform/windows/test_ui.ps1`.
`script/check_avalonia_contract.py` enforces the shared-target and internal-release
boundary, optionally inspecting `--bundle` for the internal Mac app.
