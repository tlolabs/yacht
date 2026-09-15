import Foundation

public enum CSVDelimiter: String, Codable, CaseIterable, Sendable {
    case comma, tab, semicolon, pipe
    public var byte: UInt8 {
        switch self { case .comma: 44; case .tab: 9; case .semicolon: 59; case .pipe: 124 }
    }
    public var label: String {
        switch self { case .comma: "Comma (CSV)"; case .tab: "Tab (TSV)"; case .semicolon: "Semicolon"; case .pipe: "Pipe" }
    }
}

/// A byte-oriented, incremental RFC 4180 state machine. UTF-8 is decoded only
/// after a complete field, so characters and quoted records may cross buffers.
public enum CSVParser {
    public static func parse(_ data: Data, delimiter: CSVDelimiter = .comma) throws -> TableData {
        var parser = Machine(delimiter: delimiter.byte)
        let bytes = data.starts(with: [0xef, 0xbb, 0xbf]) ? data.dropFirst(3) : data[...]
        try parser.consume(bytes)
        return try parser.finish()
    }

    public static func read(_ url: URL, delimiter: CSVDelimiter = .comma, chunkSize: Int = 65_536) throws -> TableData {
        guard chunkSize > 0 else { throw YachtError.invalid("CSV read buffer must be positive.") }
        let values = try url.resourceValues(forKeys: [.isRegularFileKey])
        guard values.isRegularFile == true else { throw YachtError.invalid("Choose a regular CSV or TSV file.") }
        let file = try FileHandle(forReadingFrom: url)
        defer { try? file.close() }
        var parser = Machine(delimiter: delimiter.byte)
        let prefix = try file.read(upToCount: 3) ?? Data()
        if prefix != Data([0xef, 0xbb, 0xbf]) { try parser.consume(prefix) }
        while let chunk = try file.read(upToCount: chunkSize), !chunk.isEmpty {
            try Task.checkCancellation()
            try parser.consume(chunk)
        }
        return try parser.finish()
    }

    private struct Machine {
        enum State { case start, unquoted, quoted, afterQuote }
        let delimiter: UInt8
        var state = State.start
        var field: [UInt8] = []
        var row: [String] = []
        var records: [[String]] = []
        var started = false
        var skipLF = false
        var previousCR = false
        var line = 1
        var processed = 0

        func failure(_ reason: String) -> YachtError {
            .invalid("Malformed CSV at line \(line), record \(records.count + 1): \(reason)")
        }

        mutating func appendField() throws {
            guard let value = String(bytes: field, encoding: .utf8) else {
                throw failure("input is not valid UTF-8. Save the file as UTF-8 CSV.")
            }
            row.append(value)
            field.removeAll(keepingCapacity: true)
        }

        mutating func endRow() throws {
            if started { try appendField() }
            records.append(row)
            row = []
            started = false
            state = .start
        }

        mutating func consume(_ bytes: Data.SubSequence) throws {
            for byte in bytes {
                processed += 1
                if processed % 65_536 == 0 { try Task.checkCancellation() }
                if byte == 0 { throw failure("NUL bytes are unsupported. Save the file as UTF-8 CSV (not UTF-16).") }
                if skipLF {
                    skipLF = false
                    if byte == 10 { previousCR = false; continue }
                }
                if state == .quoted {
                    if byte == 34 { state = .afterQuote } else { field.append(byte) }
                } else if byte == delimiter {
                    try appendField()
                    state = .start
                    started = true
                } else if byte == 10 || byte == 13 {
                    try endRow()
                    skipLF = byte == 13
                } else {
                    switch state {
                    case .start:
                        started = true
                        if byte == 34 { state = .quoted } else { state = .unquoted; field.append(byte) }
                    case .unquoted:
                        guard byte != 34 else { throw failure("quote inside an unquoted field; enclose the whole field in quotes.") }
                        field.append(byte)
                    case .afterQuote:
                        guard byte == 34 else { throw failure("expected a delimiter or newline after closing quote.") }
                        field.append(34)
                        state = .quoted
                    case .quoted: break
                    }
                }
                if byte == 13 || (byte == 10 && !previousCR) { line += 1 }
                previousCR = byte == 13
            }
        }

        mutating func finish() throws -> TableData {
            if state == .quoted { throw failure("unterminated quoted field.") }
            if started { try endRow() }
            return try TableData(records: records)
        }
    }
}
