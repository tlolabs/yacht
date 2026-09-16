# Rust/native migration audit and plan

Audit baseline: ec96ba2, 2026-09-15. The working tree was clean. Inspected all
Sources targets, core/UI tests and fixtures, project generator/Xcode project,
Support, scripts, workflows, documentation, packaging and both Python copies.

## Existing behavior and ownership

- Seven Foundation core files own strict incremental UTF-8 CSV parsing (four
  delimiters, BOM, quoted multiline cells, contextual failures), sparse table
  records, generated extra headers/warnings, sixteen style properties, CSS safety,
  semantic escaped HTML, numeric alignment, bounded preview/source, streaming
  atomic export, input preservation, and Python-compatible preset JSON.
- Swift CLI supports multiple paths, all sixteen style flags, `--`, `--flag=value`,
  tilde expansion, explicit output/overwrite and partial batch failures.
- SwiftUI owns sample startup, single/multiple-file open, TSV extension selection,
  drag/drop/Finder events, recent ten paths, style inspector and color pickers,
  built-in and named preset load/save/delete with confirmations, live styled
  WebKit preview, source view, full clipboard copy/export, batch review, progress,
  cancellation, output reveal/browser actions, keyboard commands and Settings.
- Workspace currently also orchestrates batch semantics and debounce/background
  work. Shared conversion rules move to Rust; native scheduling remains native.
- UserDefaults keys: rememberStyle, lastStyle, previewRows, recentFiles. SceneStorage
  holds showStyles. Presets: Application Support/Y.A.C.H.T./presets.json, migrated
  on first use from ~/.yacht_presets.json without changing the source.
- No updater, custom icon, CSV editing, fragment export, raw HTML, custom width,
  project file format or notification workflow exists to preserve.
- Tests cover strict parsing, chunk boundaries, Unicode, escaping, styled/unstyled
  snapshots, presets, safe replacement, cancellation, 100k rows and preview bounds.
  Seven XCTest UI cases cover startup, opening, copy, reset, presets, batch,
  dialogs, Finder multiple-file events. Python differential script has 60 cases.
- Build: SwiftPM core/CLI/app; generated Xcode macOS app/UI target; shell universal
  package script, ad-hoc/Developer ID/notary support, DMG/ZIP/checksums. macOS CI
  tests/packages; legacy Python manual/v1-tag builds remain separate.
- Python remains a reference, including its historical naming/output. It must not
  be deleted before owner acceptance. Its permissive CSV/data-loss/CSS behavior
  is not reinstated: docs/COMPATIBILITY.md records deliberate Swift fixes.

## Sibling repositories actually inspected

ATIV Cargo.toml, README, docs/architecture.md and platform layout; EnCAP Cargo.toml,
README and docs/architecture.md. Both use Rust workspaces and native SwiftUI,
WinUI 3 and GTK/libadwaita clients. Both currently use JSON process protocols;
ATIV has platform/{macos,windows,linux}. Baselines: macOS 13, Windows 10 1809,
GTK 4.10/libadwaita 1.4 (Ubuntu 24.04 package baseline in ATIV).
YACHT retains its existing macOS 14 minimum (Observation/ContentUnavailableView).
Windows/Linux follow the applicable sibling baselines. No dependency on sibling
repositories is needed for CSV processing.

## Migration sequence and boundaries

1. Record this audit, canonical behavior and parity inventory before replacing code.
2. Implement/test Rust core and Rust CLI against existing fixtures, preserving
   exact HTML except the explicitly requested YACHT title cleanup.
3. Add a versioned C ABI with JSON requests/results and opaque retained table
   handles. Swift/C#/GTK use the same shared library. This intentionally differs
   from media-engine process protocols: tables stay in Rust memory for frequent
   preview updates; no subprocess startup or full table serialization per edit.
   Cancellation callbacks are synchronous, checked during parsing/generation and
   before atomic publication. Allocation/free contracts must be tested.
4. Replace Swift business implementation with bindings while retaining SwiftUI
   presentation and existing native integration/UI tests. Keep source locations
   during this stage to preserve history and Xcode/SwiftPM integration.
5. Implement WinUI 3 and GTK 4/libadwaita workflows over the same API. Native
   dialogs, clipboard, drag/drop, recent paths, appearance, accessibility and
   launch/reveal stay outside Rust. HTML renderers are preview-only surfaces.
6. Add per-platform integration/build/package gates, common versioning, dependency
   inventory, user/developer instructions and signing-ready release jobs.
7. Verify Rust, CLI, bindings and macOS UI locally; explicitly distinguish tests
   requiring Windows/Linux runners from executed evidence. Do not mark parity
   accepted merely because a platform has source files.

Shared state: tables, styles, presets, behavior settings, conversion/batch rules.
Native state: window/layout/selection, appearance, dialogs, recent OS integration.
Remove Swift production parsing/rendering/validation only after the Rust tests
establish replacements. Retain Python and historical fixtures unchanged.

## Compatibility-sensitive names retained

`com.local.yacht.csvhtmltranslator` (bundle/signing/UserDefaults identity),
`com.local.yacht.ui-testing`, CSV/TSV UTIs, ~/.yacht_presets.json, and
Application Support/Y.A.C.H.T. remain unchanged. Public app/package/HTML titles
become YACHT. Historical Python reference code and fixtures preserve provenance.
