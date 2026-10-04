# Building and packaging

This guide records the current build scripts and historical package formats. The target stable release procedure and its unresolved signing work are in [RELEASING.md](RELEASING.md). Test coverage and commands are in [TESTING.md](TESTING.md).

Cargo.toml's workspace version is authoritative. `script/sync_version.py` updates
macOS and Windows metadata without changing the existing Mac build number.
All release platforms use the same commit. Platform minima and dependency ownership
are listed in [DEPENDENCIES.md](DEPENDENCIES.md). See [verification](VERIFICATION.md)
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

Compatibility testing uses Python standard-library code and generated CSV records; tkinter and the archived converter are not required. Core/CLI binaries
do not depend on Python. Cargo.lock is committed; do not regenerate it in release CI.

## macOS

macOS 14+, Swift 6 and Rust. CI uses Xcode 26.6 on ARM64 and 26.3 on Intel. The run/build script selects
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
./script/package.sh --arch arm64
./script/package.sh --arch x64
```

Use a fresh xcresult path per run. XCTest needs a logged-in unlocked desktop and
Xcode automation permission. The deterministic project generator adds a Rust build
phase; regenerate with `python3 script/generate_project.py`. Direct SwiftPM builds
need `build_rust_macos.sh` first. Do not run simultaneous builds that replace the
same `target/swift` archive. Packaging builds one Rust and Swift architecture per
invocation. `--arch arm64` targets Apple Silicon; `--arch x64` targets Intel.
Omitting `--arch` selects the host architecture. Cross-building is available with
the matching Rust target installed, but CI tests each architecture on its own native
runner (`macos-26` with Xcode 26.6 and `macos-15-intel` with Xcode 26.3). Use `arch=x86_64` for Intel XCTest runs.

Outputs per `<arch>` (`arm64` or `x64`): `dist/macos-<arch>/YACHT.app`,
`dist/macos-<arch>/yacht`, `release/YACHT-macos-<arch>.zip`,
`release/yacht-macos-<arch>` and `release/SHA256SUMS-macos-<arch>`.
Choose the ZIP matching the Mac's processor. Each shipped executable is checked
to contain exactly the requested architecture. CLI and checksum filenames remain
unique when release artifacts are combined.
Bundle ID is `com.tlolabs.yacht`. Application Support/Y.A.C.H.T.
and UserDefaults keys are retained. The new bundle ID uses a new UserDefaults
domain; preferences from the previous bundle ID are not automatically migrated. Window/app/HTML display names
use YACHT. Build/run modes: --build-only, --verify, --debug, --logs, --telemetry.

Developer ID/notarization remains optional:

```sh
SIGNING_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
NOTARY_PROFILE='yacht-notary' ./script/package.sh --notarize --arch arm64
```

Default builds (`--unsigned`) are ad-hoc signed and explicitly ignore ambient signing/notary credentials. Use `--sign` for signing without upload, or `--notarize` for explicit notarization. Provision a certificate/private key in an ephemeral
CI keychain or local Keychain; create a notary profile with `xcrun notarytool
store-credentials`. The script signs app/CLI, notarizes/staples the app,
verifies signatures/architectures, then creates checksums. Never commit credentials.
CI intentionally produces development signatures until credentials are configured.

## Windows

Windows 10 1809+, Visual Studio 2022 Build Tools with Windows SDK/C++ desktop tools,
.NET 10 SDK (10.0.401), Rust MSVC toolchain and Inno Setup 6. Microsoft Edge WebView2 Runtime is
needed for preview (normally present on current Windows; install on older hosts).
The UI is shared Avalonia; WebView2 is only the document preview.
The Windows Cargo targets statically link the compiler C runtime. Packaging rejects
undeclared VC runtime DLL imports, including delay imports; Visual Studio is not a
runtime prerequisite. Rebuild with an updated toolchain for compiler-runtime fixes.

```powershell
rustup target add x86_64-pc-windows-msvc aarch64-pc-windows-msvc
./script/package_windows.ps1 -Architecture x64
# On an ARM64 host, use -Architecture arm64.
dotnet build platform/avalonia/Tests/NativeIntegration.csproj -c Release
Copy-Item target/x86_64-pc-windows-msvc/release/yacht_ffi.dll platform/avalonia/Tests/bin/Release/net10.0/
dotnet platform/avalonia/Tests/bin/Release/net10.0/NativeIntegration.dll
./platform/windows/test_ui.ps1 -Executable "$PWD/target/windows-x64/YachtApp.exe"
```

Packaging uses NuGet locked restore and publishes the self-contained .NET runtime
with the app, Rust DLL and CLI. Outputs: per-user Inno installer, portable ZIP and
SHA256SUMS-windows-<arch>. The GUI is YachtApp.exe; yacht.exe is the CLI (Windows
filenames are case-insensitive). Inno registers Open With entries without taking
over the user's default CSV application. Runtime smoke tests need an interactive
Windows desktop and WebView2 runtime. They isolate preference writes.

The package script copies the exact NuGet package license and notice files into
`ThirdPartyLicenses/` before making the ZIP or installer. The historical v2.1.1 ZIPs
did not contain that folder. The new packages contain Avalonia and its open-source rendering dependencies;
retired Windows App SDK binaries are not part of this packaging path.

Set YACHT_SIGNING_THUMBPRINT to a certificate in the signing user's certificate
store and make signtool available to sign/verify app, core DLL, CLI and installer.
Without credentials development packages still build. Import a secret certificate
into an ephemeral CI store before packaging and remove it afterward; no password
or certificate belongs in source. Timestamping uses the signing service configured
in the script.

## Linux

Ubuntu 24.04 x64/ARM64 baseline (glibc 2.39), .NET SDK 10.0.401 for building,
and system WebKitGTK 4.1, GTK3, ICU, Fontconfig and X11 libraries.
`xdg-utils` supplies file-manager and browser integration. GTK3 is only the
native WebView dependency, not an alternative application UI. The .deb declares
runtime dependencies; Python is only needed for build/test tooling.

```sh
sudo apt install libicu74 libfontconfig1 libx11-6 libice6 libsm6 libgtk-3-0t64 \
  libwebkit2gtk-4.1-0 libssl3t64 xdg-utils xvfb dbus-x11 at-spi2-core desktop-file-utils
