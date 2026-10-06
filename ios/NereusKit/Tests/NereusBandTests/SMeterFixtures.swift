// NereusSDR for iOS: the S-meter tests' synthetic catalogue meters
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

/// Synthetic catalogue meters (D4): S0 at -130 dBm, 5 dB an S-unit, S9 at
/// -85, marks every 10 dB to S9+60 at -25; power 0 to 120 W, red from 100 W.
enum SMeterFixtures {
    static func meters(maxW: Double = 120, redFromW: Double = 100) throws -> StationCatalog.Meters {
        var sUnits: [[String: Any]] = []
        for unit in 0 ... 9 {
            sUnits.append(["label": "S\(unit)", "dbm": -130 + unit * 5])
        }
        var over: [[String: Any]] = []
        for step in 1 ... 6 {
            over.append(["label": "+\(step * 10)", "dbm": -85 + step * 10])
        }
        let object: [String: Any] = [
            "sMeter": ["minDbm": -130, "s9Dbm": -85, "maxDbm": -25, "dbPerSUnit": 5, "redFromDbm": -85,
                       "sUnits": sUnits, "overS9": over],
            "micLevel": ["minDb": -30, "maxDb": 5, "yellowFromDb": -5, "redFromDb": 0],
            "rfPower": ["minW": 0, "maxW": maxW, "ratedW": redFromW, "redFromW": redFromW],
            "swr": ["min": 1, "max": 3, "redFrom": 2.5],
        ]
        let data = try JSONSerialization.data(withJSONObject: object)
        return try JSONDecoder().decode(StationCatalog.Meters.self, from: data)
    }
}
