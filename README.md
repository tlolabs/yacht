# Y.A.C.H.T. — Yet Another CSV HTML Translator

Python tool to convert CSV files into styled HTML tables.

## Context

This tool was built to support my teaching workflow. I am an instructor first, and I write code when it solves a practical problem in my classes or media environment.

The project is shared publicly under the GNU General Public License v3.0 (GPLv3) for transparency and educational use. It works for my systems and use case.

The software is provided as-is, without warranty or guaranteed support. I maintain it as needed for my own environment. Bug reports and pull requests are welcome, but response times may vary during the academic term.

## Features

- Convert one or multiple CSV files to HTML
- GUI controls for CSS styling (spacing, line styles, colors, zebra striping, hover)
- Batch conversion from the GUI (`Batch Convert CSVs`)
- Copy generated HTML table code to clipboard (`Copy HTML Code`)
- CLI mode for automation
- macOS app bundling with drag-a-CSV-to-icon support

## Run (GUI)

```bash
python3 yacht.py
```

When launched with no arguments, the GUI shows built-in sample preview data (from `BTE.csv` content).  
If `BTE.csv` exists in the project folder, it is also preselected as the startup CSV file.

## Run (CLI)

```bash
python3 yacht.py input.csv
python3 yacht.py input.csv --output output.html --cell-padding 10 --border-style dashed
```

Multiple files:

```bash
python3 yacht.py one.csv two.csv three.csv
```

Each file writes next to the CSV with `.html` extension.

## Build macOS app icon (drag/drop)

1. Install build dependency:

```bash
python3 -m pip install py2app setuptools
```

2. Build:

```bash
python3 setup.py py2app
```

3. Use app:

- Open `dist/Y.A.C.H.T..app`
- Drag a `.csv` file onto the app icon (in Finder or Dock)
- It converts to an `.html` file next to the CSV

## Distribution Builds

GitHub Actions builds packaged versions for macOS Intel, macOS Apple Silicon, Windows, and Linux. See [DISTRIBUTION.md](DISTRIBUTION.md) for release workflow details.

## Notes

- The tool right-aligns numeric cells automatically.
- Percent values (like `17%`) stay left-aligned by default.
- UTF-8 CSV is supported (including UTF-8 with BOM).

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3). See [LICENSE](LICENSE).

GPLv3 is used here because it:

- Requires attribution
- Requires modified redistributed versions to remain under GPLv3
- Prevents incorporation into proprietary closed-source systems
- Requires source distribution when redistributed
- Allows commercial use
- Is widely respected in academic and technical communities
- Signals that reciprocity matters
