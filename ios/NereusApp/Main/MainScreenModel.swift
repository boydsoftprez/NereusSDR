// NereusSDR for iOS: everything the main screen shows, fed from the app's model and the Core's media events
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
import UIKit

/// The main screen's models (R-IOS-11): the Core's catalogue, the band and
/// its slices, the display subscription the band sends, and the RX panel.
/// It lives as long as the app, like the clients it reads, so the band and
/// its subscription carry across reconnects. The Core's media events come
/// in through ``receive(_:)``.
@MainActor
final class MainScreenModel: ObservableObject {
    /// The phone's band is its first pan, shown on the toolbar as Pan 1.
    static let panNumber = 1

    let catalogFeed: CatalogFeed
    let band: BandModel
    let slices: BandSlicesModel
    let subscriber: BandSubscriber
    /// The iPad's pans: Pan 1 is this band; the others open below it.
    let panStack: PanStackModel
    let rx: RxPanelModel
    /// Setup edits either the focused supportedPan1 or the device master.
    /// Rendering/subscription always uses the same effective liveBandModel.
    enum DisplayEditScope: Hashable { case all; case pan(PanIdentity) }
    @Published var displayEditScope: DisplayEditScope = .pan(.first)
    /// Accepted HTML445/1108: All edits an independently chosen kept band;
    /// Pan1 always edits its actual band. Choosing here never retunes a slice.
    @Published var displayBandChoice: String? = nil
    var displayEditingBand: String? {
        displayEditScope == .all ? displayBandChoice ?? band.settings.scaleBand : band.settings.scaleBand
    }
    var displayEditingSettings: BandDisplaySettings {
        switch displayEditScope {
        case .pan(let pan) where pan == .first:
            return band.settings
        case .pan(let pan) where panStack.open.contains(pan):
            // An open pan below Pan 1: its own band's values (its pill).
            return panStack.band(for: pan).band.settings
        case .pan(let pan):
            // A pan not open now: its kept values, on Pan 1's band, as
            // the desktop's strip edits a closed pan (spec section 7.3).
            var settings = displaySettings.settings(forPan: pan.storeKey)
            settings.enterBand(band.settings.scaleBand)
            return settings
        case .all:
            var settings = displaySettings.masterSettings(forPan: BandSubscriber.panId)
            settings.enterBand(displayEditingBand)
            return settings
        }
    }
    /// The display store's key for a pan: Pan 1's is the band's own.
    private static func displayStoreKey(_ pan: PanIdentity) -> String {
        pan == .first ? BandSubscriber.panId : pan.storeKey
    }
    func ownDisplayFields(among fields: Set<String>) -> Set<String> {
        ownDisplayFields(among: fields, pan: .first)
    }
    /// The fields among `fields` that `pan` keeps its own value for.
    func ownDisplayFields(among fields: Set<String>, pan: PanIdentity) -> Set<String> {
        displaySettings.ownFields(forPan: Self.displayStoreKey(pan), among: fields)
    }
    /// Every field `pan` keeps its own value for, on any page: what the
    /// pill's Reset this pan clears (spec section 5.3, DC4).
    func ownDisplayFields(for pan: PanIdentity) -> Set<String> {
        ownDisplayFields(among: BandDisplaySettingsStore.pictureFields, pan: pan)
    }
    /// `pan` follows Setup again in `field`, the tap on a row's amber dot.
    /// A waterfall levels mode field takes the other three with it, as the
    /// store keeps the mode as one choice.
    func clearOwnDisplayField(_ field: String, pan: PanIdentity) {
        resetDisplayPicture(fields: [field], pan: pan)
    }
    /// Reset this pan: every own field of `pan` goes and it follows Setup in
    /// every row. Its zoom (`rxViewSpanHz`), its band and the band's scale
    /// are not picture fields, so they stay where they are.
    func resetDisplay(pan: PanIdentity) {
        displaySettings.reset(pan: Self.displayStoreKey(pan))
        reloadDisplaySettings()
    }
    func resetDisplayPicture(fields: Set<String>) {
        guard displayEditScope == .pan(.first) else { return }
        resetDisplayPicture(fields: fields, pan: .first)
    }
    /// `pan` follows All pans again in `fields` (the strip's Reset this pan).
    func resetDisplayPicture(fields: Set<String>, pan: PanIdentity) {
        displaySettings.resetPicture(forPan: Self.displayStoreKey(pan), fields: fields)
        reloadDisplaySettings()
    }
    func copyOwnDisplayPictureToMaster(fields: Set<String>) -> Bool {
        guard displayEditScope == .pan(.first) else { return false }
        return copyOwnDisplayPictureToMaster(fields: fields, pan: .first)
    }
    /// `pan`'s own values in `fields` become All pans' (the strip's Use
    /// Pan N's on all pans); false when it has none there.
    @discardableResult
    func copyOwnDisplayPictureToMaster(fields: Set<String>, pan: PanIdentity) -> Bool {
        guard !ownDisplayFields(among: fields, pan: pan).isEmpty else { return false }
        displaySettings.copyOwnPictureToMaster(forPan: Self.displayStoreKey(pan), fields: fields)
        reloadDisplaySettings()
        return true
    }
    func changeDisplay(fields: Set<String>, _ change: (inout BandDisplaySettings) -> Void) {
        changeDisplay(selectedFields: fields, change)
    }
    func displayEffectiveValue(_ readout: SetupDescription.EffectiveReadout) -> Double? {
        guard displayEditScope == .pan(.first) else { return nil }
        switch readout {
        case .waterfallLow: return band.effectiveWaterfallLevels.map { Double($0.lowDbm) }
        case .waterfallHigh: return band.effectiveWaterfallLevels.map { Double($0.highDbm) }
        case .spectrumMax, .spectrumMin:
            if band.settings.gridFollowsNoiseFloor && band.settings.waterfallLevelMode == .clarity
                && band.gridTracker.clarityFloorDbm == nil { return nil }
            return readout == .spectrumMax ? band.drawnSettings.scaleRange.upperBound : band.drawnSettings.scaleRange.lowerBound
        }
    }
    /// The toolbar's Pan 1 and Display sheets.
    let pan: PanSheetModel
    let display: DisplaySheetModel
    /// The Core's display settings, as its catalogue describes them.
    let coreDisplay: CoreDisplayModel
    /// Each band's dB Max and dB Min, as the Core keeps them for every device.
    let bandScale: CoreBandScale
    /// The flags' step menu and number pad.
    let tuning: BandTuningModel
    /// What the flags' buttons open and run: the antenna and more menus
    /// and the tab panels, each on its own slice.
    let flagControls: FlagControls
    /// Other devices' slices on the band.
    let foreign: ForeignSlicesModel
    /// Transmit: the PTT, the TX panel and the keyed view.
    let transmit: TransmitModel
    /// Take transmit from the device that holds it (`tx.take`).
    let take: TransmitTakeModel
    /// The TX panel's AM Mod Monitor (D102); the app sets it once its
    /// record streams exist, and a screen test may leave it out.
    var modMonitor: ModMonitorModel?
    /// The Display pill's rows and their dispatcher; the app sets and
    /// holds it, and a screen test may leave it out.
    weak var displayPillSource: DisplayPillSource?
    /// The Core's transmitter as the band follows it: keyed on one of its
    /// slices, the carrier, high SWR (Task 54f).
    let transmitDisplay: TransmitDisplayModel
    /// The Core's own transmit display settings, for Setup's Display page.
    let coreTxDisplay: CoreTxDisplaySettings
    /// The Modes tab: the active slice's full set.
    let modes: ModesTabModel
    /// The mic level meter's level from this phone's microphone before keying.
    let micLevel: LiveMicLevel
    /// The Core's amplifiers and tuner, for their pages under the Radio tab.
    let accessories: AccessoriesModel
    /// The Core's rotor, for the Rotor page the Radio and Tools tabs share.
    let rotor: RotorModel
    let keyedMeters: KeyedMetersModel
    /// The iPad's analog S-meter and its menu (D86).
    let sMeter: SMeterModel
    /// Every slice on the Core, behind the toolbar's Slice button (R-IOS-42).
    let sliceList: SliceListModel
    /// The Core's questions and notices about other devices; nil where no
    /// session feeds one (tests of other screens).
    let devices: SeveralDevicesClient?

