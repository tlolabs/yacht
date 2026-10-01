# Changelog

All notable changes are recorded here. Earlier releases remain in Git history.

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
