import SwiftUI
import UniformTypeIdentifiers

/// SwiftUI owns the Save dialog and replacement confirmation. The prepared
/// temporary file keeps HTML generation and disk writes off the main actor.
struct HTMLFile: FileDocument {
    static var readableContentTypes: [UTType] { [.html] }
    var fileURL: URL?
    init(fileURL: URL? = nil) { self.fileURL = fileURL }
    init(configuration: ReadConfiguration) throws { fileURL = nil }
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        guard let fileURL else { throw CocoaError(.fileReadNoSuchFile) }
        return try FileWrapper(url: fileURL, options: .immediate)
    }
}
