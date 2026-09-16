# Dependencies

Versions are controlled by Cargo.lock, rust-toolchain.toml and the Windows NuGet
lockfile. Regenerate locks through their package manager; commit them with updates.
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
| .NET 8 / Windows App SDK | WinUI 3 frontend | SDK 8.0.x in CI; Microsoft.WindowsAppSDK 2.4.0 in csproj + packages.lock.json; Dependabot | Windows 10 1809+, x64/ARM64 |
| WebView2 | Windows generated-table preview | Transitive Windows App SDK NuGet lock; Edge WebView2 runtime | Evergreen runtime must be present on Windows |
| GTK 4 / libadwaita | Linux native controls | distro packages, GTK >=4.10, Adwaita >=1.4 | Ubuntu 24.04 baseline; Linux x64/ARM64 |
| Python 3 / PyGObject | Thin GTK presentation and ctypes ABI | system Python >=3.10, python3-gi distro package | GTK Linux runtime; CLI tests generate their own input records |
| WebKitGTK 6.0 | Actual HTML/CSS Linux preview | distro gir1.2-webkit-6.0 | Native GTK4 web view, scripts/network blocked |
| Inno Setup 6 | Per-user Windows installer | CI runner / choco fallback; installer source tracked | Windows packaging only |
| dpkg-deb / tar / GnuPG | Linux packages/checksums/optional signatures | distribution toolchain | GPG key optional, never committed |
| librsvg / ImageMagick / iconutil | Regenerate committed SVG-derived native icons | Local developer tools; `script/generate_icons.sh` | macOS artwork regeneration only; not required to build or run |
| GitHub Actions | Build/test/artifact release infrastructure | .github/workflows; Dependabot monthly | Standard hosted runners |

GTK packages are distribution-managed rather than vendored; install through the
supported distro repositories so security fixes apply normally. macOS uses system
WebKit; Windows preview requires the installed WebView2 runtime. No third-party UI
framework or web-based application shell is introduced. C# NuGet and Cargo locks
include transitive libraries; consult them for exact dependency versions/hashes.
