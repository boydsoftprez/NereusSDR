// NereusSDR for iOS: an authoritative PA profile body from PaProfilesFacade
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Read-only values from PaProfilesFacade.cpp. The phone never seeds a
/// profile, calculates PA drive, or chooses a deletion fallback itself.
public struct PaProfiles: Equatable, Sendable {
    public struct Band: Equatable, Sendable {
        public let band: Int
        public let label: String
        public let gain: Double
        public let adjust: [Double]
        public let maxPower: Double
        public let useMax: Bool

        public func value(for column: SetupDescription.PaColumn) -> SetupValue? {
            switch column.field {
            case "gain": return .decimal(gain)
            case "adjust": return column.driveStep.map { .decimal(adjust[$0]) }
            case "maxPower": return .decimal(maxPower)
            case "useMax": return .bool(useMax)
            default: return nil
            }
        }
    }

    public let names: [String]
    public let active: String
    /// Provenance of the active profile, not evidence of unchanged factory gains.
    public let factory: Bool
    public let bands: [Band]
    public let revision: Int64

    public init?(json: String, revision: Int64, grid: SetupDescription.PaProfileGrid) {
        guard (0...Int64(UInt32.max)).contains(revision), let data = json.data(using: .utf8),
              let root = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
              let names = root["names"] as? [String], !names.isEmpty,
              names.allSatisfy({ !$0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }),
              Set(names.map { $0.lowercased() }).count == names.count,
              let active = root["active"] as? String, names.contains(active),
              let factory = SetupDescription.boolean(root["factory"]),
              let source = root["bands"] as? [[String: Any]], source.count == grid.rows.count else { return nil }
        var bands: [Band] = []
        for (index, raw) in source.enumerated() {
            let row = grid.rows[index]
            guard raw["band"] as? String == row.label,
                  let gain = SetupDescription.number(raw["gain"]), let maxPower = SetupDescription.number(raw["maxPower"]),
                  let useMax = SetupDescription.boolean(raw["useMax"]),
                  let rawAdjust = raw["adjust"] as? [Any], rawAdjust.count == 9 else { return nil }
            let adjust = rawAdjust.compactMap(SetupDescription.number)
            guard adjust.count == 9, grid.column("gain")?.fits(gain) == true,
                  grid.column("maxPower")?.fits(maxPower) == true,
                  (0..<9).allSatisfy({ grid.column("adjust\($0 + 1)")?.fits(adjust[$0]) == true }) else { return nil }
            bands.append(Band(band: row.band, label: row.label, gain: gain, adjust: adjust, maxPower: maxPower, useMax: useMax))
        }
        self.names = names
        self.active = active
        self.factory = factory
        self.bands = bands
        self.revision = revision
    }
}
