import SwiftUI

struct SettingsView: View {
    @Bindable var preferences: Preferences
    var body: some View {
        Form {
            Toggle("Remember last-used table style", isOn: $preferences.rememberStyle)
            Picker("Maximum preview rows", selection: $preferences.previewRows) {
                Text("50").tag(50); Text("200").tag(200); Text("1,000").tag(1000)
            }
            Text("Large previews also have a size limit. Exports and copied HTML always include all rows.")
                .font(.caption).foregroundStyle(.secondary)
            Button("Clear Recent Files") { preferences.clearRecent() }
            Text("Y.A.C.H.T. — Yet Another CSV HTML Translator\nNative macOS edition · GPLv3")
                .font(.caption).foregroundStyle(.secondary)
        }.formStyle(.grouped).padding().frame(width: 440)
    }
}
