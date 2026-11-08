import Foundation
import AppKit
import Observation
import YachtCore

@MainActor @Observable
final class Preferences {
    static let shared = Preferences()
    let defaults: UserDefaults
    let repository: PresetRepository
    var presets: [String: StyleOptions] = [:]
    var presetsReadable: Bool = true
    var presetLoadError: String?
    var recentFiles: [URL] = []
    var problem: String?
    var appearance: String {
        didSet {
            defaults.set(appearance, forKey: "appearance")
            applyAppearance()
        }
    }
    var rememberStyle: Bool {
        didSet { defaults.set(rememberStyle, forKey: "rememberStyle") }
    }
    var previewRows: Int {
        didSet { defaults.set(previewRows, forKey: "previewRows") }
    }
    var outputMode: HTMLOutputMode {
        didSet { defaults.set(outputMode.rawValue, forKey: "outputMode") }
    }

    init() {
        let testing = ProcessInfo.processInfo.arguments.contains("--ui-testing")
        defaults = testing ? UserDefaults(suiteName: "com.local.yacht.ui-testing")! : .standard
        if testing { defaults.removePersistentDomain(forName: "com.local.yacht.ui-testing") }
        appearance = defaults.string(forKey: "appearance") ?? "System"
        rememberStyle = defaults.object(forKey: "rememberStyle") as? Bool ?? BehaviorSettings.defaults.rememberStyle
        previewRows = defaults.object(forKey: "previewRows") as? Int ?? BehaviorSettings.defaults.previewRows
        outputMode = defaults.string(forKey: "outputMode").flatMap(HTMLOutputMode.init(rawValue:)) ?? .table
        let base = testing ? FileManager.default.temporaryDirectory.appendingPathComponent("yacht-ui-\(ProcessInfo.processInfo.processIdentifier)") :
            FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("Y.A.C.H.T.")
        repository = PresetRepository(url: base.appendingPathComponent("presets.json"))
        do {
            if FileManager.default.fileExists(atPath: repository.url.path) {
                presets = try repository.load()
                presetsReadable = true
                presetLoadError = nil
            } else if !testing {
                let legacy = PresetRepository(url: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".yacht_presets.json"))
                if FileManager.default.fileExists(atPath: legacy.url.path) {
                    presets = try legacy.load()
                    if !presets.isEmpty { try repository.save(presets) }
                }
                presetsReadable = true
                presetLoadError = nil
            }
        } catch {
            presetsReadable = false
            presetLoadError = error.localizedDescription
            problem = "Could not load presets: \(error.localizedDescription). The original file is unchanged."
        }
        recentFiles = (defaults.stringArray(forKey: "recentFiles") ?? []).map { URL(fileURLWithPath: $0) }
        applyAppearance()
    }
    func applyAppearance() {
        switch appearance {
        case "Dark":
            NSApp?.appearance = NSAppearance(named: .darkAqua)
        case "Light":
            NSApp?.appearance = NSAppearance(named: .aqua)
        default:
            NSApp?.appearance = nil
        }
    }
    var initialStyle: StyleOptions {
        var settings = BehaviorSettings.defaults
        settings.rememberStyle = rememberStyle
        settings.previewRows = previewRows
        settings.outputMode = outputMode
        if let data = defaults.data(forKey: "lastStyle"), let style = try? JSONDecoder().decode(StyleOptions.self, from: data) { settings.lastStyle = style }
        return (try? settings.initialStyle()) ?? .init()
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
        guard presetsReadable else {
            throw PresetStoreError.unreadableStore(presetLoadError ?? "decoding error")
        }
        guard PresetRepository.builtInPresets[name] == nil || presets[name] != nil else {
            throw PresetStoreError.builtInName(name)
        }
        var next = presets; next[name] = style
        try repository.save(next); presets = next
    }
    func deletePreset(_ name: String) throws {
        guard presetsReadable else {
            throw PresetStoreError.unreadableStore(presetLoadError ?? "decoding error")
        }
        var next = presets; next.removeValue(forKey: name)
        try repository.save(next); presets = next
    }
    func recoverPresetsWithBackup() throws {
        try repository.recoverWithBackup()
        presets = [:]
        presetsReadable = true
        presetLoadError = nil
        problem = nil
    }
}

enum PresetStoreError: LocalizedError {
    case unreadableStore(String)
    case builtInName(String)
    var errorDescription: String? {
        switch self {
        case .unreadableStore(let reason):
            return "Cannot modify presets: the existing preset store could not be read safely (\(reason)). The original file is preserved to prevent data loss. Repair or recover the preset store before saving or deleting presets."
        case .builtInName(let name):
            return "“\(name)” is a built-in preset. Choose a different name for your custom style."
        }
    }
}
