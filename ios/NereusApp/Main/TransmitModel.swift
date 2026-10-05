// NereusSDR for iOS: transmit on the main screen: the PTT, the TX panel's controls and the keyed view's readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
import os
import UIKit

/// Everything the main screen shows of transmit (R-IOS-11, R-IOS-13,
/// R-IOS-17; spec section 5.1 items 1, 2, 5 to 7, 12 and 13, section 5.8
/// items 2 to 7): the PTT's state from ``PttController``, the Core's
/// `txState` (the holder, the meters, the stops), the TX panel's settings
/// on `transmit` and the amplifier's and tuner's OPERATE, and the Core's
/// reason this phone may not transmit (`txRefusalReason`).
///
/// It feeds the controller the Core's transmit state and the link, keeps
/// the screen awake while a key of this phone's is on, and tells the band's
/// subscription when the holder of transmit changes, since the Core then
/// shares its display differently (``BandSubscriber/transmitHolderChanged()``).
///
/// Nothing here keys by itself: every key is the operator's tap.
@MainActor
final class TransmitModel: ObservableObject {
    /// The PTT as the controller last reported it.
    @Published private(set) var ptt = PttController.Snapshot()
    /// The Core's transmit state as the PTT and the keyed view read it.
    @Published private(set) var report = TransmitStateReport()
    /// The radio's readings while keyed.
    @Published private(set) var forwardWatts = 0.0
    @Published private(set) var swr = 1.0
    @Published private(set) var micLevelDb = -400.0
    /// Current nullable samples for the keyed overlay; legacy consumers keep their numeric contract.
    @Published private(set) var keyedForwardWatts: Double?
    @Published private(set) var keyedSwr: Double?
    @Published private(set) var keyedMicLevelDb: Double?
    @Published private(set) var keyedReadingsCurrent = false
    /// The Core sends this device its transmit readings (`txState`), which
    /// the S-meter's TX modes read (D86).
    @Published private(set) var readingsSent = false
    /// The Core's `txReadingsVersion`, 0 without it.
    @Published private(set) var txReadingsVersion: Int64 = 0
    /// The seven transmit stage readings in ``TxStage/all``'s order (JJ,
    /// 2026-09-28), nil where the Core has no reading or sends none.
    @Published private(set) var stageReadings: [Double?] = TxStage.empty
    /// Each stage's recent peak, kept here from the readings while the
    /// radio is on the air, nil off the air.
    @Published private(set) var stagePeaks: [Double?] = TxStage.empty
    /// The radio is on the air, so the stage readings move.
    @Published private(set) var stagesOnAir = false
    /// The Core's mic is muted (`transmit.micMuted`, at
    /// `transmitSettingsVersion` 10): Mic level and Mic Gain are greyed, as
    /// the desktop greys its mic slider. The phone never writes it.
    @Published private(set) var micMuted = false
    /// The speech compressor's gain reduction in dB, nil where the Core
    /// does not send it: the S-meter's Compression.
    @Published private(set) var compressionDb: Double?
    /// How long the key on now has been on, as the Core counts it.
    @Published private(set) var keyedForSeconds: Int64 = 0
    /// Whole seconds before the Core's transmit time-out stops the key now
    /// on, -1 when none applies (`txState.timeOutRemainingSeconds`); the
    /// opened island counts it down (R-IOS-21).
    @Published private(set) var timeOutRemainingSeconds: Int64 = -1
    /// The Core lets this phone transmit.
    @Published private(set) var permitted = false
    /// Why it does not, in the Core's words, with its code and fix.
    @Published private(set) var permission: TxRefusalInfo?
    /// The Core offers remote transmit to this phone at all.
    @Published private(set) var offered = false
    /// The Core takes the Tuner Genius's TUNE from this phone
    /// (`tx.tunerTune`, `remoteTxVersion` 2 at the agreed minor 11), as the
    /// desktop's remote window asks it (StationClient.cpp:7774-7780).
    @Published private(set) var tunerTuneOffered = false
    /// The TX panel's settings, nil where the Core has not sent them.
    @Published private(set) var rfPower: Double?
    @Published private(set) var tunePower: Double?
    /// RF Power's and Tune Pwr's ranges and readouts on this radio, from
    /// the Core's catalogue (`board.transmit`); 0 to 100 as the number from
    /// a Core that sends none.
    @Published private(set) var powerControl = TransmitModel.powerFallback
    @Published private(set) var tuneControl = TransmitModel.powerFallback
    @Published private(set) var proc = false
    @Published private(set) var vox = false
    /// MON lit: the Core's `monEnabled` is on and the Core has put the
    /// transmit monitor in this phone's audio (its last
    /// `monitor-audio-context` named a route other than none; D80).
    @Published private(set) var mon = false
    /// Why MON is greyed: the Core does not send this phone its transmit
    /// monitor, or the phone's sound is not in headphones (D80).
    @Published private(set) var monReason: String? = TransmitModel.monNotSentText
    /// The phone's sound goes to wired or Bluetooth headphones.
    @Published private(set) var onHeadphones = false
    /// The route the Core last said it applied to this phone's
    /// `monitor-audio`; none until it answers, and again when the media
    /// connection ends (the Core forgets the route with it).
    @Published private(set) var monitorApplied: MonitorRoute = .none
    /// The amplifier and tuner the Core has, if any.
    @Published private(set) var amp: Accessory?
    @Published private(set) var tuner: Accessory?
    /// The media connection carries this phone's microphone line: VOX may
    /// be armed, and a voice key reaches the Core with the microphone.
    @Published private(set) var microphoneLine = false
    /// Why the radio's transmit inhibit holds transmit off (`radio`'s
    /// `txInhibited`), in the Core's words; nil while transmit is allowed.
    @Published private(set) var inhibitReason: String?
    /// This phone transmits: its own key, or a VOX key of its own (the
    /// Core keyed and this phone holds transmit). The band's sound follows it.
    @Published private(set) var transmittingHere = false
    /// The radio is on the air for someone else: another device, the
    /// radio's own PTT or the Core's own window. The holder's name as the
    /// PTT shows it ("Radio" for the radio's PTT), empty when the Core names
    /// none; nil while the radio is not on the air, or is on the air for
    /// this phone. The lock-screen card says so (parity row I16).
    @Published private(set) var onAirElsewhere: String?
    /// The persistent canonical guard shared by the existing profile surfaces.
    @Published private(set) var profileFlow: SetupTxProfileFlow?
    private var profileFlowWatch: AnyCancellable?
    let profileModelOwner = UUID()
    private var profileCanonicalSession: UInt64?
    var usesLegacyProfileSelection: Bool {
        if let profileFlow { return profileFlow.usesLegacySelection }
        return SetupTxProfileFlow.legacySelectionIsAvailable(store: mirror, canonicalSession: profileCanonicalSession)
            && commands != nil
    }
    var profileSelectionReason: String? {
        if let profileFlow { return profileFlow.selectionReason }
        return usesLegacyProfileSelection ? nil : SetupControlDispatcher.updatingReason
    }
    func bindProfileFlow(_ flow: SetupTxProfileFlow) {
        profileFlow = flow
        profileFlowWatch = flow.objectWillChange.sink { [weak self] in self?.objectWillChange.send() }
    }
    /// The Core's words for the last panel change it refused.
    @Published private(set) var note: String?
    /// A tap on PTT while another device holds transmit, or while it
    /// changes hands, sends nothing; the band shows the Core's reason.
    @Published private(set) var heldNote: String?
    /// The slice the radio's own PTT is on the air on, when it is one of
    /// this phone's: its flag says ON AIR and its frequency greys.
    @Published private(set) var radioOnAirSliceId: Int?
    /// The orange TX filter while this phone is keyed, in hertz on the band.
    @Published private(set) var txFilterHz: ClosedRange<Double>?
    /// The slice the TX zero line marks: while the radio is on the air,
    /// whatever keyed it (this phone, another device, the Core's own window
    /// or the radio's PTT), the transmit slice, when it is one of this
    /// band's. The line sits at its dial frequency, as the desktop draws it.
    @Published private(set) var txZeroLineSliceId: Int?
    /// The Core's TX filter, `transmit`'s `filterLow` and `filterHigh`, in audio hertz.
    @Published private(set) var txFilterLowHz: Int64?
    @Published private(set) var txFilterHighHz: Int64?
    /// The Core's `transmitSettingsVersion`: which transmit settings it
    /// takes from this phone (0 for none).
    @Published private(set) var settingsVersion: Int64 = 0
    /// The Core's radio is on the air: its MOX (`radio.transmitting`),
    /// TUNE (`transmit.tune`) or PureSignal's two-tone. Transmit settings
    /// wait until it stops, as on the desktop.
    @Published private(set) var coreOnAir = false
    /// VOX level and delay (`voxThresholdDb`, `voxHangTimeMs`).
    @Published private(set) var voxThresholdDb: Double?
    @Published private(set) var voxHangMs: Double?
    /// The Core's monitor mix volume (0 to 1), shared by Modes and MON.
    @Published private(set) var monitorVolume: Double?
    static let monitorVolumeRange = StationCatalog.Range(min: 0, max: 1, step: 0.01)
    func setMonitorVolume(_ value: Double) {
        guard settingsEditable(2), monitorVolume != nil, value.isFinite else { return }
        write(Self.transmitKey, "monitorVolume", .double(min(max(value, 0), 1)))
    }

    /// Anti-VOX values mirrored by the Core, independent of VOX arming.
    @Published private(set) var antiVoxRun: Bool?
    @Published private(set) var antiVoxGainDb: Double?
    @Published private(set) var antiVoxTauMs: Double?
    /// Core setter ranges: src/models/TransmitModel.cpp:2880-2913.
    static let antiVoxGainRange = StationCatalog.Range(min: -60, max: 60, step: 1)
    static let antiVoxTimeRange = StationCatalog.Range(min: 1, max: 500, step: 1)

    /// DEXP (`dexpEnabled`) and the AM carrier level (`amCarrierLevel`, percent).
    @Published private(set) var dexp: Bool?
    @Published private(set) var amCarrier: Double?
    /// The Core's TX profiles in its order, and the active one.
    @Published private(set) var profiles: [String] = []
    @Published private(set) var activeProfile: String?
    /// PS-A: PureSignal's automatic calibration (`pureSignalSettings.autoCalEnabled`).
    @Published private(set) var psa = false
    /// Why PS-A cannot be turned on now, or nil when it can.
    @Published private(set) var psaReason: String?
    /// The two-tone test is on at the Core (PureSignal's `twoToneOn`).
    @Published private(set) var twoToneOn = false
    /// The number pad open on the TX panel, if any.
    @Published private(set) var pad: ValuePadModel?
    /// The Core's configured SWR protection (Setup > Transmit > Power).
    /// Its known limit is compared with a fresh actual transmit reading;
    /// nil before the Core's settings arrive.
    @Published private(set) var swrProtection: SwrProtection?

