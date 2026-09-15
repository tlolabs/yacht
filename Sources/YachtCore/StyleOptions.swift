import Foundation

public struct StyleOptions: Codable, Equatable, Sendable {
    public var tableClass = "table table-bordered table-hover table-condensed"
    public var fontFamily = "\"Helvetica Neue\", Helvetica, Arial, sans-serif"
    public var fontSizePx = 14
    public var cellPaddingPx = 8
    public var borderWidthPx = 1
    public var borderStyle = "solid"
    public var borderColor = "#cccccc"
    public var headerBg = "#f5f5f5"
    public var headerTextColor = "#222222"
    public var bodyBg = "#ffffff"
    public var zebraEnabled = true
    public var zebraBg = "#fbfbfb"
    public var hoverEnabled = true
    public var hoverBg = "#f2f8ff"
    public var borderCollapse = "collapse"
    public var borderSpacingPx = 0

    public init() {}
    public static var unstyled: Self {
        var s = Self()
        s.tableClass = ""; s.fontFamily = ""; s.fontSizePx = 16
        s.cellPaddingPx = 0; s.borderWidthPx = 0; s.borderStyle = "none"
        s.borderColor = "#000000"; s.headerBg = "transparent"; s.headerTextColor = "#000000"
        s.bodyBg = "transparent"; s.zebraEnabled = false; s.zebraBg = "transparent"
        s.hoverEnabled = false; s.hoverBg = "transparent"; s.borderCollapse = "separate"
        return s
    }
    public var isUnstyled: Bool { self == .unstyled }

    public func validate() throws {
        for (name, value, minimum) in [("Font size", fontSizePx, 1), ("Cell padding", cellPaddingPx, 0),
            ("Border width", borderWidthPx, 0), ("Border spacing", borderSpacingPx, 0)] {
            guard (minimum...10_000).contains(value) else { throw YachtError.invalid("\(name) must be between \(minimum) and 10000 pixels.") }
        }
        guard ["solid", "dashed", "dotted", "double", "none", "hidden", "groove", "ridge", "inset", "outset"].contains(borderStyle),
              ["collapse", "separate"].contains(borderCollapse) else { throw YachtError.invalid("Choose a valid border style and collapse mode.") }
        // A font-family list, never an arbitrary CSS declaration. Restrict punctuation
        // and require matched quotes; this prevents stylesheet breakout and url().
        let fontPattern = #"^\s*(?:[\p{L}\p{N}_ -]+|"[\p{L}\p{N}_ ,.-]+"|'[\p{L}\p{N}_ ,.-]+')(?:\s*,\s*(?:[\p{L}\p{N}_ -]+|"[\p{L}\p{N}_ ,.-]+"|'[\p{L}\p{N}_ ,.-]+'))*\s*$"#
        guard fontFamily.isEmpty || fontFamily.range(of: fontPattern, options: .regularExpression) != nil else {
            throw YachtError.invalid("Enter a comma-separated list of font names, for example Helvetica, Arial, sans-serif.")
        }
        for color in [borderColor, headerBg, headerTextColor, bodyBg, zebraBg, hoverBg] {
            guard Self.isSafeColor(color) else { throw YachtError.invalid("Invalid CSS color: \(color). Use a hex color, CSS color name, rgb(), or hsl().") }
        }
    }

    public static func isSafeColor(_ value: String) -> Bool {
        let pattern = #"^(?:#[0-9a-fA-F]{3}|#[0-9a-fA-F]{4}|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8}|[a-zA-Z]+|(?:rgb|hsl)a?\([0-9.,% /+\-]+\))$"#
        return value.range(of: pattern, options: .regularExpression) != nil
    }

    enum CodingKeys: String, CodingKey {
        case tableClass = "table_class", fontFamily = "font_family", fontSizePx = "font_size_px"
        case cellPaddingPx = "cell_padding_px", borderWidthPx = "border_width_px", borderStyle = "border_style"
        case borderColor = "border_color", headerBg = "header_bg", headerTextColor = "header_text_color"
        case bodyBg = "body_bg", zebraEnabled = "zebra_enabled", zebraBg = "zebra_bg"
        case hoverEnabled = "hover_enabled", hoverBg = "hover_bg", borderCollapse = "border_collapse", borderSpacingPx = "border_spacing_px"
    }

    public init(from decoder: Decoder) throws {
        self.init()
        let c = try decoder.container(keyedBy: CodingKeys.self)
        tableClass = try c.decodeIfPresent(String.self, forKey: .tableClass) ?? tableClass
        fontFamily = try c.decodeIfPresent(String.self, forKey: .fontFamily) ?? fontFamily
        fontSizePx = try c.decodeIfPresent(Int.self, forKey: .fontSizePx) ?? fontSizePx
        cellPaddingPx = try c.decodeIfPresent(Int.self, forKey: .cellPaddingPx) ?? cellPaddingPx
        borderWidthPx = try c.decodeIfPresent(Int.self, forKey: .borderWidthPx) ?? borderWidthPx
        borderStyle = try c.decodeIfPresent(String.self, forKey: .borderStyle) ?? borderStyle
        borderColor = try c.decodeIfPresent(String.self, forKey: .borderColor) ?? borderColor
        headerBg = try c.decodeIfPresent(String.self, forKey: .headerBg) ?? headerBg
        headerTextColor = try c.decodeIfPresent(String.self, forKey: .headerTextColor) ?? headerTextColor
        bodyBg = try c.decodeIfPresent(String.self, forKey: .bodyBg) ?? bodyBg
        zebraEnabled = try c.decodeIfPresent(Bool.self, forKey: .zebraEnabled) ?? zebraEnabled
        zebraBg = try c.decodeIfPresent(String.self, forKey: .zebraBg) ?? zebraBg
        hoverEnabled = try c.decodeIfPresent(Bool.self, forKey: .hoverEnabled) ?? hoverEnabled
        hoverBg = try c.decodeIfPresent(String.self, forKey: .hoverBg) ?? hoverBg
        borderCollapse = try c.decodeIfPresent(String.self, forKey: .borderCollapse) ?? borderCollapse
        borderSpacingPx = try c.decodeIfPresent(Int.self, forKey: .borderSpacingPx) ?? borderSpacingPx
    }
}
