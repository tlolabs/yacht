# Supported platforms

The repository's native project, CI and package scripts establish these targets. The authoritative minimum-version data is in `config/platforms.json`; `script/check_platforms.py` checks relevant build metadata against it. No other platform is claimed.

| Target | Minimum | Evidence | Validation category |
|---|---|---|---|
| macOS (ARM64) | macOS 14.0 | SwiftPM, Xcode project, Info.plist; native macOS CI | Personally tested primarily by Thomas Lothian; CI build/test validated on recorded runs |
| macOS (x64) | macOS 14.0 | Same deployment target; native macOS x64 CI | Previous frontend validated on historical runs; new Avalonia native acceptance pending |
| Windows (x64) | Windows 10 1809 | .NET target/minimum and installer; Windows CI | Previous frontend validated on historical runs; new Avalonia native acceptance pending |
| Windows (ARM64) | Windows 10 1809 | Same metadata; native Windows ARM64 CI | Previous frontend validated on historical runs; new Avalonia native acceptance pending |
| Linux (x64) | Ubuntu 24.04, glibc 2.39; Avalonia, system WebKitGTK 4.1 and X11 | Debian control and Ubuntu CI | Previous frontend validated on historical runs; new Avalonia native acceptance pending |
| Linux (ARM64) | Same baseline | Same package dependencies; native Ubuntu ARM64 CI | Previous frontend validated on historical runs; new Avalonia native acceptance pending |

See [verification](VERIFICATION.md) for named runs. macOS 14 is a deployment target, not a statement that CI hosts run macOS 14. Linux requirements are a distribution baseline, not an assertion that other distributions cannot work.

Internal macOS ARM64 Avalonia is a development/reference target only, with a separate
bundle/data identity and workflow artifact. It is not a supported production Mac download.
See [current migration verification](AVALONIA-MIGRATION.md).
