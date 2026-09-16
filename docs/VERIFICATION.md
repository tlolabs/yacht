# YACHT 2.1.0 verification

Verification date: September 16, 2026. Application/test candidate: `da81a5d`.
Local host: Apple Silicon Mac, macOS 26.7, Xcode 26.6 and Rust 1.98.1.

## Native platform gates

[Release-candidate CI run](https://github.com/tlolabs/yacht/actions/runs/35123568911)
executes the Rust core and CLI, native bindings, actual native UI workflows and
packaging on each architecture. All six candidate jobs passed. Publication
requires all six jobs to pass again for the release tag, using one commit and version.

| Platform | Native runner | Candidate result |
|---|---|---|
| macOS ARM64 | macos-26 / Xcode 26.6 | Passed |
| macOS Intel x64 | macos-15-intel / Xcode 26.3 | Passed |
| Windows x64 | windows-2025 | Passed |
| Windows ARM64 | windows-11-arm | Passed |
| Linux x64 | ubuntu-24.04 | Passed |
| Linux ARM64 | ubuntu-24.04-arm | Passed |

Intel CI uses macOS 15/Xcode 26.3 because the macOS 26 Intel hosted image crashed
in the system icon service's Metal initialization, blocking AppKit accessibility.
The complete test suite and native x64 packaging remain required. The deployment
minimum remains macOS 14.

The macOS UI tests account for macOS 15's duplicate toolbar accessibility wrappers
and scope preset confirmation to the alert sheet. The Finder test launches YACHT
normally through Launch Services and then attaches XCTest. This avoids the stopped
launch state left by the macOS 15 debugger: system logs showed Launch Services
withholding document events from that state. The test still opens two files,
checks both batch paths and exported contents, then opens a single file and checks
that it appears in the same window.

## Coverage

- Rust: eight behavior groups and the C ABI ownership/error test cover strict
  CSV and chunking, BOM/Unicode/newlines, sparse rows, malformed input, frozen HTML
  snapshots, styles/escaping, presets, atomic export, cancellation, batch and
  bounded previews. Formatting and Clippy run with warnings denied.
- Swift: all 25 binding tests run against the Rust library, including retained
  CSV/HTML/file/preview cases and shared settings/TSV batch behavior.
- CLI: 60 seeded CSV cases compare decoded cells against both original records
  and frozen legacy output. Input hashes prevent silent changes to the corpus.
  Additional checks cover all sixteen style flags, ordered Unstyled reset,
  dash-prefixed paths, overwrite/input protection and mixed-success batches.
- Native bindings: ctypes and C# exercise Unicode paths, opening, metadata,
  preview/source, complete export, replacement safety, presets/settings, batch,
  cancellation and concurrent requests.
- macOS: eight UI cases exercise sample/source/copy/reset, escaped and malformed
  imports, native Open/Save, preset save/load/delete, batch collision protection,
  Finder multiple-file opening and appearance/preview settings.
- Windows: native WinUI startup, file opening, preview/source, style changes,
  presets, clipboard, export, settings and batch run before artifact upload.
- Linux: real GTK workflows exercise startup, all sixteen controls, file loading,
  preview/source, presets, clipboard, export, settings and batch. WebKit's process
  sandbox remains enabled.
- Packaging: each macOS executable is checked for exactly its requested CPU
  architecture and a valid ad-hoc signature. Windows produces per-user installers
  and portable ZIPs; Linux validates desktop entries and DEB/tar packages. Every
  architecture has its own SHA-256 manifest.

After archival cleanup, local Rust tests, formatting, Clippy, all 25 Swift tests,
60 seeded CLI cases and ctypes tests passed. All eight local macOS UI tests passed
with no skips after the launch correction. The local native app rebuilt as version
2.1.0, macOS build 5. Generated Xcode and version metadata were checked.

## Archive and compatibility provenance

The [2.0.2 archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2)
at `d8a64ff` preserves the old Tk application, packaging, migration records and
previous Swift implementation in its history. The owner authorized archival and
removal from the active tree. Current GTK Python presentation code remains.

The 60 seeded legacy results were captured and compared with Rust before removing
the converter. `Tests/YachtCoreTests/Fixtures/seeded-legacy-cells.json` records the
reference commit/path/source hash, seed and per-input hashes. Existing exact HTML
fixtures also remain. Compatibility tests no longer import legacy code or need Tk.

## Scope and distribution limits

Automated coverage does not establish full screen-reader, high-contrast/scaling,
color-picker, drag/drop or real teaching-dataset acceptance on every host. The
[parity matrix](FEATURE-PARITY.md) distinguishes those manual checks.

Packages use the documented default signing state: macOS ad-hoc signed; Windows
and Linux unsigned. No Developer ID, notarization or other signing credentials
were used. See [distribution instructions](../DISTRIBUTION.md).
