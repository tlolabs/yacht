import Foundation
public enum CSVDelimiter: String, Codable, CaseIterable, Sendable {
    case comma, tab, semicolon, pipe
    public var byte: UInt8 { switch self { case .comma: 44; case .tab: 9; case .semicolon: 59; case .pipe: 124 } }
    public var label: String { switch self { case .comma: "Comma (CSV)"; case .tab: "Tab (TSV)"; case .semicolon: "Semicolon"; case .pipe: "Pipe" } }
}
public enum CSVParser {
    public static func parse(_ data: Data, delimiter: CSVDelimiter = .comma) throws -> TableData {
        TableData(try RustBridge.call("parse", ["bytes": Array(data), "delimiter": delimiter.rawValue]))
    }
    public static func read(_ url: URL, delimiter: CSVDelimiter = .comma, chunkSize: Int = 65_536) throws -> TableData {
        guard url.isFileURL else { throw YachtError.invalid("Choose a local CSV or TSV file.") }
        return TableData(try RustBridge.call("read", ["path": url.path, "delimiter": delimiter.rawValue, "chunk_size": chunkSize]))
    }
}
