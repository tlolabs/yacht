# Current Qt migration validation — 2026-10-07

The presentation layer migration from Avalonia/.NET to Qt 6 Widgets is complete.
Avalonia and .NET have been completely removed from the active repository.

- **Production macOS**: SwiftUI/AppKit (`platform/macos`) validated with native UI and binding tests.
- **Shared Qt 6 Widgets**: Cleanly compiled with CMake and C++17 on Windows, Linux, and macOS ARM64 (`platform/qt`).
- **Internal macOS ARM64 Qt Reference**: Packaged (`script/package_qt_internal.sh`), ad-hoc signed, and verified via `script/test_qt_ui.py` and `script/check_qt_contract.py`.
- **C++ integration tests**: `build/qt/tests/test_integration` verified all Rust C ABI bindings and presentation workflows with 100% pass rate.
- **Compliance and isolation**: Verified via `script/check_qt_contract.py`, `script/check_platforms.py`, `script/dependency_inventory.py --check`, `script/check_compliance.py`, and `script/test_update_release.py`.
See [architecture and complete parity audit](QT-MIGRATION.md).

# Historical Avalonia migration validation — 2026-10-01

The `v2.1.2-rc.1` annotated prerelease tag points to version commit
`4cdfc333a254decc211b5f9743a46730c46014d3`. Its
[tagged native workflow](https://github.com/tlolabs/yacht/actions/runs/36887972162)
passed all nine jobs: both Windows, both Linux, both native macOS, the internal
Avalonia macOS ARM64 reference, repository compliance and prerelease tag policy.
The preceding [branch qualification run](https://github.com/tlolabs/yacht/actions/runs/36884956727)
passed all eight applicable platform/compliance jobs on the same commit.

Downloaded result bundles from the tagged workflow report **10 passed, zero failed
or skipped** for each native macOS architecture. Both tagged macOS app/CLI ZIPs
matched their uploaded SHA-256 manifests; extracted apps reported version `2.1.2`,
bundle ID `com.local.yacht.csvhtmltranslator` and the expected single architecture.
`codesign --verify --deep --strict` passed for both development signatures. The
[ARM64](https://github.com/tlolabs/yacht/actions/runs/36887972162/artifacts/11175493289)
and [Intel](https://github.com/tlolabs/yacht/actions/runs/36887972162/artifacts/11175628225)
packages are workflow artifacts with the repository's normal retention period.
The [internal Avalonia Mac artifact](https://github.com/tlolabs/yacht/actions/runs/36887972162/artifacts/11175049358)
is separately named and remains outside the production release channel.

This is an unsigned, development-signed release candidate: the prerelease tag is
annotated but not cryptographically signed, the apps are not Developer ID signed
or notarized, and the normal workflow did not publish a GitHub Release. Production
trust roots, platform signing, publication and OLD→NEW updater installation remain
blocked. The tag and these artifacts do not qualify the stable update channel.

## Migration implementation qualification

Local Apple Silicon validation and native CI qualification of the migration candidate.
Implementation commit: `c3f3b40dea0707a44d918a81457dfa35303363f0`.
[Final native workflow](https://github.com/tlolabs/yacht/actions/runs/36881027512): **all eight applicable jobs passed**. The tag-only release-policy job was correctly inapplicable to this untagged branch run.

| Check | Result and scope |
|---|---|
| Shared Rust workspace | **VERIFIED**: 35 tests passed, no failures/skips; rustfmt and Clippy with warnings denied passed |
| CSV/CLI regression | **VERIFIED**: 60 seeded round trips plus safety/batch checks; independent ctypes binding suite passed |
| Swift native bindings/update safety | **VERIFIED**: 29 tests passed, no failures |
| Native macOS UI | **VERIFIED**: 10 tests passed, zero failed/skipped on each final native CI runner (ARM64 macOS 26.6.2 and x64 macOS 15.7.9), independently confirmed from both downloaded xcresult bundles. Local ARM64 also passed all 10 tests; `build/AvaloniaMigration-NativeUITests.xcresult` |
| Shared C# binding | **VERIFIED**: preserved conversion, overwrite, cancellation, preset, settings, batch and 20 concurrent-request assertions |
| Shared presentation | **VERIFIED**: 30 assertions on Mac (29 on Windows/Linux; the extra assertion checks internal-only update isolation) for state, commands, presets, migration, cancellation/recovery, active-work guards and preservation of unreadable preferences |
| Preview navigation security | **VERIFIED**: 13 assertions, including exact WebView2 document acceptance and rejection of altered, stale, external, local-file and script navigation |
| Internal Mac ARM64 | **VERIFIED**: clean self-contained Release package; ad-hoc strict signature verification; real native WebView DOM/tab-reattachment/clipboard/open/export/preset/settings/batch/source-tab shutdown smoke passed |
| Shared UI inspection | **VERIFIED within scope**: rendered semantic table and accessible form names inspected; Ctrl+1/2 switching and preview restoration, settings Save/Cancel, preference persistence across restart (50 rows, then restored to 200), and two normal closures exercised |
| Production Mac packaging | **VERIFIED development packaging**: both final native ARM64 and x64 CI jobs built app/CLI packages and passed architecture/nested ad-hoc signature checks. Local ARM64 and cross-built x64 packaging also passed; Intel runtime was exercised on native CI, not locally |
| Windows x64 and ARM64 | **VERIFIED automated scope**: both native CI jobs passed Rust/CLI/ctypes/C# tests, actual WebView2 DOM and clipboard/application/shutdown smoke, and Inno Setup/ZIP packaging. Independently downloaded installer/ZIP checksums verified; all 223 PE images per target passed the native architecture and normal/delay-import audit, including absence of separately installed VC runtime dependencies. Interactive installer acceptance remains manual |
| Linux ARM64 | **VERIFIED automated scope**: native CI tests, actual WebKit DOM/clipboard/application/shutdown smoke and DEB/tar packaging passed. Independently downloaded checksums, four ARM64 ELF payloads, executable permissions and package contents verified |
| Linux x64 | **VERIFIED automated scope**: native CI tests, actual WebKit DOM/tab-reattachment/clipboard/application/source-tab shutdown smoke and DEB/tar packaging passed. Independently downloaded checksums, four x64 ELF payloads, executable permissions and DEB version/architecture/desktop-dependency metadata verified |
| Release safety | **VERIFIED policy tests**: 7 tests passed normally and under Python optimization; internal marker/assembly rejection included; real Sparkle signed-feed fixture roundtrip/tamper rejection passed |
| Repository quality | **VERIFIED**: actionlint, shellcheck, Python compilation, generated project/version checks, license/platform/update/shared-target contracts and dependency inventory |
| NuGet security data | **VERIFIED within scope**: vulnerability query returned no known vulnerable direct/transitive packages |
| Performance | **VERIFIED execution**: existing conversion benchmark completed; no performance acceptance threshold changed |

The exact uploaded [internal Mac artifact](https://github.com/tlolabs/yacht/actions/runs/36881027512/artifacts/11171960158) was independently downloaded, inspected for ARM64 app/core binaries and internal identity, verified with `codesign --verify --deep --strict`, and passed the full native application smoke again locally. It is ad-hoc signed, internal only, and has no production updater.

Qualification found and corrected issues that compilation alone did not reveal:

- WebView2 reports in-memory HTML as a data URL during navigation. An origin-only
  policy blocked the preview; the replacement accepts only the exact current
  CSP-protected document and rejects stale/modified payloads.
- Duplicate initial navigation was removed. Smoke tests now inspect the actual DOM,
  switch between Preview/Source, restore the preview and close from Source.
- GTK browser disposal is asynchronous. Window shutdown now waits for native
  teardown and drains callbacks while the UI dispatcher still runs, including a
  browser already detached by a tab switch.
- Windows test-profile cleanup now waits for the browser's own shutdown and
  preserves the primary test failure if cleanup also fails.
- The Linux package explicitly requires `xdg-utils` for browser/file-manager actions.
- Independent inspection of the initially green Windows packages found an
  unbundled `VCRUNTIME140.dll` dependency in all three Rust binaries. Windows
  compiler-runtime linkage is now static; a hard packaging gate scans normal and
  delay imports and native architectures. The gate detected both pre-fix artifacts,
  and the final downloaded packages passed. Rust allocation/free ownership is
  unchanged; compiler-runtime updates require a rebuild.

The current Rust core, CLI, C ABI implementation and Swift binding source were not
rewritten by the presentation migration. The accumulated updater work from the
preceding task is retained with its explicit qualification limits.

Warnings: native Xcode builds report that already-signed Sparkle components are
not stripped; development builds use ad-hoc signing. Xcode printed debugger-version
diagnostics but its authoritative result bundle reports all 10 UI tests passed.
Existing GitHub Actions also emitted a Node 20 deprecation notice; every applicable
job completed successfully.
One local internal-reference launch failed before application initialization with
Avalonia.Native render-timer error `-6661`; an unchanged retry passed, and both
the CI launch and the final downloaded-artifact launch passed. A working graphical
session remains required for native Mac UI tests. The internal Mac artifact does
not require Developer ID/notarization. These runs do
not qualify production updater installation; trust configuration and OLD→NEW
release evidence are still absent. No stable tag or production release is created.

Manual Windows/Linux screen-reader, scaling, file-manager, installation/uninstallation
and update-installation acceptance remain distinct from automated native CI results. See [architecture and complete parity audit](QT-MIGRATION.md).

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
