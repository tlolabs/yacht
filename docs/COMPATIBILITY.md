# Compatibility and deliberate corrections

The baseline is `703bcd2`; captured fixtures were committed before Swift implementation at `50ba807`. The original Python files and migration records are preserved on the [2.0.2 archive branch](https://github.com/tlolabs/yacht/tree/codex/archive-legacy-2.0.2) after owner-authorized removal from the active tree.

## Preserved

All sixteen styling values and their defaults; built-in styled/unstyled resets; custom preset load/save/delete and compatible JSON; album sample; single and batch conversion; complete-document HTML copy; read-only source; numeric alignment intent; percent left-alignment; embedded CSS; original table/wrapper classes; GUI/CLI styling controls; default `.html` filenames; bundle identifier; GPLv3. The repository never contained a custom icon or automatic updater.

The native CLI accepts all former style flags, multiple files and single-file `-o`/`--output`. The Rust CLI and native WinUI/GTK interfaces replace the platform-specific production cores; the active tree contains only the Rust/native rewrite. Legacy distribution CI is archived; native tags use `v2*` onward.

## Output changes

1. `scope="col"` on header cells for accessible table navigation.
2. Viewport metadata for correct mobile scaling when embedding/viewing exported pages.
3. Excess cells are preserved using generated extra-column headers. Python silently discarded them. Missing cells remain blank.
4. Numeric matching recognizes ungrouped four/five-digit numbers and consistently validates thousands grouping. Percentages, scientific notation and nonnumeric labels remain text.
5. Unsafe CSS input is rejected before preview/copy/export. An empty custom font list uses `inherit` in styled output; exact Unstyled omits CSS entirely.
6. Malformed CSV is rejected, rather than silently repaired by Python’s permissive reader. Valid quoted/multiline data is retained byte-for-byte after UTF-8 decoding.

Default styled/unstyled snapshots for ordinary, quoted, Unicode, escaped and BOM input match the Python HTML exactly after the first two changes and the documented `1234` alignment correction. Ragged fixture changes have separate assertions. Sixty seeded cases compare all header/cell contents with frozen legacy results (including input SHA-256 hashes and reference provenance) and check that CSV content cannot create executable tags.

## Workflow improvements

- Actual WebKit live preview includes hover, spacing and border behavior; the Python canvas approximated the first ten rows and did not render hover.
- Native file dialogs and overwrite confirmation; atomic export; batch collision reporting; explicit CLI `--overwrite`.
- Finder/Dock Open With opens a reviewable preview; CLI and Batch Convert provide direct conversion. Multiple dropped or Finder-opened files open a batch review.
- Explicit delimiter controls add TSV/semicolon/pipe support; the previous app supported comma only.
- Recent files, remembered valid style, settings, keyboard shortcuts, Finder reveal and default-browser opening.
- Presets migrate into Application Support without altering the Python preset file. They are independent after migration.
- Large-data preview/source limits are visible. Export and clipboard output remain complete.

No raw HTML, inline CSS, fragment export, custom table width or general CSV editing feature existed. Those features are intentionally outside this rewrite.

## Rust/native release (2.1.0)

The HTML/application display name changes from the dotted legacy name to YACHT,
including `<title>YACHT Table</title>`. Historical Python snapshots remain unchanged;
regression tests normalize only this requested title change and the earlier approved
semantic corrections. Production CSV/style/preset/export behavior is now Rust-owned.

The existing bundle/signing ID, UserDefaults domain and keys, macOS Application
Support/Y.A.C.H.T. preset directory and legacy ~/.yacht_presets.json path are retained.
The old application is archived; its preset migration paths and frozen HTML
snapshots remain part of current compatibility coverage.

Preview also enforces a hard HTML byte cap after its existing estimate, preventing
unbounded preview markup from extremely large CSS class/font strings. Full copy and
export remain complete. Unsafe CSS values ending in a newline are rejected by an
absolute-end grammar; this closes the previous regex end-anchor ambiguity.

The installed CLI remains `yacht` with the same flags and ordering. Developer-only
`swift run yacht` becomes `cargo run -p yacht-cli --`. Windows GUI is YachtApp.exe
because YACHT.exe would collide with yacht.exe on case-insensitive file systems.
Portable presets keep the same object-of-styles JSON schema across all frontends.
