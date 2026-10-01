# Dependencies

YACHT is a TLO Labs open-source project. Thomas Lothian's original code and artwork are declared GPL-3.0-or-later. Third-party components retain their own terms. The [licensing audit](LICENSE_AUDIT.md) assesses the Windows package and SignPath eligibility separately.

Versions are controlled by Cargo.lock, rust-toolchain.toml and the shared Avalonia NuGet
lockfile, plus the committed Xcode SwiftPM resolution for Sparkle. Regenerate locks through their package manager; commit them with updates.
No paid services or runtime network APIs are required for conversion.

| Dependency | Purpose | Version source / update | Platform limits |
|---|---|---|---|
| Rust toolchain | Core, CLI, C ABI compiler | 1.98.1, rust-toolchain.toml; explicit compiler updates | Native targets in CI |
| serde / serde_json | Portable style/preset/settings and ABI JSON | workspace Cargo.toml, exact Cargo.lock; Dependabot weekly | Shared across all platforms |
| regex | Bounded safe CSS/font validation | Cargo.toml + Cargo.lock; Dependabot | Rust regex, no backtracking engine |
| tempfile | Adjacent staging and atomic no-clobber publication | Cargo.toml + Cargo.lock; Dependabot | Native file-system atomic semantics |
| thiserror | Structured core errors | Cargo.toml + Cargo.lock; Dependabot | Core only |
| ctrlc | CLI cooperative cancellation | yacht-cli/Cargo.toml + Cargo.lock; Dependabot | SIGINT/Windows console events |
| Swift / SwiftUI / AppKit / WebKit | macOS presentation, OS integration, HTML preview | Xcode 26.6 ARM64 / 26.3 Intel in CI; Swift tools 6.0; OS frameworks | macOS 14+, Intel/Apple Silicon |
| .NET 10 | Shared managed host | global.json, SDK 10.0.401; self-contained runtime | Windows/Linux production, internal Mac ARM64 |
| Avalonia / Fluent / ColorPicker | Shared presentation | 12.1.3, platform/avalonia/YACHT/packages.lock.json | Same UI for all shared targets |
| Avalonia.Controls.WebView | Native generated-table preview | 12.1.0, MIT | WebView2 / WebKitGTK 4.1 / WKWebView |
| SkiaSharp / HarfBuzzSharp / ANGLE | Drawing and text | NuGet lock and exact native notices | Shared UI native dependencies |
| Python 3 | Build/test tooling only | Standard library | Not a shipped GUI dependency |
| Sparkle 2.9.6 | macOS update UI and authenticated installer | Exact SwiftPM requirement and Xcode Package.resolved | Native signed feeds/archives required |
| ring / sha2 / semver / reqwest / rustls / base64 | Shared update authentication, comparison and HTTPS | updater/Cargo.toml + Cargo.lock | No application-core dependency |
| Inno Setup 6 | Per-user Windows installer | CI runner / choco fallback; installer source tracked | Windows packaging only |
| dpkg-deb / tar / GnuPG | Linux packages/checksums/optional signatures | distribution toolchain | GPG key optional, never committed |
| librsvg / ImageMagick / iconutil | Regenerate committed SVG-derived native icons | Local developer tools; `script/generate_icons.sh` | macOS artwork regeneration only; not required to build or run |
| GitHub Actions | Build/test/artifact release infrastructure | .github/workflows; Dependabot monthly | Standard hosted runners |

**Licenses and notices.** Rust dependencies use the permissive alternatives in
Cargo metadata. Avalonia's exact package metadata declares MIT, including the
open-source WebView package; no subscription is introduced. Skia/HarfBuzz/ANGLE
native notices are retained. `script/collect_avalonia_notices.py` collects the exact
locked package metadata, license files and native notices, plus the matched .NET
runtime pack. [Migration dependency review](AVALONIA-MIGRATION.md) records the
new dependency set. The [Windows license audit](LICENSE_AUDIT.md) describes the
historical v2.1.1 binaries, whose Windows App SDK stack is now retired.

System WebKitGTK and WebView2 are distribution/runtime-managed, not vendored.
Apple frameworks retain the installed OS terms. NuGet/Cargo/SwiftPM locks and the
[dependency inventory](dependency-inventory.json) record exact versions.
A release SBOM must accompany every stable artifact.
