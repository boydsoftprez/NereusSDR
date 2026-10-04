// NereusSDR for iOS: a JSON value as the link carries it, parsed and written compactly
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One JSON value. The link's messages are JSON objects (link document
/// section 2); this keeps a boolean apart from a number, which Foundation's
/// parser does not, and compares numbers by value, so `1` and `1.0` are equal.
public enum LinkJSON: Sendable, Hashable {
    case null
    case bool(Bool)
    case number(Double)
    case string(String)
    case array([LinkJSON])
    case object([String: LinkJSON])

    /// Why a text is not one JSON value.
    public struct ParseError: Error, CustomStringConvertible {
        public let description: String
    }

    /// Parses one JSON value from `text`.
    public static func parse(_ text: String) throws -> LinkJSON {
        let parsed: Any
        do {
            parsed = try JSONSerialization.jsonObject(with: Data(text.utf8), options: [.fragmentsAllowed])
        } catch {
            throw ParseError(description: "not JSON")
        }
        return try LinkJSON(foundation: parsed)
    }

    /// Converts a value Foundation's JSON parser produced.
    public init(foundation value: Any) throws {
        switch value {
        case is NSNull:
            self = .null
        case let number as NSNumber:
            if CFGetTypeID(number) == CFBooleanGetTypeID() {
                self = .bool(number.boolValue)
            } else {
                self = .number(number.doubleValue)
            }
        case let string as String:
            self = .string(string)
        case let array as [Any]:
            self = .array(try array.map { try LinkJSON(foundation: $0) })
        case let object as [String: Any]:
            var converted: [String: LinkJSON] = [:]
            for (key, element) in object {
                converted[key] = try LinkJSON(foundation: element)
            }
            self = .object(converted)
        default:
            throw ParseError(description: "a value JSON cannot hold")
        }
    }

    /// The value written as compact JSON, object keys sorted.
    public var compactText: String {
        var out = ""
        write(into: &out)
        return out
    }

    private func write(into out: inout String) {
        switch self {
        case .null:
            out += "null"
        case .bool(let value):
            out += value ? "true" : "false"
        case .number(let value):
            out += Self.numberText(value)
        case .string(let value):
            Self.writeString(value, into: &out)
        case .array(let elements):
            out += "["
            for (index, element) in elements.enumerated() {
                if index > 0 {
                    out += ","
                }
                element.write(into: &out)
            }
            out += "]"
        case .object(let members):
            out += "{"
            for (index, key) in members.keys.sorted().enumerated() {
                if index > 0 {
                    out += ","
                }
                Self.writeString(key, into: &out)
                out += ":"
                members[key]?.write(into: &out)
            }
            out += "}"
        }
    }

    /// A whole number inside the 64-bit range is written without a
    /// fraction, so ids and counters read as they were meant.
    private static func numberText(_ value: Double) -> String {
        guard value.isFinite else {
            // JSON has no NaN and no infinity; the codec never puts one here.
            return "null"
        }
        if value == value.rounded(), abs(value) < 9.0e18 {
            return String(Int64(value))
        }
        return "\(value)"
    }

    private static func writeString(_ value: String, into out: inout String) {
        out += "\""
        for scalar in value.unicodeScalars {
            switch scalar {
            case "\"":
                out += "\\\""
            case "\\":
                out += "\\\\"
            case "\n":
                out += "\\n"
            case "\r":
                out += "\\r"
            case "\t":
                out += "\\t"
            default:
                if scalar.value < 0x20 {
                    let hex = String(scalar.value, radix: 16)
                    out += "\\u" + String(repeating: "0", count: 4 - hex.count) + hex
                } else {
                    out.unicodeScalars.append(scalar)
                }
            }
        }
        out += "\""
    }
}
