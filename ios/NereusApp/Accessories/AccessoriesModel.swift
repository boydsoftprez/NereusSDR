// NereusSDR for iOS: the Core's amplifiers and tuner as the accessory pages show them, and the verbs that switch them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels
import os

/// The Core's accessories (R-IOS-19, spec section 5.4 items 1 to 4): the
/// Power Genius XL (`amplifier`), the Tuner Genius XL (`tuner`) and the
/// RF-Kit RF2K-S (`rfkit`), with the Core's records of them
/// (`accessoryData`) and their own settings (`accessorySettings`), as the
/// remote accessory control document describes them. The Core owns every
/// connection to them; the phone reads its mirror and asks it to act with
/// the documented verbs.
///
/// Every button follows the device's report on its object, never the tap.
/// A switch that cannot run now (a Core too old for its verb, the Core not
/// connected to the device, the radio on the air) is disabled with its
/// reason; a verb the Core refuses shows the Core's reason on that
/// device's page. Nothing here keys the radio.
@MainActor
final class AccessoriesModel: ObservableObject {
    /// One of the three accessories.
    enum Device: String, CaseIterable, Identifiable, Sendable {
        case powerGenius = "pgxl"
        case tunerGenius = "tgxl"
        case rfKit = "rfkit"

        var id: String { rawValue }

        /// The page's title and the Radio tab's row.
        var title: String {
            switch self {
            case .powerGenius:
                return "Power Genius XL"
            case .tunerGenius:
                return "Tuner Genius XL"
            case .rfKit:
                return "RF-Kit RF2K-S"
            }
        }
    }

    /// The connection phase all three share (document, Connection phase).
    enum Phase: Int64, Sendable {
        case disabled = 0
        case disconnected
        case discovering
        case connecting
        case identifying
        case retrying
        case connected
        case error
    }

    /// The connection and identity every accessory object carries.
    struct Link: Equatable {
        var phase: Phase = .disabled
        var error = ""
        var host = ""
        var port: Int64 = 0
        var model = ""
        var serial = ""
        var version = ""
        var nickname = ""

        var connected: Bool { phase == .connected }
    }

    /// The Power Genius's `state` (document, The `amplifier` object).
    enum AmpState: Int64, Sendable {
        case unknown = 0
        case powerUp
        case standby
        case idle
        case operate
        case transmitA
        case transmitB
        case fault
    }

    /// Band follow, one table for both amplifiers (document, Band follow).
    enum BandFollow: Int64, Sendable {
        case off = 0
        case waiting
        case following
        case thisComputerOnly
    }

    struct PowerGenius: Equatable {
        var link = Link()
        var present = false
        var state = AmpState.unknown
        var deviceState = ""
        var operate = false
        var transmitting = false
        var forwardW = 0.0
        var swr = 1.0
        var temperatureC = 0.0
        var mainsV = 0.0
        var drainA = 0.0
        var efficiency = ""
        var bandFollow = BandFollow.off
    }

    struct TunerGenius: Equatable {
        var link = Link()
        var present = false
        var operate = false
        var bypass = false
        var tuning = false
        var antenna: Int64 = 0
        var hasAntennaSwitch = false
        var relays: [Int64] = [0, 0, 0]
        var forwardW = 0.0
        var swr = 1.0
        var address = ""
    }

    /// Optional raw readings for the TX panel; existing accessory pages retain their contracts.
    struct PowerGeniusReadings: Equatable {
        var forwardW: Double?
        var swr: Double?
        var temperatureC: Double?
    }

    struct TunerGeniusReadings: Equatable {
        var forwardW: Double?
        var swr: Double?
        var relays: [Int64?] = [nil, nil, nil]
    }

    @Published private(set) var powerGeniusReadings = PowerGeniusReadings()
    @Published private(set) var tunerGeniusReadings = TunerGeniusReadings()
    @Published private(set) var rfKitReadings = PowerGeniusReadings()

    @Published private(set) var readingsCurrent = false

    /// The RF2K-S's tuner mode (document, `tunerMode`).
    enum TunerMode: Int64, Sendable {
        case unknown = 0
        case bypass
        case manual
        case autoTuning
        case auto
    }

