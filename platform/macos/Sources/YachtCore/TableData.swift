import Foundation

private final class TableStorage: @unchecked Sendable {
    let handle: UInt64
    init(_ handle: UInt64) { self.handle = handle }
    deinit { try? RustBridge.perform("release", ["handle": handle], cancellable: false) }
}
/// Native metadata plus an immutable Rust-owned table. Rows are materialized only
/// by compatibility/test callers; the UI uses rowCount and never copies the data.
public struct TableData: Sendable, Equatable {
    private let storage: TableStorage
    var handle: UInt64 { storage.handle }
    public let header: [String]
    public let rowCount: Int
    public let shortRowCount: Int
    public let extraRowCount: Int
    public let warnings: [String]
    public var rows: [[String]] { try! RustBridge.call("rows", ["handle": handle], cancellable: false) }
    struct Metadata: Decodable {
        let handle: UInt64; let header: [String]; let row_count: Int
        let short_row_count: Int; let extra_row_count: Int; let warnings: [String]
    }
    init(_ value: Metadata) {
        storage = TableStorage(value.handle); header = value.header; rowCount = value.row_count
        shortRowCount = value.short_row_count; extraRowCount = value.extra_row_count; warnings = value.warnings
    }
    public init(records: [[String]]) throws { self.init(try RustBridge.call("records", ["records": records])) }
    public static let sample = TableData(try! RustBridge.call("sample", cancellable: false))
    public static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.handle == rhs.handle || (lhs.header == rhs.header && lhs.rows == rhs.rows && lhs.shortRowCount == rhs.shortRowCount && lhs.extraRowCount == rhs.extraRowCount)
    }
}
public enum YachtError: LocalizedError, Sendable {
    case invalid(String)
    public var errorDescription: String? { switch self { case .invalid(let message): message } }
}
