import SwiftUI

struct SettingsView: View {
    @Bindable var preferences: Preferences
    let updater: ApplicationUpdater
    @State private var automaticChecks = false
    @State private var recentCleared = false
    private var colorScheme: ColorScheme? {
        preferences.appearance == "Dark" ? .dark : preferences.appearance == "Light" ? .light : nil
    }
    var body: some View {
        Form {
            Picker("Appearance", selection: $preferences.appearance) {
                Text("System").tag("System")
                Text("Light").tag("Light")
                Text("Dark").tag("Dark")
            }
            .accessibilityLabel("Appearance")
            .accessibilityIdentifier("appearance")
            .help("Choose System, Light, or Dark appearance")

            Toggle("Remember last-used table style", isOn: $preferences.rememberStyle)
                .accessibilityLabel("Remember last-used table style")
                .accessibilityIdentifier("rememberStyle")
                .help("Save current style settings on exit and restore them on next launch")

            Picker("Maximum preview rows", selection: $preferences.previewRows) {
                Text("50").tag(50); Text("200").tag(200); Text("1,000").tag(1000)
            }
            .accessibilityLabel("Maximum preview rows")
            .accessibilityIdentifier("previewRows")
            .help("Limit row count in the table preview for performance; complete exports include all rows")

            Text("Large previews also have a size limit. Exports and copied HTML always include all rows.")
                .font(.caption).foregroundStyle(.secondary)

            Toggle("Automatically check for updates", isOn: $automaticChecks)
                .disabled(!updater.isConfigured)
                .accessibilityIdentifier("automaticUpdates")
                .help("Check for software updates when launching YACHT")
                .onAppear { automaticChecks = updater.automaticallyChecks }
                .onChange(of: automaticChecks) { _, value in updater.automaticallyChecks = value }

            if !updater.isConfigured {
                Text("Automatic updates are not configured in this build.").font(.caption).foregroundStyle(.secondary)
            }

            Button("Clear Recent Files") {
                preferences.clearRecent()
                recentCleared = true
            }
            .disabled(preferences.recentFiles.isEmpty)
            .help("Clear the list of recently opened files in the File menu")

            if recentCleared {
                Text("Recent files cleared.").font(.caption).foregroundStyle(.secondary)
            }

            Text("YACHT — Yet Another CSV HTML Translator\nNative macOS edition · GPLv3")
                .font(.caption).foregroundStyle(.secondary)
        }
        .formStyle(.grouped)
        .padding()
        .frame(width: 440)
        .preferredColorScheme(colorScheme)
    }
}
