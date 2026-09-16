import Foundation
/// Native editable projection of the Rust-owned portable style schema.
public struct StyleOptions: Codable, Equatable, Sendable {
    private var values: [String: JSONValue]
    private static let defaults: [String: JSONValue] = try! RustBridge.call("defaults", cancellable: false)
    private static let bare: [String: JSONValue] = try! RustBridge.call("unstyled", cancellable: false)
    public init() { values = Self.defaults }
    private init(_ values: [String: JSONValue]) { self.values = values }
    public static var unstyled: Self { Self(Self.bare) }
    public var isUnstyled: Bool { try! RustBridge.call("is_unstyled", ["style": try! RustBridge.object(self)], cancellable: false) }
    public func validate() throws { try RustBridge.perform("validate_style", ["style": try RustBridge.object(self)]) }
    public static func isSafeColor(_ value: String) -> Bool { try! RustBridge.call("safe_color", ["text": value], cancellable: false) }
    public init(from decoder: Decoder) throws {
        let raw = try [String: JSONValue](from: decoder)
        values = try RustBridge.call("normalize_style", ["style": try RustBridge.object(raw)], cancellable: false)
    }
    public func encode(to encoder: Encoder) throws { try values.encode(to: encoder) }
    public var tableClass: String {
        get { guard case .string(let value) = values["table_class"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["table_class"] = .string(newValue) }
    }
    public var fontFamily: String {
        get { guard case .string(let value) = values["font_family"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["font_family"] = .string(newValue) }
    }
    public var fontSizePx: Int {
        get { guard case .integer(let value) = values["font_size_px"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["font_size_px"] = .integer(newValue) }
    }
    public var cellPaddingPx: Int {
        get { guard case .integer(let value) = values["cell_padding_px"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["cell_padding_px"] = .integer(newValue) }
    }
    public var borderWidthPx: Int {
        get { guard case .integer(let value) = values["border_width_px"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["border_width_px"] = .integer(newValue) }
    }
    public var borderStyle: String {
        get { guard case .string(let value) = values["border_style"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["border_style"] = .string(newValue) }
    }
    public var borderColor: String {
        get { guard case .string(let value) = values["border_color"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["border_color"] = .string(newValue) }
    }
    public var headerBg: String {
        get { guard case .string(let value) = values["header_bg"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["header_bg"] = .string(newValue) }
    }
    public var headerTextColor: String {
        get { guard case .string(let value) = values["header_text_color"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["header_text_color"] = .string(newValue) }
    }
    public var bodyBg: String {
        get { guard case .string(let value) = values["body_bg"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["body_bg"] = .string(newValue) }
    }
    public var zebraEnabled: Bool {
        get { guard case .boolean(let value) = values["zebra_enabled"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["zebra_enabled"] = .boolean(newValue) }
    }
    public var zebraBg: String {
        get { guard case .string(let value) = values["zebra_bg"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["zebra_bg"] = .string(newValue) }
    }
    public var hoverEnabled: Bool {
        get { guard case .boolean(let value) = values["hover_enabled"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["hover_enabled"] = .boolean(newValue) }
    }
    public var hoverBg: String {
        get { guard case .string(let value) = values["hover_bg"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["hover_bg"] = .string(newValue) }
    }
    public var borderCollapse: String {
        get { guard case .string(let value) = values["border_collapse"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["border_collapse"] = .string(newValue) }
    }
    public var borderSpacingPx: Int {
        get { guard case .integer(let value) = values["border_spacing_px"] else { preconditionFailure("Rust style schema mismatch") }; return value }
        set { values["border_spacing_px"] = .integer(newValue) }
    }
}
