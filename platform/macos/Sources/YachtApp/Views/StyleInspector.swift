import SwiftUI
import YachtCore

struct StyleInspector: View {
    @Bindable var workspace: Workspace
    @State private var selectedPreset = "builtin:Y-Default"
    @State private var presetName = ""
    @State private var confirmReplace = false
    @State private var confirmDelete = false
    private var builtInNames: [String] {
        let original = ["Y-Default", "N-Blue", "N-Orange", "N-Green", "N-Red", "N-Gray", "N-White"]
        let additional = PresetRepository.builtInPresets.keys
            .filter { !original.contains($0) && $0 != "Unstyled" }.sorted()
        return original + additional + ["Unstyled"]
    }
    private var customPresetName: String? {
        selectedPreset.hasPrefix("custom:") ? String(selectedPreset.dropFirst("custom:".count)) : nil
    }
    var body: some View {
        Form {
            Section("Presets") {
                Picker("Preset", selection: $selectedPreset) {
                    ForEach(builtInNames, id: \.self) { Text($0).tag("builtin:\($0)") }
                    ForEach(workspace.preferences.presets.keys.sorted(), id: \.self) { name in
                        Text(builtInNames.contains(name) ? "\(name) (Custom)" : name).tag("custom:\(name)")
                    }
                }
                .accessibilityIdentifier("presetPicker")
                .accessibilityLabel("Style preset")
                .help("Choose a built-in or custom table style preset")
                HStack {
                    Button("Load") { loadPreset() }
                        .accessibilityIdentifier("loadPreset")
                        .help("Apply the selected preset to the current table")
                    Button("Delete", role: .destructive) { confirmDelete = true }
                        .disabled(customPresetName == nil)
                        .help("Permanently delete the selected custom preset")
                }
                TextField("Save as", text: $presetName)
                    .accessibilityIdentifier("presetName")
                    .accessibilityLabel("Save as preset name")
                    .help("Enter a name to save current style settings")
                Button("Save Current") {
                    if workspace.preferences.presets[presetName.trimmingCharacters(in: .whitespacesAndNewlines)] != nil { confirmReplace = true }
                    else { savePreset() }
                }
                .disabled(presetName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                .help("Save the current style under the specified name")
                HStack {
                    Button("Reset Y-Default") { workspace.style = .init(); selectedPreset = "builtin:Y-Default" }
                        .help("Reset table styles to the Y-Default appearance")
                    Button("Reset Unstyled") { workspace.style = .unstyled; selectedPreset = "builtin:Unstyled" }
                        .help("Reset table styles to unstyled plain HTML")
                }
            }
            Section("Typography") {
                TextField("Font family", text: $workspace.style.fontFamily)
                    .accessibilityLabel("Font family")
                    .help("CSS font family (e.g. system-ui, Helvetica, monospace)")
                Stepper("Font size: \(workspace.style.fontSizePx) px", value: $workspace.style.fontSizePx, in: 1...100)
                    .accessibilityLabel("Font size")
                    .accessibilityValue("\(workspace.style.fontSizePx) pixels")
                    .help("Base font size for table contents in pixels")
                Stepper("Cell padding: \(workspace.style.cellPaddingPx) px", value: $workspace.style.cellPaddingPx, in: 0...100)
                    .accessibilityLabel("Cell padding")
                    .accessibilityValue("\(workspace.style.cellPaddingPx) pixels")
                    .help("Internal padding for table cells in pixels")
            }
            Section("Borders") {
                Picker("Border style", selection: $workspace.style.borderStyle) {
                    ForEach(["solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset", "outset"], id: \.self) { Text($0.capitalized).tag($0) }
                }
                .accessibilityLabel("Border style")
                .help("CSS border line style")
                Stepper("Border width: \(workspace.style.borderWidthPx) px", value: $workspace.style.borderWidthPx, in: 0...100)
                    .accessibilityLabel("Border width")
                    .accessibilityValue("\(workspace.style.borderWidthPx) pixels")
                    .help("Border thickness in pixels")
                ColorSetting(title: "Border color", value: $workspace.style.borderColor)
                Picker("Border collapse", selection: $workspace.style.borderCollapse) {
                    Text("Collapse").tag("collapse"); Text("Separate").tag("separate")
                }
                .accessibilityLabel("Border collapse")
                .help("Whether adjacent cell borders collapse into a single border or separate")
                Stepper("Border spacing: \(workspace.style.borderSpacingPx) px", value: $workspace.style.borderSpacingPx, in: 0...100)
                    .accessibilityLabel("Border spacing")
                    .accessibilityValue("\(workspace.style.borderSpacingPx) pixels")
                    .help("Spacing between cell borders when borders are separate")
            }
            Section("Table colors") {
                ColorSetting(title: "Header background", value: $workspace.style.headerBg)
                ColorSetting(title: "Header text color", value: $workspace.style.headerTextColor)
                ColorSetting(title: "Body background", value: $workspace.style.bodyBg)
                Toggle("Zebra striping", isOn: $workspace.style.zebraEnabled)
                    .help("Alternate row background colors for improved scanning")
                ColorSetting(title: "Zebra color", value: $workspace.style.zebraBg)
                Toggle("First column background", isOn: $workspace.style.firstColumnEnabled)
                    .accessibilityLabel("First column background")
                    .help("Use a separate background for the first column of table body cells")
                ColorSetting(title: "First column color", value: $workspace.style.firstColumnBg)
                Toggle("Hover highlight", isOn: $workspace.style.hoverEnabled)
                    .help("Highlight table row under cursor for tracking")
                ColorSetting(title: "Hover color", value: $workspace.style.hoverBg)
            }
            Section("HTML") {
                TextField("Table class", text: $workspace.style.tableClass)
                    .accessibilityLabel("Table CSS class")
                    .help("Optional CSS class name(s) added to the table element")
                Text("Numbers align right. Percent values stay left-aligned.").font(.caption).foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .confirmationDialog("Replace preset “\(presetName)” ?", isPresented: $confirmReplace) { Button("Replace", role: .destructive) { savePreset() } }
        .confirmationDialog("Delete preset “\(customPresetName ?? "")” ?", isPresented: $confirmDelete) {
            Button("Delete", role: .destructive) {
                guard let name = customPresetName else { return }
                do { try workspace.preferences.deletePreset(name); selectedPreset = "builtin:Y-Default" }
                catch { workspace.errorMessage = error.localizedDescription }
            }
        }
    }
    private func loadPreset() {
        if let name = customPresetName, let style = workspace.preferences.presets[name] { workspace.style = style }
        else if selectedPreset.hasPrefix("builtin:"), let style = PresetRepository.builtInPresets[String(selectedPreset.dropFirst("builtin:".count))] { workspace.style = style }
    }
    private func savePreset() {
        let name = presetName.trimmingCharacters(in: .whitespacesAndNewlines)
        do { try workspace.preferences.savePreset(name: name, style: workspace.style); selectedPreset = "custom:\(name)"; presetName = ""; workspace.status = "Saved preset “\(name)”" }
        catch { workspace.errorMessage = error.localizedDescription }
    }
}
