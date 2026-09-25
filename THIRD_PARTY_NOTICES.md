# Third-party notices

YACHT source is declared under the repository [LICENSE](LICENSE). Third-party software retains its own copyright and license terms. This file identifies major third-party components; the machine-readable inventory and release SBOM provide exact versions.

| Component | Use and distribution | License/terms source |
|---|---|---|
| Rust registry crates in `Cargo.lock` | Compiled into the Rust core, CLI, and bindings | Each crate's published `Cargo.toml` and license files; primarily MIT or Apache-2.0 alternatives, with Unlicense, Zlib, and Unicode-3.0 components |
| Microsoft Windows App SDK and WinUI | Current Windows portable ZIP bundles runtime files; future framework-dependent output is under evaluation | Microsoft Windows App SDK NuGet `license.txt` and `NOTICE.txt` (Microsoft Software License Terms); [exact file audit](docs/LICENSE_AUDIT.md) |
| Microsoft Windows ML | Five unused files bundled transitively in each historical Windows ZIP | Separate Microsoft Windows ML Runtime `license.txt`; evaluate removal before the next Windows release |
| Microsoft Windows SDK .NET projections | Two bundled DLLs in each historical Windows ZIP | Windows SDK license referenced by the exact NuGet package |
| Microsoft .NET 8 runtime | Self-contained Windows application | Exact .NET runtime packs declare MIT and contain `LICENSE.TXT` and `THIRD-PARTY-NOTICES.TXT` |
| Microsoft WebView2 | BSD-3-Clause SDK/loader files bundled; Evergreen runtime installed separately | Exact WebView2 NuGet `LICENSE.txt`; Evergreen runtime terms remain Microsoft's |
| Apple system frameworks | macOS native interface and preview | Apple platform terms; supplied by macOS |
| GTK 4, libadwaita, WebKitGTK, PyGObject, Python | Linux native interface and preview | Distribution packages and their respective licenses; installed by the OS package manager |

No third-party source or artwork is vendored in the tracked repository. The icon artwork is documented as original in `assets/icons/README.md`. Do not remove upstream notices from published runtime files. Windows packaging must include the applicable Microsoft license and notice texts when those files are redistributed. The [audit](docs/LICENSE_AUDIT.md) distinguishes YACHT's GPL source license, the combined Windows package, and SignPath eligibility.
