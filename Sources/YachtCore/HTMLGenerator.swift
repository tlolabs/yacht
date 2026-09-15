import Foundation

public enum HTMLGenerator {
    public static func escape(_ text: String) -> String {
        guard text.utf8.contains(where: { $0 == 38 || $0 == 60 || $0 == 62 || $0 == 34 || $0 == 39 }) else { return text }
        var result = ""
        result.reserveCapacity(text.utf8.count)
        for c in text {
            switch c {
            case "&": result += "&amp;"
            case "<": result += "&lt;"
            case ">": result += "&gt;"
            case "\"": result += "&quot;"
            case "'": result += "&#x27;"
            default: result.append(c)
            }
        }
        return result
    }

    public static func isNumeric(_ value: String) -> Bool {
        let text = (value.first?.isWhitespace == true || value.last?.isWhitespace == true)
            ? value.trimmingCharacters(in: .whitespacesAndNewlines) : value
        var digits = 0, position = 0
        var grouped = false, fraction = false
        for byte in text.utf8 {
            defer { position += 1 }
            if position == 0 && (byte == 43 || byte == 45) { continue }
            if (48...57).contains(byte) { digits += 1; continue }
            if byte == 44 {
                guard !fraction, digits > 0, grouped ? digits == 3 : digits <= 3 else { return false }
                grouped = true; digits = 0
            } else if byte == 46 {
                guard !fraction, digits > 0, !grouped || digits == 3 else { return false }
                fraction = true; digits = 0
            } else { return false }
        }
        return digits > 0 && (fraction || !grouped || digits == 3)
    }

    public static func document(_ table: TableData, style: StyleOptions, rowLimit: Int? = nil) throws -> String {
        var result = ""
        try write(table, style: style, rowLimit: rowLimit) { result += $0 }
        return result
    }

    /// Streams HTML to a caller-owned sink. Export never allocates a second full HTML document.
    public static func write(_ table: TableData, style s: StyleOptions, rowLimit: Int? = nil, sink: (String) throws -> Void) throws {
        try s.validate()
        try Task.checkCancellation()
        let unstyled = s.isUnstyled
        var css: [String] = []
        if !unstyled {
            css = [".csv-table-wrap {", "  font-family: \(s.fontFamily.isEmpty ? "inherit" : s.fontFamily);", "  font-size: \(s.fontSizePx)px;", "}",
                ".csv-table {", "  width: 100%;", "  border-collapse: \(s.borderCollapse);", "  border-spacing: \(s.borderSpacingPx)px;", "}",
                ".csv-table th, .csv-table td {", "  border: \(s.borderWidthPx)px \(s.borderStyle) \(s.borderColor);", "  padding: \(s.cellPaddingPx)px;", "  vertical-align: top;", "}",
                ".csv-table th {", "  background: \(s.headerBg);", "  color: \(s.headerTextColor);", "  text-align: left;", "}",
                ".csv-table td {", "  background: \(s.bodyBg);", "}", ".csv-table td.num {", "  text-align: right;", "}"]
            if s.zebraEnabled { css += [".csv-table tbody tr:nth-child(even) td {", "  background: \(s.zebraBg);", "}"] }
            if s.hoverEnabled { css += [".csv-table tbody tr:hover td {", "  background: \(s.hoverBg);", "}"] }
        }
        try sink("<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n  <title>Y.A.C.H.T. Table</title>\n")
        if !css.isEmpty {
            try sink("  <style>\n")
            for line in css { try sink("    \(line)\n") }
            try sink("  </style>\n")
        }
        let suffix = s.tableClass.isEmpty ? "" : " " + escape(s.tableClass)
        let classes = unstyled ? "" : " class=\"csv-table\(suffix)\""
        try sink("</head>\n<body>\n" + (unstyled ? "  <div>\n" : "  <div class=\"csv-table-wrap\">\n"))
        try sink("    <table\(classes)>\n      <thead><tr>")
        for (i, col) in table.header.enumerated() { try sink("<th scope=\"col\" title=\"Field #\(i + 1)\">\(escape(col))</th>") }
        try sink("</tr></thead>\n      <tbody>\n")
        for (i, row) in table.rows.prefix(max(0, rowLimit ?? table.rows.count)).enumerated() {
            if i % 256 == 0 { try Task.checkCancellation() }
            var line = "        <tr>"
            for index in table.header.indices {
                let cell = index < row.count ? row[index] : ""
                let cls = !unstyled && isNumeric(cell) ? " class=\"num\"" : ""
                line += "<td\(cls)>\(escape(cell))</td>"
            }
            try sink(line + "</tr>\n")
        }
        try sink("      </tbody>\n    </table>\n  </div>\n</body>\n</html>\n")
    }
}
