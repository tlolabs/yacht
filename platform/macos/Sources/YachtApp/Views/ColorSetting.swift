import SwiftUI
import AppKit

struct ColorSetting: View {
    let title: String
    @Binding var value: String
    private var selectedColor: Binding<Color> {
        Binding(get: { Color(nsColor: Self.parse(value) ?? .clear) }, set: { color in
            guard let rgb = NSColor(color).usingColorSpace(.sRGB) else { return }
            value = String(format: "#%02x%02x%02x", Int((rgb.redComponent * 255).rounded()), Int((rgb.greenComponent * 255).rounded()), Int((rgb.blueComponent * 255).rounded()))
        })
    }
    var body: some View {
        LabeledContent(title) {
            HStack {
            ColorPicker("", selection: selectedColor, supportsOpacity: false)
                .labelsHidden().accessibilityLabel("\(title) picker")
            TextField("", text: $value).accessibilityLabel(title)
                .help("CSS hex color, color name, rgb(), hsl(), or transparent")
        }
        }
        .accessibilityElement(children: .contain)
    }
    private static func parse(_ text: String) -> NSColor? {
        let hex = text.hasPrefix("#") ? String(text.dropFirst()) : ""
        var expanded = hex
        if hex.count == 3 { expanded = hex.map { "\($0)\($0)" }.joined() }
        if expanded.count == 6, let n = UInt32(expanded, radix: 16) {
            return NSColor(srgbRed: Double((n >> 16) & 255) / 255, green: Double((n >> 8) & 255) / 255, blue: Double(n & 255) / 255, alpha: 1)
        }
        return ["transparent": NSColor.clear, "white": .white, "black": .black, "red": .red, "blue": .blue, "green": .green, "gray": .gray][text.lowercased()]
    }
}
