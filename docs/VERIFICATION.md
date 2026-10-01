# Current Avalonia migration validation — 2026-10-01

Local Apple Silicon validation of the migration candidate:

| Check | Result and scope |
|---|---|
| Shared Rust workspace | **VERIFIED**: 35 tests passed, no failures/skips; rustfmt and Clippy with warnings denied passed |
| CSV/CLI regression | **VERIFIED**: 60 seeded round trips plus safety/batch checks; independent ctypes binding suite passed |
| Swift native bindings/update safety | **VERIFIED**: 29 tests passed, no failures |
| Native macOS UI | **VERIFIED**: 10 tests passed, zero failed/skipped; `build/AvaloniaMigration-NativeUITests.xcresult` |
| Shared C# binding | **VERIFIED**: preserved conversion, overwrite, cancellation, preset, settings, batch and 20 concurrent-request assertions |
| Shared presentation | **VERIFIED**: 30 assertions for state, commands, presets, migration, cancellation/recovery, active-work guards and preservation of unreadable preferences |
| Internal Mac ARM64 | **VERIFIED**: clean self-contained Release package; ad-hoc strict signature verification; real native WebView/clipboard/open/export/preset/settings/batch smoke passed |
| Shared UI inspection | **VERIFIED within scope**: rendered semantic table and accessible form names inspected; Ctrl+2 source switching, settings/cancel and normal closure exercised |
| Production Mac packaging | **VERIFIED development packaging**: native ARM64 and cross-built x64 app/CLI ZIPs; architecture and nested ad-hoc signature checks passed. Intel runtime was not exercised locally |
| Production Windows/Linux | **NOT VERIFIED natively locally**: all four self-contained managed targets cross-published; Rust native libraries, installed packages and UI must pass their native CI jobs |
| Release safety | **VERIFIED policy tests**: 7 tests passed normally and under Python optimization; internal marker/assembly rejection included; real Sparkle signed-feed fixture roundtrip/tamper rejection passed |
| Repository quality | **VERIFIED**: actionlint, shellcheck, Python compilation, generated project/version checks, license/platform/update/shared-target contracts and dependency inventory |
| NuGet security data | **VERIFIED within scope**: vulnerability query returned no known vulnerable direct/transitive packages |
| Performance | **VERIFIED execution**: existing conversion benchmark completed; no performance acceptance threshold changed |

The current Rust core, CLI, C ABI implementation and Swift binding source were not
rewritten by the presentation migration. The accumulated updater work from the
preceding task is retained with its explicit qualification limits.

Warnings: native Xcode builds report that already-signed Sparkle components are
not stripped; development builds use ad-hoc signing. Xcode printed debugger-version
diagnostics but its authoritative result bundle reports all 10 UI tests passed.
The internal Mac artifact does not require Developer ID/notarization. These runs do
not qualify production updater installation; trust configuration and OLD→NEW
release evidence are still absent. No tag or production release is created.

Windows/Linux native CI and manual screen-reader, scaling, file-manager,
installation/uninstallation and update-installation acceptance remain distinct
from these local results. See [architecture and complete parity audit](AVALONIA-MIGRATION.md).

# Historical verification

## Repository cleanup and app icon

The macOS frontend, binding tests, UI tests and support files are grouped under
`platform/macos`. Root Xcode and SwiftPM projects remain the build entry points.
All legacy Python fixture data and captured HTML files have been removed, including
stale fixture copies in the local SwiftPM build directory. The current Linux GTK
adapter and Python build/test tools remain part of the Rust/native implementation.

Current coverage uses Rust/Swift document-contract assertions and 60 generated CSV
round trips instead of archived output. It covers UTF-8/BOM, delimiters, quoted and
multiline cells, ragged rows, escaping, styles, presets, atomic export, cancellation,
batch behavior and bounded previews. Historical exact-output comparisons remain
available in Git history at `v2.1.0`.

Local checks after the cleanup include eight Rust behavior groups, the C ABI test,
Clippy with warnings denied, 25 Swift binding tests, and 60 CLI round trips plus
style, safety and batch checks. All eight local macOS UI tests passed with no skips;
the built app contains the intended ICNS resource and passes signature verification.
The native icon is supplied as macOS ICNS, Windows
multi-resolution ICO and Linux SVG from one vector master. See
[building instructions](BUILDING.md#app-icon) for regeneration.

The [Native cross-platform workflow](https://github.com/tlolabs/yacht/actions/workflows/native-macos.yml)
validates six independent native builds on every main push:

| Distribution | Native runner | Required coverage |
|---|---|---|
| macOS ARM64 | macos-26 / Xcode 26.6 | Rust, Swift, CLI, ctypes, eight UI tests, DMG/ZIP/CLI |
| macOS Intel x64 | macos-15-intel / Xcode 26.3 | Rust, Swift, CLI, ctypes, eight UI tests, DMG/ZIP/CLI |
| Windows x64 | windows-2025 | Rust, CLI, C#, ctypes, WinUI runtime, installer/ZIP |
| Windows ARM64 | windows-11-arm | Rust, CLI, C#, ctypes, WinUI runtime, installer/ZIP |
| Linux x64 | ubuntu-24.04 | Rust, CLI, ctypes, GTK runtime, DEB/tar |
| Linux ARM64 | ubuntu-24.04-arm | Rust, CLI, ctypes, GTK runtime, DEB/tar |

Mac packages contain exactly their requested architecture. Intel uses macOS 15
because the macOS 26 Intel hosted image crashed in the system icon service's Metal
initialization. The deployment minimum remains macOS 14. Finder UI tests start the
app normally through Launch Services and then attach XCTest, avoiding the stopped
launch state retained by the older debugger. Both selected files, both exported
contents and a later single-file open in the same window are asserted.

## Published 2.1.0 baseline

[The tagged 2.1.0 run](https://github.com/tlolabs/yacht/actions/runs/35124765389)
at original commit `51c1c40` (rewritten as `bfcf06c14339`) passed all six
platform jobs and verified six manifests covering 14 package/CLI checksums before publication. The release has separate macOS Intel
and ARM64 downloads. These released binaries predate the repository cleanup and new
icon described above; their source tree is unchanged by the unsigned-commit migration.
The original release commit remains in the migration backup; see
[commit signing and history](DEVELOPMENT.md#commit-signing-and-history).

The [2.0.2 archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2)
at original commit `d8a64ff` (rewritten as `57f6b3038936`) preserves the former
application and migration history. Removing archived data from the active tree does not remove existing users' preset migration support.

## Scope and distribution limits

Automated checks do not establish full screen-reader, high-contrast/scaling,
color-picker, drag/drop or real teaching-dataset acceptance on every host. The
[parity matrix](FEATURE-PARITY.md) distinguishes those manual checks.

Default packages are macOS ad-hoc signed and Windows/Linux unsigned. No Developer
ID, notarization or other signing credentials were used.
