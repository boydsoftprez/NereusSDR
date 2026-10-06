// NereusSDR for iOS: how a described control's value is read from, and written to, its owner
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Reads and writes a described control's value in its owner's own form:
/// a Core setting's string, a mirrored property's typed value. Anything
/// that does not read as the control's kind reads nil, never a guess.
public enum SetupValueCoding {
    typealias Control = SetupDescription.Control

    // MARK: Settings

    /// A setting's string as the control's value. A setting-backed toggle
    /// matches its encoding's two strings, ignoring case; a choice among
    /// named `choices` reads its ordinal; a slider with options reads one of
    /// their values.
    static func decodeSetting(_ text: String, for control: Control) -> SetupValue? {
        switch control.kind {
        case .toggle?:
            guard let encoding = control.valueEncoding else { return nil }
            if text.caseInsensitiveCompare(encoding.trueValue) == .orderedSame { return .bool(true) }
            if text.caseInsensitiveCompare(encoding.falseValue) == .orderedSame { return .bool(false) }
            return nil
        case .choice?:
            guard let ordinal = Int64(text.trimmingCharacters(in: .whitespaces)) else { return nil }
            if let options = control.options {
                return options.contains { $0.value == ordinal } ? .integer(ordinal) : nil
            }
            guard let choices = control.choices, (0..<Int64(choices.count)).contains(ordinal) else { return nil }
            return .integer(ordinal)
        case .slider? where control.options != nil:
            guard let value = Int64(text.trimmingCharacters(in: .whitespaces)),
                  control.options?.contains(where: { $0.value == value }) == true else { return nil }
            return .integer(value)
        case .integer?, .slider?:
            let trimmed = text.trimmingCharacters(in: .whitespaces)
            if let value = Int64(trimmed) { return .integer(value) }
            if let value = Double(trimmed), value.isFinite, value.rounded() == value,
               abs(value) < 9.0e15, wholeRange(control) {
                return .integer(Int64(value))
            }
            if control.kind == .slider, let value = Double(trimmed), value.isFinite { return .decimal(value) }
            return nil
        case .decimal?:
            guard let value = Double(text.trimmingCharacters(in: .whitespaces)), value.isFinite else { return nil }
            return .decimal(value)
        case .text?, .colour?:
            return .text(text)
        default:
            return nil
        }
    }

    /// The string a setting is written as, or nil when the value does not
    /// fit the control.
    static func encodeSetting(_ value: SetupValue, for control: Control) -> String? {
        switch (control.kind, value) {
        case (.toggle?, .bool(let on)):
            guard let encoding = control.valueEncoding else { return nil }
            return on ? encoding.trueValue : encoding.falseValue
        case (.choice?, _), (.integer?, _):
            guard let whole = value.whole, fits(value, control) else { return nil }
            return String(whole)
        case (.slider?, _):
            guard fits(value, control) else { return nil }
            if let whole = value.whole, control.options != nil || wholeRange(control) { return String(whole) }
            return value.number.map(decimalText)
        case (.decimal?, _):
            guard let number = value.number, fits(value, control) else { return nil }
            return decimalText(number)
        case (.text?, .text(let text)), (.colour?, .text(let text)):
            return text
        default:
            return nil
        }
    }

    // MARK: Properties

    /// A mirrored property's value as the control's value.
    static func decodeProperty(_ value: MirrorValue, for control: Control) -> SetupValue? {
        switch (control.kind, value) {
        case (.toggle?, .bool(let on)):
            return .bool(on)
        case (.toggle?, _):
            return nil
        case (.text?, .text(let text)), (.colour?, .text(let text)):
            return .text(text)
        case (.readout?, .text(let text)):
            return .text(text)
        case (.readout?, .bool(let on)):
            return .bool(on)
        case (.readout?, .double(let number)):
            return number.isFinite ? .decimal(number) : nil
        case (_, .int(let whole)), (_, .enumeration(let whole)):
            return .integer(whole)
        case (.decimal?, .double(let number)), (.slider?, .double(let number)):
            return number.isFinite ? .decimal(number) : nil
        case (.integer?, .double(let number)), (.choice?, .double(let number)):
            guard number.isFinite, number.rounded() == number, abs(number) < 9.0e15 else { return nil }
            return .integer(Int64(number))
        default:
            return nil
        }
    }

    /// The typed value a property write carries. The store converts a
    /// whole number to its schema's kind.
    static func encodeProperty(_ value: SetupValue, for control: Control) -> MirrorValue? {
        switch (control.kind, value) {
        case (.toggle?, .bool(let on)):
            return .bool(on)
        case (.integer?, _), (.choice?, _):
            guard let whole = value.whole, fits(value, control) else { return nil }
            return .int(whole)
        case (.slider?, _):
            guard fits(value, control) else { return nil }
            if let whole = value.whole, control.options != nil || wholeRange(control) { return .int(whole) }
            return value.number.map(MirrorValue.double)
        case (.decimal?, _):
            guard let number = value.number, fits(value, control) else { return nil }
            return .double(number)
        case (.text?, .text(let text)), (.colour?, .text(let text)):
            return .text(text)
        default:
            return nil
        }
    }

    /// A command's `$controlValue`, in the control's declared type.
    static func commandValue(_ value: SetupValue, for control: Control) -> MirrorValue? {
        encodeProperty(value, for: control)
    }

    // MARK: Inside

    /// The value lies within the control's range and choices.
    static func fits(_ value: SetupValue, _ control: Control) -> Bool {
        if let options = control.options {
            guard let whole = value.whole else { return false }
            return options.contains { $0.value == whole }
        }
        if let choices = control.choices {
            guard let whole = value.whole else { return false }
            return (0..<Int64(choices.count)).contains(whole)
        }
        if let range = control.range {
            guard let number = value.number else { return false }
            return number >= range.minimum && number <= range.maximum
        }
        return value.number != nil
    }

    private static func wholeRange(_ control: Control) -> Bool {
        guard let range = control.range else { return false }
        return [range.minimum, range.maximum, range.step].allSatisfy { $0.rounded() == $0 }
    }

    private static func decimalText(_ number: Double) -> String {
        if number.rounded() == number, abs(number) < 9.0e15 {
            return String(Int64(number))
        }
        return String(number)
    }
}
