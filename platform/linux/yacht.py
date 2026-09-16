#!/usr/bin/python3
"""Native GTK 4/libadwaita presentation. CSV, HTML and presets live in Rust."""

import concurrent.futures
import json
import os
from pathlib import Path
import sys
import threading
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("WebKit", "6.0")
from gi.repository import Adw, Gtk, Gdk, Gio, GLib, WebKit
from core import Core, CoreError

POLICY = "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">"
LABELS = {
    "table_class": "Table class",
    "font_family": "Font family",
    "font_size_px": "Font size",
    "cell_padding_px": "Cell padding",
    "border_width_px": "Border width",
    "border_style": "Border style",
    "border_color": "Border color",
    "header_bg": "Header background",
    "header_text_color": "Header text color",
    "body_bg": "Body background",
    "zebra_enabled": "Zebra striping",
    "zebra_bg": "Zebra color",
    "hover_enabled": "Hover highlight",
    "hover_bg": "Hover color",
    "border_collapse": "Border collapse",
    "border_spacing_px": "Border spacing",
}
CHOICES = {
    "border_style": [
        "solid",
        "dashed",
        "dotted",
        "double",
        "none",
        "hidden",
        "groove",
        "ridge",
        "inset",
        "outset",
    ],
    "border_collapse": ["collapse", "separate"],
}


