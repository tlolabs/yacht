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
                HStack {
                    Button("Load") { loadPreset() }.accessibilityIdentifier("loadPreset")
                    Button("Delete", role: .destructive) { confirmDelete = true }
                        .disabled(workspace.preferences.presets[selectedPreset] == nil)
                }
                TextField("Save as", text: $presetName).accessibilityIdentifier("presetName")
                Button("Save Current") {
                    if workspace.preferences.presets[presetName.trimmingCharacters(in: .whitespacesAndNewlines)] != nil { confirmReplace = true }
                    else { savePreset() }
                }.disabled(presetName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                HStack {
                    Button("Reset Styled") { workspace.style = .init(); selectedPreset = "Default (Styled)" }
                    Button("Reset Unstyled") { workspace.style = .unstyled; selectedPreset = "Unstyled" }
                }
            }
            Section("Typography") {
                TextField("Font family", text: $workspace.style.fontFamily)
                Stepper("Font size: \(workspace.style.fontSizePx) px", value: $workspace.style.fontSizePx, in: 1...100)
                Stepper("Cell padding: \(workspace.style.cellPaddingPx) px", value: $workspace.style.cellPaddingPx, in: 0...100)
            }
            Section("Borders") {
                Picker("Border style", selection: $workspace.style.borderStyle) {
                    ForEach(["solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset", "outset"], id: \.self) { Text($0.capitalized).tag($0) }
                }
                Stepper("Border width: \(workspace.style.borderWidthPx) px", value: $workspace.style.borderWidthPx, in: 0...100)
                ColorSetting(title: "Border color", value: $workspace.style.borderColor)
                Picker("Border collapse", selection: $workspace.style.borderCollapse) {
                    Text("Collapse").tag("collapse"); Text("Separate").tag("separate")
                }
                Stepper("Border spacing: \(workspace.style.borderSpacingPx) px", value: $workspace.style.borderSpacingPx, in: 0...100)
            }
            Section("Table colors") {
                ColorSetting(title: "Header background", value: $workspace.style.headerBg)
                ColorSetting(title: "Header text color", value: $workspace.style.headerTextColor)
                ColorSetting(title: "Body background", value: $workspace.style.bodyBg)
                Toggle("Zebra striping", isOn: $workspace.style.zebraEnabled)
                ColorSetting(title: "Zebra color", value: $workspace.style.zebraBg)
                Toggle("Hover highlight", isOn: $workspace.style.hoverEnabled)
                ColorSetting(title: "Hover color", value: $workspace.style.hoverBg)
            }
            Section("HTML") {
                TextField("Table class", text: $workspace.style.tableClass)
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
