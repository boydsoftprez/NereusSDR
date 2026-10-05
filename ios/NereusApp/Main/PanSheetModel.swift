// NereusSDR for iOS: the Pan 1 sheet's state: the Core's band grid, a slice or a notch added here, and the extended view
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMedia
import NereusMirror
import NereusModels
import os

/// What the Pan 1 sheet shows and sends (R-IOS-11, R-IOS-27, D73; spec
/// section 5.1 items 14 and 15).
///
/// The band grid is the Core's catalogue `bands`, in its order, with its
/// labels; the lit button is the one whose id is the active slice's `band`
/// as the Core reports it. A tap asks the Core to open the band
/// (`slice.selectBand`, which restores the slice's last frequency, mode and
/// filter there). Add a slice here asks for a slice on this pan
/// (`addSliceOnPan`); Add a notch asks the Core to place one at the active
/// slice (`notch.addAtSlice`), so the phone computes no notch itself (D4).
/// The extended view is this phone's own setting for the pan, sent in the
/// pan's display subscription.
///
/// A control this Core cannot run stays on the sheet, greyed, with "Needs a
/// newer Core" (D23). A request the Core refuses shows its words as sent
/// (``note``).
@MainActor
final class PanSheetModel: ObservableObject {
    /// The band grid's state.
    enum BandGrid: Equatable {
        /// The Core's catalogue has no `bands`: one greyed row.
        case needsNewerCore
        /// The Core's buttons; greyed when it cannot open a band for the phone.
        case buttons([BandButton], enabled: Bool)
    }

    /// One band button.
    struct BandButton: Identifiable, Equatable {
        let id: Int
        let label: String
        let lit: Bool
    }

    /// The verbs, as the Core names them.
    static let selectBandVerb = "slice.selectBand"
    static let addSliceVerb = "addSliceOnPan"
    static let addNotchVerb = "notch.addAtSlice"
    /// The capabilities that gate them.
    static let bandSelectCapability = "bandSelectVersion"
    static let notchControlCapability = "notchControlVersion"
    /// `notch.addAtSlice` came with `notchControlVersion` 2.
    static let notchAtSliceVersion: Int64 = 2
    static let commandTimeout: Duration = .seconds(5)
    static let noSliceReason = "Add a slice to use this control."

    @Published private(set) var sliceLetter: String?
    @Published private(set) var bandGrid: BandGrid = .needsNewerCore
    /// This phone's exact Core key while its pan has a slice, or the
    /// Core's first-pan key when no owned slice has supplied one.
    @Published private(set) var panKey = "pan-0"
    @Published private(set) var canAddNotch = false
    @Published private(set) var canAddSlice = false
    @Published private(set) var extendedViewOn = false
    @Published private(set) var extendedViewAvailable = false
    /// The Core's words for the last request it refused; cleared by the next one it takes.
    @Published private(set) var note: String?

    private let store: MirrorStore
    private let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private let commands: CommandClient?
    private let band: BandModel
    private let changeDisplay: (_ change: (inout BandDisplaySettings) -> Void) -> Void
    private var watches: Set<AnyCancellable> = []
    private var sliceWatch: AnyCancellable?
    private var watchedKey: String?
    private var rebuildQueued = false
    private var requestGeneration = 0
    private static let logger = Logger(subsystem: "NereusSDR", category: "pan.sheet")

    init(store: MirrorStore, slices: BandSlicesModel, catalogFeed: CatalogFeed, commands: CommandClient?,
         band: BandModel, changeDisplay: @escaping (_ change: (inout BandDisplaySettings) -> Void) -> Void) {
        self.store = store
        self.slices = slices
        self.catalogFeed = catalogFeed
        self.commands = commands
        self.band = band
        self.changeDisplay = changeDisplay
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$isSnapshotComplete.dropFirst().sink { [weak self] complete in
            guard let self else { return }
            if !complete {
                self.requestGeneration &+= 1
                self.set(\.panKey, "pan-0")
                self.set(\.note, nil)
            }
            self.queueRebuild()
        }.store(in: &watches)
        store.$objectKeys.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        band.$settings.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        rebuild()
    }

    /// The media gates the Core and the agreed minor allow.
    private var gates: MediaFeatureGates {
        MediaFeatureGates(agreedMinor: store.agreedMinor ?? 0) { [store] in store.capabilityVersion($0) }
    }

    /// The Core opens a band for the phone: `bandSelectVersion` 1 or more.
    var canSelectBand: Bool {
        store.isSnapshotComplete && slices.activeSliceId != nil &&
            store.capabilityVersion(Self.bandSelectCapability) >= 1
    }