    struct RfKit: Equatable {
        var link = Link()
        var present = false
        var operate = false
        var forwardW = 0.0
        var reflectedW = 0.0
        var swr = 1.0
        var temperatureC = 0.0
        var volts = 0.0
        var amps = 0.0
        var interface = ""
        var antennaPresentMask: Int64 = 0
        var antennaDisabledMask: Int64 = 0
        var activeAntenna: Int64 = 0
        var activeAntennaExternal = false
        var tunerMode = TunerMode.unknown
        var tunerSetup = ""
        var bandFollow = BandFollow.off
        var bandFollowAddress = ""
        var bandFollowPort: Int64 = 0
    }

    /// The transmit interlock the Core enforces (`accessoryData`).
    struct Interlock: Equatable {
        /// 0 off, 1 warn, 2 block.
        var mode: Int64 = 0
        var graceMs: Int64 = 0
        var swrGateEnabled = false
        var swrGateMax = 3.0
    }

    /// One fault the Core recorded (document, The fault record).
    struct Fault: Equatable, Identifiable {
        let id: Int
        let whenMs: Int64
        let text: String
        let detail: String
    }

    /// One stored tune in the Tuner Genius's tune memory.
    struct StoredTune: Equatable, Identifiable {
        let antenna: Int64
        let band: String
        let relays: [Int64]

        var id: String { "\(band)-\(antenna)" }
    }

    /// The Core's records: the interlock, the output limit, the fault
    /// histories, the tune memory and the antenna names.
    struct Records: Equatable {
        var interlock = Interlock()
        var powerCapEnabled = false
        var powerCapW: Int64 = 0
        /// The amp's output went over the limit: the Core's words for it,
        /// and how many times it has (parity row M9).
        var powerCapExceeded = false
        var powerCapAlertText = ""
        var powerCapAlertCount: Int64 = 0
        var faults: [Device: [Fault]] = [:]
        var tuneMemory: [StoredTune] = []
        var autoRecall = false
        var tunerLabels = ["", "", ""]
        var rfKitLabels = ["", "", "", ""]
        /// The connection counters, by property name.
        var counters: [String: Int64] = [:]
    }

    /// The accessory verbs this Core takes, by their capability versions.
    struct Versions: Equatable {
        var powerGenius: Int64 = 0
        var tunerGenius: Int64 = 0
        var rfKit: Int64 = 0
        var records: Int64 = 0
        /// `configureTgxl` and `disconnectTgxl`.
        var tunerConnection: Int64 = 0
        /// `setFourO3AEnabled`.
        var fourO3A: Int64 = 0
    }

    @Published private(set) var powerGenius: PowerGenius?
    @Published private(set) var tunerGenius: TunerGenius?
    @Published private(set) var rfKit: RfKit?
    /// nil where the Core does not share its records with this app.
    @Published private(set) var records: Records?
    /// The Power Genius's and Tuner Genius's own settings, by property name.
    @Published private(set) var deviceSettings: [String: MirrorValue] = [:]
    @Published private(set) var versions = Versions()
    /// The radio is on the air: keyed from anywhere, TUNE or the two-tone test.
    @Published private(set) var onAir = false
    /// The band the radio transmits on, as the Core's catalogue names it.
    @Published private(set) var bandLabel: String?
    /// The latest command's refusal or unconfirmed outcome, by device.
    @Published internal(set) var notes: [Device: String] = [:]
    /// The station's 4O3A switch (Power Genius and Tuner Genius) and RF-Kit switch.
    @Published private(set) var fourO3AEnabled = false
    @Published private(set) var rfKitEnabled = false
    /// What the Core's last LAN scan heard, by device; nil before any.
    @Published internal(set) var scans: [Device: [ScannedDevice]] = [:]
    /// A scan is listening, by device.
    @Published internal(set) var scanning: Set<Device> = []
    /// The station settings the pages show (antenna names, the RF-Kit's
    /// connection, the Power Genius's connection, the tune memory recall).
    @Published private(set) var stationSettings: [String: String] = [:]

