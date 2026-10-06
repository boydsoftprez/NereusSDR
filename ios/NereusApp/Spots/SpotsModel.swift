// NereusSDR for iOS: the Core's spots and spot sources as the band and Spot Hub show them, and the verbs that run them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
import os

/// The Core's spots and its spot sources (R-IOS-25, D13, D32, spec section
/// 5.1 items 10 and 11, section 5.7 items 1 to 4). The Core runs the
/// sources (the DX cluster, the Reverse Beacon Network, POTA and PSK
/// Reporter) and keeps the spots; they reach the phone as the `spots`
/// record stream, each source's console as `spotConsole:<source>`, and each
/// source's state on the `spotSources` object (link document sections 7.1
/// and 7.7). How spots look on the band is this phone's own
/// (``SpotDisplaySettings``); the spot lifetime, the sources' connection
/// settings and your identity are the Core's settings.
///
/// A tap on a spot tunes the active slice to it. Every button that cannot
/// run is disabled with its reason; a verb the Core refuses shows the
/// Core's words on the source's page.
@MainActor
final class SpotsModel: ObservableObject {
    /// One spot the Core holds.
    struct Spot: Identifiable, Equatable {
        let id: String
        let time: Date?
        let frequencyHz: Double
        let call: String
        let mode: String
        /// The source's label as the Core names it: `Cluster`, `RBN`, `POTA`, `PSK`, `FreeDV`.
        let source: String
        let spotter: String
        let comment: String
        /// The band as the catalogue numbers it; nil when not sent.
        let band: Int?
        /// `#RRGGBB`, empty when the Core does not colour it.
        let dxccColour: String
        /// 4 a new country, 3 a new band, 2 a new mode, 1 worked before, 0 not known.
        let dxccPriority: Int
        /// The slice mode the Core worked out for the spot (the slice's
        /// `dspMode` value); nil when the Core names none.
        var resolvedMode: Int? = nil
    }

    /// Each spot source Spot Hub lists, in the desktop's order.
    enum Source: String, CaseIterable, Identifiable {
        case dxCluster
        case rbn
        case wsjtx
        case spotCollector
        case pota
        case freeDv
        case pskReporter

        var id: String { rawValue }

        /// The source's name on Spot Hub.
        var title: String {
            switch self {
            case .dxCluster: return "Cluster"
            case .rbn: return "RBN"
            case .wsjtx: return "WSJT-X"
            case .spotCollector: return "SpotCollector"
            case .pota: return "POTA"
            case .freeDv: return "FreeDV"
            case .pskReporter: return "PSK Reporter"
            }
        }

        /// The source's pill in the Spot List and on Display (the desktop's).
        var pill: String {
            switch self {
            case .dxCluster: return "DX"
            case .rbn: return "RBN"
            case .wsjtx: return "JT"
            case .spotCollector: return "COL"
            case .pota: return "POT"
            case .freeDv: return "FDR"
            case .pskReporter: return "PSK"
            }
        }

        /// The source's label on a spot, as the Core sends it.
        var spotLabel: String {
            switch self {
            case .dxCluster: return "Cluster"
            case .rbn: return "RBN"
            case .wsjtx: return "WSJT-X"
            case .spotCollector: return "SpotCollector"
            case .pota: return "POTA"
            case .freeDv: return "FreeDV"
            case .pskReporter: return "PSK"
            }
        }

        /// The Core runs this source: it can be connected from the phone.
        var runsAtCore: Bool {
            switch self {
            case .dxCluster, .rbn, .pota, .pskReporter: return true
            case .wsjtx, .spotCollector, .freeDv: return false
            }
        }

        /// The source takes typed commands at its console.
        var takesCommands: Bool {
            self == .dxCluster || self == .rbn
        }

        static func labelled(_ label: String) -> Source? {
            allCases.first { $0.spotLabel == label }
        }
    }

    /// A source's state as the Core reports it.
    enum SourceState: String {
        case off
        case connecting
        case connected
        case error
    }

