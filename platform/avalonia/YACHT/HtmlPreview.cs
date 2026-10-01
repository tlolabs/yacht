using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
namespace YACHT;

public sealed class HtmlPreview : ContentControl
{
    public static readonly StyledProperty<string> HtmlProperty = AvaloniaProperty.Register<HtmlPreview, string>(nameof(Html), "");
    public string Html { get => GetValue(HtmlProperty); set => SetValue(HtmlProperty, value); }
    private readonly NativeWebView web = new();
    public bool NavigationComplete { get; private set; }
    public const string Policy = "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">";
    public HtmlPreview()
    {
        var error = new TextBlock { Text = "The native preview engine is unavailable. Install WebView2 on Windows or the documented WebKitGTK packages on Linux. HTML Source, Copy and Export remain available.", TextWrapping = Avalonia.Media.TextWrapping.Wrap, IsVisible = false, Margin = new(12) };
        var layout = new Grid(); layout.Children.Add(web); layout.Children.Add(error); Content = layout;
        var requiredEngine = OperatingSystem.IsWindows() ? WebViewAdapterType.WebView2 : OperatingSystem.IsLinux() ? WebViewAdapterType.WebKitGtk : WebViewAdapterType.WkWebView;
        if (!WebViewAdapterInfo.GetAdapterInfo(requiredEngine).IsInstalled) { layout.Children.Remove(web); error.IsVisible = true; return; }
        web.PropertyChanged += (_, e) => { if (e.Property == NativeWebView.AdapterInfoProperty && web.AdapterInfo is DetailedWebViewAdapterInfo info) error.IsVisible = !info.IsInstalled; };
        web.EnvironmentRequested += (_, e) =>
        {
            e.EnableDevTools = false;
            if (e is LinuxWpeWebViewEnvironmentRequestedEventArgs wpe) wpe.PreferWebKitGtkInstead = true;
            if (e is AppleWKWebViewEnvironmentRequestedEventArgs apple) apple.NonPersistentDataStore = true;
            if (e is GtkWebViewEnvironmentRequestedEventArgs gtk) { gtk.EphemeralDataManager = true; gtk.DisableCache = true; }
            if (e is WindowsWebView2EnvironmentRequestedEventArgs windows) { windows.IsInPrivateModeEnabled = true; windows.UserDataFolder = System.IO.Path.Combine(AppIdentity.DataDirectory, "webview"); }
        };
        web.NavigationCompleted += (_, e) => NavigationComplete = e.IsSuccess;
        // Rendering accepts only Rust-generated, escaped markup; no external URL is loaded.
        web.NavigationStarted += (_, e) => { if (e.Request?.ToString() != "about:blank") e.Cancel = true; };
        web.NewWindowRequested += (_, e) => e.Handled = true;
        web.AdapterCreated += (_, _) => Navigate();
    }
    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change) { base.OnPropertyChanged(change); if (change.Property == HtmlProperty) Navigate(); }
    private void Navigate() { NavigationComplete = false; web.NavigateToString(Html.Replace("<head>", "<head>" + Policy), new Uri("about:blank")); }
}
