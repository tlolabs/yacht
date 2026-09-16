#!/usr/bin/env python3
"""Exercise the shipped C ABI through the GTK client's real ctypes adapter."""

from pathlib import Path
import sys, tempfile, threading, json, concurrent.futures

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "platform/linux"))
from core import Core, CoreError

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
    presets = {"Teaching": core.call("unstyled")}
    preset_path = str(base / "presets.json")
    core.call("save_presets", path=preset_path, presets=presets)
    assert core.call("load_presets", path=preset_path) == presets
    results = core.call(
        "batch", inputs=[str(base / "missing.csv"), str(source)], style=style
    )
    assert results[0]["error"] and not results[1]["error"]
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
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        assert all(
            pool.map(
                lambda _: table.call("preview", style=style)["html"]
                == table.call("html", style=style),
                range(20),
            )
        )
print(
    "Native ABI: Unicode paths, read, preview, complete export, presets, settings, batch, cancellation and concurrent requests passed."
)
