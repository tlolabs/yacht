# Verification

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
[distribution instructions](../DISTRIBUTION.md#app-icon) for regeneration.

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
at commit `51c1c40` passed all six platform jobs and verified six manifests covering
14 package/CLI checksums before publication. The release has separate macOS Intel
and ARM64 downloads. These released binaries predate the repository cleanup and new
icon described above; their immutable source is available under the release tag.

The [2.0.2 archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2)
at `d8a64ff` preserves the former application and migration history. Removing archived
data from the active tree does not remove existing users' preset migration support.

## Scope and distribution limits

Automated checks do not establish full screen-reader, high-contrast/scaling,
color-picker, drag/drop or real teaching-dataset acceptance on every host. The
[parity matrix](FEATURE-PARITY.md) distinguishes those manual checks.

Default packages are macOS ad-hoc signed and Windows/Linux unsigned. No Developer
ID, notarization or other signing credentials were used.
