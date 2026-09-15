import Foundation

public enum FileService {
    public static func defaultOutput(for input: URL) -> URL { input.deletingPathExtension().appendingPathExtension("html") }

    public static func export(_ table: TableData, style: StyleOptions, to url: URL, overwrite: Bool = false) throws {
        guard url.isFileURL else { throw YachtError.invalid("Choose a local output file.") }
        let fm = FileManager.default
        try style.validate()
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        let temporary = url.deletingLastPathComponent().appendingPathComponent(".yacht-\(UUID().uuidString).tmp")
        guard fm.createFile(atPath: temporary.path, contents: nil) else { throw YachtError.invalid("Cannot create export in \(url.deletingLastPathComponent().path).") }
        defer { try? fm.removeItem(at: temporary) }
        let handle = try FileHandle(forWritingTo: temporary)
        do {
            var buffer = Data()
            buffer.reserveCapacity(65_536)
            try HTMLGenerator.write(table, style: style) { chunk in
                buffer.append(contentsOf: chunk.utf8)
                if buffer.count >= 65_536 { try handle.write(contentsOf: buffer); buffer.removeAll(keepingCapacity: true) }
            }
            try handle.write(contentsOf: buffer)
            try handle.synchronize()
            try handle.close()
        } catch { try? handle.close(); throw error }
        try Task.checkCancellation()
        if overwrite && fm.fileExists(atPath: url.path) {
            _ = try fm.replaceItemAt(url, withItemAt: temporary)
        } else {
            // moveItem fails if a destination already exists, including a racing writer.
            do { try fm.moveItem(at: temporary, to: url) }
            catch { if fm.fileExists(atPath: url.path) { throw YachtError.invalid("\(url.lastPathComponent) already exists. Choose another name or explicitly allow replacement.") }; throw error }
        }
    }

    public static func convert(_ input: URL, to output: URL, style: StyleOptions, delimiter: CSVDelimiter = .comma, overwrite: Bool = false) throws {
        guard input.standardizedFileURL.resolvingSymlinksInPath() != output.standardizedFileURL.resolvingSymlinksInPath() else {
            throw YachtError.invalid("The output must not replace the input CSV.")
        }
        let table = try CSVParser.read(input, delimiter: delimiter)
        try export(table, style: style, to: output, overwrite: overwrite)
    }
}
