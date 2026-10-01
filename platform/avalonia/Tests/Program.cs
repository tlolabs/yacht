using System.Text.Json.Nodes;
using YACHT;
var navigation = new PreviewNavigationPolicy();
int navigationAssertions = 0;
void NavigationCheck(bool condition, string message) { if (!condition) throw new Exception(message); navigationAssertions++; }
NavigationCheck(!navigation.Allows(null, true), "Missing preview request rejected");
NavigationCheck(!navigation.Allows(new Uri("data:text/html;charset=utf-8;base64,"), true), "Unprepared data URL rejected");
var document = navigation.Prepare("<html><head></head><body>Café 🛥 &lt;script&gt;</body></html>");
NavigationCheck(document.IndexOf("Content-Security-Policy", StringComparison.Ordinal) < document.IndexOf("<body>", StringComparison.Ordinal), "CSP precedes document content");
var dataUrl = new Uri("data:text/html;charset=utf-8;base64," + Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(document)));
NavigationCheck(navigation.Allows(dataUrl, true), "Exact WebView2-generated request accepted");
NavigationCheck(!navigation.Allows(dataUrl, false), "Data URL is Windows-only");
NavigationCheck(navigation.Allows(new Uri("about:blank"), false), "Native in-memory origin accepted");
foreach (var rejected in new[] { "https://example.com/", "file:///tmp/preview.html", "javascript:alert(1)", "about:srcdoc", "data:text/html,<script>alert(1)</script>", dataUrl.OriginalString + "AA" })
    NavigationCheck(!navigation.Allows(new Uri(rejected), true), "Unexpected or altered preview navigation rejected");
navigation.Prepare("<html><head></head><body>Replacement</body></html>");
NavigationCheck(!navigation.Allows(dataUrl, true), "Stale document navigation rejected");
Console.WriteLine($"Preview navigation policy: {navigationAssertions} assertions passed.");
var directory = Path.Combine(Path.GetTempPath(), "yacht-csharp-" + Guid.NewGuid()); Directory.CreateDirectory(directory);
try
{
    var path = Path.Combine(directory, "Café 🛥.csv"); File.WriteAllText(path, "A,B\n<script>,🛥\n1,2,3\n");
    var table = Core.Read(path, "comma"); var style = Core.Style();
    if (table.Metadata["row_count"]!.GetValue<int>() != 2) throw new Exception("read");
    var preview = table.Call("preview", new { style, limit = 1 })!;
    if (!preview["html"]!.GetValue<string>().Contains("&lt;script&gt;") || preview["row_count"]!.GetValue<int>() != 1) throw new Exception("preview");
    var output = Path.Combine(directory, "out.html"); table.Call("export", new { style, path = output });
    if (File.ReadAllText(output) != table.Call("html", new { style })!.GetValue<string>()) throw new Exception("complete export");
    try { table.Call("export", new { style, path = output }); throw new Exception("overwrite"); } catch (CoreException) { }
    var cancelled = new CancellationToken(true);
    try { table.Call("export", new { style, path = output, overwrite = true }, cancelled); throw new Exception("cancel"); } catch (OperationCanceledException) { }
    var presets = new JsonObject { ["Teaching"] = Core.Style("unstyled") }; var presetPath = Path.Combine(directory, "presets.json");
    Core.Call("save_presets", new { path = presetPath, presets }); if (!JsonNode.DeepEquals(Core.Call("load_presets", new { path = presetPath }), presets)) throw new Exception("presets");
    var batch = Core.Call("batch", new { inputs = new[] { Path.Combine(directory, "missing.csv"), path }, style })!.AsArray();
    if (batch[0]!["error"] is null || batch[1]!["error"] is not null) throw new Exception("batch");
    if (Core.Call("settings", new { settings = new { preview_rows = 50 } })!["settings"]!["preview_rows"]!.GetValue<int>() != 50) throw new Exception("settings");
    await Task.WhenAll(Enumerable.Range(0, 20).Select(_ => Task.Run(() => table.Call("preview", new { style }))));
    Console.WriteLine("C# Rust binding: read, preview, export, presets, settings, batch, cancellation, concurrent requests passed.");
}
finally { Directory.Delete(directory, true); }

