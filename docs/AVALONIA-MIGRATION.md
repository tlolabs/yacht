# Shared Avalonia presentation migration

## Architecture and scope

Windows WinUI 3 and Linux GTK/libadwaita presentation have been replaced by one
Avalonia implementation in `platform/avalonia/YACHT`. Production macOS continues
to use the existing SwiftUI/AppKit application. The same AXAML, controls, commands,
view model and styling run on Windows x64/ARM64, Linux x64/ARM64 and the internal
macOS ARM64 reference host (macOS 15+ for the .NET 10 support baseline). There is no legacy presentation fallback.

`MainViewModel` owns user-visible state and operation lifecycle. `IDesktopServices`
is the boundary for dialogs, clipboard, color selection, appearance and shell
integration. `Core` is a thin P/Invoke binding to the unchanged versioned Rust ABI.
CSV parsing, validation, HTML, presets, bounded preview, atomic exports and batch
semantics remain in Rust. `NativeInstance` forwards file activations through a
bounded, current-user-only pipe. Shared commands gate concurrent operations,
preview requests cancel stale results, and closing active work requires confirmation
and waits for cancellation before exiting.

## Dependencies and distribution

The shared host uses .NET 10 (SDK 10.0.401), Avalonia 12.1.3, Fluent, ColorPicker,
and Avalonia.Controls.WebView 12.1.0. These Avalonia packages declare MIT in their
exact NuGet metadata; WebView is now open source and requires no paid subscription.
The telemetry-only Avalonia.BuildServices build assets are explicitly excluded;
conversion and the shipped application perform no telemetry.
Transitives include SkiaSharp 3.119.4, HarfBuzzSharp 8.3.1.3, MicroCom 0.11.6,
Tmds.DBus.Protocol 0.94.1 and ANGLE native binaries. ANGLE carries BSD-style and
component notices; Skia/HarfBuzz native packages carry third-party notices.
`licenses/avalonia/SOURCES.md` records commit-pinned license provenance for packages
that do not carry license text. Packaging copies exact NuGet metadata, license files,
native notices and the matched self-contained .NET runtime notices. These permissive
licenses are compatible with the project's GPL-3.0-or-later distribution model;
OS-provided WebKitGTK and WebView2 retain their separate system/runtime terms.
Historical v2.1.1 Windows license evidence remains historical, not the new inventory.

Windows uses WebView2, Linux uses the supported system WebKitGTK 4.1 engine,
and internal macOS uses WKWebView. HTML remains Rust-generated and escaped.
The preview injects a restrictive CSP before any content, denies remote resources
and page scripts through CSP, allows only the in-memory `about:blank` origin and WebView2 navigation events
whose data URL exactly matches the current generated document, handles
new-window requests, and disables developer tools. There is no application web
shell or remote UI. Unlike the retired WebView2-specific code, the common WebView
API does not expose a separate JavaScript-enabled switch; CSP enforces document
script denial. Exported files retain their original HTML contract.

## Storage and updates

Windows retains `%LOCALAPPDATA%/YACHT/ui.json` and `presets.json`. Linux retains
`$XDG_DATA_HOME/yacht/presets.json`, imports the old `$XDG_CONFIG_HOME/yacht/ui.ini`
when no new JSON preference file exists, and saves `ui.json` atomically. The old INI
is preserved. Existing `~/.yacht_presets.json` migration remains on production
Windows/Linux. Test storage is isolated by `YACHT_TEST_DATA`.

Internal macOS uses `~/Library/Application Support/YACHT-Avalonia-Internal` and
bundle ID `com.tlolabs.yacht.avalonia.internal`. It neither imports native macOS
preferences/presets nor checks the production updater. Both the view model and
update process adapter reject production updates on macOS. The internal package
contains neither Sparkle nor `yacht-update` nor Sparkle feed metadata.

`script/package_avalonia_internal.sh` writes only `dist/internal/` and
`build/internal-artifacts/YACHT-Avalonia-INTERNAL-macos-arm64.zip`. Its CI job has
read-only repository permissions and uploads a workflow artifact, never a Release
asset. `check_avalonia_contract.py` enforces target, dependency, bundle and workflow
contracts. Production Mac verification rejects both the internal plist marker and
Avalonia assemblies, in addition to requiring the production bundle ID, Sparkle,
Developer ID, notarization and Gatekeeper verification. Production package names
and updater manifest identity remain unchanged.

Windows retains the authenticated Rust update helper and Authenticode publisher,
digest and version checks before revealing an installer. Linux retains manual
package-manager installation. Production trust provisioning and real updater
installation qualification remain blocked as recorded in
[the updater ledger](updater/AUDIT-AND-QUALIFICATION.md). This migration does not
qualify those update installation paths and does not publish a production release.

## Feature audit

The old Windows/Linux source and integration tests were inspected before removal;
Git history remains the rollback/reference mechanism. All sixteen style fields,
color pickers, styled/unstyled resets, named presets and replacement/deletion
confirmation are present. Open, sample, refresh, copy, export, batch review/results,
cancel, settings, recent files, reveal, browser, help and update commands remain.
Native file dialogs, multiple-file drops and command-line file activations use the
same workflows. Batch replacement requires explicit confirmation; partial failures
remain visible. TSV inference, four delimiters, Unicode, ragged warnings, bounded
preview/source and complete copy/export retain the Rust behavior.

Preferences preserve remembered style, preview limits (50/200/1000), appearance,
and ten recent files with clearing. Shared keyboard commands retain Ctrl+O,
Ctrl+S, Ctrl+Shift+C/B, Ctrl+R/F5 and Ctrl+1/2 on all shared hosts. The internal Mac
intentionally uses these same commands and layout. Production Mac shortcuts and
native features remain unchanged. Numeric style fields accept typed integers with
Rust validation rather than platform-specific number spinners.

Accessible labels identify controls and dialogs. The status region is polite-live,
focus traversal is provided by Avalonia, dialogs focus cancellation by default,
source is read-only selectable text, and the native preview exposes semantic table
headers/cells. Fluent supplies shared light/dark/system styling and scalable layout;
a resizable split pane, scrolling and wrapping avoid fixed pixel assumptions.
Windows/Linux screen-reader, fractional scaling, high contrast, file associations,
file-manager reveal and installer acceptance still require execution on those hosts.

## Validation record

See [verification](VERIFICATION.md) for final results. Cross-publishing managed
binaries is not equivalent to validating the Rust library, native preview,
installer or application lifecycle on another OS. Native CI jobs remain required
for each production target. No existing core test has been removed or weakened;
the old C# integration assertions are preserved in the shared test project, and
the ctypes adapter is retained solely as independent test support. The WinUI/GTK
in-app smoke workflows have been replaced by a common real-window smoke harness.

Runtime support baseline: [.NET 10 supported operating systems](https://github.com/dotnet/core/blob/main/release-notes/10.0/supported-os.md). The native production Mac deployment target remains macOS 14.
