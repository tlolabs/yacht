using System.Text.Json.Nodes;
using YACHT;
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
