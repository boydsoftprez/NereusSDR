// NereusSDR for iOS: the Modes tab's controls for the active slice: the Core's values in, each write to its owner
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels
import os

/// The Modes tab's full set for the active slice (R-IOS-18, D15, D17, spec
/// section 5.2 item 1), beyond what it shares with the RX panel
/// (``RxPanelModel``: AF gain, the AGC modes, the filter presets, the noise
/// buttons and squelch) and the TX panel (``TransmitModel``: PROC, VOX and
/// MON).
///
/// Every value shown is the Core's, and every list and range comes from
/// the Core's catalogue (R-IOS-27): the modes, the preamp items, the step
/// attenuator's range and the antenna lists. Each control writes to its
/// owner: a slice setting to the slice, the preamp and step attenuator to
/// the `stepAtt` object, a TX antenna by the Core's `setAlexTxAntenna`, a
/// transmit setting to `transmit`. A property write shows the operator's
/// value at the touch and keeps it until the Core answers, as the
/// desktop's remote window does (`StationClient.cpp:1040-1068`;
/// ``PropertyWriteQueue``); a refusal returns to the Core's value and
/// shows the Core's words as sent (``note``), and a write not answered in
/// time, or cut off by a dropped link, says so there in plain words.
///
/// A control that cannot run stays on the page, greyed, with its reason.
@MainActor
final class ModesTabModel: ObservableObject {
    /// One slice on the slice switch.
    struct SliceChoice: Identifiable, Equatable {
        let id: Int
        let letter: String
        let colour: String
        let frequencyText: String
        let active: Bool
        /// This phone only listens to it: drawn dashed in its colour.
        var listening = false
    }

    /// A choice with the Core's id and label: a mode, a preamp item.
    struct Choice: Identifiable, Equatable {
        let id: Int
        let label: String
        let lit: Bool
    }

    /// One antenna button, by the catalogue's label; `number` counts from 1
    /// in the catalogue's order.
    struct Antenna: Identifiable, Equatable {
        let label: String
        let number: Int
        let lit: Bool

        var id: String { label }
    }

    /// The `SliceModel` properties the tab reads and writes beyond the RX panel's.
    enum Property {
        static let dspMode = "dspMode"
        static let filterLow = "filterLow"
        static let filterHigh = "filterHigh"
        static let rxAntenna = "rxAntenna"
        static let txAntenna = "txAntenna"
        static let band = "band"
        static let stepHz = "stepHz"
        static let agcThreshold = "agcThreshold"
        static let autoAgcEnabled = "autoAgcEnabled"
        static let apfEnabled = "apfEnabled"
        static let audioPan = "audioPan"
        static let muted = "muted"
        static let binauralEnabled = "binauralEnabled"
        static let ritEnabled = "ritEnabled"
        static let ritHz = "ritHz"
        static let xitEnabled = "xitEnabled"
        static let xitHz = "xitHz"
        static let apfTuneHz = "apfTuneHz"
        static let diglOffsetHz = "diglOffsetHz"
        static let diguOffsetHz = "diguOffsetHz"
        static let rttyMarkHz = "rttyMarkHz"
        static let rttyShiftHz = "rttyShiftHz"
        static let outputRoute = "outputRoute"
        static let autoAgcOffset = "autoAgcOffset"
        static let noiseFloorDbm = "stationAutoAgcNoiseFloorDbm"
        static let noiseFloorValid = "stationAutoAgcNoiseFloorValid"
    }

    /// The `stepAtt` object's properties (link document section 7.1).
    enum FrontEnd {
        static let key = "stepAtt"
        static let preampMode = "preampMode"
        static let attenuationDb = "attenuationDb"
        /// The step attenuator on (S-ATT), rather than the preamp choices (ATT).
        static let enabled = "enabled"
        /// Automatic attenuation on an ADC overload (A-ATT, with the step attenuator on).
        static let autoAttEnabled = "autoAttEnabled"
        /// The second ADC's preamp.
        static let rx1Preamp = "rx1Preamp"
        /// The Core's live attenuator range.
        static let minDb = "minDb"
        static let maxDb = "maxDb"
        // RX2's own input (`adcAttenuators` 1, link document section 7.1):
        // a slice whose bit is set in `rx2SliceMask` is on the other ADC and
        // reads and sets these instead (the desktop RX applet's
        // showStepAttValueForSlice and its preamp combo).
        static let rx2SliceMask = "rx2SliceMask"
        static let rx2AttenuationDb = "rx2AttenuationDb"
        static let rx2StepAttEnabled = "rx2StepAttEnabled"
        static let rx2AutoAttEnabled = "rx2AutoAttEnabled"
        /// RX2's own preamp mode (`radioHardwareVersion` 12).
        static let rx2PreampMode = "rx2PreampMode"
    }

    /// The Alex antennas' RX bypass on transmit (`alexAntennas.rxOutOnTx`,
    /// `radioHardwareVersion` 5, the agreed minor 11).
    enum Bypass {
        static let key = "alexAntennas"
        static let property = "rxOutOnTx"
        static let version: Int64 = 5
        /// The Core changes its radio's hardware settings at all from `radioHardwareVersion` 2.
        static let hardwareVersion: Int64 = 2
    }

    /// The front end's three ways, as the desktop RX applet names them:
    /// ATT the preamp choices, S-ATT the step attenuator, A-ATT the step
    /// attenuator set automatically on an overload.
    enum AttenuatorWay: String, CaseIterable, Identifiable {
        case att = "ATT"
        case stepAtt = "S-ATT"
        case autoAtt = "A-ATT"

        var id: String { rawValue }
    }

    /// The desktop flag's ranges and steps for the offsets it moves: RIT
    /// and XIT, APF tune, the DIG offset, and RTTY mark and shift. The
    /// Core takes any whole number; these are the flag's own limits.
    static let offsetRange: ClosedRange<Int64> = -10_000...10_000
    static let apfTuneRange = StationCatalog.Range(min: -500, max: 500, step: 1)
    static let digStepHz: Int64 = 10
    static let rttyMarkRange: ClosedRange<Int64> = 1_000...3_500
    static let rttyMarkStepHz: Int64 = 25
    static let rttyShiftRange: ClosedRange<Int64> = 50...1_000
    static let rttyShiftStepHz: Int64 = 5
    /// The slice's `outputRoute`: the Core's speakers or its headphones.
    static let speakersRoute: Int64 = 0
    static let phonesRoute: Int64 = 1

    /// The `transmit` object's properties the Transmit section reads and writes.
    enum Transmit {
        static let key = "transmit"
        static let filterLow = "filterLow"
        static let filterHigh = "filterHigh"
        static let micGainDb = "micGainDb"
        static let cpdrLevelDb = "cpdrLevelDb"
        static let txLevelerOn = "txLevelerOn"
        static let txEqEnabled = "txEqEnabled"
        static let cfcEnabled = "cfcEnabled"
    }

