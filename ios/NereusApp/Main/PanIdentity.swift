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
