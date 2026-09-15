import Foundation
import YachtCore

@main
struct YachtCLI {
    static func main() {
        do { try run(Array(CommandLine.arguments.dropFirst())) }
        catch { FileHandle.standardError.write(Data("yacht: \(error.localizedDescription)\n".utf8)); exit(1) }
    }

    static func run(_ arguments: [String]) throws {
        if arguments.isEmpty || arguments.contains("--help") || arguments.contains("-h") {
            print("""
            Y.A.C.H.T. — Yet Another CSV HTML Translator
            Usage: yacht input.csv [more.csv ...] [options]
              -o, --output PATH       Output for one input (default: beside CSV)
              --overwrite             Explicitly allow replacing existing HTML
              --delimiter VALUE       comma, tab, semicolon, pipe (default: comma)
              --unstyled              Omit all styling
              --table-class TEXT      CSS classes
              --font-family TEXT      Comma-separated font names
              --font-size N           Font size in pixels
              --cell-padding N        Cell padding in pixels
              --border-width N        Border width in pixels
              --border-style STYLE    solid, dashed, dotted, double, none, ...
              --border-color COLOR    CSS color
              --header-bg COLOR       Header background
              --header-text-color COLOR
              --body-bg COLOR         Body background
              --zebra BOOL            true/false, yes/no, on/off, 1/0
              --zebra-bg COLOR        Alternating row color
              --hover BOOL            Hover highlight
              --hover-bg COLOR        Hover background
              --border-collapse MODE  collapse or separate
              --border-spacing N      Border spacing in pixels
            """)
            return
        }
        var style = StyleOptions(), inputs: [String] = [], output: String?
        var delimiter = CSVDelimiter.comma, overwrite = false, index = 0, positional = false
        func integer(_ text: String, flag: String) throws -> Int {
            guard let value = Int(text) else { throw YachtError.invalid("\(flag) requires an integer.") }; return value
        }
        func boolean(_ text: String) throws -> Bool {
            switch text.lowercased() {
            case "1", "true", "yes", "y", "on": return true
            case "0", "false", "no", "n", "off": return false
            default: throw YachtError.invalid("Invalid boolean: \(text)")
            }
        }
        while index < arguments.count {
            let arg = arguments[index]; index += 1
            if positional || !arg.hasPrefix("-") { inputs.append(arg); continue }
            if arg == "--" { positional = true; continue }
            if arg == "--overwrite" { overwrite = true; continue }
            if arg == "--unstyled" { style = .unstyled; continue }
            let parts = arg.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            let flag = String(parts[0])
            let value: String
            if parts.count == 2 { value = String(parts[1]) }
            else { guard index < arguments.count else { throw YachtError.invalid("Missing value for \(flag).") }; value = arguments[index]; index += 1 }
            switch flag {
            case "-o", "--output": output = value
            case "--delimiter": guard let d = CSVDelimiter(rawValue: value) else { throw YachtError.invalid("Unknown delimiter: \(value)") }; delimiter = d
            case "--table-class": style.tableClass = value
            case "--font-family": style.fontFamily = value
            case "--font-size": style.fontSizePx = try integer(value, flag: flag)
            case "--cell-padding": style.cellPaddingPx = try integer(value, flag: flag)
            case "--border-width": style.borderWidthPx = try integer(value, flag: flag)
            case "--border-style": style.borderStyle = value
            case "--border-color": style.borderColor = value
            case "--header-bg": style.headerBg = value
            case "--header-text-color": style.headerTextColor = value
            case "--body-bg": style.bodyBg = value
            case "--zebra": style.zebraEnabled = try boolean(value)
            case "--zebra-bg": style.zebraBg = value
            case "--hover": style.hoverEnabled = try boolean(value)
            case "--hover-bg": style.hoverBg = value
            case "--border-collapse": style.borderCollapse = value
            case "--border-spacing": style.borderSpacingPx = try integer(value, flag: flag)
            default: throw YachtError.invalid("Unknown option: \(flag). See --help.")
            }
        }
        guard !inputs.isEmpty else { throw YachtError.invalid("Choose at least one input CSV.") }
        guard output == nil || inputs.count == 1 else { throw YachtError.invalid("--output can only be used with one input CSV.") }
        try style.validate()
        var failures = 0
        for path in inputs {
            let input = URL(fileURLWithPath: (path as NSString).expandingTildeInPath)
            let target = output.map { URL(fileURLWithPath: ($0 as NSString).expandingTildeInPath) } ?? FileService.defaultOutput(for: input)
            do {
                try FileService.convert(input, to: target, style: style, delimiter: delimiter, overwrite: overwrite)
                print("Converted: \(input.path) -> \(target.path)")
            } catch { failures += 1; FileHandle.standardError.write(Data("\(path): \(error.localizedDescription)\n".utf8)) }
        }
        if failures > 0 { throw YachtError.invalid("\(failures) file(s) failed.") }
    }
}
