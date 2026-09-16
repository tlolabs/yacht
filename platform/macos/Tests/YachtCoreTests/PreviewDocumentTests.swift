import XCTest
@testable import YachtCore

final class PreviewDocumentTests: XCTestCase {
    func testPreviewIsGeneratedHTMLWithExplicitRowLimit() throws {
        let preview = try PreviewDocument(table: .sample, style: .init(), limit: 2)
        XCTAssertEqual(preview.rowCount, 2)
        XCTAssertEqual(preview.html, try HTMLGenerator.document(.sample, style: .init(), rowLimit: 2))
        XCTAssertEqual(preview.source, try HTMLGenerator.document(.sample, style: .init()))
        XCTAssertFalse(preview.sourceTruncated)
    }
    func testHugeCellDoesNotMakeUnboundedPreview() throws {
        let table = try TableData(records: [["A"], [String(repeating: "🛥️", count: 200_000)]])
        let preview = try PreviewDocument(table: table, style: .unstyled)
        XCTAssertTrue(preview.previewUnavailable)
        XCTAssertTrue(preview.sourceTruncated)
        XCTAssertLessThanOrEqual(preview.source.utf8.count, 1_000_000)
        XCTAssertFalse(preview.source.contains("\u{fffd}"))
    }
    func testMissingCellsCountTowardPreviewBudget() throws {
        let header = Array(repeating: "A", count: 10_000)
        let table = try TableData(records: [header] + Array(repeating: [], count: 200))
        let preview = try PreviewDocument(table: table, style: .unstyled)
        XCTAssertLessThan(preview.rowCount, 3)
        XCTAssertLessThan(preview.html.utf8.count, 2_000_000)
    }
    func testUnstyledPreviewAndSourceHaveNoCSS() throws {
        let preview = try PreviewDocument(table: .sample, style: .unstyled, limit: 2)
        XCTAssertFalse(preview.html.contains("<style>"))
        XCTAssertFalse(preview.source.contains("<style>"))
        XCTAssertTrue(preview.source.contains("scope=\"col\""))
        XCTAssertEqual(preview.rowCount, 2)
    }
}
