# Cross-platform feature parity

Status: **V** implemented and verified within the listed test scope; **I** implemented,
remaining native/manual acceptance pending. I is not an unsupported feature or acceptance claim.
Windows and Linux core, binding, native runtime tests and packages passed on x64
and ARM64 CI. macOS now has separate x64 and ARM64 build, UI test and packaging gates.
See VERIFICATION.md for evidence and test scope. Manual acceptance below remains
necessary even for rows with automated verification.

| Feature | Rust core | macOS | Windows | Linux | Automated test |
|---|---|---|---|---|---|
| Strict CSV/TSV, four delimiters, BOM/Unicode | V | V | V | V | Rust behavior + Swift CSV tests |
| Blank/missing/extra cells and warnings | V | V | V | V | Rust/Swift ragged tests, native binding tests |
| Sample and single-file open | V | V | V | V | XCTest + native runtime smoke |
| All sixteen style controls, native color choice | V | V | I | I | Rust CSS tests; UI smoke; picker acceptance manual |
| Styled/unstyled and numeric alignment | V | V | V | V | Python snapshots and Rust/Swift tests |
| Live actual HTML preview | V | V | V | V | Rust preview tests + native runtime smoke |
| Bounded preview/source, unlimited export/copy | V | V | V | V | 100k rows/huge-cell tests, binding export checks |
| Preset defaults, load/save/delete, confirmation | V | V | I | I | Rust preset tests; XCTest; runtime preset tests |
| Legacy partial/null preset format and migration | V | V | V | V | Rust/Swift/ctypes/C# round trips |
| Export and explicit overwrite | V | V | V | V | Rust atomic tests; native bindings; XCTest Save dialog |
| Batch review, partial failures, TSV inference | V | V | V | V | Rust batch, CLI script, native runtime suites |
| Cancellation and unchanged destination | V | V | V | V | Rust/Swift/ctypes/C# tests |
| Native open/save, multi-file drag/drop | — | V | I | I | XCTest Finder; native startup/open tests; manual drops |
| Complete clipboard copy | V | V | V | V | XCTest and native runtime clipboard tests |
| Recent ten files and clear | — | V | I | I | Runtime tests + manual native integration |
| Remember style and preview setting | V | V | V | V | Shared settings/native runtime tests |
| File manager reveal and browser launch | — | V | I | I | Manual native integration |
| Native commands and keyboard shortcuts | — | V | I | I | XCTest shortcuts; manual keyboard pass |
| System/light/dark appearance | — | V | I | I | XCTest settings; manual scaling pass remains |
| Screen reader, focus and native scaling | scoped HTML | I | I | I | Semantic assertions + human acceptance |
| Shared Rust CLI, flags, batch/status | V | V | V | V | 60 seeded differential + CLI safety checks |
| Native bindings and concurrent access | V | V | V | V | Swift, C#, ctypes tests; native x64/ARM64 CI |
| Installers/packages and signing hooks | — | V | V | V | x64/ARM64 ZIP/DMG, Windows installer/ZIP, Linux deb/tar gates |

Review this table after CI, updating I to V only with actual platform evidence.
All six OS/architecture combinations have independent native runner jobs. The
macOS split is awaiting its first complete CI run; see VERIFICATION.md for evidence.

## Manual release acceptance

Use the same real teaching CSV/preset collection on all platforms. Exercise native
open/save cancellation and replacement, batch partial failures/cancellation,
file-manager launches, color pickers, malformed data, large cells, keyboard-only
navigation, screen reader labels/table headers, high contrast, light/dark/system,
and normal HiDPI/fractional scaling. Record the host/version and deviations here.
No significant compromise is considered approved by being listed as pending.