    struct SourceStatus: Equatable {
        var state: SourceState = .off
        /// The Core's words: the error, or what the source is doing; may be empty.
        var text = ""
        /// FreeDV Reporter only: "Hide my station" is on at the Core.
        var hidden = false
    }

    // MARK: Words

    static let olderCoreReason = "This Core does not send its spots to this app. Updating the Core may help."
    static let listensOnEachComputerReason = "WSJT-X and SpotCollector listen on each computer, not on the Core."
    static let freeDvReason = "This Core does not run FreeDV Reporter for this app. Updating the Core may help."
    /// The Core names no mode on its spots, so Auto mode cannot set one.
    static let noSpotModeReason = "This Core does not say which mode each spot uses. Updating the Core may help."
    static let freeDvHiddenWords = "hidden from the dashboard"
    static let noCommandsReason = "Only the DX cluster and the Reverse Beacon Network take typed commands."
    static let notConnectedReason = "The Core is not connected."
    static let lockedReason = "The active slice is locked. Unlock it to tune to a spot."
    static let noSliceReason = "There is no slice to tune."

    // MARK: The Core's settings

    static let lifetimeKey = "DxClusterSpotLifetimeSec"
    static let lifetimeDefaultSeconds = 1800
    static let callsignKey = "User/Callsign"
    static let gridKey = "User/GridSquare"

    /// A source's connection settings, as the Core keeps them.
    struct Connection {
        var hostKey: String?
        var hostDefault = ""
        var portKey: String?
        var portDefault = 0
        var callsignKey: String?
        var gridKey: String?
        var pollKey: String?
        var pollDefault = 0
        var autoKey: String
        /// "Auto-connect" for a server, "Auto-start" for a service.
        var autoTitle: String
    }

    static func connection(_ source: Source) -> Connection? {
        switch source {
        case .dxCluster:
            return Connection(hostKey: "DxClusterHost", hostDefault: "dxc.nc7j.com", portKey: "DxClusterPort",
                              portDefault: 7300, callsignKey: "DxClusterCallsign", autoKey: "DxClusterAutoConnect",
                              autoTitle: "Auto-connect")
        case .rbn:
            return Connection(hostKey: "RbnHost", hostDefault: "telnet.reversebeacon.net", portKey: "RbnPort",
                              portDefault: 7000, callsignKey: "RbnCallsign", autoKey: "RbnAutoConnect",
                              autoTitle: "Auto-connect")
        case .pota:
            return Connection(pollKey: "PotaPollInterval", pollDefault: 30, autoKey: "PotaAutoStart",
                              autoTitle: "Auto-start")
        case .pskReporter:
            return Connection(callsignKey: "PskReporter/Callsign", gridKey: "PskReporter/GridSquare",
                              autoKey: "PskReporterAutoStart", autoTitle: "Auto-start")
        case .wsjtx, .spotCollector, .freeDv:
            return nil
        }
    }

    /// The source's own colour for a spot the Core does not colour, and
    /// the Core's setting that changes it (the desktop's defaults, D83).
    static func sourceColour(_ label: String) -> (key: String, fallback: String)? {
        switch label {
        case "Cluster": return ("DxClusterSpotColor", "#D2B48C")
        case "RBN": return ("RbnSpotColor", "#4488FF")
        case "POTA": return ("PotaSpotColor", "#FFFF00")
        case "PSK": return ("PskReporterSpotColor", "#FF00FF")
        case "FreeDV": return ("FreeDvSpotColor", "#FF8C00")
        default: return nil
        }
    }

    /// A spot's colour when nothing else gives one.
    static let plainColour = "#00B4D8"

    /// How many console lines a source page asks for, and the spots the band asks for.
    static let consoleBacklog = 200
    static let spotsBacklog = 500

    // MARK: State