    /// How the signal level is printed on the flags and the Live Activity:
    /// the Multimeter page's units and decimal point, kept with the S-meter's
    /// settings and followed here so the band redraws when they change.
    @Published private(set) var meterReadout: SMeterReadout = .desktopDefaults
    /// The S-meter's RX Mode, which every flag's level follows (the flag's
    /// more menu sets it), followed here so the band redraws when it changes.
    @Published private(set) var meterRxMode: SMeterRxMode = SMeterSettings.desktopDefaults.rxMode
    /// The Core's name, as its devices list gives it; nil until it says.
    @Published private(set) var coreName: String?
    /// Where the Core says it can be dialled, `devices`' `coreAddresses` as
    /// it sent it (``CoreAddressList``); nil while it sends none. The
    /// connecting flow keeps it with the paired Core.
    @Published private(set) var coreAddresses: String?
    /// The radio the Core runs, as the catalogue names it.
    @Published private(set) var radioName: String?
    /// Why the Core has no radio, in its own words (`radio`'s
    /// `stationRadioWaiting`, link 7.1); nil while it has one or says nothing.
    @Published private(set) var radioWaiting: String?
    /// The active slice's letter, `A`, and its colour from the catalogue:
    /// on the iPad, the active slice of the pan last touched.
    @Published private(set) var sliceLetter: String?
    @Published private(set) var sliceColour: String?
    /// The pan the toolbar acts on: the iPad's pan last touched, so its Pan
    /// button names it, its slice letter is that pan's active slice and
    /// its Display opens that pan's pill (R-IOS-59, DC9). Pan 1 on a phone.
    @Published private(set) var toolbarPan: PanIdentity = .first

