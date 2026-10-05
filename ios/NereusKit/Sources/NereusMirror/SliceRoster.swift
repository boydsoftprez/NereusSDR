// NereusSDR for iOS: every live slice on the Core, one row each in letter order, with who holds it and what this phone may do
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The slice list behind the toolbar's Slice button (R-IOS-11, R-IOS-42;
/// JJ's rulings of 2026-09-30, the board's `#slicelist-review`): every live
/// slice on the Core, the ones this phone has joined (`slice:<id>`) and
/// every other (`marker:<id>`), one row each in letter order, never
/// regrouped, so a row never moves under a finger.
///
/// Each row says where the slice is ("This pan" or "Another pan"), who
/// holds it in the phone's own words ("You control", "Listening ·
/// controlled by the Core", "Controlled by Shack desktop") with the holder
/// named by the Core's ladder (``SliceAccess/deviceWords(_:devices:short:)``),
/// who else listens, and whether Take control is greyed and why, in the
/// Core's words (``SliceAccess/takeRefusal(_:version:devices:)``).
public enum SliceRoster {
    /// One slice as the mirror carries it: a joined `slice:<id>` or a `marker:<id>`.
    public struct Slice: Equatable, Sendable {
        public var sliceId: Int
        public var frequencyHz: Double
        /// The `dspMode` value, the catalogue's `modes` id; -1 for none.
        public var mode: Int
        /// The Core's band number, the catalogue's band grid's id; -1 for none.
        public var band: Int
        /// The receiver it sits on, from 0; -1 on none.
        public var streamIndex: Int
        /// The Core pan that owns it; nil for a marker, which carries none.
        public var panKey: String?

        public init(sliceId: Int, frequencyHz: Double, mode: Int, band: Int, streamIndex: Int, panKey: String? = nil) {
            self.sliceId = sliceId
            self.frequencyHz = frequencyHz
            self.mode = mode
            self.band = band
            self.streamIndex = streamIndex
            self.panKey = panKey
        }
    }

    /// What this phone is to a slice.
    public enum Relation: Equatable, Sendable {
        /// This phone controls it (every joined slice from a Core that does not share slices).
        case control
        /// This phone has joined it and another device, or nobody, controls it.
        case listening
        /// This phone has not joined it.
        case none
    }

    /// One row of the list.
    public struct Row: Equatable, Sendable, Identifiable {
        public var slice: Slice
        public var relation: Relation
        /// On the pan this phone's band shows: "This pan"; otherwise "Another pan".
        public var here: Bool
        /// Who holds it, in the phone's own words: "You control",
        /// "Listening · controlled by the Core", "Controlled by Shack desktop".
        public var holderLine: String
        /// The holder alone, in the Core's ladder: "the Core", "Shack desktop".
        public var holder: String
        /// Every other device that listens, each named by the Core's ladder.
        public var alsoListening: [String]
        /// Why Take control is greyed, in the Core's words; nil when it can be pressed.
        public var takeRefusal: String?
        /// The slice's `access:<id>`; nil from a Core that does not share slices.
        public var access: SliceAccess.State?

        public var id: Int { slice.sliceId }
        public var letter: String { SliceAccess.letter(slice.sliceId) }
        /// The slice is on the air (`access:<id>.onAir`).
        public var onAir: Bool { access?.onAir ?? false }
    }

    /// The phone's own words for a row.
    public static let youControlText = "You control"
    public static let thisPanText = "This pan"
    public static let anotherPanText = "Another pan"
    public static let alsoListeningText = "Also listening: "

    /// "Listening · controlled by <holder>".
    public static func listeningLine(_ holder: String) -> String {
        "Listening \u{00B7} controlled by \(holder)"
    }

    /// "Controlled by <holder>".
    public static func controlledLine(_ holder: String) -> String {
        "Controlled by \(holder)"
    }

    /// Every row, in letter order. `joined` are the `slice:<id>` objects,
    /// `markers` every other slice. `me` is this phone's device id (nil
    /// until it has its key: every joined slice reads as this phone's).
    /// `homePanKey` and `homeStreamIndex` are the pan and receiver this
    /// phone's own band shows: a slice on either is "This pan". From a
    /// Core that does not share slices (`version` 0) only this phone's own
    /// slices are listed.
    public static func rows(joined: [Slice], markers: [SeveralDevices.SliceMarker], access: [Int: SliceAccess.State],
                            devices: [SeveralDevices.ConnectedDevice], me: String?, version: Int64,
                            homePanKey: String?, homeStreamIndex: Int?) -> [Row] {
        var rows: [Row] = []
        var seen = Set<Int>()
        for slice in joined where !seen.contains(slice.sliceId) {
            seen.insert(slice.sliceId)
            let state = access[slice.sliceId]
            let relation: Relation
            if let state, let me, state.controllerDeviceId != me {
                relation = .listening
            } else {
                relation = .control
            }
            rows.append(row(slice, relation: relation, state: state, marker: nil, devices: devices, me: me,
                            version: version, homePanKey: homePanKey, homeStreamIndex: homeStreamIndex))
        }
        if version >= 1 {
            for marker in markers where !seen.contains(marker.sliceId) {
                seen.insert(marker.sliceId)
                let slice = Slice(sliceId: marker.sliceId, frequencyHz: marker.frequencyHz, mode: marker.mode,
                                  band: marker.band, streamIndex: marker.streamIndex)
                rows.append(row(slice, relation: .none, state: access[marker.sliceId], marker: marker,
                                devices: devices, me: me, version: version, homePanKey: homePanKey,
                                homeStreamIndex: homeStreamIndex))
            }
        }
        return rows.sorted { $0.slice.sliceId < $1.slice.sliceId }
    }

