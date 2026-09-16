import Foundation
public struct PresetRepository: Sendable {
    public let url: URL
    public init(url: URL) { self.url = url }
    public func load() throws -> [String: StyleOptions] { try RustBridge.call("load_presets", ["path": url.path]) }
    public func save(_ presets: [String: StyleOptions]) throws {
        try RustBridge.perform("save_presets", ["path": url.path, "presets": try RustBridge.object(presets)])
    }
}
