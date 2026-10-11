import SwiftUI

struct BatchView: View {
    @Bindable var workspace: Workspace
    @State private var confirmOverwrite = false
    private var colorScheme: ColorScheme? {
        workspace.preferences.appearance == "Dark" ? .dark : workspace.preferences.appearance == "Light" ? .light : nil
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Batch Convert CSVs").font(.title2.bold())
            Text("Save an HTML file beside each of the \(workspace.batchURLs.count) selected inputs, using the current style.")
                .foregroundStyle(.secondary)
            List {
                if workspace.batchResults.isEmpty {
                    ForEach(workspace.batchURLs, id: \.self) { Text($0.path).textSelection(.enabled) }
                } else {
                    ForEach(Array(workspace.batchResults.enumerated()), id: \.offset) { _, text in Text(text).textSelection(.enabled) }
                }
            }
            .accessibilityLabel("Selected files for batch conversion")
            .frame(minHeight: 180)

            Toggle("Replace existing HTML files", isOn: $workspace.batchOverwrite)
                .disabled(workspace.working)
                .accessibilityLabel("Replace existing HTML files")
                .help("Overwrite existing HTML files with matching names in target directories")

            if workspace.working {
                ProgressView(value: Double(workspace.batchProgress), total: Double(max(1, workspace.batchURLs.count)))
                    .accessibilityLabel("Batch conversion progress")
                    .accessibilityValue("\(workspace.batchProgress) of \(workspace.batchURLs.count) files processed")
            }
            HStack {
                Text("\(workspace.batchProgress) of \(workspace.batchURLs.count) processed").font(.caption)
                Spacer()
                if workspace.working {
                    Button("Cancel") { workspace.cancel() }
                        .help("Cancel ongoing batch conversion")
                        .accessibilityLabel("Cancel ongoing batch conversion")
                } else {
                    Button("Done") { workspace.showBatch = false }
                        .keyboardShortcut(.cancelAction)
                        .help("Close batch conversion dialog")
                    Button("Convert") {
                        if workspace.batchOverwrite { confirmOverwrite = true } else { workspace.runBatch() }
                    }
                    .keyboardShortcut(.defaultAction)
                    .help("Convert all selected files to HTML")
                }
            }
        }
        .padding(24)
        .frame(width: 620, height: 430)
        .preferredColorScheme(colorScheme)
        .interactiveDismissDisabled(workspace.working)
        .confirmationDialog("Replace existing HTML beside these CSV files?", isPresented: $confirmOverwrite) {
            Button("Replace and Convert", role: .destructive) { workspace.runBatch() }
        } message: { Text("Existing HTML files with matching names will be replaced. CSV inputs are preserved.") }
    }
}
