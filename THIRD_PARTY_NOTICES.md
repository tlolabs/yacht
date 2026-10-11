# Shared UI dependency update

The Qt 6 Widgets migration replaces the former Avalonia/.NET and earlier GTK/WinUI
presentation dependencies. Qt 6 is licensed under GNU LGPLv3 and GNU GPLv3; exact
license provenance is in `licenses/qt/SOURCES.md`. Packages include
`ThirdPartyLicenses` (inside Resources for the internal Mac bundle), collected from
Qt upstream license texts. See [dependency review](docs/QT-MIGRATION.md). Historical
package statements below continue to describe already-published releases, not the
new shared UI binaries.

The Windows Rust binaries also include statically linked Microsoft compiler C
runtime code, which retains Microsoft's terms and copyright. It is treated as a
compiler System Library under GPLv3, not relicensed as YACHT code. See
[GPLv3's System Libraries definition](https://www.gnu.org/licenses/gpl-3.0.en.html),
[Microsoft's CRT library documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features),
and [Microsoft deployment guidance](https://learn.microsoft.com/en-us/cpp/windows/deployment-in-visual-cpp).
Build/distribution must use an appropriately licensed MSVC toolchain. Runtime
security fixes require rebuilding these binaries with the updated toolchain.
Windows packages also bundle MSVC runtime DLLs used by Qt plugins. Those files
retain Microsoft's redistribution terms and are checked for architecture and
presence during packaging.

# Third-party notices

YACHT source is declared under the repository [LICENSE](LICENSE). Third-party software retains its own copyright and license terms. This file identifies major third-party components; the machine-readable inventory and release SBOM provide exact versions.

| Component | Use and distribution | License/terms source |
|---|---|---|
| Rust registry crates in `Cargo.lock` | Compiled into the Rust core, CLI, and bindings | Each crate's published `Cargo.toml` and license files; primarily MIT or Apache-2.0 alternatives, with Unlicense, Zlib, and Unicode-3.0 components |
| Qt 6 Widgets | Compiled into the shared Windows, Linux and internal macOS reference applications | GNU LGPLv3 / GNU GPLv3; upstream Qt Company notices in `licenses/qt/` |
| Microsoft Windows App SDK and WinUI | Historical Windows portable ZIP runtime files | Microsoft Windows App SDK NuGet `license.txt` and `NOTICE.txt` (Microsoft Software License Terms); [exact file audit](docs/LICENSE_AUDIT.md) |
| Microsoft Windows ML | Five unused files bundled transitively in each historical Windows ZIP | Separate Microsoft Windows ML Runtime `license.txt`; evaluate removal before the next Windows release |
| Microsoft Windows SDK .NET projections | Two bundled DLLs in each historical Windows ZIP | Windows SDK license referenced by the exact NuGet package |
| Microsoft .NET 8 runtime | Historical self-contained Windows application | Exact .NET runtime packs declare MIT and contain `LICENSE.TXT` and `THIRD-PARTY-NOTICES.TXT` |
| Microsoft WebView2 | BSD-3-Clause SDK/loader files bundled; Evergreen runtime installed separately | Exact WebView2 NuGet `LICENSE.txt`; Evergreen runtime terms remain Microsoft's |
| Apple system frameworks | macOS native interface and preview | Apple platform terms; supplied by macOS |
| GTK 4, libadwaita, WebKitGTK, PyGObject, Python | Historical Linux native interface and preview | Distribution packages and their respective licenses; installed by the OS package manager |

No third-party source or artwork is vendored in the tracked repository. The icon artwork is documented as original in `assets/icons/README.md`. Do not remove upstream notices from published runtime files. Windows packaging must include the applicable Microsoft license and notice texts when those files are redistributed. The [audit](docs/LICENSE_AUDIT.md) distinguishes YACHT's GPL source license, the combined Windows package, and SignPath eligibility.

## Update components

The macOS frontend embeds [Sparkle 2.9.6](https://github.com/sparkle-project/Sparkle),
under its upstream permissive license. Its full license is shipped as
`Contents/Resources/Sparkle-LICENSE`. The independent updater uses ring (Ed25519),
RustCrypto sha2, semver, reqwest/rustls, base64 and tempfile. See the locked
dependency inventory for the full transitive set. webpki-roots contains trust
anchor data under CDLA-Permissive-2.0; preserve its data license in release notices.
