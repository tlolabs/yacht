import SwiftUI

@main
struct YachtApp: App {
    @State private var workspace = Workspace()
    var body: some Scene {
        WindowGroup("Y.A.C.H.T.", id: "main") {
            ContentView(workspace: workspace)
                .handlesExternalEvents(preferring: ["*"], allowing: ["*"])
        }
        .handlesExternalEvents(matching: ["*"])
        .defaultSize(width: 1180, height: 780)
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
