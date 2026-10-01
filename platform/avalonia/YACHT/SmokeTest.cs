using Avalonia.Controls;
using Avalonia.Input.Platform;
namespace YACHT;

// Runs only with explicit isolated storage. Uses the production window, commands,
// clipboard, native web engine, Rust binding and persistence.
internal static class SmokeTest
{
    public static async Task Run(Window window, MainViewModel model, HtmlPreview preview, string directory)
    {
        if (Environment.GetEnvironmentVariable("YACHT_TEST_DATA") is null) throw new InvalidOperationException("UI tests require isolated YACHT_TEST_DATA.");
        Directory.CreateDirectory(directory);
        try
        {
            var input = Path.Combine(directory, "Café.csv"); await File.WriteAllTextAsync(input, "A,B\n<script>,🛥\n1,2,3\n");
            await model.Load(input); await model.SetStyle(Core.Style("unstyled"));
            if (!model.Source.Contains("&lt;script&gt;") || model.Source.Contains("<style>")) throw new Exception("Preview/source escaping and style");
            for (int i = 0; i < 100 && !preview.NavigationComplete; i++) await Task.Delay(100);
            if (!preview.NavigationComplete) throw new Exception("Native HTML preview did not complete navigation");
            model.PresetName = "CI-" + Guid.NewGuid(); await model.SavePresetCommand.ExecuteAsync(); await model.LoadPresetCommand.ExecuteAsync();
            await model.CopyCommand.ExecuteAsync();
            if (!(await window.Clipboard!.TryGetTextAsync())!.Contains("&lt;script&gt;")) throw new Exception("Native clipboard");
            var output = Path.Combine(directory, "out.html"); await model.ExportTo(output, false);
            if (!File.ReadAllText(output).Contains("&lt;script&gt;")) throw new Exception("Native export");
            var results = Core.Call("batch", new { inputs = new[] { input }, style = model.Style() })!.AsArray();
            if (results[0]!["error"] is not null) throw new Exception("Native batch");
            var preferences = Preferences.Load(); preferences.PreviewRows = 50; preferences.Save();
            if (Preferences.Load().PreviewRows != 50 || model.Fields.Count != 16) throw new Exception("Settings/style controls");
            await File.WriteAllTextAsync(Path.Combine(directory, "passed.txt"), "Avalonia native startup/open/preview/source/style/preset/clipboard/export/settings/batch passed");
        }
        catch (Exception e) { await File.WriteAllTextAsync(Path.Combine(directory, "failed.txt"), e.ToString()); Environment.ExitCode = 1; }
    }
}
