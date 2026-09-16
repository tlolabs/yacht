using Microsoft.UI.Xaml;
namespace YACHT;
public partial class App : Application
{
    private MainWindow? window;
    private NativeInstance? instance;
    private static void RecordTestFailure(Exception error)
    {
        var directory = Environment.GetEnvironmentVariable("YACHT_TEST_DATA");
        if (string.IsNullOrEmpty(directory)) return;
        Directory.CreateDirectory(directory);
        File.WriteAllText(Path.Combine(directory, "startup-failed.txt"), error.ToString());
    }
    public App()
    {
        AppDomain.CurrentDomain.UnhandledException += (_, e) => RecordTestFailure(e.ExceptionObject as Exception ?? new Exception(e.ExceptionObject.ToString()));
        UnhandledException += (_, e) => RecordTestFailure(e.Exception);
        try { InitializeComponent(); }
        catch (Exception e) { RecordTestFailure(e); throw; }
    }
    protected override async void OnLaunched(LaunchActivatedEventArgs args)
    {
        var paths = Environment.GetCommandLineArgs().Skip(1).ToArray();
        var testing = paths.Length == 2 && paths[0] == "--ui-smoke-test";
        if (!testing)
        {
            instance = new NativeInstance();
            if (!instance.IsPrimary)
            {
                try { await instance.Forward(paths); Exit(); return; }
                catch (IOException) { instance.Dispose(); instance = null; }
                catch (TimeoutException) { instance.Dispose(); instance = null; }
            }
        }
        window = new MainWindow();
        if (testing) window.StartSmokeTest(paths[1]);
        else window.ReceiveAfterLoad(paths);
        window.Activate();
        window.Closed += (_, _) => instance?.Dispose();
        if (instance?.IsPrimary == true)
            _ = instance.Listen(files => window.DispatcherQueue.TryEnqueue(() => { window.Activate(); window.QueueFiles(files); }));
    }
}
