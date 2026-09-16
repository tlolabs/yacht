# Rust/native migration verification

Run date: 2026-09-15. Local host: Apple Silicon Mac, macOS 26.7, Xcode 26.6,
Rust 1.98.1; project-local .NET SDK 8.0.425 for C# compilation/binding tests.

## Executed locally

- Rust workspace tests: eight behavior groups plus the C ABI ownership/error test
  passed. They cover strict CSV and chunking, Unicode/BOM/newlines, sparse rows,
  malformed encodings/quoting, Python HTML snapshots, all styling/escaping,
  presets, batch, atomic replacement, cancellation, large inputs and preview limits.
- `cargo fmt --all --check` and Clippy across all workspace targets with warnings
  denied passed. Rust `cargo check` passed for x86_64 Windows MSVC and Linux GNU;
  these checks are not native execution on those platforms.
- SwiftPM: **25 tests passed** against the actual Rust library, including all 23
  retained tests and new shared settings/TSV batch binding cases.
- CLI: **60 seeded Python/Rust differential cases**, all sixteen styling flags,
  ordered Unstyled reset, dash-prefixed positional paths, overwrite/input safety
  and mixed-success batch checks passed.
- ctypes adapter: Unicode paths, opening, metadata, preview/source, complete export,
  replacement safety, presets/settings, batch, cancellation and concurrent calls
  passed against the macOS dynamic Rust library.
- C# adapter: built with zero warnings/errors and passed equivalent binding tests
  against that library. Windows dependencies were restored and locked; locked ARM64
  resolution passed. The full WinUI C# source compiled against resolved SDK references
  using temporary XAML-generated declarations (compiler scaffolding only, not shipped).
- Universal Rust static library and CLI cross-built for Apple Silicon and Intel.
  Local unsigned/ad-hoc macOS packaging produced DMG, ZIP, CLI and checksums; the
  script verified code signatures and both CPU architectures.
- Shell syntax, Python syntax, workflow YAML and git whitespace checks passed.

## Native UI execution

The first Rust-backed macOS run passed all seven retained UI tests
(`build/RustMigrationUITests.xcresult`). A subsequent expanded run found missing
Settings accessibility identifiers and native event-targeting failures; its trace
also captured unrelated desktop application automation crashes. Settings controls
now have explicit accessible labels/identifiers, appearance uses the isolated test
preference domain, and test launch/panel helpers explicitly activate YACHT.

All four affected cases passed the focused rerun
(`build/RustMigrationFocusedUITests.xcresult`). The final complete **eight-case run passed**, followed by successful universal
ad-hoc packaging (`build/RustMigrationAcceptedUITests.xcresult`). Tests cover sample/source/copy,
malformed input, semantic escaping, preset save/load/delete, native Open/Save,
batch replacement protection, Finder multiple files, appearance and preview settings.

## Native CI follow-up

**All five required platform jobs passed** for source commit `b3505be` on
September 16, 2026: https://github.com/tlolabs/yacht/actions/runs/35068094113.
macOS universal, Windows x64/ARM64 and Linux x64/ARM64 artifacts were uploaded
only after their required checks passed. The tagged release job was skipped.

Owner-authorized branch: `codex/rust-native-core`. Native CI has now executed on
macOS 26 (Apple Silicon), Ubuntu 24.04 x64/ARM64 and Windows Server 2025 x64 /
Windows 11 ARM64.

- macOS: Rust quality gates, generated-file consistency, all 25 Swift tests,
  differential CLI and ctypes tests, all eight UI tests and universal DMG/ZIP
  packaging passed in https://github.com/tlolabs/yacht/actions/runs/35066573379.
- Linux: Rust tests, all CLI differential cases, ctypes bindings, real GTK runtime
  workflows and DEB/tar packaging passed on both architectures. The GTK suite drives
  startup, all sixteen controls, file load, preview/source, styles, preset save,
  clipboard, export, settings and batch. Example evidence:
  https://github.com/tlolabs/yacht/actions/runs/35065733023.
- Windows: Rust, CLI differential, C# bindings and ctypes bindings passed natively
  on both architectures. WinUI startup, file open, preview/source, styles, presets,
  clipboard, export, settings and batch passed, followed by successful artifact
  uploads in https://github.com/tlolabs/yacht/actions/runs/35068094113 (`b3505be`).
  Both architectures produced installers and ZIPs. CI identified a Windows App SDK
  bug omitting the generated application PRI from publish output; an explicit
  publish target and missing-resource gate fix that omission. It also identified
  delayed native control events cancelling style previews; unchanged values now
  avoid redundant renders.
- CI exposed Git checkout newline conversion of reference fixtures and a test read
  using Windows' default encoding. Fixture bytes are now preserved by attributes,
  and generated HTML is read explicitly as UTF-8.
