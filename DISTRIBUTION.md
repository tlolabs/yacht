# Building, testing and distribution

Cargo.toml's workspace version is authoritative. `script/sync_version.py` updates
macOS and Windows metadata without changing the existing Mac build number.
All release platforms use the same commit. Platform minima and dependency ownership
are listed in [DEPENDENCIES.md](DEPENDENCIES.md). See [verification](docs/VERIFICATION.md)
for what was actually executed locally; a configured CI job is not proof it passed.

## Shared Rust and CLI

Install Rust through rustup; rust-toolchain.toml pins the compiler and components.

```sh
cargo test --workspace --locked
cargo fmt --all --check
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --release --workspace --locked
cargo bench -p yacht-core --bench conversion
python3 script/verify_compatibility.py
```

Python differential testing uses the preserved Tk-based reference module, so a
Python installation with tkinter is required for that test only. Core/CLI binaries
do not depend on Python. Cargo.lock is committed; do not regenerate it in release CI.

## macOS

macOS 14+, Xcode 26.6 (CI)/Swift 6, and Rust. The run/build script selects
`/Applications/Xcode.app/Contents/Developer` unless DEVELOPER_DIR is set. It does not
change machine-wide xcode-select. Rust is linked statically, so the shipped app
requires no separate Rust installation/library.

```sh
rustup target add aarch64-apple-darwin x86_64-apple-darwin
./script/build_and_run.sh --verify
./script/build_rust_macos.sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer swift test
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcodebuild \
  -project Yacht.xcodeproj -scheme Yacht -derivedDataPath build \
  -destination 'platform=macOS,arch=arm64' -resultBundlePath build/UITests.xcresult test
./script/package.sh
```

Use a fresh xcresult path per run. XCTest needs a logged-in unlocked desktop and
Xcode automation permission. The deterministic project generator adds a Rust build
phase; regenerate with `python3 script/generate_project.py`. Direct SwiftPM builds
need `build_rust_macos.sh` first. Do not run simultaneous builds that replace the
same `target/swift` archive. Universal packaging builds both Rust architectures and
both Swift architectures before linking/signing.

Outputs: `dist/YACHT.app`, `dist/yacht`, `release/YACHT-macos-universal.dmg`,
`release/YACHT-macos-universal.zip`, `release/yacht`, `release/SHA256SUMS-macos`.
Bundle ID remains `com.local.yacht.csvhtmltranslator`. Application Support/Y.A.C.H.T.
and UserDefaults keys are intentionally retained. Window/app/HTML display names
use YACHT. Build/run modes: --build-only, --verify, --debug, --logs, --telemetry.

Developer ID/notarization remains optional:

```sh
SIGNING_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
NOTARY_PROFILE='yacht-notary' ./script/package.sh --notarize
```

Default builds (`--unsigned`) are ad-hoc signed and explicitly ignore ambient signing/notary credentials. Use `--sign` for signing without upload, or `--notarize` for explicit notarization. Provision a certificate/private key in an ephemeral
CI keychain or local Keychain; create a notary profile with `xcrun notarytool
store-credentials`. The script signs app/CLI, notarizes/staples the app and DMG,
verifies signatures/architectures, then creates checksums. Never commit credentials.
CI intentionally produces development signatures until credentials are configured.

## Windows

Windows 10 1809+, Visual Studio 2022 Build Tools with Windows SDK/C++ desktop tools,
.NET 8 SDK, Rust MSVC toolchain and Inno Setup 6. Microsoft Edge WebView2 Runtime is
needed for preview (normally present on current Windows; install on older hosts).
Native controls are WinUI 3; WebView2 is only the document preview.

```powershell
rustup target add x86_64-pc-windows-msvc aarch64-pc-windows-msvc
./script/package_windows.ps1 -Architecture x64
# On an ARM64 host, use -Architecture arm64.
dotnet build platform/windows/Tests/NativeIntegration.csproj -c Release
Copy-Item target/x86_64-pc-windows-msvc/release/yacht_ffi.dll platform/windows/Tests/bin/Release/net8.0/
dotnet platform/windows/Tests/bin/Release/net8.0/NativeIntegration.dll
./platform/windows/test_ui.ps1 -Executable "$PWD/target/windows-x64/YachtApp.exe"
```

Packaging uses NuGet locked restore and publishes the .NET/Windows App SDK runtime
with the app, Rust DLL and CLI. Outputs: per-user Inno installer, portable ZIP and
SHA256SUMS-windows-<arch>. The GUI is YachtApp.exe; yacht.exe is the CLI (Windows
filenames are case-insensitive). Inno registers Open With entries without taking
over the user's default CSV application. Runtime smoke tests need an interactive
Windows desktop and WebView2 runtime. They isolate preference writes.

Set YACHT_SIGNING_THUMBPRINT to a certificate in the signing user's certificate
store and make signtool available to sign/verify app, core DLL, CLI and installer.
Without credentials development packages still build. Import a secret certificate
into an ephemeral CI store before packaging and remove it afterward; no password
or certificate belongs in source. Timestamping uses the signing service configured
in the script.

## Linux

Ubuntu 24.04 x64/ARM64 baseline (glibc 2.39). GTK 4.10+, libadwaita 1.4+, WebKitGTK
6.0, Python 3 and PyGObject. The .deb declares these dependencies.

```sh
sudo apt install python3-gi python3-tk gir1.2-gtk-4.0 gir1.2-adw-1 \
  gir1.2-webkit-6.0 xvfb dbus-x11 desktop-file-utils
cargo build --workspace --locked
YACHT_LIBRARY="$PWD/target/debug/libyacht_ffi.so" python3 platform/linux/yacht.py
python3 script/test_native_binding.py target/debug/libyacht_ffi.so
YACHT_LIBRARY="$PWD/target/debug/libyacht_ffi.so" \
  dbus-run-session -- xvfb-run -a python3 platform/linux/test_ui.py
./script/package_linux.sh
```

Outputs: `.deb`, tar.gz and SHA256SUMS-linux-<arch>. Tar packages use the same
native dependencies; they are not universal static binaries. The desktop entry
registers CSV/TSV handling and the native launcher. A system spreadsheet icon is
used because the repository had no custom icon asset. Optional YACHT_GPG_KEY signs
the checksum manifest with an already-provisioned GnuPG key; unsigned development
packages remain buildable. Architecture-specific Linux CI runs the code natively.

## CI and releases

The Native cross-platform workflow runs on PRs, main pushes, nightly schedules,
manual requests and non-v1 version tags. macOS builds/tests/packages universal
binaries. Windows has x64 and ARM64 jobs; Linux has x64 and ARM64 jobs. Every job
runs the shared Rust/CLI regressions and its native binding/runtime tests before
artifact upload. No platform job is allowed to fail optionally. Uploaded artifact
names include the commit SHA; retention is 30 days.

Stable publication depends on every platform job, verifies the tag matches the
workspace version, and publishes packages/checksums plus BUILD_INFO.txt. Development
and nightly artifacts are downloaded from their successful Actions run; no new
stable release/tag is created by this migration. Update Cargo.toml, run version
sync, commit the metadata and lockfiles, pass tests/manual acceptance, then tag
`v<workspace-version>`. Credentials are optional; release notes must accurately
state the signing state. The legacy Python workflow remains manual/v1-tag only.

Required human acceptance includes screen readers, keyboard-only navigation,
light/dark/high contrast/scaling, native pickers, file manager behavior and the
owner's real CSV/preset collection. Record actual platform evidence in the parity
matrix before claiming release readiness.
