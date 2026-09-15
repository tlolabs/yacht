# Native architecture

## Targets and state ownership

`YachtCore` is a Foundation-only Swift package library. Both the CLI and the SwiftUI app depend on it. The checked-in Xcode project builds the app and UI-test runner; `script/generate_project.py` reproduces it without a third-party dependency. Regenerate after adding/removing app or UI-test Swift files and commit the result.

The utility uses a main WindowGroup with external-file events routed to the existing workspace, a native Settings scene, File/View/Help commands and standard keyboard shortcuts. `Workspace` is a main-actor observable owner of current input, table, style, preview and operations. `Preferences` owns the last style, recent files and named presets. Views contain presentation/control logic; parsing and generation remain in the core.

## CSV parser and data model

`CSVParser` is a four-state byte parser (field start, unquoted, quoted, after quote). FileHandle reads 64 KiB buffers, with BOM handling independent of buffer boundaries. Field bytes are decoded only after the complete UTF-8 field is available. A one-byte-buffer test exercises split CRLF, escaped quotes and multibyte characters.

CSV quoting follows RFC 4180, with CR, LF and CRLF accepted as record separators. Quoted newlines are retained verbatim. A blank record remains an empty row; a trailing delimiter creates an empty field. Empty files, NUL/UTF-16, invalid UTF-8 and malformed quotes fail with context. Alternative delimiter choices are explicit, not guessed from cell data. The first record is always the header.

`TableData` stores immutable headers and rows. Rows retain their original lengths without eagerly allocating padding. Header width expands to preserve excess cells. The generator fills missing values. Blank/empty headers supplied by the CSV remain blank; generated extra column names are `Column N`. The empty-record header case creates at least one generated column to avoid a zero-column table.

The parser checks cancellation at 64 KiB intervals. Data is retained in memory for repeated previews without disk rereads. Parsing is linear in input bytes; peak memory includes the table, largest field and transient row-container copies. No arbitrary file-size cutoff is imposed, so extremely large files remain RAM-limited. A future disk-backed model can replace table storage without changing style configuration.

## HTML and styling

`StyleOptions` is separate from data. Its Codable keys match all sixteen Python style properties; partial JSON presets receive defaults. Presets compare to the exact legacy Unstyled configuration to omit all style/classes. Style validation enforces numeric ranges, known border modes, font-family syntax and a CSS color grammar that excludes declarations, URLs and markup. Color names still depend on browser CSS support.

`HTMLGenerator.write` sends document chunks to a throwing sink. It escapes text and attribute content (`&`, `<`, `>`, quotes, apostrophes). There is no trusted/raw mode. Column headers use scope=col and the table retains the original CSS classes. Numeric alignment uses an allocation-free ASCII number scanner. Export writes buffered chunks without building a second full HTML string. Copy deliberately builds the full string for the system clipboard.

`PreviewDocument` applies row and byte budgets and builds an explicitly limited source excerpt. It uses the same generator as export; there is no separate approximate table renderer. Rows shown have exactly the same generated CSS/markup as exported rows.

## Concurrency and cancellation

Swift 6 strict concurrency is enabled. `Background.run` creates a detached user-initiated task and propagates cancellation. Value snapshots of the table/style cross the boundary. Main-actor state changes apply only to the current request. Import and preview tasks cancel superseded work; preview changes debounce for 180 ms. The parser and generator check cancellation periodically. Export snapshots the chosen configuration and stages data before opening the Save panel.

`FileService` stages in a uniquely named adjacent temporary file, synchronizes the output, then moves without replacement or atomically replaces when explicitly authorized. Failed parsing never creates/truncates the requested output. Cleanup removes temporary files. Batch conversion reports failures per file and continues. The CLI also protects the input path from replacement, including resolved symbolic links.

## Native interoperability and security

- SwiftUI owns controls, source display, settings, menus, drag/drop and file dialogs.
- `HTMLPreview` wraps WKWebView because macOS 14 SwiftUI has no native HTML renderer. Its coordinator only owns load identity and navigation policy.
- Preview disables content JavaScript, uses nonpersistent website data, injects a preview-only Content Security Policy blocking all resources except embedded style, and cancels navigation. The exported document does not receive this preview-only policy.
- `DesktopActions` is a small boundary for NSPasteboard and NSWorkspace (clipboard, Finder reveal and default-browser opening).
- `ColorSetting` bridges NSColor solely to convert native ColorPicker values to CSS hex.
- File import uses security-scoped access during reading, keeping a future sandboxed edition feasible. A Mac App Store edition would additionally need persistent bookmarks and explicit output-folder grants for batch writes.

No networking, analytics, script execution, update service or commercial dependencies are added. The Help link and explicitly opening an exported file are the only user-requested external navigation actions.

WebKit JavaScript control follows [Apple’s documentation](https://developer.apple.com/documentation/webkit/wkwebpagepreferences/allowscontentjavascript). CI uses the stable [Swift 6.3.3 toolchain in Xcode 26.6](https://forums.swift.org/t/announcing-swift-6-3-3/87888).

## Persistence and accessibility

Named presets live in `~/Library/Application Support/Y.A.C.H.T./presets.json`. On first native launch, existing `~/.yacht_presets.json` is read and copied without changing the original. Errors are shown and preserve the original file. UserDefaults remembers the valid last style, preview setting and recent paths. No automatic reopen/export occurs on startup; the sample makes the initial view useful.

A small NSApplicationDelegate bridge receives the complete Finder open-URL array and passes it to Workspace. Multiple inputs open a batch review; one opens a preview. SwiftUI onOpenURL is avoided because it delivered only the first file in a multi-file event during verification.

Native labeled controls, standard focus traversal, keyboard shortcuts, selectable source text, adaptive interface colors and semantic HTML support assistive technology. Export colors are author-selected document content, so they intentionally remain independent of the Mac’s light/dark appearance. A human VoiceOver and display/accessibility acceptance pass remains valuable.
