// NereusSDR for iOS: which pan a display control acts on, and how many pans and slices the radio allows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror

/// One of the phone's pans, numbered from 1 as the toolbar shows them.
/// The Core names the same pan `pan-<number - 1>` in a slice's `panKey`,
/// and the phone's own display store keys it by its number.
struct PanIdentity: Hashable, Sendable, Comparable {
    let number: Int

    /// Pan 1, the phone's first pan and its band.
    static let first = PanIdentity(number: 1)

    /// The pan's key in the phone's display store: Pan 1's is `1`, so
    /// `phone.band.display.1` stays Pan 1's.
    var storeKey: String { "\(number)" }

    /// The Core's key for this pan, as a slice's `panKey` carries it.
    var corePanKey: String { "pan-\(number - 1)" }

    /// The pan's name, as the toolbar and the column show it.
    var title: String { "Pan \(number)" }

    /// The pan a Core `panKey` (`pan-0`, `pan-1`, ...) names, or nil for
    /// any other key.
    init?(corePanKey: String) {
        let prefix = "pan-"
        guard corePanKey.hasPrefix(prefix),
              let index = Int(corePanKey.dropFirst(prefix.count)), index >= 0,
              String(index) == corePanKey.dropFirst(prefix.count) else {
            return nil
        }
        number = index + 1
    }

    init(number: Int) {
        self.number = number
    }

    static func < (lhs: PanIdentity, rhs: PanIdentity) -> Bool {
        lhs.number < rhs.number
    }
}

/// How many pans and slices the connected radio allows, from the Core's
/// capabilities, read as ``ConnectionPerformanceModel/receiverSlots(_:)``
/// reads them.
enum PanLimits {
    /// The pans the radio allows: the fewer of its slices
    /// (`effectiveMaxSlices`) and its receivers (`userDdcCount`), and at
    /// least 1. Either one missing counts as the other; both missing gives 1.
    // From src/gui/MainWindow.cpp:18953-18957 (panLayoutLimitFor: qMin(maxSlices(), userStreamCount()))
    static func panCount(capabilities: [String: MirrorValue]) -> Int {
        let slices = count("effectiveMaxSlices", in: capabilities)
        let receivers = count("userDdcCount", in: capabilities)
        guard let limit = [slices, receivers].compactMap({ $0 }).min() else {
            return 1
        }
        return max(1, limit)
    }

    /// The slices the radio allows (`effectiveMaxSlices`), or nil when the
    /// Core does not say.
    static func sliceCount(capabilities: [String: MirrorValue]) -> Int? {
        count("effectiveMaxSlices", in: capabilities)
    }

    private static func count(_ key: String, in capabilities: [String: MirrorValue]) -> Int? {
        if case .int(let value)? = capabilities[key], value > 0 {
            return Int(value)
        }
        return nil
    }
}

/// How the radio's limits are worded, as the approved board words them:
/// the radio by the name the Core reports, and the count as a word
/// ("The Hermes Lite 2 can show two pans."). One place for +RX's reason
/// and Add a pan's.
enum RadioWords {
    /// The radio's name, or "radio" when the Core gives none.
    static func name(_ radio: String?) -> String {
        radio.flatMap { $0.isEmpty ? nil : $0 } ?? "radio"
    }

    /// `count` as the board writes it: a word from one to twelve, digits past that.
    static func number(_ count: Int) -> String {
        let words = ["no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
                     "eleven", "twelve"]
        return words.indices.contains(count) ? words[count] : String(count)
    }

    /// `count` and its noun, singular for one: "one slice", "five slices".
    static func count(_ count: Int, _ singular: String, _ plural: String) -> String {
        "\(number(count)) \(count == 1 ? singular : plural)"
    }

    /// +RX's reason at the slice limit: "The <radio> runs <n> slices." (spec sections 6.2 and 9).
    static func sliceLimit(radio: String?, limit: Int) -> String {
        "The \(name(radio)) runs \(count(limit, "slice", "slices"))."
    }

    /// Add a pan's reason at the pan limit: "The <radio> can show <n> pans." (spec sections 6.3 and 9).
    static func panLimit(radio: String?, limit: Int) -> String {
        "The \(name(radio)) can show \(count(limit, "pan", "pans"))."
    }

    /// Letters or pan names joined as the board joins them: "C", "B and C", "A, B and C".
    static func list(_ items: [String]) -> String {
        guard items.count > 1, let last = items.last else {
            return items.joined()
        }
        return items.dropLast().joined(separator: ", ") + " and " + last
    }
}