    private let mirror: MirrorStore
    private let settingsProxy: SettingsProxyClient
    private let commands: CommandClient?
    /// The open pans' controls below Pan 1, made as each is first shown.
    private var otherControls: [PanIdentity: PanControls] = [:]
    /// Pan 1's controls: the main screen's own models.
    private var firstControls: PanControls?
    /// The followed pan's slices, for the toolbar's letter.
    private var followWatches: Set<AnyCancellable> = []
    private var followedSlices: BandSlicesModel?
    private let displaySettings: BandDisplaySettingsStore
    /// The view of the phone's own band while it shows another slice's,
    /// put back by Back to your band.
    private var homeView: TuneGestures.View?
    private var watches: Set<AnyCancellable> = []
    private var devicesWatch: AnyCancellable?
    private var watchedDevices: MirrorObject?
    private var radioWatch: AnyCancellable?
    private var watchedRadio: MirrorObject?
    private var refreshQueued = false
    /// A rename the Core accepted, shown until its devices object says so
    /// (or names the Core otherwise): the new name, and the name it had.
    private var renamed: (name: String, from: String)?

    /// The mirrored object that holds the Core's name.
    static let devicesKey = "devices"

    init(mirror: MirrorStore, settings: SettingsProxyClient, commands: CommandClient?,
         operations: BandSubscriber.Operations, displaySettings: BandDisplaySettingsStore = BandDisplaySettingsStore(),
         meterSettings: SMeterSettingsStore = SMeterSettingsStore(),
         keyedMeters: KeyedMetersModel = KeyedMetersModel(),
         deadline: Duration = BandSubscriber.acknowledgementDeadline, devices: SeveralDevicesClient? = nil,
         microphone: (any MicrophoneSource)? = nil, uplink: MediaUplink? = nil,
         captureVoxSender: (@Sendable () -> MirrorStore.BoundSender)? = nil,
         captureTakeSender: CommandClient.CaptureSender? = nil,
         platform: TransmitModel.Platform = TransmitModel.Platform()) {
        self.mirror = mirror
        settingsProxy = settings
        self.commands = commands
        self.keyedMeters = keyedMeters
        self.devices = devices
        foreign = ForeignSlicesModel(store: mirror)
        self.displaySettings = displaySettings
        catalogFeed = CatalogFeed(store: mirror)
        band = BandModel(settings: displaySettings.settings(forPan: BandSubscriber.panId))
        slices = BandSlicesModel(store: mirror, commands: commands)
        // Take it back of control worked: as after Take control, the first
        // key on the slice waits for its TX button.
        devices?.tookControlBack = { [weak slices] sliceId, revision in
            slices?.tookControl(sliceId, revision: revision)
        }
        let shared = SharedDisplayAllocator()
        subscriber = BandSubscriber(band: band, slices: slices, mirror: mirror, settings: settings,
                                    operations: operations, deadline: deadline, pan: .first, shared: shared)
        panStack = PanStackModel(first: PanBand(pan: .first, band: band, slices: slices, subscriber: subscriber),
                                 mirror: mirror, settings: settings, commands: commands, operations: operations,
                                 displaySettings: displaySettings, shared: shared, deadline: deadline,
                                 catalogFeed: catalogFeed, allowsMorePans: UIDevice.current.userInterfaceIdiom == .pad)
        transmitDisplay = TransmitDisplayModel(band: band, slices: slices, mirror: mirror)
        coreTxDisplay = CoreTxDisplaySettings(settings: settings, mirror: mirror)
        rx = RxPanelModel(store: mirror, slices: slices, catalogFeed: catalogFeed, commands: commands)
        transmit = TransmitModel(mirror: mirror, commands: commands, slices: slices, subscriber: subscriber, band: band,
                                 microphone: microphone, uplink: uplink, settings: settings,
                                 captureVoxSender: captureVoxSender, platform: platform)
        take = TransmitTakeModel(mirror: mirror, commands: commands, slices: slices, transmit: transmit,
                                 devices: devices, captureTakeSender: captureTakeSender)
        modes = ModesTabModel(store: mirror, slices: slices, catalogFeed: catalogFeed, commands: commands, rx: rx,
                              transmit: transmit)
        micLevel = LiveMicLevel(microphone: microphone, gain: modes.$micGainDb.eraseToAnyPublisher(),
                                allowed: transmit.$permitted.eraseToAnyPublisher(),
                                notificationCenter: platform.notificationCenter)
        tuning = BandTuningModel(slices: slices)
        flagControls = FlagControls(slices: slices, modes: modes, tuning: tuning, store: mirror)
        sMeter = SMeterModel(slices: slices, band: band, subscriber: subscriber,
                             transmit: transmit, catalogFeed: catalogFeed, store: meterSettings)
        sliceList = SliceListModel(store: mirror, slices: slices, commands: commands)
        accessories = AccessoriesModel(mirror: mirror, commands: commands, slices: slices, catalogFeed: catalogFeed,
                                       settings: settings)
        rotor = RotorModel(mirror: mirror, commands: commands)
        let band = band
        // A keyed zoom the band remembers is kept like any other change.
        band.keepSettings = { [weak band, displaySettings] kept in
            displaySettings.setSettings(kept, forPan: BandSubscriber.panId)
            guard let band else { return }
            var saved = displaySettings.settings(forPan: BandSubscriber.panId)
            saved.sideways = band.settings.sideways
            band.settings = saved
        }
        // A weak local owner is filled only after initialization. Pan 1's own
        // sheet, Core and pan models write Pan 1, whatever pan Setup's strip
        // or a pill has chosen, as every other pan's models write their own
        // (``controls(for:)``); without another subscriber, live pan registry
        // or strong ownership cycle.
        weak var displayOwner: MainScreenModel?
        let change: (_ change: (inout BandDisplaySettings) -> Void) -> Void = { change in
            displayOwner?.changeDisplay(pan: .first, change)
        }
        pan = PanSheetModel(store: mirror, slices: slices, catalogFeed: catalogFeed, commands: commands, band: band,
                            changeDisplay: change)
        sliceList.pan = pan
        coreDisplay = CoreDisplayModel(band: band, slices: slices, subscriber: subscriber, catalogFeed: catalogFeed,
                                       settingsProxy: settings, changeDisplay: change)
        bandScale = CoreBandScale(band: band, mirror: mirror, settingsProxy: settings, coreDisplay: coreDisplay)
        display = DisplaySheetModel(band: band, slices: slices, subscriber: subscriber, store: mirror,
                                    catalogFeed: catalogFeed, settingsProxy: settings, bandScale: bandScale,
                                    changeDisplay: change)
        catalogFeed.$catalog.sink { [weak self] catalog in
            guard let self else {
                return
            }
            self.band.catalog = catalog
            self.slices.catalog = catalog
            self.foreign.catalog = catalog
            self.queueRefresh()
        }.store(in: &watches)
        sMeter.$state.map(\.settings.readout).removeDuplicates().sink { [weak self] readout in
            self?.meterReadout = readout
        }.store(in: &watches)
        flagControls.sMeter = sMeter
        sMeter.$state.map(\.settings.rxMode).removeDuplicates().sink { [weak self] mode in
            self?.meterRxMode = mode
        }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        // The 3D view, on a Core that offers it.
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        // Stop on TX: the band knows while this phone transmits.
        transmit.$transmittingHere.sink { [weak self] keyed in
            guard let self else { return }
            self.band.keyed = keyed
            for pan in self.panStack.open where pan != .first {
                self.panStack.band(for: pan).band.keyed = keyed
            }
        }.store(in: &watches)
        // Another slice's band: the band remembers its own view and gets it back.
        slices.$showingAnotherBand.removeDuplicates().dropFirst().sink { [weak self] away in
            guard let self else {
                return
            }
            if away {
                self.homeView = self.band.requestedView
            } else {
                // Any way back to the phone's own band: the pan's own split returns.
                self.band.setJumpedShareFloor(nil)
                if let view = self.homeView {
                    self.homeView = nil
                    self.band.requestView(view)
                }
            }
        }.store(in: &watches)
        // The screen's notices place themselves by the split as shown.
        band.$jumpedShareFloor.removeDuplicates().dropFirst().sink { [weak self] _ in
            self?.objectWillChange.send()
        }.store(in: &watches)
        // PTT while showing a listened slice's band: back to the transmit slice's (JJ's ruling 7).
        transmit.$ptt.map { snapshot -> Bool in
            switch snapshot.state {
            case .keying, .keyed:
                return true
            default:
                return false
            }
        }.removeDuplicates().sink { [weak self] keying in
            if keying {
                self?.slices.backForTransmit()
            }
        }.store(in: &watches)
        devices?.$question.sink { [weak self] question in
            self?.slices.sendsOnlyFinalValue = question != nil
        }.store(in: &watches)
        // The Core's `tuneEnded` notice: this phone's tuner tune ended
        // without keying (link section 18.x notices; StationServer.cpp:2719-2729).
        // The banner shows its words; the PTT ends the tune here, once per notice.
        devices?.$notices.sink { [weak self] notices in
            guard let self else { return }
            if notices.isEmpty {
                // The link went (the client forgets its notices): a new
                // session's notices may reuse ids.
                self.tuneEndedSeen.removeAll()
            }
            for received in notices where received.notice.kind == Self.tuneEndedNotice
                && !self.tuneEndedSeen.contains(received.id) {
                self.tuneEndedSeen.insert(received.id)
                self.transmit.tunerTuneEndedUnkeyed()
            }
        }.store(in: &watches)
        // A PTT tap while another device holds transmit asks to take it,
        // where the take is offered (Task 54); the take never keys.
        transmit.askToTake = { [weak take] granted in
            take?.begin(sliceId: nil, granted: granted) ?? false
        }
        transmit.takeInProgress = { [weak take] in
            guard let take else { return false }
            return take.inFlight || take.questionUp
        }
        displayOwner = self
        display.focusPan = { [weak self] in self?.displayEditScope = .pan(.first) }
        // The RX panel, Modes, transmit's Match RX, the S-meter and the
        // toolbar follow the pan last touched (R-IOS-59, DC9). Both publish
        // before they store; the pan's band is already in place by then.
        panStack.$lastTouched.sink { [weak self] pan in
            self?.follow(pan)
        }.store(in: &watches)
        // Diversity arrives when its sheet first appears: it acts on the pan last touched from then on.
        flagControls.$diversity.compactMap { $0 }.sink { [weak self] diversity in
            guard let self else { return }
            let pan = self.panStack.open.contains(self.panStack.lastTouched) ? self.panStack.lastTouched : .first
            diversity.follow(self.panStack.band(for: pan).slices)
        }.store(in: &watches)
        panStack.$open.sink { [weak self] open in
            guard let self else { return }
            self.otherControls = self.otherControls.filter { pan, controls in
                open.contains(pan) && controls.band.band === self.panStack.band(for: pan).band
            }
            for pan in open where pan != .first {
                let band = self.panStack.band(for: pan).band
                band.keyed = self.transmit.transmittingHere
                band.stackOffered = self.band.stackOffered
            }
            self.follow(open.contains(self.panStack.lastTouched) ? self.panStack.lastTouched : .first)
            self.queueRefresh()
        }.store(in: &watches)
    }

