import XCTest
@testable import YachtCore

final class HTMLGeneratorTests: XCTestCase {
    func testRegressionAgainstPythonFixtures() throws {
        for name in ["ordinary", "quoted", "unicode", "escaping", "bom"] {
            let base = Bundle.module.resourceURL!.appendingPathComponent("Fixtures")
            let table = try CSVParser.read(base.appendingPathComponent(name + ".csv"))
            for (suffix, style) in [("styled", StyleOptions()), ("unstyled", .unstyled)] {
                var expected = try String(contentsOf: base.appendingPathComponent(name + ".python-" + suffix + ".html"), encoding: .utf8)
                // Only approved semantic changes. Ragged data is tested separately.
                expected = expected.replacingOccurrences(of: "<th title=", with: "<th scope=\"col\" title=")
                expected = expected.replacingOccurrences(of: "  <meta charset=\"utf-8\">\n", with: "  <meta charset=\"utf-8\">\n  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n")
                if suffix == "styled" { expected = expected.replacingOccurrences(of: "<td>1234</td>", with: "<td class=\"num\">1234</td>") }
                XCTAssertEqual(try HTMLGenerator.document(table, style: style), expected, name + suffix)
            }
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