    // The words for a switch that cannot run now, and the pages' notes.
    // A local window uses the same on-air sentence (document, Window behaviour).
    static let onAirReason = "The radio is on the air. Try again when it stops."
    static let ampNotConnectedReason = "The Core is not connected to the Power Genius."
    static let tunerNotConnectedReason = "The Core is not connected to the Tuner Genius."
    static let rfKitNotConnectedReason = "The Core is not connected to the RF-Kit amplifier."
    static let rfKitAntennaUnavailable = "This antenna is not available on the RF-Kit amplifier."
    static let rfKitAntennasUnknown = "The RF-Kit amplifier has not listed its antennas."
    static let tunerNoAntennaSwitch = "This Tuner Genius has one antenna, so there is nothing to switch."
    /// Spec section 5.4 item 4: the RF2K-S's tuner buttons stay greyed.
    static let rfKitTunerNote = "Use the amp's front panel for these. Its firmware doesn't accept tuner commands."
    static let noRecordsText =
        "This Core does not share its transmit interlock or fault history with this app. Updating the Core may help."

    static let amplifierKey = "amplifier"
    static let tunerKey = "tuner"
    static let rfKitKey = "rfkit"
    static let recordsKey = "accessoryData"
    static let settingsKey = "accessorySettings"
    static let radioKey = "radio"
    static let txStateKey = "txState"

    private let mirror: MirrorStore
    let commands: CommandClient?
    /// The Core's settings, for the station settings the pages change; nil in screen tests without one.
    let settings: SettingsProxyClient?
    /// Commands and settings share each device page's note owner.
    let settingOutcomeOwner = ControlOutcomeOwner()
    private let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watchedObjects: [String: ObjectIdentifier] = [:]
    private var refreshQueued = false
    static let logger = Logger(subsystem: "NereusSDR", category: "accessories")

    init(mirror: MirrorStore, commands: CommandClient?, slices: BandSlicesModel, catalogFeed: CatalogFeed,
         settings: SettingsProxyClient? = nil) {
        self.mirror = mirror
        self.commands = commands
        self.settings = settings
        self.slices = slices
        self.catalogFeed = catalogFeed
        settings?.$values.sink { [weak self] values in
            self?.stationSettings = values.filter { Self.isAccessorySetting($0.key) }
        }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isStale.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
    }

    // MARK: What the Radio tab lists

    /// Where an accessory stands, for its row on the Radio tab and its page.
    enum Standing: Equatable {
        /// The Core does not send this device's object to this app.
        case notReported
        /// The station has it switched off (the 4O3A or RF-Kit switch).
        case switchedOff
        /// Switched on, but the Core has no address for it and has never seen it.
        case notSetUp
        /// Set up: the Core has an address for it, or is reaching it.
        case setUp
    }

    /// The Radio tab always lists all three, as the desktop's Peripherals
    /// page always shows them: one the station has switched off, or never
    /// set up, says so and opens its page, where it can be switched on or
    /// set up (disabled, never hidden).
    var listed: [Device] { Device.allCases }

    func standing(_ device: Device) -> Standing {
        let reported: (link: Link, present: Bool)?
        switch device {
        case .powerGenius:
            reported = powerGenius.map { ($0.link, $0.present) }
        case .tunerGenius:
            reported = tunerGenius.map { ($0.link, $0.present) }
        case .rfKit:
            reported = rfKit.map { ($0.link, $0.present) }
        }
        guard let reported else {
            return .notReported
        }
        if reported.link.phase == .disabled {
            return .switchedOff
        }
        return Self.isSetUp(reported.link, present: reported.present) ? .setUp : .notSetUp
    }

    static func isSetUp(_ link: Link, present: Bool) -> Bool {
        guard link.phase != .disabled else {
            return false
        }
        return present || !link.host.isEmpty || link.phase.rawValue >= Phase.discovering.rawValue
    }

    /// The station's switch for `device`: the 4O3A switch for the Power
    /// Genius and the Tuner Genius, the RF-Kit switch for the RF2K-S.
    func stationSwitch(_ device: Device) -> Bool {
        device == .rfKit ? rfKitEnabled : fourO3AEnabled
    }

    /// Why the station's switch for `device` can't be changed from here, or nil.
    func stationSwitchReason(_ device: Device) -> String? {
        if device == .rfKit {
            return connectionReason(.rfKit)
        }
        return versions.fourO3A >= 1 ? nil : Self.olderCoreSettingText
    }

    /// Switches `device` on or off at the Core.
    func setStationSwitch(_ device: Device, _ on: Bool) {
        if device == .rfKit {
            setRfKitEnabled(on)
        } else {
            setFourO3AEnabled(on, from: device)
        }
    }

