#!/usr/bin/env python3
"""Y.A.C.H.T. - Yet Another CSV HTML Translator."""

from __future__ import annotations

import argparse
import csv
import html
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

try:
    import tkinter as tk
    from tkinter import colorchooser, filedialog, messagebox, ttk
except Exception:  # pragma: no cover - GUI is optional in CLI mode
    tk = None
    colorchooser = None
    filedialog = None
    messagebox = None
    ttk = None


NUMERIC_RE = re.compile(r"^\s*[-+]?\d{1,3}(?:,?\d{3})*(?:\.\d+)?\s*$")
DEFAULT_SAMPLE_CSV = Path(__file__).with_name("BTE.csv")
DEFAULT_PREVIEW_HEADER = ["Year", "Album details", "Peak chart position"]
DEFAULT_PREVIEW_ROWS = [
    ["1990", "Surprise", "—"],
    ["1993", "Deluxe", "35"],
    ["1996", "Friction, Baby", "64"],
    ["1998", "How Does Your Garden Grow?", "129"],
    ["2001", "Closer", "110"],
    ["2005", "Before the Robots", "84"],
    ["2009", "Paper Empire", "62"],
    ["2014", "All Together Now", "43"],
    ["2024", "Super Magick", "—"],
]


@dataclass
class StyleOptions:
    table_class: str = "table table-bordered table-hover table-condensed"
    font_family: str = '"Helvetica Neue", Helvetica, Arial, sans-serif'
    font_size_px: int = 14
    cell_padding_px: int = 8
    border_width_px: int = 1
    border_style: str = "solid"
    border_color: str = "#cccccc"
    header_bg: str = "#f5f5f5"
    header_text_color: str = "#222222"
    body_bg: str = "#ffffff"
    zebra_enabled: bool = True
    zebra_bg: str = "#fbfbfb"
    hover_enabled: bool = True
    hover_bg: str = "#f2f8ff"
    border_collapse: str = "collapse"  # collapse or separate
    border_spacing_px: int = 0


def unstyled_options() -> StyleOptions:
    return StyleOptions(
        table_class="",
        font_family="",
        font_size_px=16,
        cell_padding_px=0,
        border_width_px=0,
        border_style="none",
        border_color="#000000",
        header_bg="transparent",
        header_text_color="#000000",
        body_bg="transparent",
        zebra_enabled=False,
        zebra_bg="transparent",
        hover_enabled=False,
        hover_bg="transparent",
        border_collapse="separate",
        border_spacing_px=0,
    )


def options_are_unstyled(options: StyleOptions) -> bool:
    return options == unstyled_options()


def is_numeric(value: str) -> bool:
    if value is None:
        return False
    value = value.strip()
    if not value:
        return False
    if value.endswith("%"):
        return False
    return bool(NUMERIC_RE.match(value))


def generate_html_chunks(header: list[str], rows: Iterable[list[str]], options: StyleOptions) -> Iterable[str]:
    unstyled = options_are_unstyled(options)
    css_lines: list[str] = []
    if not unstyled:
        css_lines = [
            ".csv-table-wrap {",
            f"  font-family: {options.font_family};",
            f"  font-size: {options.font_size_px}px;",
            "}",
            ".csv-table {",
            "  width: 100%;",
            f"  border-collapse: {options.border_collapse};",
            f"  border-spacing: {max(0, options.border_spacing_px)}px;",
            "}",
            ".csv-table th, .csv-table td {",
            f"  border: {max(0, options.border_width_px)}px {options.border_style} {options.border_color};",
            f"  padding: {max(0, options.cell_padding_px)}px;",
            "  vertical-align: top;",
            "}",
            ".csv-table th {",
            f"  background: {options.header_bg};",
            f"  color: {options.header_text_color};",
            "  text-align: left;",
            "}",
            ".csv-table td {",
            f"  background: {options.body_bg};",
            "}",
            ".csv-table td.num {",
            "  text-align: right;",
            "}",
        ]

        if options.zebra_enabled:
            css_lines.extend(
                [
                    ".csv-table tbody tr:nth-child(even) td {",
                    f"  background: {options.zebra_bg};",
                    "}",
                ]
            )

        if options.hover_enabled:
            css_lines.extend(
                [
                    ".csv-table tbody tr:hover td {",
                    f"  background: {options.hover_bg};",
                    "}",
                ]
            )

    thead_cells = []
    for idx, col in enumerate(header, start=1):
        escaped = html.escape(col)
        thead_cells.append(f'<th title="Field #{idx}">{escaped}</th>')

    class_suffix = f" {html.escape(options.table_class)}" if options.table_class else ""
    table_class_attr = " class=\"csv-table" + class_suffix + "\"" if not unstyled else ""

    yield "<!doctype html>\n"
    yield '<html lang="en">\n'
    yield "<head>\n"
    yield '  <meta charset="utf-8">\n'
    yield "  <title>Y.A.C.H.T. Table</title>\n"
    
    if css_lines:
        yield "  <style>\n"
        for line in css_lines:
            yield f"    {line}\n"
        yield "  </style>\n"
        
    yield "</head>\n"
    yield "<body>\n"
    yield ('  <div class="csv-table-wrap">\n' if not unstyled else "  <div>\n")
    yield f"    <table{table_class_attr}>\n"
    yield "      <thead><tr>" + "".join(thead_cells) + "</tr></thead>\n"
    yield "      <tbody>\n"

    for row in rows:
        if len(row) < len(header):
            row = row + [""] * (len(header) - len(row))
        elif len(row) > len(header):
            row = row[: len(header)]

        tds = []
        for cell in row:
            escaped = html.escape(cell)
            cls = " class=\"num\"" if (is_numeric(cell) and not unstyled) else ""
            tds.append(f"<td{cls}>{escaped}</td>")
        yield "        <tr>" + "".join(tds) + "</tr>\n"

    yield "      </tbody>\n"
    yield "    </table>\n"
    yield "  </div>\n"
    yield "</body>\n"
    yield "</html>\n"

