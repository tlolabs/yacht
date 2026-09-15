import Foundation

public struct TableData: Sendable, Equatable {
    public let header: [String]
    public let rows: [[String]]
    public let shortRowCount: Int
    public let extraRowCount: Int

    public init(records: [[String]]) throws {
        guard let first = records.first else { throw YachtError.invalid("CSV file is empty.") }
        let body = Array(records.dropFirst())
        let width = max(1, first.count, body.lazy.map(\.count).max() ?? 0)
        header = first + (first.count..<width).map { "Column \($0 + 1)" }
        rows = body
        shortRowCount = body.reduce(0) { $0 + ($1.count < first.count ? 1 : 0) }
        extraRowCount = body.reduce(0) { $0 + ($1.count > first.count ? 1 : 0) }
    }

    public var warnings: [String] {
        var values: [String] = []
        if shortRowCount > 0 { values.append("\(shortRowCount) row(s) have missing cells; exported cells will be blank.") }
        if extraRowCount > 0 { values.append("\(extraRowCount) row(s) have extra cells; additional columns are preserved.") }
        return values
    }

    public static let sample: TableData = try! TableData(records: [
        ["Year", "Album details", "Peak chart position"],
        ["1990", "Surprise", "—"], ["1993", "Deluxe", "35"],
        ["1996", "Friction, Baby", "64"], ["1998", "How Does Your Garden Grow?", "129"],
        ["2001", "Closer", "110"], ["2005", "Before the Robots", "84"],
        ["2009", "Paper Empire", "62"], ["2014", "All Together Now", "43"],
        ["2024", "Super Magick", "—"]
    ])
}

public enum YachtError: LocalizedError, Sendable {
    case invalid(String)
    public var errorDescription: String? {
        switch self { case .invalid(let message): message }
    }
}
