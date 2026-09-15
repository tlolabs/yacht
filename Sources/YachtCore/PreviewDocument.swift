import Foundation

public struct PreviewDocument: Sendable {
    public let html: String
    public let source: String
    public let rowCount: Int
    public let sourceTruncated: Bool
    public let previewUnavailable: Bool

    public init(table: TableData, style: StyleOptions, limit: Int = 200) throws {
        // Bound DOM size by rows AND source bytes. Full exports remain unlimited.
        let budget = 2_000_000
        var bytes = table.header.reduce(0) { $0 + $1.utf8.count * 6 + 100 }
        var count = 0
        for row in table.rows.prefix(limit) {
            let cost = table.header.count * 100 + row.reduce(0) { $0 + $1.utf8.count * 6 }
            if bytes + cost > budget { break }
            bytes += cost; count += 1
        }
        previewUnavailable = bytes > budget || (count == 0 && !table.rows.isEmpty)
        html = previewUnavailable ? "" : try HTMLGenerator.document(table, style: style, rowLimit: count)
        rowCount = count
        var collected = "", collectedBytes = 0, truncated = false
        enum LimitReached: Error { case reached }
        do {
            try HTMLGenerator.write(table, style: style) { chunk in
                let remaining = 1_000_000 - collectedBytes
                guard chunk.utf8.count <= remaining else {
                    // Avoid splitting Unicode scalars in a displayed source excerpt.
                    for scalar in chunk.unicodeScalars {
                        let text = String(scalar)
                        if collectedBytes + text.utf8.count > 1_000_000 { break }
                        collected += text; collectedBytes += text.utf8.count
                    }
                    truncated = true
                    throw LimitReached.reached
                }
                collected += chunk; collectedBytes += chunk.utf8.count
            }
        } catch LimitReached.reached {}
        source = collected; sourceTruncated = truncated
    }
}
