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
        Exception? failure = null;
        try
        {
            var input = Path.Combine(directory, "Café.csv"); await File.WriteAllTextAsync(input, "A,B\n<script>,🛥\n1,2,3\n");
            await model.Load(input); await model.SetStyle(Core.Style("unstyled"));
            if (!model.Source.Contains("&lt;script&gt;") || model.Source.Contains("<style>")) throw new Exception("Preview/source escaping and style");
            for (int i = 0; i < 100 && !preview.NavigationComplete; i++) await Task.Delay(100);
            if (!preview.NavigationComplete) throw new Exception("Native HTML preview did not complete navigation: " + preview.NavigationDiagnostics);
            if (await preview.InspectSmokeDocument().WaitAsync(TimeSpan.FromSeconds(5)) != "1") throw new Exception("Native preview DOM does not contain the current escaped, unstyled table: " + preview.NavigationDiagnostics);
            model.PresetName = "CI-" + Guid.NewGuid(); await model.SavePresetCommand.ExecuteAsync(); await model.LoadPresetCommand.ExecuteAsync();
            await model.CopyCommand.ExecuteAsync();
            if (!(await window.Clipboard!.TryGetTextAsync())!.Contains("&lt;script&gt;")) throw new Exception("Native clipboard");
            var output = Path.Combine(directory, "out.html"); await model.ExportTo(output, false);
            if (!File.ReadAllText(output).Contains("&lt;script&gt;")) throw new Exception("Native export");
            var results = Core.Call("batch", new { inputs = new[] { input }, style = model.Style() })!.AsArray();
            if (results[0]!["error"] is not null) throw new Exception("Native batch");
            var preferences = Preferences.Load(); preferences.PreviewRows = 50; preferences.Save();
            if (Preferences.Load().PreviewRows != 50 || model.Fields.Count != 16) throw new Exception("Settings/style controls");
        }
        catch (Exception e) { failure = e; }
        try { await model.StopWork(); await preview.Shutdown(); }
        catch (Exception e) { failure = failure is null ? e : new AggregateException(failure, e); }
        if (failure is not null) { await File.WriteAllTextAsync(Path.Combine(directory, "failed.txt"), failure.ToString()); Environment.ExitCode = 1; }
        else await File.WriteAllTextAsync(Path.Combine(directory, "passed.txt"), "Avalonia native startup/open/preview/source/style/preset/clipboard/export/settings/batch/shutdown passed");
    }
}
