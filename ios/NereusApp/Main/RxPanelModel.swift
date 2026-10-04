// NereusSDR for iOS: the RX panel's controls for the active slice: the Core's values in, the operator's writes out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels
import os

/// The RX panel's quick receive subset for the active slice (R-IOS-11,
/// spec section 5.1 item 5; D15): AF gain, AGC, the filter presets, the
/// noise buttons and squelch.
///
/// Every value shown is the Core's, read from the active `SliceModel` in
/// the mirror; every range and list comes from the Core's catalogue
/// (R-IOS-27): the AF and SQL ranges from `receive`, the AGC modes from
/// `agc.modes`, the presets from `filterPresets` under the mode's label.
/// A control shows the operator's value at the touch and keeps it until
/// the Core answers, as the desktop's remote window does
/// (`StationClient.cpp:1040-1068`; ``PropertyWriteQueue``). A write the
/// Core refuses returns to the Core's value and shows its reason as sent
/// (``note``); one it has not answered in time, or that a dropped link cut
/// off, says so there in plain words.
///
/// A noise button the Core cannot run stays on the panel, greyed, with the
/// Core's reason (JJ's rule, 2026-09-25), from the mirrored
/// `DspAssetService` alone, never the phone's own build (link document
/// section 5.3, `dspAssetVersion`): NR3 from `nr3Runnable` and
/// `nr3ModelStatus`, DFNR from `dfnrRunnable` and `dfnrModelStatus`
/// (version 3), MNR from `mnrRunnable` and `mnrStatus` (version 4). A Core
/// below 3 does not say whether it runs DFNR, nor below 4 MNR: that button
/// is greyed with ``noiseReductionNotSaidText``. NNR follows the slice's
/// own `nnrAvailable` and `nnrStatus`, as the desktop's NR menu does; a
/// slice that does not carry `nnrAvailable` does not say, and NNR is
/// greyed with the same reason. A control whose property
/// this Core does not mirror is greyed with "Needs a newer Core" (D23).
@MainActor
final class RxPanelModel: ObservableObject {
    /// One filter preset button.
    struct Preset: Identifiable, Equatable {
        /// The catalogue's slot, 0 for `F1`.
        let slot: Int
        /// The catalogue's name for the preset, `F1`.
        let name: String
        /// The button's words: the preset's width, `2.9K`.
        let text: String
        let lowHz: Int64
        let highHz: Int64
        /// The slice's filter is this preset's, within 50 Hz at each edge
        /// (the desktop RX applet's match).
        let lit: Bool

        var id: Int { slot }
    }

    /// One noise button.
    struct Noise: Identifiable, Equatable {
        enum Kind: Equatable {
            /// The blanker: Off, NB, NB2 in turn (`nbMode`).
            case blanker
            /// One noise reduction: `activeNr` holds one at a time.
            case reduction(Int64)
            /// A switch of its own (`anfEnabled`, `snbEnabled`).
            case toggle(String)
        }

        let id: String
        let kind: Kind
        let label: String
        let lit: Bool
        /// Why the Core cannot run it; nil when it can.
        let reason: String?
        /// Why the Core stepped it back while it runs (NNR held at Standard
        /// or turned off), shown in the warning colour; nil otherwise.
        var warning: String? = nil
    }

    /// NNR as the desktop's NNR controls show it: the Core's step-back and
    /// its reason, Try again, and the Standard or Premium model.
    struct Nnr: Equatable {
        /// The Core's step-back (`nnrLimit`: 1 held at Standard, 2 off),
        /// 0 while none holds.
        let limit: Int64
        /// The step-back's reason in the Core's words (`nnrStatus`).
        let limitText: String?
        /// The saved model (`nnrModelSlot`: 0 Standard, 1 Premium).
        let modelSlot: Int64?
        let standardReady: Bool
        let premiumReady: Bool
        /// Why Try again cannot be sent; nil when it can.
        let tryAgainReason: String?
    }

    /// One AGC mode button.
    struct AgcChoice: Identifiable, Equatable {
        let id: Int
        let label: String
        let lit: Bool
    }

