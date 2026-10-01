using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
namespace YACHT;

public sealed partial class MainWindow : Window
{
    private readonly MainViewModel model;
    private readonly DispatcherTimer updateTimer = new() { Interval = TimeSpan.FromHours(1) };
    private bool closeConfirmed, askingToClose;
    private readonly List<string> activations = [];
    private bool draining;
    private async void QueueFiles(string[] paths)
    {
        activations.AddRange(paths); Activate(); if (draining) return; draining = true;
        try
        {
            while (activations.Count > 0)
            {
                // Explorer and file managers can launch one process per selected file.
                await Task.Delay(300);
                while (model.Busy && !closeConfirmed) await Task.Delay(100);
                if (closeConfirmed) break;
                var batch = activations.ToArray(); activations.Clear();
                await model.Guard(() => model.Receive(batch));
            }
        }
        finally { draining = false; }
    }

    public MainWindow()
    {
        InitializeComponent(); model = new(new DesktopServices(this)); DataContext = model;
        AddHandler(DragDrop.DragOverEvent, (_, e) => e.DragEffects = model.Idle ? DragDropEffects.Copy : DragDropEffects.None);
        AddHandler(DragDrop.DropEvent, async (_, e) => await model.Guard(() => model.Receive(e.DataTransfer.TryGetFiles()?.Select(f => f.TryGetLocalPath()).OfType<string>().ToArray() ?? [])));
        Opened += async (_, _) => await model.Guard(async () =>
        {
            await model.Initialize(); if (Program.Arguments is ["--ui-smoke-test", var directory]) { await SmokeTest.Run(this, model, Preview, directory); closeConfirmed = true; Close(); return; }
            await model.Receive(Program.Arguments.Where(p => !p.StartsWith("--")).ToArray());
            if (Program.Instance is { } instance) _ = instance.Listen(paths => Dispatcher.UIThread.Post(() => QueueFiles(paths)));
            updateTimer.Start(); await model.CheckUpdates(true);
        });
        updateTimer.Tick += async (_, _) => await model.CheckUpdates(true);
        Closing += async (_, e) => { if (closeConfirmed) return; e.Cancel = true; if (askingToClose) return; askingToClose = true; try { if (await model.MayClose()) { await model.StopWork(); updateTimer.Stop(); await Preview.Shutdown(); closeConfirmed = true; Close(); } } finally { askingToClose = false; } };
        Closed += (_, _) => { updateTimer.Stop(); model.Dispose(); };
    }
}
