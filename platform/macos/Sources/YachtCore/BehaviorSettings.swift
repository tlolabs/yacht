import Foundation
public struct BehaviorSettings: Codable, Sendable {
    public var rememberStyle: Bool
    public var previewRows: Int
    public var lastStyle: StyleOptions
    enum CodingKeys: String, CodingKey { case rememberStyle = "remember_style", previewRows = "preview_rows", lastStyle = "last_style" }
    private struct Result: Decodable { let settings: BehaviorSettings; let initial_style: StyleOptions }
    public static var defaults: Self { let r: Result = try! RustBridge.call("settings", cancellable: false); return r.settings }
    public func initialStyle() throws -> StyleOptions {
        let r: Result = try RustBridge.call("settings", ["settings": try RustBridge.object(self)], cancellable: false)
        return r.initial_style
    }
}
