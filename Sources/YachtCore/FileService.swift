import Foundation
public enum FileService {
    public static func defaultOutput(for input: URL) -> URL {
        URL(fileURLWithPath: try! RustBridge.call("default_output", ["path": input.path], cancellable: false) as String)
    }
    public static func export(_ table: TableData, style: StyleOptions, to url: URL, overwrite: Bool = false) throws {
        guard url.isFileURL else { throw YachtError.invalid("Choose a local output file.") }
        try RustBridge.perform("export", ["handle": table.handle, "style": try RustBridge.object(style), "path": url.path, "overwrite": overwrite])
    }
    /// Native code grants access and presents progress; Rust owns each batch outcome.
    public static func batchItem(_ input: URL, style: StyleOptions, delimiter: CSVDelimiter, overwrite: Bool) throws -> URL {
        struct Outcome: Decodable { let output: String; let error: String? }
        let results: [Outcome] = try RustBridge.call("batch", ["inputs": [input.path], "style": try RustBridge.object(style), "delimiter": delimiter.rawValue, "overwrite": overwrite])
        if let message = results[0].error { throw YachtError.invalid(message) }
        return URL(fileURLWithPath: results[0].output)
    }
    public static func convert(_ input: URL, to output: URL, style: StyleOptions, delimiter: CSVDelimiter = .comma, overwrite: Bool = false) throws {
        guard input.isFileURL && output.isFileURL else { throw YachtError.invalid("Choose local input and output files.") }
        try RustBridge.perform("convert", ["input": input.path, "output": output.path, "style": try RustBridge.object(style), "delimiter": delimiter.rawValue, "overwrite": overwrite])
    }
}
