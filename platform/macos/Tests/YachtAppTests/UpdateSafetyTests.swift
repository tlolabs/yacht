import XCTest
import Sparkle
@testable import YachtApp

final class UpdateSafetyTests: XCTestCase {
    func testStableNewerVersionAndArchitecture() {
        XCTAssertTrue(accepts("2.10.0", installed: "2.9.0"))
        for version in ["2.1.1", "2.0.0", "2.1", "02.2.0", "2.2.0-rc.1", "2.2.0+build", "invalid"] {
            XCTAssertFalse(accepts(version))
        }
        XCTAssertFalse(UpdateSafety.accepts(version: "2.2.0", installed: "2.1.1", url: URL(string: "https://github.com/tlolabs/yacht/releases/download/v2.2.0/YACHT-macos-x64.zip"), arch: "arm64"))
    }
    func testIncorrectRepositoryArtifactAndProtocolAreRejected() {
        for url in ["http://github.com/tlolabs/yacht/releases/download/v2.2.0/YACHT-macos-arm64.zip", "https://github.com/tlolabs/other/releases/download/v2.2.0/YACHT-macos-arm64.zip", "https://github.com/tlolabs/yacht/releases/download/v2.1.0/YACHT-macos-arm64.zip"] {
            XCTAssertFalse(UpdateSafety.accepts(version: "2.2.0", installed: "2.1.1", url: URL(string: url), arch: "arm64"))
        }
    }
    @MainActor func testActiveWorkPreventsTerminationEvenWithoutAnUpdate() {
        let updater = ApplicationUpdater()
        updater.isBusy = { true }
        updater.confirmTermination = { XCTFail("Busy work must not be overridden by confirmation"); return true }
        XCTAssertFalse(updater.allowsTermination(hasWork: true))
        XCTAssertFalse(updater.allowsTermination(hasWork: false))
        updater.isBusy = { false }
        XCTAssertTrue(updater.allowsTermination(hasWork: true))
    }
    @MainActor func testPendingUpdatePreservesWorkUntilUserConfirms() {
        XCTAssertFalse(UpdateSafety.allowsTermination(busy: false, installationPending: true, hasWork: true, confirm: { false }))
        XCTAssertTrue(UpdateSafety.allowsTermination(busy: false, installationPending: true, hasWork: true, confirm: { true }))
        XCTAssertFalse(UpdateSafety.allowsTermination(busy: true, installationPending: true, hasWork: true, confirm: { XCTFail("Must not prompt while busy"); return true }))
        XCTAssertTrue(UpdateSafety.allowsTermination(busy: false, installationPending: true, hasWork: false, confirm: { XCTFail("No work to discard"); return false }))
    }
    private func accepts(_ version: String, installed: String = "2.1.1") -> Bool {
        UpdateSafety.accepts(version: version, installed: installed, url: URL(string: "https://github.com/tlolabs/yacht/releases/download/v\(version)/YACHT-macos-arm64.zip"), arch: "arm64")
    }
}
