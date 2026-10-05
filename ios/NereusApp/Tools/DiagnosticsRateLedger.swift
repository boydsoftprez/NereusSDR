// NereusSDR for iOS: reset-aware rates for selected application payloads
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Baselines are local to the collector. A changed physical lifetime, absent
/// counter, decrease or nonpositive interval leaves a gap and starts anew.
struct DiagnosticsRateLedger {
    private struct Baseline {
        let lifetime: String
        let time: Int64
        let count: UInt64
    }
    private var baselines: [String: Baseline] = [:]

    mutating func rate(_ key: String, lifetime: String?, count: UInt64?,
                       at time: Int64, multiplier: Double) -> Double? {
        guard let lifetime, let count else {
            baselines.removeValue(forKey: key)
            return nil
        }
        let old = baselines.updateValue(Baseline(lifetime: lifetime, time: time, count: count),
                                        forKey: key)
        guard let old, old.lifetime == lifetime, time > old.time, count >= old.count,
              multiplier.isFinite, multiplier >= 0 else { return nil }
        let value = Double(count - old.count) * multiplier / Double(time - old.time)
        return value.isFinite ? value : nil
    }

    mutating func clear() { baselines.removeAll() }
}