    /// One band's TX antenna, applied by the Core as its own Antenna
    /// Control grid applies it (link document sections 6.3 and 9.1:
    /// `radioHardwareVersion` 6, agreed minor 11).
    static let txAntennaVerb = "setAlexTxAntenna"
    static let txAntennaCapability = "radioHardwareVersion"
    static let txAntennaVersion: Int64 = 6
    static let txAntennaMinor: UInt16 = 11
    /// The same change for the connected radio by its address, on a Core
    /// that declares radio-bound antenna rows (`radioAntennaRowsVersion` 1
    /// at agreed minor 11), as the desktop's remote sends it
    /// (StationClient.cpp:5524-5545, `requestAlexTxAntenna`).
    static let txAntennaForRadioVerb = "setAlexTxAntennaForRadio"
    static let antennaRowsCapability = "radioAntennaRowsVersion"
    static let antennaRowsMinor: UInt16 = 11
    /// The desktop's words when the Core has not named the connected radio
    /// (StationClient.cpp:5531).
    static let radioNotReadyReason = "The Core's connected radio is not ready for this antenna change."
    static let commandTimeout: Duration = .seconds(5)

    /// The Core's own ranges for the two transmit sliders, as the link
    /// document states them (section 7.1, `transmit` at
    /// `transmitSettingsVersion` 2): `micGainDb` -50 to 70 dB and
    /// `cpdrLevelDb` 0 to 20 dB; the Core refuses anything outside with
    /// its own words. The mic gain slider takes the board's own range from
    /// the catalogue (`board.transmit.micGainDb`) where the Core sends it,
    /// and this one from a Core that does not.
    static let micGainFallback = StationCatalog.Range(min: -50, max: 70, step: 1)
    static let procLevelRange = StationCatalog.Range(min: 0, max: 20, step: 1)
    /// A slice's `audioPan` travels from -1 (left) to 1 (right); the slider
    /// moves it in the desktop RX applet's hundred steps.
    static let panRange = StationCatalog.Range(min: -1, max: 1, step: 0.02)

    // Reasons a control is greyed, in plain operator words.
    static let noAttenuatorText = "This radio has no step attenuator."
    static let noPreampText = "This radio has no preamp choices."
    static let frontEndNotReadyText = "The Core has no attenuator settings ready."
    static let noAntennaListText = "The Core does not list this radio's antennas."
    static let apfTuneText = "APF tune works in CW with APF on."
    /// The CFC compression bars: no Core sends them to a remote device, so
    /// no update is suggested (M10).
    static let cfcBarsNotOnLinkText = "This Core does not send the CFC bars."
    /// The prefix the catalogue's CW mode labels share (`CWL`, `CWU`).
    static let cwLabelPrefix = "CW"
    /// The catalogue's digital mode labels, whose offset and RTTY settings the flag shows.
    static let diglLabel = "DIGL"
    static let diguLabel = "DIGU"
    static let digOffsetText = "The DIG offset works in DIGL and DIGU."
    static let rttyText = "Mark and shift work in DIGL."
    static let attOffText = "The step attenuator is off: choose S-ATT or A-ATT to use it."
    static let preampWithStepAttText = "The preamp choices apply with the step attenuator off (ATT)."
    static let rx1PreampOlderCoreText = CatalogFeed.needsNewerCoreText
    /// A slice on the other input on a Core that does not send RX2's own
    /// preamp (the desktop's IStationLink::rx2PreampModeUnavailableReason).
    static let rx2PreampOlderCoreText =
        "This Core cannot change the preamp of this slice's receiver input for this app. Updating the Core may help."
    /// A slice on the other input whose radio has RX2's own attenuator but
    /// no RX2 preamp list in the catalogue, and no reason from the Core. The
    /// row's note puts it after "Preamp: " (``FrontEndSection/preampNote(_:)``),
    /// so it reads "Preamp: none on this receiver input." (JJ, 2026-09-30).
    static let rx2NoPreampText = "none on this receiver input."
    static let hardwareOlderCoreText =
        "This Core cannot change its radio's hardware settings for this phone. Updating the Core may help."
    static let bypassOlderCoreText =
        "This Core cannot switch its receive bypass on transmit for this phone. Updating the Core may help."
    /// The AUTO line while the Core has not measured the noise floor.
    static let noiseFloorWaitingText = "NF awaiting measurement"

    // The slice switch, mode and filter.
    @Published private(set) var sliceChoices: [SliceChoice] = []
    @Published private(set) var modes: [Choice] = []
    /// The active slice's mode, as the catalogue names it: the filter's heading.
    @Published private(set) var modeLabel: String?
    @Published private(set) var filterLowHz: Int64?
    @Published private(set) var filterHighHz: Int64?

    // The front end.
    @Published private(set) var preamp: [Choice] = []
    @Published private(set) var preampReason: String?
    @Published private(set) var attenuationDb: Int64?
    @Published private(set) var attenuatorRange: StationCatalog.Range?
    @Published private(set) var attenuatorReason: String?
    /// ATT, S-ATT or A-ATT, as the Core's `stepAtt` holds it; nil until it says.
    @Published private(set) var attenuatorWay: AttenuatorWay?
    /// Why the three ways cannot change (the step attenuator's own reason).
    @Published private(set) var attenuatorWayReason: String?
    /// Whether the active slice is on the other input (its bit in
    /// `stepAtt.rx2SliceMask`): the preamp, ATT, S-ATT, A-ATT and step
    /// attenuator are then RX2's own.
    @Published private(set) var onRx2Input = false
    /// The second ADC's preamp, and why it cannot change.
    @Published private(set) var rx1Preamp: Bool?
    @Published private(set) var rx1PreampReason: String?
    /// RX bypass on transmit, and why it cannot change.
    @Published private(set) var bypass: Bool?
    @Published private(set) var bypassReason: String?
    /// Whether the radio has the second ADC's preamp, and the RX bypass
    /// relay. A control for hardware the radio lacks is hidden, as on the
    /// desktop (JJ, 2026-09-26), by the catalogue's `board.rx1Preamp` and
    /// `board.relays.rxOutOnTx`; a Core that does not say keeps it, greyed
    /// with its reason where it cannot change.
    @Published private(set) var rx1PreampPresent = true
    @Published private(set) var bypassPresent = true
    @Published private(set) var rxAntennas: [Antenna] = []
    @Published private(set) var rxOnlyInputs: [Antenna] = []
    @Published private(set) var rxAntennaReason: String?
    @Published private(set) var txAntennas: [Antenna] = []
    @Published private(set) var txAntennaReason: String?

    // AGC-T and AUTO, APF, the audio switches, RIT and XIT.
    @Published private(set) var agcThreshold: Double?
    @Published private(set) var agcThresholdRange: StationCatalog.Range?
    @Published private(set) var autoAgc: Bool?
    /// What AUTO chose, as the desktop flag says it while AUTO is on:
    /// "NF −110 dB · offset +8", or that the Core has not measured yet.
    @Published private(set) var autoAgcInfo: String?
    @Published private(set) var apf: Bool?
    @Published private(set) var apfReason: String?
    @Published private(set) var apfTuneHz: Double?
    /// Why APF tune cannot move: it works in CW with APF on.
    @Published private(set) var apfTuneReason: String?
    /// The DIG offset for the slice's DIGL or DIGU, and why it cannot change.
    @Published private(set) var digOffsetHz: Int64?
    @Published private(set) var digOffsetReason: String?
    /// RTTY mark and shift (DIGL), and why they cannot change.
    @Published private(set) var rttyMarkHz: Int64?
    @Published private(set) var rttyShiftHz: Int64?
    @Published private(set) var rttyReason: String?
    /// The slice's output at the Core: its speakers (0) or its headphones (1).
    @Published private(set) var outputRoute: Int64?
    @Published private(set) var pan: Double?
    @Published private(set) var muted: Bool?
    @Published private(set) var binaural: Bool?
    @Published private(set) var ritOn: Bool?
    @Published private(set) var ritHz: Int64?
    @Published private(set) var xitOn: Bool?
    @Published private(set) var xitHz: Int64?
    @Published private(set) var stepHz: Int64?

