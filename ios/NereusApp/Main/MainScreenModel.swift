// NereusSDR for iOS: everything the main screen shows, fed from the app's model and the Core's media events
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels

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
    let rx: RxPanelModel
    /// The toolbar's Pan 1 and Display sheets.
    let pan: PanSheetModel
    let display: DisplaySheetModel
    /// The Core's display settings, as its catalogue describes them.
    let coreDisplay: CoreDisplayModel
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
    /// The active slice's letter, `A`, and its colour from the catalogue.
    @Published private(set) var sliceLetter: String?
    @Published private(set) var sliceColour: String?

    private let mirror: MirrorStore
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
        subscriber = BandSubscriber(band: band, slices: slices, mirror: mirror, settings: settings,
                                    operations: operations, deadline: deadline)
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
        let band = band
        // A keyed zoom the band remembers is kept like any other change.
        band.keepSettings = { [displaySettings] kept in
            displaySettings.setSettings(kept, forPan: BandSubscriber.panId)
        }
        let change: (_ change: (inout BandDisplaySettings) -> Void) -> Void = { change in
            Self.changeDisplay(change, band: band, store: displaySettings)
        }
        pan = PanSheetModel(store: mirror, slices: slices, catalogFeed: catalogFeed, commands: commands, band: band,
                            changeDisplay: change)
        sliceList.pan = pan
        display = DisplaySheetModel(band: band, slices: slices, subscriber: subscriber, store: mirror,
                                    catalogFeed: catalogFeed, settingsProxy: settings, changeDisplay: change)
        coreDisplay = CoreDisplayModel(band: band, slices: slices, subscriber: subscriber, catalogFeed: catalogFeed,
                                       settingsProxy: settings, changeDisplay: change)
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
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        // The 3D view, on a Core that offers it.
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        // Stop on TX: the band knows while this phone transmits.
        transmit.$transmittingHere.sink { [weak self] keyed in
            self?.band.keyed = keyed
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
        display.receive(event)
        sMeter.refresh()
    }

    /// Changes this pan's display settings on this phone: kept at once, and
    /// the band redraws. The subscription follows only if its request changed.
    func changeDisplay(_ change: (inout BandDisplaySettings) -> Void) {
        Self.changeDisplay(change, band: band, store: displaySettings)
    }

    /// The main screen is turned sideways or upright: the band takes that
    /// way's spectrum share (D84). Nothing is kept for it.
    func setBandSideways(_ sideways: Bool) {
        guard band.settings.sideways != sideways else {
            return
        }
        band.settings.sideways = sideways
    }

    private static func changeDisplay(_ change: (inout BandDisplaySettings) -> Void, band: BandModel,
                                      store: BandDisplaySettingsStore) {
        var settings = band.settings
        change(&settings)
        guard settings != band.settings else {
            return
        }
        band.settings = settings
        store.setSettings(settings, forPan: BandSubscriber.panId)
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
        var radio = catalogFeed.catalog?.board.productLabel
        if radio == nil, case .text(let model)? = radioObject?["model"], !model.isEmpty {
            radio = model
        }
        if radioName != radio {
            radioName = radio
        }
        let letter = slices.activeSliceId.map(BandSlice.letter(forIndex:))
        if sliceLetter != letter {
            sliceLetter = letter
        }
        let colour = slices.active?.slice.colour
        if sliceColour != colour {
            sliceColour = colour
        }
        let offered = Self.offersStack { [mirror] in mirror.capabilityVersion($0) }
        if band.stackOffered != offered {
            band.stackOffered = offered
        }
        // Each band keeps its own dBm scale, as the desktop's per-band grid:
        // the active slice's band, by the Core's number.
        if let bandNumber = slices.active?.band {
            changeDisplay { $0.enterBand(String(bandNumber)) }
        }
    }
}
