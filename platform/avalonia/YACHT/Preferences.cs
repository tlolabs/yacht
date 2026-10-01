using System.Text.Json;
using System.Text.Json.Nodes;
namespace YACHT;

// The reference host cannot read or mutate the native macOS application's data.
public static class AppIdentity
{
    public static bool IsInternal => OperatingSystem.IsMacOS();
    public static string Title => IsInternal ? "YACHT — Avalonia Internal Reference" : "YACHT";
    public static string DataDirectory => Environment.GetEnvironmentVariable("YACHT_TEST_DATA") ??
        (IsInternal ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library/Application Support/YACHT-Avalonia-Internal") :
        OperatingSystem.IsWindows() ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "YACHT") :
        Path.Combine(Environment.GetEnvironmentVariable("XDG_CONFIG_HOME") ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config"), "yacht"));
    public static string PresetPath => OperatingSystem.IsLinux() && Environment.GetEnvironmentVariable("YACHT_TEST_DATA") is null ?
        Path.Combine(Environment.GetEnvironmentVariable("XDG_DATA_HOME") ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".local/share"), "yacht/presets.json") : Path.Combine(DataDirectory, "presets.json");
}
public sealed class Preferences
{
    public bool RememberStyle { get; set; } = true;
    public int PreviewRows { get; set; } = 200;
    public string Appearance { get; set; } = "System";
    public List<string> Recent { get; set; } = [];
    public JsonObject? LastStyle { get; set; }
    public static Preferences Load()
    {
        var path = Path.Combine(AppIdentity.DataDirectory, "ui.json");
        var prefs = File.Exists(path) ? JsonSerializer.Deserialize<Preferences>(File.ReadAllText(path)) ?? new() : OperatingSystem.IsLinux() ? ImportLinuxPreferences(Path.Combine(AppIdentity.DataDirectory, "ui.ini")) : new Preferences();
        if (!new[] { 50, 200, 1000 }.Contains(prefs.PreviewRows)) prefs.PreviewRows = 200;
        if (!new[] { "System", "Light", "Dark" }.Contains(prefs.Appearance)) prefs.Appearance = "System";
        prefs.Recent = (prefs.Recent ?? []).Where(x => !string.IsNullOrWhiteSpace(x)).Distinct().Take(10).ToList();
        return prefs;
    }
    public static Preferences ImportLinuxPreferences(string path)
    {
        var prefs = new Preferences();
        if (!File.Exists(path)) return prefs;
        bool inUi = false;
        foreach (var line in File.ReadLines(path))
        {
            if (line.StartsWith('[')) { inUi = line.Trim() == "[ui]"; continue; }
            var pair = line.Split('=', 2); if (!inUi || pair.Length != 2) continue;
            // GLib KeyFile string escaping (JSON backslashes are themselves escaped).
            var value = System.Text.RegularExpressions.Regex.Replace(pair[1], @"\\([snrt\\])", m => m.Groups[1].Value switch { "s" => " ", "n" => "\n", "r" => "\r", "t" => "\t", _ => "\\" });
            switch (pair[0].Trim())
            {
                case "remember_style": prefs.RememberStyle = value == "true"; break;
                case "preview_rows": if (int.TryParse(value, out int rows)) prefs.PreviewRows = rows; break;
                case "appearance": prefs.Appearance = value; break;
                case "recent_files": prefs.Recent = JsonSerializer.Deserialize<List<string>>(value) ?? []; break;
                case "last_style": prefs.LastStyle = JsonNode.Parse(value)?.AsObject(); break;
            }
        }
        return prefs;
    }
    public void Save()
    {
        Directory.CreateDirectory(AppIdentity.DataDirectory);
        var temp = Path.Combine(AppIdentity.DataDirectory, "ui-" + Guid.NewGuid() + ".tmp");
        try { File.WriteAllText(temp, JsonSerializer.Serialize(this)); File.Move(temp, Path.Combine(AppIdentity.DataDirectory, "ui.json"), true); }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
}
