// NereusSDR for iOS: one mirrored value, as the Core's objects, commands and capabilities hold it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// One value of a mirrored property, a capability or a command's argument
/// or result (link document section 4.1). An enum value travels as a whole
/// number within int64, one of the values its property allows; the class's
/// schema says which properties are enums.
///
/// Two NaN doubles compare equal, so a property the Core holds at NaN (a
/// slice's `snrDb` before a RADE signal is decoded) reads as unchanged.
public enum MirrorValue: Equatable, Sendable {
    case bool(Bool)
    case int(Int64)
    case double(Double)
    case text(String)
    case enumeration(Int64)

    public static func == (lhs: MirrorValue, rhs: MirrorValue) -> Bool {
        switch (lhs, rhs) {
        case (.bool(let a), .bool(let b)):
            return a == b
        case (.int(let a), .int(let b)):
            return a == b
        case (.double(let a), .double(let b)):
            return a == b || (a.isNaN && b.isNaN)
        case (.text(let a), .text(let b)):
            return a == b
        case (.enumeration(let a), .enumeration(let b)):
            return a == b
        default:
            return false
        }
    }

    /// The value a wire entry carries.
    public init(_ wire: LinkMessage.PropertyValue) {
        switch wire {
        case .bool(let value):
            self = .bool(value)
        case .i64(let value):
            self = .int(value)
        case .f64(let value):
            self = .double(value)
        case .utf8(let value):
            self = .text(value)
        case .enumeration(let value):
            self = .enumeration(value)
        }
    }

    /// The wire kind this value travels as when nothing says otherwise.
    public var naturalKind: LinkMessage.WireKind {
        switch self {
        case .bool:
            return .bool
        case .int:
            return .i64
        case .double:
            return .f64
        case .text:
            return .utf8
        case .enumeration:
            return .enumeration
        }
    }

    /// The value on the wire as `kind`, where the two are the same number:
    /// a whole number as an `f64`, an enum as an `i64` and the reverse. Any
    /// other pairing travels in the value's own kind, and the Core answers
    /// it as a wrong kind.
    public func wireValue(as kind: LinkMessage.WireKind) -> LinkMessage.PropertyValue {
        switch (self, kind) {
        case (.int(let value), .f64), (.enumeration(let value), .f64):
            return .f64(Double(value))
        case (.int(let value), .enumeration):
            return .enumeration(value)
        case (.enumeration(let value), .i64):
            return .i64(value)
        default:
            return wireValue
        }
    }

    /// The value on the wire in its own kind.
    public var wireValue: LinkMessage.PropertyValue {
        switch self {
        case .bool(let value):
            return .bool(value)
        case .int(let value):
            return .i64(value)
        case .double(let value):
            return .f64(value)
        case .text(let value):
            return .utf8(value)
        case .enumeration(let value):
            return .enumeration(value)
        }
    }
}
