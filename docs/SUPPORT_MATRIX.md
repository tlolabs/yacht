# Supported platforms

The repository's native project, CI and package scripts establish these targets. The authoritative minimum-version data is in `config/platforms.json`; `script/check_platforms.py` checks relevant build metadata against it. No other platform is claimed.

| Target | Minimum | Evidence | Validation category |
|---|---|---|---|
| macOS (ARM64) | macOS 14.0 | SwiftPM, Xcode project, Info.plist; native macOS CI | Personally tested primarily by Thomas Lothian; CI build/test validated on recorded runs |
| macOS (x64) | macOS 14.0 | Same deployment target; native macOS x64 CI | Production SwiftUI/AppKit validated on CI |
| Windows (x64) | Windows 10 1809 | Qt 6 Widgets and installer; Windows CI | Validated via CMake/Qt build and UI smoke tests |
| Windows (ARM64) | Windows 10 1809 | Same metadata; native Windows ARM64 CI | Validated via CMake/Qt build and UI smoke tests |
| Linux (x64) | Ubuntu 24.04, glibc 2.39; Qt 6 Widgets and X11 | Debian control and Ubuntu CI | Validated via CMake/Qt build and UI smoke tests |
| Linux (ARM64) | Same baseline | Same package dependencies; native Ubuntu ARM64 CI | Validated via CMake/Qt build and UI smoke tests |

See [verification](VERIFICATION.md) for named runs. macOS 14 is a deployment target, not a statement that CI hosts run macOS 14. Linux requirements are a distribution baseline, not an assertion that other distributions cannot work.

Internal macOS ARM64 Qt is a development/reference target only, with a separate
bundle/data identity and workflow artifact. It is not a supported production Mac download.
See [current migration verification](QT-MIGRATION.md).
