// NereusSDR for iOS: other devices' slices on this band, read from the Core's markers, never tuned from here
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels

/// Other devices' slices (D46, R-IOS-17): each `marker:<id>` the Core
/// mirrors, with its letter's colour from the catalogue and its mode's
/// label, and who is on the Core, for how long an owner has been away.
/// Nothing here writes to the Core: another device's slice is only shown.
@MainActor
final class ForeignSlicesModel: ObservableObject {
    /// One other device's slice and what its label and note read.
    struct Entry: Equatable, Identifiable {
        var slice: ForeignSliceMarkers.Slice
        var marker: SeveralDevices.SliceMarker
        /// The catalogue's label for the slice's mode, `LSB`.
        var modeLabel: String
        /// How long its owner has been away when the Core last said, if it is.
        var awayForSeconds: Int64?

        var id: Int { slice.id }
    }

    @Published private(set) var entries: [Entry] = []
    /// Who is on the Core now, as its `connectedDevices` lists them.
    @Published private(set) var connected: [SeveralDevices.ConnectedDevice] = []
    /// The slice whose note is open under a tap on its label, if any.
    @Published var openNote: Int?

    var catalog: StationCatalog? {
        didSet {
            if catalog != oldValue {
                rebuild()
            }
        }
    }

    private let store: MirrorStore
    private var watching: [String: AnyCancellable] = [:]
    private var keysWatch: AnyCancellable?

    init(store: MirrorStore, catalog: StationCatalog? = nil) {
        self.store = store
        self.catalog = catalog
        keysWatch = store.$objectKeys.sink { [weak self] _ in
            Task { @MainActor in self?.watch() }
        }
        watch()
    }

    /// The kind of a device on the Core (`phone`, `computer`, ...), or "".
    func kind(of deviceId: String) -> String {
        connected.first { $0.deviceId == deviceId }?.kind ?? ""
    }

    /// The catalogue's label for a mode, or "" when it has none.
    func modeLabel(_ mode: Int) -> String {
        catalog?.modes.first { $0.id == mode }?.label ?? ""
    }

    private func watch() {
        let objects = store.objects(ofClass: SeveralDevices.markerClass)
            + [store.object(SeveralDevices.connectedDevicesKey)].compactMap { $0 }
        let keys = Set(objects.map(\.key))
        for key in watching.keys where !keys.contains(key) {
            watching[key] = nil
        }
        for object in objects where watching[object.key] == nil {
            watching[object.key] = object.$values.dropFirst().sink { [weak self] _ in
                Task { @MainActor in self?.rebuild() }
            }
        }
        rebuild()
    }

    private func rebuild() {
        let devices = SeveralDevices.connectedDevices(in: store)
        if devices != connected {
            connected = devices
        }
        let next = SeveralDevices.markers(in: store).map { marker in
            let slice = ForeignSliceMarkers.Slice(
                id: marker.sliceId, frequencyHz: marker.frequencyHz, filterLowHz: marker.filterLowHz,
                filterHighHz: marker.filterHighHz,
                colour: BandSlice.colour(forIndex: marker.sliceId, in: catalog?.sliceColours ?? []),
                ownerName: marker.ownerName, ownerShortName: marker.ownerShortName, ownerKind: marker.ownerKind,
                txSlice: marker.txSlice,
                away: marker.ownerAway)
            let owner = devices.first { $0.deviceId == marker.ownerDeviceId }
            return Entry(slice: slice, marker: marker, modeLabel: modeLabel(marker.mode),
                         awayForSeconds: owner?.state == .away ? owner?.awayForSeconds : nil)
        }
        if next != entries {
            entries = next
        }
        if let open = openNote, !next.contains(where: { $0.id == open }) {
            openNote = nil
        }
    }
}
