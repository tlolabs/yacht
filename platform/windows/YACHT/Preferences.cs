using System.Text.Json;
using System.Text.Json.Nodes;
namespace YACHT;
public sealed class Preferences
{
    public bool RememberStyle { get; set; } = true;
    public int PreviewRows { get; set; } = 200;
    public string Appearance { get; set; } = "System";
    public List<string> Recent { get; set; } = [];
    public JsonObject? LastStyle { get; set; }
    public static string DirectoryPath => Environment.GetEnvironmentVariable("YACHT_TEST_DATA") ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "YACHT");
    public static string PresetPath => Path.Combine(DirectoryPath, "presets.json");
    public static Preferences Load()
    {
        var path = Path.Combine(DirectoryPath, "ui.json");
        if (!File.Exists(path)) return new();
        var prefs = JsonSerializer.Deserialize<Preferences>(File.ReadAllText(path)) ?? new();
        if (!new[] { 50, 200, 1000 }.Contains(prefs.PreviewRows)) prefs.PreviewRows = 200;
        if (!new[] { "System", "Light", "Dark" }.Contains(prefs.Appearance)) prefs.Appearance = "System";
        return prefs;
    }
    public void Save()
    {
        Directory.CreateDirectory(DirectoryPath);
        var temp = Path.Combine(DirectoryPath, "ui-" + Guid.NewGuid() + ".tmp");
        try { File.WriteAllText(temp, JsonSerializer.Serialize(this)); File.Move(temp, Path.Combine(DirectoryPath, "ui.json"), true); }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
}