    // MARK: The pans

    /// `pan`'s controls: Pan 1's are the main screen's own models; an open
    /// pan below it has its own, made the first time it is asked for and
    /// kept while it stays open. A pan not open gets Pan 1's.
    func controls(for pan: PanIdentity) -> PanControls {
        let shown = panStack.band(for: pan)
        guard shown.pan != .first else {
            if let firstControls {
                return firstControls
            }
            let made = PanControls(band: shown, tuning: tuning, flagControls: flagControls, sheet: self.pan,
                                   display: display, bandScale: bandScale)
            firstControls = made
            return made
        }
        if let kept = otherControls[shown.pan], kept.band.band === shown.band {
            return kept
        }
        let key = shown.pan
        let change: (_ change: (inout BandDisplaySettings) -> Void) -> Void = { [weak self] change in
            self?.changeDisplay(pan: key, change)
        }
        let tuning = BandTuningModel(slices: shown.slices)
        // The dial's haptics as Setup, Navigation sets them, as on Pan 1.
        let haptics = self.tuning.dialHaptics
        tuning.setDialHaptics(ticksOnDetents: haptics.ticksOnDetents, bumpsOnKilohertz: haptics.bumpsOnKilohertz)
        let flags = FlagControls(slices: shown.slices, modes: modes, tuning: tuning, store: mirror)
        // Diversity from the pan's flags: the one Diversity context, as Pan 1's flags have it.
        flags.diversity = flagControls.diversity
        let sheet = PanSheetModel(store: mirror, slices: shown.slices, catalogFeed: catalogFeed, commands: commands,
                                  band: shown.band, changeDisplay: change)
        let scale = CoreBandScale(band: shown.band, mirror: mirror, settingsProxy: settingsProxy,
                                  coreDisplay: coreDisplay)
        let display = DisplaySheetModel(band: shown.band, slices: shown.slices, subscriber: shown.subscriber,
                                        store: mirror, catalogFeed: catalogFeed, settingsProxy: settingsProxy,
                                        bandScale: scale, changeDisplay: change)
        display.focusPan = { [weak self] in self?.displayEditScope = .pan(key) }
        let made = PanControls(band: shown, tuning: tuning, flagControls: flags, sheet: sheet, display: display,
                               bandScale: scale)
        otherControls[key] = made
        return made
    }

