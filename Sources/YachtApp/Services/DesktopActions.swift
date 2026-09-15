import AppKit

/// Small platform boundary: SwiftUI has no clipboard or Finder reveal API.
@MainActor enum DesktopActions {
    static func copy(_ text: String) {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
    }
    static func reveal(_ url: URL) { NSWorkspace.shared.activateFileViewerSelecting([url]) }
    static func open(_ url: URL) { NSWorkspace.shared.open(url) }
}
