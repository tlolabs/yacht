import SwiftUI
import WebKit

/// The only wrapped rendering view: WebKit renders the exact generated markup.
/// Scripts, network resources, persistent website data, and navigation are disabled.
struct HTMLPreview: NSViewRepresentable {
    let html: String
    func makeCoordinator() -> Coordinator { Coordinator() }
    func makeNSView(context: Context) -> WKWebView {
        let configuration = WKWebViewConfiguration()
        configuration.defaultWebpagePreferences.allowsContentJavaScript = false
        configuration.websiteDataStore = .nonPersistent()
        let web = WKWebView(frame: .zero, configuration: configuration)
        web.navigationDelegate = context.coordinator
        web.setAccessibilityLabel("Generated table preview")
        web.allowsBackForwardNavigationGestures = false
        return web
    }
    func updateNSView(_ web: WKWebView, context: Context) {
        guard context.coordinator.lastHTML != html else { return }
        context.coordinator.lastHTML = html
        let policy = "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">"
        web.loadHTMLString(html.replacingOccurrences(of: "<head>", with: "<head>" + policy), baseURL: nil)
    }
    final class Coordinator: NSObject, WKNavigationDelegate {
        var lastHTML = ""
        func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                     decisionHandler: @escaping @MainActor @Sendable (WKNavigationActionPolicy) -> Void) {
            decisionHandler(navigationAction.navigationType == .other && navigationAction.request.url?.absoluteString == "about:blank" ? .allow : .cancel)
        }
    }
}
