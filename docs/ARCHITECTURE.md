# Shared behavior, native presentation

```
SwiftUI → Swift typed adapter ─┐
WinUI 3 → C# P/Invoke ─────────┼→ yacht-ffi (C ABI / JSON v1) → yacht-core
GTK 4/libadwaita → ctypes ─────┘                               ↑
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
| Sources/YachtCore | Swift adapters and editable native projections only |
| Sources/YachtApp | Preserved SwiftUI views, stores and macOS integrations |
| platform/windows/YACHT | WinUI 3, P/Invoke, native Windows preferences |
| platform/linux | PyGObject GTK/libadwaita, ctypes, native Linux preferences |
| Tests/YachtCoreTests, UITests | Existing Swift binding and macOS UI coverage |
| platform/windows/Tests, platform/linux/test_ui.py | Native runtime integration |
| script, .github | Version generation, tests, packaging and required CI gates |
| legacy-python | Frozen reference, fixtures remain under Tests |

Swift source locations remain to preserve Xcode/SwiftPM history and existing test
entrypoints; YachtCore's name is a compatibility facade, not a second business
implementation. The former Swift CLI is removed after Rust differential tests
passed. Original Python remains at the root and under legacy-python; only the root
copy's visible branding changed. No legacy implementation may be removed without
owner acceptance. See [audit and migration plan](MIGRATION.md).

## Operation lifecycle

Parsing incrementally consumes UTF-8 bytes and stores sparse rows. Native worker
queues call the ABI off the UI thread. Swift Task cancellation, C# CancellationToken
and GTK threading.Event feed the same Rust checks. Preview edits debounce, cancel
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
Windows: WinUI controls, HWND-initialized WinRT pickers, clipboard/DataPackage,
Explorer reveal, file associations and WebView2 restricted to table preview.
The GUI executable is YachtApp.exe to avoid a case-insensitive collision with the
shared CLI yacht.exe. Linux: GTK/GIO dialogs, FileList drops, clipboard, file-manager
DBus reveal, desktop/MIME registration, GLib preferences and WebKitGTK preview.
GTK's Python code is a native presentation adapter; it does not import the legacy
Python converter or implement any CSV/HTML logic.

Controls use native accessible names and focus traversal; HTML uses scoped headers.
Appearance is independent of export colors. There is no automatic updater or
notification workflow inherited from the reference app. Completion remains visible
in the app; no permission-prompting notifications were added speculatively.

## Rules for future changes

Platform-independent functionality belongs in Rust by default.
User-facing changes must be evaluated for implementation across all supported native UIs.

Update [behavior](BEHAVIOR.md), [parity](FEATURE-PARITY.md), core regression tests
and every affected native frontend together. Keep ABI additions compatible within
v1; do not persist handles. Never introduce a native parser/rendering fallback.
All release artifacts must pass required platform gates and use one commit/version.
