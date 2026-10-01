using System.Text;
namespace YACHT;

// WebView2 reports NavigateToString as a data URL at NavigationStarting, even
// though the completed document's origin is about:blank. Permit only the exact
// current Rust-generated document, never arbitrary data URLs or stale content.
internal sealed class PreviewNavigationPolicy
{
    internal const string ContentSecurityPolicy = "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">";
    private string? expectedDataUrl;
    internal string Prepare(string html)
    {
        var document = html.Replace("<head>", "<head>" + ContentSecurityPolicy);
        expectedDataUrl = "data:text/html;charset=utf-8;base64," + Convert.ToBase64String(Encoding.UTF8.GetBytes(document));
        return document;
    }
    internal bool Allows(Uri? request, bool windows) => request?.OriginalString == "about:blank" ||
        (windows && expectedDataUrl is not null && request?.OriginalString == expectedDataUrl);
}