    private static func row(_ slice: Slice, relation: Relation, state: SliceAccess.State?,
                            marker: SeveralDevices.SliceMarker?, devices: [SeveralDevices.ConnectedDevice],
                            me: String?, version: Int64, homePanKey: String?, homeStreamIndex: Int?) -> Row {
        let holder: String
        if let state {
            holder = SliceAccess.deviceWords(state.controllerDeviceId, devices: devices)
        } else if let marker {
            holder = markerHolder(marker, devices: devices)
        } else {
            holder = SliceAccess.coreWords
        }
        let line: String
        switch relation {
        case .control:
            line = youControlText
        case .listening:
            line = listeningLine(holder)
        case .none:
            line = controlledLine(holder)
        }
        let also = state.map { SliceAccess.listenerWords($0, devices: devices, excluding: me) } ?? []
        var refusal: String?
        if relation != .control, let state {
            refusal = SliceAccess.takeRefusal(state, version: version, devices: devices)
        }
        let onPan = slice.panKey != nil && slice.panKey == homePanKey
        let onReceiver = slice.streamIndex >= 0 && slice.streamIndex == homeStreamIndex
        return Row(slice: slice, relation: relation, here: onPan || onReceiver, holderLine: line, holder: holder,
                   alsoListening: also, takeRefusal: refusal, access: state)
    }

    /// A marker's holder while the Core has sent no `access:<id>` for it:
    /// the Core's own position by its hosting desktop's name (or "the
    /// Core"), else the owner's name as sent, else the Core's ladder.
    static func markerHolder(_ marker: SeveralDevices.SliceMarker, devices: [SeveralDevices.ConnectedDevice]) -> String {
        if marker.ownerKind == SliceAccess.stationDeviceId {
            return SliceAccess.deviceWords(SliceAccess.stationDeviceId, devices: devices)
        }
        if marker.ownerDeviceId.isEmpty {
            return SliceAccess.coreWords
        }
        if !marker.ownerName.isEmpty {
            return marker.ownerName
        }
        if !marker.ownerShortName.isEmpty {
            return marker.ownerShortName
        }
        return SliceAccess.deviceWords(marker.ownerDeviceId, devices: devices)
    }

    // MARK: A refused new slice

    /// One live slice a refused `addSlice` or `addSliceOnPan` offers to
    /// listen in to (`usableSlices`, the contract note, "Capacity results").
    public struct UsableSlice: Equatable, Sendable, Identifiable {
        public var sliceId: Int
        public var incarnation: Int64
        public var letter: String
        public var controllerDeviceId: String

        public var id: Int { sliceId }

        public init(sliceId: Int, incarnation: Int64, letter: String, controllerDeviceId: String) {
            self.sliceId = sliceId
            self.incarnation = incarnation
            self.letter = letter
            self.controllerDeviceId = controllerDeviceId
        }
    }

    /// The `usableSlices` JSON array, in the Core's order; empty when unreadable.
    public static func usableSlices(_ text: String) -> [UsableSlice] {
        guard case .array(let entries)? = try? LinkJSON.parse(text) else {
            return []
        }
        // Only whole numbers a slice id and an incarnation can be: a
        // fraction or a number past 2^53 skips the entry rather than trap.
        func whole(_ value: LinkJSON?) -> Int64? {
            guard case .number(let number)? = value, number.isFinite, number.rounded(.towardZero) == number,
                  abs(number) < 9_007_199_254_740_992 else {
                return nil
            }
            return Int64(number)
        }
        return entries.compactMap { entry in
            guard case .object(let object) = entry, let wholeId = whole(object["sliceId"]),
                  let id = Int(exactly: wholeId), let incarnation = whole(object["incarnation"]) else {
                return nil
            }
            var letter = ""
            if case .string(let text)? = object["letter"] {
                letter = text
            }
            var controller = ""
            if case .string(let text)? = object["controllerDeviceId"] {
                controller = text
            }
            return UsableSlice(sliceId: id, incarnation: incarnation,
                               letter: letter.isEmpty ? SliceAccess.letter(id) : letter,
                               controllerDeviceId: controller)
        }
    }
}
