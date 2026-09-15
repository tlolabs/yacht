import SwiftUI

@main
struct YachtApp: App {
    @NSApplicationDelegateAdaptor(FileOpenDelegate.self) private var fileOpenDelegate
    private var workspace: Workspace { fileOpenDelegate.workspace }
    var body: some Scene {
        WindowGroup("Y.A.C.H.T.", id: "main") {
            ContentView(workspace: workspace)
                .handlesExternalEvents(preferring: ["*"], allowing: ["*"])
        }
        .handlesExternalEvents(matching: ["*"])
        .defaultSize(width: 1000, height: 650)
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Open CSV…") { workspace.chooseFile() }.keyboardShortcut("o")
                Menu("Open Recent") {
                    ForEach(workspace.preferences.recentFiles, id: \.self) { url in Button(url.lastPathComponent) { workspace.load(url) } }
                    Divider()
                    Button("Clear Menu") { workspace.preferences.clearRecent() }
                }
                Button("Use Sample Table") { workspace.useSample() }
                Divider()
                Button("Batch Convert CSVs…") { workspace.chooseFile(batch: true) }.keyboardShortcut("b", modifiers: [.command, .shift])
            }
            CommandGroup(replacing: .saveItem) {
                Button("Export HTML…") { workspace.prepareExport() }.keyboardShortcut("s").disabled(!workspace.canExport)
                Button("Copy HTML Code") { workspace.copyHTML() }.keyboardShortcut("c", modifiers: [.command, .shift]).disabled(!workspace.canExport)
            }
            CommandGroup(after: .toolbar) {
                Button("Refresh Preview") { workspace.refresh() }.keyboardShortcut("r")
                Button("Table Preview") { workspace.section = "preview" }.keyboardShortcut("1")
                Button("HTML Source") { workspace.section = "source" }.keyboardShortcut("2")
            }
            CommandGroup(replacing: .help) {
                Link("Y.A.C.H.T. User Guide", destination: URL(string: "https://github.com/tlolabs/yacht#using-yacht")!)
            }
        }
        Settings { SettingsView(preferences: workspace.preferences) }
    }
}

/// Receives the entire Finder Open event; onOpenURL can discard all but its first file.
@MainActor
final class FileOpenDelegate: NSObject, NSApplicationDelegate {
    let workspace = Workspace()
    func application(_ application: NSApplication, open urls: [URL]) {
        workspace.receive(urls)
    }
}
