#!/usr/bin/env python3
"""Exercise the shipped C ABI through an independent ctypes test adapter."""

from pathlib import Path
import sys, tempfile, threading, json, concurrent.futures

root = Path(__file__).resolve().parents[1]
from binding_test_support import Core, CoreError

core = Core(sys.argv[1])
with tempfile.TemporaryDirectory(prefix="yacht-binding-") as directory:
    base = Path(directory)
    source = base / "Café 🛥.csv"
    source.write_text("A,B\n<script>,🛥\n1,2,3\n", encoding="utf-8")
    table = core.table("read", path=str(source))
    style = core.call("defaults")
    assert table.metadata["row_count"] == 2 and len(table.metadata["header"]) == 3
    assert "extra cells" in table.metadata["warnings"][0]
    p = table.call("preview", style=style, limit=1)
    assert (
        p["row_count"] == 1
        and "&lt;script&gt;" in p["html"]
        and "3</td>" in p["source"]
    )
    output = base / "out.html"
    table.call("export", path=str(output), style=style)
    assert output.read_text(encoding="utf-8") == table.call("html", style=style)
    fragment = table.call("html", style=style)
    assert fragment.lstrip().startswith("<table")
    assert fragment.rstrip().endswith("</table>")
    assert all(
        wrapper not in fragment
        for wrapper in ("<!doctype", "<html", "<head", "<body", "<div", "<style")
    )
    assert "style=" in fragment and "&lt;script&gt;" in fragment
    assert p["source"] == fragment
    document = table.call("html", style=style, output_mode="document")
    assert document.startswith("<!doctype html>") and "<style>" in document
    complete_output = base / "complete.html"
    table.call(
        "export", path=str(complete_output), style=style, output_mode="document"
    )
    assert complete_output.read_text(encoding="utf-8") == document
    assert table.call("preview", style=style, output_mode="document")["source"] == document
    bare = table.call("html", style=core.call("unstyled"))
    assert bare.lstrip().startswith("<table>")
    assert "style=" not in bare and "class=" not in bare
    builtins = core.call("builtin_presets")
    google_names = {
        "G-Gray", "G-Cyan", "G-Green", "G-Yellow", "G-Orange", "G-Blue",
        "G-Teal", "G-Blue Gray", "G-Brown", "G-Light Green", "G-Purple",
    }
    assert set(builtins) == google_names | {
        "Y-Default", "Unstyled", "N-Blue", "N-Orange", "N-Green",
        "N-Red", "N-Gray", "N-White",
    }
    assert builtins["Y-Default"] == style
    assert all(builtins[name]["first_column_enabled"] for name in builtins if name.startswith("N-"))
    assert not builtins["N-Gray"]["zebra_enabled"]
    for name in google_names:
        google_style = builtins[name]
        assert google_style["zebra_enabled"], name
        assert not google_style["first_column_enabled"], name
        google_html = table.call("html", style=google_style)
        assert google_html.startswith("<table"), name
        assert google_style["header_bg"] in google_html, name
        assert google_style["zebra_bg"] in google_html, name
    before = output.read_bytes()
    try:
        table.call("export", path=str(output), style=style)
    except CoreError:
        pass
    else:
        raise AssertionError("overwrite accepted")
    cancelled = threading.Event()
    cancelled.set()
    try:
        table.call(
            "export", path=str(output), style=style, overwrite=True, cancel=cancelled
        )
    except CoreError as e:
        assert e.code == "cancelled"
    else:
        raise AssertionError("cancellation ignored")
    assert output.read_bytes() == before
    try:
        table.call("export", path=str(output), overwrite=True, output_mode="invalid")
    except CoreError:
        pass
    else:
        raise AssertionError("invalid output mode accepted")
    assert output.read_bytes() == before
    presets = {"Teaching": core.call("unstyled")}
    preset_path = str(base / "presets.json")
    core.call("save_presets", path=preset_path, presets=presets)
    assert core.call("load_presets", path=preset_path) == presets
    results = core.call(
        "batch", inputs=[str(base / "missing.csv"), str(source)], style=style
    )
    assert results[0]["error"] and not results[1]["error"]
    assert Path(results[1]["output"]).read_text(encoding="utf-8") == fragment
    results = core.call(
        "batch", inputs=[str(source)], style=style,
        output_mode="document", overwrite=True,
    )
    assert not results[0]["error"]
    assert Path(results[0]["output"]).read_text(encoding="utf-8") == document
    assert (
        core.call(
            "settings",
            settings={
                "remember_style": False,
                "preview_rows": 50,
                "last_style": presets["Teaching"],
            },
        )["initial_style"]
        == style
    )
    assert core.call("settings")["settings"]["output_mode"] == "table"
    assert core.call("settings", settings={"output_mode": "document"})["settings"]["output_mode"] == "document"
    normalized = core.call("normalize_style", style={"font_size_px": 22})
    assert normalized["font_size_px"] == 22
    assert normalized["first_column_enabled"] is False
    assert normalized["first_column_bg"] == "#ffffff"
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        assert all(
            pool.map(
                lambda _: table.call("preview", style=style)["source"]
                == table.call("html", style=style),
                range(20),
            )
        )
print(
    "Native ABI: Unicode paths, read, preview, table/document exports, built-in styles, legacy presets/settings, batch, cancellation and concurrent requests passed."
)