    // Transmit.
    @Published private(set) var txFilterLowHz: Int64?
    @Published private(set) var txFilterHighHz: Int64?
    @Published private(set) var micGainDb: Double?
    /// The mic gain slider's range on this radio.
    @Published private(set) var micGainRange = ModesTabModel.micGainFallback
    @Published private(set) var procLevelDb: Double?
    @Published private(set) var leveler: Bool?
    @Published private(set) var eq: Bool?
    @Published private(set) var cfc: Bool?

    /// NNR's settings on the slice, by property; the Noise section's NNR
    /// settings group shows them (``NnrSettingsSection``) on a Core whose
    /// catalogue does not describe them.
    @Published private(set) var nnrSettings: [String: Double] = [:]
    /// The noise reductions' settings as the Core describes them (the
    /// catalogue's `noiseReduction`); nil from a Core that does not.
    @Published private(set) var noiseReduction: StationCatalog.NoiseReduction?
    /// The active slice's values of those settings, by property; one the
    /// slice does not carry is absent.
    @Published private(set) var nrValues: [String: MirrorValue] = [:]
    /// The number pad open on the page, if any.
    @Published private(set) var pad: ValuePadModel?

    /// The Core's words for the last change it refused; cleared by the next one it takes.
    @Published private(set) var note: String?
    /// The slice and transmit properties this Core does not mirror.
    @Published private(set) var olderCore: Set<String> = []