def convert_csv_to_html(csv_path: Path, output_path: Path, options: StyleOptions) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f_in, output_path.open("w", encoding="utf-8") as f_out:
        reader = csv.reader(f_in)
        try:
            header = next(reader)
        except StopIteration:
            raise ValueError(f"CSV file is empty: {csv_path}")
        for chunk in generate_html_chunks(header, reader, options):
            f_out.write(chunk)
    return output_path


def render_csv_to_html_doc(csv_path: Path, options: StyleOptions) -> str:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f_in:
        reader = csv.reader(f_in)
        try:
            header = next(reader)
        except StopIteration:
            raise ValueError(f"CSV file is empty: {csv_path}")
        return "".join(generate_html_chunks(header, reader, options))


def default_output_path(csv_path: Path) -> Path:
    return csv_path.with_suffix(".html")


def parse_bool(value: str) -> bool:
    value = value.strip().lower()
    if value in {"1", "true", "yes", "y", "on"}:
        return True
    if value in {"0", "false", "no", "n", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"Invalid boolean value: {value}")


def cli_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Y.A.C.H.T. - Yet Another CSV HTML Translator (CSV to styled HTML tables)"
    )
    parser.add_argument("csv_files", nargs="*", help="Input CSV file path(s)")
    parser.add_argument("-o", "--output", help="Output HTML path (valid only with one input CSV)")
    parser.add_argument("--table-class", default=StyleOptions.table_class)
    parser.add_argument("--font-family", default=StyleOptions.font_family)
    parser.add_argument("--font-size", type=int, default=StyleOptions.font_size_px)
    parser.add_argument("--cell-padding", type=int, default=StyleOptions.cell_padding_px)
    parser.add_argument("--border-width", type=int, default=StyleOptions.border_width_px)
    parser.add_argument("--border-style", default=StyleOptions.border_style)
    parser.add_argument("--border-color", default=StyleOptions.border_color)
    parser.add_argument("--header-bg", default=StyleOptions.header_bg)
    parser.add_argument("--header-text-color", default=StyleOptions.header_text_color)
    parser.add_argument("--body-bg", default=StyleOptions.body_bg)
    parser.add_argument("--zebra", type=parse_bool, default=StyleOptions.zebra_enabled)
    parser.add_argument("--zebra-bg", default=StyleOptions.zebra_bg)
    parser.add_argument("--hover", type=parse_bool, default=StyleOptions.hover_enabled)
    parser.add_argument("--hover-bg", default=StyleOptions.hover_bg)
    parser.add_argument(
        "--border-collapse",
        choices=["collapse", "separate"],
        default=StyleOptions.border_collapse,
    )
    parser.add_argument("--border-spacing", type=int, default=StyleOptions.border_spacing_px)
    return parser


def options_from_args(args: argparse.Namespace) -> StyleOptions:
    return StyleOptions(
        table_class=args.table_class,
        font_family=args.font_family,
        font_size_px=args.font_size,
        cell_padding_px=args.cell_padding,
        border_width_px=args.border_width,
        border_style=args.border_style,
        border_color=args.border_color,
        header_bg=args.header_bg,
        header_text_color=args.header_text_color,
        body_bg=args.body_bg,
        zebra_enabled=args.zebra,
        zebra_bg=args.zebra_bg,
        hover_enabled=args.hover,
        hover_bg=args.hover_bg,
        border_collapse=args.border_collapse,
        border_spacing_px=args.border_spacing,
    )