    /// The conditional warning uses an actual fresh sample and current settings.
    var highSwr: TxSwrWarning? {
        guard settings?.currentSnapshotIdentity != nil else { return nil }
        return TxSwrWarning.evaluate(mirror.currentTxSwr, limit: swrProtection?.limit)
    }

    /// The Core's SWR protection: on or off, and its limit when the Core
    /// has one set.
    struct SwrProtection: Equatable {
        var on: Bool
        var limit: Double?
    }

    static let swrProtectionEnabledKey = "SwrProtectionEnabled"
    static let swrProtectionLimitKey = "SwrProtectionLimit"
    /// Why the SWR Prot row never lights on the phone: the Core keeps
    /// when its protection acts to itself.
    static let swrProtectionNotSentText = "This Core does not tell the phone when SWR protection cuts the power."

    /// An amplifier or tuner on the Core, as the TX panel shows it.
    struct Accessory: Equatable {
        enum Kind: Equatable {
            case powerGenius
            case rfKit
            case tunerGenius
        }

        let kind: Kind
        let name: String
        let operate: Bool
    }

    /// While the media connection carries no microphone line VOX cannot be
    /// armed; the Core refuses it with these words (its `micNotReady`
    /// reason), and the panel shows them on its disabled VOX (link document
    /// section 18.7).
    static let voxNeedsMicrophone =
        "This device's microphone is not connected to the Core. Wait a moment and try again."

    /// A Core without remote transmit (no `remoteTxVersion`): PTT, TUNE
    /// and MOX say so rather than going grey without a word.
    static let noRemoteTransmitText =
        "This Core can't take transmit from a phone. Updating the Core may help."
    /// The radio's transmit inhibit with no words of its own (the External
    /// TX Inhibit line); the Core refuses a key with the same words.
    static let inhibitInputText = "The radio's transmit inhibit input is holding transmit off."
    /// The tuner's TUNE on a Core below `remoteTxVersion` 2, which does not
    /// take `tx.tunerTune` (the desktop's remote window greys its TUNE there
    /// too, TunerApplet.cpp:620-626).
    static let tunerTuneOlderCoreText =
        "This Core can't start the tuner's tune from a phone. Updating the Core may help."
    /// `tx.tunerTune` comes with `remoteTxVersion` 2 at the agreed minor 11.
    static let tunerTuneVersion: Int64 = 2
    static let tunerTuneMinor: UInt16 = 11
    /// A control whose verb this Core does not take.
    static let ampOlderCoreText = "This Core can't switch the amplifier from a phone. Updating the Core may help."
    static let tunerOlderCoreText = "This Core can't switch the tuner from a phone. Updating the Core may help."
    static let tunePowerOlderCoreText =
        "This Core can't change the tune power from a phone. Updating the Core may help."
    /// MON (D80): with a Core that sends the transmit monitor
    /// (`txMonitorAudioVersion` 1) and the phone's sound in headphones, MON
    /// writes the Core's `monEnabled` as the desktop's MON does and asks for
    /// the monitor in this phone's audio (`monitor-audio`, route
    /// headphones); off writes it off and asks for route none. The
    /// loudspeaker would feed the monitor back into the microphone.
    static let monNotSentText = "This Core does not send the transmit monitor. Updating the Core may help."
    static let monNeedsHeadphonesText =
        "Plug in headphones to hear your transmit. The loudspeaker would feed back into the microphone."
    static let monitorCapability = "txMonitorAudioVersion"
    static let monEnabledProperty = "monEnabled"
    /// `transmit.micMuted` comes with `transmitSettingsVersion` 10.
    static let micMutedVersion: Int64 = 10
    /// Under Mic level while the Core's mic is muted.
    static let micMutedText = "The Core's mic is muted."

    /// The transmit settings gate (the desktop's): a setting that keys
    /// nothing changes while the Core takes it at the needed
    /// `transmitSettingsVersion` (at the agreed minor 11) and its radio is
    /// off the air, whether or not this phone may transmit. Version 1
    /// covers RF power and the TX filter; 2 the mic gain, PROC, LEV, EQ,
    /// CFC, VOX level and delay, DEXP, the AM carrier and tune power; 3
    /// the TX profile; 5 RF power by band (link document section 7.1).
    static let settingsCapability = "transmitSettingsVersion"
    static let settingsMinor: UInt16 = 11
    /// From this `transmitSettingsVersion` the Core takes the transmit
    /// settings while its radio is on the air, as a local window does; an
    /// older Core refuses them then, so they wait, greyed with the reason
    /// (the desktop's kTransmitSettingsOnAirVersion,
    /// src/core/session/StationCapabilities.h:248, and its
    /// MainWindow::transmitSettingsPermitted, src/gui/MainWindow.cpp:15254-15266).
    static let settingsOnAirVersion: Int64 = 13
    /// The Core refuses a transmit setting while its radio is on the air.
    static let onAirText = "The radio is on the air. Try again when it stops."
    /// A Core without the version a setting needs.
    static let settingsNotTakenText =
        "This Core does not let this phone change transmit settings. Updating the Core may help."

    /// The desktop TX applet's TX filter ranges, in audio hertz: the low
    /// edge 0 to 5000, the high edge 200 to 10000. The Core swaps the edges
    /// itself when one passes the other.
    static let txFilterLowRange: ClosedRange<Int64> = 0...5000
    static let txFilterHighRange: ClosedRange<Int64> = 200...10000
    /// The Core's own ranges for VOX level and delay and the AM carrier
    /// (link document section 7.1, `transmit` at `transmitSettingsVersion`
    /// 2); the Core refuses anything outside with its own words.
    static let voxThresholdRange = StationCatalog.Range(min: -80, max: 0, step: 1)
    static let voxHangRange = StationCatalog.Range(min: 1, max: 2000, step: 1)
    static let amCarrierRange = StationCatalog.Range(min: 0, max: 100, step: 1)
    /// RF Power and Tune Pwr on a Core whose catalogue gives no transmit
    /// ranges: 0 to 100, shown as the number.
    static let powerFallback = StationCatalog.TransmitRange(min: 0, max: 100, step: 1)
    /// `tuneDrivePowerSource`'s drive slider: RF Power sets it, as the
    /// desktop's slider does, so TUNE follows RF Power until Tune Pwr moves.
    static let driveSliderSource: Int64 = 0

    /// PureSignal's automatic calibration on and off (`ps3.automatic`,
    /// `ps3.off`, at `psAlgorithmVersion` 3 and the agreed minor 5).
    static let psaOnVerb = "ps3.automatic"
    static let psaOffVerb = "ps3.off"
    static let psaCapability = "psAlgorithmVersion"
    static let psaVersion: Int64 = 3
    static let psaMinor: UInt16 = 5
    /// PS-A arms off the air on a Core at `transmitSettingsVersion` 7.
    static let psaArmingVersion: Int64 = 7
    static let pureSignalKey = "pureSignal"
    static let pureSignalSettingsKey = "pureSignalSettings"
    static let radioKey = "radio"
    static let noPureSignalText = "This radio has no PureSignal."
    static let psaNotOfferedText = "This Core does not offer PureSignal to this phone. Updating the Core may help."
    static let psaNoRadioText = "PureSignal needs a connected radio that supports it."
    static let psaCannotRunText = "The Core cannot run PureSignal for this phone."
    /// An amplifier or tuner the Core has none of.
    static let notSetUpText = "Not set up"
    static let txProfileVerb = "txProfile.select"

    static let txStateKey = "txState"
    /// The transmit readings' version (D86) and the compression reading's
    /// name in `txState`. The name is the phone's expectation until the
    /// Core's link document fixes it; while no such property arrives,
    /// Compression stays off with its reason.
    static let txReadingsCapability = "txReadingsVersion"
    static let compressionReading = "compressionDb"
    static let transmitKey = "transmit"
    static let amplifierKey = "amplifier"
    static let rfKitKey = "rfkit"
    static let tunerKey = "tuner"

    let controller: PttController
    /// The Core's catalogue, for the transmit controls' ranges; nil reads
    /// the band's (``BandSlicesModel/catalog``).
    var catalog: StationCatalog? {
        didSet {
            if catalog != oldValue {
                queueRefresh()
            }
        }
    }
    /// This phone's device id, as `connectedDevices` and `txState` name it;
    /// nil until the phone has its key.
    var thisDeviceId: String? {
        didSet {
            queueRefresh()
        }
    }

    private let mirror: MirrorStore
    private let settings: SettingsProxyClient?
    private let commands: CommandClient?
    private let slices: BandSlicesModel
    private let subscriber: BandSubscriber?
    private let platform: Platform
    private let band: BandModel?
    /// The phone's microphone, started for a key or VOX; nil where there is
    /// none (screen tests).
    private let microphone: TransmitMicrophoneDemand?
    /// Takes transmit (``TransmitTakeModel/begin(sliceId:offered:granted:)``)
    /// for a PTT tap while another device holds it, or before VOX arms;
    /// true when the take's question came up or the take started. The
    /// closure given runs once the Core gave this phone transmit; a take
    /// cancelled or refused runs nothing. Set by the main screen.
    var askToTake: (((() -> Void)?) -> Bool)?
    /// A take of transmit is on its way or its question is up (the main
    /// screen's ``TransmitTakeModel``); VOX waits for it.
    var takeInProgress: (() -> Bool)?
    /// The newest transmit state report's number, for the PTT.
    private var reportSerial: UInt64 = 0
    private let captureVoxSender: (@Sendable () -> MirrorStore.BoundSender)?
    private var microphoneOwner: UInt64?
    private var microphoneRevision = 0
    private var microphoneAuthority = CommandSendPermit()
    /// Local stop intents retire synchronously when their logical session leaves.
    private var localStopAuthority = CommandSendPermit()
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watchedObjects: [String: ObjectIdentifier] = [:]
    private var refreshQueued = false
    private var pump: Task<Void, Never>?
    /// The holder's epoch and away flag last seen, to tell the band's
    /// subscription when either moves.
    private var lastHolder: (epoch: Int64, away: Bool)?
    /// Whether the screen was kept awake before this phone keyed.
    private var awakeBefore: Bool?
    #if DEBUG
    /// Lets tests await the exact TUNE action started by the screen.
    private(set) var lastTuneOperationForTesting: Task<Void, Never>?
    /// Parks a local stop before its first controller admission.
    var beforeLocalStopAdmissionForTesting: (@MainActor () async -> Void)?
    private(set) var lastLocalStopOperationForTesting: Task<Void, Never>?
    var beforeMicrophoneCloseAdmissionForTesting: (@MainActor () async -> Void)?
    private(set) var lastMicrophoneCloseOperationForTesting: Task<Void, Never>?
    /// VOX arms on their way.
    var armsInFlightForTesting: Int { armsInFlight }
    /// The microphone's losses heard so far, for a test to hear one late.
    var microphoneLossesForTesting: UInt64 { microphone?.lossCount ?? 0 }
    #endif
    /// The Core's `txState` says the radio is on the air: keyed, TUNE or
    /// two-tone. The lock-screen card reads it (I16); the settings gate
    /// reads `coreOnAir`, the desktop's rule (I1).
    private var txStateOnAir = false
    /// Asks the media connection for the transmit monitor's route; set by
    /// the app, nil where there is no media connection (screen tests).
    var monitorRouteSender: (@Sendable (MonitorRoute) async -> Void)?
    /// The Core's `monEnabled`, as the mirror holds it.
    private var coreMonEnabled = false
    /// MON's value while this phone's writes of it are on their way.
    private var monPending: Bool?
    private var monWritesInFlight = 0
    /// The route last handed to ``monitorRouteSender``, and its sends in order.
    private var monitorRouteSent: MonitorRoute = .none
    private var monitorRouteTask: Task<Void, Never>?
    private static let logger = Logger(subsystem: "NereusSDR", category: "tx.model")