class Window(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(
            application=app, title="YACHT", default_width=1100, default_height=720
        )
        self.core = Core()
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=3)
        self.table = self.core.table()
        self.path = None
        self.last_export = None
        self.preview_cancel = threading.Event()
        self.operation_cancel = threading.Event()
        self.busy = False
        self.loading_style = False
        self.preview_timeout = None
        self.config = Path(GLib.get_user_config_dir()) / "yacht"
        self.preset_path = Path(GLib.get_user_data_dir()) / "yacht" / "presets.json"
        self.preferences = GLib.KeyFile()
        try:
            self.preferences.load_from_file(
                str(self.config / "ui.ini"), GLib.KeyFileFlags.NONE
            )
        except GLib.Error:
            pass
        self.remember = self.pref("remember_style", "true") == "true"
        self.limit = (
            int(self.pref("preview_rows", "200"))
            if self.pref("preview_rows", "200") in ["50", "200", "1000"]
            else 200
        )
        self.recent = []
        self.style = self.core.call("defaults")
        problem = None
        try:
            recent = json.loads(self.pref("recent_files", "[]"))
            if not isinstance(recent, list) or not all(
                isinstance(p, str) for p in recent
            ):
                raise ValueError("Recent files must be a list of paths.")
            self.recent = recent[:10]
        except (ValueError, TypeError) as error:
            problem = "Could not load recent files: " + str(error)
        try:
            if self.remember:
                self.style = self.core.call(
                    "normalize_style", style=json.loads(self.pref("last_style", "{}"))
                )
                self.core.call("validate_style", style=self.style)
        except Exception as error:
            problem = "Could not load last style: " + str(error)
            self.style = self.core.call("defaults")
        try:
            if not self.preset_path.exists():
                legacy = self.core.call(
                    "load_presets", path=str(Path.home() / ".yacht_presets.json")
                )
                if legacy:
                    self.core.call(
                        "save_presets", path=str(self.preset_path), presets=legacy
                    )
            self.presets = self.core.call("load_presets", path=str(self.preset_path))
        except Exception as error:
            problem = str(error)
            self.presets = {}
            self.style = self.core.call("defaults")
        settings = self.core.call(
            "settings",
            settings={
                "remember_style": self.remember,
                "preview_rows": self.limit,
                "last_style": self.style,
            },
        )
        self.style = settings["initial_style"]
        self.build()
        self.connect("close-request", self.close)
        self.render()
        if problem:
            GLib.idle_add(lambda: self.error(problem))

    def pref(self, key, default):
        try:
            return self.preferences.get_string("ui", key)
        except GLib.Error:
            return default

    def persist(self):
        self.core.call(
            "settings",
            settings={
                "remember_style": self.remember,
                "preview_rows": self.limit,
                "last_style": self.style,
            },
        )
        self.config.mkdir(parents=True, exist_ok=True)
        self.preferences.set_string("ui", "remember_style", str(self.remember).lower())
        self.preferences.set_string("ui", "preview_rows", str(self.limit))
        self.preferences.set_string("ui", "recent_files", json.dumps(self.recent))
        if self.remember:
            self.preferences.set_string("ui", "last_style", json.dumps(self.style))
        self.preferences.save_to_file(str(self.config / "ui.ini"))

    def button(self, label, action):
        button = Gtk.Button(label=label)
        button.connect("clicked", lambda _: action())
        return button

    def build(self):
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        header = Adw.HeaderBar()
        header.set_title_widget(
            Adw.WindowTitle(title="YACHT", subtitle="Yet Another CSV HTML Translator")
        )
        header.pack_start(self.button("Open…", lambda: self.pick(False)))
        header.pack_start(self.button("Batch…", lambda: self.pick(True)))
        header.pack_end(self.button("Settings", self.settings))
        menu = Gio.Menu()
        for label, action in [
            ("Open…", "open"),
            ("Batch Convert…", "batch"),
            ("Export HTML…", "export"),
            ("Copy HTML", "copy"),
            ("Table Preview", "preview"),
            ("HTML Source", "source"),
            ("Refresh", "refresh"),
            ("User Guide", "help"),
        ]:
            menu.append(label, "win." + action)
        header.pack_end(Gtk.MenuButton(icon_name="open-menu-symbolic", menu_model=menu))
        outer.append(header)
        toolbar = Gtk.Box(spacing=8, margin_start=12, margin_end=12, margin_bottom=8)
        for title, action in [
            ("Sample", self.sample),
            ("Refresh", self.refresh),
            ("Copy HTML", self.copy),
            ("Export…", self.export),
            ("Cancel", self.cancel),
            ("Reveal", self.reveal),
            ("Browser", self.browser),
        ]:
            toolbar.append(self.button(title, action))
        self.delimiter = Gtk.DropDown.new_from_strings(
            ["comma", "tab", "semicolon", "pipe"]
        )
        self.delimiter.set_tooltip_text("Delimiter")
        self.delimiter.connect("notify::selected", lambda *_: self.refresh())
        toolbar.append(self.delimiter)
        outer.append(toolbar)
        pane = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL, position=340)
        inspector = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=10,
            margin_start=12,
            margin_end=12,
            margin_top=12,
            margin_bottom=12,
        )
        self.preset = Gtk.DropDown.new_from_strings([])
        self.preset.set_tooltip_text("Preset")
        inspector.append(self.preset)
        presets_row = Gtk.Box(spacing=8)
        presets_row.append(self.button("Load", self.load_preset))
        presets_row.append(self.button("Delete", self.delete_preset))
        inspector.append(presets_row)
        self.preset_name = Gtk.Entry(placeholder_text="Save preset as")
        inspector.append(self.preset_name)
        inspector.append(self.button("Save Current", self.save_preset))
        reset = Gtk.Box(spacing=8)
        reset.append(
            self.button(
                "Reset Styled", lambda: self.set_style(self.core.call("defaults"))
            )
        )
        reset.append(
            self.button(
                "Reset Unstyled", lambda: self.set_style(self.core.call("unstyled"))
            )
        )
        inspector.append(reset)
        self.controls = {}
        for key, value in self.style.items():
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            label = Gtk.Label(label=LABELS[key], xalign=0)
            box.append(label)
            if isinstance(value, bool):
                control = Gtk.Switch(active=value, halign=Gtk.Align.START)
                control.connect(
                    "notify::active", lambda w, _, k=key: self.edit(k, w.get_active())
                )
            elif isinstance(value, int):
                control = Gtk.SpinButton.new_with_range(
                    1 if key == "font_size_px" else 0, 10000, 1
                )
                control.set_value(value)
                control.connect(
                    "value-changed", lambda w, k=key: self.edit(k, w.get_value_as_int())
                )
            elif key in CHOICES:
                control = Gtk.DropDown.new_from_strings(CHOICES[key])
                control.set_selected(CHOICES[key].index(value))
                control.connect(
                    "notify::selected",
                    lambda w, _, k=key: self.edit(
                        k, w.get_selected_item().get_string()
                    ),
                )
            else:
                control = Gtk.Entry(text=value)
                control.connect("changed", lambda w, k=key: self.edit(k, w.get_text()))
            control.update_property([Gtk.AccessibleProperty.LABEL], [LABELS[key]])
            label.set_mnemonic_widget(control)
            box.append(control)
            self.controls[key] = control
            if key.endswith("_bg") or key.endswith("_color"):
                color = Gtk.ColorDialogButton(dialog=Gtk.ColorDialog(title=LABELS[key]))
                color.set_tooltip_text(LABELS[key] + " picker")

                def chosen(w, _, k=key):
                    c = w.get_rgba()
                    self.controls[k].set_text(
                        "#%02x%02x%02x"
                        % (
                            round(c.red * 255),
                            round(c.green * 255),
                            round(c.blue * 255),
                        )
                    )

                color.connect("notify::rgba", chosen)
                box.append(color)
            inspector.append(box)
        scroll = Gtk.ScrolledWindow(min_content_width=280)
        scroll.set_child(inspector)
        pane.set_start_child(scroll)
        pane.set_resize_start_child(False)
        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.summary = Gtk.Label(
            label="Built-in sample",
            xalign=0,
            wrap=True,
            selectable=True,
            margin_start=12,
            margin_end=12,
        )
        right.append(self.summary)
        self.recent_picker = Gtk.DropDown.new_from_strings(self.recent)
        recents = Gtk.Box(spacing=8)
        recents.append(self.recent_picker)
        recents.append(self.button("Open Recent", self.open_recent))
        right.append(recents)
        self.stack = Gtk.Stack(vexpand=True, hexpand=True)
        switcher = Gtk.StackSwitcher(stack=self.stack, halign=Gtk.Align.CENTER)
        right.append(switcher)
        self.web = WebKit.WebView(network_session=WebKit.NetworkSession.new_ephemeral())
        self.web.get_settings().set_enable_javascript(False)
        self.web.get_settings().set_enable_html5_local_storage(False)
        self.web.connect("decide-policy", self.web_policy)
        self.web.update_property(
            [Gtk.AccessibleProperty.LABEL], ["Generated table preview"]
        )
        self.stack.add_titled(self.web, "preview", "Table Preview")
        self.source = Gtk.TextView(editable=False, monospace=True)
        self.source.update_property([Gtk.AccessibleProperty.LABEL], ["HTML Source"])
        source_scroll = Gtk.ScrolledWindow()
        source_scroll.set_child(self.source)
        self.stack.add_titled(source_scroll, "source", "HTML Source")
        right.append(self.stack)
        self.note = Gtk.Label(wrap=True)
        right.append(self.note)
        pane.set_end_child(right)
        outer.append(pane)
        pane.set_vexpand(True)
        self.status = Gtk.Label(
            label="Built-in sample · Open a CSV to begin",
            xalign=0,
            wrap=True,
            selectable=True,
            margin_start=12,
            margin_bottom=8,
        )
        outer.append(self.status)
        self.set_content(outer)
        self.update_presets()
        target = Gtk.DropTarget.new(Gdk.FileList, Gdk.DragAction.COPY)
        target.connect(
            "drop",
            lambda _, files, x, y: self.receive(
                [f.get_path() for f in files.get_files()]
            ),
        )
        self.add_controller(target)
        for name, keys, action in [
            ("open", ["<Primary>o"], lambda: self.pick(False)),
            ("export", ["<Primary>s"], self.export),
            ("copy", ["<Primary><Shift>c"], self.copy),
            ("batch", ["<Primary><Shift>b"], lambda: self.pick(True)),
            ("refresh", ["<Primary>r", "F5"], self.refresh),
            (
                "preview",
                ["<Primary>1"],
                lambda: self.stack.set_visible_child_name("preview"),
            ),
            (
                "source",
                ["<Primary>2"],
                lambda: self.stack.set_visible_child_name("source"),
            ),
            (
                "help",
                [],
                lambda: Gio.AppInfo.launch_default_for_uri(
                    "https://github.com/tlolabs/yacht#using-yacht", None
                ),
            ),
        ]:
            a = Gio.SimpleAction.new(name, None)
            a.connect("activate", lambda a, p, fn=action: fn())
            self.add_action(a)
            self.get_application().set_accels_for_action("win." + name, keys)
        self.apply_appearance(self.pref("appearance", "System"))

    def web_policy(self, web, decision, kind):
        if kind == WebKit.PolicyDecisionType.NAVIGATION_ACTION:
            if (
                decision.get_navigation_action().get_request().get_uri()
                != "about:blank"
            ):
                decision.ignore()
                return True
        return False

    def error(self, text):
        self.status.set_text(text)
        dialog = Adw.MessageDialog.new(self, "YACHT", text)
        dialog.add_response("ok", "OK")
        dialog.present()
        return False

    def confirm(self, title, action):
        dialog = Adw.MessageDialog.new(
            self, title, "This action changes existing saved data."
        )
        dialog.add_response("cancel", "Cancel")
        dialog.add_response("confirm", "Continue")
        dialog.set_response_appearance("confirm", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response("cancel")
        dialog.set_close_response("cancel")
        dialog.connect("response", lambda d, r: action() if r == "confirm" else None)
        dialog.present()

    def work(self, fn, done, cancel=None):
        future = self.pool.submit(fn)

        def completed(f):
            def deliver():
                if cancel and cancel.is_set():
                    return False
                try:
                    done(f.result())
                except CoreError as error:
                    if error.code != "cancelled":
                        self.error(str(error))
                except Exception as error:
                    self.error(str(error))
                return False

            GLib.idle_add(deliver)

        future.add_done_callback(completed)

    def edit(self, key, value):
        if self.loading_style:
            return
        self.style[key] = value
        if self.preview_timeout:
            GLib.source_remove(self.preview_timeout)
        self.preview_timeout = GLib.timeout_add(180, self.render)

    def set_style(self, style):
        try:
            self.core.call("validate_style", style=style)
        except CoreError as error:
            self.error(str(error))
            return
        self.style = style
        self.loading_style = True
        for key, value in style.items():
            w = self.controls[key]
            if isinstance(value, bool):
                w.set_active(value)
            elif isinstance(value, int):
                w.set_value(value)
            elif key in CHOICES:
                w.set_selected(CHOICES[key].index(value))
            else:
                w.set_text(value)
        self.loading_style = False
        self.render()

    def render(self):
        self.preview_timeout = None
        self.preview_cancel.set()
        token = threading.Event()
        self.preview_cancel = token
        if not self.table:
            return False
        table = self.table
        style = dict(self.style)
        limit = self.limit

        def done(p):
            self.web.load_html(
                p["html"].replace("<head>", "<head>" + POLICY), "about:blank"
            )
            self.source.get_buffer().set_text(p["source"])
            self.note.set_text(
                (
                    "Cells too large to preview. "
                    if p["preview_unavailable"]
                    else "Preview shows %d of %d rows. "
                    % (p["row_count"], table.metadata["row_count"])
                )
                + ("Source shows the first 1 MB. " if p["source_truncated"] else "")
                + "Copy and Export include every row."
            )
            self.persist()

        self.work(
            lambda: table.call("preview", style=style, limit=limit, cancel=token),
            done,
            token,
        )
        return False

    def separator(self):
        return self.delimiter.get_selected_item().get_string()

    def sample(self):
        if self.busy:
            return
        self.cancel()
        self.table = self.core.table()
        self.path = None
        self.summary.set_text("Built-in sample · 9 rows · 3 columns")
        self.render()

    def refresh(self):
        if self.path:
            self.load(self.path, False)
        else:
            self.render()

    def load(self, path, infer=True):
        if self.busy:
            return
        self.cancel()
        self.path = path
        self.table = None
        self.web.load_html("", "about:blank")
        self.source.get_buffer().set_text("")
        self.operation_cancel = threading.Event()
        token = self.operation_cancel
        if infer and Path(path).suffix.lower() == ".tsv":
            self.delimiter.set_selected(1)
        separator = self.separator()
        self.status.set_text("Reading " + path)

        def done(table):
            self.table = table
            self.summary.set_text(
                "%s · %d rows · %d columns\n%s"
                % (
                    Path(path).name,
                    table.metadata["row_count"],
                    len(table.metadata["header"]),
                    " ".join(table.metadata["warnings"]),
                )
            )
            self.recent = ([path] + [p for p in self.recent if p != path])[:10]
            self.recent_picker.set_model(Gtk.StringList.new(self.recent))
            self.status.set_text("Loaded " + path)
            self.render()

        self.work(
            lambda: self.core.table(
                "read", path=path, delimiter=separator, cancel=token
            ),
            done,
            token,
        )

    def receive(self, paths, batch=False):
        paths = [p for p in paths if p]
        if not paths or self.busy:
            return False
        if len(paths) > 1 or batch:
            self.batch(paths)
        else:
            self.load(paths[0])
        return True

    def pick(self, batch):
        if self.busy:
            return
        dialog = Gtk.FileDialog(title="Open CSV or TSV")

        def selected(d, result):
            try:
                files = d.open_multiple_finish(result)
                self.receive(
                    [files.get_item(i).get_path() for i in range(files.get_n_items())],
                    batch,
                )
            except GLib.Error:
                pass

        dialog.open_multiple(self, None, selected)

    def open_recent(self):
        item = self.recent_picker.get_selected_item()
        if item:
            self.load(item.get_string())

    def cancel(self):
        self.preview_cancel.set()
        self.operation_cancel.set()
        self.status.set_text("Cancellation requested")

    def operation(self, fn, done):
        if self.busy:
            return
        self.busy = True
        self.operation_cancel = threading.Event()
        token = self.operation_cancel

        def perform():
            try:
                return (fn(token), None)
            except Exception as error:
                return (None, error)

        def complete(result):
            self.busy = False
            value, error = result
            if error:
                if isinstance(error, CoreError) and error.code == "cancelled":
                    self.status.set_text("Cancelled")
                else:
                    self.error(str(error))
            else:
                done(value)

        self.work(perform, complete)

    def copy(self):
        if not self.table:
            return
        table = self.table
        style = dict(self.style)

        def done(html):
            self.get_clipboard().set(html)
            self.status.set_text("Copied complete HTML document")

        self.operation(
            lambda token: table.call("html", style=style, cancel=token), done
        )

    def export(self):
        if not self.table or self.busy:
            return
        table = self.table
        style = dict(self.style)
        dialog = Gtk.FileDialog(
            title="Export HTML",
            initial_name=(
                Path(self.path).with_suffix(".html").name
                if self.path
                else "YACHT Table.html"
            ),
        )

        def selected(d, result):
            try:
                path = d.save_finish(result).get_path()
            except GLib.Error:
                return
            self.operation(
                lambda token: table.call(
                    "export", path=path, style=style, overwrite=True, cancel=token
                ),
                lambda _: self.exported(path),
            )

        dialog.save(self, None, selected)

    def exported(self, path):
        self.last_export = path
        self.status.set_text("Exported " + path)

    def reveal(self):
        if self.last_export:
            # org.freedesktop.FileManager1 selects the actual output when supported.
            try:
                proxy = Gio.DBusProxy.new_for_bus_sync(
                    Gio.BusType.SESSION,
                    Gio.DBusProxyFlags.NONE,
                    None,
                    "org.freedesktop.FileManager1",
                    "/org/freedesktop/FileManager1",
                    "org.freedesktop.FileManager1",
                    None,
                )
                proxy.call(
                    "ShowItems",
                    GLib.Variant("(ass)", ([Path(self.last_export).as_uri()], "")),
                    Gio.DBusCallFlags.NONE,
                    -1,
                    None,
                    None,
                )
            except GLib.Error:
                Gio.AppInfo.launch_default_for_uri(
                    Path(self.last_export).parent.as_uri(), None
                )

    def browser(self):
        if self.last_export:
            Gio.AppInfo.launch_default_for_uri(Path(self.last_export).as_uri(), None)

    def batch(self, paths):
        dialog = Adw.Window(
            title="Batch Convert CSVs",
            transient_for=self,
            modal=True,
            default_width=640,
            default_height=420,
        )
        box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=12,
            margin_start=20,
            margin_end=20,
            margin_top=20,
            margin_bottom=20,
        )
        log = Gtk.TextView(editable=False, wrap_mode=Gtk.WrapMode.WORD_CHAR)
        log.get_buffer().set_text("\n".join(paths))
        scroll = Gtk.ScrolledWindow(vexpand=True)
        scroll.set_child(log)
        box.append(scroll)
        overwrite = Gtk.CheckButton(label="Replace existing HTML files")
        box.append(overwrite)
        progress = Gtk.ProgressBar(show_text=True)
        box.append(progress)
        buttons = Gtk.Box(spacing=8)
        box.append(buttons)
        results = []

        def run():
            style = dict(self.style)
            separator = self.separator()
            replace = overwrite.get_active()
            start.set_sensitive(False)
            overwrite.set_sensitive(False)
            done.set_sensitive(False)

            def convert(token):
                for index, path in enumerate(paths):
                    result = self.core.call(
                        "batch",
                        inputs=[path],
                        style=style,
                        delimiter=separator,
                        overwrite=replace,
                        cancel=token,
                    )[0]
                    results.append(result)

                    def update(r=result, i=index, snapshot=list(results)):
                        log.get_buffer().set_text(
                            "\n".join(
                                (
                                    (r["input"] + ": " + r["error"])
                                    if r["error"]
                                    else "Saved " + r["output"]
                                )
                                for r in snapshot
                            )
                        )
                        progress.set_fraction((i + 1) / len(paths))
                        progress.set_text("%d of %d processed" % (i + 1, len(paths)))
                        if not r["error"]:
                            self.last_export = r["output"]
                        return False

                    GLib.idle_add(update)
                return results

            def finish(_):
                start.set_sensitive(True)
                overwrite.set_sensitive(True)
                done.set_sensitive(True)
                self.status.set_text(
                    "Batch processed %d of %d files" % (len(results), len(paths))
                )

            def wrapped(token):
                try:
                    return convert(token)
                finally:
                    GLib.idle_add(
                        lambda: (
                            start.set_sensitive(True),
                            overwrite.set_sensitive(True),
                            done.set_sensitive(True),
                        )
                        and False
                    )

            self.operation(wrapped, finish)

        start = self.button(
            "Convert",
            lambda: (
                self.confirm("Replace existing HTML beside these CSV files?", run)
                if overwrite.get_active()
                else run()
            ),
        )
        buttons.append(start)
        buttons.append(self.button("Cancel", self.cancel))
        done = self.button("Done", dialog.close)
        buttons.append(done)
        dialog.connect("close-request", lambda _: self.busy)
        dialog.set_content(box)
        dialog.present()

    def update_presets(self):
        self.preset.set_model(
            Gtk.StringList.new(["Default (Styled)", "Unstyled"] + sorted(self.presets))
        )

    def load_preset(self):
        name = self.preset.get_selected_item().get_string()
        self.set_style(
            self.core.call("defaults" if name == "Default (Styled)" else "unstyled")
            if name in ["Default (Styled)", "Unstyled"]
            else dict(self.presets[name])
        )

    def save_preset(self):
        name = self.preset_name.get_text().strip()

        def save():
            try:
                next_presets = dict(self.presets)
                next_presets[name] = dict(self.style)
                self.core.call(
                    "save_presets", path=str(self.preset_path), presets=next_presets
                )
                self.presets = next_presets
                self.update_presets()
                self.status.set_text("Saved preset " + name)
            except Exception as error:
                self.error(str(error))

        if name in self.presets:
            self.confirm("Replace preset " + name + "?", save)
        else:
            save()

    def delete_preset(self):
        name = self.preset.get_selected_item().get_string()
        if name not in self.presets:
            return

        def remove():
            try:
                next_presets = dict(self.presets)
                del next_presets[name]
                self.core.call(
                    "save_presets", path=str(self.preset_path), presets=next_presets
                )
                self.presets = next_presets
                self.update_presets()
            except Exception as error:
                self.error(str(error))

        self.confirm("Delete preset " + name + "?", remove)

    def apply_appearance(self, value):
        Adw.StyleManager.get_default().set_color_scheme(
            {
                "System": Adw.ColorScheme.DEFAULT,
                "Light": Adw.ColorScheme.FORCE_LIGHT,
                "Dark": Adw.ColorScheme.FORCE_DARK,
            }[value]
        )
        self.preferences.set_string("ui", "appearance", value)

    def settings(self):
        window = Adw.PreferencesWindow(
            transient_for=self, modal=True, title="YACHT Settings"
        )
        page = Adw.PreferencesPage()
        group = Adw.PreferencesGroup(title="Preferences")
        page.add(group)
        window.add(page)
        remember = Adw.SwitchRow(
            title="Remember last-used table style", active=self.remember
        )
        remember.connect(
            "notify::active", lambda w, _: setattr(self, "remember", w.get_active())
        )
        group.add(remember)
        limit = Adw.ComboRow(
            title="Maximum preview rows",
            model=Gtk.StringList.new(["50", "200", "1000"]),
            selected=[50, 200, 1000].index(self.limit),
        )
        limit.connect(
            "notify::selected",
            lambda w, _: (
                setattr(self, "limit", [50, 200, 1000][w.get_selected()]),
                self.render(),
            ),
        )
        group.add(limit)
        appearance = Adw.ComboRow(
            title="Appearance",
            model=Gtk.StringList.new(["System", "Light", "Dark"]),
            selected=["System", "Light", "Dark"].index(
                self.pref("appearance", "System")
            ),
        )
        appearance.connect(
            "notify::selected",
            lambda w, _: self.apply_appearance(
                ["System", "Light", "Dark"][w.get_selected()]
            ),
        )
        group.add(appearance)
        clear = Adw.ActionRow(title="Recent files")
        clear.add_suffix(
            self.button(
                "Clear",
                lambda: (
                    self.recent.clear(),
                    self.recent_picker.set_model(Gtk.StringList.new([])),
                    self.persist(),
                ),
            )
        )
        group.add(clear)
        group.add(
            Adw.ActionRow(
                title="YACHT " + self.core.call("info")["version"],
                subtitle="Yet Another CSV HTML Translator · GPLv3",
            )
        )
        window.connect("close-request", lambda _: (self.persist(), False)[1])
        window.present()

    def close(self, _):
        self.cancel()
        self.persist()
        self.pool.shutdown(wait=False, cancel_futures=True)
        return False


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="com.local.yacht.csvhtmltranslator",
            flags=Gio.ApplicationFlags.HANDLES_OPEN,
        )
        self.window = None

    def do_activate(self):
        if not self.window:
            self.window = Window(self)
        self.window.present()

    def do_open(self, files, n_files, hint):
        self.do_activate()
        paths = [f.get_path() for f in files]

        def deliver():
            if self.window.busy:
                return True
            self.window.receive(paths)
            return False

        GLib.timeout_add(100, deliver)


if __name__ == "__main__":
    raise SystemExit(Application().run(sys.argv))
