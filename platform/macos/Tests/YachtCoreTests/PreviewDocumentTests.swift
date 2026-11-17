import XCTest
@testable import YachtCore

final class PreviewDocumentTests: XCTestCase {
    func testPreviewIsGeneratedHTMLWithExplicitRowLimit() throws {
        let preview = try PreviewDocument(table: .sample, style: .init(), limit: 2)
        XCTAssertEqual(preview.rowCount, 2)
        XCTAssertEqual(preview.html, try HTMLGenerator.document(.sample, style: .init(), rowLimit: 2))
        XCTAssertEqual(preview.source, try HTMLGenerator.output(.sample, style: .init()))
        XCTAssertFalse(preview.sourceTruncated)
    }
    func testOutputModeChangesSourceAndRetainsCompletePreviewDocument() throws {
        let table = try PreviewDocument(table: .sample, style: .init(), limit: 2, outputMode: .table)
        let document = try PreviewDocument(table: .sample, style: .init(), limit: 2, outputMode: .document)
        XCTAssertEqual(table.html, document.html)
        XCTAssertTrue(table.html.hasPrefix("<!doctype html>"))
        XCTAssertTrue(table.source.hasPrefix("<table"))
        XCTAssertTrue(document.source.hasPrefix("<!doctype html>"))
        XCTAssertEqual(document.source, try HTMLGenerator.document(.sample, style: .init()))
    }
    func testHugeCellDoesNotMakeUnboundedPreview() throws {
        let table = try TableData(records: [["A"], [String(repeating: "🛥️", count: 400_000)]])
        let preview = try PreviewDocument(table: table, style: .unstyled)
        XCTAssertTrue(preview.previewUnavailable)
        XCTAssertEqual(preview.rowCount, 0)
        XCTAssertTrue(preview.sourceTruncated)
        XCTAssertLessThanOrEqual(preview.source.utf8.count, 1_000_000)
        XCTAssertFalse(preview.source.contains("\u{fffd}"))
    }
    func testMissingCellsCountTowardPreviewBudget() throws {
        let header = Array(repeating: "A", count: 10_000)
        let table = try TableData(records: [header] + Array(repeating: [], count: 200))
        let preview = try PreviewDocument(table: table, style: .unstyled)
        XCTAssertFalse(preview.previewUnavailable)
        XCTAssertGreaterThan(preview.rowCount, 0)
        XCTAssertLessThan(preview.rowCount, 200)
        XCTAssertLessThanOrEqual(preview.html.utf8.count, 2_000_000)
        XCTAssertEqual(preview.html, try HTMLGenerator.document(table, style: .unstyled, rowLimit: preview.rowCount))
    }
    func testLargeCellsThatFitTheRenderedBudgetRemainVisible() throws {
        for cell in [String(repeating: "a", count: 400_000), String(repeating: "🛥️", count: 200_000)] {
            let table = try TableData(records: [["A"], [cell]])
            let preview = try PreviewDocument(table: table, style: .unstyled, outputMode: .document)
            let full = try HTMLGenerator.document(table, style: .unstyled)
            XCTAssertFalse(preview.previewUnavailable)
            XCTAssertEqual(preview.rowCount, 1)
            XCTAssertEqual(preview.html, full)
            XCTAssertTrue(full.hasPrefix(preview.source))
            XCTAssertEqual(preview.sourceTruncated, full.utf8.count > preview.source.utf8.count)
            XCTAssertLessThanOrEqual(preview.source.utf8.count, 1_000_000)
        }
    }
    func testUnstyledPreviewAndSourceHaveNoCSS() throws {
        let preview = try PreviewDocument(table: .sample, style: .unstyled, limit: 2)
        XCTAssertFalse(preview.html.contains("<style>"))
        XCTAssertFalse(preview.source.contains("<style>"))
        XCTAssertTrue(preview.source.contains("scope=\"col\""))
        XCTAssertEqual(preview.rowCount, 2)
    }
}