class CsvToHtmlApp:
    def __init__(self, root: tk.Tk, initial_csv: str | None = None) -> None:
        self.root = root
        self.root.title("Y.A.C.H.T. - Yet Another CSV HTML Translator")
        self.root.geometry("1180x760")

        startup_csv = initial_csv
        if startup_csv is None and DEFAULT_SAMPLE_CSV.exists():
            startup_csv = str(DEFAULT_SAMPLE_CSV)

        self.csv_path_var = tk.StringVar(value=startup_csv or "")
        default_out = default_output_path(Path(startup_csv)).as_posix() if startup_csv else ""
        self.output_path_var = tk.StringVar(value=default_out)

        self.table_class_var = tk.StringVar(value=StyleOptions.table_class)
        self.font_family_var = tk.StringVar(value=StyleOptions.font_family)
        self.font_size_var = tk.IntVar(value=StyleOptions.font_size_px)
        self.cell_padding_var = tk.IntVar(value=StyleOptions.cell_padding_px)
        self.border_width_var = tk.IntVar(value=StyleOptions.border_width_px)
        self.border_style_var = tk.StringVar(value=StyleOptions.border_style)
        self.border_color_var = tk.StringVar(value=StyleOptions.border_color)
        self.header_bg_var = tk.StringVar(value=StyleOptions.header_bg)
        self.header_text_color_var = tk.StringVar(value=StyleOptions.header_text_color)
        self.body_bg_var = tk.StringVar(value=StyleOptions.body_bg)
        self.zebra_enabled_var = tk.BooleanVar(value=StyleOptions.zebra_enabled)
        self.zebra_bg_var = tk.StringVar(value=StyleOptions.zebra_bg)
        self.hover_enabled_var = tk.BooleanVar(value=StyleOptions.hover_enabled)
        self.hover_bg_var = tk.StringVar(value=StyleOptions.hover_bg)
        self.border_collapse_var = tk.StringVar(value=StyleOptions.border_collapse)
        self.border_spacing_var = tk.IntVar(value=StyleOptions.border_spacing_px)
        self.preset_name_var = tk.StringVar(value="")
        self.preset_select_var = tk.StringVar(value="")
        self.color_swatch_labels: dict[str, tk.Label] = {}
        self.preset_store_path = Path.home() / ".yacht_presets.json"
        
        legacy_path = Path(__file__).with_name("yacht_presets.json")
        if legacy_path.exists() and not self.preset_store_path.exists():
            try:
                self.preset_store_path.write_text(legacy_path.read_text(encoding="utf-8"), encoding="utf-8")
            except Exception:
                pass
                
        self.user_presets: dict[str, dict[str, object]] = {}

        self.preview_header = DEFAULT_PREVIEW_HEADER.copy()
        self.preview_rows = [row.copy() for row in DEFAULT_PREVIEW_ROWS]

        self._build_ui()
        self.load_user_presets()
        self.refresh_preset_dropdown()
        self._attach_preview_traces()
        self._attach_color_swatch_traces()
        self._load_preview_data()
        self.update_preview()

    def _build_ui(self) -> None:
        frame = ttk.Frame(self.root, padding=12)
        frame.pack(fill="both", expand=True)

        paned = ttk.Panedwindow(frame, orient="horizontal")
        paned.pack(fill="both", expand=True)

        left_shell = ttk.Frame(paned, padding=(0, 0, 8, 0))
        right = ttk.Frame(paned, padding=(8, 0, 0, 0))
        paned.add(left_shell, weight=2)
        paned.add(right, weight=3)

        self.left_controls_canvas = tk.Canvas(left_shell, highlightthickness=0)
        left_scroll = ttk.Scrollbar(left_shell, orient="vertical", command=self.left_controls_canvas.yview)
        self.left_controls_canvas.configure(yscrollcommand=left_scroll.set)
        self.left_controls_canvas.pack(side="left", fill="both", expand=True)
        left_scroll.pack(side="right", fill="y")

        left = ttk.Frame(self.left_controls_canvas)
        self.left_controls_window = self.left_controls_canvas.create_window((0, 0), window=left, anchor="nw")
        left.bind("<Configure>", self._on_left_content_configure)
        self.left_controls_canvas.bind("<Configure>", self._on_left_canvas_configure)

        left.columnconfigure(1, weight=1)

        ttk.Label(left, text="CSV file").grid(row=0, column=0, sticky="w", padx=(0, 8), pady=4)
        ttk.Entry(left, textvariable=self.csv_path_var).grid(row=0, column=1, sticky="ew", pady=4)
        ttk.Button(left, text="Browse", command=self.choose_csv).grid(row=0, column=2, padx=(8, 0), pady=4)

        ttk.Label(left, text="Output HTML").grid(row=1, column=0, sticky="w", padx=(0, 8), pady=4)
        ttk.Entry(left, textvariable=self.output_path_var).grid(row=1, column=1, sticky="ew", pady=4)
        ttk.Button(left, text="Save As", command=self.choose_output).grid(row=1, column=2, padx=(8, 0), pady=4)

        options_box = ttk.LabelFrame(left, text="Style Controls", padding=10)
        options_box.grid(row=2, column=0, columnspan=3, sticky="nsew", pady=(12, 12))
        options_box.columnconfigure(1, weight=1)
        options_box.columnconfigure(4, weight=1)

        self._labeled_entry(options_box, "Table class", self.table_class_var, 0, 0)
        self._labeled_entry(options_box, "Font family", self.font_family_var, 0, 3)

        self._labeled_spin(options_box, "Font size", self.font_size_var, 1, 0)
        self._labeled_spin(options_box, "Cell padding", self.cell_padding_var, 1, 3)

        self._labeled_spin(options_box, "Border width", self.border_width_var, 2, 0)
        ttk.Label(options_box, text="Border style").grid(row=2, column=3, sticky="w", padx=(0, 8), pady=4)
        ttk.Combobox(
            options_box,
            textvariable=self.border_style_var,
            values=["solid", "dashed", "dotted", "double", "none"],
            state="readonly",
        ).grid(row=2, column=4, sticky="ew", pady=4)

        self._labeled_color_entry(options_box, "Border color", self.border_color_var, 3, 0)
        self._labeled_color_entry(options_box, "Header bg", self.header_bg_var, 3, 3)

        self._labeled_color_entry(options_box, "Header text color", self.header_text_color_var, 4, 0)
        self._labeled_color_entry(options_box, "Body bg", self.body_bg_var, 4, 3)

        ttk.Label(options_box, text="Border collapse").grid(row=5, column=0, sticky="w", padx=(0, 8), pady=4)
        ttk.Combobox(
            options_box,
            textvariable=self.border_collapse_var,
            values=["collapse", "separate"],
            state="readonly",
        ).grid(row=5, column=1, sticky="ew", pady=4)

        self._labeled_spin(options_box, "Border spacing", self.border_spacing_var, 5, 3)

        ttk.Checkbutton(options_box, text="Zebra striping", variable=self.zebra_enabled_var).grid(
            row=6, column=0, sticky="w", pady=4
        )
        self._labeled_color_entry(options_box, "Zebra color", self.zebra_bg_var, 6, 3)

        ttk.Checkbutton(options_box, text="Hover highlight", variable=self.hover_enabled_var).grid(
            row=7, column=0, sticky="w", pady=4
        )
        self._labeled_color_entry(options_box, "Hover color", self.hover_bg_var, 7, 3)

        preset_box = ttk.LabelFrame(left, text="Presets", padding=10)
        preset_box.grid(row=3, column=0, columnspan=3, sticky="ew", pady=(0, 12))
        preset_box.columnconfigure(1, weight=1)

        ttk.Label(preset_box, text="Preset").grid(row=0, column=0, sticky="w", padx=(0, 8), pady=4)
        self.preset_combo = ttk.Combobox(
            preset_box,
            textvariable=self.preset_select_var,
            state="readonly",
        )
        self.preset_combo.grid(row=0, column=1, sticky="ew", pady=4)
        self.preset_load_delete_frame = ttk.Frame(preset_box)
        self.preset_load_delete_frame.grid(row=0, column=2, columnspan=2, padx=(8, 0), pady=4, sticky="ew")
        self.preset_load_button = ttk.Button(self.preset_load_delete_frame, text="Load", command=self.load_selected_preset)
        self.preset_delete_button = ttk.Button(
            self.preset_load_delete_frame, text="Delete", command=self.delete_selected_preset
        )

        ttk.Label(preset_box, text="Save as").grid(row=1, column=0, sticky="w", padx=(0, 8), pady=4)
        ttk.Entry(preset_box, textvariable=self.preset_name_var).grid(row=1, column=1, sticky="ew", pady=4)
        self.preset_utility_frame = ttk.Frame(preset_box)
        self.preset_utility_frame.grid(row=1, column=2, columnspan=3, padx=(8, 0), pady=4, sticky="ew")
        self.preset_save_button = ttk.Button(
            self.preset_utility_frame, text="Save Current", command=self.save_current_preset
        )
        self.preset_unstyled_button = ttk.Button(
            self.preset_utility_frame, text="Reset Unstyled", command=self.reset_unstyled
        )
        self.preset_styled_button = ttk.Button(self.preset_utility_frame, text="Reset Styled", command=self.reset_styled)

        action_box = ttk.Frame(left)
        action_box.grid(row=4, column=0, columnspan=3, sticky="ew")
        action_box.columnconfigure(0, weight=1)

        self.primary_action_frame = ttk.Frame(action_box)
        self.primary_action_frame.grid(row=0, column=0, sticky="ew")
        self.secondary_action_frame = ttk.Frame(action_box)
        self.secondary_action_frame.grid(row=1, column=0, sticky="ew", pady=(8, 0))

        self.generate_button = ttk.Button(self.primary_action_frame, text="Generate HTML", command=self.generate)
        self.batch_button = ttk.Button(self.primary_action_frame, text="Batch Convert CSVs", command=self.generate_batch)
        self.copy_button = ttk.Button(self.primary_action_frame, text="Copy HTML Code", command=self.copy_html_code)

        self.preview_source_button = ttk.Button(
            self.secondary_action_frame, text="Preview HTML Source", command=self.preview_css
        )
        self.refresh_preview_button = ttk.Button(
            self.secondary_action_frame, text="Refresh Preview", command=self.update_preview
        )

        preview_box = ttk.LabelFrame(right, text="WYSIWYG Table Preview", padding=8)
        preview_box.pack(fill="both", expand=True)
        preview_box.rowconfigure(0, weight=1)
        preview_box.columnconfigure(0, weight=1)

        self.preview_canvas = tk.Canvas(preview_box, bg="#ffffff", highlightthickness=0)
        v_scroll = ttk.Scrollbar(preview_box, orient="vertical", command=self.preview_canvas.yview)
        h_scroll = ttk.Scrollbar(preview_box, orient="horizontal", command=self.preview_canvas.xview)
        self.preview_canvas.configure(yscrollcommand=v_scroll.set, xscrollcommand=h_scroll.set)

        self.preview_canvas.grid(row=0, column=0, sticky="nsew")
        v_scroll.grid(row=0, column=1, sticky="ns")
        h_scroll.grid(row=1, column=0, sticky="ew")

        self.preview_note_var = tk.StringVar(
            value="Preview uses first rows from selected CSV. Hover color is not shown in static preview."
        )
        ttk.Label(right, textvariable=self.preview_note_var).pack(anchor="w", pady=(8, 0))
        self._layout_responsive_controls(900)

    def _on_left_content_configure(self, _event: tk.Event) -> None:
        self.left_controls_canvas.configure(scrollregion=self.left_controls_canvas.bbox("all"))

    def _on_left_canvas_configure(self, event: tk.Event) -> None:
        self.left_controls_canvas.itemconfigure(self.left_controls_window, width=event.width)
        self._layout_responsive_controls(event.width)

    @staticmethod
    def _layout_button_group(
        parent: ttk.Frame, buttons: list[ttk.Button], compact: bool, pad_x: int = 6, pad_y: int = 6
    ) -> None:
        for child in parent.winfo_children():
            child.grid_forget()

        if compact:
            parent.columnconfigure(0, weight=1)
            for index, button in enumerate(buttons):
                button.grid(row=index, column=0, sticky="ew", pady=(0, pad_y if index < len(buttons) - 1 else 0))
            return

        for index, button in enumerate(buttons):
            parent.columnconfigure(index, weight=1)
            button.grid(row=0, column=index, sticky="ew", padx=(0, pad_x if index < len(buttons) - 1 else 0))

    def _layout_responsive_controls(self, width: int) -> None:
        compact = width < 820
        self._layout_button_group(
            self.preset_load_delete_frame,
            [self.preset_load_button, self.preset_delete_button],
            compact=compact,
        )
        self._layout_button_group(
            self.preset_utility_frame,
            [self.preset_save_button, self.preset_unstyled_button, self.preset_styled_button],
            compact=compact,
        )
        self._layout_button_group(
            self.primary_action_frame,
            [self.generate_button, self.batch_button, self.copy_button],
            compact=compact,
        )
        self._layout_button_group(
            self.secondary_action_frame,
            [self.preview_source_button, self.refresh_preview_button],
            compact=compact,
        )

    @staticmethod
    def _labeled_entry(parent: ttk.Frame, label: str, variable: tk.Variable, row: int, col: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w", padx=(0, 8), pady=4)
        ttk.Entry(parent, textvariable=variable).grid(row=row, column=col + 1, sticky="ew", pady=4)

    @staticmethod
    def _labeled_spin(parent: ttk.Frame, label: str, variable: tk.IntVar, row: int, col: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w", padx=(0, 8), pady=4)
        ttk.Spinbox(parent, from_=0, to=100, textvariable=variable, width=8).grid(
            row=row, column=col + 1, sticky="w", pady=4
        )

    def _labeled_color_entry(self, parent: ttk.Frame, label: str, variable: tk.StringVar, row: int, col: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w", padx=(0, 8), pady=4)
        container = ttk.Frame(parent)
        container.grid(row=row, column=col + 1, sticky="ew", pady=4)
        container.columnconfigure(1, weight=1)
        swatch = tk.Label(container, width=2, relief="solid", borderwidth=1, cursor="hand2")
        swatch.grid(row=0, column=0, sticky="w")
        swatch.bind("<Button-1>", lambda _e, v=variable: self.pick_color(v))
        self.color_swatch_labels[str(variable)] = swatch
        ttk.Entry(container, textvariable=variable).grid(row=0, column=1, sticky="ew", padx=(6, 0))

    def _attach_color_swatch_traces(self) -> None:
        color_vars = [
            self.border_color_var,
            self.header_bg_var,
            self.header_text_color_var,
            self.body_bg_var,
            self.zebra_bg_var,
            self.hover_bg_var,
        ]
        for var in color_vars:
            var.trace_add("write", lambda *_args, v=var: self._update_color_swatch(v))
            self._update_color_swatch(var)

    def _attach_preview_traces(self) -> None:
        variables = [
            self.table_class_var,
            self.font_family_var,
            self.font_size_var,
            self.cell_padding_var,
            self.border_width_var,
            self.border_style_var,
            self.border_color_var,
            self.header_bg_var,
            self.header_text_color_var,
            self.body_bg_var,
            self.zebra_enabled_var,
            self.zebra_bg_var,
            self.hover_enabled_var,
            self.hover_bg_var,
            self.border_collapse_var,
            self.border_spacing_var,
        ]
        for var in variables:
            var.trace_add("write", lambda *_: self.root.after_idle(self.update_preview))

    def _safe_int(self, value: tk.Variable, fallback: int) -> int:
        try:
            return int(value.get())
        except (TypeError, ValueError, tk.TclError):
            return fallback

    def _safe_color(self, color: str, fallback: str) -> str:
        color = (color or "").strip()
        if not color:
            return fallback
        try:
            self.root.winfo_rgb(color)
            return color
        except tk.TclError:
            return fallback

    def pick_color(self, variable: tk.StringVar) -> None:
        initial = variable.get().strip() or "#ffffff"
        try:
            _, hex_color = colorchooser.askcolor(color=initial, parent=self.root)
        except tk.TclError:
            hex_color = None
        if hex_color:
            variable.set(hex_color)

    def _update_color_swatch(self, variable: tk.StringVar) -> None:
        swatch = self.color_swatch_labels.get(str(variable))
        if not swatch:
            return
        color = self._safe_color(variable.get(), "#ffffff")
        swatch.configure(bg=color)

    def options_to_dict(self, options: StyleOptions) -> dict[str, object]:
        return {
            "table_class": options.table_class,
            "font_family": options.font_family,
            "font_size_px": options.font_size_px,
            "cell_padding_px": options.cell_padding_px,
            "border_width_px": options.border_width_px,
            "border_style": options.border_style,
            "border_color": options.border_color,
            "header_bg": options.header_bg,
            "header_text_color": options.header_text_color,
            "body_bg": options.body_bg,
            "zebra_enabled": options.zebra_enabled,
            "zebra_bg": options.zebra_bg,
            "hover_enabled": options.hover_enabled,
            "hover_bg": options.hover_bg,
            "border_collapse": options.border_collapse,
            "border_spacing_px": options.border_spacing_px,
        }

    def options_from_dict(self, payload: dict[str, object]) -> StyleOptions:
        defaults = StyleOptions()
        return StyleOptions(
            table_class=str(payload.get("table_class", defaults.table_class)),
            font_family=str(payload.get("font_family", defaults.font_family)),
            font_size_px=int(payload.get("font_size_px", defaults.font_size_px)),
            cell_padding_px=int(payload.get("cell_padding_px", defaults.cell_padding_px)),
            border_width_px=int(payload.get("border_width_px", defaults.border_width_px)),
            border_style=str(payload.get("border_style", defaults.border_style)),
            border_color=str(payload.get("border_color", defaults.border_color)),
            header_bg=str(payload.get("header_bg", defaults.header_bg)),
            header_text_color=str(payload.get("header_text_color", defaults.header_text_color)),
            body_bg=str(payload.get("body_bg", defaults.body_bg)),
            zebra_enabled=bool(payload.get("zebra_enabled", defaults.zebra_enabled)),
            zebra_bg=str(payload.get("zebra_bg", defaults.zebra_bg)),
            hover_enabled=bool(payload.get("hover_enabled", defaults.hover_enabled)),
            hover_bg=str(payload.get("hover_bg", defaults.hover_bg)),
            border_collapse=str(payload.get("border_collapse", defaults.border_collapse)),
            border_spacing_px=int(payload.get("border_spacing_px", defaults.border_spacing_px)),
        )

    def apply_options(self, options: StyleOptions) -> None:
        self.table_class_var.set(options.table_class)
        self.font_family_var.set(options.font_family)
        self.font_size_var.set(options.font_size_px)
        self.cell_padding_var.set(options.cell_padding_px)
        self.border_width_var.set(options.border_width_px)
        self.border_style_var.set(options.border_style)
        self.border_color_var.set(options.border_color)
        self.header_bg_var.set(options.header_bg)
        self.header_text_color_var.set(options.header_text_color)
        self.body_bg_var.set(options.body_bg)
        self.zebra_enabled_var.set(options.zebra_enabled)
        self.zebra_bg_var.set(options.zebra_bg)
        self.hover_enabled_var.set(options.hover_enabled)
        self.hover_bg_var.set(options.hover_bg)
        self.border_collapse_var.set(options.border_collapse)
        self.border_spacing_var.set(options.border_spacing_px)
        self.update_preview()

    def load_user_presets(self) -> None:
        if not self.preset_store_path.exists():
            self.user_presets = {}
            return
        try:
            payload = json.loads(self.preset_store_path.read_text(encoding="utf-8"))
            if isinstance(payload, dict):
                self.user_presets = {
                    str(name): data for name, data in payload.items() if isinstance(name, str) and isinstance(data, dict)
                }
            else:
                self.user_presets = {}
        except Exception:
            self.user_presets = {}

    def save_user_presets(self) -> None:
        self.preset_store_path.write_text(
            json.dumps(self.user_presets, indent=2, sort_keys=True),
            encoding="utf-8",
        )

    def refresh_preset_dropdown(self) -> None:
        names = ["Default (Styled)", "Unstyled"] + sorted(self.user_presets.keys())
        self.preset_combo["values"] = names
        if self.preset_select_var.get() not in names:
            self.preset_select_var.set("Default (Styled)")

    def save_current_preset(self) -> None:
        name = self.preset_name_var.get().strip()
        if not name:
            messagebox.showerror("Preset name required", "Enter a preset name before saving.")
            return
        if name in {"Default (Styled)", "Unstyled"}:
            messagebox.showerror("Reserved name", "Choose a different preset name.")
            return
        self.user_presets[name] = self.options_to_dict(self._current_options())
        self.save_user_presets()
        self.refresh_preset_dropdown()
        self.preset_select_var.set(name)
        self.preset_name_var.set("")
        messagebox.showinfo("Preset saved", f"Saved preset: {name}")

    def load_selected_preset(self) -> None:
        name = self.preset_select_var.get().strip()
        if not name:
            return
        if name == "Default (Styled)":
            self.apply_options(StyleOptions())
            return
        if name == "Unstyled":
            self.apply_options(unstyled_options())
            return
        payload = self.user_presets.get(name)
        if payload is None:
            messagebox.showerror("Preset not found", f"Could not find preset: {name}")
            return
        self.apply_options(self.options_from_dict(payload))

    def delete_selected_preset(self) -> None:
        name = self.preset_select_var.get().strip()
        if not name:
            return
        if name in {"Default (Styled)", "Unstyled"}:
            messagebox.showerror("Cannot delete", "Built-in presets cannot be deleted.")
            return
        if name in self.user_presets:
            del self.user_presets[name]
            self.save_user_presets()
            self.refresh_preset_dropdown()
            messagebox.showinfo("Preset deleted", f"Deleted preset: {name}")

    def reset_unstyled(self) -> None:
        self.apply_options(unstyled_options())
        self.preset_select_var.set("Unstyled")

    def reset_styled(self) -> None:
        self.apply_options(StyleOptions())
        self.preset_select_var.set("Default (Styled)")

    def _draw_cell_border(
        self, x1: float, y1: float, x2: float, y2: float, color: str, width: int, style: str
    ) -> None:
        if style == "none" or width <= 0:
            return
        dash = None
        if style == "dashed":
            dash = (8, 5)
        elif style == "dotted":
            dash = (2, 3)
        self.preview_canvas.create_rectangle(x1, y1, x2, y2, outline=color, width=width, dash=dash)
        if style == "double" and width > 1:
            inset = max(2, width + 1)
            self.preview_canvas.create_rectangle(
                x1 + inset, y1 + inset, x2 - inset, y2 - inset, outline=color, width=max(1, width // 2)
            )

    def _load_preview_data(self) -> None:
        csv_raw = self.csv_path_var.get().strip()
        if not csv_raw:
            self.preview_header = ["(No file)"]
            self.preview_rows = [[""]]
            return
        path = Path(csv_raw)
        if not path.exists():
            self.preview_header = ["(Not found)"]
            self.preview_rows = [[""]]
            return
        try:
            with path.open("r", encoding="utf-8-sig", newline="") as f:
                reader = csv.reader(f)
                try:
                    self.preview_header = next(reader)
                except StopIteration:
                    self.preview_header = ["(Empty CSV)"]
                    self.preview_rows = [[""]]
                    return
                rows = []
                for _, row in zip(range(10), reader):
                    rows.append(row)
                self.preview_rows = rows if rows else [[]]
        except Exception as e:
            self.preview_header = ["(Error)"]
            self.preview_rows = [[str(e)]]

    def update_preview(self) -> None:
        self._load_preview_data()
        options = self._current_options()
        canvas = self.preview_canvas
        canvas.delete("all")

        border_color = self._safe_color(options.border_color, "#cccccc")
        header_bg = self._safe_color(options.header_bg, "#f5f5f5")
        header_fg = self._safe_color(options.header_text_color, "#222222")
        body_bg = self._safe_color(options.body_bg, "#ffffff")
        zebra_bg = self._safe_color(options.zebra_bg, "#fbfbfb")

        header = self.preview_header or ["Column 1", "Column 2"]
        rows = self.preview_rows or [["", ""]]

        font_family = options.font_family.split(",")[0].strip().strip('"').strip("'") or "Helvetica"
        font_size = max(8, options.font_size_px)
        pad = max(0, options.cell_padding_px)
        border_width = max(0, options.border_width_px)
        spacing = max(0, options.border_spacing_px) if options.border_collapse == "separate" else 0

        # Estimate reasonable column widths from text lengths.
        col_widths: list[int] = []
        for col_idx, col_name in enumerate(header):
            max_len = len(col_name)
            for row in rows:
                if col_idx < len(row):
                    max_len = max(max_len, len(row[col_idx]))
            width = min(420, max(90, int(max_len * font_size * 0.6) + (2 * pad) + 18))
            col_widths.append(width)

        row_height = font_size + (2 * pad) + 12
        x_origin = 12
        y_origin = 12

        def row_bg(row_idx: int) -> str:
            if row_idx == 0:
                return header_bg
            if options.zebra_enabled and row_idx % 2 == 0:
                return zebra_bg
            return body_bg

        all_rows = [header] + rows
        for r_idx, row in enumerate(all_rows):
            y1 = y_origin + (r_idx * (row_height + spacing))
            y2 = y1 + row_height
            x = x_origin
            fill = row_bg(r_idx)
            for c_idx, width in enumerate(col_widths):
                x1 = x
                x2 = x1 + width
                value = row[c_idx] if c_idx < len(row) else ""
                canvas.create_rectangle(x1, y1, x2, y2, fill=fill, outline="")
                self._draw_cell_border(x1, y1, x2, y2, border_color, border_width, options.border_style)

                if r_idx == 0:
                    anchor = "w"
                    text_x = x1 + pad + 6
                    txt_color = header_fg
                    text_font = (font_family, font_size, "bold")
                else:
                    is_num = is_numeric(value)
                    anchor = "e" if is_num else "w"
                    text_x = (x2 - pad - 6) if is_num else (x1 + pad + 6)
                    txt_color = "#111111"
                    text_font = (font_family, font_size)

                canvas.create_text(
                    text_x,
                    y1 + (row_height / 2),
                    text=value,
                    anchor=anchor,
                    fill=txt_color,
                    font=text_font,
                )
                x = x2 + spacing

        total_width = sum(col_widths) + (spacing * (len(col_widths) - 1)) + 24
        total_height = (len(all_rows) * (row_height + spacing)) - spacing + 24
        canvas.configure(scrollregion=(0, 0, total_width, total_height))

    def choose_csv(self) -> None:
        path = filedialog.askopenfilename(
            title="Choose CSV",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if path:
            self.csv_path_var.set(path)
            if not self.output_path_var.get().strip():
                self.output_path_var.set(default_output_path(Path(path)).as_posix())
            self._load_preview_data()
            self.update_preview()

    def choose_output(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Save HTML As",
            defaultextension=".html",
            filetypes=[("HTML files", "*.html"), ("All files", "*.*")],
        )
        if path:
            self.output_path_var.set(path)

    def _current_options(self) -> StyleOptions:
        return StyleOptions(
            table_class=self.table_class_var.get(),
            font_family=self.font_family_var.get(),
            font_size_px=max(1, self._safe_int(self.font_size_var, StyleOptions.font_size_px)),
            cell_padding_px=max(0, self._safe_int(self.cell_padding_var, StyleOptions.cell_padding_px)),
            border_width_px=max(0, self._safe_int(self.border_width_var, StyleOptions.border_width_px)),
            border_style=self.border_style_var.get(),
            border_color=self.border_color_var.get(),
            header_bg=self.header_bg_var.get(),
            header_text_color=self.header_text_color_var.get(),
            body_bg=self.body_bg_var.get(),
            zebra_enabled=bool(self.zebra_enabled_var.get()),
            zebra_bg=self.zebra_bg_var.get(),
            hover_enabled=bool(self.hover_enabled_var.get()),
            hover_bg=self.hover_bg_var.get(),
            border_collapse=self.border_collapse_var.get(),
            border_spacing_px=max(0, self._safe_int(self.border_spacing_var, StyleOptions.border_spacing_px)),
        )

    def preview_css(self) -> None:
        try:
            csv_path = Path(self.csv_path_var.get().strip())
            if not csv_path.exists():
                raise FileNotFoundError(f"CSV not found: {csv_path}")
            css_preview = render_csv_to_html_doc(csv_path, self._current_options())
        except Exception as e:  # pragma: no cover - GUI path
            messagebox.showerror("Error", str(e))
            return

        win = tk.Toplevel(self.root)
        win.title("Generated HTML Preview")
        win.geometry("760x450")

        text = tk.Text(win, wrap="none")
        text.pack(fill="both", expand=True)
        text.insert("1.0", css_preview)
        text.configure(state="disabled")

    def copy_html_code(self) -> None:
        try:
            csv_path = Path(self.csv_path_var.get().strip())
            if not csv_path.exists():
                raise FileNotFoundError(f"CSV not found: {csv_path}")
            html_doc = render_csv_to_html_doc(csv_path, self._current_options())
            self.root.clipboard_clear()
            self.root.clipboard_append(html_doc)
            self.root.update()
            messagebox.showinfo("Copied", "HTML table code copied to clipboard.")
        except Exception as e:  # pragma: no cover - GUI path
            messagebox.showerror("Copy failed", str(e))

    def generate(self) -> None:
        try:
            csv_path = Path(self.csv_path_var.get().strip())
            if not csv_path.exists():
                raise FileNotFoundError(f"CSV not found: {csv_path}")

            output_raw = self.output_path_var.get().strip()
            output = Path(output_raw) if output_raw else default_output_path(csv_path)

            convert_csv_to_html(csv_path, output, self._current_options())
            messagebox.showinfo("Done", f"Wrote HTML to:\n{output}")
            self.output_path_var.set(output.as_posix())
        except Exception as e:  # pragma: no cover - GUI path
            messagebox.showerror("Conversion failed", str(e))

    def generate_batch(self) -> None:
        csv_files = filedialog.askopenfilenames(
            title="Choose CSV files for batch conversion",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if not csv_files:
            return

        options = self._current_options()
        converted: list[Path] = []
        failures: list[str] = []

        for raw in csv_files:
            csv_path = Path(raw)
            output = default_output_path(csv_path)
            try:
                convert_csv_to_html(csv_path, output, options)
                converted.append(output)
            except Exception as err:
                failures.append(f"{csv_path.name}: {err}")

        if converted:
            self.csv_path_var.set(str(Path(csv_files[0])))
            self.output_path_var.set(converted[0].as_posix())
            self._load_preview_data()
            self.update_preview()

        if failures:
            message = (
                f"Converted {len(converted)} file(s).\n"
                f"Failed {len(failures)} file(s):\n\n" + "\n".join(failures[:10])
            )
            if len(failures) > 10:
                message += f"\n...and {len(failures) - 10} more."
            messagebox.showwarning("Batch finished with errors", message)
            return

        messagebox.showinfo(
            "Batch complete",
            f"Converted {len(converted)} CSV file(s) to HTML next to each source file.",
        )


def run_cli(args: argparse.Namespace) -> int:
    options = options_from_args(args)
    csv_paths = [Path(p).expanduser() for p in args.csv_files]

    if not csv_paths:
        return 1

    if args.output and len(csv_paths) != 1:
        raise SystemExit("--output can only be used with one input CSV")

    for index, csv_path in enumerate(csv_paths):
        if not csv_path.exists():
            raise FileNotFoundError(f"CSV not found: {csv_path}")

        out_path = Path(args.output).expanduser() if (args.output and index == 0) else default_output_path(csv_path)
        written = convert_csv_to_html(csv_path, out_path, options)
        print(f"Converted: {csv_path} -> {written}")

    return 0


def run_gui(initial_csv: str | None = None) -> int:
    if tk is None:
        print("Error: Tkinter is not available in this Python installation. Please install python3-tk or run CLI only.", file=sys.stderr)
        return 1

    root = tk.Tk()
    app = CsvToHtmlApp(root, initial_csv=initial_csv)
    root.mainloop()
    return 0


def main(argv: Iterable[str] | None = None) -> int:
    parser = cli_parser()
    args = parser.parse_args(argv)

    if args.csv_files:
        return run_cli(args)

    # Finder app-drop behavior via py2app can pass the opened file as argv.
    # If no args were supplied, launch GUI.
    return run_gui()


if __name__ == "__main__":
    sys.exit(main())