- Windows restore now selects Windows runtime identifiers explicitly, preventing
  the build host from contaminating the NuGet lockfile.
- Linux CI starts D-Bus inside its virtual display and grants the bubblewrap helper
  user-namespace access through an AppArmor profile. WebKit sandboxing stays enabled.

Intel Mac UI execution, full screen-reader, high-contrast/display-scaling and owner
real-dataset acceptance remain pending. No signing credentials, notarization
submission, stable tag or public release were used. A focused scan of changed/new
text files found no private-key or common access-token patterns; this is not a
comprehensive security audit. macOS packaging defaults to explicit local unsigned
mode; signing and notarization require separate command-line modes.

## Performance sample

One optimized local 100,000-row, three-column sample via `cargo bench`:

| Operation | Elapsed |
|---|---:|
| UTF-8 parsing | 15.35 ms |
| Bounded HTML preview + source excerpt | 3.81 ms |
| Streaming HTML generation into a sink | 2.74 ms |

These are local samples, not portable performance guarantees. The sink figure is
not disk export latency. Export remains buffered/streaming; table and full clipboard
storage remain RAM-limited. Compare on the same host/data when evaluating regressions.

---

# Historical Swift migration verification

The following records the prior implementation before Rust. It is retained as
historical evidence, not a claim about the current architecture.

# Rewrite verification

Verified September 15, 2026, with Xcode 26.6 / Swift 6.3.3 on Apple Silicon. The original Python implementation remains in place and under `legacy-python/` pending manual acceptance.

## Automated coverage

- 23 Swift unit tests: CSV quoting, escaped quotes, commas, multiline cells, all newline conventions, blanks, Unicode/BOM/emoji, one-byte read buffers, short/extra rows, invalid UTF-8/NUL and malformed quotes, 100,000-row input, long cells, HTML structure/escaping, combining Unicode characters in attributes, CSS injection rejection, numeric alignment, presets, atomic replacement, failed imports, cancellation cleanup and bounded previews (including very wide missing-cell tables).
- Exact regression fixtures from the original Python app, with only the documented header/viewport/numeric changes normalized for comparison.
- 60 deterministic Python/Swift differential CSV cases, including HTML-looking content and combining Unicode. The integration script also tests every-file batch continuation, collision rejection, explicit replacement, unsafe styles and input-path protection.
- Seven native UI workflows: sample preview/source/copy/reset; importing escaped content with extra columns; malformed-input error and disabled export; native Open/Export panels and saved-file content; named preset save/load/delete; batch collision protection; Finder multi-file opening with both exported outputs and subsequent single-file preview. CI runs the complete suite, including on its 1024-pixel-wide virtual display. The initial window size fits that smaller desktop.

Local XCTest initially could not enable automation while the desktop was unavailable. Once the desktop was available, the tests ran normally. Result bundles remain under the ignored `build/` directory and CI uploads its `.xcresult` artifact for inspection.

## Direct verification

- Constructed and rendered the original Tk GUI and executed its CLI before rewriting.
- Built and launched the native `.app` bundle through the repository run script.
- Inspected native UI layout and accessibility structure.
- Opened the escaping fixture through the native Open panel; verified `<script>` renders as literal text, and HTML Source contains escaped markup.
- Exported through the native Export panel and checked the saved file on disk, including escaped content and Finder/browser completion controls.
- Verified Finder-style `Open With` loads a single CSV into the native window and cold-launch multi-file opening retains both inputs in batch review.
- Built universal ARM64/x86_64 app and CLI, verified both architectures and code signatures, and produced DMG/ZIP/checksums.

## Performance sample

A generated 100,000-row, three-column CSV was converted end to end using the original Python CLI and optimized native CLI. Four trials per implementation; median of the last three:

| Implementation | Time |
| --- | ---: |
| Python | 0.194 s |
| Swift release | 0.118 s |

These are local warm-cache measurements, not a universal performance guarantee. The native app performs parsing/generation in background tasks and bounds preview/source rendering. Full table storage and clipboard copy remain RAM-limited.

## Acceptance and distribution limits

Development artifacts are ad-hoc signed, with no Developer ID certificate or notarization credentials available. There was no original icon asset or updater. Signing/notarization and release instructions are in `DISTRIBUTION.md`; future updater, disk-backed storage and Mac App Store access work are recorded in GitHub Issues.

Manual acceptance should include the owner's actual teaching CSVs/presets, light/dark appearance, keyboard/VoiceOver use, alternate display setups, and the preferred browser/editor workflows. Intel binaries are cross-built and architecture-verified; runtime UI testing was performed on Apple Silicon. No native release tag has been created and no Python implementation has been removed.