    init(mirror: MirrorStore, commands: CommandClient?, slices: BandSlicesModel, subscriber: BandSubscriber?,
         band: BandModel? = nil,
         controller: PttController? = nil,
         microphone: (any MicrophoneSource)? = nil, uplink: MediaUplink? = nil,
         settings: SettingsProxyClient? = nil,
         captureVoxSender: (@Sendable () -> MirrorStore.BoundSender)? = nil,
         platform: Platform = Platform()) {
        self.mirror = mirror
        self.settings = settings
        self.commands = commands
        self.slices = slices
        self.subscriber = subscriber
        self.platform = platform
        self.captureVoxSender = captureVoxSender
        self.band = band
        let microphone = microphone.map { TransmitMicrophoneDemand(source: $0) }
        self.microphone = microphone
        // The key waits for the microphone to start (PttController bounds it).
        let startMicrophone: @Sendable () async -> MicrophoneStart
        if let microphone {
            startMicrophone = { await microphone.start() }
        } else {
            startMicrophone = { .started }
        }
        // The keepalive rides the media connection's "tx" channel while it is open.
        var keepaliveOnMedia: (@Sendable (Int64, Int64) -> Bool)?
        if let uplink {
            keepaliveOnMedia = { sequence, epoch in uplink.sendKeepalive(sequence: sequence, epoch: epoch) }
        }
        if let controller {
            self.controller = controller
        } else if let commands {
            self.controller = PttController(commands: TransmitCommandClient(commands: commands,
                                                                            keepaliveOnMedia: keepaliveOnMedia),
                                            startMicrophone: startMicrophone)
        } else {
            self.controller = PttController(commands: NoTransmitCommands(), startMicrophone: startMicrophone)
        }
        let snapshots = self.controller.snapshots
        pump = Task { [weak self] in
            for await snapshot in snapshots {
                self?.show(snapshot)
            }
        }
        mirror.$unconfirmedWrites.sink { [weak self] _ in
            Task { @MainActor in self?.confirmationRevision &+= 1 }
        }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] capabilities in
            guard let self else { return }
            switch capabilities["setupDescriptionVersion"] {
            case .int(let version)?, .enumeration(let version)?:
                if version >= 15 { self.profileCanonicalSession = self.mirror.snapshotIdentity }
            default: break
            }
            self.queueRefresh()
        }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isStale.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        band?.$settings.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        band?.$transmit.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        settings?.$values.sink { [weak self] values in
            let protection = Self.swrProtection(values)
            if self?.swrProtection != protection {
                self?.swrProtection = protection
            }
        }.store(in: &watches)
        // The microphone stopping by itself ends the key it was carrying.
        // Each loss is numbered where it happens, before the hop to the
        // main queue, so a late one is known for the run it belongs to.
        microphone?.onLost { [weak self] reason, loss in
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    self?.microphoneLost(reason, loss: loss)
                }
            }
        }
        // Locking the phone ends this phone's transmission (D24, spec 4.7):
        // the key, and VOX, which would key again at the next sound.
        platform.notificationCenter.publisher(for: UIApplication.protectedDataWillBecomeUnavailableNotification)
            .sink { [weak self] _ in
                MainActor.assumeIsolated {
                    guard let self else { return }
                    self.queueLocalStop(.lock, intent: self.beginLocalStop())
                }
            }
            .store(in: &watches)
    }

    deinit {
        pump?.cancel()
    }

    /// What a model reaches in iOS itself: the lock notification, background
    /// time, and the screen's idle timer. The app uses iOS's own; a test
    /// gives each model its own, so no test hears another's lock, records
    /// another's background time or reads another's screen setting.
    struct Platform {
        /// Where the lock (`protectedDataWillBecomeUnavailable`) is heard.
        var notificationCenter: NotificationCenter = .default
        /// How a stop of everything here asks iOS for background time; the
        /// returned closure says it is done.
        var backgroundWork: @MainActor (String) -> @MainActor () -> Void = { name in
            TransmitModel.beginBackgroundWork(name)
        }
        /// Whether the screen is kept awake now.
        var isScreenAwake: @MainActor () -> Bool = { UIApplication.shared.isIdleTimerDisabled }
        /// Keeps the screen awake, or lets it sleep.
        var setScreenAwake: @MainActor (Bool) -> Void = { awake in
            TransmitModel.keepScreenAwake(awake)
        }
    }

    /// Asks iOS for time to finish `name` in the background; the returned
    /// closure says it is done. iOS may end the time first, which ends it too.
    static func beginBackgroundWork(_ name: String) -> @MainActor () -> Void {
        let work = BackgroundWork()
        work.identifier = UIApplication.shared.beginBackgroundTask(withName: name) {
            work.end()
        }
        return { work.end() }
    }

    /// The Core's SWR protection from its settings: off where the Core has
    /// not set it, as the Core reads it; nil before any settings came.
    static func swrProtection(_ values: [String: String]) -> SwrProtection? {
        guard !values.isEmpty else {
            return nil
        }
        let limit = values[swrProtectionLimitKey].flatMap { Double($0.trimmingCharacters(in: .whitespaces)) }
        return SwrProtection(on: values[swrProtectionEnabledKey] == "True",
                             limit: limit.flatMap { $0.isFinite && $0 > 0 ? $0 : nil })
    }

    /// The system's idle timer: off while this phone transmits.
    static func keepScreenAwake(_ awake: Bool) {
        UIApplication.shared.isIdleTimerDisabled = awake
    }

    @Published private(set) var confirmationRevision: UInt64 = 0
    func isUnconfirmed(_ property: String) -> Bool { mirror.isUnconfirmed(Self.transmitKey, property: property) }
    // MARK: The link

    /// The session's state: the PTT follows the link; a new session starts
    /// idle and keys nothing by itself.
    func sessionChanged(_ state: StationSession.State, owner: UInt64) async {
        let up = state == .ready
        if microphoneOwner != owner || !up {
            localStopAuthority.revoke()
            localStopAuthority = CommandSendPermit()
            microphoneOwner = owner
            retireMicrophoneDemand()
            beginStop()
        }
        await controller.logicalSessionChanged(owner)
        await controller.linkChanged(up: up)
    }

    // MARK: The operator

    /// The PTT, and the TX panel's MOX.
    func tapPtt() {
        guard offered else {
            heldNote = Self.noRemoteTransmitText
            return
        }
        switch ptt.state {
        case .heldElsewhere:
            // Nothing is sent to key. Where the take is offered its question
            // comes up, as the TX button's does; the take never keys.
            if askToTake?(nil) == true {
                heldNote = nil
                return
            }
            heldNote = permission?.reason
            return
        case .waiting:
            // Nothing is sent: taking transmit arrives with the Core's take (Task 77).
            heldNote = permission?.reason
            return
        default:
            heldNote = nil
        }
        let controller = controller
        Task { await controller.tap(trigger: TransmitVerb.screenTrigger) }
    }

    /// The TX panel's TUNE.
    func toggleTune() {
        let controller = controller
        let operation = Task { await controller.toggleTune() }
        #if DEBUG
        lastTuneOperationForTesting = operation
        #endif
    }

    /// The Tuner Genius's TUNE (the TX panel's tuner row and the Tuner
    /// Genius page): the Core's autotune, keyed and ended through the PTT's
    /// one queue as TUNE is (`tx.tunerTune`, link 18.6), so PTT and Stop end
    /// it too. Greyed with ``tunerTuneReason`` it starts nothing; this
    /// phone's own tune always ends.
    func toggleTunerTune() {
        guard tunerTuneReason == nil else {
            return
        }
        let controller = controller
        let operation = Task { await controller.toggleTunerTune() }
        #if DEBUG
        lastTuneOperationForTesting = operation
        #endif
    }

    /// Why the tuner's TUNE may not start now, or nil when it may (or when
    /// this phone's own tune is on, which it ends). The desktop remote
    /// window's rules (TunerApplet.cpp:620-651): transmit permitted, the
    /// Core takes the tune, transmit not blocked, and the radio off the
    /// air, since the cycle switches the amplifier to standby first (the
    /// Core refuses it on the air, RemoteKeying.cpp:690-699).
    var tunerTuneReason: String? {
        if ptt.tunerTuning {
            return nil
        }
        if !permitted {
            return permission?.reason ?? Self.noRemoteTransmitText
        }
        if !tunerTuneOffered {
            return Self.tunerTuneOlderCoreText
        }
        if let inhibitReason {
            return inhibitReason
        }
        if coreOnAir || ptt.transmitting {
            return Self.onAirText
        }
        return nil
    }

    /// The Core told this phone its tuner tune ended without keying (the
    /// `tuneEnded` notice): it ends here too, without a send.
    func tunerTuneEndedUnkeyed() {
        let controller = controller
        Task { await controller.tunerTuneEndedUnkeyed() }
    }

    /// Stop, on the TX pill: everything of this phone's that is on ends.
    func stopAll() {
        let controller = controller
        let owner = microphoneOwner
        let authority = CommandSendPermit(parents: [localStopAuthority])
        Task { await controller.stopAll(owner: owner, authority: authority) }
    }

    /// Clears the refusal or stop the band shows.
    func dismissNotice() {
        heldNote = nil
        let controller = controller
        Task { await controller.dismissNotice() }
    }

    /// The fix a refusal offers: "Operate amp" puts the amplifier in operate.
    func applyFix(_ fix: String) {
        guard fix == TxRefusalInfo.operateAmp else {
            return
        }
        setAmpOperate(true)
        dismissNotice()
    }

    /// The RF Power slider: `power`, then, on a Core at version 5, the tune
    /// power source back to the drive slider, as the desktop's slider
    /// writes it. The per-band power tables belong to the Core alone: the
    /// Core keeps the TX band's slot from `power`, and the phone never
    /// writes `powerByBandJson` or `tunePowerByBandJson`.
    func setRfPower(_ value: Double) {
        guard settingsEditable(1) else {
            return
        }
        let watts = Int64(RxPanelModel.snapped(value, to: powerControl.range).rounded())
        write(Self.transmitKey, "power", .int(watts))
        guard settingsEditable(5) else {
            return
        }
        write(Self.transmitKey, "tuneDrivePowerSource", .enumeration(Self.driveSliderSource))
    }

    /// The Tune Pwr slider: the tune power for the band the Core transmits on.
    func setTunePower(_ value: Double) {
        guard settingsEditable(2) || !tunePowerAvailable else {
            return
        }
        guard tunePowerAvailable else {
            note = Self.tunePowerOlderCoreText
            return
        }
        let watts = Int64(RxPanelModel.snapped(value, to: tuneControl.range).rounded())
        // Shown at the touch and kept until the Core answers, as a property
        // write is (StationClient.cpp:1040-1068).
        invoke("setTunePowerForTxBand", [CommandArgument(name: "watts", value: .int(watts))],
               shows: [.init(Self.transmitKey, "tunePowerForTxBand", .int(watts))])
    }

    func toggleProc() {
        guard settingsEditable(2) else {
            return
        }
        write(Self.transmitKey, "cpdrOn", .bool(!proc))
    }

    /// VOX level, in dB.
    func setVoxThreshold(_ value: Double) {
        guard settingsEditable(2) else {
            return
        }
        write(Self.transmitKey, "voxThresholdDb",
              .int(Int64(RxPanelModel.snapped(value, to: Self.voxThresholdRange).rounded())))
    }

    /// VOX delay (its hang time), in milliseconds.
    func setVoxHang(_ value: Double) {
        guard settingsEditable(2) else {
            return
        }
        write(Self.transmitKey, "voxHangTimeMs",
              .int(Int64(RxPanelModel.snapped(value, to: Self.voxHangRange).rounded())))
    }

    // Nil-default interfaces let the binding regression compile before behavior lands.
    func toggleAntiVox() {
        guard settingsEditable(1), let antiVoxRun else { return }
        write(Self.transmitKey, "antiVoxRun", .bool(!antiVoxRun))
    }
    func setAntiVoxGain(_ value: Double) {
        guard settingsEditable(5), antiVoxGainDb != nil, value.isFinite else { return }
        write(Self.transmitKey, "antiVoxGainDb", .int(Int64(min(max(value, -60), 60).rounded())))
    }
    func setAntiVoxTime(_ value: Double) {
        guard settingsEditable(1), antiVoxTauMs != nil, value.isFinite else { return }
        write(Self.transmitKey, "antiVoxTauMs", .int(Int64(min(max(value, 1), 500).rounded())))
    }

    var antiVoxRunReason: String? { antiVoxReason(1, present: antiVoxRun != nil) }
    var antiVoxTimeReason: String? { antiVoxReason(1, present: antiVoxTauMs != nil) }
    var antiVoxGainReason: String? { antiVoxReason(5, present: antiVoxGainDb != nil) }

    private func antiVoxReason(_ version: Int64, present: Bool) -> String? {
        guard mirror.isSnapshotComplete else { return nil }
        guard settingsVersion >= version, present else {
            return "This Core does not send this anti-VOX control. Updating the Core may help."
        }
        return settingsReason(version)
    }

    func toggleDexp() {
        guard settingsEditable(2) else {
            return
        }
        write(Self.transmitKey, "dexpEnabled", .bool(!(dexp ?? false)))
    }

    /// The AM carrier level, in percent.
    func setAmCarrier(_ value: Double) {
        guard settingsEditable(2) else {
            return
        }
        write(Self.transmitKey, "amCarrierLevel",
              .int(Int64(RxPanelModel.snapped(value, to: Self.amCarrierRange).rounded())))
    }

    /// A TX profile, by the Core's name for it (`txProfile.select`).
    func selectProfile(_ name: String, owner: UUID? = nil) {
        guard settingsEditable(3), name != activeProfile else {
            return
        }
        if usesLegacyProfileSelection {
            invoke(Self.txProfileVerb, [CommandArgument(name: "name", value: .text(name))],
                   shows: [.init(Self.transmitKey, "activeTxProfile", .text(name))])
        } else if let flow = profileFlow { flow.choose(name, owner: owner ?? profileModelOwner) }
        else { note = SetupControlDispatcher.updatingReason }
    }

    /// 2-Tone: the Core's two-tone test, keyed through the PTT's one queue as TUNE is.
    /// Greyed with ``twoToneReason`` it starts nothing: lit by another
    /// keyer's two-tone test, a tap is not this phone's to stop.
    func toggleTwoTone() {
        guard twoToneReason == nil else {
            return
        }
        let on = !ptt.twoTone
        let controller = controller
        let operation = Task { await controller.setTwoTone(on) }
        #if DEBUG
        lastTuneOperationForTesting = operation
        #endif
    }

    /// Why 2-Tone is greyed: the Core's two-tone test is on (`twoToneOn`)
    /// and is not this phone's, so the radio is on the air for someone
    /// else (the Core refuses a start then, TxRefusal.cpp:204-209). Nil
    /// while this phone's own two-tone is on, so a tap ends it.
    var twoToneReason: String? {
        if ptt.twoTone {
            return nil
        }
        return twoToneOn ? Self.onAirText : nil
    }

    /// PS-A: PureSignal's automatic calibration on, or off (off is always
    /// taken, as a stop is).
    func togglePsa() {
        if psa {
            guard psaVerbsOffered else {
                return
            }
            invoke(Self.psaOffVerb, [], shows: [.init(Self.pureSignalSettingsKey, "autoCalEnabled", .bool(false))])
        } else {
            guard psaReason == nil else {
                return
            }
            invoke(Self.psaOnVerb, [], shows: [.init(Self.pureSignalSettingsKey, "autoCalEnabled", .bool(true))])
        }
    }

    /// The Core takes `ps3.automatic` and `ps3.off`.
    var psaVerbsOffered: Bool {
        (mirror.agreedMinor ?? 0) >= Self.psaMinor && mirror.capabilityVersion(Self.psaCapability) == Self.psaVersion
    }

    // MARK: The TX filter's edges

    /// Opens the number pad for the TX filter's low or high edge.
    func openTxFilterPad(low: Bool) {
        pad?.retire()
        pad = txFilterPad(low: low) { [weak self] in self?.closePad() }
    }

    func closePad() {
        pad?.retire()
        pad = nil
    }

    /// A number pad for one TX filter edge, within the desktop's range for
    /// it; nil while the setting may not change.
    func txFilterPad(low: Bool, close: @escaping () -> Void) -> ValuePadModel? {
        guard settingsEditable(1) else {
            return nil
        }
        let property = low ? "filterLow" : "filterHigh"
        let store = mirror
        return ValuePadModel(title: low ? "TX filter low edge" : "TX filter high edge", unit: "Hz",
                             range: low ? Self.txFilterLowRange : Self.txFilterHighRange,
                             current: low ? txFilterLowHz : txFilterHighHz,
                             sendWithLate: { [weak self] hz, late in
                                 guard let self, self.settingsEditable(1) else {
                                     return nil
                                 }
                                 return await store.write(Self.transmitKey, property: property, value: .int(hz),
                                                          onLateOutcome: late)
                             }, onOutcome: { [weak self] in self?.noteWrite($0) },
                             readCurrent: { RxPanelModel.number(store.object(Self.transmitKey)?[property]) }, close: close)
    }

    /// Match RX (D81): the TX filter becomes the active slice's receive
    /// filter in audio hertz (``TxFilterMatch``), written as the TX filter's
    /// own `filterLow` and `filterHigh`, in the order that never leaves the
    /// low edge above the high one. A transmit setting: nothing is sent
    /// while the settings may not change.
    func matchRxFilter() {
        guard settingsEditable(1), let slice = RxPanelModel.sliceObject(slices.activeSliceId, in: mirror),
              let edges = TxFilterMatch.edges(slice: slice, modes: slices.catalog?.modes ?? []) else {
            return
        }
        let writes = TxFilterMatch.writes(edges, currentHighHz: txFilterHighHz)
        let store = mirror
        // Each pre-held edge keeps its touch identity through task admission.
        let edits = writes.map { property, value in
            store.hold(Self.transmitKey, property: property, value: .int(value))
        }
        let current = { writes.indices.allSatisfy { index in
            edits[index].map { store.isCurrent(Self.transmitKey, property: writes[index].0, edit: $0) } ?? false
        } }
        Task { [weak self] in
            for (index, (property, value)) in writes.enumerated() {
                guard current() else { return }
                let outcome = await store.write(Self.transmitKey, property: property, value: .int(value), edit: edits[index],
                                                onLateOutcome: { [weak self] outcome in
                    if current() { self?.noteWrite(outcome) }
                })
                if current() { self?.noteWrite(outcome) }
                if !outcome.accepted {
                    for next in writes.indices where next > index {
                        if let edit = edits[next], store.isCurrent(Self.transmitKey, property: writes[next].0, edit: edit) {
                            store.releaseUnsent(Self.transmitKey, property: writes[next].0)
                        }
                    }
                    return
                }
            }
        }
    }

    /// VOX: armed by writing `transmit.voxEnabled`; while this phone has no
    /// microphone line it stays disabled with the Core's words. Arming
    /// starts the microphone first, so the Core's VOX hears this phone
    /// from the moment it is on; a refused arm stops it again.
    func toggleVox() {
        guard microphoneLine else {
            note = Self.voxNeedsMicrophone
            return
        }
        let on = !vox
        if on, !report.heldHere, let askToTake {
            // VOX keys on sound, not on a person, so the Core arms it only
            // for the device that holds transmit (StationServer.cpp:7619-7627,
            // TransmitHolder::isHeldBy): where the take is offered, transmit
            // is taken first (tx.take, which never keys) and VOX arms once
            // the Core gave it, unless a stop came in between.
            let generation = stopGeneration
            let owner = microphoneOwner
            let started = askToTake { [weak self] in
                guard let self, self.stopGeneration == generation, self.microphoneOwner == owner,
                      self.microphoneLine, !self.vox else {
                    return
                }
                self.setVox(true)
            }
            if started {
                return
            }
            if takeInProgress?() == true {
                // A take is still on its way, or its question is up: this
                // phone does not hold transmit, so VOX does not arm now.
                // A take an earlier VOX tap started arms VOX once the Core
                // gives transmit; this tap adds nothing.
                return
            }
        }
        setVox(on)
    }

    /// VOX on or off at the Core: on starts the microphone first.
    private func setVox(_ on: Bool) {
        let store = mirror
        let controller = controller
        let microphone = microphone
        // A stop of everything here while this arm is on its way wins: the
        // arm goes no further, and the stop sends VOX off.
        let generation = stopGeneration
        let owner = microphoneOwner
        let authority = microphoneAuthority
        let sender = captureVoxSender?()
        if on {
            armsInFlight += 1
        }
        Task { [weak self] in
            defer {
                if on {
                    self?.armsInFlight -= 1
                }
            }
            if on {
                guard await controller.mayArmVox(owner: owner, authority: authority) else { return }
                guard self?.stopGeneration == generation, self?.microphoneOwner == owner,
                      !authority.isRevoked else { return }
            }
            if on, let microphone {
                let result = await microphone.start()
                guard self?.stopGeneration == generation, self?.microphoneOwner == owner,
                      !authority.isRevoked else { return }
                if case .failed(let reason) = result {
                    self?.note = reason
                    return
                }
            }
            guard self?.stopGeneration == generation, self?.microphoneOwner == owner,
                  !authority.isRevoked else { return }
            let outcome: PropertyWriteOutcome
            if let sender {
                // VOX arms the transmitter: it keeps the keying path's wait
                // for the Core's answer, never the operator's value at the touch.
                outcome = await store.writeBound(Self.transmitKey, property: "voxEnabled", value: .bool(on),
                                                 sender: sender, authority: authority, holdsOperatorValue: false)
            } else {
                outcome = await store.write(Self.transmitKey, property: "voxEnabled", value: .bool(on),
                                            holdsOperatorValue: false)
            }
            guard self?.stopGeneration == generation, self?.microphoneOwner == owner,
                  !authority.isRevoked else { return }
            self?.noteOutcome(outcome.accepted, outcome.reason, answered: outcome.answeredByCore)
            if outcome.accepted {
                await controller.setVoxArmed(on, owner: owner, authority: authority)
            } else if on {
                let wanted = await controller.snapshot.microphoneWanted
                guard self?.stopGeneration == generation, self?.microphoneOwner == owner,
                      !authority.isRevoked else { return }
                if !wanted { microphone?.stop() }
            }
        }
    }

    /// Moves with each stop of everything here, so an arm that began before
    /// it never arms after it.
    private var stopGeneration = 0
    /// VOX arms on their way (the microphone starting, or the write out).
    private var armsInFlight = 0

    private func retireMicrophoneDemand() {
        microphoneRevision += 1
        microphoneAuthority.revoke()
        microphoneAuthority = CommandSendPermit()
    }

    /// The first, immediate part of a stop of everything here: the
    /// microphone stops and any arm on its way is overtaken.
    private func beginStop() {
        stopGeneration += 1
        retireMicrophoneDemand()
        microphone?.stop()
    }

    /// The media connection carries this phone's microphone line, or no
    /// longer does.
    func microphoneLineChanged(_ carried: Bool) {
        set(\.microphoneLine, carried)
    }

    /// The Core closed this phone's microphone line's track while the media
    /// connection stays up: a PTT key held here is released through the
    /// PTT, as a tap releases it, and the band shows the Core's words for a
    /// key without its microphone line. The line going with the connection
    /// never comes here; the link's own end stops a key then.
    func microphoneTrackClosed() {
        closeMicrophoneTrack(owner: microphoneOwner, sourceAuthority: nil)
    }

    /// The app forwards the producer's ownership; reading the current owner
    /// at consumption cannot establish where an already-buffered close began.
    func microphoneTrackClosed(owner: UInt64, sourceAuthority: CommandSendPermit) {
        closeMicrophoneTrack(owner: owner, sourceAuthority: sourceAuthority)
    }

    private func closeMicrophoneTrack(owner: UInt64?, sourceAuthority: CommandSendPermit?) {
        guard owner == microphoneOwner, !(sourceAuthority?.isRevoked ?? false) else { return }
        let authority = CommandSendPermit(parents: [localStopAuthority] + [sourceAuthority].compactMap { $0 })
        let controller = controller
        let text = Self.voxNeedsMicrophone
        let operation = Task { [weak self] in
            #if DEBUG
            await self?.beforeMicrophoneCloseAdmissionForTesting?()
            #endif
            guard self?.microphoneOwner == owner, !authority.isRevoked else { return }
            let released = await controller.microphoneLineClosed(text, owner: owner, authority: authority)
            guard self?.microphoneOwner == owner, !authority.isRevoked else { return }
            if released {
                Self.logger.info("The Core closed the microphone line while keyed; the key is released")
            }
        }
        #if DEBUG
        lastMicrophoneCloseOperationForTesting = operation
        #endif
    }

    // MARK: MON (D80)

    /// The Core sends this phone its transmit monitor.
    private var monitorOffered: Bool {
        mirror.capabilityVersion(Self.monitorCapability) >= 1
    }

    /// MON on or off, from the TX panel or the Modes tab: one write of the
    /// Core's `monEnabled`, and the route this phone asks for follows it.
    func toggleMon() {
        guard monReason == nil else {
            return
        }
        setMon(!(monPending ?? mon))
    }

    /// The phone's sound moved to or from headphones. Losing them with MON
    /// on asks for route none and writes `monEnabled` off, so MON shows off
    /// and the loudspeaker never plays the monitor.
    func headphonesChanged(_ headphones: Bool) {
        guard headphones != onHeadphones else {
            return
        }
        set(\.onHeadphones, headphones)
        if !headphones, monitorOffered, monPending ?? coreMonEnabled {
            setMon(false)
        }
        updateMon(mirror.object(Self.transmitKey))
    }

    /// The media connection's news for MON: the Core's answer to this
    /// phone's `monitor-audio`, and the connection ending (the Core forgets
    /// the route with it; the media client asks again on the next one).
    func monitorEvent(_ event: MediaControlEvent) {
        switch event {
        case .monitorAudioContext(let context):
            set(\.monitorApplied, context.route)
        case .mediaState(.closed), .mediaState(.failed), .mediaState(.disconnected):
            set(\.monitorApplied, .none)
        default:
            return
        }
        updateMon(mirror.object(Self.transmitKey))
    }

    private let monOutcomeOwner = ControlOutcomeOwner()

    private func setMon(_ on: Bool) {
        let noteEdit = monOutcomeOwner.begin()
        let edit = mirror.hold(Self.transmitKey, property: Self.monEnabledProperty, value: .bool(on))
        monPending = on
        monWritesInFlight += 1
        syncMonitorRoute()
        let store = mirror
        Task { [weak self] in
            let outcome = await store.write(Self.transmitKey, property: Self.monEnabledProperty, value: .bool(on), edit: edit,
                                            onLateOutcome: { [weak self] outcome in
                guard let self, self.monOutcomeOwner.isCurrent(noteEdit) else { return }
                self.noteWrite(outcome)
            })
            guard let self else {
                return
            }
            if self.monOutcomeOwner.isCurrent(noteEdit) { self.noteWrite(outcome) }
            self.monWritesInFlight -= 1
            if self.monWritesInFlight == 0 {
                self.monPending = nil
            }
            self.updateMon(self.mirror.object(Self.transmitKey))
        }
    }

    /// MON's lit state and reason from the Core's `monEnabled`, the route
    /// it applied and the phone's output; the route asked for follows.
    private func updateMon(_ transmit: MirrorObject?) {
        coreMonEnabled = transmit?[Self.monEnabledProperty].flag ?? false
        let reason: String? = !monitorOffered ? Self.monNotSentText
            : (onHeadphones ? nil : Self.monNeedsHeadphonesText)
        set(\.monReason, reason)
        set(\.mon, coreMonEnabled && monitorApplied != .none)
        syncMonitorRoute()
    }

    /// Headphones while the Core sends the monitor, the phone's sound is in
    /// headphones and MON is on (or being turned on); none otherwise. Sent
    /// only when it changes, in order.
    private func syncMonitorRoute() {
        let wanted: MonitorRoute = monitorOffered && onHeadphones && (monPending ?? coreMonEnabled)
            ? .headphones : .none
        guard wanted != monitorRouteSent, let sender = monitorRouteSender else {
            return
        }
        monitorRouteSent = wanted
        let previous = monitorRouteTask
        monitorRouteTask = Task {
            await previous?.value
            await sender(wanted)
        }
    }

    /// A call or Siri began (R-IOS-13): this phone's key ends at once, its
    /// VOX is disarmed and the microphone stops. Nothing comes back when the
    /// interruption ends: the next key is the operator's.
    func interrupted() {
        // The microphone stops at once; the rest follows in order.
        queueLocalStop(.interruption, intent: beginLocalStop())
    }

    /// The phone's media services reset: every audio object is gone, the
    /// microphone's input too. As for a call, this phone's key ends at
    /// once, its VOX is disarmed and the microphone stops; then the
    /// microphone is built again for whatever still wants it (the level).
    /// Nothing keys again by itself.
    func mediaServicesReset() {
        let intent = beginLocalStop()
        microphone?.servicesReset()
        queueLocalStop(.interruption, intent: intent)
    }

    /// The microphone stopped by itself while this phone wanted it (it
    /// could not start again after the sound moved): nothing is being
    /// sent, so the key ends, VOX is disarmed and the band says why. A
    /// loss reported after the microphone was no longer wanted changes
    /// nothing. Nothing keys.
    ///
    /// `loss` numbers the loss where it happened (``TransmitMicrophoneDemand``):
    /// one that happened before the microphone's present run started
    /// belongs to an earlier run, whose key is already over, and is heard
    /// late; it changes nothing, so a new key it would otherwise release
    /// stays on.
    func microphoneLost(_ reason: String, loss: UInt64? = nil) {
        guard ptt.microphoneWanted else {
            return
        }
        if let loss, let microphone, !microphone.lossIsCurrent(loss) {
            Self.logger.info("A microphone loss from an earlier run was heard late; it changes nothing")
            return
        }
        queueLocalStop(.microphoneLost, intent: beginLocalStop(), microphoneLoss: reason)
    }

    /// Why everything of this phone's was stopped.
    enum StopCause: Equatable, Sendable {
        /// The phone was locked (D24).
        case lock
        /// UNKEY on the lock-screen card or the Dynamic Island.
        case unkeyButton
        /// A call or Siri (R-IOS-13), or the media services resetting.
        case interruption
        /// The microphone stopped by itself (``microphoneLost(_:)``).
        case microphoneLost
    }

    /// A stop of everything here, once it has been sent: its cause, and
    /// whether this phone was transmitting when it began.
    struct LocalStop: Equatable, Sendable {
        let cause: StopCause
        /// Moves with each stop, so the same cause twice reads as two.
        let serial: Int
        let wasTransmitting: Bool
    }

    /// How long a stop waits for the Core's answer to VOX off.
    static let voxDisarmWait: Duration = .seconds(2)

    /// Runs `work` and returns when it ends or `limit` has passed, whichever is first.
    static func waitAtMost(_ limit: Duration, _ work: @escaping @MainActor () async -> Void) async {
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            let once = ResumeOnce(continuation)
            Task { @MainActor in
                await work()
                once.resume()
            }
            Task {
                try? await Task.sleep(for: limit)
                once.resume()
            }
        }
    }

    /// The newest stop of everything here, published once it has been sent.
    @Published private(set) var localStop: LocalStop?
    private var localStopSerial = 0

    /// Stops everything of this phone's that transmits or could: the key
    /// (PTT, TUNE or two-tone) through the PTT's one ordered queue, VOX
    /// (disarmed here and at the Core, since an armed VOX keys again at the
    /// next sound), and the microphone. It runs under background time from
    /// iOS so the Core hears it before the app is suspended, returns once
    /// every command has its answer, and then publishes ``localStop``.
    /// Nothing here keys.
    private struct LocalStopIntent {
        let owner: UInt64?
        let generation: Int
        let authority: CommandSendPermit
        let sender: MirrorStore.BoundSender?
        let wasTransmitting: Bool
        let voxPending: Bool
    }

    /// Capture at the event entry, before a scheduled task can borrow a new owner.
    private func beginLocalStop() -> LocalStopIntent {
        let owner = microphoneOwner
        let authority = CommandSendPermit(parents: [localStopAuthority])
        let sender = captureVoxSender?()
        let wasTransmitting = transmittingHere
        let voxPending = ptt.voxArmed || armsInFlight > 0
        beginStop()
        return LocalStopIntent(owner: owner, generation: stopGeneration, authority: authority,
                               sender: sender, wasTransmitting: wasTransmitting, voxPending: voxPending)
    }

    private func ownsLocalStop(_ intent: LocalStopIntent) -> Bool {
        microphoneOwner == intent.owner && stopGeneration == intent.generation && !intent.authority.isRevoked
    }

    private func queueLocalStop(_ cause: StopCause, intent: LocalStopIntent, microphoneLoss: String? = nil) {
        let operation = Task { [weak self] in
            guard let self else { return }
            await self.completeLocalStop(cause, intent: intent, microphoneLoss: microphoneLoss)
        }
        #if DEBUG
        lastLocalStopOperationForTesting = operation
        #endif
    }

    func stopEverythingHere(_ cause: StopCause) async {
        await completeLocalStop(cause, intent: beginLocalStop())
    }

    private func completeLocalStop(_ cause: StopCause, intent: LocalStopIntent, microphoneLoss: String? = nil) async {
        let finish = platform.backgroundWork("Stop transmitting")
        defer { finish() }
        let controller = controller
        #if DEBUG
        await beforeLocalStopAdmissionForTesting?()
        #endif
        guard ownsLocalStop(intent) else { return }
        let snapshot = await controller.snapshot
        guard ownsLocalStop(intent) else { return }
        let armed = snapshot.voxArmed || intent.voxPending
        if let microphoneLoss {
            await controller.microphoneLost(microphoneLoss, owner: intent.owner, authority: intent.authority)
            guard ownsLocalStop(intent) else { return }
        }
        await controller.stopAll(owner: intent.owner, authority: intent.authority)
        guard ownsLocalStop(intent) else { return }
        if armed {
            await controller.setVoxArmed(false, owner: intent.owner, authority: intent.authority)
            guard ownsLocalStop(intent) else { return }
            // The write goes at once; its answer is waited for a little, so a
            // Core that never answers can't hold the background time.
            let store = mirror
            let answer = VoxOffAnswer()
            await Self.waitAtMost(Self.voxDisarmWait) { [weak self] in
                guard self?.ownsLocalStop(intent) == true else { return }
                let outcome: PropertyWriteOutcome
                if let sender = intent.sender {
                    outcome = await store.writeBound(Self.transmitKey, property: "voxEnabled", value: .bool(false),
                                                     sender: sender, authority: intent.authority, holdsOperatorValue: false)
                } else {
                    outcome = await store.write(Self.transmitKey, property: "voxEnabled", value: .bool(false),
                                                holdsOperatorValue: false)
                }
                guard self?.ownsLocalStop(intent) == true else { return }
                answer.outcome = outcome.accepted ? "accepted" : outcome.answeredByCore ? "refused" : "unanswered"
            }
            guard ownsLocalStop(intent) else { return }
            if let outcome = answer.outcome {
                Self.logger.info("VOX off on a stop: \(outcome, privacy: .public)")
            } else {
                Self.logger.info("VOX off on a stop: no answer within 2 s")
            }
        }
        await controller.settle()
        guard ownsLocalStop(intent) else { return }
        localStopSerial += 1
        localStop = LocalStop(cause: cause, serial: localStopSerial, wasTransmitting: intent.wasTransmitting)
    }

    /// Sleep only admits an idle actor-owned TX session. It retires the local
    /// microphone demand and disarms VOX through the old session's bound
    /// property sender; it never releases a fresh operator key.
    func stopIdleVoxForSleep(owner: UInt64, authority: CommandSendPermit,
                             stillAllowed: @escaping @MainActor () -> Bool) async -> Bool {
        guard stillAllowed(), microphoneOwner == owner, !authority.isRevoked else { return false }
        let sender = captureVoxSender?()
        guard let sender, await controller.beginIdleSleepRetirement(owner: owner, authority: authority) else {
            return false
        }
        var keepGateForDisconnect = false
        defer {
            if !keepGateForDisconnect {
                Task { await controller.endIdleSleepRetirement(owner: owner, authority: authority) }
            }
        }
        guard await controller.ownsIdleSleepRetirement(owner: owner, authority: authority) else { return false }
        guard stillAllowed(), microphoneOwner == owner, !authority.isRevoked else { return false }
        // The actor gate now rejects new PTT/TUNE/VOX starts. This immediate
        // local stop uses the accepted D88 revision and arm invalidation.
        beginStop()
        await controller.setVoxArmed(false, owner: owner, authority: authority)
        guard await controller.ownsIdleSleepRetirement(owner: owner, authority: authority) else { return false }
        guard stillAllowed(), microphoneOwner == owner, !authority.isRevoked else { return false }
        let answer = VoxOffAnswer()
        let store = mirror
        await Self.waitAtMost(Self.voxDisarmWait) {
            let outcome = await store.writeBound(Self.transmitKey, property: "voxEnabled", value: .bool(false),
                                                 sender: sender, authority: authority, holdsOperatorValue: false)
            answer.outcome = outcome.accepted ? "accepted" : outcome.answeredByCore ? "refused" : "unanswered"
        }
        guard await controller.ownsIdleSleepRetirement(owner: owner, authority: authority) else { return false }
        guard stillAllowed(), microphoneOwner == owner, !authority.isRevoked else { return false }
        keepGateForDisconnect = true
        return true
    }

    /// The expiry keeps its actor gate through the guarded leave. A cancelled
    /// or replaced expiry can release only the gate it admitted.
    func releaseIdleSleepRetirement(owner: UInt64, authority: CommandSendPermit) async {
        await controller.endIdleSleepRetirement(owner: owner, authority: authority)
    }

    /// The amplifier's OPERATE (the Power Genius's or the RF-Kit's).
    func setAmpOperate(_ on: Bool) {
        guard ampOperateAvailable else {
            note = Self.ampOlderCoreText
            return
        }
        switch amp?.kind {
        case .powerGenius?:
            invoke("setPgxlOperate", [CommandArgument(name: "on", value: .bool(on))],
                   shows: [.init(Self.amplifierKey, "operate", .bool(on))])
        case .rfKit?:
            invoke("setRfKitOperate", [CommandArgument(name: "on", value: .bool(on))],
                   shows: [.init(Self.rfKitKey, "operate", .bool(on))])
        default:
            break
        }
    }

    /// The tuner's OPERATE.
    func setTunerOperate(_ on: Bool) {
        guard tunerOperateAvailable else {
            note = Self.tunerOlderCoreText
            return
        }
        invoke("setTgxlOperate", [CommandArgument(name: "on", value: .bool(on))],
               shows: [.init(Self.tunerKey, "isOperate", .bool(on))])
    }

    /// The amplifier's OPERATE verb for the amp the Core has, on this Core
    /// (`setPgxlOperate` at `remotePgxlControlVersion` 4, `setRfKitOperate`
    /// at `remoteRfKitControlVersion` 4).
    var ampOperateAvailable: Bool {
        switch amp?.kind {
        case .powerGenius?:
            return mirror.capabilityVersion("remotePgxlControlVersion") >= 4
        case .rfKit?:
            return mirror.capabilityVersion("remoteRfKitControlVersion") >= 4
        default:
            return false
        }
    }

    /// `setTgxlOperate`, at `remoteTgxlControlVersion` 2.
    var tunerOperateAvailable: Bool {
        mirror.capabilityVersion("remoteTgxlControlVersion") >= 2
    }

    /// `setTunePowerForTxBand`, at `transmitSettingsVersion` 2.
    var tunePowerAvailable: Bool {
        mirror.capabilityVersion("transmitSettingsVersion") >= 2
    }

    /// The version 1 transmit settings (RF power, the TX filter) may change.
    var settingsEditable: Bool {
        settingsEditable(1)
    }

    /// A transmit setting that came with `version` may change: the Core
    /// takes it, and its radio is off the air or the Core takes the
    /// settings on the air (``settingsOnAirVersion``); the desktop's rule (I1).
    func settingsEditable(_ version: Int64) -> Bool {
        mirror.isSnapshotComplete && (mirror.agreedMinor ?? 0) >= Self.settingsMinor
            && settingsVersion >= max(version, 1)
            && (settingsVersion >= Self.settingsOnAirVersion || !coreOnAir)
    }

    /// The outcome of a transmit setting written elsewhere (the Modes tab's
    /// mic gain, PROC's level, LEV, EQ and CFC): a refusal shows the Core's
    /// words with this model's own, in the TX panel and the Transmit
    /// section, and the next change taken clears them.
    func noteSettingOutcome(_ accepted: Bool, _ reason: String, answered: Bool) {
        noteOutcome(accepted, reason, answered: answered)
    }

    /// Why a transmit setting that came with `version` cannot change now,
    /// or nil when it can (or while the Core is still sending its state).
    func settingsReason(_ version: Int64) -> String? {
        guard mirror.isSnapshotComplete, !settingsEditable(version) else {
            return nil
        }
        return coreOnAir ? Self.onAirText : Self.settingsNotTakenText
    }

    // MARK: Sending

    private lazy var writes = PropertyWriteQueue(store: mirror) { [weak self] _, outcome in
        self?.noteWrite(outcome)
    }

    /// A transmit setting's write, shown at the touch and kept until the
    /// Core answers (`StationClient.cpp:1040-1068`), sent one at a time per
    /// property with the newest value next (``PropertyWriteQueue``).
    private func write(_ key: String, _ property: String, _ value: MirrorValue) {
        writes.write(key, property, value)
    }

    /// What came of a setting's write that showed the operator's value: a
    /// refusal shows the Core's words, and a write not sent, not answered
    /// in time or cut off by a dropped link says so in plain words.
    private func noteWrite(_ outcome: PropertyWriteOutcome) {
        guard outcome.isCurrent else { return }
        if outcome.heldForQuestion {
            // Held for this phone's question, not refused (StationClient.cpp:7308-7317).
            return
        }
        if !outcome.answeredByCore {
            Self.logger.info("A transmit setting's write: \(outcome.reason, privacy: .private)")
            note = outcome.reason
            return
        }
        noteOutcome(outcome.accepted, outcome.reason, answered: true)
    }

    /// A command that changes a mirrored value, shown at the touch and kept
    /// until the Core answers (``CommandHoldQueue``).
    private func invoke(_ verb: String, _ arguments: [CommandArgument], shows: [CommandHoldQueue.Shown]) {
        guard let commands else {
            noteWrite(.notSent)
            return
        }
        commandHolds.send(verb, shows: shows, invokeWithLate: { late in
            try await commands.invokeHeld(verb, arguments: arguments, timeout: .seconds(5), onLateOutcome: { outcome in
                await MainActor.run { late(outcome) }
            })
        }, onOutcome: { [weak self] outcome in
            self?.noteWrite(PropertyWriteOutcome(outcome))
        })
    }

    private lazy var commandHolds = CommandHoldQueue(store: mirror)

    private func noteOutcome(_ accepted: Bool, _ reason: String, answered: Bool) {
        if accepted {
            if note != nil {
                note = nil
            }
        } else if answered, !reason.isEmpty {
            note = reason
        }
    }

    // MARK: Reading

    private func show(_ snapshot: PttController.Snapshot) {
        guard snapshot.logicalSessionOwner == microphoneOwner else { return }
        let voxWasArmed = ptt.voxArmed
        let wasTransmitting = ptt.transmitting
        let micWasWanted = ptt.microphoneWanted
        if ptt != snapshot {
            ptt = snapshot
        }
        // The unkey, VOX disarmed, the link gone: the microphone stops.
        if micWasWanted, !snapshot.microphoneWanted {
            retireMicrophoneDemand()
            microphone?.stop()
        } else if !micWasWanted, snapshot.microphoneWanted, voxWasArmed, snapshot.voxArmed,
                  let microphone {
            // Only restore already armed VOX after its off fence is accepted.
            // Initial PTT/VOX starts retain their existing prerequisites.
            let revision = microphoneRevision
            let owner = microphoneOwner
            let authority = microphoneAuthority
            Task { [weak self] in
                guard let self, self.microphoneRevision == revision,
                      self.microphoneOwner == owner, self.ptt.microphoneWanted else { return }
                let result = await microphone.start()
                guard self.microphoneRevision == revision, self.microphoneOwner == owner,
                      self.ptt.microphoneWanted else { return }
                if case .failed(let reason) = result {
                    self.note = reason
                    await self.controller.setVoxArmed(false, owner: owner, authority: authority)
                }
            }
        }
        updateTransmittingHere()
        switch snapshot.state {
        case .heldElsewhere, .waiting:
            break
        default:
            if heldNote != nil {
                heldNote = nil
            }
        }
        if snapshot.transmitting != wasTransmitting {
            screenAwake(snapshot.transmitting)
            queueRefresh()
        }
    }

    private func updateTransmittingHere() {
        set(\.transmittingHere, ptt.transmitting || (report.keyed && report.heldHere))
        let elsewhere = txStateOnAir && !transmittingHere && !report.heldHere
        set(\.onAirElsewhere, elsewhere ? (report.held ? report.holderLabel : "") : nil)
    }

    /// Keeps the screen awake while this phone transmits, whatever it was
    /// set to, and puts it back after.
    private func screenAwake(_ transmitting: Bool) {
        if transmitting {
            if awakeBefore == nil {
                awakeBefore = platform.isScreenAwake()
            }
            platform.setScreenAwake(true)
        } else {
            platform.setScreenAwake(awakeBefore ?? false)
            awakeBefore = nil
        }
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

    func refresh() {
        let state = watch(Self.txStateKey)
        let transmit = watch(Self.transmitKey)
        let amplifier = watch(Self.amplifierKey)
        let rfkit = watch(Self.rfKitKey)
        let tunerObject = watch(Self.tunerKey)
        let radio = watch(Self.radioKey)
        let pureSignal = watch(Self.pureSignalKey)
        let pureSignalSettings = watch(Self.pureSignalSettingsKey)

        // Permission, from the capabilities.
        let caps = mirror.capabilities
        set(\.offered, mirror.capabilityVersion("remoteTxVersion") >= 1)
        set(\.tunerTuneOffered, (mirror.agreedMinor ?? 0) >= Self.tunerTuneMinor
            && mirror.capabilityVersion("remoteTxVersion") >= Self.tunerTuneVersion)
        let allowed = caps["txPermitted"].flag ?? false
        set(\.permitted, offered && allowed)
        let reason = caps["txRefusalReason"].text ?? ""
        set(\.permission, reason.isEmpty ? nil
            : TxRefusalInfo(reason: reason, code: caps["txRefusalCode"].text ?? "",
                            fix: caps["txRefusalFix"].text ?? ""))

        // The Core's transmit state.
        var next = TransmitStateReport()
        if let state {
            next.keyed = state["keyed"].flag ?? false
            next.txEnding = state["txEnding"].flag ?? false
            next.stopReason = state["stopReason"].text ?? ""
            next.stopText = state["stopText"].text ?? ""
            next.stopSerial = state["stopSerial"].whole ?? 0
            next.stopEpoch = state["stopEpoch"].whole ?? 0
            let holderId = state["holderDeviceId"].text ?? ""
            next.held = !holderId.isEmpty
            if next.held {
                if let mine = thisDeviceId {
                    next.heldHere = holderId == mine
                } else {
                    // Without its own id the phone reads the Core's permission:
                    // transmit held and still permitted is held here.
                    next.heldHere = permitted
                }
            }
            next.holderName = state["holderName"].text ?? ""
            next.holderShortName = state["holderShortName"].text ?? ""
            next.holderSource = state["holderSource"].text ?? ""
            next.holderEpoch = state["holderEpoch"].whole ?? 0
            next.holderAway = state["holderAway"].flag ?? false
            next.holderTransferring = state["holderTransferring"].flag ?? false
            set(\.forwardWatts, state["forwardPowerWatts"].number ?? 0)
            set(\.swr, state["swr"].number ?? 1)
            set(\.micLevelDb, state["micLevelDb"].number ?? -400)
            set(\.keyedForSeconds, state["keyedForSeconds"].whole ?? 0)
            set(\.timeOutRemainingSeconds, state["timeOutRemainingSeconds"].whole ?? -1)
            let txSliceId = state["txSliceId"].whole.map(Int.init) ?? -1
            let radioOnAir = next.keyed && next.holderSource == TransmitStateReport.radioPttSource
                && slices.entries.contains { $0.id == txSliceId }
            set(\.radioOnAirSliceId, radioOnAir ? txSliceId : nil)
        }
        txStateOnAir = next.keyed || state?["tuning"].flag == true || state?["twoTone"].flag == true
        if next != report {
            report = next
            updateTransmittingHere()
            if state != nil {
                let controller = controller
                // Numbered: each goes on a task of its own, and an older one
                // landing after a newer one changes nothing at the PTT.
                reportSerial += 1
                let serial = reportSerial
                Task { await controller.update(next, serial: serial) }
            }
        }
        updateTransmittingHere()
        if state != nil {
            let holder = (next.holderEpoch, next.holderAway)
            if let last = lastHolder, last != holder {
                subscriber?.transmitHolderChanged()
            }
            lastHolder = holder
        }

        // The S-meter's transmit readings (D86): the Core sends them in
        // `txState`; compression comes with `txReadingsVersion` 1.
        let keyedReadingsCurrent = mirror.isSnapshotComplete && !mirror.isStale
        set(\.keyedReadingsCurrent, keyedReadingsCurrent)
        let keyedPower = state?["forwardPowerWatts"].number
        let keyedRatio = state?["swr"].number
        set(\.keyedForwardWatts, keyedReadingsCurrent ? keyedPower.flatMap { $0.isFinite && $0 >= 0 ? $0 : nil } : nil)
        set(\.keyedSwr, keyedReadingsCurrent ? keyedRatio.flatMap { $0.isFinite && $0 >= 1 ? $0 : nil } : nil)
        set(\.keyedMicLevelDb, keyedReadingsCurrent ? TxStage.reading(state?["micLevelDb"].number) : nil)
        set(\.readingsSent, state != nil)
        set(\.txReadingsVersion, mirror.capabilityVersion(Self.txReadingsCapability))
        set(\.compressionDb, state?[Self.compressionReading].number)

        // The TX panel's settings.
        set(\.rfPower, transmit?["power"].number)
        set(\.tunePower, transmit?["tunePowerForTxBand"].number)
        let ranges = (catalog ?? slices.catalog)?.board.transmit
        set(\.powerControl, ranges?.power ?? Self.powerFallback)
        set(\.tuneControl, ranges?.tunePowerForTxBand ?? Self.powerFallback)
        set(\.proc, transmit?["cpdrOn"].flag ?? false)
        let voxWas = vox
        set(\.vox, transmit?["voxEnabled"].flag ?? false)
        if voxWas, !vox, ptt.voxArmed {
            // The Core turned VOX off (another device, or the Core itself).
            let controller = controller
            Task { await controller.setVoxArmed(false) }
        }
        updateMon(transmit)
        set(\.voxThresholdDb, transmit?["voxThresholdDb"].number)
        set(\.voxHangMs, transmit?["voxHangTimeMs"].number)
        set(\.antiVoxRun, transmit?["antiVoxRun"].flag)
        set(\.antiVoxGainDb, transmit?["antiVoxGainDb"].number)
        set(\.antiVoxTauMs, transmit?["antiVoxTauMs"].number)
        set(\.monitorVolume, transmit?["monitorVolume"].number)
        set(\.dexp, transmit?["dexpEnabled"].flag)
        set(\.amCarrier, transmit?["amCarrierLevel"].number)
        set(\.activeProfile, transmit?["activeTxProfile"].text.flatMap { $0.isEmpty ? nil : $0 })
        set(\.profiles, Self.profileNames(transmit?["txProfilesJson"].text))

        // The transmit settings gate: the Core's version, and whether its radio is on the air.
        set(\.settingsVersion, mirror.capabilityVersion(Self.settingsCapability))
        set(\.twoToneOn, pureSignal?["twoToneOn"].flag ?? false)
        // The radio's transmit inhibit (hl2-port-2): the Core's reason, or the
        // inhibit input's words when it sends none.
        let inhibitWords = radio?["txInhibitReason"].text ?? ""
        set(\.inhibitReason, radio?["txInhibited"].flag == true
            ? (inhibitWords.isEmpty ? Self.inhibitInputText : inhibitWords) : nil)
        set(\.coreOnAir, radio?["transmitting"].flag == true || transmit?["tune"].flag == true || twoToneOn)
        set(\.micMuted, settingsVersion >= Self.micMutedVersion && transmit?["micMuted"].flag == true)

        // The transmit stage meters (JJ, 2026-09-28): the Core's seven
        // readings at `txReadingsVersion` 3, and each one's recent peak,
        // kept here from the readings while the radio is on the air.
        let stagesLive = txStateOnAir || coreOnAir
        let readings: [Double?] = txReadingsVersion >= TxStage.readingsVersion
            ? TxStage.all.map { TxStage.reading(state?[$0.property].number) } : TxStage.empty
        if readings != stageReadings || stagesLive != stagesOnAir {
            set(\.stagePeaks, stagesLive ? TxStage.peaks(stagePeaks, readings) : TxStage.empty)
        }
        set(\.stageReadings, readings)
        set(\.stagesOnAir, stagesLive)

        // PS-A.
        set(\.psa, pureSignalSettings?["autoCalEnabled"].flag ?? false)
        set(\.psaReason, psaCannotArm(pureSignal: pureSignal))

        // The amplifier and the tuner.
        var ampNow: Accessory?
        if let amplifier, amplifier["present"].flag == true {
            ampNow = Accessory(kind: .powerGenius, name: Self.name(amplifier, fallback: "Power Genius XL"),
                               operate: amplifier["operate"].flag ?? false)
        } else if let rfkit, rfkit["present"].flag == true {
            ampNow = Accessory(kind: .rfKit, name: Self.name(rfkit, fallback: "RF-Kit RF2K-S"),
                               operate: rfkit["operate"].flag ?? false)
        }
        set(\.amp, ampNow)
        var tunerNow: Accessory?
        if let tunerObject, tunerObject["isPresent"].flag == true {
            tunerNow = Accessory(kind: .tunerGenius, name: Self.name(tunerObject, fallback: "Tuner Genius XL"),
                                 operate: tunerObject["isOperate"].flag ?? false)
        }
        set(\.tuner, tunerNow)

        // The orange TX filter on the transmit slice while this phone is
        // keyed, or while the Core's radio is keyed on one of this band's
        // slices (the keyed view, desktop PR #317): about the carrier, which
        // XIT moves off the dial.
        var filter: ClosedRange<Double>?
        let keyedSlice = report.keyed
            ? state?["txSliceId"].whole.flatMap { id in slices.entries.first { $0.id == Int(id) } } : nil
        if ptt.transmitting || keyedSlice != nil, let low = transmit?["filterLow"].number,
           let high = transmit?["filterHigh"].number,
           let entry = keyedSlice ?? slices.entries.first(where: { $0.slice.txSlice }) ?? slices.active,
           band == nil || band?.transmit.keyedHere == true {
            let sliceObject = watch("slice:\(entry.id)")
            // With DUP the Core and desktop keep the receive span at the
            // VFO, and draw this filter without XIT on that span.
            let xitHz = band?.duplexActive == true ? 0
                : (sliceObject?["xitEnabled"].flag == true ? sliceObject?["xitHz"].number ?? 0 : 0)
            let passband = entry.slice.passbandHz
            filter = Self.txFilter(carrierHz: entry.slice.frequencyHz + xitHz,
                                   passband: (passband.lowerBound + xitHz)...(passband.upperBound + xitHz),
                                   lowHz: low, highHz: high,
                                   bothSides: TxFilterMatch.fromZeroModes.contains(entry.modeLabel))
        }
        set(\.txFilterHz, filter)
        set(\.txZeroLineSliceId, zeroLineSlice(state: state))
        set(\.txFilterLowHz, transmit?["filterLow"].whole)
        set(\.txFilterHighHz, transmit?["filterHigh"].whole)
    }

    /// The slice the TX zero line marks (``txZeroLineSliceId``): nil while
    /// the radio is off the air. The Core's `txSliceId` names the transmit
    /// slice; one on another pan leaves this band without the line. A Core
    /// that names none is taken at the slice marked for transmit, else,
    /// while this phone keys, the active slice.
    private func zeroLineSlice(state: MirrorObject?) -> Int? {
        guard ptt.transmitting || txStateOnAir || coreOnAir else {
            return nil
        }
        if let id = state?["txSliceId"].whole, id >= 0 {
            return slices.entries.first { $0.id == Int(id) }?.id
        }
        return (slices.entries.first(where: { $0.slice.txSlice }) ?? (ptt.transmitting ? slices.active : nil))?.id
    }

    /// The TX filter's span on the band: `transmit`'s `filterLow` and
    /// `filterHigh` are audio frequencies, on the side of the carrier the
    /// slice's own passband is on. In AM, SAM, DSB, FM and DRM
    /// (`bothSides`, ``TxFilterMatch/fromZeroModes``) the transmission
    /// fills both sides of the carrier out to the high edge and the low
    /// edge plays no part, so the span is the carrier plus and minus the
    /// high edge, as the desktop's orange TX filter spans it.
    static func txFilter(carrierHz: Double, passband: ClosedRange<Double>, lowHz: Double,
                         highHz: Double, bothSides: Bool = false) -> ClosedRange<Double>? {
        if bothSides {
            let high = abs(highHz)
            guard high > 0 else {
                return nil
            }
            return (carrierHz - high)...(carrierHz + high)
        }
        let low = min(abs(lowHz), abs(highHz))
        let high = max(abs(lowHz), abs(highHz))
        guard high > low else {
            return nil
        }
        let centre = (passband.lowerBound + passband.upperBound) / 2
        if centre < carrierHz {
            return (carrierHz - high)...(carrierHz - low)
        }
        return (carrierHz + low)...(carrierHz + high)
    }

    /// Why PS-A cannot be turned on, as the desktop's PS-A button decides
    /// it: the radio has PureSignal, the Core offers its controls and runs
    /// it, and arming is allowed (this phone may transmit, or a Core at
    /// version 7 takes it off the air).
    private func psaCannotArm(pureSignal: MirrorObject?) -> String? {
        guard mirror.isSnapshotComplete else {
            return nil
        }
        if slices.catalog?.board.pureSignal == false {
            return Self.noPureSignalText
        }
        guard psaVerbsOffered else {
            return Self.psaNotOfferedText
        }
        guard pureSignal?["available"].flag == true else {
            return Self.psaNoRadioText
        }
        guard pureSignal?["canActuate"].flag == true else {
            return Self.psaCannotRunText
        }
        if permitted || settingsEditable(Self.psaArmingVersion) {
            return nil
        }
        if settingsVersion >= Self.psaArmingVersion {
            return settingsReason(Self.psaArmingVersion)
        }
        return offered ? (permission?.reason ?? Self.settingsNotTakenText) : Self.noRemoteTransmitText
    }

    /// The Core's TX profile names from `txProfilesJson`, a JSON array of
    /// names in its order; none when it is not one.
    static func profileNames(_ json: String?) -> [String] {
        guard let json, let names = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [Any] else {
            return []
        }
        let strings = names.compactMap { $0 as? String }
        return strings.count == names.count ? strings : []
    }

    private static func name(_ object: MirrorObject, fallback: String) -> String {
        let model = object["deviceModel"].text ?? ""
        return model.isEmpty ? fallback : model
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<TransmitModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}

/// What the Core said to VOX off on a stop, once it said it.
@MainActor
private final class VoxOffAnswer {
    var outcome: String?
}

/// A continuation resumed by whichever of two tasks comes first.
private final class ResumeOnce: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Void, Never>?

    init(_ continuation: CheckedContinuation<Void, Never>) {
        self.continuation = continuation
    }

    func resume() {
        let waiting = lock.withLock { () -> CheckedContinuation<Void, Never>? in
            defer { continuation = nil }
            return continuation
        }
        waiting?.resume()
    }
}

