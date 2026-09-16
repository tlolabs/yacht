import Foundation
import CYacht

/// Synchronous ABI adapter. Call expensive operations through Background.run.
/// Rust calls cancellation on this same thread while UnsafeCurrentTask is valid.
enum RustBridge {
    static func call<T: Decodable>(_ op: String, _ arguments: [String: Any] = [:], cancellable: Bool = true) throws -> T {
        if cancellable { try Task.checkCancellation() }
        var request = arguments; request["op"] = op; request["version"] = 1
        let json = String(decoding: try JSONSerialization.data(withJSONObject: request, options: [.sortedKeys]), as: UTF8.self)
        let data: Data = try withUnsafeCurrentTask { task in
            var task = cancellable ? task : nil
            return try withUnsafeMutablePointer(to: &task) { context in
                guard let response = json.withCString({ yacht_request($0, { raw in
                    raw?.assumingMemoryBound(to: UnsafeCurrentTask?.self).pointee?.isCancelled ?? false
                }, context) }) else { throw YachtError.invalid("The Rust core returned no response.") }
                defer { yacht_free(response) }
                return Data(bytes: response, count: strlen(response))
            }
        }
        let envelope = try JSONDecoder().decode(Envelope<T>.self, from: data)
        if let error = envelope.error {
            if error.code == "cancelled" { throw CancellationError() }
            throw YachtError.invalid(error.message)
        }
        guard let value = envelope.ok else { throw YachtError.invalid("Invalid response from the Rust core.") }
        return value
    }
    static func perform(_ op: String, _ arguments: [String: Any] = [:], cancellable: Bool = true) throws {
        let _: JSONValue = try call(op, arguments, cancellable: cancellable)
    }
    static func object<T: Encodable>(_ value: T) throws -> Any {
        try JSONSerialization.jsonObject(with: JSONEncoder().encode(value), options: [.fragmentsAllowed])
    }
    private struct Failure: Decodable { let code: String; let message: String }
    private struct Envelope<T: Decodable>: Decodable {
        let ok: T?; let error: Failure?
        enum CodingKeys: String, CodingKey { case ok, error }
        init(from decoder: Decoder) throws {
            let c = try decoder.container(keyedBy: CodingKeys.self)
            error = try c.decodeIfPresent(Failure.self, forKey: .error)
            ok = c.contains(.ok) ? try c.decode(T.self, forKey: .ok) : nil
        }
    }
}

enum JSONValue: Codable, Equatable, Sendable {
    case string(String), integer(Int), boolean(Bool), object([String: JSONValue]), array([JSONValue]), null
    init(from decoder: Decoder) throws {
        let c = try decoder.singleValueContainer()
        if c.decodeNil() { self = .null }
        else if let v = try? c.decode(Bool.self) { self = .boolean(v) }
        else if let v = try? c.decode(Int.self) { self = .integer(v) }
        else if let v = try? c.decode(String.self) { self = .string(v) }
        else if let v = try? c.decode([String: JSONValue].self) { self = .object(v) }
        else { self = .array(try c.decode([JSONValue].self)) }
    }
    func encode(to encoder: Encoder) throws {
        var c = encoder.singleValueContainer()
        switch self {
        case .string(let v): try c.encode(v)
        case .integer(let v): try c.encode(v)
        case .boolean(let v): try c.encode(v)
        case .object(let v): try c.encode(v)
        case .array(let v): try c.encode(v)
        case .null: try c.encodeNil()
        }
    }
}
