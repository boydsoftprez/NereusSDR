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
    /// Setup edits either the focused supportedPan1 or the device master.
    /// Rendering/subscription always uses the same effective liveBandModel.
    enum DisplayEditScope: Hashable { case pan, all }
    @Published var displayEditScope: DisplayEditScope = .pan
    /// Accepted HTML445/1108: All edits an independently chosen kept band;
    /// Pan1 always edits its actual band. Choosing here never retunes a slice.
    @Published var displayBandChoice: String? = nil
    var displayEditingBand: String? {
        displayEditScope == .all ? displayBandChoice ?? band.settings.scaleBand : band.settings.scaleBand
    }
    var displayEditingSettings: BandDisplaySettings {
        guard displayEditScope == .all else { return band.settings }
        var settings = displaySettings.masterSettings(forPan: BandSubscriber.panId)
        settings.enterBand(displayEditingBand)
        return settings
    }
    func ownDisplayFields(among fields: Set<String>) -> Set<String> {
        displaySettings.ownFields(forPan: BandSubscriber.panId, among: fields)
    }
    func resetDisplayPicture(fields: Set<String>) {
        guard displayEditScope == .pan else { return }
        displaySettings.resetPicture(forPan: BandSubscriber.panId, fields: fields)
        reloadDisplaySettings()
    }
    func copyOwnDisplayPictureToMaster(fields: Set<String>) -> Bool {
        guard displayEditScope == .pan, !ownDisplayFields(among: fields).isEmpty else { return false }
        displaySettings.copyOwnPictureToMaster(forPan: BandSubscriber.panId, fields: fields)
        reloadDisplaySettings()
        return true
    }
    func changeDisplay(fields: Set<String>, _ change: (inout BandDisplaySettings) -> Void) {
        changeDisplay(selectedFields: fields, change)
    }
    func displayEffectiveValue(_ readout: SetupDescription.EffectiveReadout) -> Double? {
        guard displayEditScope == .pan else { return nil }
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
        // A weak local owner is filled only after initialization. The existing
        // sheet/Core/pan callbacks then share this same scope/store transaction,
        // without another subscriber, live pan registry or strong ownership cycle.
        weak var displayOwner: MainScreenModel?
        let change: (_ change: (inout BandDisplaySettings) -> Void) -> Void = { change in
            displayOwner?.changeDisplay(change)
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
        displayOwner = self
        display.focusPan = { [weak self] in self?.displayEditScope = .pan }
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
        changeDisplay(selectedFields: nil, change)
    }

    private func reloadDisplaySettings() {
        var saved = displaySettings.settings(forPan: BandSubscriber.panId)
        saved.sideways = band.settings.sideways // Current orientation is runtime-only.
        band.settings = saved
        objectWillChange.send()
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
            if selected.isDisjoint(with: ["scaleTopDbm", "scaleBottomDbm", "threeDFloorDb", "bandScales", "threeDFloors"]) {
                edited.bandScales = band.settings.bandScales
                edited.threeDFloors = band.settings.threeDFloors
            }
            edited.enterBand(band.settings.scaleBand)
            // Shared/device fields retain their existing authority. Master
            // picture writes explicitly clear the selected closed-pan own fields,
            // even when a selected value already equals the master.
            displaySettings.setSettings(edited, forPan: BandSubscriber.panId, includingPicture: false)
            displaySettings.setMasterSettings(edited, fields: selected)
        } else if edited != before {
            displaySettings.setSettings(edited, forPan: BandSubscriber.panId)
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