/// One stretch of background time from iOS, ended once.
@MainActor
private final class BackgroundWork {
    var identifier = UIBackgroundTaskIdentifier.invalid

    func end() {
        guard identifier != .invalid else {
            return
        }
        UIApplication.shared.endBackgroundTask(identifier)
        identifier = .invalid
    }
}

/// Where no command client runs (screen tests without a session): nothing is sent.
private struct NoTransmitCommands: TransmitCommandSending {
    func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
        TransmitPending { .noAnswer }
    }

    func sendKeepalive(sequence: Int64, epoch: Int64) async {}
}

private extension Optional where Wrapped == MirrorValue {
    var flag: Bool? {
        if case .bool(let value)? = self {
            return value
        }
        return nil
    }

    var text: String? {
        if case .text(let value)? = self {
            return value
        }
        return nil
    }

    var whole: Int64? {
        switch self {
        case .int(let value)?, .enumeration(let value)?:
            return value
        case .double(let value)?:
            return value.isFinite ? Int64(value) : nil
        default:
            return nil
        }
    }

    var number: Double? {
        switch self {
        case .int(let value)?, .enumeration(let value)?:
            return Double(value)
        case .double(let value)?:
            return value.isFinite ? value : nil
        default:
            return nil
        }
    }
}

/// A stop retires a pending start without waiting for it. Completion reconciles
/// with current demand: OLD work can neither revive a stopped mic nor stop NEW.
@MainActor
private final class TransmitMicrophoneDemand {
    private let source: any MicrophoneSource
    private var generation = 0
    private var wanted = false
    private var running = false
    private var pending: Task<MicrophoneStart, Never>?

