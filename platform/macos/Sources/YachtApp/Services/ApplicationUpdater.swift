import AppKit
import Sparkle

/// Sparkle owns scheduling, authenticated downloads, installation and relaunch.
@MainActor
final class ApplicationUpdater: NSObject, SPUUpdaterDelegate {
    private var controller: SPUStandardUpdaterController?
    private var installationPending = false
    var confirmTermination: @MainActor () -> Bool = ApplicationUpdater.confirmRestart
    var isConfigured: Bool { controller != nil }
    var isBusy: () -> Bool = { false }
    func start() {
        guard controller == nil,
              let key = Bundle.main.object(forInfoDictionaryKey: "SUPublicEDKey") as? String,
              Data(base64Encoded: key)?.count == 32 else { return }
        controller = SPUStandardUpdaterController(startingUpdater: false, updaterDelegate: self, userDriverDelegate: nil)
        controller?.updater.userAgentString = "TLO-Updater/1"
        controller?.startUpdater()
    }
    func check() {
        start()
        guard let controller else {
            let alert = NSAlert()
            alert.messageText = "Updates are not configured in this build"
            alert.informativeText = "Download official releases from github.com/tlolabs/yacht/releases."
            alert.addButton(withTitle: "OK")
            alert.addButton(withTitle: "Open Releases")
            if alert.runModal() == .alertSecondButtonReturn {
                NSWorkspace.shared.open(URL(string: "https://github.com/tlolabs/yacht/releases")!)
            }
            return
        }
        controller.checkForUpdates(nil)
    }
    var automaticallyChecks: Bool {
        get { controller?.updater.automaticallyChecksForUpdates ?? false }
        set { controller?.updater.automaticallyChecksForUpdates = newValue }
    }
    func updater(_ updater: SPUUpdater, mayPerform updateCheck: SPUUpdateCheck) throws {
        if isBusy() { throw NSError(domain: "YACHT.Updates", code: 1, userInfo: [NSLocalizedDescriptionKey: "Finish the current conversion before checking for updates."]) }
    }
    func updater(_ updater: SPUUpdater, shouldProceedWithUpdate item: SUAppcastItem, updateCheck: SPUUpdateCheck) throws {
        #if arch(arm64)
        let arch = "arm64"
        #else
        let arch = "x64"
        #endif
        guard let installed = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String,
              UpdateSafety.accepts(version: item.versionString, installed: installed,
                                   url: item.fileURL, arch: arch), item.channel == nil else {
            throw NSError(domain: "YACHT.Updates", code: 2, userInfo: [NSLocalizedDescriptionKey: "The update is not a compatible stable YACHT release."])
        }
    }
    func updater(_ updater: SPUUpdater, willInstallUpdate item: SUAppcastItem) {
        installationPending = true
    }
    func updater(_ updater: SPUUpdater, didAbortWithError error: Error) {
        installationPending = false
    }
    /// Sparkle requests normal application termination. Enforce this at the
    /// application delegate, including install-on-quit and relaunch retries.
    func allowsTermination(hasWork: Bool) -> Bool {
        UpdateSafety.allowsTermination(busy: isBusy(), installationPending: installationPending,
                                       hasWork: hasWork, confirm: confirmTermination)
    }
    private static func confirmRestart() -> Bool {
        let alert = NSAlert()
        alert.messageText = "Restart YACHT to install the update?"
        alert.informativeText = "Export any table you want to keep before restarting. Choose Later to continue working."
        alert.addButton(withTitle: "Later")
        alert.addButton(withTitle: "Restart Now")
        return alert.runModal() == .alertSecondButtonReturn
    }
}

/// Additional feed policy; Sparkle still performs signature validation and installation.
enum UpdateSafety {
    @MainActor static func allowsTermination(busy: Bool, installationPending: Bool, hasWork: Bool, confirm: @MainActor () -> Bool) -> Bool {
        guard !busy else { return false }
        return !installationPending || !hasWork || confirm()
    }
    static func accepts(version: String, installed: String, url: URL?, arch: String) -> Bool {
        func components(_ value: String) -> [UInt64]? {
            guard value.range(of: #"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$"#, options: .regularExpression) != nil else { return nil }
            let parts = value.split(separator: ".").compactMap { UInt64($0) }
            return parts.count == 3 ? parts : nil
        }
        guard ["arm64", "x64"].contains(arch), let new = components(version),
              let old = components(installed), old.lexicographicallyPrecedes(new) else { return false }
        return url?.absoluteString == "https://github.com/tlolabs/yacht/releases/download/v\(version)/YACHT-macos-\(arch).zip"
    }
}
