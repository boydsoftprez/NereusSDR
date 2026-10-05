// NereusSDR for iOS: follows the Core's transmitter for the band: keyed on one of its slices, the carrier and high SWR
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror

/// Tells the band what the Core's transmitter is doing to it (R-IOS-11,
/// R-IOS-13; desktop PR #317 parity), as the desktop's remote window follows
/// its Core: the Core's `txState` says whether the radio is keyed and on
/// which slice (`keyed`, `txSliceId`), and a Core that sends no `txState`
/// says it with `radio.transmitting` and the slice whose `txSlice` is true.
/// The band is keyed when that slice is one of the band's. The carrier is
/// that slice's frequency plus its XIT offset while XIT is on, as the Core
/// places it. `txState.highSwr` and `swrWindBackLatched` give the high-SWR
/// border, whatever the Core's `txDisplayVersion`. Whoever keyed it (this
/// phone, another device, the radio's PTT, TUNE, VOX), the band follows.
@MainActor
final class TransmitDisplayModel {
    static let txStateKey = "txState"
    static let radioKey = "radio"

    private let band: BandModel
    private let slices: BandSlicesModel
    private let mirror: MirrorStore
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watchedObjects: [String: ObjectIdentifier] = [:]
    private var refreshQueued = false
    /// The Core keeps the transmit display on the pan that held the TX
    /// slice at the rise, even if slices are moved while keyed.
    private var riseSliceId: Int?
    private var risePanKey = ""

    init(band: BandModel, slices: BandSlicesModel, mirror: MirrorStore) {
        self.band = band
        self.slices = slices
        self.mirror = mirror
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
    }

    private func queueRefresh() {
        guard !refreshQueued else {
            return
        }
        refreshQueued = true
        Task { @MainActor [weak self] in
            self?.refreshQueued = false
            self?.refresh()
        }
    }

    private func watch(_ key: String) -> MirrorObject? {
        let object = mirror.object(key)
        let id = object.map(ObjectIdentifier.init)
        if watchedObjects[key] != id {
            watchedObjects[key] = id
            objectWatches[key] = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRefresh() }
        }
        return object
    }

    /// Reads the Core's transmitter now and hands the band what it touches.
    func refresh() {
        let state = watch(Self.txStateKey)
        let radio = watch(Self.radioKey)
        var keyed = false
        var txSliceId: Int?
        if let state, mirror.capabilityVersion("txStateVersion") >= 1 {
            keyed = Self.flag(state["keyed"]) ?? false
            txSliceId = Self.whole(state["txSliceId"]).map(Int.init)
        } else {
            // A Core that sends no txState: its radio's transmitting and the
            // slice marked for transmit.
            keyed = Self.flag(radio?["transmitting"]) ?? false
            txSliceId = slices.entries.first { $0.slice.txSlice }?.id
        }
        keyed = keyed && mirror.isSnapshotComplete
        if !keyed {
            riseSliceId = nil
            risePanKey = ""
        } else if riseSliceId == nil, let id = txSliceId, let tx = watch("slice:\(id)") {
            riseSliceId = id
            risePanKey = Self.panKey(tx)
        }
        var next = TransmitDisplay()
        let active = slices.active
        let activePanKey = active.flatMap { watch("slice:\($0.id)") }.map(Self.panKey)
        let onTransmitPan = keyed && active != nil && riseSliceId != nil
            && (risePanKey.isEmpty ? active?.id == riseSliceId : activePanKey == risePanKey)
        if onTransmitPan, let id = riseSliceId, let entry = slices.entries.first(where: { $0.id == id }) {
            let slice = watch("slice:\(id)")
            let xitOn = Self.flag(slice?["xitEnabled"]) ?? false
            let xitHz = xitOn ? Double(Self.whole(slice?["xitHz"]) ?? 0) : 0
            next.keyedHere = true
            next.carrierHz = entry.slice.frequencyHz + xitHz
        }
        // The desktop's red border belongs to the pan that holds transmit,
        // just like its keyed analyzer and orange filter.
        if onTransmitPan, let state {
            next.highSwr = Self.flag(state["highSwr"]) ?? false
            next.windBackLatched = Self.flag(state["swrWindBackLatched"]) ?? false
        }
        if band.transmit != next {
            band.transmit = next
        }
    }

    private static func flag(_ value: MirrorValue?) -> Bool? {
        if case .bool(let flag)? = value {
            return flag
        }
        return nil
    }

    private static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let whole)?, .enumeration(let whole)?:
            return whole
        case .double(let number)?:
            return Int64(exactly: number)
        default:
            return nil
        }
    }

    private static func panKey(_ slice: MirrorObject) -> String {
        if case .text(let key)? = slice["panKey"] {
            return key
        }
        return ""
    }
}
