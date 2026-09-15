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
    func testDefaultsMatchLegacyExactly() throws {
        let data = try Data(contentsOf: Bundle.module.resourceURL!.appendingPathComponent("Fixtures/default-options.json"))
        XCTAssertEqual(try JSONDecoder().decode(StyleOptions.self, from: data), StyleOptions())
    }
}
