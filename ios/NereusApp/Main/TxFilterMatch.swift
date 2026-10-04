// NereusSDR for iOS: Match RX: the TX filter from the active slice's receive filter, in audio hertz
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import NereusModels

/// Match RX (D81): the transmit passband set to the active slice's receive
/// filter. A slice's `filterLow` and `filterHigh` are signed about the
/// carrier (negative below it); the TX filter's are audio hertz. The rule
/// is the desktop's filter-preset match (NereusSDR's RX applet and VFO
/// flag): AM, SAM, DSB, FM and DRM take 0 to the magnitude of the high
/// edge; every other mode takes the two edges' magnitudes, smaller first,
/// which flips the LSB family's edges and keeps the USB family's.
@MainActor
enum TxFilterMatch {
    /// The modes, as the Core's catalogue names them, whose TX filter
    /// starts at 0 Hz: they transmit both sides of the carrier out to the
    /// high edge (the band's orange TX filter, ``TransmitModel/txFilter(carrierHz:passband:lowHz:highHz:bothSides:)``).
    static let fromZeroModes: Set<String> = ["AM", "SAM", "DSB", "FM", "DRM"]

    /// The TX filter's low and high edges, in audio hertz, for a receive
    /// filter of `lowHz` to `highHz` in the mode the catalogue calls `modeLabel`.
    static func audioEdges(lowHz: Int64, highHz: Int64, modeLabel: String) -> (low: Int64, high: Int64) {
        if fromZeroModes.contains(modeLabel) {
            return (0, abs(highHz))
        }
        let low = abs(lowHz)
        let high = abs(highHz)
        return (min(low, high), max(low, high))
    }

    /// The edges for the slice's own filter and mode, or nil while the
    /// slice lacks either or the catalogue does not name its mode.
    static func edges(slice: MirrorObject, modes: [StationCatalog.Mode]) -> (low: Int64, high: Int64)? {
        guard let low = RxPanelModel.whole(slice[RxPanelModel.Property.filterLow]),
              let high = RxPanelModel.whole(slice[RxPanelModel.Property.filterHigh]),
              let mode = RxPanelModel.whole(slice[RxPanelModel.Property.dspMode]),
              let label = modes.first(where: { Int64($0.id) == mode })?.label else {
            return nil
        }
        return audioEdges(lowHz: low, highHz: high, modeLabel: label)
    }

    /// The two writes, in the order that never leaves the low edge above
    /// the high one: the high edge first when the new low edge is at or
    /// above the TX filter's high edge now.
    static func writes(_ edges: (low: Int64, high: Int64), currentHighHz: Int64?) -> [(String, Int64)] {
        if let currentHighHz, edges.low >= currentHighHz {
            return [("filterHigh", edges.high), ("filterLow", edges.low)]
        }
        return [("filterLow", edges.low), ("filterHigh", edges.high)]
    }
}