    /// The Core's spots, newest first.
    @Published private(set) var spots: [Spot] = []
    @Published private(set) var sourceStatus: [Source: SourceStatus] = [:]
    /// FreeDV Reporter's state at the Core, or nil when the Core does not send it.
    @Published private(set) var freeDvStatus: SourceStatus?
    /// Each source's console lines, oldest first.
    @Published private(set) var consoles: [Source: [String]] = [:]
    /// The Core sends its spots to this app.
    @Published private(set) var available = false
    /// The Core's `recordStreamVersion` while connected; 0 otherwise.
    @Published private(set) var recordVersion: Int64 = 0
    /// The phone is connected to the Core.
    @Published private(set) var connected = false
    /// How spots look on this phone's band.
    @Published private(set) var display: SpotDisplaySettings
    /// The Core's words for the last verb it refused, by source; `nil`
    /// key-less notes (Clear all spots, the lifetime) go under Display.
    @Published var notes: [Source: String] = [:]
    @Published var displayNote: String?
    /// The Core's settings Spot Hub shows.
    @Published private(set) var stationSettings: [String: String] = [:]
    /// The Spot List's source pills (all on) and its band filter (this band).
    @Published var listSources: Set<Source> = Set(Source.allCases)
    @Published var listThisBandOnly = true
    /// On the band: the spot whose details are open, and the spots a badge
    /// hides, when their list is open.
    @Published var openDetails: Spot?
    @Published var openHidden: [Spot]?

    static let logger = Logger(subsystem: "NereusSDR", category: "spots")

    private let records: RecordStreamClient?
    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let settings: SettingsProxyClient?
    private let settingOutcomeOwner = ControlOutcomeOwner()
    private let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private let phone: PhoneSettings
    private var watches: Set<AnyCancellable> = []
    private var sourcesWatch: AnyCancellable?
    private var watchedSources: ObjectIdentifier?

    static let displayKey = "spots.display"
    static let sourcesKey = "spotSources"
    /// FreeDV Reporter's name on `spotSources` (link section 7.1).
    static let freeDvSourceName = "freedvReporter"

