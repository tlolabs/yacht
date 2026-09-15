import XCTest
import AppKit

@MainActor final class YachtUITests: XCTestCase {
    var app: XCUIApplication!
    var directory: URL!
    override func setUp() async throws {
        continueAfterFailure = false
        directory = FileManager.default.temporaryDirectory.appendingPathComponent("yacht-ui-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        app = XCUIApplication()
        app.launchArguments = ["--ui-testing"]
    }
    override func tearDown() async throws {
        app.terminate()
        try? FileManager.default.removeItem(at: directory)
    }
    func launch(csv: String? = nil) throws {
        if let csv {
            let file = directory.appendingPathComponent("input.csv")
            try Data(csv.utf8).write(to: file)
            app.launchArguments += ["--open", file.path]
        }
        app.launch()
        XCTAssertTrue(app.windows.firstMatch.waitForExistence(timeout: 10))
    }
    func testSamplePreviewSourceCopyAndReset() throws {
        try launch()
        XCTAssertTrue(app.webViews.firstMatch.waitForExistence(timeout: 10))
        XCTAssertTrue(app.webViews.staticTexts["Friction, Baby"].waitForExistence(timeout: 10))
        app.typeKey("2", modifierFlags: .command)
        XCTAssertTrue(app.staticTexts["htmlSource"].waitForExistence(timeout: 5))
        app.buttons["copyHTML"].click()
        XCTAssertTrue(app.staticTexts["Copied complete HTML document"].waitForExistence(timeout: 10))
        XCTAssertTrue(NSPasteboard.general.string(forType: .string)?.contains("scope=\"col\"") == true)
        app.buttons["Reset Unstyled"].click()
        app.buttons["copyHTML"].click()
        let copied = NSPredicate { _, _ in !(NSPasteboard.general.string(forType: .string) ?? "<style>").contains("<style>") }
        expectation(for: copied, evaluatedWith: nil)
        waitForExpectations(timeout: 10)
        app.buttons["Reset Styled"].click()
    }
    func testImportEscapingAndExtraColumnWarning() throws {
        try launch(csv: "Name,Value\n<script>alert(1)</script>,17%\nCafé,🛥️,extra\n")
        XCTAssertTrue(app.staticTexts.containing(NSPredicate(format: "label CONTAINS %@", "additional columns are preserved")).firstMatch.waitForExistence(timeout: 10))
        XCTAssertTrue(app.webViews.staticTexts["<script>alert(1)</script>"].waitForExistence(timeout: 10))
        app.buttons["copyHTML"].click()
        XCTAssertTrue(app.staticTexts["Copied complete HTML document"].waitForExistence(timeout: 10))
        let html = NSPasteboard.general.string(forType: .string) ?? ""
        XCTAssertTrue(html.contains("&lt;script&gt;")); XCTAssertFalse(html.contains("<script>"))
        XCTAssertTrue(html.contains("Column 3")); XCTAssertTrue(html.contains("extra</td>"))
    }
    func testMalformedImportShowsErrorAndDisablesExport() throws {
        try launch(csv: "A,B\n\"unterminated")
        XCTAssertTrue(app.alerts.firstMatch.waitForExistence(timeout: 10))
        XCTAssertTrue(app.alerts.staticTexts.containing(NSPredicate(format: "label CONTAINS %@", "unterminated quoted field")).firstMatch.exists)
        app.alerts.buttons["OK"].click()
        XCTAssertFalse(app.buttons["exportHTML"].isEnabled)
    }
    func testNativeOpenAndExportDialogs() throws {
        try launch()
        app.typeKey("o", modifierFlags: .command)
        XCTAssertTrue(app.dialogs.firstMatch.waitForExistence(timeout: 5))
        app.typeKey(.escape, modifierFlags: [])
        XCTAssertTrue(app.buttons["exportHTML"].waitForExistence(timeout: 5))
        app.typeKey("s", modifierFlags: .command)
        XCTAssertTrue(app.dialogs.firstMatch.waitForExistence(timeout: 10))
        app.typeKey("g", modifierFlags: [.command, .shift])
        app.typeText(directory.path + "/table.html")
        app.typeKey(.return, modifierFlags: [])
        let save = app.buttons["Save"].firstMatch
        XCTAssertTrue(save.waitForExistence(timeout: 5)); save.click()
        let url = directory.appendingPathComponent("table.html")
        let exists = NSPredicate { _, _ in FileManager.default.fileExists(atPath: url.path) }
        expectation(for: exists, evaluatedWith: nil); waitForExpectations(timeout: 10)
        XCTAssertTrue(try String(contentsOf: url, encoding: .utf8).contains("Friction, Baby"))
        XCTAssertTrue(app.buttons["Reveal in Finder"].waitForExistence(timeout: 5))
    }
    func testPresetSaveLoadAndDelete() throws {
        try launch()
        app.buttons["Reset Unstyled"].click()
        let name = app.textFields["presetName"]
        name.click(); name.typeText("Classroom")
        app.buttons["Save Current"].click()
        XCTAssertTrue(app.staticTexts["Saved preset “Classroom”"].waitForExistence(timeout: 5))
        app.buttons["Reset Styled"].click()
        let picker = app.popUpButtons["Preset"]
        picker.click(); app.menuItems["Classroom"].click()
        app.buttons["loadPreset"].click()
        app.buttons["copyHTML"].click()
        XCTAssertTrue(app.staticTexts["Copied complete HTML document"].waitForExistence(timeout: 5))
        XCTAssertFalse((NSPasteboard.general.string(forType: .string) ?? "<style>").contains("<style>"))
        app.buttons["Delete"].click()
        app.buttons.matching(identifier: "Delete").allElementsBoundByIndex.last!.click()
    }
}
