# Canonical YACHT behavior

Platform-independent functionality belongs in Rust by default.
User-facing changes must be evaluated for implementation across all supported native UIs.

## Inputs and tables

YACHT converts local UTF-8 CSV/TSV into complete HTML documents. UTF-8 BOM is
optional. Delimiters are comma, tab, semicolon and pipe. GUI opening/batch chooses
tab for a `.tsv` extension; explicit refresh respects the selected delimiter.
CLI uses its explicit delimiter (comma by default), including for TSV filenames.

The first record is the header. CR, LF and CRLF terminate records, except inside
quoted fields where their exact bytes are retained. Double quotes delimit a whole
field and doubled quotes escape a quote. Trailing empty fields and blank records
are preserved. Empty input, NUL, invalid UTF-8, unterminated quotes, quotes inside
unquoted fields and text after closing quotes are errors with record/line context.

Rows retain their input lengths. Output width is the largest record width, at
least one. Extra header names are `Column N`; existing empty headers stay empty.
Missing cells render blank. Warnings count body rows shorter/longer than the
original header. No cell is silently discarded or interpreted as HTML.

## Style and output

Sixteen snake_case properties use the defaults defined by `Style::default()` in
`crates/yacht-core/src/style.rs`. Partial/null properties receive
Rust defaults; unknown properties are ignored for legacy compatibility. Wrong
property types fail. Pixel values are integers: font size 1–10000, other sizes
0–10000. Border styles: solid, dashed, dotted, double, none, hidden, groove, ridge,
inset, outset. Collapse: collapse or separate. Font lists and CSS colors exclude
markup, declarations and URL expressions. Color names may be browser-dependent.

Exact equality with the built-in Unstyled configuration omits CSS and classes.
Otherwise output includes embedded CSS, the existing wrapper/table classes,
optional extra classes, 100% width, header left alignment and top cell alignment.
Numeric alignment accepts signed ASCII integers/decimals and correctly grouped
thousands; percentages and scientific notation stay left aligned.

Output is UTF-8 with LF formatting, title `YACHT Table`, language en, viewport
metadata, a table with thead/tbody and scope=col headers. Text and attributes
escape &, <, >, double quotes and apostrophes, including adjacent combining marks.
Equivalent tables/styles produce byte-identical documents across operating systems.

## Preview, source, copy and export

The startup sample has three columns and nine album rows. Preview uses the same
HTML/CSS generator as full export. Default preview is 200 rows; Settings offers
50/200/1000. Row and estimated 2 MB budgets apply, with a hard 2 MB HTML sink cap
as a final guard for huge styling strings. A table whose first row/header is too
large remains exportable with an explicit preview-unavailable message. Source is
a UTF-8-safe prefix of at most 1,000,000 bytes; truncation is visible.
Copy and Export include all rows. Export streams into a buffered adjacent temporary
file, flushes/syncs, checks cancellation, then atomically publishes. No-overwrite
publication must resist racing writers. Failure/cancellation before publication
preserves the destination and removes staging files. Completed publication cannot
be undone by a later cancellation. Conversion must not replace its source path,
including resolved symlinks. Cancellation is checked during parsing and generation,
and immediately before publication. Full table/copy storage remains RAM-limited.

## Presets, preferences and batch

Portable presets are a JSON object mapping names to the sixteen style fields.
Save validates every style; blank names and the two built-in names are reserved.
Load preserves legacy partial/default behavior, and never rewrites the source.
Preset replacement/deletion requires an explicit native confirmation.
First use imports `~/.yacht_presets.json` when no native store exists; original
remains unchanged. Behavior preferences are remember-style, last valid style and
preview rows; Rust owns their defaults/validation. Native storage and recent files
are platform responsibilities. Window/layout/view/appearance are presentation state.

Batch reviews all inputs, defaults to no replacement, explicitly confirms overwrite,
reports outcomes per input, continues after individual failures and stops on cancel.
Output is beside each input with its extension replaced by `.html`. Multiple GUI
open/drop inputs enter batch review; single inputs open a preview. Import/preview
completion from an obsolete request must never replace newer displayed state.

## CLI

`yacht input.csv [more.csv ...] [options]`. Preserve `-h/--help`, `-o/--output`,
`--overwrite`, `--delimiter`, `--unstyled`, all style flags, `--flag=value`, ordered
style resets and `--` positional separation. Help/no arguments succeeds. Output
is allowed only for one input; batch failures continue and exit 1. Cancellation
exits 130. Boolean values: true/false, yes/no, y/n, on/off, 1/0 (case insensitive).
Errors go to stderr; each successful conversion goes to stdout. Paths are never
shell commands. Ordinary current-user `~/` paths are expanded by the CLI.

## Native interaction and acceptance

SwiftUI, WinUI 3 and GTK/libadwaita own controls, menus/commands, native dialogs,
keyboard focus, accessibility, drag/drop, clipboard, recent files, file-manager
reveal and browser launching. A web engine may render only the generated table;
it is not the application interface. Preview JavaScript, outbound content and
navigation are blocked. The exported document does not contain the preview CSP.

Interfaces support system/light/dark appearance and native scaling. Keyboard
shortcuts use Command on macOS and Control on Windows/Linux; F5 also refreshes on
Windows/Linux. Existing data identities are not renamed with display branding.
