import Foundation
public struct PreviewDocument: Sendable, Decodable {
    public let html: String
    public let source: String
    public let rowCount: Int
    public let sourceTruncated: Bool
    public let previewUnavailable: Bool
    enum CodingKeys: String, CodingKey {
        case html, source, rowCount = "row_count", sourceTruncated = "source_truncated", previewUnavailable = "preview_unavailable"
    }
    public init(table: TableData, style: StyleOptions, limit: Int = 200) throws {
        self = try RustBridge.call("preview", ["handle": table.handle, "style": try RustBridge.object(style), "limit": max(0, limit)])
    }
}
