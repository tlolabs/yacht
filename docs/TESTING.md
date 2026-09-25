# Testing

Run commands from the repository root. A configured CI job is evidence of a test gate, not evidence that it passed on a particular commit. The [verification record](VERIFICATION.md) and [feature parity matrix](FEATURE-PARITY.md) contain the last recorded runs and their limits.

## Shared core

```sh
cargo fmt --all --check
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
python3 script/verify_compatibility.py
```

The compatibility script creates its own CSV data and compares decoded output. `script/test_native_binding.py` exercises the compiled C ABI; supply the platform's built `yacht_ffi` library path.

## Native targets

- **macOS (ARM64/x64):** Build the Rust library, run `swift test`, then the Xcode `Yacht` scheme's UI tests on a logged-in desktop. The workflow runs each architecture on a native runner. See [building](BUILDING.md#macos) for commands.
- **Windows (x64/ARM64):** Run `platform/windows/Tests/NativeIntegration.csproj`, the native binding test, and `platform/windows/test_ui.ps1` on a Windows host with WebView2. The workflow uses native runners for both targets.
- **Linux (x64/ARM64):** Run the native binding test and `platform/linux/test_ui.py` in an Xvfb/DBus session with GTK, libadwaita, and WebKitGTK installed. The workflow uses Ubuntu 24.04 native runners.

CI should verify generated project/version files, declared minimum OS versions, dependency inventory drift, package contents, and checksums. Dependency-license concerns are reported as warnings; structural errors in licensing files and project SPDX declarations are hard failures.

Before an official stable release, Thomas Lothian should exercise real CSV/preset files, screen readers, keyboard navigation, high contrast and scaling, native dialogs, file manager integration, and install/uninstall on the supported hosts. Record the actual host and result; CI build success does not imply personal hands-on testing.
