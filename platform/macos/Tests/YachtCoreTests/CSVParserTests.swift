import XCTest
@testable import YachtCore

final class CSVParserTests: XCTestCase {
    func parse(_ text: String, delimiter: CSVDelimiter = .comma) throws -> TableData {
        try CSVParser.parse(Data(text.utf8), delimiter: delimiter)
    }
    func testQuotedCommasQuotesAndMultiline() throws {
        let t = try parse("A,B,C\r\n\"a,b\",\"a\"\"b\",\"x\r\ny\nz\rw\"\r\n")
        XCTAssertEqual(t.rows, [["a,b", "a\"b", "x\r\ny\nz\rw"]])
    }
    func testNewlinesBlankRowsAndTrailingFields() throws {
        for newline in ["\n", "\r", "\r\n"] {
            let t = try parse("A,B" + newline + "1," + newline + newline + "," + newline)
            XCTAssertEqual(t.rows, [["1", ""], [], ["", ""]])
        }
        XCTAssertEqual(try parse("A,B\n1,").rows, [["1", ""]])
        XCTAssertEqual(try parse("\"\"").header, [""])
    }
    func testUnicodeBOMAndEmoji() throws {
        let t = try parse("\u{feff}名前,Emoji\nCafe\u{301},🛥️\n")
        XCTAssertEqual(t.header, ["名前", "Emoji"])
        XCTAssertEqual(t.rows[0], ["Cafe\u{301}", "🛥️"])
    }
    func testRaggedRowsRetainData() throws {
        let t = try parse("A,B\n1\n2,3,4\n\n")
        XCTAssertEqual(t.header, ["A", "B", "Column 3"])
        XCTAssertEqual(t.rows[1], ["2", "3", "4"])
        XCTAssertEqual(t.extraRowCount, 1)
        XCTAssertEqual(t.shortRowCount, 2)
    }
    func testAlternativeDelimiters() throws {
        for d in CSVDelimiter.allCases {
            let separator = String(UnicodeScalar(d.byte))
            let t = try parse("A\(separator)B\n\"one\(separator)two\"\(separator)three", delimiter: d)
            XCTAssertEqual(t.rows, [["one\(separator)two", "three"]])
        }
    }
    func testMalformedData() throws {
        for text in ["", "A\n\"unclosed", "A\n\"closed\"oops", "A\nbad\"quote", "A\n\0"] {
            XCTAssertThrowsError(try parse(text), text)
        }
        XCTAssertThrowsError(try CSVParser.parse(Data([0xff, 0xfe, 0x41, 0])))
        XCTAssertThrowsError(try CSVParser.parse(Data([0x41, 0x0a, 0xff])))
    }
    func testOneByteChunksIncludingBOMAndUnicode() throws {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: url) }
        let data = Data("\u{feff}A,B\r\n\"🛥️\r\n漢字\",\"a\"\"b\"\r\n".utf8)
        try data.write(to: url)
        for size in [1, 2, 3, 7, 65_536] {
            XCTAssertEqual(try CSVParser.read(url, chunkSize: size), try CSVParser.parse(data))
        }
    }
    func testLargeTableAndLongCells() throws {
        let long = String(repeating: "🛥", count: 50_000)
        let t = try parse("A,B\n\"\(long)\",x\n" + String(repeating: "123456,hello\n", count: 100_000))
        XCTAssertEqual(t.rows.count, 100_001)
        XCTAssertEqual(t.rows.first?.first, long)
        XCTAssertTrue(try HTMLGenerator.document(t, style: .init(), rowLimit: 10).utf8.count < 300_000)
    }
}