    /// The Core's words while the Power Genius's output is over its limit,
    /// as the desktop's alert says them; nil while it is not.
    var powerCapAlert: String? {
        guard let records, records.powerCapExceeded, !records.powerCapAlertText.isEmpty else {
            return nil
        }
        return records.powerCapAlertText
    }

    // MARK: Why a switch cannot run

    /// Why `device`'s OPERATE (and its antennas) cannot switch now, or nil.
    func switchReason(_ device: Device) -> String? {
        switch device {
        case .powerGenius:
            if versions.powerGenius < 4 {
                return TransmitModel.ampOlderCoreText
            }
            if powerGenius?.link.connected != true {
                return Self.ampNotConnectedReason
            }
        case .tunerGenius:
            if versions.tunerGenius < 2 {
                return TransmitModel.tunerOlderCoreText
            }
            if tunerGenius?.link.connected != true {
                return Self.tunerNotConnectedReason
            }
        case .rfKit:
            if versions.rfKit < 4 {
                return TransmitModel.ampOlderCoreText
            }
            if rfKit?.link.connected != true {
                return Self.rfKitNotConnectedReason
            }
        }
        return onAir ? Self.onAirReason : nil
    }

    /// Why the Tuner Genius's antenna buttons cannot switch, or nil.
    var tunerAntennaReason: String? {
        if let reason = switchReason(.tunerGenius) {
            return reason
        }
        return tunerGenius?.hasAntennaSwitch == true ? nil : Self.tunerNoAntennaSwitch
    }

    /// Why the RF2K-S's antenna `port` cannot be chosen, or nil.
    func rfKitAntennaReason(_ port: Int64) -> String? {
        if let reason = switchReason(.rfKit) {
            return reason
        }
        guard let rfKit, rfKit.antennaPresentMask != 0 else {
            return Self.rfKitAntennasUnknown
        }
        let bit: Int64 = 1 << (port - 1)
        guard rfKit.antennaPresentMask & bit != 0, rfKit.antennaDisabledMask & bit == 0 else {
            return Self.rfKitAntennaUnavailable
        }
        return nil
    }

    // MARK: The operator

    /// The Power Genius's OPERATE or STANDBY (`setPgxlOperate`).
    func setAmpOperate(_ on: Bool) {
        guard allowed(.powerGenius, switchReason(.powerGenius)) else {
            return
        }
        send(.powerGenius, "setPgxlOperate", [CommandArgument(name: "on", value: .bool(on))])
    }

    /// The Tuner Genius's OPERATE or STANDBY (`setTgxlOperate`). From
    /// bypass, a Core below `remoteTgxlControlVersion` 3 takes bypass off
    /// first, as the desktop does (document, Negotiation).
    func setTunerOperate(_ on: Bool) {
        guard allowed(.tunerGenius, switchReason(.tunerGenius)) else {
            return
        }
        let operate = [CommandArgument(name: "on", value: .bool(on))]
        guard on, tunerGenius?.bypass == true, versions.tunerGenius < 3 else {
            send(.tunerGenius, "setTgxlOperate", operate)
            return
        }
        Task { [weak self] in
            guard let self, await self.invoke(.tunerGenius, "setTgxlBypass",
                                              [CommandArgument(name: "on", value: .bool(false))]) else {
                return
            }
            _ = await self.invoke(.tunerGenius, "setTgxlOperate", operate)
        }
    }

    /// One of the Tuner Genius's three antennas (`setTgxlAntenna`).
    func setTunerAntenna(_ port: Int64) {
        guard allowed(.tunerGenius, tunerAntennaReason) else {
            return
        }
        send(.tunerGenius, "setTgxlAntenna", [CommandArgument(name: "port", value: .int(port))])
    }

    /// The RF2K-S's OPERATE or STANDBY (`setRfKitOperate`).
    func setRfKitOperate(_ on: Bool) {
        guard allowed(.rfKit, switchReason(.rfKit)) else {
            return
        }
        send(.rfKit, "setRfKitOperate", [CommandArgument(name: "on", value: .bool(on))])
    }

    /// One of the RF2K-S's internal antennas (`setRfKitAntenna`).
    func setRfKitAntenna(_ port: Int64) {
        guard allowed(.rfKit, rfKitAntennaReason(port)) else {
            return
        }
        send(.rfKit, "setRfKitAntenna", [CommandArgument(name: "port", value: .int(port))])
    }

