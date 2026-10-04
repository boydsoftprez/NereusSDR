// NereusSDR for iOS: synthetic frames, plans and palettes for the band's tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMedia
import NereusModels

/// Synthetic inputs for the band's tests. None of these values is the
/// Core's: the palettes, plans and frames are made up here (D4).
enum BandFixtures {
    static func frame(trace: [Float], waterfall: [Float]? = nil, sequence: UInt32 = 1, endpointId: UInt32 = 1,
                      generation: UInt32 = 1, advance: Bool = true) -> DisplayFrame {
        DisplayFrame(endpointId: endpointId, contextGeneration: generation, encoderSequence: sequence,
                     producerTimestamp: UInt64(sequence) * 33_000_000, isKeyframe: sequence == 1,
                     waterfallAdvance: advance, minDbm: -160, maxDbm: 0, traceDbm: trace,
                     waterfallDbm: waterfall ?? trace, wideDbm: [])
    }

    static func extras(for frame: DisplayFrame, blobs: [DisplayExtras.PeakBlob]? = nil, hold: [Float]? = nil,
                       floor: Float? = nil, levels: (Float, Float)? = nil) -> DisplayExtras {
        DisplayExtras(endpointId: frame.endpointId, contextGeneration: frame.contextGeneration,
                      encoderSequence: frame.encoderSequence, peakBlobs: blobs, peakHoldDbm: hold,
                      noiseFloorDbm: floor,
                      waterfallLevels: levels.map { DisplayExtras.WaterfallLevels(lowDbm: $0.0, highDbm: $0.1) })
    }

    /// A palette from `stops` (`at`, `#RRGGBB`), decoded as the catalogue's are.
    static func palette(id: Int, _ stops: [(Double, String)]) throws -> StationCatalog.Palette {
        let object: [String: Any] = [
            "id": id, "name": "Test \(id)",
            "stops": stops.map { ["at": $0.0, "colour": $0.1] },
        ]
        return try JSONDecoder().decode(StationCatalog.Palette.self,
                                        from: JSONSerialization.data(withJSONObject: object))
    }

    /// A band plan from `segments` (low, high, label, `#RRGGBB`).
    static func plan(id: String, isDefault: Bool, _ segments: [(Double, Double, String, String)]) throws
        -> StationCatalog.BandPlan {
        try plan(id: id, isDefault: isDefault, licensed: segments.map { ($0.0, $0.1, $0.2, "", $0.3) })
    }

    /// A band plan from `licensed` segments (low, high, label, licence,
    /// `#RRGGBB`), marked `active` when `active` is set (nil sends no
    /// `active`, as an older Core), with `spots` (hz, label) when given.
    static func plan(id: String, name: String? = nil, isDefault: Bool, active: Bool? = nil,
                     licensed: [(Double, Double, String, String, String)],
                     spots: [(Double, String)]? = nil) throws -> StationCatalog.BandPlan {
        var object: [String: Any] = [
            "id": id, "name": name ?? "Plan \(id)", "default": isDefault,
            "segments": licensed.map {
                ["lowHz": $0.0, "highHz": $0.1, "label": $0.2, "licence": $0.3, "colour": $0.4]
            },
        ]
        if let active {
            object["active"] = active
        }
        if let spots {
            object["spots"] = spots.map { ["hz": $0.0, "label": $0.1] as [String: Any] }
        }
        return try JSONDecoder().decode(StationCatalog.BandPlan.self,
                                        from: JSONSerialization.data(withJSONObject: object))
    }
}
