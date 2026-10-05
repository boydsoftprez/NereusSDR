// NereusSDR for iOS: reading one of the Core's property values as the Tools pages show it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror

/// A mirrored property's value read as the kind a Tools page shows: nil
/// when the Core has not sent it, or sent another kind.
enum ToolValue {
    static func flag(_ value: MirrorValue?) -> Bool? {
        if case .bool(let flag)? = value {
            return flag
        }
        return nil
    }

    static func text(_ value: MirrorValue?) -> String? {
        if case .text(let text)? = value {
            return text
        }
        return nil
    }

    static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let whole)?, .enumeration(let whole)?:
            return whole
        case .double(let number)? where number.isFinite && number == number.rounded()
            && abs(number) < 9.0e15:
            return Int64(number)
        default:
            return nil
        }
    }

    static func number(_ value: MirrorValue?) -> Double? {
        switch value {
        case .double(let number)? where number.isFinite:
            return number
        case .int(let whole)?, .enumeration(let whole)?:
            return Double(whole)
        default:
            return nil
        }
    }

    /// A compact JSON array of `count` whole numbers, as the Core keeps a
    /// ten-band setting (`[-12,-12,...]`); nil for anything else.
    static func wholeArray(_ text: String?, count: Int) -> [Int]? {
        guard let text, let array = (try? JSONSerialization.jsonObject(with: Data(text.utf8))) as? [Any],
              array.count == count else {
            return nil
        }
        var values: [Int] = []
        for item in array {
            guard let number = item as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
                  number.doubleValue.isFinite, number.doubleValue == number.doubleValue.rounded(),
                  abs(number.doubleValue) < 1.0e9 else {
                return nil
            }
            values.append(number.intValue)
        }
        return values
    }

    /// Whole numbers as the Core takes a ten-band setting: `[-12,-12,...]`.
    static func wholeArrayText(_ values: [Int]) -> String {
        "[" + values.map(String.init).joined(separator: ",") + "]"
    }
}