    /// Which of the dial's haptics play, as Setup, Navigation sets them:
    /// on every open pan's dial, and on a pan opened later.
    func setDialHaptics(ticksOnDetents: Bool, bumpsOnKilohertz: Bool) {
        tuning.setDialHaptics(ticksOnDetents: ticksOnDetents, bumpsOnKilohertz: bumpsOnKilohertz)
        for controls in otherControls.values {
            controls.tuning.setDialHaptics(ticksOnDetents: ticksOnDetents, bumpsOnKilohertz: bumpsOnKilohertz)
        }
    }

    /// The panel, Modes, Match RX, the S-meter, Diversity and the toolbar
    /// act on `pan`'s active slice from now on.
    private func follow(_ pan: PanIdentity) {
        let shown = panStack.band(for: pan)
        rx.follow(shown.slices)
        modes.follow(shown.slices)
        flagControls.diversity?.follow(shown.slices)
        sMeter.follow(slices: shown.slices, band: shown.band, subscriber: shown.subscriber)
        transmit.receiveSlices = shown.pan == .first ? nil : shown.slices
        if toolbarPan != shown.pan {
            toolbarPan = shown.pan
        }
        if followedSlices !== shown.slices {
            followedSlices = shown.slices
            followWatches = []
            if shown.slices !== slices {
                shown.slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &followWatches)
                shown.slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &followWatches)
            }
        }
        queueRefresh()
    }

    /// The radio's name: the catalogue's product label, else the radio
    /// object's model; nil while neither says.
    static func radioName(catalog: StationCatalog?, radio: MirrorObject?) -> String? {
        if let label = catalog?.board.productLabel {
            return label
        }
        if case .text(let model)? = radio?["model"], !model.isEmpty {
            return model
        }
        return nil
    }

    /// The notice kind for a tuner tune that ended without keying.
    static let tuneEndedNotice = SeveralDevices.NoticeKind.other("tuneEnded")
    /// The `tuneEnded` notices already passed to the PTT.
    private var tuneEndedSeen: Set<Int64> = []

    /// The catalogue's label for a mode, `USB`, or "".
    func modeLabel(_ mode: Int) -> String {
        foreign.modeLabel(mode)
    }

    /// One of the media client's events, for the band and its subscription.
    func receive(_ event: MediaControlEvent) {
        if case .microphoneLine(let carried) = event {
            transmit.microphoneLineChanged(carried)
        }
        if case .microphoneTrackClosed = event {
            transmit.microphoneTrackClosed()
        }
        transmit.monitorEvent(event)
        band.receive(event)
        subscriber.receive(event)
        panStack.receive(event)
        display.receive(event)
        for controls in otherControls.values {
            controls.display.receive(event)
        }
        sMeter.refresh()
    }

    /// Changes this pan's display settings on this phone: kept at once, and
    /// the band redraws. The subscription follows only if its request changed.
    func changeDisplay(_ change: (inout BandDisplaySettings) -> Void) {
        changeDisplay(selectedFields: nil, change)
    }

    private func reloadDisplaySettings() {
        var saved = displaySettings.settings(forPan: BandSubscriber.panId)
        saved.sideways = band.settings.sideways // Current orientation is runtime-only.
        band.settings = saved
        // Every other open pan takes the store's values too: a shared row
        // changed on one pan shows on every pan (DC2, DC9).
        for pan in panStack.open where pan != .first {
            let other = panStack.band(for: pan).band
            var kept = displaySettings.settings(forPan: pan.storeKey)
            kept.sideways = other.settings.sideways
            if other.settings != kept {
                other.settings = kept
            }
        }
        objectWillChange.send()
    }

    /// Changes `pan`'s display settings, whatever pan Setup's strip or the
    /// pill has chosen: the band follows its slice's band number this way,
    /// and an open pan's own Display pill and Pan sheet write through it.
    func changeDisplay(pan: PanIdentity, _ change: (inout BandDisplaySettings) -> Void) {
        let before = pan == .first || !panStack.open.contains(pan) ? band.settings
            : panStack.band(for: pan).band.settings
        let key = panStack.open.contains(pan) ? pan : .first
        var edited = before
        change(&edited)
        if edited != before {
            displaySettings.setSettings(edited, forPan: Self.displayStoreKey(key))
        }
        reloadDisplaySettings()
    }

    private func changeDisplay(selectedFields: Set<String>?, _ change: (inout BandDisplaySettings) -> Void) {
        let before = displayEditingSettings
        var edited = before
        change(&edited)
        let selected = selectedFields ?? Self.changedDisplayFields(before, edited)
        if displayEditScope == .all {
            // The edited copy carries the chosen band's dictionaries. Restore
            // the real active band before saving so no runtime band/frame/VFO
            // changes, and editing a picture field never materializes a new
            // band's untouched defaults.
            if selected.isDisjoint(with: ["scaleTopDbm", "scaleBottomDbm", "threeDFloorDb", "threeDFloors"]) {
                edited.threeDFloors = band.settings.threeDFloors
            }
            edited.enterBand(band.settings.scaleBand)
            // Shared/device fields retain their existing authority. Master
            // picture writes explicitly clear the selected closed-pan own fields,
            // even when a selected value already equals the master.
            displaySettings.setSettings(edited, forPan: BandSubscriber.panId, includingPicture: false)
            displaySettings.setMasterSettings(edited, fields: selected)
        } else if edited != before, case .pan(let pan) = displayEditScope {
            displaySettings.setSettings(edited, forPan: Self.displayStoreKey(pan))
        }
        // Refresh the accepted snapshot, including a newer master/shared field
        // a stale local save was correctly forbidden from overwriting.
        reloadDisplaySettings()
    }

    /// Smooth's profile fields follow All's ordinary winner rule; its mode
    /// conversion is separate so closed nonmanual tuples are not overwritten.
    func applySmoothDisplayDefaults() {
        guard displayEditScope == .all else {
            changeDisplay(PhoneSetupKeys.smoothDefaults)
            return
        }
        // Each named profile row reaches every pan even if its master
        // already equals the preset (the reference's forced put calls).
        let fields: Set<String> = ["waterfallPaletteId", "spectrumAveraging", "spectrumAverageTimeMs",
            "traceColour", "traceLineColour", "traceFill", "waterfallPeriodMs"]
        changeDisplay(fields: fields) { settings in
            let clarity = settings.clarityEnabled, agc = settings.waterfallAgc
            let floor = settings.waterfallNfAgc, scale = settings.useSpectrumMinMax
            PhoneSetupKeys.smoothDefaults(&settings)
            settings.clarityEnabled = clarity; settings.waterfallAgc = agc
            settings.waterfallNfAgc = floor; settings.useSpectrumMinMax = scale
        }
        displaySettings.convertManualWaterfallModesForSmooth(forPan: BandSubscriber.panId)
        reloadDisplaySettings()
    }

    private static func changedDisplayFields(_ before: BandDisplaySettings, _ after: BandDisplaySettings) -> Set<String> {
        func values(_ settings: BandDisplaySettings) -> [String: Any] {
            guard let data = try? JSONEncoder().encode(settings),
                  let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return [:] }
            return object
        }
        let old = values(before), new = values(after)
        return Set(Set(old.keys).union(new.keys).filter { key in
            key != "storeSnapshotVersion"
                && !NSDictionary(dictionary: ["value": old[key] ?? NSNull()]).isEqual(to: ["value": new[key] ?? NSNull()])
        })
    }

    /// The main screen is turned sideways or upright: the band takes that
    /// way's spectrum share (D84). Nothing is kept for it.
    func setBandSideways(_ sideways: Bool) {
        guard band.settings.sideways != sideways else {
            return
        }
        band.settings.sideways = sideways
    }

    /// The Core offers the 3D view: its Setup description reaches version
    /// 12, where the 3D View page arrives, and it carries the 3D choice's
    /// gate, `remoteMediaVersion` 1 (the Core's V12 row).
    static func offersStack(capabilityVersion: (String) -> Int64) -> Bool {
        capabilityVersion(stackDescriptionCapability) >= stackDescriptionVersion
            && capabilityVersion("remoteMediaVersion") >= 1
    }

    static let stackDescriptionCapability = "setupDescriptionVersion"
    static let stackDescriptionVersion: Int64 = 12

    // MARK: The toolbar's words

    /// A rename the Core accepted is shown and its devices object hasn't caught up yet.
    var renamePending: Bool { renamed != nil }

    /// The Core accepted a rename to `name` (D76): the toolbar shows it at
    /// once, before the devices object's delta carries it.
    func coreRenamed(to name: String) {
        var current = ""
        if case .text(let label)? = mirror.object(Self.devicesKey)?["stationLabel"] {
            current = label
        }
        renamed = name == current ? nil : (name, current)
        queueRefresh()
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

    private func refresh() {
        let devices = mirror.object(Self.devicesKey)
        if devices !== watchedDevices {
            watchedDevices = devices
            devicesWatch = devices?.$values.dropFirst().sink { [weak self] _ in self?.queueRefresh() }
        }
        var label = ""
        if case .text(let text)? = devices?["stationLabel"] {
            label = text
        }
        if let pending = renamed, label != pending.from {
            // The devices object has caught up, or someone renamed it since.
            renamed = nil
        }
        let shown = renamed?.name ?? label
        let name: String? = shown.isEmpty ? nil : shown
        if coreName != name {
            coreName = name
        }
        var addresses: String?
        if case .text(let text)? = devices?[CoreAddressList.propertyName] {
            addresses = text
        }
        if coreAddresses != addresses {
            coreAddresses = addresses
        }
        let radioObject = mirror.object("radio")
        if radioObject !== watchedRadio {
            watchedRadio = radioObject
            radioWatch = radioObject?.$values.dropFirst().sink { [weak self] _ in self?.queueRefresh() }
        }
        var waiting: String?
        if case .text(let words)? = radioObject?["stationRadioWaiting"], !words.isEmpty {
            waiting = words
        }
        if radioWaiting != waiting {
            radioWaiting = waiting
        }
        let radio = Self.radioName(catalog: catalogFeed.catalog, radio: radioObject)
        if radioName != radio {
            radioName = radio
        }
        // The toolbar's slice: the pan last touched's active slice.
        let followed = panStack.band(for: toolbarPan).slices
        let letter = followed.activeSliceId.map(BandSlice.letter(forIndex:))
        if sliceLetter != letter {
            sliceLetter = letter
        }
        let colour = followed.active?.slice.colour
        if sliceColour != colour {
            sliceColour = colour
        }
        let offered = Self.offersStack { [mirror] in mirror.capabilityVersion($0) }
        if band.stackOffered != offered {
            band.stackOffered = offered
        }
        // The band the pan is on, by the Core's number: the Core's scale for
        // it (CoreBandScale) and its 3D Floor follow it.
        if let bandNumber = slices.active?.band {
            // Pan 1's band goes to Pan 1, whatever pan Setup's strip or a pill
            // has chosen: on All pans too, whose edit keeps the band it had.
            changeDisplay(pan: .first) { $0.enterBand(String(bandNumber)) }
        }
        // Each other open pan follows its own slice's band the same way.
        for pan in panStack.open where pan != .first {
            let shown = panStack.band(for: pan)
            if shown.band.stackOffered != offered {
                shown.band.stackOffered = offered
            }
            if let bandNumber = shown.slices.active?.band, shown.band.settings.scaleBand != String(bandNumber) {
                changeDisplay(pan: pan) { $0.enterBand(String(bandNumber)) }
            }
        }
    }
}
