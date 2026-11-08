import Foundation

public enum HTMLOutputMode: String, Codable, CaseIterable, Sendable {
    case table
    case document
}

public struct BehaviorSettings: Codable, Sendable {
    public var rememberStyle: Bool
    public var previewRows: Int
    public var lastStyle: StyleOptions
    public var outputMode: HTMLOutputMode
    enum CodingKeys: String, CodingKey { case rememberStyle = "remember_style", previewRows = "preview_rows", lastStyle = "last_style", outputMode = "output_mode" }
    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        rememberStyle = try values.decode(Bool.self, forKey: .rememberStyle)
        previewRows = try values.decode(Int.self, forKey: .previewRows)
        lastStyle = try values.decode(StyleOptions.self, forKey: .lastStyle)
        outputMode = try values.decodeIfPresent(HTMLOutputMode.self, forKey: .outputMode) ?? .table
    }
    private struct Result: Decodable { let settings: BehaviorSettings; let initial_style: StyleOptions }
    public static var defaults: Self { let r: Result = try! RustBridge.call("settings", cancellable: false); return r.settings }
    public func initialStyle() throws -> StyleOptions {
        let r: Result = try RustBridge.call("settings", ["settings": try RustBridge.object(self)], cancellable: false)
        return r.initial_style
    }
}