    init(records: RecordStreamClient?, mirror: MirrorStore, commands: CommandClient?, settings: SettingsProxyClient?,
         slices: BandSlicesModel, catalogFeed: CatalogFeed, phone: PhoneSettings) {
        self.records = records
        self.mirror = mirror
        self.commands = commands
        self.settings = settings
        self.slices = slices
        self.catalogFeed = catalogFeed
        self.phone = phone
        display = Self.readDisplay(phone)
        records?.want(RecordStreamClient.spotsStream, backlog: Self.spotsBacklog, by: self)
        records?.$streams.sink { [weak self] streams in
            self?.read(streams)
        }.store(in: &watches)
        records?.$available.sink { [weak self] available in
            self?.available = available
        }.store(in: &watches)
        records?.$version.sink { [weak self] version in
            self?.recordVersion = version
        }.store(in: &watches)
        settings?.$values.sink { [weak self] values in
            self?.stationSettings = values
        }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in
            Task { @MainActor [weak self] in self?.watchSources() }
        }.store(in: &watches)
        mirror.$isSnapshotComplete.combineLatest(mirror.$isStale).sink { [weak self] complete, stale in
            self?.connected = complete && !stale
        }.store(in: &watches)
        // The Spot List's band follows the active slice's.
        slices.$entries.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
    }

    // MARK: Reading

    private func read(_ streams: [String: RecordStreamClient.Stream]) {
        let records = streams[RecordStreamClient.spotsStream]?.records ?? []
        let next = records.reversed().map(Self.spot)
        if next != spots {
            spots = next
        }
        var lines: [Source: [String]] = [:]
        for source in Source.allCases where source.runsAtCore {
            if let stream = streams[RecordStreamClient.consoleStream(source.rawValue)] {
                lines[source] = stream.records.map { record in
                    if case .string(let line)? = record.fields["line"] {
                        return line
                    }
                    return ""
                }
            }
        }
        if lines != consoles {
            consoles = lines
        }
        if let openDetails, !next.contains(where: { $0.id == openDetails.id }) {
            // The Core dropped the spot (its lifetime ran out, or Clear all spots).
            self.openDetails = nil
        }
    }

    static func spot(_ record: LinkMessage.RecordBatch.Record) -> Spot {
        let fields = record.fields
        func text(_ name: String) -> String {
            if case .string(let value)? = fields[name] {
                return value
            }
            return ""
        }
        func number(_ name: String) -> Double? {
            if case .number(let value)? = fields[name], value.isFinite {
                return value
            }
            return nil
        }
        return Spot(id: record.id, time: time(text("timeUtc")),
                    frequencyHz: number("frequencyHz") ?? 0, call: text("call"), mode: text("mode"),
                    source: text("source"), spotter: text("spotter"), comment: text("comment"),
                    band: number("band").map { Int($0) }, dxccColour: text("dxccColour"),
                    dxccPriority: Int(number("dxccPriority") ?? 0),
                    resolvedMode: RecordStreamClient.resolvedMode(of: record))
    }

    private func watchSources() {
        let object = mirror.object(Self.sourcesKey)
        let id = object.map(ObjectIdentifier.init)
        if id != watchedSources {
            watchedSources = id
            sourcesWatch = object?.$values.sink { [weak self] values in
                self?.readSources(values)
            }
            if object == nil {
                sourceStatus = [:]
                freeDvStatus = nil
            }
        }
    }

    private func readSources(_ values: [String: MirrorValue]) {
        var status: [Source: SourceStatus] = [:]
        for source in Source.allCases where source.runsAtCore {
            var entry = SourceStatus()
            if case .text(let state)? = values["\(source.rawValue)State"] {
                entry.state = SourceState(rawValue: state) ?? .off
            }
            if case .text(let text)? = values["\(source.rawValue)Text"] {
                entry.text = text
            }
            status[source] = entry
        }
        if status != sourceStatus {
            sourceStatus = status
        }
        let freeDv = Self.freeDvStatus(values)
        if freeDv != freeDvStatus {
            freeDvStatus = freeDv
        }
    }

    /// FreeDV Reporter's state from `spotSources`, or nil from a Core that
    /// sends none (one before it ran FreeDV Reporter).
    static func freeDvStatus(_ values: [String: MirrorValue]) -> SourceStatus? {
        guard case .text(let state)? = values["\(freeDvSourceName)State"] else {
            return nil
        }
        var status = SourceStatus(state: SourceState(rawValue: state) ?? .off)
        if case .text(let text)? = values["\(freeDvSourceName)Text"] {
            status.text = text
        }
        if case .bool(let hidden)? = values["\(freeDvSourceName)Hidden"] {
            status.hidden = hidden
        }
        return status
    }

    /// FreeDV Reporter's line on Spot Hub, in the desktop's words:
    /// Connected (or the Core's words), Connecting, "Error: ...", the
    /// Core's reason while it is off, else Stopped; then whether your
    /// station is hidden from the dashboard.
    static func freeDvLine(_ status: SourceStatus) -> String {
        var words: String
        switch status.state {
        case .connected:
            words = status.text.isEmpty ? "Connected" : status.text
        case .connecting:
            words = "Connecting"
        case .error:
            words = status.text.isEmpty ? "Error" : "Error: \(status.text)"
        case .off:
            words = status.text.isEmpty ? "Stopped" : status.text
        }
        if status.hidden {
            words += " \u{00B7} " + freeDvHiddenWords
        }
        return words
    }

    // MARK: The band

    /// The spots this phone's band shows, as the layout takes them.
    var bandSpots: [SpotLayout.Spot] {
        spots.map { SpotLayout.Spot(id: $0.id, frequencyHz: $0.frequencyHz, call: $0.call, source: $0.source) }
    }

    func spot(id: String) -> Spot? {
        spots.first { $0.id == id }
    }

    /// A spot's text colour, `#RRGGBB`: the override while it is on, else
    /// the Core's DXCC colour, else its source's colour.
    func colour(_ spot: Spot) -> String {
        if display.overrideColours {
            return display.overrideColour
        }
        if !spot.dxccColour.isEmpty {
            return spot.dxccColour
        }
        if let source = Self.sourceColour(spot.source) {
            let set = stationSettings[source.key] ?? ""
            return set.isEmpty ? source.fallback : set
        }
        return Self.plainColour
    }

    /// What your log says of the spot, from the Core's DXCC priority.
    static func logWords(_ priority: Int) -> String? {
        switch priority {
        case 4: return "New country"
        case 3: return "New band"
        case 2: return "New mode"
        case 1: return "Worked before"
        default: return nil
        }
    }

    /// Why a spot can't be tuned to now, or nil: on a slice this phone
    /// only listens to, its owner line (who controls it); on a locked one,
    /// that it is locked.
    var tuneReason: String? {
        guard let active = slices.active else {
            return Self.noSliceReason
        }
        if active.listening {
            return active.ownerLine ?? Self.lockedReason
        }
        return active.locked ? Self.lockedReason : nil
    }

    /// Tunes the active slice to the spot, and closes what is open over the
    /// band. With Auto mode on, the slice then takes the mode the Core
    /// worked out for the spot, when the Core names one and it differs from
    /// the slice's, as a click on a spot does on the desktop. `onBand` is
    /// whether the tap came from the band (a flag, or a sheet over it).
    func tune(_ spot: Spot, onBand: Bool) {
        openDetails = nil
        openHidden = nil
        if let reason = tuneReason {
            // A spot tapped on the band says why, over the band, as a
            // greyed flag button does. The Spot List shows it as its own note.
            if onBand {
                slices.showReason(reason)
            }
            return
        }
        guard let active = slices.active else {
            return
        }
        slices.tap(to: spot.frequencyHz)
        guard display.autoMode, autoModeReason == nil, let mode = spot.resolvedMode, mode != active.mode else {
            return
        }
        slices.setMode(mode, sliceId: active.id)
    }

    func showDetails(_ spot: Spot) {
        openHidden = nil
        openDetails = spot
    }

    func showHidden(_ ids: [String]) {
        openDetails = nil
        let hidden = ids.compactMap(spot(id:)).sorted { $0.frequencyHz < $1.frequencyHz }
        openHidden = hidden.isEmpty ? nil : hidden
    }

    func closeBandPopups() {
        openDetails = nil
        openHidden = nil
    }

    // MARK: The Spot List

    /// The active slice's band, as the catalogue numbers it, and its name.
    var activeBand: (id: Int, label: String)? {
        guard let band = slices.active?.band else {
            return nil
        }
        guard let found = catalogFeed.catalog?.bands?.first(where: { $0.id == band }) else {
            return (band, "This band")
        }
        // The catalogue names an amateur band by its metres alone ("40").
        return (band, found.label.allSatisfy(\.isNumber) ? found.label + "m" : found.label)
    }

    /// The Spot List's rows: the Core's spots, newest first, from the
    /// sources whose pills are on, on this band unless All bands is chosen.
    var listed: [Spot] {
        let band = listThisBandOnly ? activeBand?.id : nil
        return spots.filter { spot in
            let source = Source.labelled(spot.source)
            guard source.map(listSources.contains) ?? true else {
                return false
            }
            if let band {
                return spot.band == band
            }
            return true
        }
    }

    func toggleListSource(_ source: Source) {
        if listSources.contains(source) {
            listSources.remove(source)
        } else {
            listSources.insert(source)
        }
    }

    /// Why a source's pill can't filter anything on this phone, or nil.
    static func pillReason(_ source: Source) -> String? {
        switch source {
        case .wsjtx, .spotCollector:
            return listensOnEachComputerReason
        default:
            return nil
        }
    }

    // MARK: Spot Hub's lines

    /// The Spot List's line on Spot Hub: "13 on 40m · 212 in all · newest 19:43".
    var listSummary: String {
        guard available else {
            return connected ? Self.olderCoreReason : "Not connected"
        }
        guard let newest = spots.first else {
            return "No spots"
        }
        var parts: [String] = []
        if let band = activeBand {
            parts.append("\(spots.filter { $0.band == band.id }.count) on \(band.label)")
        }
        parts.append("\(spots.count) in all")
        if let time = newest.time {
            parts.append("newest \(Self.shortTime(time))")
        }
        return parts.joined(separator: " \u{00B7} ")
    }

    /// The Display line on Spot Hub: "On · 3 levels · halfway · 16 pt".
    var displaySummary: String {
        guard display.enabled else {
            return "Off"
        }
        let levels = display.maxLevels == 1 ? "1 level" : "\(display.maxLevels) levels"
        let start = display.startPercent == 50 ? "halfway" : "\(display.startPercent)% down"
        return ["On", levels, start, "\(display.fontSize) pt"].joined(separator: " \u{00B7} ")
    }

    /// Why a source's connection can't be changed from here, or nil.
    func sourceReason(_ source: Source) -> String? {
        switch source {
        case .wsjtx, .spotCollector:
            return Self.listensOnEachComputerReason
        case .freeDv:
            return freeDvStatus == nil ? Self.freeDvReason : nil
        case .dxCluster, .rbn, .pota, .pskReporter:
            if !connected {
                return Self.notConnectedReason
            }
            return available ? nil : Self.olderCoreReason
        }
    }

    func status(_ source: Source) -> SourceStatus {
        if source == .freeDv {
            return freeDvStatus ?? SourceStatus()
        }
        return sourceStatus[source] ?? SourceStatus()
    }

    /// A source's line on Spot Hub.
    func sourceLine(_ source: Source) -> String {
        if source == .freeDv {
            return freeDvStatus.map(Self.freeDvLine) ?? Self.freeDvReason
        }
        if let reason = sourceReason(source), !source.runsAtCore || !available {
            return reason
        }
        let status = status(source)
        if status.state == .connected, let connection = Self.connection(source), let hostKey = connection.hostKey,
           let portKey = connection.portKey {
            let host = setting(hostKey, connection.hostDefault)
            let port = setting(portKey, String(connection.portDefault))
            return "\(host):\(port) \u{00B7} connected"
        }
        if !status.text.isEmpty {
            return status.text
        }
        return Self.stateWords(status.state)
    }

    static func stateWords(_ state: SourceState) -> String {
        switch state {
        case .off: return "Off"
        case .connecting: return "Connecting"
        case .connected: return "Connected"
        case .error: return "Error"
        }
    }

    /// Your identity line on Spot Hub: "KG4VCF · grid not set · used by every source".
    var identitySummary: String {
        let call = setting(Self.callsignKey, "")
        let grid = setting(Self.gridKey, "")
        return [call.isEmpty ? "Callsign not set" : call, grid.isEmpty ? "grid not set" : grid,
                "used by every source"].joined(separator: " \u{00B7} ")
    }

    // MARK: Settings

    /// The Core's setting `key`, or `fallback` while it has none.
    func setting(_ key: String, _ fallback: String) -> String {
        let value = stationSettings[key] ?? ""
        return value.isEmpty ? fallback : value
    }

    func flag(_ key: String) -> Bool {
        stationSettings[key] == "True"
    }

    /// The callsign a source signs in with: its own, else your identity's.
    func callsign(for source: Source) -> String {
        guard let key = Self.connection(source)?.callsignKey else {
            return ""
        }
        return setting(key, setting(Self.callsignKey, ""))
    }

    /// Writes one of the Core's settings; the Core's refusal goes on `source`'s page, or under Display.
    func write(_ key: String, _ value: String, for source: Source?) {
        guard let settings else {
            return
        }
        if stationSettings[key] == value {
            return
        }
        let slot = source?.rawValue ?? "display"
        let edit = settingOutcomeOwner.begin(slot)
        Task { [weak self] in
            let outcome = await settings.write(key, value, onLateOutcome: { [weak self] outcome in
                self?.receiveSetting(outcome, source: source, edit: edit)
            })
            self?.receiveSetting(outcome, source: source, edit: edit)
        }
    }

    private func receiveSetting(_ outcome: SettingsWriteOutcome, source: Source?, edit: UInt64) {
        guard settingOutcomeOwner.isCurrent(edit, source?.rawValue ?? "display"),
              !outcome.propertyOutcome.heldForQuestion else { return }
        switch outcome {
        case .accepted: break
        case .rejected(let reason): if !reason.isEmpty { note(reason, for: source) }
        case .notConfirmed: note(PropertyWriteOutcome.notConfirmed.reason, for: source)
        case .linkLost: note(PropertyWriteOutcome.linkLost.reason, for: source)
        case .keptOnThisDevice, .notSent: Self.logger.info("A spot setting was not stored by the Core")
        }
    }

    // MARK: The lifetime (the Core's)

    /// The lifetime's steps: every 5 seconds from 10 to 55, every 5
    /// minutes from 5 to 55, every hour from 1 to 24, in seconds.
    static let lifetimeSteps: [Int] = Array(stride(from: 10, through: 55, by: 5))
        + Array(stride(from: 5, through: 55, by: 5)).map { $0 * 60 }
        + Array(1...24).map { $0 * 3600 }

    var lifetimeSeconds: Int {
        Int(setting(Self.lifetimeKey, "")) ?? Self.lifetimeDefaultSeconds
    }

    /// The step nearest the Core's lifetime.
    var lifetimeStep: Int {
        let seconds = lifetimeSeconds
        return Self.lifetimeSteps.indices.min { abs(Self.lifetimeSteps[$0] - seconds) < abs(Self.lifetimeSteps[$1] - seconds) }
            ?? 0
    }

    func setLifetimeStep(_ step: Int) {
        let index = min(max(step, 0), Self.lifetimeSteps.count - 1)
        write(Self.lifetimeKey, String(Self.lifetimeSteps[index]), for: nil)
    }

    /// "30 mins", "45 sec", "1 hr", "1 day".
    static func lifetimeWords(_ seconds: Int) -> String {
        if seconds < 60 {
            return "\(seconds) sec"
        }
        if seconds < 3600 {
            let minutes = seconds / 60
            return minutes == 1 ? "1 min" : "\(minutes) mins"
        }
        let hours = seconds / 3600
        if hours == 24 {
            return "1 day"
        }
        return hours == 1 ? "1 hr" : "\(hours) hrs"
    }

    // MARK: This phone's display

    /// Why Auto mode can't set a slice's mode from a spot, or nil. Only a
    /// Core at `recordStreamVersion` 2 names each spot's mode; the phone
    /// does not work one out itself. Away from a Core the switch is this
    /// phone's own setting, so it stays free to change.
    var autoModeReason: String? {
        guard connected else {
            return nil
        }
        return recordVersion >= RecordStreamClient.resolvedModeVersion ? nil : Self.noSpotModeReason
    }

    func changeDisplay(_ change: (inout SpotDisplaySettings) -> Void) {
        var next = display
        change(&next)
        next = next.clamped
        guard next != display else {
            return
        }
        display = next
        if let data = try? JSONEncoder().encode(next) {
            phone.setString(String(decoding: data, as: UTF8.self), for: Self.displayKey)
        }
    }

    func showsOnBand(_ source: Source) -> Bool {
        !display.hiddenSources.contains(source.spotLabel)
    }

    func toggleBandSource(_ source: Source) {
        changeDisplay { settings in
            if settings.hiddenSources.contains(source.spotLabel) {
                settings.hiddenSources.remove(source.spotLabel)
            } else {
                settings.hiddenSources.insert(source.spotLabel)
            }
        }
    }

    private static func readDisplay(_ phone: PhoneSettings) -> SpotDisplaySettings {
        let text = phone.string(displayKey, default: "")
        guard !text.isEmpty else {
            return .desktopDefaults
        }
        do {
            return try JSONDecoder().decode(SpotDisplaySettings.self, from: Data(text.utf8))
        } catch {
            logger.warning("This phone's spot display settings could not be read; the defaults apply")
            return .desktopDefaults
        }
    }

    // MARK: The Core's verbs

    /// Why Connect, Disconnect and a typed command can't run for `source`, or nil.
    func connectReason(_ source: Source) -> String? {
        sourceReason(source)
    }

    func commandReason(_ source: Source) -> String? {
        if let reason = sourceReason(source) {
            return reason
        }
        return source.takesCommands ? nil : Self.noCommandsReason
    }

    /// Connects or disconnects one of the Core's sources.
    func setRunning(_ source: Source, _ on: Bool) {
        // Only the four sources Spot Hub's source pages run; FreeDV Reporter is not started from here.
        guard source.runsAtCore, allowed(source, connectReason(source)) else {
            return
        }
        send(source, on ? "spots.connect" : "spots.disconnect",
             [CommandArgument(name: "source", value: .text(source.rawValue))])
    }

    /// Types a line into a source's console at the Core.
    func sendCommand(_ source: Source, _ text: String) {
        guard allowed(source, commandReason(source)) else {
            return
        }
        send(source, "spots.sendCommand", [CommandArgument(name: "source", value: .text(source.rawValue)),
                                           CommandArgument(name: "text", value: .text(text))])
    }

    /// Why Clear all spots can't run, or nil.
    var clearReason: String? {
        if !connected {
            return Self.notConnectedReason
        }
        return available ? nil : Self.olderCoreReason
    }

    /// Clears every spot the Core holds, for every device.
    func clearAll() {
        if let reason = clearReason {
            displayNote = reason
            return
        }
        Task { [weak self] in
            guard let self, let commands = self.commands else {
                return
            }
            do {
                let result = try await commands.invoke("spots.clearAll", arguments: [], timeout: .seconds(5))
                self.displayNote = result.accepted ? nil : (result.reason.isEmpty ? nil : result.reason)
            } catch {
                Self.logger.info("Clear all spots had no answer from the Core")
            }
        }
    }

    /// A source page opens: its console is asked for. It closes: it stops.
    func pageOpened(_ source: Source) {
        guard source.runsAtCore else {
            return
        }
        records?.want(RecordStreamClient.consoleStream(source.rawValue), backlog: Self.consoleBacklog, by: self)
    }

    func pageClosed(_ source: Source) {
        guard source.runsAtCore else {
            return
        }
        records?.unwant(RecordStreamClient.consoleStream(source.rawValue), by: self)
    }

    private func allowed(_ source: Source, _ reason: String?) -> Bool {
        guard let reason else {
            return true
        }
        notes[source] = reason
        return false
    }

    private func note(_ reason: String, for source: Source?) {
        if let source {
            notes[source] = reason
        } else {
            displayNote = reason
        }
    }

    private func send(_ source: Source, _ verb: String, _ arguments: [CommandArgument]) {
        guard let commands else {
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: .seconds(5))
                if result.accepted {
                    self?.notes[source] = nil
                } else if !result.reason.isEmpty {
                    self?.notes[source] = result.reason
                }
            } catch {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            }
        }
    }

    // MARK: Times

    private static let wholeSeconds: ISO8601DateFormatter = {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime]
        return formatter
    }()

    private static let fractionalSeconds: ISO8601DateFormatter = {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter
    }()

    /// A spot's `timeUtc`, ISO 8601, with or without fractions of a second.
    static func time(_ text: String) -> Date? {
        wholeSeconds.date(from: text) ?? fractionalSeconds.date(from: text)
    }

    private static func utcFormatter(_ format: String) -> DateFormatter {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(identifier: "UTC")
        formatter.dateFormat = format
        return formatter
    }

    private static let shortFormatter = utcFormatter("HH:mm")
    private static let longFormatter = utcFormatter("HH:mm:ss")

    /// "19:43", in UTC.
    static func shortTime(_ time: Date) -> String {
        shortFormatter.string(from: time)
    }

    /// "19:42:10", in UTC.
    static func longTime(_ time: Date) -> String {
        longFormatter.string(from: time)
    }
}