    init(source: any MicrophoneSource) { self.source = source }

    /// The source's input is gone with the media services: built again for
    /// whatever still wants it.
    func servicesReset() { source.servicesReset() }

    /// The losses so far, counted on the thread that heard each one.
    private let losses = MicrophoneLosses()
    /// The losses counted when the present run of the source started.
    private var runMark: UInt64 = 0

    /// The losses heard so far.
    var lossCount: UInt64 { losses.count }

    /// `lost` runs, on the source's thread, with the source's words and
    /// the loss's number, counted there at once.
    func onLost(_ lost: @escaping @Sendable (String, UInt64) -> Void) {
        let losses = losses
        source.onLost { reason in
            lost(reason, losses.next())
        }
    }

    /// The loss numbered `loss` belongs to the source's present run: it
    /// was heard after that run started.
    func lossIsCurrent(_ loss: UInt64) -> Bool {
        loss > runMark
    }

    func start() async -> MicrophoneStart {
        wanted = true
        if running { return .started }
        if let pending { return await pending.value }
        let admitted = generation
        // A loss heard from here on is this run's.
        runMark = losses.count
        let task = Task { [self] in
            let result = await source.start()
            guard admitted == generation else {
                // A NEW start may already be using this same source. Only
                // silence it when the current owner also wants it stopped.
                if !wanted { source.stop() }
                return MicrophoneStart.failed(reason: "Microphone start was canceled.")
            }
            pending = nil
            running = result == .started
            if !running { wanted = false; source.stop() }
            return result
        }
        pending = task
        return await task.value
    }

    func stop() {
        generation += 1
        pending = nil
        wanted = false
        running = false
        source.stop()
    }
}

/// The microphone's losses, counted on whichever thread hears them.
private final class MicrophoneLosses: @unchecked Sendable {
    private let lock = NSLock()
    private var heard: UInt64 = 0

    var count: UInt64 { lock.withLock { heard } }

    /// Counts one more loss and returns its number (from 1).
    func next() -> UInt64 {
        lock.withLock {
            heard += 1
            return heard
        }
    }
}
