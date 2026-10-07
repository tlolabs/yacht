# Dependencies

YACHT is a TLO Labs open-source project. Thomas Lothian's original code and artwork are declared GPL-3.0-or-later. Third-party components retain their own terms. The [licensing audit](LICENSE_AUDIT.md) assesses the Windows package and SignPath eligibility separately.

Versions are controlled by Cargo.lock, rust-toolchain.toml, CMake and Qt 6,
plus the committed Xcode SwiftPM resolution for Sparkle. Regenerate locks through their package manager; commit them with updates.
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
| MSVC compiler C runtime | Statically linked into Windows Rust binaries; Qt plugin runtime DLLs bundled with Windows packages; no separate VC runtime installation | Hosted Windows MSVC toolchain and `windeployqt`; rebuild to service runtime fixes | Microsoft redistribution terms retained |
| Qt 6 Widgets | Shared presentation and HTML preview | Qt 6.4+, LGPL-3.0 / GPL-3.0 | Windows, Linux, and internal Mac ARM64 |
| CMake | Build configuration for shared Qt layer | CMake 3.16+ | Windows, Linux, and internal Mac ARM64 |
| Python 3 | Build/test tooling only | Standard library | Not a shipped GUI dependency |
| Sparkle 2.9.6 | macOS update UI and authenticated installer | Exact SwiftPM requirement and Xcode Package.resolved | Native signed feeds/archives required |
| ring / sha2 / semver / reqwest / rustls / base64 | Shared update authentication, comparison and HTTPS | updater/Cargo.toml + Cargo.lock | No application-core dependency |
| Inno Setup 6 | Per-user Windows installer | CI runner / choco fallback; installer source tracked | Windows packaging only |
| dpkg-deb / tar / GnuPG | Linux packages/checksums/optional signatures | distribution toolchain | GPG key optional, never committed |
| librsvg / ImageMagick / iconutil | Regenerate committed SVG-derived native icons | Local developer tools; `script/generate_icons.sh` | macOS artwork regeneration only; not required to build or run |
| GitHub Actions | Build/test/artifact release infrastructure | .github/workflows; Dependabot monthly | Standard hosted runners |

**Licenses and notices.** Rust dependencies use the permissive alternatives in
Cargo metadata. Qt 6 is licensed under LGPL-3.0 / GPL-3.0; upstream notices are retained in `licenses/qt/`.
`script/collect_qt_notices.py` collects notices into `ThirdPartyLicenses/`.
[Migration dependency review](QT-MIGRATION.md) records the new dependency set. The
[Windows license audit](LICENSE_AUDIT.md) describes the historical v2.1.1 binaries, whose Windows App SDK stack is now retired.

Apple frameworks retain the installed OS terms. Cargo/SwiftPM locks and the
[dependency inventory](dependency-inventory.json) record exact versions.
A release SBOM must accompany every stable artifact.
