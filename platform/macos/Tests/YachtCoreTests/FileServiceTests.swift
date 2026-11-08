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
            let expected = try HTMLGenerator.output(.sample, style: .init())
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), expected)
            XCTAssertThrowsError(try FileService.export(.sample, style: .unstyled, to: url))
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), expected)
            try FileService.export(.sample, style: .unstyled, to: url, overwrite: true)
            XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), try HTMLGenerator.output(.sample, style: .unstyled))
            XCTAssertEqual(try FileManager.default.contentsOfDirectory(atPath: directory.path), ["table.html"])
        }
    }
    func testOutputModeReachesExportConvertAndBatch() throws {
        try withDirectory { directory in
            let input = directory.appendingPathComponent("source.csv")
            try Data("Name,Count\nCompass,12\n".utf8).write(to: input)
            for mode in HTMLOutputMode.allCases {
                let url = directory.appendingPathComponent("export-\(mode.rawValue).html")
                try FileService.export(.sample, style: .init(), to: url, outputMode: mode)
                XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), try HTMLGenerator.output(.sample, style: .init(), outputMode: mode))
                let converted = directory.appendingPathComponent("converted-\(mode.rawValue).html")
                try FileService.convert(input, to: converted, style: .unstyled, outputMode: mode)
                let batch = try FileService.batchItem(input, style: .unstyled, delimiter: .comma, overwrite: true, outputMode: mode)
                for output in [converted, batch] {
                    let html = try String(contentsOf: output, encoding: .utf8)
                    XCTAssertEqual(html.hasPrefix("<!doctype html>"), mode == .document)
                    XCTAssertTrue(html.contains("Compass</td>"))
                }
            }
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
    func testPresetsRoundTripAndPartialDefaults() throws {
        try withDirectory { directory in
            let repo = PresetRepository(url: directory.appendingPathComponent("presets.json"))
            XCTAssertEqual(try repo.load(), [:])
            try repo.save(["Teaching": .unstyled])
            XCTAssertEqual(try repo.load(), ["Teaching": .unstyled])
            XCTAssertThrowsError(try repo.save(["Unstyled": .init()]))
            let decoded = try JSONDecoder().decode(StyleOptions.self, from: Data("{\"font_size_px\":22}".utf8))
            XCTAssertEqual(decoded.fontSizePx, 22); XCTAssertEqual(decoded.borderColor, "#cccccc")
            let encoded = try JSONEncoder().encode(decoded)
            XCTAssertEqual(try JSONDecoder().decode(StyleOptions.self, from: encoded), decoded)
        }
    }
    func testRecoveryPreservesEachUnreadableStoreBeforeReplacement() throws {
        try withDirectory { directory in
            let repo = PresetRepository(url: directory.appendingPathComponent("presets.json"))
            var backups: [URL] = []
            for original in ["broken first store", "broken second store"] {
                try Data(original.utf8).write(to: repo.url)
                let backup = try XCTUnwrap(repo.recoverWithBackup())
                XCTAssertEqual(try String(contentsOf: backup, encoding: .utf8), original)
                XCTAssertEqual(try repo.load(), [:])
                backups.append(backup)
            }
            XCTAssertNotEqual(backups[0], backups[1])
            XCTAssertEqual(try String(contentsOf: backups[0], encoding: .utf8), "broken first store")
        }
    }
    func testRecoveryFailureDoesNotRemoveOriginalStore() throws {
        try withDirectory { directory in
            let repo = PresetRepository(url: directory.appendingPathComponent("presets.json"))
            try FileManager.default.createDirectory(at: repo.url, withIntermediateDirectories: false)
            let original = repo.url.appendingPathComponent("preserved")
            try Data("KEEP".utf8).write(to: original)
            XCTAssertThrowsError(try repo.recoverWithBackup())
            XCTAssertEqual(try String(contentsOf: original, encoding: .utf8), "KEEP")
        }
    }
    func testCancelledExportLeavesExistingFile() async throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let url = directory.appendingPathComponent("safe.html")
        try Data("KEEP".utf8).write(to: url)
        let task = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            try FileService.export(.sample, style: .init(), to: url, overwrite: true)
        }
        do { try await task.value; XCTFail("Expected cancellation") } catch is CancellationError {} catch { XCTFail("\(error)") }
        XCTAssertEqual(try String(contentsOf: url, encoding: .utf8), "KEEP")
    }
}

final class RustSettingsTests: XCTestCase {
    func testSharedBehaviorDefaultsAndValidation() throws {
        var settings = BehaviorSettings.defaults
        XCTAssertTrue(settings.rememberStyle)
        XCTAssertEqual(settings.previewRows, 200)
        XCTAssertEqual(settings.outputMode, .table)
        settings.lastStyle = .unstyled
        XCTAssertEqual(try settings.initialStyle(), .unstyled)
        settings.rememberStyle = false
        XCTAssertEqual(try settings.initialStyle(), StyleOptions())
        settings.previewRows = 0
        XCTAssertThrowsError(try settings.initialStyle())
    }
    func testExistingSettingsDefaultToTableOutputAndPersistMode() throws {
        let old = Data("{\"remember_style\":true,\"preview_rows\":200,\"last_style\":{}}".utf8)
        var settings = try JSONDecoder().decode(BehaviorSettings.self, from: old)
        XCTAssertEqual(settings.outputMode, .table)
        settings.outputMode = .document
        let encoded = try JSONEncoder().encode(settings)
        XCTAssertEqual(try JSONDecoder().decode(BehaviorSettings.self, from: encoded).outputMode, .document)
    }
    func testSharedBatchInfersTSVAndReportsCollision() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let input = directory.appendingPathComponent("table.tsv")
        try Data("A\tB\n1\t2".utf8).write(to: input)
        let output = try FileService.batchItem(input, style: .unstyled, delimiter: .comma, overwrite: false)
        XCTAssertTrue(try String(contentsOf: output, encoding: .utf8).contains("Field #2"))
        XCTAssertThrowsError(try FileService.batchItem(input, style: .unstyled, delimiter: .comma, overwrite: false))
    }
}