cargo build --workspace --locked
python3 script/test_native_binding.py target/debug/libyacht_ffi.so
dotnet build platform/avalonia/Tests/NativeIntegration.csproj -c Release
cp target/debug/libyacht_ffi.so platform/avalonia/Tests/bin/Release/net10.0/
dotnet platform/avalonia/Tests/bin/Release/net10.0/NativeIntegration.dll
./script/package_linux.sh
```

CI runs `script/test_avalonia_ui.py` against the published application in an
Xvfb/DBus session. This verifies the native WebView rather than a simulated browser.

WebKit keeps its process sandbox enabled. Ubuntu hosts must permit the distro
`/usr/bin/bwrap` helper to create user namespaces. If startup reports a denied UID
map, check the installed AppArmor/bubblewrap policy; the workflow installs a narrow
helper profile on its ephemeral test hosts. Follow [Ubuntu's application profile
guidance](https://ubuntu.com/blog/ubuntu-23-10-restricted-unprivileged-user-namespaces)
when configuring a restricted workstation; do not disable WebKit's sandbox.

Outputs: `.deb`, tar.gz and SHA256SUMS-linux-<arch>. Tar packages use the same
native dependencies; they are not universal static binaries. The desktop entry
registers CSV/TSV handling and the native launcher. The shared YACHT icon is installed into the hicolor theme and referenced by the desktop entry. Optional YACHT_GPG_KEY signs
the checksum manifest with an already-provisioned GnuPG key; unsigned development
packages remain buildable. Architecture-specific Linux CI runs the code natively.

## Internal macOS ARM64 Avalonia reference

On macOS 15 or newer, install the SDK pinned in `global.json`, then run:

```sh
./script/package_avalonia_internal.sh
python3 script/test_avalonia_ui.py 'dist/internal/YACHT Avalonia Internal.app/Contents/MacOS/YachtApp'
open 'dist/internal/YACHT Avalonia Internal.app'
```

`DOTNET` may point to a repository-local SDK executable. The bundle is ad-hoc signed
for local execution, with no Developer ID or notarization requirement. It has a
separate identity and data store, no production updater, and no Intel target.
Download it from the `YACHT-Avalonia-INTERNAL-macos-arm64-*` workflow artifact.
It is never the production Mac download. See [isolation safeguards](AVALONIA-MIGRATION.md).

## CI and releases

The Native cross-platform workflow builds/tests six native targets on pull requests
and main. Platform checks run separately, and successful development artifacts are
retained for 30 days. A runnable nightly package is intended only for macOS (ARM64);
the other nightly targets may compile/test without packaging.

The previous tag workflow created a draft release from development-signed artifacts.
That is not an official stable release under the current [code signing policy](../CODE_SIGNING_POLICY.md).
Use [RELEASING.md](RELEASING.md) for the target release process and its open
credentials, license, and packaging gates. Do not promote an unsigned development
artifact by renaming it. Legacy packaging is preserved only on the
[archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2).

## App icon

The original vector master is `assets/icons/YACHT.svg`. Its table-grid sail connects
YACHT's name with the CSV-to-HTML workflow. Committed PNG, ICNS and ICO files come
from `script/generate_icons.sh` (macOS, librsvg and ImageMagick). Regeneration is not
required for normal app builds. macOS compiles the layered Icon Composer document
`assets/icons/YACHT.icon` with Xcode 26, including native light, dark and tintable
appearances and a generated ICNS fallback for older macOS. Edit that document in
Icon Composer; the flat export script does not overwrite it. Windows embeds the
ICO in its executable and installer and loads it for the window; Linux
ships the scalable SVG in the hicolor icon theme. All six architectures use the same
artwork. Artwork changes on main appear in subsequent builds; existing tagged
release assets are immutable.