    var bandUnavailableReason: String {
        slices.activeSliceId == nil || !store.isSnapshotComplete ? Self.noSliceReason : CatalogFeed.needsNewerCoreText
    }

    var notchUnavailableReason: String {
        slices.activeSliceId == nil || !store.isSnapshotComplete ? Self.noSliceReason : CatalogFeed.needsNewerCoreText
    }

    var addSliceUnavailableReason: String { "Wait for the Core to connect." }

    // MARK: Asking the Core

    /// A band button: the Core opens the band on the active slice. The lit
    /// button changes only when the Core reports the slice's new band.
    func selectBand(_ id: Int) {
        // Only a band the Core's grid lists: a Core without 2 m (no
        // `band2mVersion`) lists none numbered 27, so it is never sent one.
        guard canSelectBand, let sliceId = slices.activeSliceId,
              catalogFeed.catalog?.bands?.contains(where: { $0.id == id }) == true else {
            return
        }
        invoke(Self.selectBandVerb, [CommandArgument(name: "sliceId", value: .int(Int64(sliceId))),
                                     CommandArgument(name: "band", value: .int(Int64(id)))])
    }

    /// Add a slice here: a slice on this pan.
    func addSlice() {
        guard canAddSlice else {
            return
        }
        invoke(Self.addSliceVerb, [CommandArgument(name: "panId", value: .text(panKey))])
    }

    /// Add a notch at the active slice, where the Core places it.
    func addNotch() {
        guard canAddNotch, let sliceId = slices.activeSliceId else {
            return
        }
        invoke(Self.addNotchVerb, [CommandArgument(name: "sliceId", value: .int(Int64(sliceId)))])
    }

    /// The extended view's On and Off, kept for this pan on this phone.
    func toggleExtendedView() {
        guard extendedViewAvailable else {
            return
        }
        changeDisplay { $0.extendedView.toggle() }
    }

    private func invoke(_ verb: String, _ arguments: [CommandArgument]) {
        guard let commands else {
            return
        }
        let generation = requestGeneration
        Task {
            guard generation == self.requestGeneration else { return }
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: Self.commandTimeout)
                guard generation == self.requestGeneration else { return }
                if result.accepted {
                    if self.note != nil {
                        self.note = nil
                    }
                } else {
                    self.note = result.reason.isEmpty ? "The Core refused this request." : result.reason
                }
            } catch {
                guard generation == self.requestGeneration else { return }
                Self.logger.info("\(verb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
                switch error as? CommandError {
                case .notSent: self.note = "The request could not be sent to the Core."
                case .linkLost: self.note = "The connection to the Core was lost."
                case .timedOut: self.note = "The Core did not answer this request."
                case nil: self.note = "The request failed: \(error.localizedDescription)"
                }
            }
        }
    }

    // MARK: Reading

    private func queueRebuild() {
        guard !rebuildQueued else {
            return
        }
        rebuildQueued = true
        Task { @MainActor [weak self] in
            self?.rebuildQueued = false
            self?.rebuild()
        }
    }

    /// The active slice's mirrored object.
    private var slice: MirrorObject? {
        guard let id = slices.activeSliceId else {
            return nil
        }
        return store.objects(ofClass: BandSlicesModel.sliceClass).first { object in
            if case .int(let index)? = object["sliceIndex"] {
                return index == Int64(id)
            }
            return object.key == "slice:\(id)"
        }
    }

    private func rebuild() {
        let object = store.isSnapshotComplete ? slice : nil
        if object?.key != watchedKey {
            watchedKey = object?.key
            sliceWatch = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
        }
        set(\.sliceLetter, store.isSnapshotComplete ? slices.activeSliceId.map(BandSlice.letter(forIndex:)) : nil)

        let current = RxPanelModel.whole(object?["band"])
        if let bands = catalogFeed.catalog?.bands {
            let buttons = bands.map { BandButton(id: $0.id, label: $0.label, lit: current == Int64($0.id)) }
            set(\.bandGrid, .buttons(buttons, enabled: canSelectBand))
        } else {
            set(\.bandGrid, .needsNewerCore)
        }

        if store.isSnapshotComplete, case .text(let key)? = object?["panKey"], !key.isEmpty {
            set(\.panKey, key)
        }
        set(\.canAddNotch, store.isSnapshotComplete && slices.activeSliceId != nil &&
            store.capabilityVersion(Self.notchControlCapability) >= Self.notchAtSliceVersion)
        set(\.canAddSlice, commands != nil && store.isSnapshotComplete)
        let wideband = gates.wideband
        set(\.extendedViewAvailable, wideband)
        set(\.extendedViewOn, wideband && band.settings.extendedView)
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<PanSheetModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
