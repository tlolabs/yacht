import SwiftUI
import OSLog

@main
struct YachtApp: App {
    @NSApplicationDelegateAdaptor(FileOpenDelegate.self) private var fileOpenDelegate
    private var colorScheme: ColorScheme? { workspace.preferences.appearance == "Dark" ? .dark : workspace.preferences.appearance == "Light" ? .light : nil }
    private var workspace: Workspace { fileOpenDelegate.workspace }
    var body: some Scene {
        WindowGroup("YACHT", id: "main") {
            ContentView(workspace: workspace)
                .preferredColorScheme(colorScheme)
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
                Link("YACHT User Guide", destination: URL(string: "https://github.com/tlolabs/yacht#using-yacht")!)
            }
        }
        Settings { SettingsView(preferences: workspace.preferences).preferredColorScheme(colorScheme) }
    }
}

/// Receives the entire Finder Open event; onOpenURL can discard all but its first file.
@MainActor
final class FileOpenDelegate: NSObject, NSApplicationDelegate {
    let workspace = Workspace()
    private let logger = Logger(subsystem: "com.local.yacht.csvhtmltranslator", category: "FileOpen")
    func applicationDidFinishLaunching(_ notification: Notification) {
        logger.info("Native file-open delegate ready")
    }
    func application(_ application: NSApplication, open urls: [URL]) {
        logger.info("Received \(urls.count, privacy: .public) file URLs")
        workspace.receive(urls)
    }
    func application(_ application: NSApplication, openFiles filenames: [String]) {
        // Accept filename-based delivery as well as URL-based delivery.
        logger.info("Received \(filenames.count, privacy: .public) file paths")
        workspace.receive(filenames.map { URL(fileURLWithPath: $0) })
        application.reply(toOpenOrPrint: .success)
    }
}
