import Foundation

/// Uses the Python JSON schema so existing named styles remain portable.
public struct PresetRepository: Sendable {
    public let url: URL
    public init(url: URL) { self.url = url }
    public func load() throws -> [String: StyleOptions] {
        guard FileManager.default.fileExists(atPath: url.path) else { return [:] }
        return try JSONDecoder().decode([String: StyleOptions].self, from: Data(contentsOf: url))
    }
    public func save(_ presets: [String: StyleOptions]) throws {
        for (name, style) in presets {
            guard !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
                  !["Default (Styled)", "Unstyled"].contains(name) else { throw YachtError.invalid("Choose a nonempty name other than the built-in preset names.") }
            try style.validate()
        }
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        try encoder.encode(presets).write(to: url, options: .atomic)
    }
}
