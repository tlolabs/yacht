using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;
namespace YACHT;

public sealed class HtmlPreview : ContentControl
{
    public static readonly StyledProperty<string> HtmlProperty = AvaloniaProperty.Register<HtmlPreview, string>(nameof(Html), "");
    public string Html { get => GetValue(HtmlProperty); set => SetValue(HtmlProperty, value); }
    private readonly NativeWebView web = new();
    private readonly Grid layout = new();
    private Task? shutdown;
    private readonly PreviewNavigationPolicy navigation = new();
    public bool NavigationComplete { get; private set; }
    private readonly Queue<string> navigationEvents = new();
    internal string NavigationDiagnostics => string.Join("; ", navigationEvents);
    private void Record(string message) { if (navigationEvents.Count == 24) navigationEvents.Dequeue(); navigationEvents.Enqueue(message); }
    internal Task<string?> InspectSmokeDocument() => web.InvokeScript("document.querySelectorAll('tbody tr').length === 2 && document.querySelector('td')?.textContent === '<script>' && document.querySelectorAll('script, style').length === 0 ? 1 : 0");
    public HtmlPreview()
    {
        var error = new TextBlock { Text = "The native preview engine is unavailable. Install WebView2 on Windows or the documented WebKitGTK packages on Linux. HTML Source, Copy and Export remain available.", TextWrapping = Avalonia.Media.TextWrapping.Wrap, IsVisible = false, Margin = new(12) };
        layout.Children.Add(web); layout.Children.Add(error); Content = layout;
        var requiredEngine = OperatingSystem.IsWindows() ? WebViewAdapterType.WebView2 : OperatingSystem.IsLinux() ? WebViewAdapterType.WebKitGtk : WebViewAdapterType.WkWebView;
        var availability = WebViewAdapterInfo.GetAdapterInfo(requiredEngine); Record(availability.ToString());
        if (!availability.IsInstalled) { layout.Children.Remove(web); error.IsVisible = true; return; }
        web.PropertyChanged += (_, e) => { if (e.Property == NativeWebView.AdapterInfoProperty && web.AdapterInfo is DetailedWebViewAdapterInfo info) error.IsVisible = !info.IsInstalled; };
        web.EnvironmentRequested += (_, e) =>
        {
            e.EnableDevTools = false;
            if (e is LinuxWpeWebViewEnvironmentRequestedEventArgs wpe) wpe.PreferWebKitGtkInstead = true;
            if (e is AppleWKWebViewEnvironmentRequestedEventArgs apple) apple.NonPersistentDataStore = true;
            if (e is GtkWebViewEnvironmentRequestedEventArgs gtk) { gtk.EphemeralDataManager = true; gtk.DisableCache = true; }
            if (e is WindowsWebView2EnvironmentRequestedEventArgs windows) { windows.IsInPrivateModeEnabled = true; windows.UserDataFolder = System.IO.Path.Combine(AppIdentity.DataDirectory, "webview"); }
        };
        web.NavigationCompleted += (_, e) => { NavigationComplete = e.IsSuccess; Record($"Completed: {e.Request?.Scheme}, success={e.IsSuccess}"); };
        // Rendering accepts only Rust-generated, escaped markup; no external URL is loaded.
        web.NavigationStarted += (_, e) => { e.Cancel = !navigation.Allows(e.Request, OperatingSystem.IsWindows()); Record($"Started: {e.Request?.Scheme}, blocked={e.Cancel}"); };
        web.NewWindowRequested += (_, e) => e.Handled = true;
        // NativeWebView replays its latest NavigateToString request when the adapter
        // becomes ready. Reissuing it here creates competing initial navigations.
        web.AdapterCreated += (_, _) => Record("Adapter created");
    }
    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) { base.OnPropertyChanged(change); if (change.Property == HtmlProperty) Navigate(); }
    private void Navigate() { if (shutdown is not null) return; NavigationComplete = false; Record("HTML requested"); web.NavigateToString(navigation.Prepare(Html), new Uri("about:blank")); }
    internal Task Shutdown() => shutdown ??= ShutdownCore();
    private async Task ShutdownCore()
    {
        var handle = web.TryGetPlatformHandle();
        web.Stop();
        // Detach while Avalonia's dispatcher is still alive. GTK disposes its
        // widget on the GLib thread; wait for its public handle to be released
        // before draining the final callbacks and allowing the window to exit.
        layout.Children.Remove(web);
        if (handle is IGtkWebViewPlatformHandle gtk)
        {
            using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            while (gtk.WebKitWebView != IntPtr.Zero) await Task.Delay(10, deadline.Token);
        }
        await Dispatcher.UIThread.InvokeAsync(() => { }, DispatcherPriority.Background);
    }
}
