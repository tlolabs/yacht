# Cross-platform feature parity

Status: **V** implemented and verified locally; **I** implemented, native runner
verification pending. I is not an unsupported feature or acceptance claim.
The Windows/GTK runtime suites and packages require their CI hosts; the C# and
ctypes bindings can additionally be exercised with a local shared library.

| Feature | Rust core | macOS | Windows | Linux | Automated test |
|---|---|---|---|---|---|
| Strict CSV/TSV, four delimiters, BOM/Unicode | V | V | I | I | Rust behavior + Swift CSV tests |
| Blank/missing/extra cells and warnings | V | V | I | I | Rust/Swift ragged tests, native binding tests |
| Sample and single-file open | V | V | I | I | XCTest + native runtime smoke |
| All sixteen style controls, native color choice | V | V | I | I | Rust CSS tests; UI smoke; picker acceptance manual |
| Styled/unstyled and numeric alignment | V | V | I | I | Python snapshots and Rust/Swift tests |
| Live actual HTML preview | V | V | I | I | Rust preview tests + native runtime smoke |
| Bounded preview/source, unlimited export/copy | V | V | I | I | 100k rows/huge-cell tests, binding export checks |
| Preset defaults, load/save/delete, confirmation | V | V | I | I | Rust preset tests; XCTest; runtime preset tests |
| Legacy partial/null preset format and migration | V | V | I | I | Rust/Swift/ctypes/C# round trips |
| Export and explicit overwrite | V | V | I | I | Rust atomic tests; native bindings; XCTest Save dialog |
| Batch review, partial failures, TSV inference | V | V | I | I | Rust batch, CLI script, native runtime suites |
| Cancellation and unchanged destination | V | V | I | I | Rust/Swift/ctypes/C# tests |
| Native open/save, multi-file drag/drop | — | V | I | I | XCTest Finder; native startup/open tests; manual drops |
| Complete clipboard copy | V | V | I | I | XCTest and native runtime clipboard tests |
| Recent ten files and clear | — | V | I | I | Runtime tests + manual native integration |
| Remember style and preview setting | V | V | I | I | Shared settings/native runtime tests |
| File manager reveal and browser launch | — | V | I | I | Manual native integration |
| Native commands and keyboard shortcuts | — | V | I | I | XCTest shortcuts; manual keyboard pass |
| System/light/dark appearance | — | V | I | I | XCTest settings; manual scaling pass remains |
| Screen reader, focus and native scaling | scoped HTML | I | I | I | Semantic assertions + human acceptance |
| Shared Rust CLI, flags, batch/status | V | V | I | I | 60 seeded differential + CLI safety checks |
| Native bindings and concurrent access | V | V | V* | V* | Swift, C#, ctypes tests; *C#/ctypes on macOS |
| Installers/packages and signing hooks | — | V | I | I | Universal/ZIP/DMG, Windows installer/ZIP, Linux deb/tar gates |

Review this table after CI, updating I to V only with actual platform evidence.
Intel Mac binaries are cross-built; Intel UI execution is a separate acceptance
check. ARM64 Windows/Linux have independent runner jobs, not just cross-compilation.

## Manual release acceptance

Use the same real teaching CSV/preset collection on all platforms. Exercise native
open/save cancellation and replacement, batch partial failures/cancellation,
file-manager launches, color pickers, malformed data, large cells, keyboard-only
navigation, screen reader labels/table headers, high contrast, light/dark/system,
and normal HiDPI/fractional scaling. Record the host/version and deviations here.
No significant compromise is considered approved by being listed as pending.
