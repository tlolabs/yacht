# Changelog

## Unreleased — shared Avalonia presentation

- Replace WinUI and GTK frontends with shared Avalonia AXAML, commands and state.
- Preserve Rust conversion and native SwiftUI macOS, migrate preferences and tests.
- Add an isolated internal Mac ARM64 reference artifact excluded from production updates.
- Update Windows/Linux packaging, native CI coverage and dependency/license notices.


All notable changes to YACHT from the next release onward will be recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

## [Unreleased]

- Qualification audit: bind accepted update versions/digests, enforce stable macOS
  feed URLs, protect active work on termination, harden production verification,
  pin the release maintainer key, and add failure/recovery tests. Production
  release and real older-to-newer installation remain blocked and unqualified.


### Added

- Candidate authenticated GitHub stable-update library, signed metadata tooling,
  native update controls and Sparkle integration. Production trust configuration,
  Linux installation and fleet consolidation remain open; no platform upgrade is
  end-to-end qualified. See `docs/updater/AUDIT-AND-QUALIFICATION.md`.

### Changed

- Standardize project documentation, contribution policy, privacy and release guidance.

The [Git history](https://github.com/tlolabs/yacht/commits/main/) and [GitHub Releases](https://github.com/tlolabs/yacht/releases) remain the record for earlier versions.