var data = Path.Combine(Path.GetTempPath(), "yacht-viewmodels-" + Guid.NewGuid()); Directory.CreateDirectory(data);
Environment.SetEnvironmentVariable("YACHT_TEST_DATA", data);
try
{
    var desktop = new TestDesktop(); using var model = new MainViewModel(desktop); await model.Initialize();
    int assertions = 0;
    void Check(bool condition, string message) { if (!condition) throw new Exception(message); assertions++; }
    Check(model.Fields.Count == 16, "All style fields");
    Check(model.Source.Contains("<table"), "Sample source");
    var input = Path.Combine(data, "Unicode 🛥.csv"); File.WriteAllText(input, "A,B\n<script>,🛥\n1,2,3\n");
    await model.Load(input); Check(model.Summary.Contains("2 rows"), "Open rows/warnings");
    await model.UnstyledCommand.ExecuteAsync(); Check(!model.Source.Contains("<style>"), "Unstyled");
    Check(model.Source.Contains("&lt;script&gt;"), "Escaping");
    await model.CopyCommand.ExecuteAsync(); Check(desktop.Clipboard.Contains("&lt;script&gt;"), "Copy all HTML");
    var output = Path.Combine(data, "out.html"); await model.ExportTo(output, false); Check(File.ReadAllText(output) == desktop.Clipboard, "Export matches copy");
    try { await model.ExportTo(output, false); throw new Exception("Overwrite accepted"); } catch (CoreException) { assertions++; }
    Check(!model.Busy, "Failure releases busy state");
    model.PresetName = "Teaching"; await model.SavePresetCommand.ExecuteAsync(); Check(model.Presets.Contains("Teaching"), "Save preset");
    await model.StyledCommand.ExecuteAsync(); model.SelectedPreset = "Teaching"; await model.LoadPresetCommand.ExecuteAsync(); Check(!model.Source.Contains("<style>"), "Load preset");
    desktop.Accept = false; await model.DeletePresetCommand.ExecuteAsync(); Check(model.Presets.Contains("Teaching"), "Delete cancellation");
    desktop.Accept = true; await model.DeletePresetCommand.ExecuteAsync(); Check(!model.Presets.Contains("Teaching"), "Delete confirmed");
    await model.Batch([Path.Combine(data, "missing.csv"), input]); Check(desktop.Messages.Last().Contains("Saved"), "Batch partial success");
    Check(model.Recent.Contains(input), "Recent file persistence");
    Check(Preferences.Load().Recent.Contains(input), "Saved recent files");
    var field = model.Fields.Single(x => x.Key == "font_size_px"); field.Text = "not-a-number"; await model.Render(); Check(model.Source == "", "Invalid style clears stale preview");
    field.Text = "20"; await model.Render(); Check(model.Source.Contains("<table"), "Recovery after validation failure");
    var tsv = Path.Combine(data, "tabs.tsv"); File.WriteAllText(tsv, "A\tB\n1\t2\n"); await model.Load(tsv); Check(model.Delimiter == "tab", "TSV inference");
    desktop.PickGate = new TaskCompletionSource<string[]>(); var pending = model.OpenCommand.ExecuteAsync();
    Check(model.Busy && !model.ExportCommand.CanExecute(null), "Dialog gates operations");
    desktop.Accept = false; Check(!await model.MayClose(), "Close refuses active work without consent");
    desktop.PickGate.SetResult([]); await pending; Check(!model.Busy, "Dialog cleanup");
    if (AppIdentity.IsInternal) { await model.CheckUpdates(false); Check(desktop.Messages.Last().Contains("Production updates are disabled"), "Internal updater isolation"); }
    var legacy = Path.Combine(data, "ui.ini");
    File.WriteAllText(legacy, "[ui]\nremember_style=false\npreview_rows=50\nappearance=Dark\nrecent_files=[\"/tmp/Café.csv\"]\nlast_style={\"font_size_px\":18}\n");
    var migrated = Preferences.ImportLinuxPreferences(legacy);
    Check(!migrated.RememberStyle && migrated.PreviewRows == 50 && migrated.Appearance == "Dark", "GTK preferences migration");
    Check(migrated.Recent.Single() == "/tmp/Café.csv" && migrated.LastStyle!["font_size_px"]!.GetValue<int>() == 18, "GTK recent/style migration");
    Check(File.Exists(legacy), "Legacy preferences preserved");
    var huge = Path.Combine(data, "cancel.csv"); File.WriteAllText(huge, "A,B\n" + string.Concat(Enumerable.Repeat("1,2\n", 100000)));
    var loading = model.Load(huge); model.Cancel();
    try { await loading; throw new Exception("Cancelled load succeeded"); } catch (OperationCanceledException) { assertions++; }
    Check(!model.Busy, "Cancellation releases active work");
    await model.Load(tsv); Check(model.Source.Contains("<table"), "Retry after cancellation");
    var preferencePath = Path.Combine(data, "ui.json");
    var validPrefs = File.ReadAllText(preferencePath);
    File.WriteAllText(preferencePath, "{ broken existing preferences");
    using (var recovery = new MainViewModel(new TestDesktop())) { await recovery.Initialize(); }
    Check(File.ReadAllText(preferencePath) == "{ broken existing preferences", "Unreadable preferences are not overwritten");
    File.WriteAllText(preferencePath, validPrefs);
    Console.WriteLine($"Shared presentation: {assertions} assertions passed.");
}
finally { Environment.SetEnvironmentVariable("YACHT_TEST_DATA", null); Directory.Delete(data, true); }

sealed class TestDesktop : IDesktopServices
{
    public bool Accept = true;
    public string Clipboard = "";
    public List<string> Messages = [];
    public TaskCompletionSource<string[]>? PickGate;
    public Task<string[]> OpenFiles() => PickGate?.Task ?? Task.FromResult(Array.Empty<string>());
    public Task<string?> SaveFile(string suggestedName) => Task.FromResult<string?>(null);
    public Task<bool> Confirm(string title, string message, string accept) => Task.FromResult(Accept);
    public Task Show(string title, string message) { Messages.Add(message); return Task.CompletedTask; }
    public Task<bool?> ReviewBatch(string[] paths) => Task.FromResult<bool?>(false);
    public Task<bool> Settings(Preferences preferences) => Task.FromResult(false);
    public Task SetClipboard(string text) { Clipboard = text; return Task.CompletedTask; }
    public Task Reveal(string path) => Task.CompletedTask;
    public Task Open(string pathOrUrl) => Task.CompletedTask;
    public Task<string?> PickColor(string label, string value) => Task.FromResult<string?>(null);
    public void ApplyAppearance(string appearance) { }
}
