import SwiftUI
import YachtCore

struct StyleInspector: View {
    @Bindable var workspace: Workspace
    @State private var selectedPreset = "Default (Styled)"
    @State private var presetName = ""
    @State private var confirmReplace = false
    @State private var confirmDelete = false
    var body: some View {
        Form {
            Section("Presets") {
                Picker("Preset", selection: $selectedPreset) {
                    Text("Default (Styled)").tag("Default (Styled)")
                    Text("Unstyled").tag("Unstyled")
                    ForEach(workspace.preferences.presets.keys.sorted(), id: \.self) { Text($0).tag($0) }
                }
                .accessibilityIdentifier("presetPicker")
                .accessibilityLabel("Style preset")
                .help("Choose a built-in or custom table style preset")
                HStack {
                    Button("Load") { loadPreset() }
                        .accessibilityIdentifier("loadPreset")
                        .help("Apply the selected preset to the current table")
                    Button("Delete", role: .destructive) { confirmDelete = true }
                        .disabled(workspace.preferences.presets[selectedPreset] == nil)
                        .help("Permanently delete the selected custom preset")
                }
                TextField("Save as", text: $presetName)
                    .accessibilityIdentifier("presetName")
                    .accessibilityLabel("Preset name")
                    .help("Enter a name to save current style settings")
                Button("Save Current") {
                    if workspace.preferences.presets[presetName.trimmingCharacters(in: .whitespacesAndNewlines)] != nil { confirmReplace = true }
                    else { savePreset() }
                }
                .disabled(presetName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                .help("Save the current style under the specified name")
                HStack {
                    Button("Reset Styled") { workspace.style = .init(); selectedPreset = "Default (Styled)" }
                        .help("Reset table styles to default styled appearance")
                    Button("Reset Unstyled") { workspace.style = .unstyled; selectedPreset = "Unstyled" }
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
        .confirmationDialog("Delete preset “\(selectedPreset)” ?", isPresented: $confirmDelete) {
            Button("Delete", role: .destructive) {
                do { try workspace.preferences.deletePreset(selectedPreset); selectedPreset = "Default (Styled)" }
                catch { workspace.errorMessage = error.localizedDescription }
            }
        }
    }
    private func loadPreset() {
        if selectedPreset == "Default (Styled)" { workspace.style = .init() }
        else if selectedPreset == "Unstyled" { workspace.style = .unstyled }
        else if let style = workspace.preferences.presets[selectedPreset] { workspace.style = style }
    }
    private func savePreset() {
        let name = presetName.trimmingCharacters(in: .whitespacesAndNewlines)
        do { try workspace.preferences.savePreset(name: name, style: workspace.style); selectedPreset = name; presetName = ""; workspace.status = "Saved preset “\(name)”" }
        catch { workspace.errorMessage = error.localizedDescription }
    }
}
