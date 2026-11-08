import Foundation
public struct PresetRepository: Sendable {
    public static let builtInPresets: [String: StyleOptions] = try! RustBridge.call("builtin_presets", cancellable: false)
    public let url: URL
    public init(url: URL) { self.url = url }
    public func load() throws -> [String: StyleOptions] { try RustBridge.call("load_presets", ["path": url.path]) }
    /// Preserve the unreadable bytes before replacing the store. Never ignore a
    /// failed backup, and use a unique name so rapid recovery cannot clobber it.
    @discardableResult
    public func recoverWithBackup() throws -> URL? {
        let manager = FileManager.default
        var backup: URL?
        if manager.fileExists(atPath: url.path) {
            let destination = url.deletingLastPathComponent().appendingPathComponent(
                "\(url.lastPathComponent).corrupt-\(UUID().uuidString)")
            try manager.copyItem(at: url, to: destination)
            backup = destination
        }
        try save([:])
        return backup
    }
    public func save(_ presets: [String: StyleOptions]) throws {
        try RustBridge.perform("save_presets", ["path": url.path, "presets": try RustBridge.object(presets)])
    }
}
