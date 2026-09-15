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
