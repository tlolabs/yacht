import SwiftUI
import UniformTypeIdentifiers
import YachtCore

struct ContentView: View {
    @Bindable var workspace: Workspace
    @SceneStorage("showStyles") private var showStyles = true
    @State private var dropTargeted = false
    var body: some View {
        HSplitView {
            if showStyles {
                StyleInspector(workspace: workspace).frame(minWidth: 290, idealWidth: 320, maxWidth: 390)
                    .accessibilityLabel("Table style controls")
            }
            VStack(spacing: 0) {
                header
                Divider()
                previewContent
                Divider()
                footer
            }.frame(minWidth: 450, maxWidth: .infinity, maxHeight: .infinity)
        }
        .frame(minWidth: 820, minHeight: 600)
        .navigationTitle("Y.A.C.H.T.")
        .navigationSubtitle(workspace.title)
        .toolbar { toolbar }
        .fileImporter(isPresented: $workspace.showImporter, allowedContentTypes: [.commaSeparatedText, .tabSeparatedText, .plainText], allowsMultipleSelection: true) { result in
            switch result {
            case .success(let urls): workspace.receive(urls, batch: workspace.batchImport)
            case .failure(let error): workspace.errorMessage = error.localizedDescription
            }
        }
        .fileExporter(isPresented: $workspace.showExporter, document: workspace.exportDocument, contentTypes: [.html],
            defaultFilename: workspace.sourceURL?.deletingPathExtension().lastPathComponent ?? "Y.A.C.H.T. Table", onCompletion: { workspace.finishExport($0) }, onCancellation: { workspace.cleanPreparedExport(); workspace.status = "Export cancelled" })
        .sheet(isPresented: $workspace.showBatch) { BatchView(workspace: workspace) }
        .alert("Y.A.C.H.T.", isPresented: Binding(get: { workspace.errorMessage != nil }, set: { if !$0 { workspace.errorMessage = nil } })) {
            Button("OK", role: .cancel) { workspace.errorMessage = nil }
        } message: { Text(workspace.errorMessage ?? "") }
        .dropDestination(for: URL.self) { urls, _ in
            guard !workspace.working else { return false }; workspace.receive(urls); return !urls.isEmpty
        } isTargeted: { dropTargeted = $0 }
        .overlay { if dropTargeted { RoundedRectangle(cornerRadius: 8).stroke(.tint, lineWidth: 3).padding(4).allowsHitTesting(false) } }
        .onOpenURL { workspace.receive([$0]) }
        .onChange(of: workspace.preferences.previewRows) { workspace.schedulePreview() }
    }
    private var header: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Label(workspace.title, systemImage: "tablecells").font(.headline).lineLimit(1)
                Spacer()
                Picker("View", selection: $workspace.section) {
                    Text("Table Preview").tag("preview")
                    Text("HTML Source").tag("source")
                }.pickerStyle(.segmented).labelsHidden().frame(width: 235).accessibilityIdentifier("viewSelector")
            }
            HStack {
                Picker("Delimiter", selection: $workspace.delimiter) {
                    ForEach(CSVDelimiter.allCases, id: \.self) { Text($0.label).tag($0) }
                }.frame(width: 230)
                .onChange(of: workspace.delimiter) { if let url = workspace.sourceURL { workspace.load(url, inferDelimiter: false) } }
                Spacer()
                if let table = workspace.table { Text("\(table.rows.count.formatted()) rows · \(table.header.count.formatted()) columns").font(.caption).foregroundStyle(.secondary) }
            }
            if let warning = workspace.table?.warnings.joined(separator: " "), !warning.isEmpty {
                Label(warning, systemImage: "exclamationmark.triangle").font(.caption).foregroundStyle(.orange).textSelection(.enabled)
            }
        }.padding(16)
    }
    @ViewBuilder private var previewContent: some View {
        if let message = workspace.validationMessage {
            ContentUnavailableView("Check Table Settings", systemImage: "exclamationmark.triangle", description: Text(message))
        } else if workspace.importing {
            VStack(spacing: 12) { ProgressView(); Text("Reading CSV…"); Button("Cancel") { workspace.cancel() } }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if let preview = workspace.preview {
            VStack(spacing: 0) {
                if workspace.section == "source" {
                    ScrollView([.horizontal, .vertical]) {
                        Text(preview.source).font(.system(size: 12, design: .monospaced)).textSelection(.enabled)
                            .fixedSize(horizontal: true, vertical: true).padding(16).accessibilityIdentifier("htmlSource")
                    }.frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                    if preview.sourceTruncated { note("Source shows the first 1 MB. Copy HTML and Export include the complete document.") }
                } else if preview.previewUnavailable {
                    ContentUnavailableView("Cells Too Large to Preview", systemImage: "tablecells", description: Text("The table is ready to export. Preview is limited to keep the app responsive."))
                } else {
                    HTMLPreview(html: preview.html)
                    if preview.rowCount < (workspace.table?.rows.count ?? 0) { note("Preview shows the first \(preview.rowCount) rows. Copy HTML and Export include every row.") }
                }
            }.overlay(alignment: .topTrailing) { if workspace.rendering { ProgressView().controlSize(.small).padding(12).accessibilityLabel("Updating preview") } }
        } else {
            ContentUnavailableView("Open a CSV", systemImage: "tablecells", description: Text("Open or drop a CSV file to preview and export its table."))
        }
    }
    private func note(_ text: String) -> some View { Text(text).font(.caption).foregroundStyle(.secondary).padding(8).frame(maxWidth: .infinity) }
    private var footer: some View {
        HStack {
            if workspace.working { ProgressView().controlSize(.small) }
            Text(workspace.status).font(.caption).foregroundStyle(.secondary).lineLimit(2).accessibilityIdentifier("status")
            Spacer()
            if workspace.working { Button("Cancel") { workspace.cancel() } }
            if let url = workspace.lastExport {
                Button("Reveal in Finder") { DesktopActions.reveal(url) }
                Button("Open in Browser") { DesktopActions.open(url) }
            }
        }.padding(12)
    }
    @ToolbarContentBuilder private var toolbar: some ToolbarContent {
        ToolbarItem(placement: .navigation) {
            Button { showStyles.toggle() } label: { Label("Toggle Style Controls", systemImage: "sidebar.left") }
        }
        ToolbarItemGroup {
            Button { workspace.chooseFile() } label: { Label("Open CSV", systemImage: "folder") }.accessibilityIdentifier("openCSV")
            Button { workspace.chooseFile(batch: true) } label: { Label("Batch Convert CSVs", systemImage: "square.stack.3d.up") }.disabled(workspace.working).accessibilityIdentifier("batchConvert")
            Button { workspace.refresh() } label: { Label("Refresh Preview", systemImage: "arrow.clockwise") }.disabled(workspace.importing)
            Button { workspace.copyHTML() } label: { Label("Copy HTML Code", systemImage: "doc.on.doc") }.disabled(!workspace.canExport).accessibilityIdentifier("copyHTML")
            Button { workspace.prepareExport() } label: { Label("Export HTML", systemImage: "square.and.arrow.up") }.disabled(!workspace.canExport).accessibilityIdentifier("exportHTML")
        }
    }
}
