#!/usr/bin/python3
"""Run with dbus-run-session -- xvfb-run -a python3 platform/linux/test_ui.py."""

import os, tempfile, sys, time
from pathlib import Path

sandbox = tempfile.TemporaryDirectory(prefix="yacht-gtk-ui-")
os.environ["XDG_CONFIG_HOME"] = sandbox.name + "/config"
os.environ["XDG_DATA_HOME"] = sandbox.name + "/data"
from yacht import Application, Gtk, GLib, CoreError

app = Application()
errors = []
started = time.monotonic()
stage = 0
base = Path(sandbox.name)
fixture = base / "Café.csv"
fixture.write_text("A,B\n<script>,🛥\n1,2,3\n", encoding="utf-8")


def tick():
    global stage
    try:
        assert time.monotonic() - started < 40, "GTK UI timed out"
        w = app.window
        if w is None:
            return True
        if stage == 0:
            # Real application error paths fail this test instead of leaving a modal open.
            def fail(message):
                errors.append(message)
                app.quit()
                return False

            w.error = fail
            assert len(w.controls) == 16
            assert w.get_title() == "YACHT"
            w.load(str(fixture))
            stage = 1
        elif stage == 1 and w.table and w.status.get_text().startswith("Loaded"):
            assert w.table.metadata["row_count"] == 2
            w.set_style(w.core.call("unstyled"))
            w.limit = 50
            w.render()
            stage = 2
        elif stage == 2 and "&lt;script&gt;" in w.source.get_buffer().get_text(
            w.source.get_buffer().get_start_iter(),
            w.source.get_buffer().get_end_iter(),
            True,
        ):
            w.preset_name.set_text("Classroom")
            w.save_preset()
            assert "Classroom" in w.presets
            w.copy()
            stage = 3
        elif stage == 3 and w.status.get_text() == "Copied complete HTML document":
            output = base / "out.html"
            # Same worker and Rust export path invoked after GTK's native Save dialog.
            table = w.table
            style = dict(w.style)
            w.operation(
                lambda token: table.call(
                    "export", path=str(output), style=style, cancel=token
                ),
                lambda _: w.exported(str(output)),
            )
            stage = 4
        elif stage == 4 and (base / "out.html").exists():
            assert "&lt;script&gt;" in (base / "out.html").read_text()
            w.batch([str(fixture)])
            stage = 5
            for window in Gtk.Window.get_toplevels():
                if window.get_title() == "Batch Convert CSVs":

                    def click(widget):
                        if (
                            isinstance(widget, Gtk.Button)
                            and widget.get_label() == "Convert"
                        ):
                            widget.emit("clicked")
                            return True
                        child = widget.get_first_child()
                        while child:
                            if click(child):
                                return True
                            child = child.get_next_sibling()
                        return False

                    assert click(window)
        elif stage == 5 and fixture.with_suffix(".html").exists() and not w.busy:
            assert "<style>" not in fixture.with_suffix(".html").read_text()
            w.settings()
            w.persist()
            assert (base / "config/yacht/ui.ini").exists()
            print(
                "GTK startup, controls, file open, preview/source, style, preset, clipboard, export, settings and batch passed."
            )
            app.quit()
            return False
    except Exception as error:
        errors.append(repr(error))
        app.quit()
        return False
    return True


GLib.timeout_add(100, tick)
app.run([sys.argv[0]])
if app.window:
    app.window.cancel()
    app.window.pool.shutdown(wait=True, cancel_futures=True)
sandbox.cleanup()
if errors:
    raise AssertionError(errors)
if stage != 5:
    raise AssertionError("GTK smoke test incomplete")
