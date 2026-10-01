using Avalonia.Input.Platform;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Automation;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Styling;
using System.Diagnostics;
namespace YACHT;

public sealed class DesktopServices(Window owner) : IDesktopServices
{
    private static T Named<T>(T control, string name) where T : Control { AutomationProperties.SetName(control, name); return control; }
    public async Task<string[]> OpenFiles() => (await owner.StorageProvider.OpenFilePickerAsync(new() { Title = "Open CSV files", AllowMultiple = true, FileTypeFilter = [new("CSV / TSV / Text") { Patterns = ["*.csv", "*.tsv", "*.txt"] }, FilePickerFileTypes.All] })).Select(p => p.TryGetLocalPath()).OfType<string>().ToArray();
    public async Task<string?> SaveFile(string suggestedName) => (await owner.StorageProvider.SaveFilePickerAsync(new() { Title = "Export HTML", SuggestedFileName = suggestedName, DefaultExtension = "html", ShowOverwritePrompt = true, FileTypeChoices = [new("HTML") { Patterns = ["*.html"] }] }))?.TryGetLocalPath();
    private async Task<bool> Dialog(string title, Control content, string accept = "OK", bool cancel = true)
    {
        var dialog = new Window { Title = title, Width = 550, SizeToContent = SizeToContent.Height, MaxHeight = 720, MinHeight = 160, WindowStartupLocation = WindowStartupLocation.CenterOwner, CanResize = true };
        var ok = new Button { Content = accept, IsDefault = true }; ok.Click += (_, _) => dialog.Close(true);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 12 }; buttons.Children.Add(ok);
        if (cancel) { var no = new Button { Content = "Cancel", IsCancel = true }; no.Click += (_, _) => dialog.Close(false); buttons.Children.Add(no); }
        var panel = new Grid { RowDefinitions = new("*,Auto"), Margin = new(20), RowSpacing = 16 };
        panel.Children.Add(new ScrollViewer { Content = content, MaxHeight = 550 }); Grid.SetRow(buttons, 1); panel.Children.Add(buttons); dialog.Content = panel;
        dialog.Opened += (_, _) => { if (cancel) buttons.Children[^1].Focus(); else ok.Focus(); };
        return await dialog.ShowDialog<bool>(owner);
    }
    private static TextBox Message(string text) => new() { Text = text, IsReadOnly = true, AcceptsReturn = true, TextWrapping = TextWrapping.Wrap, BorderThickness = new(0), Background = Brushes.Transparent };
    public Task<bool> Confirm(string title, string message, string accept) => Dialog(title, Message(message), accept);
    public async Task Show(string title, string message) => await Dialog(title, Message(message), cancel: false);
    public async Task<bool?> ReviewBatch(string[] paths)
    {
        var overwrite = new CheckBox { Content = "Replace existing HTML files" };
        var panel = new StackPanel { Spacing = 12 }; panel.Children.Add(Message(string.Join("\n", paths))); panel.Children.Add(overwrite);
        return await Dialog("Batch Convert CSVs", panel, "Convert") ? overwrite.IsChecked == true : null;
    }
    public async Task<bool> Settings(Preferences preferences)
    {
        var remember = new CheckBox { Content = "Remember last-used table style", IsChecked = preferences.RememberStyle };
        var limit = Named(new ComboBox { ItemsSource = new[] { 50, 200, 1000 }, SelectedItem = preferences.PreviewRows }, "Maximum preview rows");
        var appearance = Named(new ComboBox { ItemsSource = new[] { "System", "Light", "Dark" }, SelectedItem = preferences.Appearance }, "Appearance");
        bool clearRecent = false; var clear = new Button { Content = "Clear Recent Files" }; clear.Click += (_, _) => { clearRecent = true; clear.Content = "Recent files will be cleared on Save"; };
        var panel = new StackPanel { Spacing = 12 };
        foreach (var c in new Control[] { remember, new TextBlock { Text = "Maximum preview rows" }, limit, new TextBlock { Text = "Appearance" }, appearance, clear, new TextBlock { Text = AppIdentity.Title + " " + Core.Call("info")!["version"] + " · GPLv3" } }) panel.Children.Add(c);
        if (!await Dialog("Settings", panel, "Save")) return false;
        preferences.RememberStyle = remember.IsChecked == true; preferences.PreviewRows = (int)limit.SelectedItem!; preferences.Appearance = (string)appearance.SelectedItem!;
        if (clearRecent) preferences.Recent.Clear(); return true;
    }
    public async Task<string?> PickColor(string label, string value)
    {
        var picker = new ColorPicker { IsAlphaEnabled = false, Color = Color.TryParse(value, out var c) ? c : Colors.Black };
        return await Dialog(label, picker, "Use Color") ? $"#{picker.Color.R:x2}{picker.Color.G:x2}{picker.Color.B:x2}" : null;
    }
    public void ApplyAppearance(string appearance) { if (Application.Current is { } app) app.RequestedThemeVariant = appearance switch { "Light" => ThemeVariant.Light, "Dark" => ThemeVariant.Dark, _ => ThemeVariant.Default }; }
    public async Task SetClipboard(string text) { if (owner.Clipboard is not { } clipboard) throw new IOException("Clipboard unavailable"); await clipboard.SetTextAsync(text); }
    public Task Open(string pathOrUrl) { Process.Start(new ProcessStartInfo(pathOrUrl) { UseShellExecute = true }); return Task.CompletedTask; }
    public Task Reveal(string path)
    {
        if (OperatingSystem.IsWindows()) Process.Start(new ProcessStartInfo("explorer.exe") { ArgumentList = { "/select,", path }, UseShellExecute = false });
        else if (OperatingSystem.IsMacOS()) Process.Start(new ProcessStartInfo("/usr/bin/open") { ArgumentList = { "-R", path }, UseShellExecute = false });
        else Process.Start(new ProcessStartInfo("xdg-open") { ArgumentList = { Path.GetDirectoryName(path)! }, UseShellExecute = false });
        return Task.CompletedTask;
    }
}
