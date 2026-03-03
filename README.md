# Y.A.C.H.T. — Yet Another CSV HTML Translator

Python tool to convert CSV files into styled HTML tables.

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

## Notes

- The tool right-aligns numeric cells automatically.
- Percent values (like `17%`) stay left-aligned by default.
- UTF-8 CSV is supported (including UTF-8 with BOM).
