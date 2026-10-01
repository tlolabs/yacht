using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
namespace YACHT;

internal static class Program
{
    internal static NativeInstance? Instance { get; private set; }
    public static string[] Arguments { get; private set; } = [];
    [STAThread]
    public static void Main(string[] args)
    {
        if (OperatingSystem.IsMacOS() && System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture != System.Runtime.InteropServices.Architecture.Arm64) throw new PlatformNotSupportedException("The internal Mac host requires ARM64.");
        Arguments = args.Select(value => value.StartsWith("--") ? value : Path.GetFullPath(value)).ToArray();
        using var instance = new NativeInstance(); Instance = instance;
        if (!instance.IsPrimary) { instance.Forward(Arguments).GetAwaiter().GetResult(); return; }
        AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace().StartWithClassicDesktopLifetime(args);
    }
}
public sealed partial class App : Application
{
    public override void Initialize() { Name = AppIdentity.Title; AvaloniaXamlLoader.Load(this); }
    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) desktop.MainWindow = new MainWindow();
        base.OnFrameworkInitializationCompleted();
    }
}
