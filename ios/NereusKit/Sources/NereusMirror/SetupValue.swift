// NereusSDR for iOS: one value of a control the Core describes in Setup, as the phone shows and edits it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A described control's value at the phone's boundary. A toggle is a
/// `bool`; an integer, a slider on whole numbers and a choice (its ordinal,
/// or its option's value) are `integer`; a decimal is `decimal`; text and
/// a colour (`#RRGGBBAA`) are `text`.
public enum SetupValue: Equatable, Sendable {
    case bool(Bool)
    case integer(Int64)
    case decimal(Double)
    case text(String)

    /// The value as a number, for numeric rows.
    public var number: Double? {
        switch self {
        case .integer(let value):
            return Double(value)
        case .decimal(let value):
            return value.isFinite ? value : nil
        default:
            return nil
        }
    }

    /// The value as a whole number, when it is one.
    public var whole: Int64? {
        switch self {
        case .integer(let value):
            return value
        case .decimal(let value):
            guard value.isFinite, value.rounded() == value, abs(value) < 9.0e15 else {
                return nil
            }
            return Int64(value)
        default:
            return nil
        }
    }

    public var flag: Bool? {
        if case .bool(let value) = self { return value }
        return nil
    }

    public var text: String? {
        if case .text(let value) = self { return value }
        return nil
    }
}
