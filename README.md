# Y.A.C.H.T. — Yet Another CSV HTML Translator

A native Swift and SwiftUI Mac utility for turning CSV files into styled, accessible HTML tables. macOS 14 or later; Apple Silicon and Intel. No runtime dependencies or paid services.

The Python application is preserved at its original paths and under [`legacy-python/`](legacy-python/). It remains available for comparison and for Windows/Linux. **Do not remove it until the owner manually accepts the native replacement.**

## Using Y.A.C.H.T.

1. **Open CSV** (`⌘O`), drop a file into the window, or use Finder’s **Open With**. The built-in album sample is available from File → Use Sample Table.
2. Choose a delimiter: comma, tab, semicolon, or pipe. TSV files initially select Tab. The first CSV record contains column headers.
3. Adjust typography, cell padding, borders, colors, zebra striping, hover highlighting, and CSS classes in the style controls. Changes update the actual HTML preview automatically.
4. Use **Default (Styled)**, **Unstyled**, or save a named preset. Existing Python presets are copied into the native store on first launch; the original JSON remains unchanged.
5. Switch between **Table Preview** (`⌘1`) and read-only **HTML Source** (`⌘2`). **Copy HTML Code** (`⇧⌘C`) copies the complete document.
6. **Export HTML** (`⌘S`) opens the standard Save dialog, including normal replacement confirmation. After export, **Reveal in Finder** or **Open in Browser** is available.

**Batch Convert CSVs** (`⇧⌘B`) writes `.html` beside each selected input using the current style. Existing output files are skipped with an explanation unless you explicitly choose and confirm replacement. Each result is listed, including partial failures. Dropping multiple files also opens batch conversion.

**Refresh Preview** (`⌘R`) rereads the source CSV. Settings (`⌘,`) controls remembered styles and preview size. File → Open Recent remembers the ten most recently imported files. macOS handles file-dialog directory memory.

### CSV and output behavior

- UTF-8, optional UTF-8 BOM, Unicode and emoji.
- Quoted commas, escaped double quotes, multiline cells, CR/LF/CRLF, blank cells and blank rows.
- Missing cells become blank cells. Extra cells are preserved under added `Column N` headers and reported in the window.
- Malformed quoting and invalid UTF-8 produce useful errors. Export as UTF-8 CSV from your spreadsheet if encoding errors appear. A quote inside an unquoted field must be escaped by quoting the entire field and doubling internal quotes.
- Cell content is always escaped text. There is no raw HTML mode.
- Whole HTML document, embedded CSS, semantic `thead`/`tbody`, `scope="col"` headers. Styled output retains the legacy CSS classes and defaults; Unstyled removes CSS/classes.
- Numbers align right; percentages stay left-aligned. Default widths and alignment match the Python tool.

Preview defaults to 200 rows, with an additional 2 MB estimated size budget. Source display is limited to 1 MB and clearly labels excerpts. **Exports and copied HTML include every row.** Import, generation and streaming export run off the main UI thread. Cancel is available for long operations. The parsed table resides in memory, so practical file size is limited by available RAM; this is not a disk-backed database.

Finder/Dock opening now opens a preview for review. Use batch conversion or the CLI for direct conversion. See [output and workflow changes](docs/COMPATIBILITY.md).

## Development build

Download the `Y.A.C.H.T.-macos-universal` artifact from a successful [Native macOS Actions run](https://github.com/tlolabs/yacht/actions/workflows/native-macos.yml). It includes a DMG, app ZIP, command-line tool, and SHA-256 checksums. These are ad-hoc-signed development builds until Developer ID signing and notarization are configured. Gatekeeper may require explicit user approval for a downloaded development app.

Local build with Xcode 26.6 / Swift 6.3.3:

```sh
./script/build_and_run.sh
```

This builds and opens `dist/Y.A.C.H.T..app`. The Codex Run action uses the same script. Options: `--build-only`, `--verify`, `--debug`, `--logs`, `--telemetry`. The project also supports Swift 6.0+ through SwiftPM; current CI uses Swift 6.3.3.

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
swift test
python3 script/verify_compatibility.py
xcodebuild -project Yacht.xcodeproj -scheme Yacht -derivedDataPath build \
  -destination 'platform=macOS,arch=arm64' test
./script/package.sh
```

UI tests require a logged-in Mac desktop and permission for Xcode’s automation runner. Use `arch=x86_64` for an Intel test host. The universal distribution is cross-compiled for both architectures. [Build, signing, CI and release guide](DISTRIBUTION.md).

## Command-line automation

The native `yacht` executable preserves every Python CLI styling flag:

```sh
swift run yacht input.csv --output output.html --cell-padding 10 --border-style dashed
swift run yacht one.csv two.csv three.csv
swift run yacht input.tsv --delimiter tab --unstyled
swift run yacht input.csv --overwrite
swift run yacht --help
```

Multiple input files produce outputs beside their inputs; `-o`/`--output` is valid only for one input. Existing output replacement requires `--overwrite`. Inputs are never overwritten by conversion. Batch failures do not prevent subsequent files from being processed, and return a nonzero exit status.

## Repository and architecture

| Location | Responsibility |
| --- | --- |
| `Sources/YachtCore` | Incremental CSV parsing, immutable table data, style validation, HTML generation, bounded previews, atomic exports, portable preset JSON |
| `Sources/YachtApp` | SwiftUI app, views, observable workspace/preferences, small WebKit and desktop bridges |
| `Sources/YachtCLI` | Native command-line interface sharing the same core |
| `Tests/YachtCoreTests` | Unit, regression, security, import/export and large-data tests plus Python fixtures |
| `UITests` | Native macOS UI workflow tests |
| `Yacht.xcodeproj` | App and UI-test targets; local Swift package core dependency |
| `script` | Reproducible build, packaging, project generation and differential tests |
| `legacy-python` | Preserved Python implementation and its original documentation/build workflow |
| `docs` | Baseline inventory, compatibility notes, architecture and verification evidence |

[Architecture](docs/ARCHITECTURE.md) · [Baseline](docs/LEGACY-BASELINE.md) · [Compatibility](docs/COMPATIBILITY.md)

## Context and license

This tool supports a teaching workflow. I am an instructor first, and write code when it solves practical problems in classes or a media environment. The project is shared for transparency and educational use, as-is, without warranty or guaranteed support. Bug reports and pull requests are welcome; response times may vary during the academic term.

GNU General Public License v3.0 (GPLv3). See [LICENSE](LICENSE). Distributed modifications must retain the applicable license and source availability requirements. The repository and tagged GitHub releases provide corresponding source.
