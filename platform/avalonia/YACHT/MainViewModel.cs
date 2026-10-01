using System.Collections.ObjectModel;
using System.Text.Json.Nodes;
namespace YACHT;

public sealed class MainViewModel : Observable, IDisposable
{
    private readonly IDesktopServices desktop;
    private readonly List<Command> commands = [];
    private Preferences preferences = new();
    private Core.Table? table;
    private JsonObject presets = new();
    private CancellationTokenSource operation = new(), preview = new();
    private bool busy, disposed, checkingUpdates, closing;
    private bool preferencesReadable = true, presetsReadable = true;
    private TaskCompletionSource? workFinished, updateFinished;
    private string source = "", html = "", status = "Ready", summary = "Built-in sample", note = "", delimiter = "comma";
    private string? sourcePath, lastExport;
    private int selectedTab;
    public string Title => AppIdentity.Title;
    public string Version => Core.Call("info")!["version"]!.GetValue<string>();
    public bool Busy { get => busy; private set { if (Set(ref busy, value)) { Changed(nameof(Idle)); foreach (var c in commands) c.Refresh(); } } }
    public bool Idle => !Busy;
    public string Source { get => source; private set => Set(ref source, value); }
    public string Html { get => html; private set => Set(ref html, value); }
    public string Status { get => status; set => Set(ref status, value); }
    public string Summary { get => summary; private set => Set(ref summary, value); }
    public string Note { get => note; private set => Set(ref note, value); }
    public int SelectedTab { get => selectedTab; set => Set(ref selectedTab, value); }
    public string Delimiter { get => delimiter; set { if (Set(ref delimiter, value) && !Busy) RefreshCommand.Execute(null); } }
    public string[] Delimiters { get; } = ["comma", "tab", "semicolon", "pipe"];
    public ObservableCollection<StyleField> Fields { get; } = [];
    public ObservableCollection<string> Presets { get; } = [];
    public ObservableCollection<string> Recent { get; } = [];
    public string? SelectedPreset { get; set; }
    public string? SelectedRecent { get; set; }
    public string PresetName { get; set; } = "";
    public Command OpenCommand { get; }
    public Command BatchCommand { get; }
    public Command SampleCommand { get; }
    public Command RefreshCommand { get; }
    public Command CopyCommand { get; }
    public Command ExportCommand { get; }
    public Command CancelCommand { get; }
    public Command SettingsCommand { get; }
    public Command LoadPresetCommand { get; }
    public Command SavePresetCommand { get; }
    public Command DeletePresetCommand { get; }
    public Command StyledCommand { get; }
    public Command UnstyledCommand { get; }
    public Command RecentCommand { get; }
    public Command RevealCommand { get; }
    public Command BrowserCommand { get; }
    public Command UpdatesCommand { get; }
    public Command EnableUpdatesCommand { get; }
    public Command DisableUpdatesCommand { get; }
    public Command ReleasesCommand { get; }
    public Command GuideCommand { get; }
    public Command PreviewCommand { get; }
    public Command SourceCommand { get; }
    public MainViewModel(IDesktopServices desktop)
    {
        this.desktop = desktop;
        Command Cmd(Func<Task> action, bool duringWork = false) { var c = new Command(() => Guard(action), () => !disposed && (duringWork || !Busy)); commands.Add(c); return c; }
        OpenCommand = Cmd(() => Pick(false)); BatchCommand = Cmd(() => Pick(true));
        SampleCommand = Cmd(async () => { sourcePath = null; ReplaceTable(new(Core.Call("sample")!.AsObject())); Summary = "Built-in sample · 9 rows · 3 columns"; await Render(); });
        RefreshCommand = Cmd(() => sourcePath is null ? Render() : Load(sourcePath, false));
        CopyCommand = Cmd(() => Work(async token => { var current = table; var style = Style(); if (current is null) return; var result = await Task.Run(() => current.Call("html", new { style }, token)!.GetValue<string>(), token); token.ThrowIfCancellationRequested(); await desktop.SetClipboard(result); Status = "Copied complete HTML document"; }));
        ExportCommand = Cmd(Export);
        CancelCommand = Cmd(() => { Cancel(); return Task.CompletedTask; }, true);
        SettingsCommand = Cmd(async () => { Busy = true; try { if (await desktop.Settings(preferences)) { SyncRecent(); desktop.ApplyAppearance(preferences.Appearance); SavePreferences(); await Render(); } } finally { Busy = false; } });
        StyledCommand = Cmd(() => SetStyle(Core.Style())); UnstyledCommand = Cmd(() => SetStyle(Core.Style("unstyled")));
        LoadPresetCommand = Cmd(() => SetStyle(SelectedPreset == "Unstyled" ? Core.Style("unstyled") : SelectedPreset is not null && presets[SelectedPreset] is JsonNode p ? p.DeepClone().AsObject() : Core.Style()));
        SavePresetCommand = Cmd(SavePreset); DeletePresetCommand = Cmd(DeletePreset);
        RecentCommand = Cmd(() => SelectedRecent is null ? Task.CompletedTask : Load(SelectedRecent));
        RevealCommand = Cmd(() => lastExport is null ? Task.CompletedTask : desktop.Reveal(lastExport));
        BrowserCommand = Cmd(() => lastExport is null ? Task.CompletedTask : desktop.Open(lastExport));
        UpdatesCommand = Cmd(() => CheckUpdates(false));
        EnableUpdatesCommand = Cmd(() => UpdatePreference(true)); DisableUpdatesCommand = Cmd(() => UpdatePreference(false));
        ReleasesCommand = Cmd(() => desktop.Open("https://github.com/tlolabs/yacht/releases")); GuideCommand = Cmd(() => desktop.Open("https://github.com/tlolabs/yacht#using-yacht"));
        PreviewCommand = Cmd(() => { SelectedTab = 0; return Task.CompletedTask; }, true); SourceCommand = Cmd(() => { SelectedTab = 1; return Task.CompletedTask; }, true);
    }
    public async Task Initialize()
    {
        string? warning = null;
        try { preferences = Preferences.Load(); } catch (Exception e) { preferencesReadable = false; warning = "Could not load preferences; the original file will be preserved: " + e.Message; }
        desktop.ApplyAppearance(preferences.Appearance); SyncRecent();
        var style = Core.Style();
        try
        {
            if (preferences.RememberStyle && preferences.LastStyle is not null) { style = Core.Call("normalize_style", new { style = preferences.LastStyle })!.AsObject(); Core.Call("validate_style", new { style }); }
            if (!AppIdentity.IsInternal && !File.Exists(AppIdentity.PresetPath))
            {
                var legacy = Core.Call("load_presets", new { path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".yacht_presets.json") })!.AsObject();
                if (legacy.Count > 0) Core.Call("save_presets", new { path = AppIdentity.PresetPath, presets = legacy });
            }
            presets = Core.Call("load_presets", new { path = AppIdentity.PresetPath })!.AsObject();
        }
        catch (Exception e) { presetsReadable = false; warning = "Could not load saved style/presets; the original files will be preserved: " + e.Message; style = Core.Style(); }
        SyncPresets(); table = new(Core.Call("sample")!.AsObject()); await SetStyle(style);
        if (warning is not null) { Status = warning; await desktop.Show("Settings could not be loaded", warning); }
    }
    public async Task Guard(Func<Task> action)
    {
        try { await action(); }
        catch (OperationCanceledException) { if (!disposed) Status = "Cancelled"; }
        catch (Exception e) { if (!disposed) { Status = e.Message; Busy = true; try { await desktop.Show("YACHT", e.Message); } finally { Busy = false; } } }
    }
    public JsonObject Style() { var result = new JsonObject(); foreach (var field in Fields) result[field.Key] = field.Value; Core.Call("validate_style", new { style = result }); return result; }
    public async Task SetStyle(JsonObject style)
    {
        Fields.Clear();
        string[] labels = ["Table class", "Font family", "Font size", "Cell padding", "Border width", "Border style", "Border color", "Header background", "Header text color", "Body background", "Zebra striping", "Zebra color", "Hover highlight", "Hover color", "Border collapse", "Border spacing"];
        string[] keys = ["table_class", "font_family", "font_size_px", "cell_padding_px", "border_width_px", "border_style", "border_color", "header_bg", "header_text_color", "body_bg", "zebra_enabled", "zebra_bg", "hover_enabled", "hover_bg", "border_collapse", "border_spacing_px"];
        for (int i = 0; i < keys.Length; i++) Fields.Add(new(keys[i], labels[i], style[keys[i]]!, () => _ = Render(), f => Guard(async () => { Busy = true; try { var color = await desktop.PickColor(f.Label, f.Text); if (color is not null) f.Text = color; } finally { Busy = false; } })));
        await Render();
    }
    public async Task Render()
    {
        preview.Cancel(); preview.Dispose(); preview = new(); var token = preview.Token;
        var current = table; if (current is null || disposed) return;
        try
        {
            var style = Style(); var limit = preferences.PreviewRows;
            await Task.Delay(180, token);
            var result = await Task.Run(() => current.Call("preview", new { style, limit }, token)!, token); token.ThrowIfCancellationRequested();
            Html = result["html"]!.GetValue<string>(); Source = result["source"]!.GetValue<string>();
            Note = (result["preview_unavailable"]!.GetValue<bool>() ? "Cells too large to preview. " : $"Preview shows {result["row_count"]} of {current.Metadata["row_count"]} rows. ") + (result["source_truncated"]!.GetValue<bool>() ? "Source shows the first 1 MB. " : "") + "Copy and Export include every row.";
            preferences.LastStyle = preferences.RememberStyle ? style : null; SavePreferences();
        }
        catch (OperationCanceledException) { }
        catch (Exception e) { if (!token.IsCancellationRequested) { Status = e.Message; Source = ""; Html = ""; } }
    }
    private void ReplaceTable(Core.Table value) { preview.Cancel(); var old = table; table = value; old?.Dispose(); }
    public async Task Load(string path, bool infer = true)
    {
        await Work(async token =>
        {
            preview.Cancel(); Html = ""; Source = "";
            if (infer && Path.GetExtension(path).Equals(".tsv", StringComparison.OrdinalIgnoreCase)) Delimiter = "tab";
            var separator = Delimiter; Status = "Reading " + path;
            var result = await Task.Run(() => Core.Read(path, separator, token), token);
            if (token.IsCancellationRequested) { result.Dispose(); token.ThrowIfCancellationRequested(); }
            ReplaceTable(result); sourcePath = path;
            Summary = $"{Path.GetFileName(path)} · {result.Metadata["row_count"]} rows · {result.Metadata["header"]!.AsArray().Count} columns\n" + string.Join(" ", result.Metadata["warnings"]!.AsArray().Select(x => x!.GetValue<string>()));
            preferences.Recent.Remove(path); preferences.Recent.Insert(0, path); preferences.Recent = preferences.Recent.Take(10).ToList(); SyncRecent();
            Status = "Loaded " + path; await Render();
        });
    }
    private async Task Work(Func<CancellationToken, Task> action)
    {
        if (Busy || disposed) return; Busy = true; operation.Dispose(); operation = new();
        var finished = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously); workFinished = finished;
        try { await action(operation.Token); } finally { Busy = false; finished.TrySetResult(); }
    }
    private async Task Pick(bool batch)
    {
        if (Busy) return; Busy = true;
        string[] paths;
        try { paths = await desktop.OpenFiles(); } finally { Busy = false; }
        await Receive(paths, batch);
    }
    public async Task Receive(string[] paths, bool batch = false)
    {
        if (paths.Length == 0 || Busy) return;
        if (paths.Length > 1 || batch) await Batch(paths); else await Load(paths[0]);
    }
    public async Task ExportTo(string path, bool overwrite) => await Work(async token =>
    {
        var current = table; if (current is null) return; var style = Style();
        await Task.Run(() => current.Call("export", new { style, path, overwrite }, token), token);
        lastExport = path; Status = "Exported " + Path.GetFileName(path);
    });
    private async Task Export()
    {
        Busy = true; string? path;
        try { path = await desktop.SaveFile(sourcePath is null ? "YACHT Table.html" : Path.GetFileNameWithoutExtension(sourcePath) + ".html"); } finally { Busy = false; }
        if (path is not null) await ExportTo(path, true);
    }
    public async Task Batch(string[] paths)
    {
        Busy = true; bool? overwrite;
        try
        {
            overwrite = await desktop.ReviewBatch(paths);
            if (overwrite == true && !await desktop.Confirm("Replace existing HTML?", "CSV inputs are preserved. Existing matching HTML will be replaced.", "Replace and Convert")) return;
        }
        finally { Busy = false; }
        if (overwrite is null) return;
        var results = new List<string>();
        try
        {
            await Work(async token =>
            {
                var style = Style(); var separator = Delimiter;
                foreach (var path in paths)
                {
                    token.ThrowIfCancellationRequested();
                    var r = (await Task.Run(() => Core.Call("batch", new { inputs = new[] { path }, style, delimiter = separator, overwrite = overwrite.Value }, token), token))!.AsArray()[0]!;
                    results.Add(r["error"] is null ? "Saved " + r["output"] : path + ": " + r["error"]);
                    if (r["error"] is null) lastExport = r["output"]!.GetValue<string>();
                    Status = $"Batch processed {results.Count} of {paths.Length} files";
                }
            });
        }
        catch (OperationCanceledException) { results.Add("Cancelled; completed files are preserved."); }
        Busy = true; try { await desktop.Show("Batch results", string.Join("\n", results)); } finally { Busy = false; }
    }
    private async Task SavePreset()
    {
        if (!presetsReadable) throw new IOException("Repair the unreadable preset store and restart before saving presets.");
        var name = PresetName.Trim(); var style = Style();
        if (name is "Unstyled" or "Default (Styled)") throw new ArgumentException("Choose a preset name other than a built-in preset name.");
        Busy = true;
        try
        {
            if (presets.ContainsKey(name) && !await desktop.Confirm("Replace preset?", name, "Replace")) return;
            var next = presets.DeepClone().AsObject(); next[name] = style;
            Core.Call("save_presets", new { path = AppIdentity.PresetPath, presets = next }); presets = next; SyncPresets(); SelectedPreset = name; Changed(nameof(SelectedPreset)); Status = "Saved preset " + name;
        }
        finally { Busy = false; }
    }
    private async Task DeletePreset()
    {
        var name = SelectedPreset; if (name is null || !presets.ContainsKey(name)) return;
        Busy = true;
        try { if (!await desktop.Confirm("Delete preset?", name, "Delete")) return; var next = presets.DeepClone().AsObject(); next.Remove(name); Core.Call("save_presets", new { path = AppIdentity.PresetPath, presets = next }); presets = next; SyncPresets(); }
        finally { Busy = false; }
    }
    private void SyncPresets() { Presets.Clear(); Presets.Add("Default (Styled)"); Presets.Add("Unstyled"); foreach (var p in presets.OrderBy(p => p.Key)) Presets.Add(p.Key); SelectedPreset = Presets[0]; Changed(nameof(SelectedPreset)); }
    private void SyncRecent() { Recent.Clear(); foreach (var p in preferences.Recent) Recent.Add(p); SelectedRecent = Recent.FirstOrDefault(); Changed(nameof(SelectedRecent)); }
    private void SavePreferences() { if (!preferencesReadable) { Status = "Preferences could not be read. Repair the file and restart before saving settings."; return; } try { preferences.Save(); } catch (Exception e) { Status = "Could not save preferences: " + e.Message; } }
    public void Cancel() { operation.Cancel(); preview.Cancel(); Status = "Cancellation requested"; }
    public async Task<bool> MayClose() => !Busy || await desktop.Confirm("Work is in progress", "Cancel the active operation and close YACHT? Completed exports will be preserved.", "Cancel Work and Close");
    private async Task UpdatePreference(bool enabled)
    {
        Busy = true;
        try { if (AppIdentity.IsInternal) await desktop.Show("Internal reference build", "Production updates are disabled for this internal application."); else await UpdateClient.Run(enabled ? "enable" : "disable"); }
        finally { Busy = false; }
    }
    public async Task CheckUpdates(bool automatic)
    {
        if (Busy || disposed || checkingUpdates) return;
        if (AppIdentity.IsInternal) { if (!automatic) { Busy = true; try { await desktop.Show("Internal reference build", "Production updates are disabled. Obtain reference builds from CI artifacts."); } finally { Busy = false; } } return; }
        checkingUpdates = true;
        if (!automatic) Busy = true;
        var finished = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously); updateFinished = finished;
        try
        {
            var update = await UpdateClient.Run(automatic ? "check-auto" : "check");
            if (disposed || closing) return;
            if (update["status"]?.GetValue<string>() != "available") { if (!automatic) await desktop.Show("Updates", "No compatible newer update was found. Production updates may not yet be configured; published packages are available from Help → Download Releases."); return; }
            if (automatic) { if (!Busy) Status = "YACHT " + update["version"] + " is available. Use Help → Check for Updates…"; return; }
            if (!OperatingSystem.IsWindows()) { await desktop.Show("Update available", "YACHT " + update["version"] + " is available. Install the published package using your distribution's package manager. Export your work first."); return; }
            if (!await desktop.Confirm("Update available", "Download YACHT " + update["version"] + "? Export your work before closing YACHT to install.", "Download")) return;
            var download = await UpdateClient.Run("download", update["version"]!.GetValue<string>(), update["sha256"]!.GetValue<string>());
            if (closing) return;
            if (download["migration"]?.GetValue<string>() != "none") throw new IOException("This release requires manual migration; see the release notes.");
            await UpdateClient.VerifyInstaller(download);
            if (closing) return;
            await desktop.Show("Verified installer ready", "Signature, publisher and checksum verified. Export your work, close YACHT, then run the installer from the folder that opens next.");
            await desktop.Reveal(download["path"]!.GetValue<string>());
        }
        catch (Exception e) { if (!automatic && !closing) await desktop.Show("Updates", e.Message); }
        finally { if (!automatic) Busy = false; checkingUpdates = false; finished.TrySetResult(); }
    }
    public async Task StopWork() { closing = true; Cancel(); if (workFinished is { } pending) await pending.Task; if (updateFinished is { } update) await update.Task; }
    public void Dispose() { disposed = true; operation.Cancel(); preview.Cancel(); SavePreferences(); table?.Dispose(); }
}
