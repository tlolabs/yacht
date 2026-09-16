using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System.Diagnostics;
using System.Text.Json.Nodes;
using Windows.ApplicationModel.DataTransfer;
using Windows.Storage.Pickers;
using Windows.System;
using Microsoft.Web.WebView2.Core;
namespace YACHT;
public sealed partial class MainWindow : Window
{
    private Core.Table? table;
    private JsonObject style = Core.Style(), presets = new();
    private Preferences preferences = new();
    private string? sourcePath, lastExport;
    private CancellationTokenSource previewCancel = new(), operationCancel = new(), importCancel = new();
    private bool working, changingStyle, changingDelimiter;
    private readonly Dictionary<string, Control> controls = [];
    private readonly ComboBox delimiter = new() { ItemsSource = new[] { "comma", "tab", "semicolon", "pipe" }, SelectedIndex = 0, Header = "Delimiter" };
    private readonly ComboBox preset = new() { Header = "Preset" }, recent = new() { Header = "Open Recent" };
    private readonly TextBox presetName = new() { Header = "Save preset as" };
    private readonly TextBlock summary = new() { Text = "Built-in sample", TextWrapping = TextWrapping.Wrap }, status = new() { Text = "Open a CSV to begin", TextWrapping = TextWrapping.Wrap }, note = new() { TextWrapping = TextWrapping.Wrap };
    private readonly WebView2 web = new();
    private readonly TextBox source = new() { IsReadOnly = true, AcceptsReturn = true, FontFamily = new Microsoft.UI.Xaml.Media.FontFamily("Consolas"), TextWrapping = TextWrapping.NoWrap };
    private readonly Pivot views = new();
    private readonly StackPanel stylePanel = new() { Spacing = 10, Padding = new Thickness(12) };
    private Task? webReady;
    private static readonly Dictionary<string, string> Labels = new() { ["table_class"] = "Table class", ["font_family"] = "Font family", ["font_size_px"] = "Font size", ["cell_padding_px"] = "Cell padding", ["border_width_px"] = "Border width", ["border_style"] = "Border style", ["border_color"] = "Border color", ["header_bg"] = "Header background", ["header_text_color"] = "Header text color", ["body_bg"] = "Body background", ["zebra_enabled"] = "Zebra striping", ["zebra_bg"] = "Zebra color", ["hover_enabled"] = "Hover highlight", ["hover_bg"] = "Hover color", ["border_collapse"] = "Border collapse", ["border_spacing_px"] = "Border spacing" };
    private static readonly Dictionary<string, string[]> Choices = new() { ["border_style"] = ["solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset", "outset"], ["border_collapse"] = ["collapse", "separate"] };
    private const string Policy = "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">";
    public MainWindow()
    {
        InitializeComponent();
        AppWindow.Resize(new Windows.Graphics.SizeInt32(1100, 760));
        string? problem = null;
        try
        {
            preferences = Preferences.Load();
            if (preferences.RememberStyle && preferences.LastStyle is not null) { style = Core.Call("normalize_style", new { style = preferences.LastStyle })!.AsObject(); Core.Call("validate_style", new { style }); }
            if (!File.Exists(Preferences.PresetPath))
            {
                var legacy = Core.Call("load_presets", new { path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".yacht_presets.json") })!.AsObject();
                if (legacy.Count > 0) Core.Call("save_presets", new { path = Preferences.PresetPath, presets = legacy });
            }
            presets = Core.Call("load_presets", new { path = Preferences.PresetPath })!.AsObject();
        }
        catch (Exception e) { problem = e.Message; style = Core.Style(); }
        style = Core.Call("settings", new { settings = new { remember_style = preferences.RememberStyle, preview_rows = preferences.PreviewRows, last_style = style } })!["initial_style"]!.AsObject();
        Build(); table = new(Core.Call("sample")!.AsObject());
        Root.Loaded += async (_, _) => { webReady = InitializeWeb(); await Render(); if (problem is not null) await Error(problem); };
        Closed += (_, _) => { Cancel(); importCancel.Cancel(); SavePreferences(); };
    }
    private Button Button(string label, Func<Task> action)
    {
        var b = new Button { Content = label }; AutomationProperties.SetName(b, label);
        b.Click += async (_, _) => { try { await action(); } catch (OperationCanceledException) { status.Text = "Cancelled"; } catch (Exception e) { await Error(e.Message); } }; return b;
    }
    private Task Do(Action action) { action(); return Task.CompletedTask; }
    private void Build()
    {
        Root.RowDefinitions.Add(new() { Height = GridLength.Auto }); Root.RowDefinitions.Add(new()); Root.RowDefinitions.Add(new() { Height = GridLength.Auto });
        var bar = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(12) };
        var menu = new MenuBar();
        var fileMenu = new MenuBarItem { Title = "File" }; var viewMenu = new MenuBarItem { Title = "View" }; var helpMenu = new MenuBarItem { Title = "Help" };
        void Item(MenuBarItem parent, string title, Func<Task> action) { var item = new MenuFlyoutItem { Text = title }; item.Click += async (_, _) => { try { await action(); } catch (Exception e) { await Error(e.Message); } }; parent.Items.Add(item); }
        Item(fileMenu, "Open…", () => Pick(false)); Item(fileMenu, "Batch Convert…", () => Pick(true)); Item(fileMenu, "Export HTML…", Export); Item(fileMenu, "Copy HTML", Copy);
        Item(viewMenu, "Table Preview", () => Do(() => views.SelectedIndex = 0)); Item(viewMenu, "HTML Source", () => Do(() => views.SelectedIndex = 1)); Item(viewMenu, "Refresh", Refresh); Item(viewMenu, "Settings", Settings);
        Item(helpMenu, "User Guide", () => Do(() => Process.Start(new ProcessStartInfo("https://github.com/tlolabs/yacht#using-yacht") { UseShellExecute = true })));
        menu.Items.Add(fileMenu); menu.Items.Add(viewMenu); menu.Items.Add(helpMenu); bar.Children.Add(menu);
        bar.Children.Add(Button("Open…", () => Pick(false))); bar.Children.Add(Button("Batch…", () => Pick(true))); bar.Children.Add(Button("Sample", Sample));
        bar.Children.Add(Button("Refresh", Refresh)); bar.Children.Add(Button("Copy HTML", Copy)); bar.Children.Add(Button("Export…", Export));
        bar.Children.Add(Button("Cancel", () => Do(Cancel))); bar.Children.Add(Button("Settings", Settings)); Root.Children.Add(new ScrollViewer { Content = bar, HorizontalScrollBarVisibility = ScrollBarVisibility.Auto, VerticalScrollBarVisibility = ScrollBarVisibility.Disabled });
        var body = new Grid(); Grid.SetRow(body, 1); body.ColumnDefinitions.Add(new() { Width = new GridLength(330) }); body.ColumnDefinitions.Add(new()); Root.Children.Add(body);
        stylePanel.Children.Add(preset);
        var presetButtons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 }; presetButtons.Children.Add(Button("Load", LoadPreset)); presetButtons.Children.Add(Button("Delete", DeletePreset)); stylePanel.Children.Add(presetButtons);
        stylePanel.Children.Add(presetName); stylePanel.Children.Add(Button("Save Current", SavePreset));
        var resets = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 }; resets.Children.Add(Button("Reset Styled", () => SetStyle(Core.Style()))); resets.Children.Add(Button("Reset Unstyled", () => SetStyle(Core.Style("unstyled")))); stylePanel.Children.Add(resets);
        foreach (var property in style)
        {
            var key = property.Key; var value = property.Value!; Control control;
            if (value is JsonValue boolean && boolean.TryGetValue<bool>(out var active))
            {
                var toggle = new ToggleSwitch { Header = Labels[key], IsOn = active }; toggle.Toggled += (_, _) => Edit(key, JsonValue.Create(toggle.IsOn)!); control = toggle;
            }
            else if (value is JsonValue number && number.TryGetValue<long>(out var n))
            {
                var box = new NumberBox { Header = Labels[key], Minimum = key == "font_size_px" ? 1 : 0, Maximum = 10000, Value = n, SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Compact };
                box.ValueChanged += (_, _) => { if (!double.IsNaN(box.Value)) Edit(key, JsonValue.Create((long)box.Value)!); }; control = box;
            }
            else if (Choices.TryGetValue(key, out var choices))
            {
                var box = new ComboBox { Header = Labels[key], ItemsSource = choices, SelectedItem = value.GetValue<string>() }; box.SelectionChanged += (_, _) => { if (box.SelectedItem is string text) Edit(key, JsonValue.Create(text)!); }; control = box;
            }
            else
            {
                var box = new TextBox { Header = Labels[key], Text = value.GetValue<string>() }; box.TextChanged += (_, _) => Edit(key, JsonValue.Create(box.Text)!); control = box;
            }
            AutomationProperties.SetName(control, Labels[key]); controls[key] = control; stylePanel.Children.Add(control);
            if (key.EndsWith("_bg") || key.EndsWith("_color")) stylePanel.Children.Add(Button(Labels[key] + " picker", async () =>
            {
                var picker = new ColorPicker { IsAlphaEnabled = false }; var result = await Dialog(Labels[key], picker, "Use Color");
                if (result == ContentDialogResult.Primary) ((TextBox)controls[key]).Text = $"#{picker.Color.R:x2}{picker.Color.G:x2}{picker.Color.B:x2}";
            }));
        }
        body.Children.Add(new ScrollViewer { Content = stylePanel, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled });
        var right = new Grid { Margin = new Thickness(12) }; Grid.SetColumn(right, 1); right.RowDefinitions.Add(new() { Height = GridLength.Auto }); right.RowDefinitions.Add(new()); right.RowDefinitions.Add(new() { Height = GridLength.Auto }); body.Children.Add(right);
        var info = new StackPanel { Spacing = 8 }; info.Children.Add(summary);
        var inputs = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 }; inputs.Children.Add(delimiter); inputs.Children.Add(recent); inputs.Children.Add(Button("Open Recent", async () => { if (recent.SelectedItem is string path) await Load(path); })); info.Children.Add(inputs); right.Children.Add(info);
        delimiter.SelectionChanged += async (_, _) => { if (!changingDelimiter) await Refresh(); };
        views.Items.Add(new PivotItem { Header = "Table Preview", Content = web }); views.Items.Add(new PivotItem { Header = "HTML Source", Content = source }); Grid.SetRow(views, 1); right.Children.Add(views);
        AutomationProperties.SetName(web, "Generated table preview"); AutomationProperties.SetName(source, "HTML Source"); Grid.SetRow(note, 2); right.Children.Add(note);
        var footer = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12, Margin = new Thickness(12) }; footer.Children.Add(status); footer.Children.Add(Button("Reveal in Explorer", Reveal)); footer.Children.Add(Button("Open in Browser", Browser)); Grid.SetRow(footer, 2); Root.Children.Add(footer);
        Root.AllowDrop = true; Root.DragOver += (_, e) => e.AcceptedOperation = DataPackageOperation.Copy;
        Root.Drop += async (_, e) => { if (e.DataView.Contains(StandardDataFormats.StorageItems)) { var items = await e.DataView.GetStorageItemsAsync(); await Receive(items.Select(i => i.Path).ToArray()); } };
        Shortcut(VirtualKey.O, VirtualKeyModifiers.Control, () => Pick(false)); Shortcut(VirtualKey.S, VirtualKeyModifiers.Control, Export);
        Shortcut(VirtualKey.C, VirtualKeyModifiers.Control | VirtualKeyModifiers.Shift, Copy); Shortcut(VirtualKey.B, VirtualKeyModifiers.Control | VirtualKeyModifiers.Shift, () => Pick(true));
        Shortcut(VirtualKey.R, VirtualKeyModifiers.Control, Refresh); Shortcut(VirtualKey.F5, VirtualKeyModifiers.None, Refresh);
        Shortcut(VirtualKey.Number1, VirtualKeyModifiers.Control, () => Do(() => views.SelectedIndex = 0)); Shortcut(VirtualKey.Number2, VirtualKeyModifiers.Control, () => Do(() => views.SelectedIndex = 1));
        UpdatePresets(); UpdateRecent(); ApplyAppearance();
    }
    private void Shortcut(VirtualKey key, VirtualKeyModifiers modifiers, Func<Task> action)
    {
        var accelerator = new KeyboardAccelerator { Key = key, Modifiers = modifiers }; accelerator.Invoked += async (_, e) => { e.Handled = true; try { await action(); } catch (Exception ex) { await Error(ex.Message); } }; Root.KeyboardAccelerators.Add(accelerator);
    }
    private async Task InitializeWeb()
    {
        await web.EnsureCoreWebView2Async();
        web.CoreWebView2.Settings.IsScriptEnabled = false; web.CoreWebView2.Settings.AreDevToolsEnabled = false;
        web.CoreWebView2.Settings.AreDefaultContextMenusEnabled = false;
        web.CoreWebView2.NavigationStarting += (_, e) => { if (e.Uri != "about:blank") e.Cancel = true; };
        web.CoreWebView2.NewWindowRequested += (_, e) => e.Handled = true;
        web.CoreWebView2.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
        web.CoreWebView2.WebResourceRequested += (_, e) => { e.Response = web.CoreWebView2.Environment.CreateWebResourceResponse(null, 403, "Blocked", ""); };
    }
    private async Task<ContentDialogResult> Dialog(string title, object content, string primary = "OK")
    {
        var dialog = new ContentDialog { XamlRoot = Root.XamlRoot, Title = title, Content = content, PrimaryButtonText = primary, CloseButtonText = "Cancel", DefaultButton = ContentDialogButton.Close }; return await dialog.ShowAsync();
    }
    private async Task Error(string message) { status.Text = message; await Dialog("YACHT", new TextBlock { Text = message, TextWrapping = TextWrapping.Wrap }); }
    private void Edit(string key, JsonNode value) { if (changingStyle || JsonNode.DeepEquals(style[key], value)) return; style[key] = value; _ = Render(); }
    private async Task SetStyle(JsonObject value)
    {
        changingStyle = true; style = value;
        foreach (var p in style) { switch (controls[p.Key]) { case ToggleSwitch t: t.IsOn = p.Value!.GetValue<bool>(); break; case NumberBox n: n.Value = p.Value!.GetValue<long>(); break; case ComboBox c: c.SelectedItem = p.Value!.GetValue<string>(); break; case TextBox t: t.Text = p.Value!.GetValue<string>(); break; } }
        changingStyle = false; await Render();
    }
    private async Task Render()
    {
        previewCancel.Cancel(); previewCancel = new(); var token = previewCancel.Token; var current = table; var options = style.DeepClone(); var limit = preferences.PreviewRows;
        if (current is null) return;
        try
        {
            await Task.Delay(180, token);
            var p = await Task.Run(() => current.Call("preview", new { style = options, limit }, token)!, token); token.ThrowIfCancellationRequested();
            if (webReady is not null) await webReady; token.ThrowIfCancellationRequested();
            web.NavigateToString(p["html"]!.GetValue<string>().Replace("<head>", "<head>" + Policy)); source.Text = p["source"]!.GetValue<string>();
            note.Text = (p["preview_unavailable"]!.GetValue<bool>() ? "Cells too large to preview. " : $"Preview shows {p["row_count"]} of {current.Metadata["row_count"]} rows. ") + (p["source_truncated"]!.GetValue<bool>() ? "Source shows the first 1 MB. " : "") + "Copy and Export include every row.";
            if (preferences.RememberStyle) preferences.LastStyle = options.AsObject(); SavePreferences();
        }
        catch (OperationCanceledException) { }
        catch (Exception e) { if (!token.IsCancellationRequested) { status.Text = e.Message; source.Text = ""; if (web.CoreWebView2 is not null) web.NavigateToString(""); } }
    }
    private async Task Load(string path, bool infer = true)
    {
        if (working) return; importCancel.Cancel(); importCancel = new(); var token = importCancel.Token; previewCancel.Cancel(); table = null; sourcePath = path;
        source.Text = ""; if (web.CoreWebView2 is not null) web.NavigateToString("");
        if (infer && Path.GetExtension(path).Equals(".tsv", StringComparison.OrdinalIgnoreCase)) { changingDelimiter = true; delimiter.SelectedItem = "tab"; changingDelimiter = false; }
        var separator = (string)delimiter.SelectedItem; status.Text = "Reading " + path;
        try
        {
            var result = await Task.Run(() => Core.Read(path, separator, token), token); token.ThrowIfCancellationRequested(); table = result;
            summary.Text = $"{Path.GetFileName(path)} · {result.Metadata["row_count"]} rows · {result.Metadata["header"]!.AsArray().Count} columns\n" + string.Join(" ", result.Metadata["warnings"]!.AsArray().Select(x => x!.GetValue<string>()));
            preferences.Recent.Remove(path); preferences.Recent.Insert(0, path); preferences.Recent = preferences.Recent.Take(10).ToList(); UpdateRecent(); status.Text = "Loaded " + path; await Render();
        }
        catch (OperationCanceledException) { }
        catch (Exception e) { if (!token.IsCancellationRequested) await Error(e.Message); }
    }
    private readonly List<string> pendingFiles = [];
    private CancellationTokenSource activationCancel = new();
    public void ReceiveAfterLoad(string[] paths)
    {
        Root.Loaded += (_, _) => QueueFiles(paths);
    }
    public async void QueueFiles(string[] paths)
    {
        pendingFiles.AddRange(paths);
        activationCancel.Cancel(); activationCancel = new(); var token = activationCancel.Token;
        try
        {
            await Task.Delay(300, token);
            while (working) await Task.Delay(100, token);
            var files = pendingFiles.ToArray(); pendingFiles.Clear();
            await Receive(files);
        }
        catch (OperationCanceledException) { }
        catch (Exception e) { await Error(e.Message); }
    }

    private async Task Receive(string[] paths, bool batch = false) { if (working || paths.Length == 0) return; if (paths.Length > 1 || batch) await Batch(paths); else await Load(paths[0]); }
    private async Task Pick(bool batch) { if (working) return; var picker = new FileOpenPicker(); WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(this)); foreach (var ext in new[] { ".csv", ".tsv", ".txt" }) picker.FileTypeFilter.Add(ext); var files = await picker.PickMultipleFilesAsync(); await Receive(files.Select(f => f.Path).ToArray(), batch); }
    private Task Refresh() => sourcePath is null ? Render() : Load(sourcePath, false);
    private async Task Sample() { if (working) return; importCancel.Cancel(); sourcePath = null; table = new(Core.Call("sample")!.AsObject()); summary.Text = "Built-in sample · 9 rows · 3 columns"; await Render(); }
    private void Cancel() { previewCancel.Cancel(); operationCancel.Cancel(); importCancel.Cancel(); status.Text = "Cancellation requested"; }
    private async Task Operation(Func<CancellationToken, Task> action) { if (working) return; working = true; operationCancel = new(); try { await action(operationCancel.Token); } catch (OperationCanceledException) { status.Text = "Cancelled"; } catch (Exception e) { await Error(e.Message); } finally { working = false; } }
    private async Task Copy() { var current = table; var options = style.DeepClone(); if (current is null) return; await Operation(async token => { var html = await Task.Run(() => current.Call("html", new { style = options }, token)!.GetValue<string>(), token); var data = new DataPackage(); data.SetText(html); Clipboard.SetContent(data); status.Text = "Copied complete HTML document"; }); }
    private async Task Export() { var current = table; var options = style.DeepClone(); if (current is null || working) return; var picker = new FileSavePicker { SuggestedFileName = sourcePath is null ? "YACHT Table" : Path.GetFileNameWithoutExtension(sourcePath) }; picker.FileTypeChoices.Add("HTML", new List<string> { ".html" }); WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(this)); var file = await picker.PickSaveFileAsync(); if (file is null) return; await Operation(async token => { await Task.Run(() => current.Call("export", new { style = options, path = file.Path, overwrite = true }, token), token); lastExport = file.Path; status.Text = "Exported " + file.Name; }); }
    private Task Reveal() { if (lastExport is not null) Process.Start(new ProcessStartInfo("explorer.exe") { UseShellExecute = false, ArgumentList = { "/select,", lastExport } }); return Task.CompletedTask; }
    private Task Browser() { if (lastExport is not null) Process.Start(new ProcessStartInfo(lastExport) { UseShellExecute = true }); return Task.CompletedTask; }
    private async Task Batch(string[] paths)
    {
        var panel = new StackPanel { Spacing = 12 }; var log = new TextBox { Text = string.Join("\n", paths), IsReadOnly = true, AcceptsReturn = true, Height = 240 }; panel.Children.Add(log);
        var replace = new CheckBox { Content = "Replace existing HTML files" }; panel.Children.Add(replace);
        if (await Dialog("Batch Convert CSVs", panel, "Convert") != ContentDialogResult.Primary) return;
        var overwrite = replace.IsChecked == true; if (overwrite && await Dialog("Replace existing HTML?", "CSV inputs are preserved. Existing matching HTML will be replaced.", "Replace and Convert") != ContentDialogResult.Primary) return;
        var options = style.DeepClone(); var separator = (string)delimiter.SelectedItem; var results = new List<string>();
        await Operation(async token => { foreach (var path in paths) { token.ThrowIfCancellationRequested(); var result = (await Task.Run(() => Core.Call("batch", new { inputs = new[] { path }, style = options, delimiter = separator, overwrite }, token), token))!.AsArray()[0]!; var error = result["error"]?.GetValue<string>(); results.Add(error is null ? "Saved " + result["output"] : path + ": " + error); if (error is null) lastExport = result["output"]!.GetValue<string>(); status.Text = $"Batch processed {results.Count} of {paths.Length} files"; } });
        await Dialog("Batch results", new TextBox { Text = string.Join("\n", results), IsReadOnly = true, AcceptsReturn = true, TextWrapping = TextWrapping.Wrap, MaxHeight = 400 });
    }
    private void UpdatePresets() { preset.ItemsSource = new[] { "Default (Styled)", "Unstyled" }.Concat(presets.Select(p => p.Key).Order()).ToArray(); preset.SelectedIndex = 0; }
    private Task LoadPreset() { var name = (string)preset.SelectedItem; return SetStyle(name == "Default (Styled)" ? Core.Style() : name == "Unstyled" ? Core.Style("unstyled") : presets[name]!.DeepClone().AsObject()); }
    private async Task SavePreset() { var name = presetName.Text.Trim(); if (presets.ContainsKey(name) && await Dialog("Replace preset?", name, "Replace") != ContentDialogResult.Primary) return; var next = presets.DeepClone().AsObject(); next[name] = style.DeepClone(); Core.Call("save_presets", new { path = Preferences.PresetPath, presets = next }); presets = next; UpdatePresets(); preset.SelectedItem = name; status.Text = "Saved preset " + name; }
    private async Task DeletePreset() { var name = (string)preset.SelectedItem; if (!presets.ContainsKey(name)) return; if (await Dialog("Delete preset?", name, "Delete") != ContentDialogResult.Primary) return; var next = presets.DeepClone().AsObject(); next.Remove(name); Core.Call("save_presets", new { path = Preferences.PresetPath, presets = next }); presets = next; UpdatePresets(); }
    private void UpdateRecent() { recent.ItemsSource = preferences.Recent.ToArray(); if (preferences.Recent.Count > 0) recent.SelectedIndex = 0; }
    private void ApplyAppearance() { Root.RequestedTheme = preferences.Appearance switch { "Light" => ElementTheme.Light, "Dark" => ElementTheme.Dark, _ => ElementTheme.Default }; }
    private void SavePreferences() { try { preferences.Save(); } catch (Exception e) { status.Text = "Could not save preferences: " + e.Message; } }
    private async Task Settings() { var panel = new StackPanel { Spacing = 12 }; var remember = new ToggleSwitch { Header = "Remember last-used table style", IsOn = preferences.RememberStyle }; panel.Children.Add(remember); var limit = new ComboBox { Header = "Maximum preview rows", ItemsSource = new[] { 50, 200, 1000 }, SelectedItem = preferences.PreviewRows }; panel.Children.Add(limit); var appearance = new ComboBox { Header = "Appearance", ItemsSource = new[] { "System", "Light", "Dark" }, SelectedItem = preferences.Appearance }; panel.Children.Add(appearance); panel.Children.Add(Button("Clear Recent Files", () => Do(() => { preferences.Recent.Clear(); UpdateRecent(); SavePreferences(); }))); panel.Children.Add(new TextBlock { Text = "YACHT " + Core.Call("info")!["version"] + " · GPLv3" }); if (await Dialog("Settings", panel, "Save") == ContentDialogResult.Primary) { preferences.RememberStyle = remember.IsOn; preferences.PreviewRows = (int)limit.SelectedItem; preferences.Appearance = (string)appearance.SelectedItem; ApplyAppearance(); SavePreferences(); await Render(); } }
    // CI drives the real native window and the same stores/bindings as user actions.
    public void StartSmokeTest(string directory)
    {
        Root.Loaded += async (_, _) =>
        {
            try
            {
                Directory.CreateDirectory(directory);
                if (webReady is not null) await webReady;
                var input = Path.Combine(directory, "Café.csv"); File.WriteAllText(input, "A,B\n<script>,🛥\n1,2,3\n");
                await Load(input); if (table is null || table.Metadata["row_count"]!.GetValue<int>() != 2) throw new Exception("native open");
                await SetStyle(Core.Style("unstyled"));
                if (!source.Text.Contains("&lt;script&gt;") || source.Text.Contains("<style>")) throw new Exception($"native preview/source: {status.Text}; source length={source.Text.Length}");
                presetName.Text = "CI-" + Guid.NewGuid(); await SavePreset(); await LoadPreset();
                await Copy(); if (!(await Clipboard.GetContent().GetTextAsync()).Contains("&lt;script&gt;")) throw new Exception("native clipboard");
                var current = table; var output = Path.Combine(directory, "out.html"); var options = style.DeepClone();
                await Operation(async token => { await Task.Run(() => current.Call("export", new { style = options, path = output }, token)); lastExport = output; });
                if (!File.ReadAllText(output).Contains("&lt;script&gt;")) throw new Exception("native export");
                var results = await Task.Run(() => Core.Call("batch", new { inputs = new[] { input }, style = options })!.AsArray());
                if (results[0]!["error"] is not null) throw new Exception("native batch");
                preferences.PreviewRows = 50; await Render(); SavePreferences();
                if (controls.Count != 16) throw new Exception("style controls");
                File.WriteAllText(Path.Combine(directory, "passed.txt"), "WinUI startup/open/preview/source/style/preset/clipboard/export/settings/batch passed");
                Application.Current.Exit();
            }
            catch (Exception e) { File.WriteAllText(Path.Combine(directory, "failed.txt"), e.ToString()); Environment.Exit(1); }
        };
    }

}