    let rx: RxPanelModel
    let transmit: TransmitModel
    private let store: MirrorStore
    /// The band's slices: the Modes tab follows the active one (and the owner block reads it).
    let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private let commands: CommandClient?
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watchedObjects: [String: ObjectIdentifier] = [:]
    private lazy var writes = PropertyWriteQueue(store: store) { [weak self] _, outcome in
        self?.noteOutcome(outcome.accepted, outcome.reason, answered: outcome.answeredByCore)
    }
    private lazy var transmitWrites = PropertyWriteQueue(store: store) { [weak self] _, outcome in
        self?.noteOutcome(outcome.accepted, outcome.reason, answered: outcome.answeredByCore)
        // Shown where the control is, too: the TX panel and the Transmit section.
        self?.transmit.noteSettingOutcome(outcome.accepted, outcome.reason,
                                         answered: outcome.answeredByCore)
    }
    private lazy var commandHolds = CommandHoldQueue(store: store)
    private var rebuildQueued = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "modes.tab")

    init(store: MirrorStore, slices: BandSlicesModel, catalogFeed: CatalogFeed, commands: CommandClient?,
         rx: RxPanelModel, transmit: TransmitModel) {
        self.store = store
        self.slices = slices
        self.catalogFeed = catalogFeed
        self.commands = commands
        self.rx = rx
        self.transmit = transmit
        // The TX panel's power ranges come from the same catalogue.
        catalogFeed.$catalog.sink { [weak transmit] catalog in transmit?.catalog = catalog }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$unconfirmedWrites.sink { [weak self] _ in
            Task { @MainActor in self?.confirmationRevision &+= 1 }
        }.store(in: &watches)
        store.$objectKeys.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        store.$isSnapshotComplete.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRebuild() }.store(in: &watches)
        rebuild()
    }

    /// The active slice's mirrored object (the RX panel's).
    var slice: MirrorObject? {
        rx.slice
    }

    /// The Core takes antenna changes for the connected radio by its
    /// address: `radioAntennaRowsVersion` exactly 1 at agreed minor 11, the
    /// desktop's test (StationClient.cpp:5531-5532).
    var radioBoundAntennas: Bool {
        (store.agreedMinor ?? 0) >= Self.antennaRowsMinor && store.capabilities[Self.antennaRowsCapability] == .int(1)
    }

    /// The connected radio's address as the Core names it (`macAddress`),
    /// only in its own form, upper case in colon-separated pairs, as the
    /// desktop's AppSettings::normalizedRadioMac writes it; nil otherwise.
    var connectedRadioMac: String? {
        if store.capabilities["radioConnected"] == .bool(false) {
            return nil
        }
        guard case .text(let mac)? = store.capabilities["macAddress"] else {
            return nil
        }
        return SetupSpecializedWire.canonicalMac(mac) ? mac : nil
    }

    /// The Core takes one band's TX antenna by `setAlexTxAntenna`.
    var txAntennaByBand: Bool {
        (store.agreedMinor ?? 0) >= Self.txAntennaMinor
            && store.capabilityVersion(Self.txAntennaCapability) >= Self.txAntennaVersion
    }

    @Published private(set) var confirmationRevision: UInt64 = 0
    func isUnconfirmed(_ property: String, transmit: Bool = false) -> Bool {
        let key = transmit ? Transmit.key : slice?.key
        return key.map { store.isUnconfirmed($0, property: property) } ?? false
    }
    // MARK: Writing

    /// The slice switch: that slice becomes active, as a tap on its flag does.
    func selectSlice(_ id: Int) {
        slices.activate(id)
    }

    func selectMode(_ id: Int) {
        writeSlice(Property.dspMode, .enumeration(Int64(id)))
    }

    func selectPreamp(_ id: Int) {
        guard preampReason == nil else {
            return
        }
        write(FrontEnd.key, onRx2Input ? FrontEnd.rx2PreampMode : FrontEnd.preampMode, .int(Int64(id)))
    }

    /// The step attenuator's arrows: one step of the Core's range, within it.
    func stepAttenuator(up: Bool) {
        guard attenuatorReason == nil, let range = attenuatorRange, let current = attenuationDb else {
            return
        }
        let step = range.step > 0 ? range.step : 1
        let next = RxPanelModel.snapped(Double(current) + (up ? step : -step), to: range)
        guard Int64(next.rounded()) != current else {
            return
        }
        writeAttenuation(Int64(next.rounded()))
    }

    /// The step attenuator of the active slice's input. RX2's goes as the
    /// pair the Core asks for: `rx2StepAttEnabled` true, then
    /// `rx2AttenuationDb` (catalogue `board.rx2Attenuator`).
    private func writeAttenuation(_ db: Int64) {
        if onRx2Input {
            write(FrontEnd.key, FrontEnd.rx2StepAttEnabled, .bool(true))
            write(FrontEnd.key, FrontEnd.rx2AttenuationDb, .int(db))
        } else {
            write(FrontEnd.key, FrontEnd.attenuationDb, .int(db))
        }
    }

    /// The typed step attenuator, answered: RX2's pair, or slice A's input.
    private func writeAttenuationNow(_ db: Int64, onLateOutcome: @escaping @MainActor (PropertyWriteOutcome) -> Void) async -> PropertyWriteOutcome {
        guard onRx2Input else {
            return await writeNow(FrontEnd.key, FrontEnd.attenuationDb, .int(db), onLateOutcome: onLateOutcome)
        }
        let enabled = await writeNow(FrontEnd.key, FrontEnd.rx2StepAttEnabled, .bool(true), onLateOutcome: { outcome in
            // Enabling RX2 alone is not completion of the typed attenuation.
            if !outcome.accepted { onLateOutcome(outcome) }
        })
        guard enabled.accepted else {
            return enabled
        }
        return await writeNow(FrontEnd.key, FrontEnd.rx2AttenuationDb, .int(db), onLateOutcome: onLateOutcome)
    }

    /// ATT, S-ATT or A-ATT: the step attenuator off, on, or on and set
    /// automatically on an overload (`stepAtt.enabled`, `autoAttEnabled`).
    func selectAttenuatorWay(_ way: AttenuatorWay) {
        guard attenuatorWayReason == nil, attenuatorWay != nil, way != attenuatorWay else {
            return
        }
        let stepAtt = store.object(FrontEnd.key)
        // A slice on the other input sets RX2's own (the desktop RX
        // applet's _rx2_step_att_enabled and _auto_att_rx2).
        let enabledName = onRx2Input ? FrontEnd.rx2StepAttEnabled : FrontEnd.enabled
        let autoName = onRx2Input ? FrontEnd.rx2AutoAttEnabled : FrontEnd.autoAttEnabled
        let enabled = RxPanelModel.flag(stepAtt?[enabledName]) ?? false
        let auto = RxPanelModel.flag(stepAtt?[autoName]) ?? false
        switch way {
        case .att:
            write(FrontEnd.key, enabledName, .bool(false))
        case .stepAtt, .autoAtt:
            if !enabled {
                write(FrontEnd.key, enabledName, .bool(true))
            }
            if auto != (way == .autoAtt) {
                write(FrontEnd.key, autoName, .bool(way == .autoAtt))
            }
        }
    }

    func toggleRx1Preamp() {
        guard rx1PreampReason == nil, let on = rx1Preamp else {
            return
        }
        write(FrontEnd.key, FrontEnd.rx1Preamp, .bool(!on))
    }

    /// RX bypass on transmit (the desktop flag's BYPS).
    func toggleBypass() {
        guard bypassReason == nil, let on = bypass else {
            return
        }
        write(Bypass.key, Bypass.property, .bool(!on))
    }

    // MARK: Typed values

    func closePad() {
        pad?.retire()
        pad = nil
    }

    /// Shows the pad `make` builds, whose own close shuts only that pad: a
    /// late answer to a pad closed meanwhile leaves the newer one open.
    private func present(_ make: (@escaping () -> Void) -> ValuePadModel?) {
        final class Shown {
            weak var pad: ValuePadModel?
        }
        let shown = Shown()
        let made = make { [weak self, shown] in
            guard let self, let mine = shown.pad, self.pad === mine else {
                return
            }
            self.pad = nil
        }
        shown.pad = made
        pad?.retire()
        pad = made
    }

    /// The step attenuator typed in, within the Core's range.
    func openAttenuatorPad() {
        guard attenuatorReason == nil, let range = attenuatorRange else {
            return
        }
        let low = Int64(range.min.rounded())
        let high = Int64(range.max.rounded())
        guard low <= high else {
            return
        }
        let current = attenuationDb
        present { close in
            ValuePadModel(title: "Step attenuator", unit: "dB", range: low...high, current: current,
                          sendWithLate: { [weak self] db, late in
                              Self.closingWhenHeld(await self?.writeAttenuationNow(db, onLateOutcome: late), close)
                          },
                          onOutcome: { [weak self] in self?.notePadOutcome($0) },
                          readCurrent: { [weak self] in self?.attenuationDb.map(Double.init) }, close: close)
        }
    }

    /// One of the slice's filter edges typed in (the RX panel's rule).
    func openFilterEdgePad(low: Bool) {
        present { close in rx.filterEdgePad(low: low, close: close) }
    }

    /// One of the TX filter's edges typed in (the TX panel's rule).
    func openTxFilterPad(low: Bool) {
        present { close in transmit.txFilterPad(low: low, close: close) }
    }

    func openRitPad() {
        openSlicePad("RIT offset", property: Property.ritHz, range: Self.offsetRange, current: ritHz)
    }

    func openXitPad() {
        openSlicePad("XIT offset", property: Property.xitHz, range: Self.offsetRange, current: xitHz)
    }

    func openDigOffsetPad() {
        guard digOffsetReason == nil, let property = digOffsetProperty else {
            return
        }
        openSlicePad("DIG offset", property: property, range: Self.offsetRange, current: digOffsetHz)
    }

    func openRttyPad(mark: Bool) {
        guard rttyReason == nil else {
            return
        }
        openSlicePad(mark ? "RTTY mark" : "RTTY shift", property: mark ? Property.rttyMarkHz : Property.rttyShiftHz,
                     range: mark ? Self.rttyMarkRange : Self.rttyShiftRange, current: mark ? rttyMarkHz : rttyShiftHz)
    }

    private func openSlicePad(_ title: String, property: String, range: ClosedRange<Int64>, current: Int64?) {
        guard let key = slice?.key, current != nil else {
            return
        }
        present { close in
            ValuePadModel(title: title, unit: "Hz", range: range, current: current,
                          sendWithLate: { [weak self] hz, late in
                              Self.closingWhenHeld(await self?.writeNow(key, property, .int(hz), onLateOutcome: late), close)
                          },
                          onOutcome: { [weak self] in self?.notePadOutcome($0) },
                          readCurrent: { [weak self] in RxPanelModel.number(self?.store.object(key)?[property]) }, close: close)
        }
    }

    /// A pad's answer: a change the Core holds for this phone's question
    /// closes the pad, and the question sheet asks about it. The tab follows
    /// the mirror's held value until the Core sends its next value.
    /// Kept open, the pad would show the typed value with the waiting words
    /// as if refused, after the question closed.
    private static func closingWhenHeld(_ outcome: PropertyWriteOutcome?,
                                        _ close: () -> Void) -> PropertyWriteOutcome? {
        if let outcome, outcome.answeredByCore, !outcome.accepted, outcome.reason == SeveralDevices.waitingReason {
            close()
        }
        return outcome
    }

    /// One write, answered: for a number pad, which waits for the Core's word.
    private func writeNow(_ key: String, _ property: String, _ value: MirrorValue,
                          onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        await store.write(key, property: property, value: value, onLateOutcome: onLateOutcome)
    }

    private func notePadOutcome(_ outcome: PropertyWriteOutcome) {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return }
        noteOutcome(outcome.accepted, outcome.reason, answered: outcome.answeredByCore)
    }

    /// An RX antenna or receive-only input: the slice's `rxAntenna`, by
    /// the catalogue's label, as the desktop's RX antenna menu writes it.
    func selectRxAntenna(_ antenna: Antenna) {
        guard rxAntennaReason == nil, !antenna.lit else {
            return
        }
        writeSlice(Property.rxAntenna, .text(antenna.label))
    }

    /// A TX antenna: the slice's band's TX antenna, by `setAlexTxAntenna`
    /// on a Core that takes it; on an older one, the slice's `txAntenna`.
    func selectTxAntenna(_ antenna: Antenna) {
        guard txAntennaReason == nil, !antenna.lit, let object = slice else {
            return
        }
        if txAntennaByBand, let band = Self.whole(object[Property.band]) {
            let change = [CommandArgument(name: "band", value: .int(band)),
                          CommandArgument(name: "antenna", value: .int(Int64(antenna.number)))]
            // Lit at the touch, as the slice's `txAntenna` the Core sends back
            // (StationClient.cpp:1040-1068), until the Core answers.
            let shows: [CommandHoldQueue.Shown] = [.init(object.key, Property.txAntenna, .text(antenna.label))]
            // Radio-bound: the change names the radio it was made for, so a
            // change the Core holds for a question cannot land on another
            // radio after a switch (StationClient.cpp:5524-5545).
            guard radioBoundAntennas else {
                invoke(Self.txAntennaVerb, change, shows: shows)
                return
            }
            guard let mac = connectedRadioMac else {
                note = Self.radioNotReadyReason
                return
            }
            invoke(Self.txAntennaForRadioVerb, [CommandArgument(name: "mac", value: .text(mac))] + change, shows: shows)
        } else {
            writeSlice(Property.txAntenna, .text(antenna.label))
        }
    }

    /// The AGC-T slider, in the catalogue's units.
    func setAgcThreshold(_ value: Double) {
        guard let range = agcThresholdRange else {
            return
        }
        writeSlice(Property.agcThreshold, .int(Int64(RxPanelModel.snapped(value, to: range).rounded())))
    }

    func toggleAutoAgc() {
        writeSlice(Property.autoAgcEnabled, .bool(!(autoAgc ?? false)))
    }

    /// APF, in every mode as on the desktop flag (M2); its tune works in CW.
    func toggleApf() {
        guard apfReason == nil else {
            return
        }
        writeSlice(Property.apfEnabled, .bool(!(apf ?? false)))
    }

    /// The APF tune slider, -500 to 500 Hz.
    func setApfTune(_ value: Double) {
        guard apfTuneReason == nil else {
            return
        }
        writeSlice(Property.apfTuneHz, .int(Int64(RxPanelModel.snapped(value, to: Self.apfTuneRange).rounded())))
    }

    /// The DIG offset's arrows, 10 Hz a step, for the slice's DIGL or DIGU.
    func stepDigOffset(up: Bool) {
        guard digOffsetReason == nil, let property = digOffsetProperty, let hz = digOffsetHz else {
            return
        }
        writeClamped(property, hz + (up ? Self.digStepHz : -Self.digStepHz), within: Self.offsetRange, from: hz)
    }

    /// RTTY mark's and shift's arrows.
    func stepRtty(mark: Bool, up: Bool) {
        guard rttyReason == nil, let hz = mark ? rttyMarkHz : rttyShiftHz else {
            return
        }
        let step = mark ? Self.rttyMarkStepHz : Self.rttyShiftStepHz
        writeClamped(mark ? Property.rttyMarkHz : Property.rttyShiftHz, hz + (up ? step : -step),
                     within: mark ? Self.rttyMarkRange : Self.rttyShiftRange, from: hz)
    }

    /// The slice's output at the Core: its speakers or its headphones.
    func selectOutputRoute(_ route: Int64) {
        guard outputRoute != nil, outputRoute != route else {
            return
        }
        writeSlice(Property.outputRoute, .enumeration(route))
    }

    /// One of NNR's settings, by its property (``NnrSettingsSection``).
    func setNnrSetting(_ setting: NnrSettingsSection.Setting, _ value: Double) {
        guard nnrSettings[setting.property] != nil else {
            return
        }
        let snapped = RxPanelModel.snapped(value, to: setting.range)
        switch setting.kind {
        case .number:
            writeSlice(setting.property, .double(snapped))
        case .choice:
            writeSlice(setting.property, .enumeration(Int64(snapped.rounded())))
        }
    }

    /// A noise-reduction slider or choice the Core describes moved to
    /// `value`, in the control's own units (a choice's id); the slice gets
    /// the property's value, in the kind the slice holds it.
    func setNrControl(_ control: StationCatalog.NrControl, _ value: Double) {
        guard let current = nrValues[control.property] else {
            return
        }
        switch control.kind {
        case .slider(let slider):
            let position = RxPanelModel.snapped(value, to: slider.range)
            writeSlice(control.property, Self.typed(slider.propertyValue(position), like: current))
        case .toggle:
            writeSlice(control.property, .bool(value != 0))
        case .choice(let choices):
            let id = Int(value.rounded())
            guard choices.options.contains(where: { $0.id == id }) else {
                return
            }
            writeSlice(control.property, Self.typed(Double(id), like: current))
        }
    }

    /// A noise-reduction switch the Core describes: on becomes off, off on.
    func toggleNrControl(_ control: StationCatalog.NrControl) {
        guard case .toggle = control.kind, let on = RxPanelModel.flag(nrValues[control.property]) else {
            return
        }
        writeSlice(control.property, .bool(!on))
    }

    /// Reset for one slot, as the desktop's popup has it: each of its
    /// controls with a Reset of its own back to that value; the others
    /// (NNR's model) as they are.
    func resetNrSlot(_ slot: String) {
        for control in noiseReduction?[slot] ?? [] {
            guard let current = nrValues[control.property] else {
                continue
            }
            switch control.kind {
            case .slider(let slider):
                if let reset = slider.reset {
                    let next = Self.typed(slider.propertyValue(reset), like: current)
                    if next != current {
                        writeSlice(control.property, next)
                    }
                }
            case .choice(let choices):
                if let reset = choices.reset {
                    let next = Self.typed(Double(reset), like: current)
                    if next != current {
                        writeSlice(control.property, next)
                    }
                }
            case .toggle:
                break
            }
        }
    }

    /// `value` in the kind the slice holds `like` in.
    static func typed(_ value: Double, like: MirrorValue) -> MirrorValue {
        switch like {
        case .int:
            return .int(Int64(value.rounded()))
        case .enumeration:
            return .enumeration(Int64(value.rounded()))
        default:
            return .double(value)
        }
    }

    /// The property the DIG offset moves: DIGL's or DIGU's.
    private var digOffsetProperty: String? {
        switch modeLabel {
        case Self.diglLabel?:
            return Property.diglOffsetHz
        case Self.diguLabel?:
            return Property.diguOffsetHz
        default:
            return nil
        }
    }

    private func writeClamped(_ property: String, _ hz: Int64, within range: ClosedRange<Int64>, from current: Int64) {
        let next = min(max(hz, range.lowerBound), range.upperBound)
        guard next != current else {
            return
        }
        writeSlice(property, .int(next))
    }

    func setPan(_ value: Double) {
        writeSlice(Property.audioPan, .double(RxPanelModel.snapped(value, to: Self.panRange)))
    }

    func toggleMute() {
        writeSlice(Property.muted, .bool(!(muted ?? false)))
    }

    func toggleBinaural() {
        writeSlice(Property.binauralEnabled, .bool(!(binaural ?? false)))
    }

    func toggleRit() {
        writeSlice(Property.ritEnabled, .bool(!(ritOn ?? false)))
    }

    func toggleXit() {
        writeSlice(Property.xitEnabled, .bool(!(xitOn ?? false)))
    }

    /// RIT's arrows: one of the slice's tuning steps, as the desktop flag's,
    /// within its -10000 to 10000 Hz.
    func stepRit(up: Bool) {
        guard let hz = ritHz, let step = stepHz else {
            return
        }
        writeClamped(Property.ritHz, hz + (up ? step : -step), within: Self.offsetRange, from: hz)
    }

    /// XIT's arrows: one of the slice's tuning steps.
    func stepXit(up: Bool) {
        guard let hz = xitHz, let step = stepHz else {
            return
        }
        writeClamped(Property.xitHz, hz + (up ? step : -step), within: Self.offsetRange, from: hz)
    }

    /// RIT's "0": the offset back to zero; RIT stays as it is.
    func clearRit() {
        guard let hz = ritHz, hz != 0 else {
            return
        }
        writeSlice(Property.ritHz, .int(0))
    }

    /// XIT's "0".
    func clearXit() {
        guard let hz = xitHz, hz != 0 else {
            return
        }
        writeSlice(Property.xitHz, .int(0))
    }

    func setMicGain(_ value: Double) {
        writeTransmit(Transmit.micGainDb,
                      .int(Int64(RxPanelModel.snapped(value, to: micGainRange).rounded())))
    }

    func setProcLevel(_ value: Double) {
        writeTransmit(Transmit.cpdrLevelDb,
                      .int(Int64(RxPanelModel.snapped(value, to: Self.procLevelRange).rounded())))
    }

    func toggleLeveler() {
        writeTransmit(Transmit.txLevelerOn, .bool(!(leveler ?? false)))
    }

    func toggleEq() {
        writeTransmit(Transmit.txEqEnabled, .bool(!(eq ?? false)))
    }

    func toggleCfc() {
        writeTransmit(Transmit.cfcEnabled, .bool(!(cfc ?? false)))
    }

    /// Match RX (D81): the TX panel's own, beside the TX filter here too.
    func matchRx() {
        transmit.matchRxFilter()
    }

    /// The transmit settings this section writes itself (mic gain, PROC's
    /// level, LEV, EQ, CFC) came with `transmitSettingsVersion` 2; they
    /// change as the desktop's do: while the Core takes them and its radio
    /// is off the air (I1).
    static let chainVersion: Int64 = 2

    private func writeTransmit(_ property: String, _ value: MirrorValue) {
        guard transmit.settingsEditable(Self.chainVersion) else {
            return
        }
        write(Transmit.key, property, value)
    }

    private func writeSlice(_ property: String, _ value: MirrorValue) {
        guard let key = slice?.key else {
            return
        }
        write(key, property, value)
    }

    /// One property write, shown at once and sent one at a time per
    /// property, the newest value next (``PropertyWriteQueue``).
    private func write(_ key: String, _ property: String, _ value: MirrorValue) {
        if key == Transmit.key {
            transmitWrites.write(key, property, value)
        } else {
            writes.write(key, property, value)
        }
    }

    private func invoke(_ verb: String, _ arguments: [CommandArgument], shows: [CommandHoldQueue.Shown]) {
        guard let commands else {
            noteOutcome(false, PropertyWriteOutcome.notSent.reason, answered: false)
            return
        }
        commandHolds.send(verb, shows: shows, invokeWithLate: { late in
            try await commands.invokeHeld(verb, arguments: arguments, timeout: Self.commandTimeout, onLateOutcome: { outcome in
                await MainActor.run { late(outcome) }
            })
        }, onOutcome: { [weak self] outcome in
            let outcome = PropertyWriteOutcome(outcome)
            if !outcome.answeredByCore {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            }
            self?.noteOutcome(outcome.accepted, outcome.reason, answered: outcome.answeredByCore)
        })
    }

    private func noteOutcome(_ accepted: Bool, _ reason: String, answered: Bool) {
        if accepted {
            if note != nil {
                note = nil
            }
        } else if answered && reason == SeveralDevices.waitingReason {
            // Held for this phone's question sheet, not refused: never a
            // note (the desktop, StationClient.cpp:7308-7317).
            return
        } else if answered {
            // A refusal without words shows the phone's words for any refused request.
            note = reason.isEmpty ? BandSlicesModel.refusedText : reason
        } else {
            // Not sent, not answered in time, or cut off by a dropped link.
            Self.logger.info("A Modes tab change: \(reason, privacy: .private)")
            note = reason
        }
    }

    // MARK: Reading

    /// The store publishes before it changes, so the tab reads it on the
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

    private func watch(_ name: String, _ object: MirrorObject?) {
        let id = object.map(ObjectIdentifier.init)
        guard watchedObjects[name] != id else {
            return
        }
        watchedObjects[name] = id
        objectWatches[name] = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRebuild() }
    }

    private func rebuild() {
        let object = slice
        let stepAtt = store.object(FrontEnd.key)
        let transmitObject = store.object(Transmit.key)
        let alex = store.object(Bypass.key)
        watch("slice", object)
        watch(FrontEnd.key, stepAtt)
        watch(Transmit.key, transmitObject)
        watch(Bypass.key, alex)
        let catalog = catalogFeed.catalog

        var missing: Set<String> = []
        func sliceValue(_ property: String) -> MirrorValue? {
            guard let object else {
                return nil
            }
            guard let value = object[property] else {
                missing.insert(property)
                return nil
            }
            return value
        }

        // The slice switch.
        set(\.sliceChoices, slices.entries.map { entry in
            SliceChoice(id: entry.id, letter: BandSlice.letter(forIndex: entry.id), colour: entry.slice.colour,
                        frequencyText: entry.slice.frequencyText, active: entry.id == slices.activeSliceId,
                        listening: entry.listening)
        })

        // The mode and the filter's edges.
        let mode = Self.whole(sliceValue(Property.dspMode))
        set(\.modes, (catalog?.modes ?? []).map { Choice(id: $0.id, label: $0.label, lit: Int64($0.id) == mode) })
        let label = catalog?.modes.first { Int64($0.id) == mode }?.label
        set(\.modeLabel, label)
        set(\.filterLowHz, Self.whole(sliceValue(Property.filterLow)))
        set(\.filterHighHz, Self.whole(sliceValue(Property.filterHigh)))

        // The front end: the preamp and step attenuator on `stepAtt`, the
        // antennas on the slice, every list and range the catalogue's.
        let board = catalog?.board
        let frontEndReason: String? = stepAtt != nil ? nil : frontEndMissingReason()
        // A slice on the other input (its bit in `rx2SliceMask`, absent
        // meaning none) shows RX2's own, as the desktop RX applet does
        // (showStepAttValueForSlice, fillPreampCombo).
        let rx2Mask = Self.whole(stepAtt?[FrontEnd.rx2SliceMask]) ?? 0
        let rx2 = slices.activeSliceId.map { $0 >= 0 && $0 < 63 && (rx2Mask >> Int64($0)) & 1 == 1 } ?? false
        set(\.onRx2Input, rx2)
        let rx2OlderReason = rx2 ? rx2InputOlderCoreReason(stepAtt) : nil
        if rx2 {
            let items = board?.rx2PreampItems
            let mode = Self.whole(stepAtt?[FrontEnd.rx2PreampMode])
            if let items, !items.isEmpty {
                set(\.preamp, items.map { Choice(id: $0.id, label: $0.label, lit: Int64($0.id) == mode) })
                set(\.preampReason, frontEndReason ?? (mode == nil ? Self.rx2PreampOlderCoreText : nil))
            } else if items == nil || rx2OlderReason != nil {
                // A Core that does not send RX2's list: the first input's
                // list stays in its place, greyed, with the desktop's reason.
                set(\.preamp, (board?.preampItems ?? []).map { Choice(id: $0.id, label: $0.label, lit: false) })
                set(\.preampReason, frontEndReason ?? Self.rx2PreampOlderCoreText)
            } else {
                set(\.preamp, [])
                set(\.preampReason, frontEndReason ?? board?.rx2AttenuatorReason ?? Self.rx2NoPreampText)
            }
        } else {
            let preampMode = Self.whole(stepAtt?[FrontEnd.preampMode])
            set(\.preamp, (board?.preampItems ?? []).map { Choice(id: $0.id, label: $0.label, lit: Int64($0.id) == preampMode) })
            if let board, board.preampItems.isEmpty {
                set(\.preampReason, Self.noPreampText)
            } else {
                set(\.preampReason, frontEndReason)
            }
        }
        var range = board?.attenuator
        if rx2 {
            // RX2's range is the catalogue's alone (board.rx2Attenuator).
            range = board?.rx2Attenuator
        } else if let low = RxPanelModel.number(stepAtt?[FrontEnd.minDb]),
                  let high = RxPanelModel.number(stepAtt?[FrontEnd.maxDb]), low < high {
            // The Core's live range first (the desktop's remote window), else the catalogue's.
            range = StationCatalog.Range(min: low, max: high, step: board?.attenuator?.step ?? 1)
        }
        set(\.attenuatorRange, range)
        set(\.attenuationDb, Self.whole(stepAtt?[rx2 ? FrontEnd.rx2AttenuationDb : FrontEnd.attenuationDb]))
        // ATT, S-ATT or A-ATT, as the desktop RX applet labels it.
        let stepOn = RxPanelModel.flag(stepAtt?[rx2 ? FrontEnd.rx2StepAttEnabled : FrontEnd.enabled])
        let autoOn = RxPanelModel.flag(stepAtt?[rx2 ? FrontEnd.rx2AutoAttEnabled : FrontEnd.autoAttEnabled]) ?? false
        set(\.attenuatorWay, stepOn.map { $0 ? (autoOn ? .autoAtt : .stepAtt) : .att })
        // RX2 without an attenuator of its own says why, in the Core's words.
        let noAttenuatorText = rx2 ? (rx2OlderReason ?? board?.rx2AttenuatorReason ?? Self.noAttenuatorText)
            : Self.noAttenuatorText
        let noAttenuator = rx2 ? (rx2OlderReason != nil || board?.rx2Attenuator == nil)
            : board.map { $0.attenuator == nil } ?? false
        set(\.attenuatorWayReason, noAttenuator ? noAttenuatorText
            : frontEndReason ?? (stepAtt != nil && stepOn == nil ? CatalogFeed.needsNewerCoreText : nil))
        if noAttenuator {
            set(\.attenuatorReason, noAttenuatorText)
        } else if let frontEndReason {
            set(\.attenuatorReason, frontEndReason)
        } else {
            set(\.attenuatorReason, stepOn == false ? Self.attOffText : nil)
        }
        // The desktop shows the preamp choices with the step attenuator off.
        if preampReason == nil, stepOn == true {
            set(\.preampReason, Self.preampWithStepAttText)
        }
        // The second ADC's preamp and RX bypass on transmit (on the Core's
        // Alex antennas): hidden on a radio without them.
        set(\.rx1PreampPresent, board?.rx1Preamp ?? true)
        set(\.bypassPresent, board?.relays?.rxOutOnTx ?? true)
        set(\.rx1Preamp, rx1PreampPresent ? RxPanelModel.flag(stepAtt?[FrontEnd.rx1Preamp]) : nil)
        set(\.rx1PreampReason, !rx1PreampPresent ? nil
            : frontEndReason ?? (stepAtt != nil && rx1Preamp == nil ? Self.rx1PreampOlderCoreText : nil))
        set(\.bypass, bypassPresent ? RxPanelModel.flag(alex?[Bypass.property]) : nil)
        set(\.bypassReason, bypassPresent ? bypassUnavailableReason(alex) : nil)

        let rxAntenna = Self.text(sliceValue(Property.rxAntenna))
        let txAntenna = Self.text(sliceValue(Property.txAntenna))
        set(\.rxAntennas, Self.antennas(board?.rxAntennas ?? [], lit: rxAntenna))
        set(\.rxOnlyInputs, Self.antennas(board?.rxOnlyInputs ?? [], lit: rxAntenna, from: (board?.rxAntennas.count ?? 0) + 1))
        set(\.txAntennas, Self.antennas(board?.txAntennas ?? [], lit: txAntenna))
        set(\.rxAntennaReason, Self.antennaReason(object: object, value: rxAntenna, list: board?.rxAntennas, catalog: catalog))
        set(\.txAntennaReason, Self.antennaReason(object: object, value: txAntenna, list: board?.txAntennas, catalog: catalog))

        // AGC-T and AUTO.
        set(\.agcThresholdRange, catalog?.agc.thresholdDb)
        set(\.agcThreshold, RxPanelModel.number(sliceValue(Property.agcThreshold)))
        set(\.autoAgc, RxPanelModel.flag(sliceValue(Property.autoAgcEnabled)))
        set(\.autoAgcInfo, autoAgc == true ? Self.autoAgcText(object) : nil)

        // APF: the switch in every mode, as the desktop flag's (M2); its
        // tune in CW with APF on.
        set(\.apf, RxPanelModel.flag(sliceValue(Property.apfEnabled)))
        set(\.apfReason, object != nil && apf == nil ? CatalogFeed.needsNewerCoreText : nil)
        set(\.apfTuneHz, RxPanelModel.number(sliceValue(Property.apfTuneHz)))
        let cw = label?.hasPrefix(Self.cwLabelPrefix) ?? false
        if object != nil, apfTuneHz == nil {
            set(\.apfTuneReason, CatalogFeed.needsNewerCoreText)
        } else {
            set(\.apfTuneReason, cw && apf == true ? nil : Self.apfTuneText)
        }

        // The DIG offset (DIGL and DIGU) and RTTY mark and shift (DIGL), as the flag shows them.
        let digl = label == Self.diglLabel
        let digu = label == Self.diguLabel
        let diglOffset = Self.whole(sliceValue(Property.diglOffsetHz))
        let diguOffset = Self.whole(sliceValue(Property.diguOffsetHz))
        set(\.digOffsetHz, digu ? diguOffset : diglOffset)
        if object != nil, diglOffset == nil || diguOffset == nil {
            set(\.digOffsetReason, CatalogFeed.needsNewerCoreText)
        } else {
            set(\.digOffsetReason, digl || digu ? nil : Self.digOffsetText)
        }
        set(\.rttyMarkHz, Self.whole(sliceValue(Property.rttyMarkHz)))
        set(\.rttyShiftHz, Self.whole(sliceValue(Property.rttyShiftHz)))
        if object != nil, rttyMarkHz == nil || rttyShiftHz == nil {
            set(\.rttyReason, CatalogFeed.needsNewerCoreText)
        } else {
            set(\.rttyReason, digl ? nil : Self.rttyText)
        }

        // Audio.
        set(\.outputRoute, Self.whole(sliceValue(Property.outputRoute)))
        set(\.pan, RxPanelModel.number(sliceValue(Property.audioPan)))
        set(\.muted, RxPanelModel.flag(sliceValue(Property.muted)))
        set(\.binaural, RxPanelModel.flag(sliceValue(Property.binauralEnabled)))

        // RIT and XIT.
        set(\.ritOn, RxPanelModel.flag(sliceValue(Property.ritEnabled)))
        set(\.ritHz, Self.whole(sliceValue(Property.ritHz)))
        set(\.xitOn, RxPanelModel.flag(sliceValue(Property.xitEnabled)))
        set(\.xitHz, Self.whole(sliceValue(Property.xitHz)))
        set(\.stepHz, Self.whole(sliceValue(Property.stepHz)))

        // NNR's settings.
        var nnr: [String: Double] = [:]
        for setting in NnrSettingsSection.settings {
            if let value = RxPanelModel.number(object?[setting.property]) {
                nnr[setting.property] = value
            }
        }
        set(\.nnrSettings, nnr)

        // The noise reductions' settings the Core describes.
        let reductions = catalog?.noiseReduction
        set(\.noiseReduction, reductions)
        var values: [String: MirrorValue] = [:]
        let described: [StationCatalog.NrControl] = reductions.map { Array($0.slots.values.joined()) } ?? []
        for control in described {
            if let value = object?[control.property] {
                values[control.property] = value
            }
        }
        set(\.nrValues, values)

        // Transmit.
        func transmitValue(_ property: String) -> MirrorValue? {
            guard let transmitObject else {
                return nil
            }
            guard let value = transmitObject[property] else {
                missing.insert(property)
                return nil
            }
            return value
        }
        set(\.txFilterLowHz, Self.whole(transmitValue(Transmit.filterLow)))
        set(\.txFilterHighHz, Self.whole(transmitValue(Transmit.filterHigh)))
        set(\.micGainDb, RxPanelModel.number(transmitValue(Transmit.micGainDb)))
        set(\.micGainRange, board?.transmit?.micGainDb ?? Self.micGainFallback)
        set(\.procLevelDb, RxPanelModel.number(transmitValue(Transmit.cpdrLevelDb)))
        set(\.leveler, RxPanelModel.flag(transmitValue(Transmit.txLevelerOn)))
        set(\.eq, RxPanelModel.flag(transmitValue(Transmit.txEqEnabled)))
        set(\.cfc, RxPanelModel.flag(transmitValue(Transmit.cfcEnabled)))

        set(\.olderCore, missing)
    }

    /// Why the preamp and step attenuator are greyed while the Core sends
    /// no `stepAtt` object: nothing while it is still sending its snapshot;
    /// an older Core, which does not know the attenuator capability, needs
    /// updating; a current one has none ready (link document section 6.3,
    /// `radioHardwareVersion` 0 until its step attenuator is bound).
    private func frontEndMissingReason() -> String? {
        guard store.isSnapshotComplete else {
            return nil
        }
        if (store.agreedMinor ?? 0) < Self.txAntennaMinor || store.capabilities[Self.txAntennaCapability] == nil {
            return CatalogFeed.needsNewerCoreText
        }
        return Self.frontEndNotReadyText
    }

    /// Why RX2's own input control cannot be used for a slice on the other
    /// input: a Core that does not send RX2's catalogue items
    /// (`rx2AttenuatorVersion`) needs updating. Nil while the Core is still
    /// sending its snapshot, or when the Core sends them.
    private func rx2InputOlderCoreReason(_ stepAtt: MirrorObject?) -> String? {
        let catalogued = catalogFeed.catalog?.board.rx2PreampItems != nil
        if catalogued, store.capabilities[Self.rx2AttenuatorCapability] != nil {
            return nil
        }
        return store.isSnapshotComplete ? CatalogFeed.needsNewerCoreText : nil
    }

    /// The capability a Core sends a peer that declared `rx2Attenuator` 1.
    static let rx2AttenuatorCapability = "rx2AttenuatorVersion"

    /// Why RX bypass on transmit cannot change: the desktop flag's BYPS
    /// reasons. Nil while the Core is still sending its snapshot.
    private func bypassUnavailableReason(_ alex: MirrorObject?) -> String? {
        guard store.isSnapshotComplete else {
            return nil
        }
        let version = store.capabilityVersion(Self.txAntennaCapability)
        if (store.agreedMinor ?? 0) < Self.txAntennaMinor || version < Bypass.hardwareVersion {
            return Self.hardwareOlderCoreText
        }
        if version < Bypass.version || alex == nil {
            return Self.bypassOlderCoreText
        }
        return RxPanelModel.flag(alex?[Bypass.property]) == nil ? CatalogFeed.needsNewerCoreText : nil
    }

    /// The AUTO line: "NF −110 dB · offset +8" once the Core has measured
    /// the noise floor, else that it has not.
    static func autoAgcText(_ slice: MirrorObject?) -> String? {
        guard let slice else {
            return nil
        }
        guard RxPanelModel.flag(slice[Property.noiseFloorValid]) == true,
              let floor = RxPanelModel.number(slice[Property.noiseFloorDbm]) else {
            return noiseFloorWaitingText
        }
        let offset = Int((RxPanelModel.number(slice[Property.autoAgcOffset]) ?? 0).rounded(.towardZero))
        let nf = Int(floor.rounded(.towardZero))
        return "NF \(ValuePadModel.text(Int64(nf))) dB \u{00B7} offset \(offset < 0 ? ValuePadModel.text(Int64(offset)) : "+\(offset)")"
    }

    /// Why an antenna row is greyed: the slice does not mirror its antenna,
    /// or the Core lists no antennas for its radio.
    private static func antennaReason(object: MirrorObject?, value: String?, list: [String]?,
                                      catalog: StationCatalog?) -> String? {
        guard object != nil else {
            return nil
        }
        if value == nil {
            return CatalogFeed.needsNewerCoreText
        }
        if catalog != nil, list?.isEmpty ?? true {
            return noAntennaListText
        }
        return nil
    }

    private static func antennas(_ labels: [String], lit: String?, from first: Int = 1) -> [Antenna] {
        labels.enumerated().map { Antenna(label: $1, number: first + $0, lit: $1 == lit) }
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<ModesTabModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }

    static func whole(_ value: MirrorValue?) -> Int64? {
        RxPanelModel.whole(value)
    }

    static func text(_ value: MirrorValue?) -> String? {
        if case .text(let text)? = value {
            return text
        }
        return nil
    }
}