    /// The `SliceModel` property names the panel reads and writes.
    enum Property {
        static let afGain = "afGain"
        static let agcMode = "agcMode"
        static let dspMode = "dspMode"
        static let filterLow = "filterLow"
        static let filterHigh = "filterHigh"
        static let nbMode = "nbMode"
        static let activeNr = "activeNr"
        static let anfEnabled = "anfEnabled"
        static let snbEnabled = "snbEnabled"
        static let ssqlEnabled = "ssqlEnabled"
        static let ssqlThresh = "ssqlThresh"
    }

    /// The `DspAssetService` pair that says whether the Core can run a
    /// noise reduction, and the `dspAssetVersion` that brought it.
    struct AssetPair: Equatable {
        let runnable: String
        let status: String
        /// Below this version the Core does not say; 0 when no version rule applies.
        let since: Int64
        /// The desktop's words when the Core says it cannot and gives no reason
        /// (`StationClient.cpp`, `RadioModel::nrCannotRunReason`).
        let fallback: String
    }

    static let nr3Pair = AssetPair(runnable: "nr3Runnable", status: "nr3ModelStatus", since: 0,
                                   fallback: "NR3 cannot run on this Core: no NR3 model file was found.")
    static let dfnrPair = AssetPair(runnable: "dfnrRunnable", status: "dfnrModelStatus", since: 3,
                                    fallback: "DFNR cannot run on this Core.")
    static let mnrPair = AssetPair(runnable: "mnrRunnable", status: "mnrStatus", since: 4,
                                   fallback: "MNR runs only on a Mac, and this Core is not a Mac, so MNR cannot run.")

    /// A Core too old to say which noise reduction it runs (link document
    /// section 5.3; the desktop's `RadioModel::noiseReductionNotSaidReason`).
    static let noiseReductionNotSaidText =
        "This Core does not say which noise reduction it can run. Updating the Core may help."

    /// The `activeNr` values of the noise reductions on the panel, in the
    /// desktop flag's order (NereusSDR's `NrSlot`, `src/core/WdspTypes.h`):
    /// Off 0, NR1 1, NR2 2, NR3 3, NR4 4, DFNR 5, MNR 7, NNR 8. BNR (6) is
    /// left out, as on the desktop.
    static let reductions: [(label: String, slot: Int64, asset: AssetPair?)] = [
        ("NR1", 1, nil), ("NR2", 2, nil), ("NR3", 3, nr3Pair), ("NR4", 4, nil), ("DFNR", 5, dfnrPair),
        ("MNR", 7, mnrPair), ("NNR", nnrSlot, nil),
    ]
    /// NNR's `activeNr` value; whether it runs is the slice's to say.
    static let nnrSlot: Int64 = 8
    /// The slice's pair that says whether NNR can run, and why not.
    static let nnrAvailable = "nnrAvailable"
    static let nnrStatus = "nnrStatus"
    /// When the Core says NNR cannot run and gives no reason.
    static let nnrFallback = "NNR cannot run on this Core."
    /// NNR's step-back and model (link document section 7.1, the slice's
    /// `nnrLimit`, `nnrModelSlot`, `nnrStandardAvailable`, `nnrPremiumAvailable`).
    static let nnrLimit = "nnrLimit"
    static let nnrModelSlot = "nnrModelSlot"
    static let nnrStandardAvailable = "nnrStandardAvailable"
    static let nnrPremiumAvailable = "nnrPremiumAvailable"
    /// Try again: the Core runs the saved NNR choice again (`nnr.tryAgain`,
    /// `nnrVersion` 1, the agreed minor 11).
    static let nnrTryAgainVerb = "nnr.tryAgain"
    static let nnrCapability = "nnrVersion"
    static let nnrTryAgainMinor: UInt16 = 11
    /// When a step-back's reason did not arrive with it.
    static let nnrSteppedBackText = "The Core stepped noise reduction back: it could not keep up."
    static let nnrTryAgainOlderCoreText = "This Core cannot try noise reduction again. Updating the Core may help."
    /// The saved model is one the Core does not have ready.
    static let nnrModelNotReadyText = "The Core does not have this model ready, so it may not run it."
    /// Two filter edges no closer than this (the desktop's filter edge drag).
    static let minimumFilterWidthHz: Int64 = 10
    static let filterEdgeOrderText = "The low edge must be at least 10 Hz below the high edge."
    /// The mirrored object that says which models the Core can run.
    static let dspAssetsKey = "dspAssets"
    /// Two edges this close are the same preset (the desktop RX applet's rule).
    static let presetToleranceHz: Int64 = 50

