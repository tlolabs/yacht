# Shared core and presentation

```
SwiftUI → Swift typed adapter ─┐
Avalonia → view model → C# ───┘→ yacht-ffi (C ABI / JSON v1) → yacht-core
  Windows, Linux, internal Mac                               ↑
                                                      Rust yacht CLI
```

`crates/yacht-core` is the authoritative implementation of CSV, table, styles,
HTML, preview/source, presets, settings, atomic file publication and batch rules.
It contains no UI, clipboard, shell launch or platform preference-location APIs.
`crates/yacht-cli` owns command-line argument/output conventions and directly calls
that core. `crates/yacht-ffi` exposes two functions declared in `bindings/c/yacht.h`.
The core API dispatch is transport-neutral and independently tested.

The native API accepts UTF-8 JSON `{version:1, op:..., ...}` and returns either
`{"ok":value}` (including null) or `{"error":{"code":...,"message":...}}`.
The caller frees every response with yacht_free. Rust retains immutable tables;
read/parse/records/sample return metadata and a process-local handle. Native RAII
wrappers release the handle after its last owner. Concurrent operations retain an
Arc snapshot, so releasing one handle cannot invalidate an in-flight operation.
No Rust references, Vec/String layouts, allocators, exceptions or Swift objects
cross the ABI. Panics are contained. Cancellation callbacks run synchronously on
the calling thread and must remain valid until the call returns.

This is an intentional variation from the inspected ATIV/EnCAP process interfaces:
YACHT updates previews frequently and retains parsed tables. In-process opaque
handles avoid process startup and transferring an entire table on every keystroke.
They do not provide crash isolation from an abort or out-of-memory condition.

## Components and history

| Path | Responsibility |
|---|---|
| crates/yacht-core | Shared behavior, schemas, tests and benchmark |
| crates/yacht-cli | Rust `yacht` executable |
| crates/yacht-ffi, bindings/c | Stable ABI, allocation/cancellation contract |
| platform/macos/Sources/YachtCore | Swift adapters and editable native projections only |
| platform/macos/Sources/YachtApp | Preserved SwiftUI views, stores and macOS integrations |
| platform/avalonia/YACHT | Shared AXAML, view model, services and P/Invoke |
| platform/windows, platform/linux | OS packaging and integration metadata |
| platform/macos/Tests, platform/macos/UITests | Existing Swift binding and macOS UI coverage |
| platform/avalonia/Tests, script/test_avalonia_ui.py | Shared presentation and native runtime integration |
| script, .github | Version generation, tests, packaging and required CI gates |

The macOS source, binding tests, UI tests and Info.plist live under platform/macos.
Root Xcode/SwiftPM entry points reference those paths; YachtCore remains a Swift
binding facade, not a second business implementation. Shared icon artwork and native
formats live under assets/icons. The former Swift CLI and Python/Tk application are
preserved in Git history and on the [2.0.2 archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2).
Archived Python data and output fixtures have been removed. The retired WinUI/GTK presentation exists only in history. An independent Python
ctypes test adapter remains under script/ and is not shipped.

## Operation lifecycle

Parsing incrementally consumes UTF-8 bytes and stores sparse rows. Native worker
queues call the ABI off the UI thread. Swift Task cancellation and C# CancellationToken feed the same Rust checks. Preview edits debounce, cancel
obsolete requests and discard stale completion. Metadata drives native row counts;
full rows are materialized only for Swift compatibility/test consumers. Copy builds
the complete string; file export streams and does not allocate full HTML.

Rust owns batch file naming, TSV inference, overwrite rules and per-file outcomes.
Native clients grant file access, call one batch item at a time for progress and
cancellation, and display the results. Native file dialogs own selection and explicit
replacement confirmation. Swift's staged FileDocument workflow is preserved.

## Native responsibilities

macOS: SwiftUI, AppKit clipboard/Finder/browser bridge, security-scoped file access,
complete Finder open arrays, WKWebView, UserDefaults/SceneStorage and Settings.
Windows/Linux/internal Mac: shared Avalonia AXAML, view models, commands and
Fluent styles. Desktop services handle native pickers, clipboard, file-manager
integration and native WebView engines. The GUI executable is YachtApp.exe on
Windows to avoid colliding with yacht.exe. Linux desktop/MIME registration remains.
See [migration architecture and storage contracts](AVALONIA-MIGRATION.md).

Controls use native accessible names and focus traversal; HTML uses scoped headers.
Appearance is independent of export colors. The [candidate updater architecture](updater/ARCHITECTURE.md)
adds authenticated GitHub checks with native installation adapters; production
trust configuration and installation qualification remain open. No notification
workflow is inherited from the reference app. Completion remains visible
in the app; no permission-prompting notifications were added speculatively.

## Rules for future changes

Platform-independent functionality belongs in Rust by default.
User-facing changes must be evaluated for implementation across all supported native UIs.

Update [behavior](BEHAVIOR.md), [parity](FEATURE-PARITY.md), core regression tests
and every affected native frontend together. Keep ABI additions compatible within
v1; do not persist handles. Never introduce a native parser/rendering fallback.
All release artifacts must pass required platform gates and use one commit/version.
