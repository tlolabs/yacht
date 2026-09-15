import XCTest
@testable import YachtCore

final class FileServiceTests: XCTestCase {
    func withDirectory(_ body: (URL) throws -> Void) throws {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: url) }
        try body(url)
    }
    func testExportRoundTripAndReplacementSafeguard() throws {
        try withDirectory { directory in
            let url = directory.appendingPathComponent("table.html")
            try FileService.export(.sample, style: .init(), to: url)
            let expected = try HTMLGenerator.document(.sample, style: .init())
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), expected)
            XCTAssertThrowsError(try FileService.export(.sample, style: .unstyled, to: url))
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), expected)
            try FileService.export(.sample, style: .unstyled, to: url, overwrite: true)
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), try HTMLGenerator.document(.sample, style: .unstyled))
            XCTAssertEqual(try FileManager.default.contentsOfDirectory(atPath: directory.path), ["table.html"])
        }
    }
    func testMalformedImportDoesNotCreateOrTruncateOutput() throws {
        try withDirectory { directory in
            let source = directory.appendingPathComponent("bad.csv"), output = directory.appendingPathComponent("bad.html")
            try Data("A\n\"bad".utf8).write(to: source)
            XCTAssertThrowsError(try FileService.convert(source, to: output, style: .init()))
            XCTAssertFalse(FileManager.default.fileExists(atPath: output.path))
            try Data("KEEP".utf8).write(to: output)
            XCTAssertThrowsError(try FileService.convert(source, to: output, style: .init(), overwrite: true))
            XCTAssertEqual(try String(contentsOf: output, encoding: .utf8), "KEEP")
            XCTAssertThrowsError(try FileService.convert(source, to: source, style: .init(), overwrite: true))
        }
    }
    func testPresetsRoundTripAndLegacyDefaults() throws {
        try withDirectory { directory in
            let repo = PresetRepository(url: directory.appendingPathComponent("presets.json"))
            XCTAssertEqual(try repo.load(), [:])
            try repo.save(["Teaching": .unstyled])
            XCTAssertEqual(try repo.load(), ["Teaching": .unstyled])
            XCTAssertThrowsError(try repo.save(["Unstyled": .init()]))
            let decoded = try JSONDecoder().decode(StyleOptions.self, from: Data("{\"font_size_px\":22}".utf8))
            XCTAssertEqual(decoded.fontSizePx, 22); XCTAssertEqual(decoded.borderColor, "#cccccc")
            let fixture = Bundle.module.resourceURL!.appendingPathComponent("Fixtures/default-options.json")
            XCTAssertEqual(try JSONDecoder().decode(StyleOptions.self, from: Data(contentsOf: fixture)), StyleOptions())
        }
    }
    func testCancelledExportLeavesExistingFile() async throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let url = directory.appendingPathComponent("safe.html")
        try Data("KEEP".utf8).write(to: url)
        let task = Task {
            try await Task.sleep(for: .milliseconds(100))
            try FileService.export(.sample, style: .init(), to: url, overwrite: true)
        }
        task.cancel()
        do { try await task.value; XCTFail("Expected cancellation") } catch is CancellationError {} catch { XCTFail("\(error)") }
        XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), "KEEP")
    }
}