    /// Empties `device`'s fault history on the Core (`clearAccessoryFaults`).
    func clearFaults(_ device: Device) {
        guard allowed(device, versions.records >= 1 ? nil : Self.noRecordsText) else {
            return
        }
        send(device, "clearAccessoryFaults", [CommandArgument(name: "device", value: .text(device.rawValue))])
    }

    /// Clears the Core's refusal shown on `device`'s page.
    func dismissNote(_ device: Device) {
        _ = settingOutcomeOwner.begin(device.rawValue)
        notes[device] = nil
    }

    func allowed(_ device: Device, _ reason: String?) -> Bool {
        guard let reason else {
            return true
        }
        _ = settingOutcomeOwner.begin(device.rawValue)
        notes[device] = reason
        return false
    }

    func send(_ device: Device, _ verb: String, _ arguments: [CommandArgument]) {
        Task { [weak self] in
            _ = await self?.invoke(device, verb, arguments)
        }
    }

    /// Sends `verb` and notes its current outcome; true when the Core accepted it.
    func invoke(_ device: Device, _ verb: String, _ arguments: [CommandArgument]) async -> Bool {
        let edit = settingOutcomeOwner.begin(device.rawValue)
        let snapshot = mirror.snapshotIdentity
        let outcome: PropertyWriteOutcome
        if let commands {
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: .seconds(5))
                outcome = PropertyWriteOutcome(.success(result))
            } catch let error as CommandError {
                outcome = PropertyWriteOutcome(.failure(error))
            } catch {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
                return false
            }
        } else {
            outcome = .notSent
        }
        guard settingOutcomeOwner.isCurrent(edit, device.rawValue), mirror.snapshotIdentity == snapshot,
              !outcome.answeredByCore || (mirror.isSnapshotComplete && !mirror.isStale) else {
            return false
        }
        if outcome.heldForQuestion {
            // The Core holds the change for this phone's question
            // sheet; it is not a refusal, as the desktop does not show
            // it as one (StationClient.cpp:7308-7317).
            return false
        }
        notes[device] = outcome.noteText(refused: BandSlicesModel.refusedText)
        return outcome.accepted
    }

    // MARK: Reading

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
        let amplifier = watch(Self.amplifierKey)
        let tuner = watch(Self.tunerKey)
        let rfkit = watch(Self.rfKitKey)
        let data = watch(Self.recordsKey)
        let settings = watch(Self.settingsKey)
        let radio = watch(Self.radioKey)
        let state = watch(Self.txStateKey)

        set(\.versions, Versions(powerGenius: mirror.capabilityVersion("remotePgxlControlVersion"),
                                 tunerGenius: mirror.capabilityVersion("remoteTgxlControlVersion"),
                                 rfKit: mirror.capabilityVersion("remoteRfKitControlVersion"),
                                 records: mirror.capabilityVersion("accessoryDataVersion"),
                                 tunerConnection: mirror.capabilityVersion("remoteTgxlConfigVersion"),
                                 fourO3A: mirror.capabilityVersion("remoteFourO3AControlVersion")))
        set(\.powerGenius, amplifier.map(Self.powerGenius))
        set(\.tunerGenius, tuner.map(Self.tunerGenius))
        set(\.rfKit, rfkit.map(Self.rfKit))
        // MirrorStore keeps historical objects across a lost link/new snapshot.
        // Only a completed, connected snapshot and an actually linked device can supply current readings.
        let current = mirror.isSnapshotComplete && !mirror.isStale
        set(\.readingsCurrent, current)
        set(\.powerGeniusReadings, current ? Self.powerGeniusReadings(amplifier) : PowerGeniusReadings())
        set(\.tunerGeniusReadings, current ? Self.tunerGeniusReadings(tuner) : TunerGeniusReadings())
        set(\.rfKitReadings, current ? Self.rfKitReadings(rfkit) : PowerGeniusReadings())
        set(\.records, data.map(Self.records))
        set(\.deviceSettings, settings?.values ?? [:])
        let keyed = radio?["transmitting"].flag == true || state?["keyed"].flag == true
            || state?["tuning"].flag == true || state?["twoTone"].flag == true
        set(\.onAir, keyed)
        set(\.fourO3AEnabled, radio?["fourO3AEnabled"].flag ?? false)
        set(\.rfKitEnabled, radio?["rfKitEnabled"].flag ?? false)

        // The band the radio transmits on: the transmit slice's, else the active one's.
        var label: String?
        if let entry = slices.entries.first(where: { $0.slice.txSlice }) ?? slices.active,
           let band = mirror.object("slice:\(entry.id)")?["band"].whole,
           let found = catalogFeed.catalog?.bands?.first(where: { Int64($0.id) == band }) {
            // The catalogue names an amateur band by its metres alone ("40"); the amp's line says "40m".
            label = found.label.allSatisfy(\.isNumber) ? found.label + "m" : found.label
        }
        set(\.bandLabel, label)
    }

    private static func reading(_ value: MirrorValue?, minimum: Double? = 0) -> Double? {
        let number: Double
        switch value {
        case .int(let value)?: number = Double(value)
        case .double(let value)?: number = value
        default: return nil
        }
        guard number.isFinite else { return nil }
        if let minimum, number < minimum { return nil }
        return number
    }

    private static func powerGeniusReadings(_ object: MirrorObject?) -> PowerGeniusReadings {
        guard let object, object["present"].flag == true, link(object).connected else { return .init() }
        return PowerGeniusReadings(forwardW: reading(object["forwardPowerW"]),
                                   swr: reading(object["swr"], minimum: 1),
                                   temperatureC: reading(object["temperatureC"], minimum: nil))
    }

    private static func rfKitReadings(_ object: MirrorObject?) -> PowerGeniusReadings {
        guard let object, object["present"].flag == true, link(object).connected else { return .init() }
        return PowerGeniusReadings(forwardW: reading(object["forwardPowerW"]),
                                  swr: reading(object["swr"], minimum: 1),
                                  temperatureC: reading(object["temperatureC"], minimum: nil))
    }

    private static func tunerGeniusReadings(_ object: MirrorObject?) -> TunerGeniusReadings {
        guard let object, object["isPresent"].flag == true, link(object).connected else { return .init() }
        return TunerGeniusReadings(forwardW: reading(object["fwdPower"]), swr: reading(object["swr"], minimum: 1),
                                   relays: ["relayC1", "relayL", "relayC2"].map { relayReading(object[$0]) })
    }

    private static func relayReading(_ value: MirrorValue?) -> Int64? {
        // Relay bytes are 0...255: src/core/TuneMemoryStore.h:35-37 and src/gui/RelayBar.h:36-44.
        // Accept the wire integer only; do not truncate doubles through the existing page parser.
        guard case .int(let value)? = value, (0...255).contains(value) else { return nil }
        return value
    }

    static func link(_ object: MirrorObject) -> Link {
        Link(phase: Phase(rawValue: object["connectionPhase"].whole ?? 0) ?? .disabled,
             error: object["connectionError"].text ?? "", host: object["configuredHost"].text ?? "",
             port: object["configuredPort"].whole ?? 0, model: object["deviceModel"].text ?? "",
             serial: object["deviceSerial"].text ?? "", version: object["deviceVersion"].text ?? "",
             nickname: object["deviceNickname"].text ?? "")
    }

    static func powerGenius(_ object: MirrorObject) -> PowerGenius {
        PowerGenius(link: link(object), present: object["present"].flag ?? false,
                    state: AmpState(rawValue: object["state"].whole ?? 0) ?? .unknown,
                    deviceState: object["deviceState"].text ?? "", operate: object["operate"].flag ?? false,
                    transmitting: object["transmitting"].flag ?? false,
                    forwardW: object["forwardPowerW"].number ?? 0, swr: object["swr"].number ?? 1,
                    temperatureC: object["temperatureC"].number ?? 0, mainsV: object["mainsVoltageV"].number ?? 0,
                    drainA: object["drainCurrentA"].number ?? 0, efficiency: object["efficiencyText"].text ?? "",
                    bandFollow: BandFollow(rawValue: object["bandFollow"].whole ?? 0) ?? .off)
    }

    static func tunerGenius(_ object: MirrorObject) -> TunerGenius {
        TunerGenius(link: link(object), present: object["isPresent"].flag ?? false,
                    operate: object["isOperate"].flag ?? false, bypass: object["isBypass"].flag ?? false,
                    tuning: object["isTuning"].flag ?? false,
                    // The Core reports the tuner's antenna 0-based (ANT 1 is 0), as the
                    // tuner does; the page and setTgxlAntenna count from 1. Unreported is 0.
                    antenna: object["antennaA"].whole.map { $0 + 1 } ?? 0,
                    hasAntennaSwitch: object["hasAntennaSwitch"].flag ?? false,
                    relays: ["relayC1", "relayL", "relayC2"].map { object[$0].whole ?? 0 },
                    forwardW: object["fwdPower"].number ?? 0, swr: object["swr"].number ?? 1,
                    address: object["tgxlIp"].text ?? "")
    }

    static func rfKit(_ object: MirrorObject) -> RfKit {
        RfKit(link: link(object), present: object["present"].flag ?? false, operate: object["operate"].flag ?? false,
              forwardW: object["forwardPowerW"].number ?? 0, reflectedW: object["reflectedPowerW"].number ?? 0,
              swr: object["swr"].number ?? 1, temperatureC: object["temperatureC"].number ?? 0,
              volts: object["voltageV"].number ?? 0, amps: object["currentA"].number ?? 0,
              interface: object["operationalInterface"].text ?? "",
              antennaPresentMask: object["antennaPresentMask"].whole ?? 0,
              antennaDisabledMask: object["antennaDisabledMask"].whole ?? 0,
              activeAntenna: object["activeAntennaNumber"].whole ?? 0,
              activeAntennaExternal: object["activeAntennaExternal"].flag ?? false,
              tunerMode: TunerMode(rawValue: object["tunerMode"].whole ?? 0) ?? .unknown,
              tunerSetup: object["tunerSetup"].text ?? "",
              bandFollow: BandFollow(rawValue: object["bandFollow"].whole ?? 0) ?? .off,
              bandFollowAddress: object["bandFollowAddress"].text ?? "",
              bandFollowPort: object["bandFollowPort"].whole ?? 0)
    }

    static func records(_ object: MirrorObject) -> Records {
        var records = Records()
        records.interlock = Interlock(mode: object["interlockMode"].whole ?? 0,
                                      graceMs: object["interlockGraceMs"].whole ?? 0,
                                      swrGateEnabled: object["interlockSwrGateEnabled"].flag ?? false,
                                      swrGateMax: object["interlockSwrGateMax"].number ?? 3)
        records.powerCapEnabled = object["powerCapEnabled"].flag ?? false
        records.powerCapW = object["powerCapW"].whole ?? 0
        records.powerCapExceeded = object["powerCapExceeded"].flag ?? false
        records.powerCapAlertText = object["powerCapAlertText"].text ?? ""
        records.powerCapAlertCount = object["powerCapAlertCount"].whole ?? 0
        for device in Device.allCases {
            records.faults[device] = faults(object["\(device.rawValue)Faults"].text ?? "")
        }
        records.tuneMemory = tuneMemory(object["tuneMemory"].text ?? "")
        records.autoRecall = object["autoTuneMemoryRecall"].flag ?? false
        records.tunerLabels = (1...3).map { object["tgxlAntenna\($0)Label"].text ?? "" }
        records.rfKitLabels = (1...4).map { object["rfkitAntenna\($0)Label"].text ?? "" }
        for (name, value) in object.values where name.hasPrefix("pgxl") || name.hasPrefix("tgxl")
            || name.hasPrefix("rfkit") {
            if let number = Optional(value).whole, !name.hasSuffix("Faults") {
                records.counters[name] = number
            }
        }
        return records
    }

    /// A fault history as the Core sends it: a JSON array, newest first.
    static func faults(_ json: String) -> [Fault] {
        guard let array = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [[String: Any]] else {
            return []
        }
        return array.enumerated().map { index, record in
            Fault(id: index, whenMs: (record["whenMs"] as? NSNumber)?.int64Value ?? 0,
                  text: record["text"] as? String ?? "", detail: record["detail"] as? String ?? "")
        }
    }

    /// The tune memory as the Core sends it, in its order (by band, then antenna).
    static func tuneMemory(_ json: String) -> [StoredTune] {
        guard let array = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [[String: Any]] else {
            return []
        }
        return array.compactMap { entry in
            guard let antenna = (entry["antenna"] as? NSNumber)?.int64Value, let band = entry["band"] as? String else {
                return nil
            }
            let relays = ["c1", "l", "c2"].map { (entry[$0] as? NSNumber)?.int64Value ?? 0 }
            return StoredTune(antenna: antenna, band: band, relays: relays)
        }
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<AccessoriesModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
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
