# Supported platforms

The repository's native project, CI and package scripts establish these targets. The authoritative minimum-version data is in `config/platforms.json`; `script/check_platforms.py` checks relevant build metadata against it. No other platform is claimed.

| Target | Minimum | Evidence | Validation category |
|---|---|---|---|
| macOS (ARM64) | macOS 14.0 | SwiftPM, Xcode project, Info.plist; native macOS CI | Personally tested primarily by Thomas Lothian; CI build/test validated on recorded runs |
| macOS (x64) | macOS 14.0 | Same deployment target; native macOS x64 CI | CI build/test validated on recorded runs; no personal hands-on claim |
| Windows (x64) | Windows 10 1809 | .NET target/minimum and installer; Windows CI | CI build/test validated on recorded runs; no personal hands-on claim |
| Windows (ARM64) | Windows 10 1809 | Same metadata; native Windows ARM64 CI | CI build/test validated on recorded runs; no personal hands-on claim |
| Linux (x64) | Ubuntu 24.04, glibc 2.39; GTK 4.10+, libadwaita 1.4+, WebKitGTK 6.0 | Debian control and Ubuntu CI | CI build/test validated on recorded runs; no personal hands-on claim |
| Linux (ARM64) | Same baseline | Same package dependencies; native Ubuntu ARM64 CI | CI build/test validated on recorded runs; no personal hands-on claim |

See [verification](VERIFICATION.md) for named runs. macOS 14 is a deployment target, not a statement that CI hosts run macOS 14. Linux requirements are a distribution baseline, not an assertion that other distributions cannot work.
