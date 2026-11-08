import Foundation
public enum HTMLGenerator {
    public static func escape(_ text: String) -> String { try! RustBridge.call("escape", ["text": text], cancellable: false) }
    public static func isNumeric(_ text: String) -> Bool { try! RustBridge.call("numeric", ["text": text], cancellable: false) }
    public static func document(_ table: TableData, style: StyleOptions, rowLimit: Int? = nil) throws -> String {
        try output(table, style: style, outputMode: .document, rowLimit: rowLimit)
    }
    public static func output(_ table: TableData, style: StyleOptions, outputMode: HTMLOutputMode = .table, rowLimit: Int? = nil) throws -> String {
        var args: [String: Any] = ["handle": table.handle, "style": try RustBridge.object(style), "output_mode": outputMode.rawValue]
        if let rowLimit { args["limit"] = max(0, rowLimit) }
        return try RustBridge.call("html", args)
    }
    /// Compatibility sink API. Production file exports stream inside Rust.
    public static func write(_ table: TableData, style: StyleOptions, outputMode: HTMLOutputMode = .table, rowLimit: Int? = nil, sink: (String) throws -> Void) throws {
        try sink(output(table, style: style, outputMode: outputMode, rowLimit: rowLimit))
    }
}
