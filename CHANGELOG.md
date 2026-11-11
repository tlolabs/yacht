# Changelog

All notable changes are recorded here. Earlier releases remain in Git history.

## [Unreleased]

## [2.1.3] — 2026-11-11

### Added

- Add an HTML output setting on macOS, Windows, and Linux, defaulting to a single table with inline styles for pasting into Canvas. Complete-page output remains available; source, copy, export, and batch follow the setting.
- Add N-Blue, N-Orange, N-Green, N-Red, N-Gray, and N-White table styles, with screenshot-inspired row banding and first-column fills.
- Add eleven Google Sheets-inspired table presets: G-Gray, G-Cyan, G-Green, G-Yellow, G-Orange, G-Blue, G-Teal, G-Blue Gray, G-Brown, G-Light Green, and G-Purple, with colored headers and alternating white/tinted full rows.
- Add `--html-output table|document` to the CLI, retaining its complete-document default.

### Changed

- Name the original built-in table style Y-Default. Existing custom presets and remembered styles remain compatible.
- Reduce inline HTML formatting, parser row allocations, and JSON response copying while preserving exact output and C ABI ownership.
- Avoid rewriting unchanged Qt preview/source documents, preserving selection and Unicode content.
- Require Developer ID signing, Apple notarization acceptance, stapling, and final archive Gatekeeper verification for local distributable macOS packages.

### Fixed

- Reuse blank-row parser scratch storage without retaining unnecessary cell capacity.
- Prevent wide numeric Document responses from doubling serialization buffers beyond the original allocation behavior.
- Invalidate the Qt source cache on document changes and compare exact source text across Unicode normalization cases.

### Known issues

- Production automatic updates remain disabled pending separate trust and installation qualification.
- Large repeated Copy operations can retain substantial allocator address-space/RSS; the optimization evidence does not establish leak freedom.
- SwiftUI GUI test execution and native verification of every supported architecture remain incomplete; see `docs/PERFORMANCE.md` for actual coverage.

## [2.1.2-rc.2] — 2026-10-08

### Added

- Add shared Qt 6 Widgets presentation for Windows x64/ARM64 and Linux x64/ARM64.
- Add internal macOS ARM64 Qt reference application for development, debugging, and parity verification.
- Add Qt integration tests and automated UI smoke test harness (`--ui-smoke-test`).
- Add a package dependency audit for the internal macOS Qt reference app.

### Changed

- Replace the shared Avalonia/.NET presentation layer with Qt 6 Widgets (C++17, CMake) across all shared targets.
- Retain the authoritative Rust core, C ABI bindings, and native SwiftUI/AppKit macOS application.
- Update packaging, compliance, and CI workflows for Qt 6 and CMake.
- Use the native platform appearance when System is selected in the Qt app.

### Removed

- Remove Avalonia and .NET from tracked application source, dependencies, and build inputs.

### Fixed

- Keep Qt responsive during core work and allow in-flight cancellation and retry.
- Preserve existing settings and legacy presets when writes or migration fail.
- Correct Windows ARM64 Qt selection, Windows VC runtime packaging, Linux headless tests, and the macOS UI test's accessibility label.
- Bundle the internal macOS Qt app's runtime dependencies so its archive does not depend on the build machine.

### Known issues

- This prerelease is not production signed. Production update trust and installation remain unqualified.
- The Qt text preview does not implement every browser CSS interaction, and update checks can still block the Qt UI.

## [2.1.2-rc.1] — 2026-10-01

### Added

- Add one shared Avalonia presentation for Windows x64/ARM64 and Linux x64/ARM64, with an isolated internal macOS ARM64 reference artifact.
- Add candidate authenticated GitHub update discovery and native controls. Production update trust and installation remain unqualified.

### Changed

- Preserve the Rust conversion core and native SwiftUI/AppKit macOS application while migrating Windows/Linux preferences, packaging, tests and CI.
- Include audited Avalonia/.NET dependency and license notices in the new packages.

### Fixed

- Reject altered or stale preview navigation, await native browser teardown, and restore preview content after switching tabs.
- Include the Linux desktop launcher dependency and remove Windows native binaries' dependence on a separately installed VC runtime.
- Tighten update offer binding, release verification and active-work protection without enabling the production updater.

The [Git history](https://github.com/tlolabs/yacht/commits/main/) and [GitHub Releases](https://github.com/tlolabs/yacht/releases) remain the record for earlier versions.
