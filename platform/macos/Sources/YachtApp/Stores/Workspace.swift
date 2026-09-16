import SwiftUI
import Observation
import YachtCore

@MainActor @Observable
final class Workspace {
    var table: TableData? = .sample
    var sourceURL: URL?
    var style: StyleOptions { didSet { schedulePreview() } }
    var delimiter = CSVDelimiter.comma
    var preview: PreviewDocument?
    var validationMessage: String?
    var errorMessage: String?
    var status = "Built-in sample · Open a CSV to begin"
    var importing = false
    var rendering = false
    var working = false
    var showImporter = false
    var batchImport = false
    var showExporter = false
    var exportDocument = HTMLFile()
    var preparedExport: URL?
    var lastExport: URL?
    var batchURLs: [URL] = []
    var showBatch = false
    var batchOverwrite = false
    var batchProgress = 0
    var batchResults: [String] = []
    var section = "preview"
    @ObservationIgnored private var importTask: Task<Void, Never>?
    @ObservationIgnored private var previewTask: Task<Void, Never>?
    @ObservationIgnored private var operationTask: Task<Void, Never>?
    @ObservationIgnored private var generation = UUID()
    let preferences: Preferences

    init(preferences: Preferences = .shared) {
        self.preferences = preferences
        style = preferences.initialStyle
        errorMessage = preferences.problem
        schedulePreview()
    }
    var canExport: Bool { table != nil && !importing && !working && validationMessage == nil }
    var title: String { sourceURL?.lastPathComponent ?? "Sample table" }
    func chooseFile(batch: Bool = false) { batchImport = batch; showImporter = true }
    func receive(_ urls: [URL], batch: Bool = false) {
        guard !urls.isEmpty else { return }
        if urls.count > 1 || batch { batchURLs = urls; batchResults = []; batchOverwrite = false; showBatch = true }
        else { load(urls[0]) }
    }
    func load(_ url: URL, inferDelimiter: Bool = true) {
        importTask?.cancel(); previewTask?.cancel()
        generation = UUID()
        let request = generation
        sourceURL = url; table = nil; preview = nil; importing = true; rendering = false
        validationMessage = nil
        status = "Reading \(url.lastPathComponent)…"
        let selectedDelimiter = inferDelimiter && url.pathExtension.lowercased() == "tsv" ? CSVDelimiter.tab : delimiter
        delimiter = selectedDelimiter
        importTask = Task {
            do {
                let value = try await Background.run {
                    let granted = url.startAccessingSecurityScopedResource()
                    defer { if granted { url.stopAccessingSecurityScopedResource() } }
                    return try CSVParser.read(url, delimiter: selectedDelimiter)
                }
                guard !Task.isCancelled, request == generation else { return }
                table = value; importing = false; preferences.addRecent(url)
                status = "\(value.rowCount.formatted()) rows · \(value.header.count.formatted()) columns"
                schedulePreview()
            } catch is CancellationError {} catch {
                guard request == generation else { return }
                importing = false; status = "Import failed"; errorMessage = error.localizedDescription
            }
        }
    }
    func refresh() {
        if let sourceURL { load(sourceURL, inferDelimiter: false) } else { schedulePreview() }
    }
    func useSample() {
        importTask?.cancel(); generation = UUID(); importing = false
        sourceURL = nil; table = .sample; status = "Built-in sample · 9 rows · 3 columns"; schedulePreview()
    }
    func schedulePreview() {
        previewTask?.cancel()
        do { try style.validate(); validationMessage = nil }
        catch { validationMessage = error.localizedDescription; preview = nil; rendering = false; return }
        guard let table else { return }
        let options = style, limit = preferences.previewRows
        rendering = true
        previewTask = Task {
            do {
                try await Task.sleep(for: .milliseconds(180))
                let result = try await Background.run { try PreviewDocument(table: table, style: options, limit: limit) }
                guard !Task.isCancelled else { return }
                preview = result; rendering = false; preferences.remember(options)
            } catch is CancellationError {} catch {
                guard !Task.isCancelled else { return }
                rendering = false; validationMessage = error.localizedDescription
            }
        }
    }
    func copyHTML() {
        guard let table, canExport else { return }
        let options = style; working = true; status = "Preparing HTML for clipboard…"
        operationTask = Task {
            do {
                let html = try await Background.run { try HTMLGenerator.document(table, style: options) }
                try Task.checkCancellation()
                DesktopActions.copy(html); status = "Copied complete HTML document"
            } catch is CancellationError { status = "Copy cancelled" }
            catch { errorMessage = error.localizedDescription }
            working = false
        }
    }
    func prepareExport() {
        guard let table, canExport else { return }
        let options = style; working = true; status = "Preparing export…"
        operationTask = Task {
            do {
                let url = FileManager.default.temporaryDirectory.appendingPathComponent("yacht-export-\(UUID().uuidString).html")
                try await Background.run { try FileService.export(table, style: options, to: url) }
                if Task.isCancelled { try? FileManager.default.removeItem(at: url); throw CancellationError() }
                preparedExport = url; exportDocument = HTMLFile(fileURL: url)
                showExporter = true; status = "Choose where to save HTML"
            } catch is CancellationError { status = "Export cancelled" }
            catch { errorMessage = error.localizedDescription }
            working = false
        }
    }
    func finishExport(_ result: Result<URL, Error>) {
        switch result {
        case .success(let url): lastExport = url; status = "Exported \(url.lastPathComponent)"
        case .failure(let error): errorMessage = error.localizedDescription
        }
        cleanPreparedExport()
    }
    func cleanPreparedExport() {
        if let preparedExport { try? FileManager.default.removeItem(at: preparedExport) }
        preparedExport = nil
    }
    func cancel() {
        importTask?.cancel(); operationTask?.cancel(); importing = false
        generation = UUID(); status = "Cancelled"
    }
    func runBatch() {
        guard !working else { return }
        let urls = batchURLs, options = style, separator = delimiter, overwrite = batchOverwrite
        working = true; batchProgress = 0; batchResults = []
        operationTask = Task {
            for url in urls {
                if Task.isCancelled { break }
                do {
                    let output = FileService.defaultOutput(for: url)
                    try await Background.run {
                        let granted = url.startAccessingSecurityScopedResource()
                        defer { if granted { url.stopAccessingSecurityScopedResource() } }
                        _ = try FileService.batchItem(url, style: options, delimiter: separator, overwrite: overwrite)
                    }
                    batchResults.append("Saved \(output.lastPathComponent)"); lastExport = output
                } catch is CancellationError { break }
                catch { batchResults.append("\(url.lastPathComponent): \(error.localizedDescription)") }
                batchProgress += 1
            }
            working = false
            status = "Batch processed \(batchProgress) of \(urls.count) files"
        }
    }
}