    // What the panel shows.
    @Published private(set) var sliceLetter: String?
    @Published private(set) var afGain: Double?
    @Published private(set) var afRange: StationCatalog.Range?
    @Published private(set) var agc: [AgcChoice] = []
    @Published private(set) var presets: [Preset] = []
    @Published private(set) var noise: [Noise] = []
    @Published private(set) var squelchOn = false
    @Published private(set) var squelch: Double?
    @Published private(set) var squelchRange: StationCatalog.Range?
    /// The slice's filter edges, signed about the carrier.
    @Published private(set) var filterLowHz: Int64?
    @Published private(set) var filterHighHz: Int64?
    /// NNR's step-back, Try again and model; nil without a slice.
    @Published private(set) var nnr: Nnr?
    /// The number pad open on the RX panel, if any.
    @Published private(set) var pad: ValuePadModel?
    /// The Core's words for the last write it refused; cleared by the next one it takes.
    @Published private(set) var note: String?
    /// Why a control whose property this Core does not mirror is greyed.
    @Published private(set) var olderCore: Set<String> = []

    private let store: MirrorStore
    /// The band's slices: the panel follows the active one.
    let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private let commands: CommandClient?
    private var watches: Set<AnyCancellable> = []
    private var sliceWatch: AnyCancellable?
    private var assetsWatch: AnyCancellable?
    private var watchedKey: String?
    private var watchedAssets: MirrorObject?
    private lazy var writes = PropertyWriteQueue(store: store) { [weak self] _, outcome in
        self?.noteOutcome(outcome)
    }
    private var rebuildQueued = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "rx.panel")

    init(store: MirrorStore, slices: BandSlicesModel, catalogFeed: CatalogFeed, commands: CommandClient? = nil) {
        self.store = store
        self.slices = slices
        self.catalogFeed = catalogFeed
        self.commands = commands
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$unconfirmedWrites.sink { [weak self] _ in
            Task { @MainActor in self?.confirmationRevision &+= 1 }
        }.store(in: &watches)
        store.$objectKeys.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        rebuild()
    }

    /// The active slice's mirrored object.
    var slice: MirrorObject? {
        Self.sliceObject(slices.activeSliceId, in: store)
    }

    /// Slice `id`'s mirrored object, by its `sliceIndex`, else its key.
    static func sliceObject(_ id: Int?, in store: MirrorStore) -> MirrorObject? {
        guard let id else {
            return nil
        }
        let objects = store.objects(ofClass: BandSlicesModel.sliceClass)
        return objects.first { object in
            if case .int(let index)? = object["sliceIndex"] {
                return index == id
            }
            return object.key == "slice:\(id)"
        }
    }

    @Published private(set) var confirmationRevision: UInt64 = 0
    func isUnconfirmed(_ property: String) -> Bool {
        slice.map { store.isUnconfirmed($0.key, property: property) } ?? false
    }
    // MARK: Writing

    /// The AF slider moved to `value`, in the catalogue's units.
    func setAfGain(_ value: Double) {
        guard let range = afRange else {
            return
        }
        write(Property.afGain, .int(Int64(Self.snapped(value, to: range).rounded())))
    }

    /// An AGC mode button.
    func selectAgc(_ id: Int) {
        write(Property.agcMode, .enumeration(Int64(id)))
    }

    /// A filter preset button: both edges, in the order that never leaves
    /// the low edge above the high one.
    func selectPreset(_ preset: Preset) {
        guard let object = slice else {
            return
        }
        let high = Self.whole(object[Property.filterHigh]) ?? 0
        let writes: [(String, Int64)] = preset.lowHz >= high
            ? [(Property.filterHigh, preset.highHz), (Property.filterLow, preset.lowHz)]
            : [(Property.filterLow, preset.lowHz), (Property.filterHigh, preset.highHz)]
        let key = object.key
        let store = store
        // Each pre-held edge keeps its touch identity through task admission.
        let edits = writes.map { property, value in
            store.hold(key, property: property, value: .int(value))
        }
        let current = { writes.indices.allSatisfy { index in
            edits[index].map { store.isCurrent(key, property: writes[index].0, edit: $0) } ?? false
        } }
        Task { [weak self] in
            for (index, (property, value)) in writes.enumerated() {
                guard current() else { return }
                let outcome = await store.write(key, property: property, value: .int(value), edit: edits[index],
                                                onLateOutcome: { [weak self] outcome in
                    if current() { self?.noteOutcome(outcome) }
                })
                if current() { self?.noteOutcome(outcome) }
                if !outcome.accepted {
                    for next in writes.indices where next > index {
                        if let edit = edits[next], store.isCurrent(key, property: writes[next].0, edit: edit) {
                            store.releaseUnsent(key, property: writes[next].0)
                        }
                    }
                    return
                }
            }
        }
    }

    /// A noise button.
    func tap(_ button: Noise) {
        guard button.reason == nil, let object = slice else {
            return
        }
        switch button.kind {
        case .blanker:
            let mode = Self.whole(object[Property.nbMode]) ?? 0
            write(Property.nbMode, .enumeration((mode + 1) % 3))
        case .reduction(let slot):
            let current = Self.whole(object[Property.activeNr]) ?? 0
            write(Property.activeNr, .enumeration(current == slot ? 0 : slot))
        case .toggle(let property):
            write(property, .bool(!(Self.flag(object[property]) ?? false)))
        }
    }

    /// The squelch switch.
    func toggleSquelch() {
        write(Property.ssqlEnabled, .bool(!squelchOn))
    }

    /// The SQL slider moved to `value`, in the catalogue's units.
    func setSquelch(_ value: Double) {
        guard let range = squelchRange else {
            return
        }
        write(Property.ssqlThresh, .double(Self.snapped(value, to: range)))
    }

    // MARK: The filter's edges

    /// Opens the number pad for the slice's low or high filter edge.
    func openFilterEdgePad(low: Bool) {
        pad = filterEdgePad(low: low) { [weak self] in self?.closePad() }
    }

    func closePad() {
        pad = nil
    }

    /// A number pad for one of the slice's filter edges, as the desktop's
    /// typed and dragged edges set them: signed about the carrier, within
    /// half the slice's sample rate, and at least 10 Hz from the other edge
    /// on its own side. Nil without a slice, its edges or its sample rate.
    func filterEdgePad(low: Bool, close: @escaping () -> Void) -> ValuePadModel? {
        guard let object = slice, let other = Self.whole(object[low ? Property.filterHigh : Property.filterLow]),
              let rate = Self.whole(object[Self.sampleRateHz]), rate > 0 else {
            return nil
        }
        let key = object.key
        let property = low ? Property.filterLow : Property.filterHigh
        let store = store
        let half = rate / 2
        return ValuePadModel(
            title: low ? "Filter low edge" : "Filter high edge", unit: "Hz", range: -half...half,
            current: Self.whole(object[property]),
            check: { value in
                let fine = low ? value <= other - Self.minimumFilterWidthHz : value >= other + Self.minimumFilterWidthHz
                return fine ? nil : Self.filterEdgeOrderText
            },
            sendWithLate: { hz, late in
                await store.write(key, property: property, value: .int(hz), onLateOutcome: late)
            }, onOutcome: { [weak self] in self?.noteOutcome($0) },
            readCurrent: { Self.number(store.object(key)?[property]) }, close: close)
    }

    /// The slice's sample rate, which bounds its filter edges.
    static let sampleRateHz = "sampleRateHz"

    // MARK: NNR

    /// Try again: the Core runs the saved NNR choice again.
    func tryNnrAgain() {
        guard let commands, nnr?.tryAgainReason == nil, let id = slices.activeSliceId else {
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(Self.nnrTryAgainVerb,
                                                       arguments: [CommandArgument(name: "sliceId", value: .int(Int64(id)))],
                                                       timeout: .seconds(5))
                self?.noteOutcome(PropertyWriteOutcome(accepted: result.accepted, reason: result.reason, value: nil))
            } catch {
                Self.logger.info("An NNR try again had no answer from the Core")
            }
        }
    }

    /// The NNR model: 0 Standard, 1 Premium (`nnrModelSlot`).
    func selectNnrModel(_ slot: Int64) {
        guard nnr?.modelSlot != nil, nnr?.modelSlot != slot else {
            return
        }
        write(Self.nnrModelSlot, .int(slot))
    }

    /// One property write to the active slice, shown at once and sent one
    /// at a time per property, the newest value next (``PropertyWriteQueue``).
    private func write(_ property: String, _ value: MirrorValue) {
        guard let key = slice?.key else {
            return
        }
        writes.write(key, property, value)
    }

    private func noteOutcome(_ outcome: PropertyWriteOutcome) {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return }
        let text = outcome.noteText(refused: BandSlicesModel.refusedText)
        if !outcome.answeredByCore {
            Self.logger.info("An RX panel write: \(outcome.reason, privacy: .private)")
        }
        if note != text {
            note = text
        }
    }

    // MARK: Reading

    /// The store publishes before it changes, so the panel reads it on the
    /// main actor's next turn.
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

    private func watch(_ object: MirrorObject?, assets: MirrorObject?) {
        if object?.key != watchedKey {
            watchedKey = object?.key
            sliceWatch = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
        }
        if assets !== watchedAssets {
            watchedAssets = assets
            assetsWatch = assets?.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
        }
    }

    private func rebuild() {
        let object = slice
        let assets = store.object(Self.dspAssetsKey)
        watch(object, assets: assets)
        let catalog = catalogFeed.catalog

        set(\.sliceLetter, slices.activeSliceId.map(BandSlice.letter(forIndex:)))
        var missing: Set<String> = []
        func has(_ property: String) -> Bool {
            guard let object else {
                return false
            }
            if object[property] == nil {
                missing.insert(property)
                return false
            }
            return true
        }

        set(\.afRange, catalog?.receive.afGain)
        set(\.afGain, has(Property.afGain) ? Self.number(object?[Property.afGain]) : nil)

        let agcMode = has(Property.agcMode) ? Self.whole(object?[Property.agcMode]) : nil
        set(\.agc, (catalog?.agc.modes ?? []).map { AgcChoice(id: $0.id, label: $0.label, lit: Int64($0.id) == agcMode) })

        let mode = Int(Self.whole(object?[Property.dspMode]) ?? -1)
        let label = catalog?.modes.first(where: { $0.id == mode })?.label
        let low = Self.whole(object?[Property.filterLow])
        let high = Self.whole(object?[Property.filterHigh])
        set(\.filterLowHz, low)
        set(\.filterHighHz, high)
        let presets = label.flatMap { catalog?.filterPresets[$0] } ?? []
        set(\.presets, presets.map { preset in
            let presetLow = Int64(preset.lowHz.rounded())
            let presetHigh = Int64(preset.highHz.rounded())
            let lit = low.map { abs($0 - presetLow) <= Self.presetToleranceHz } == true
                && high.map { abs($0 - presetHigh) <= Self.presetToleranceHz } == true
            return Preset(slot: preset.slot, name: preset.label, text: BandSlice.widthText(hz: preset.highHz - preset.lowHz),
                          lowHz: presetLow, highHz: presetHigh, lit: lit)
        })

        var noise: [Noise] = []
        let nbMode = has(Property.nbMode) ? Self.whole(object?[Property.nbMode]) : nil
        noise.append(Noise(id: "NB", kind: .blanker, label: nbMode == 2 ? "NB2" : "NB", lit: (nbMode ?? 0) != 0,
                           reason: object == nil || nbMode != nil ? nil : CatalogFeed.needsNewerCoreText))
        let activeNr = has(Property.activeNr) ? Self.whole(object?[Property.activeNr]) : nil
        let dspAssetVersion = store.capabilityVersion("dspAssetVersion")
        let nnrState = Self.nnrState(object, store: store)
        set(\.nnr, nnrState)
        for reduction in Self.reductions {
            let reason: String?
            if object != nil && activeNr == nil {
                reason = CatalogFeed.needsNewerCoreText
            } else if reduction.slot == Self.nnrSlot {
                reason = Self.nnrCannotRun(object)
            } else {
                reason = reduction.asset.flatMap {
                    Self.cannotRun($0, assets: assets, dspAssetVersion: dspAssetVersion)
                }
            }
            // The desktop's amber mark on NNR while the Core holds it back.
            let warning = reduction.slot == Self.nnrSlot && reason == nil && (nnrState?.limit ?? 0) != 0
                ? nnrState?.limitText : nil
            noise.append(Noise(id: reduction.label, kind: .reduction(reduction.slot), label: reduction.label,
                               lit: activeNr == reduction.slot, reason: reason, warning: warning))
        }
        for (label, property) in [("ANF", Property.anfEnabled), ("SNB", Property.snbEnabled)] {
            let on = has(property) ? Self.flag(object?[property]) : nil
            noise.append(Noise(id: label, kind: .toggle(property), label: label, lit: on == true,
                               reason: object == nil || on != nil ? nil : CatalogFeed.needsNewerCoreText))
        }
        set(\.noise, noise)

        set(\.squelchRange, catalog?.receive.ssqlThresh)
        set(\.squelchOn, (has(Property.ssqlEnabled) ? Self.flag(object?[Property.ssqlEnabled]) : nil) ?? false)
        set(\.squelch, has(Property.ssqlThresh) ? Self.number(object?[Property.ssqlThresh]) : nil)
        set(\.olderCore, missing)
    }

    /// Why the Core cannot run a noise reduction, from its mirrored pair,
    /// or nil when it can: a Core below the pair's `dspAssetVersion` does
    /// not say, and one that says it cannot gives its status, else the
    /// desktop's words.
    static func cannotRun(_ pair: AssetPair, assets: MirrorObject?, dspAssetVersion: Int64) -> String? {
        if dspAssetVersion < pair.since {
            return noiseReductionNotSaidText
        }
        guard case .bool(false)? = assets?[pair.runnable] else {
            return nil
        }
        if case .text(let status)? = assets?[pair.status], !status.isEmpty {
            return status
        }
        return pair.fallback
    }

    /// NNR's step-back, model and Try again on the slice, as the Core
    /// mirrors them; nil without a slice. While a step-back holds, the
    /// Core words `nnrStatus` as its reason, naming the Core computer.
    static func nnrState(_ slice: MirrorObject?, store: MirrorStore) -> Nnr? {
        guard let slice else {
            return nil
        }
        let limit = whole(slice[nnrLimit]) ?? 0
        var limitText: String?
        if limit != 0 {
            if case .text(let status)? = slice[nnrStatus], !status.isEmpty {
                limitText = status
            } else {
                limitText = nnrSteppedBackText
            }
        }
        let offered = (store.agreedMinor ?? 0) >= nnrTryAgainMinor && store.capabilityVersion(nnrCapability) >= 1
        return Nnr(limit: limit, limitText: limitText, modelSlot: whole(slice[nnrModelSlot]),
                   standardReady: flag(slice[nnrStandardAvailable]) ?? false,
                   premiumReady: flag(slice[nnrPremiumAvailable]) ?? false,
                   tryAgainReason: offered ? nil : nnrTryAgainOlderCoreText)
    }

    /// Why NNR cannot run on the slice, or nil when it can: the slice's
    /// `nnrAvailable` and `nnrStatus`, as the Core sends them.
    static func nnrCannotRun(_ slice: MirrorObject?) -> String? {
        guard let slice else {
            return nil
        }
        switch slice[nnrAvailable] {
        case .bool(true)?:
            return nil
        case .bool(false)?:
            if case .text(let status)? = slice[nnrStatus], !status.isEmpty {
                return status
            }
            return nnrFallback
        default:
            return noiseReductionNotSaidText
        }
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<RxPanelModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }

    // MARK: Values

    static func snapped(_ value: Double, to range: StationCatalog.Range) -> Double {
        let clamped = min(max(value, range.min), range.max)
        guard range.step > 0 else {
            return clamped
        }
        return min(range.min + ((clamped - range.min) / range.step).rounded() * range.step, range.max)
    }

    static func number(_ value: MirrorValue?) -> Double? {
        switch value {
        case .double(let number)?:
            return number
        case .int(let number)?, .enumeration(let number)?:
            return Double(number)
        default:
            return nil
        }
    }

    static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let number)?, .enumeration(let number)?:
            return number
        case .double(let number)? where number.isFinite:
            return Int64(number.rounded())
        default:
            return nil
        }
    }

    static func flag(_ value: MirrorValue?) -> Bool? {
        if case .bool(let on)? = value {
            return on
        }
        return nil
    }
}
