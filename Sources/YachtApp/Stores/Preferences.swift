import Foundation
import Observation
import YachtCore

@MainActor @Observable
final class Preferences {
    static let shared = Preferences()
    let defaults: UserDefaults
    let repository: PresetRepository
    var presets: [String: StyleOptions] = [:]
    var recentFiles: [URL] = []
    var problem: String?
    var rememberStyle: Bool {
        didSet { defaults.set(rememberStyle, forKey: "rememberStyle") }
    }
    var previewRows: Int {
        didSet { defaults.set(previewRows, forKey: "previewRows") }
    }

    init() {
        let testing = ProcessInfo.processInfo.arguments.contains("--ui-testing")
        defaults = testing ? UserDefaults(suiteName: "com.local.yacht.ui-testing")! : .standard
        if testing { defaults.removePersistentDomain(forName: "com.local.yacht.ui-testing") }
        rememberStyle = defaults.object(forKey: "rememberStyle") as? Bool ?? true
        previewRows = defaults.object(forKey: "previewRows") as? Int ?? 200
        let base = testing ? FileManager.default.temporaryDirectory.appendingPathComponent("yacht-ui-\(ProcessInfo.processInfo.processIdentifier)") :
            FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("Y.A.C.H.T.")
        repository = PresetRepository(url: base.appendingPathComponent("presets.json"))
        do {
            if FileManager.default.fileExists(atPath: repository.url.path) { presets = try repository.load() }
            else if !testing {
                let legacy = PresetRepository(url: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".yacht_presets.json"))
                presets = try legacy.load()
                if !presets.isEmpty { try repository.save(presets) }
            }
        } catch { problem = "Could not load presets: \(error.localizedDescription). The original file is unchanged." }
        recentFiles = (defaults.stringArray(forKey: "recentFiles") ?? []).map { URL(fileURLWithPath: $0) }
    }
    var initialStyle: StyleOptions {
        guard rememberStyle, let data = defaults.data(forKey: "lastStyle"),
              let value = try? JSONDecoder().decode(StyleOptions.self, from: data), (try? value.validate()) != nil else { return .init() }
        return value
    }
    func remember(_ style: StyleOptions) {
        guard rememberStyle, let data = try? JSONEncoder().encode(style) else { return }
        defaults.set(data, forKey: "lastStyle")
    }
    func addRecent(_ url: URL) {
        recentFiles.removeAll { $0 == url }; recentFiles.insert(url, at: 0)
        recentFiles = Array(recentFiles.prefix(10))
        defaults.set(recentFiles.map(\.path), forKey: "recentFiles")
    }
    func clearRecent() { recentFiles = []; defaults.removeObject(forKey: "recentFiles") }
    func savePreset(name: String, style: StyleOptions) throws {
        var next = presets; next[name] = style
        try repository.save(next); presets = next
    }
    func deletePreset(_ name: String) throws {
        var next = presets; next.removeValue(forKey: name)
        try repository.save(next); presets = next
    }
}
