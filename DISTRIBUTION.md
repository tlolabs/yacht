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
`dist/macos-<arch>/yacht`, `release/YACHT-macos-<arch>.dmg`,
`release/YACHT-macos-<arch>.zip`, `release/yacht-macos-<arch>` and
`release/SHA256SUMS-macos-<arch>`. Both packages install the same `YACHT.app`;
choose the package matching the Mac's processor. Each shipped executable is checked
to contain exactly the requested architecture. CLI and checksum filenames remain
unique when release artifacts are combined.
Bundle ID remains `com.local.yacht.csvhtmltranslator`. Application Support/Y.A.C.H.T.
and UserDefaults keys are intentionally retained. Window/app/HTML display names
use YACHT. Build/run modes: --build-only, --verify, --debug, --logs, --telemetry.

Developer ID/notarization remains optional:

```sh
SIGNING_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
NOTARY_PROFILE='yacht-notary' ./script/package.sh --notarize --arch arm64
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
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1 \
  gir1.2-webkit-6.0 xvfb dbus-x11 at-spi2-core desktop-file-utils
cargo build --workspace --locked
YACHT_LIBRARY="$PWD/target/debug/libyacht_ffi.so" python3 platform/linux/yacht.py
python3 script/test_native_binding.py target/debug/libyacht_ffi.so
YACHT_LIBRARY="$PWD/target/debug/libyacht_ffi.so" \
  xvfb-run -a dbus-run-session -- python3 platform/linux/test_ui.py
./script/package_linux.sh
```

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

## CI and releases

The Native cross-platform workflow runs on PRs, main pushes, nightly schedules,
manual requests and non-v1 version tags. macOS, Windows and Linux each have
separate native x64 and ARM64 jobs: six required platform jobs. Every job
runs the shared Rust/CLI regressions and its native binding/runtime tests before
artifact upload. No platform job is allowed to fail optionally. Uploaded artifact
names include the commit SHA; retention is 30 days.

A draft release depends on every platform job, verifies the tag matches the
workspace version, and publishes packages/checksums plus BUILD_INFO.txt. Development
and nightly artifacts are downloaded from their successful Actions run. Update
Cargo.toml, increment the macOS build number, run version
sync, commit the metadata and lockfiles, pass tests/manual acceptance, then tag
`v<workspace-version>`. CI creates a draft with development signatures. Before publishing a stable release,
build both Mac architectures from the exact tagged commit using `--notarize`,
replace the draft's Mac DMG, ZIP, CLI and checksum assets with the verified signed
outputs, and update its notes to state the signing status. Verify the app and DMG
staples and all release checksums before publishing. Keep signing credentials in
the local Keychain; they are not needed on GitHub. Legacy packaging is preserved only on the
[archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2).

Required human acceptance includes screen readers, keyboard-only navigation,
light/dark/high contrast/scaling, native pickers, file manager behavior and the
owner's real CSV/preset collection. Record actual platform evidence in the parity
matrix before claiming release readiness.

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
