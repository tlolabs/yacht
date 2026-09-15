# Pre-rewrite baseline

Baseline: `703bcd2`, main, September 15, 2026. Working tree was clean, including after fetching origin. There were no legitimate uncommitted changes to preserve. The ignored BTE.html was a generated local output and remains untouched. Git history contains four commits (initial source, publication, license/context, cross-platform distribution). No tests, TODO/FIXME markers, custom icon, updater, or GitHub Issues existed.

The Python files remain at their original paths and are also copied verbatim under legacy-python/ for verification; no implementation has been removed or archived. Keep both until the owner accepts the Swift replacement.

## Verified behavior

Executed the Python generator on six CSV fixtures with both styled and unstyled options; captured twelve exact HTML fixtures under Tests/YachtCoreTests/Fixtures. Constructed the Tk GUI, rendered its startup sample, and verified three headers/nine rows. Runtime is Python stdlib plus Tkinter; setup.py uses py2app.

- UTF-8-sig input; comma delimiter only; Python csv.reader defaults, including permissive malformed quoting. First physical CSV record is the header. CR/LF/CRLF, quoted commas, escaped quotes and quoted multiline fields are supported.
- Empty file fails; missing cells pad; excess cells silently truncate to header width; blank records produce empty table rows.
- Full HTML document, English language, fixed title, embedded CSS, wrapper div, table, thead/tbody. Cells and classes use html.escape including quotes/apostrophes. No raw HTML mode.
- Numeric regex right-aligns integers and grouped/decimal numbers but misses some ungrouped four/five-digit values. Percent stays left-aligned.
- Eighteen style values: table_class, font_family, font_size_px, cell_padding_px, border_width_px, border_style, border_color, header_bg, header_text_color, body_bg, zebra_enabled, zebra_bg, hover_enabled, hover_bg, border_collapse, border_spacing_px, zebra_bg, hover_bg (the latter two are associated with their toggles). Defaults captured in JSON. Width is always 100%; vertical alignment top, header left, numeric body right. No custom width/alignment, delimiter, fragment, inline CSS or raw HTML controls.
- Border styles solid/dashed/dotted/double/none; collapse/separate and spacing; font list, size, padding; six colors with color chooser and text entry.
- Built-in Default (Styled) and Unstyled presets; load/save/delete named presets in ~/.yacht_presets.json, legacy local-file migration. Reserved built-in names. Exact unstyled configuration omits CSS/classes.
- Browse CSV, choose export path, Generate HTML, Batch Convert CSVs beside inputs with partial-failure summary, Copy HTML Code (full document), read-only Preview HTML Source, Refresh Preview.
- Tk canvas shows first ten rows and approximates CSS; no hover rendering. Built-in album sample/startup BTE.csv.
- CLI accepts multiple paths, -o/--output only for a single file, and flags for every style property. Outputs next to inputs by default and overwrites silently. Finder/Dock drop invokes the same CLI via py2app.
- Bundle name Y.A.C.H.T.; id com.local.yacht.csvhtmltranslator; version 1.1.0; no icon asset or signing/update configuration. GPLv3. Legacy packaging targets macOS Intel/ARM, Windows and Linux.

## Defects to correct deliberately

Prevent silent extra-column loss and accidental export overwrite. Reject malformed quoting and unsupported encodings with record/line context. Validate CSS values to block stylesheet/HTML injection (Python interpolates font and color fields unchecked). Add scope=col headers and viewport metadata. Preserve default CSS classes, styles, numeric intent, full-document copy, and blank-row behavior. Native preview must render the actual HTML/CSS including hover.
