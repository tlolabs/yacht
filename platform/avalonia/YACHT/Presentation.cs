using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using System.Text.Json.Nodes;
namespace YACHT;

public abstract class Observable : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;
    protected void Changed([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new(name));
    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null) { if (EqualityComparer<T>.Default.Equals(field, value)) return false; field = value; Changed(name); return true; }
}
public sealed class Command(Func<Task> action, Func<bool>? enabled = null) : ICommand
{
    public bool CanExecute(object? parameter) => enabled?.Invoke() ?? true;
    public event EventHandler? CanExecuteChanged;
    public void Refresh() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    public Task ExecuteAsync() => CanExecute(null) ? action() : Task.CompletedTask;
    public async void Execute(object? parameter) => await ExecuteAsync();
}
public interface IDesktopServices
{
    Task<string[]> OpenFiles();
    Task<string?> SaveFile(string suggestedName);
    Task<bool> Confirm(string title, string message, string accept);
    Task Show(string title, string message);
    Task<bool?> ReviewBatch(string[] paths);
    Task<bool> Settings(Preferences preferences);
    Task SetClipboard(string text);
    Task Reveal(string path);
    Task Open(string pathOrUrl);
    Task<string?> PickColor(string label, string value);
    void ApplyAppearance(string appearance);
}
public sealed class StyleField : Observable
{
    private readonly Action changed;
    private string text;
    private bool flag;
    public string Key { get; }
    public string Label { get; }
    public string[] Choices { get; }
    public bool IsFlag { get; }
    public bool IsNumber { get; }
    public bool IsChoice => Choices.Length > 0;
    public bool IsText => !IsFlag && !IsChoice;
    public bool IsColor => Key.EndsWith("_bg") || Key.EndsWith("_color");
    public string Text { get => text; set { if (Set(ref text, value)) changed(); } }
    public bool Flag { get => flag; set { if (Set(ref flag, value)) changed(); } }
    public Command ColorCommand { get; }
    public StyleField(string key, string label, JsonNode value, Action changed, Func<StyleField, Task> pick)
    {
        Key = key; Label = label; this.changed = changed;
        Choices = key switch { "border_style" => ["solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset", "outset"], "border_collapse" => ["collapse", "separate"], _ => [] };
        IsFlag = value is JsonValue b && b.TryGetValue<bool>(out _);
        IsNumber = value is JsonValue n && n.TryGetValue<long>(out _);
        flag = IsFlag && value.GetValue<bool>(); text = IsFlag ? "" : IsNumber ? value.ToString() : value.GetValue<string>();
        ColorCommand = new(() => pick(this));
    }
    public JsonNode Value => IsFlag ? JsonValue.Create(Flag)! : IsNumber ? JsonValue.Create(long.Parse(Text, System.Globalization.CultureInfo.InvariantCulture))! : JsonValue.Create(Text)!;
}
