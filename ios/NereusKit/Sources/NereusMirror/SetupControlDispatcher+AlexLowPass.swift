// NereusSDR for iOS: the Alex-1 low-pass rows: which filter the radio is using, and the reason for a Core that cannot take them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Setup description 17's Alex-1 low-pass rows (R-IOS-18). The rows are
/// ordinary radio settings: one edge written, and the Core moves any
/// neighbour it has to and sends it back as a setting of its own, so the
/// phone never moves one itself. What this adds is read only: the filter
/// the radio is using now, from `radio`'s `alexLpfBits` (link section 7.1,
/// sent to a peer that declares `alexLpf` 1), shown beside its rows as
/// the desktop lights one lamp.
public extension SetupControlDispatcher {
    /// The mark beside a row: its filter is the one in use, or another is.
    enum LowPassLamp: Equatable, Sendable {
        case inUse
        case notInUse
    }

    /// What a row's filled mark reads as.
    nonisolated static let lowPassInUseText = "In use"
    /// A Core below `radioHardwareVersion` 10 stores the rows without using them.
    nonisolated static let lowPassNeedsNewerCoreReason =
        "This Core cannot change the low-pass filter rows for this app. Updating the Core may help."

    /// The low-pass rows' ids begin with this; the bypass row's too.
    nonisolated static let lowPassRowPrefix = "hardware.alex1Filters.lpf"
    nonisolated static let lowPassBitsProperty = "alexLpfBits"
    nonisolated static let radioObjectKey = "radio"

    /// Each row's filter by its id's band, as the link document's section
    /// 7.1 names the bits: 0x01 30/20m, 0x02 60/40m, 0x04 80m, 0x08 160m,
    /// 0x10 6m, 0x20 12/10m, 0x40 17/15m.
    nonisolated static let lowPassBits: [String: Int64] = [
        "20m": 0x01, "40m": 0x02, "80m": 0x04, "160m": 0x08, "6m": 0x10, "10m": 0x20, "15m": 0x40,
    ]

    /// The band a low-pass edge row names (`hardware.alex1Filters.lpf.<band>.start|end`);
    /// nil for any other row, the bypass included.
    nonisolated static func lowPassBand(_ id: String) -> String? {
        let parts = id.split(separator: ".", omittingEmptySubsequences: false)
        guard parts.count == 5, parts[0] == "hardware", parts[1] == "alex1Filters", parts[2] == "lpf",
              parts[4] == "start" || parts[4] == "end", lowPassBits[String(parts[3])] != nil else {
            return nil
        }
        return String(parts[3])
    }

    /// The mark beside a low-pass row: nil for any other row, and for every
    /// row while the Core has not said which filter is in use (no value, or
    /// -1 before its first choice).
    func lowPassLamp(for control: SetupDescription.Control) -> LowPassLamp? {
        guard let band = Self.lowPassBand(control.id), let bit = Self.lowPassBits[band] else {
            return nil
        }
        watch(Self.radioObjectKey)
        guard store.isSnapshotComplete, !store.isStale,
              case .int(let bits)? = store.object(Self.radioObjectKey)?[Self.lowPassBitsProperty], bits >= 0 else {
            return nil
        }
        return bits & bit != 0 ? .inUse : .notInUse
    }

    /// A low-pass row's gate reason in its own words: a Core too old for
    /// the rows says so plainly; any other reason is kept.
    internal func lowPassGateReason(_ control: SetupDescription.Control, _ reason: String) -> String {
        guard control.id.hasPrefix(Self.lowPassRowPrefix), reason == Self.needsNewerCoreReason,
              control.gate?.capability == "radioHardwareVersion" else {
            return reason
        }
        return Self.lowPassNeedsNewerCoreReason
    }
}
