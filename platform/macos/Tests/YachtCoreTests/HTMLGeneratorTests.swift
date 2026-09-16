import XCTest
@testable import YachtCore

final class HTMLGeneratorTests: XCTestCase {
    func testCurrentDocumentContract() throws {
        let table = try CSVParser.parse(Data("Item,Count,Notes\nCompass,12,\nCafé,3,\"north & south\"\n".utf8))
        for style in [StyleOptions(), .unstyled] {
            let html = try HTMLGenerator.document(table, style: style)
            XCTAssertTrue(html.hasPrefix("<!doctype html>"))
            XCTAssertTrue(html.contains("<title>YACHT Table</title>"))
            XCTAssertTrue(html.contains("name=\"viewport\""))
            XCTAssertEqual(html.components(separatedBy: "scope=\"col\"").count - 1, 3)
            XCTAssertTrue(html.contains("Compass</td>"))
            XCTAssertTrue(html.contains("Café</td>"))
            XCTAssertTrue(html.contains("north &amp; south</td>"))
            XCTAssertTrue(html.contains("<td></td>"))
        }
    }
    func testEscapingAndNoExecutableCellHTML() throws {
        XCTAssertEqual(HTMLGenerator.escape("&<>\"'"), "&amp;&lt;&gt;&quot;&#x27;")
        let table = try TableData(records: [["<img onerror=alert(1)>"], ["</td><script>alert('x')</script>"]])
        var style = StyleOptions(); style.tableClass = "\" onmouseover=\"alert(1)"
        let html = try HTMLGenerator.document(table, style: style)
        XCTAssertFalse(html.contains("<script")); XCTAssertFalse(html.contains("<img"))
        XCTAssertTrue(html.contains("scope=\"col\""))
        XCTAssertTrue(html.contains("&quot; onmouseover=&quot;"))
    }
    func testEscapingCombiningCharactersCannotBreakAttributes() throws {
        let value = "\"\u{fe0f} onmouseover=\"alert(1)"
        var style = StyleOptions(); style.tableClass = value
        let html = try HTMLGenerator.document(.sample, style: style)
        XCTAssertTrue(html.contains("&quot;\u{fe0f} onmouseover=&quot;alert(1)"))
        XCTAssertEqual(HTMLGenerator.escape("<\u{301}&\u{fe0f}'\u{301}"), "&lt;\u{301}&amp;\u{fe0f}&#x27;\u{301}")
    }
    func testStyleInjectionBlocked() {
        for attack in ["</style><script>alert(1)</script>", "red; background:url(https://example.com)", "url(file:///etc/passwd)", "red\\3c"] {
            var s = StyleOptions(); s.fontFamily = attack
            XCTAssertThrowsError(try s.validate())
            s = .init(); s.headerBg = attack
            XCTAssertThrowsError(try s.validate())
        }
    }
    func testStyledUnstyledAndAllStyleOptions() throws {
        var s = StyleOptions()
        s.fontFamily = "'Times New Roman', serif"; s.fontSizePx = 20; s.cellPaddingPx = 12
        s.borderWidthPx = 3; s.borderStyle = "dashed"; s.borderColor = "rebeccapurple"
        s.headerBg = "rgb(1, 2, 3)"; s.headerTextColor = "white"; s.bodyBg = "#ffeecc"
        s.zebraEnabled = false; s.hoverEnabled = false; s.borderCollapse = "separate"; s.borderSpacingPx = 4
        let html = try HTMLGenerator.document(.sample, style: s)
        for token in ["20px", "12px", "3px dashed rebeccapurple", "rgb(1, 2, 3)", "#ffeecc", "border-spacing: 4px"] { XCTAssertTrue(html.contains(token)) }
        XCTAssertFalse(html.contains("nth-child")); XCTAssertFalse(html.contains("tr:hover"))
        let bare = try HTMLGenerator.document(.sample, style: .unstyled)
        XCTAssertFalse(bare.contains("<style>")); XCTAssertFalse(bare.contains("class="))
    }
    func testNumericAlignment() {
        for value in ["0", "-4", "+3", "1234", "12345", "1,234.5", " 12.0 ", "123456789"] { XCTAssertTrue(HTMLGenerator.isNumeric(value), value) }
        for value in ["", "17%", "1,23", "1,2345", "NaN", "1e4", "—", "3a"] { XCTAssertFalse(HTMLGenerator.isNumeric(value), value) }
    }
    func testExtraAndMissingColumnsRendered() throws {
        let t = try CSVParser.parse(Data("A,B\n1\n2,3,4\n\n".utf8))
        let html = try HTMLGenerator.document(t, style: .unstyled)
        XCTAssertTrue(html.contains("Column 3</th>"))
        XCTAssertTrue(html.contains("<tr><td>1</td><td></td><td></td></tr>"))
        XCTAssertTrue(html.contains("<tr><td>2</td><td>3</td><td>4</td></tr>"))
    }
}
