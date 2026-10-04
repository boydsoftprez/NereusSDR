// NereusSDR for iOS: FreeDV Reporter as the Core runs it: its stations, tints, filters, status message and requests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
import os

/// FreeDV Reporter on the phone (R-IOS-26, D33, spec section 5.7 items 5 to
/// 10). The Core holds the reporter connection, registers with the
/// operator's callsign (never the Core's label), and sends the stations it
/// hears as the `freedvStations` record stream, the reporter's console as
/// `spotConsole:freedvReporter` and its state on `spotSources` (link
/// document sections 7.1, 7.7 and 9.1). This model lists those stations,
/// tints them, filters them, and asks the Core to start or stop the
/// reporter, hide the station, send the status message and pass a QSY
/// request. Miles or kilometres, kHz or MHz, the band filter and "follow the
/// radio" are this phone's own.
///
/// It lives on ``AppModel`` beside ``SpotsModel``: the Tools tab is rebuilt
/// on every tab switch, and the tints need the times each change arrived.
@MainActor
final class FreeDVReporterModel: ObservableObject {
    /// What "follow the radio" follows.
    enum Follow: String {
        case band
        case frequency
    }

    /// A row's tint, in the order one beats another.
    enum Tint: Equatable {
        /// Its message changed (mauve).
        case message
        /// It is transmitting (rust).
        case transmitting
        /// It is hearing someone (slate).
        case hearing
    }

    // MARK: Words

    static let olderCoreReason = SpotsModel.freeDvReason
    static let notConnectedReason = SpotsModel.notConnectedReason
    static let noBandReason = "This Core does not say which band each station is on. Updating the Core may help."
    static let reporterOffReason = "FreeDV Reporter is not connected on the Core."
    static let noFrequencyReason = "This station has not said its frequency."
    static let noQsyFrequencyReason = "Enter a frequency for the QSY request first."
    static let noSliceReason = SpotsModel.noSliceReason
    static let lockedReason = "The active slice is locked. Unlock it to tune there."
    static let gridNote =
        "Distance and heading need your grid square. Set it once in Spot Hub, Settings, and every source uses it."
    static let website = URL(string: "https://qso.freedv.org")!
    static let server = "qso.freedv.org"

    // MARK: Settings

    /// The Core's settings this page reads and writes (Station scope).
    static let messageKey = "FreeDvReporter/Message"
    static let savedMessagesKey = "FreeDvReporter/SavedMessages"
    static let autoStartKey = "FreeDvAutoStart"
    static let callsignKeys = ["FreeDvReporter/Callsign", "User/Callsign", "StationCallsign"]
    static let gridKeys = ["FreeDvReporter/GridSquare", "User/GridSquare"]
    /// As many saved messages as the desktop keeps.
    static let savedLimit = 10
    /// This phone's own choices.
    static let milesKey = "freedv.distanceMiles"
    static let kilohertzKey = "freedv.frequencyKhz"
    static let bandFilterKey = "freedv.bandFilter"
    static let followKey = "freedv.follow"

    /// How long a tint lasts after the change that set it, as on the desktop.
    static let fadeSeconds: TimeInterval = 6
    static let stationsBacklog = FreeDVStation.capacity
    static let consoleBacklog = 200
    static let consoleStream = RecordStreamClient.consoleStream(FreeDVStation.sourceName)
    static let kilometresPerMile = 1.609344

    // MARK: State

    /// Every station the Core lists, newest news first.
    @Published private(set) var stations: [FreeDVStation] = []
    /// The Core's console lines, oldest first.
    @Published private(set) var console: [String] = []
    /// The Core's `stationFreedvVersion`: 0 before FreeDV Reporter, 2 once it names each station's band.
    @Published private(set) var version: Int64 = 0
    /// The phone is connected to the Core.
    @Published private(set) var connected = false
    /// The reporter's state at the Core, from `spotSources`.
    @Published private(set) var status: SpotsModel.SourceStatus?
    /// The Core's settings this page shows.
    @Published private(set) var stationSettings: [String: String] = [:]
    /// This phone's choices.
    @Published private(set) var miles: Bool
    @Published private(set) var kilohertz: Bool
    @Published private(set) var bandFilter: Int?
    @Published private(set) var follow: Follow?
    /// The Core's words for a request it refused, on the reporter page and on the FreeDV page.
    @Published var note: String?
    @Published var sourceNote: String?
    /// What is open over the list.
    @Published var openDetails: FreeDVStation?
    @Published var openQsy: FreeDVStation?
    /// The status message editor is open. Closing it drops what was typed
    /// and not sent, so the bar shows the Core's message again. A message
    /// sent and not answered stays until the Core answers.
    @Published var editingMessage = false {
        didSet {
            if !editingMessage, messageInFlight == nil || statusDraft != messageInFlight {
                statusDraft = nil
            }
        }
    }
    /// The status message as typed here, until the Core has it; nil shows
    /// the Core's. A change of the Core's message, from here or elsewhere,
    /// drops it.
    @Published var statusDraft: String?
    /// The status message last sent, until the Core answers it.
    private var messageInFlight: String?
    /// The callsign the last QSY request went to, once the Core took it.
    @Published private(set) var qsySentTo: String?

    static let logger = Logger(subsystem: "NereusSDR", category: "freedv")
    static let setMessageVerb = "freedv.setMessage"

    private let records: RecordStreamClient?
    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let settings: SettingsProxyClient?
    private let slices: BandSlicesModel
    private let catalogFeed: CatalogFeed
    private let phone: PhoneSettings
    private let now: () -> Date
    private var watches: Set<AnyCancellable> = []
    private var consoleOpen = false

    /// When this phone last saw each station's news that tints it.
    private struct Marks {
        var message: Date?
        var transmitting: Date?
        var hearing: Date?
    }

    private var marks: [String: Marks] = [:]
    private var previous: [String: FreeDVStation] = [:]

    init(records: RecordStreamClient?, mirror: MirrorStore, commands: CommandClient?, settings: SettingsProxyClient?,
         spots: SpotsModel, slices: BandSlicesModel, catalogFeed: CatalogFeed, phone: PhoneSettings,
         now: @escaping () -> Date = Date.init) {
        self.records = records
        self.mirror = mirror
        self.commands = commands
        self.settings = settings
        self.slices = slices
        self.catalogFeed = catalogFeed
        self.phone = phone
        self.now = now
        miles = phone.bool(Self.milesKey, default: false)
        kilohertz = phone.bool(Self.kilohertzKey, default: false)
        let band = phone.integer(Self.bandFilterKey, default: -1)
        bandFilter = band >= 0 ? band : nil
        // The desktop follows the radio's band unless told otherwise.
        let followed = phone.string(Self.followKey, default: Follow.band.rawValue)
        follow = Follow(rawValue: followed)
        records?.$streams.sink { [weak self] streams in
            self?.read(streams)
        }.store(in: &watches)
        settings?.$values.sink { [weak self] values in
            guard let self else {
                return
            }
            let before = message
            stationSettings = values
            if message != before {
                statusDraft = nil
            }
        }.store(in: &watches)
        spots.$freeDvStatus.sink { [weak self] status in
            self?.status = status
        }.store(in: &watches)
        mirror.$isSnapshotComplete.combineLatest(mirror.$isStale).sink { [weak self] complete, stale in
            self?.connected = complete && !stale
        }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in
            Task { @MainActor [weak self] in self?.readVersion() }
        }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &watches)
    }

    // MARK: Reading

    private func readVersion() {
        let next = mirror.capabilityVersion(FreeDVStation.capabilityName)
        if next != version {
            version = next
        }
        // A window subscribes to the list only from a Core that runs FreeDV Reporter.
        if next >= 1 {
            records?.want(FreeDVStation.streamName, backlog: Self.stationsBacklog, by: self)
            if consoleOpen {
                records?.want(Self.consoleStream, backlog: Self.consoleBacklog, by: self)
            }
        } else {
            records?.unwant(FreeDVStation.streamName, by: self)
            records?.unwant(Self.consoleStream, by: self)
        }
    }

    private func read(_ streams: [String: RecordStreamClient.Stream]) {
        let records = streams[FreeDVStation.streamName]?.records ?? []
        let read = records.map(FreeDVStation.init(record:))
        let seeding = previous.isEmpty
        let at = now()
        var next: [String: FreeDVStation] = [:]
        for station in read {
            next[station.id] = station
            let before = previous[station.id]
            guard before != station else {
                continue
            }
            var mark = marks[station.id] ?? Marks()
            if station.transmitting {
                mark.transmitting = at
            }
            if !station.receivingFrom.isEmpty {
                mark.hearing = at
            }
            let messageChanged = before.map { $0.messageChangedAtMs != station.messageChangedAtMs } ?? !seeding
            if messageChanged, station.messageChangedAtMs > 0, !station.userMessage.isEmpty {
                mark.message = at
            }
            marks[station.id] = mark
        }
        marks = marks.filter { next[$0.key] != nil }
        previous = next
        let sorted = read.sorted { left, right in
            switch (left.lastUpdate, right.lastUpdate) {
            case let (l?, r?) where l != r:
                return l > r
            case (.some, nil):
                return true
            case (nil, .some):
                return false
            default:
                return left.callsign < right.callsign
            }
        }
        if sorted != stations {
            stations = sorted
        }
        if let open = openDetails {
            openDetails = next[open.id]
        }
        let lines = (streams[Self.consoleStream]?.records ?? []).map { record -> String in
            if case .string(let line)? = record.fields["line"] {
                return line
            }
            return ""
        }
        if lines != console {
            console = lines
        }
    }

    /// A station's tint at `date`, or nil: its message changed, it is
    /// transmitting, or it is hearing someone, each for six seconds after
    /// the phone saw it.
    func tint(_ station: FreeDVStation, at date: Date) -> Tint? {
        guard let mark = marks[station.id] else {
            return nil
        }
        func recent(_ time: Date?) -> Bool {
            guard let time else {
                return false
            }
            return date.timeIntervalSince(time) < Self.fadeSeconds
        }
        if recent(mark.message) {
            return .message
        }
        if recent(mark.transmitting) {
            return .transmitting
        }
        if recent(mark.hearing) {
            return .hearing
        }
        return nil
    }

    func tint(_ station: FreeDVStation) -> Tint? {
        tint(station, at: now())
    }

    func station(id: String) -> FreeDVStation? {
        stations.first { $0.id == id }
    }

    // MARK: Reasons

    /// The Core runs FreeDV Reporter for this app.
    var runsReporter: Bool {
        version >= 1
    }

    /// Why the page, its requests and its settings can't reach the Core, or nil.
    var coreReason: String? {
        if !runsReporter {
            return Self.olderCoreReason
        }
        return connected ? nil : Self.notConnectedReason
    }

    /// Why Send QSY can't run, or nil.
    var qsyReason: String? {
        if let reason = coreReason {
            return reason
        }
        return status?.state == .connected ? nil : Self.reporterOffReason
    }

    /// Why the band buttons and follow-by-band can't filter, or nil.
    var bandReason: String? {
        version >= FreeDVStation.bandVersion ? nil : Self.noBandReason
    }

    /// Why a station can't be tuned to now, or nil.
    func tuneReason(_ station: FreeDVStation) -> String? {
        if station.frequencyHz <= 0 {
            return Self.noFrequencyReason
        }
        return sliceReason
    }

    /// Why the active slice can't be tuned from here, or nil: on a slice
    /// this phone only listens to, its owner line; on a locked one, that
    /// it is locked.
    var sliceReason: String? {
        guard let active = slices.active else {
            return Self.noSliceReason
        }
        if active.listening {
            return active.ownerLine ?? Self.lockedReason
        }
        return active.locked ? Self.lockedReason : nil
    }

    // MARK: Filters

    /// The band buttons: the Core's band grid's amateur bands, as `40m`.
    var bandButtons: [(id: Int, label: String)] {
        (catalogFeed.catalog?.bands ?? []).compactMap { band in
            guard !band.label.isEmpty, band.label.allSatisfy(\.isNumber) else {
                return nil
            }
            return (band.id, band.label + "m")
        }
    }

    /// "Follow the radio" as it acts now: by band only on a Core that names bands.
    var effectiveFollow: Follow? {
        if follow == .band, bandReason != nil {
            return nil
        }
        return follow
    }

    /// The band button that is lit, or nil for All.
    var selectedBand: Int? {
        guard bandReason == nil else {
            return nil
        }
        switch effectiveFollow {
        case .band?:
            return slices.active?.band
        case .frequency?:
            return nil
        case nil:
            return bandFilter
        }
    }

    /// The active slice's letter, `A`.
    var sliceLetter: String? {
        slices.active?.slice.letter
    }

    /// The active slice's frequency, whole hertz.
    var sliceFrequencyHz: Double? {
        slices.active.map { $0.slice.frequencyHz.rounded() }
    }

    /// The stations the list shows.
    var listed: [FreeDVStation] {
        if effectiveFollow == .frequency {
            guard let hz = sliceFrequencyHz else {
                return []
            }
            // Exact, as on the desktop: both are the dial frequency in whole hertz.
            return stations.filter { $0.frequencyHz.rounded() == hz }
        }
        guard let band = selectedBand else {
            return stations
        }
        return stations.filter { $0.band == band }
    }

    /// A band button, or All (nil). Picking one stops following the radio's band.
    func pickBand(_ band: Int?) {
        if band != nil, bandReason != nil {
            note = bandReason
            return
        }
        if follow == .band || (band != nil && follow == .frequency) {
            setFollow(nil)
        }
        bandFilter = band
        phone.setInteger(band ?? -1, for: Self.bandFilterKey)
    }

    /// Band or Frequency; the lit one again turns following off.
    func toggleFollow(_ choice: Follow) {
        if choice == .band, let reason = bandReason {
            note = reason
            return
        }
        setFollow(effectiveFollow == choice ? nil : choice)
    }

    private func setFollow(_ choice: Follow?) {
        follow = choice
        phone.setString(choice?.rawValue ?? "", for: Self.followKey)
    }

    func setMiles(_ on: Bool) {
        miles = on
        phone.setBool(on, for: Self.milesKey)
    }

    func setKilohertz(_ on: Bool) {
        kilohertz = on
        phone.setBool(on, for: Self.kilohertzKey)
    }

    // MARK: Words for a station

    /// "14.2360" in MHz, or "14236.0" in kHz; "-" when not known.
    func frequencyText(_ hz: Double) -> String {
        guard hz > 0 else {
            return "-"
        }
        return kilohertz ? String(format: "%.1f", hz / 1_000) : String(format: "%.4f", hz / 1_000_000)
    }

    /// "14.2360 MHz" or "14236.0 kHz".
    func frequencyWithUnit(_ hz: Double) -> String {
        guard hz > 0 else {
            return "Not known"
        }
        return frequencyText(hz) + (kilohertz ? " kHz" : " MHz")
    }

    /// "812 km" or "505 mi"; nil until both grid squares are known.
    func distanceText(_ station: FreeDVStation) -> String? {
        guard station.hasBearing else {
            return nil
        }
        let value = miles ? station.distanceKm / Self.kilometresPerMile : station.distanceKm
        return "\(Int(value.rounded())) \(miles ? "mi" : "km")"
    }

    /// "045° NE"; nil until both grid squares are known.
    static func headingText(_ station: FreeDVStation) -> String? {
        guard station.hasBearing else {
            return nil
        }
        let degrees = (Int(station.headingDeg.rounded()) % 360 + 360) % 360
        return String(format: "%03d\u{00B0} ", degrees) + station.headingCardinal
    }

    /// "now", "4 min", "2 hr", from the phone's clock.
    func age(_ time: Date?) -> String {
        guard let time else {
            return "-"
        }
        let seconds = max(0, now().timeIntervalSince(time))
        if seconds < 60 {
            return "now"
        }
        if seconds < 3600 {
            return "\(Int(seconds / 60)) min"
        }
        if seconds < 86_400 {
            return "\(Int(seconds / 3600)) hr"
        }
        return "\(Int(seconds / 86_400)) d"
    }

    /// The second line: "Transmitting", "Hearing W1ABC RADE, SNR 6 dB", "Receive only" or "Receiving".
    static func stateLine(_ station: FreeDVStation) -> (lead: String, callsign: String?, rest: String) {
        if station.transmitting || station.status == "TX" {
            return ("Transmitting", nil, "")
        }
        let heard = station.receivingFrom.isEmpty ? station.lastRxCallsign : station.receivingFrom
        if !heard.isEmpty {
            var rest = station.lastRxMode
            if let snr = station.snrDb {
                rest += rest.isEmpty ? "SNR \(snr) dB" : ", SNR \(snr) dB"
            }
            return ("Hearing", heard, rest)
        }
        return (station.status == "RX Only" ? "Receive only" : "Receiving", nil, "")
    }

    /// The details' rows: every column the desktop shows, in its words.
    func detailRows(_ station: FreeDVStation) -> [(name: String, value: String)] {
        var rows: [(String, String)] = [("Locator", station.gridSquare.isEmpty ? "-" : station.gridSquare)]
        let unset = "Set your grid to see it"
        rows.append(("Distance", distanceText(station) ?? unset))
        rows.append(("Heading", Self.headingText(station) ?? unset))
        rows.append(("Frequency", frequencyWithUnit(station.frequencyHz)))
        rows.append(("Mode", station.txMode.isEmpty ? "-" : station.txMode))
        rows.append(("Status", Self.stateLine(station).lead))
        if !station.userMessage.isEmpty {
            rows.append(("Message", station.userMessage))
        }
        rows.append(("Last TX", age(station.lastTx)))
        if !station.lastRxCallsign.isEmpty {
            rows.append(("Last heard", station.lastRxCallsign))
        }
        if !station.lastRxMode.isEmpty {
            rows.append(("Heard in", station.lastRxMode))
        }
        rows.append(("SNR", station.snrDb.map { "\($0) dB" } ?? "-"))
        rows.append(("Software", station.version.isEmpty ? "-" : station.version))
        rows.append(("Last update", age(station.lastUpdate)))
        return rows
    }

    /// Where to look a callsign up, opened in Safari.
    static func qrzURL(_ callsign: String) -> URL? {
        lookup("https://www.qrz.com/db/", callsign)
    }

    static func hamQthURL(_ callsign: String) -> URL? {
        lookup("https://www.hamqth.com/", callsign)
    }

    private static func lookup(_ base: String, _ callsign: String) -> URL? {
        let trimmed = callsign.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty,
              let path = trimmed.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) else {
            return nil
        }
        return URL(string: base + path)
    }

    // MARK: The Core's settings

    func setting(_ key: String) -> String {
        stationSettings[key] ?? ""
    }

    /// The callsign FreeDV Reporter lists: the reporter's own, else your identity's. Never the Core's label.
    var callsign: String {
        Self.callsignKeys.lazy.map(setting).first { !$0.isEmpty } ?? ""
    }

    var gridSquare: String {
        Self.gridKeys.lazy.map(setting).first { !$0.isEmpty } ?? ""
    }

    /// The status message the Core sends.
    var message: String {
        setting(Self.messageKey)
    }

    /// The status message as the bar shows it: what is typed here, else the Core's.
    var statusText: String {
        statusDraft ?? message
    }

    /// The saved messages, newest first, as the Core keeps them.
    var savedMessages: [String] {
        setting(Self.savedMessagesKey).split(separator: "\n", omittingEmptySubsequences: true).map(String.init)
    }

    var autoStart: Bool {
        setting(Self.autoStartKey) == "True"
    }

    func setAutoStart(_ on: Bool) {
        write(Self.autoStartKey, on ? "True" : "False")
    }

    /// Keeps `text` at the top of the saved messages, ten at most.
    func saveMessage(_ text: String) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, coreReason == nil else {
            return
        }
        var saved = savedMessages.filter { $0 != trimmed }
        saved.insert(trimmed, at: 0)
        write(Self.savedMessagesKey, saved.prefix(Self.savedLimit).joined(separator: "\n"))
    }

    private func write(_ key: String, _ value: String) {
        guard let settings, stationSettings[key] != value else {
            return
        }
        Task { [weak self] in
            let outcome = await settings.write(key, value)
            if case .rejected(let reason) = outcome, !reason.isEmpty {
                self?.note = reason
            }
        }
    }

    // MARK: The Core's requests

    /// Starts or stops FreeDV Reporter at the Core.
    func setRunning(_ on: Bool) {
        if let reason = coreReason {
            sourceNote = reason
            return
        }
        send(on ? "spots.connect" : "spots.disconnect",
             [CommandArgument(name: "source", value: .text(FreeDVStation.sourceName))], onSourcePage: true)
    }

    /// "Hide my station".
    func setHidden(_ on: Bool) {
        if let reason = coreReason {
            sourceNote = reason
            return
        }
        send("freedv.setHidden", [CommandArgument(name: "on", value: .bool(on))], onSourcePage: true)
    }

    /// Sends the status message (empty clears it). The Core keeps it for the next connection.
    func sendMessage(_ text: String) {
        if let reason = coreReason {
            note = reason
            return
        }
        messageInFlight = text
        send(Self.setMessageVerb, [CommandArgument(name: "text", value: .text(text))], onSourcePage: false) { [weak self] accepted in
            guard let self, messageInFlight == text else {
                return
            }
            messageInFlight = nil
            if accepted, statusDraft == message {
                // The Core has the message typed here: the bar shows the Core's.
                statusDraft = nil
            } else if !accepted, !editingMessage, statusDraft == text {
                // Not taken, and the editor is closed: the bar shows the Core's message.
                statusDraft = nil
            }
        }
    }

    /// Tunes the active slice to a station (D33), and closes its details.
    func tune(_ station: FreeDVStation) {
        if let reason = tuneReason(station) {
            note = reason
            return
        }
        openDetails = nil
        slices.tap(to: station.frequencyHz)
    }

    func showDetails(_ station: FreeDVStation) {
        openDetails = station
    }

    func askToQsy(_ station: FreeDVStation) {
        openDetails = nil
        qsySentTo = nil
        openQsy = station
    }

    /// Asks `station` to move to `hz`, and tunes this radio there too, as on the desktop.
    func sendQsy(_ station: FreeDVStation, hz: Double) {
        if let reason = qsyReason {
            note = reason
            return
        }
        guard hz > 0 else {
            note = Self.noQsyFrequencyReason
            return
        }
        let callsign = station.callsign
        Task { [weak self] in
            guard let self, let commands = self.commands else {
                return
            }
            do {
                let result = try await commands.invoke("freedv.sendQsy", arguments: [
                    CommandArgument(name: "callsign", value: .text(callsign)),
                    CommandArgument(name: "frequencyHz", value: .int(Int64(hz.rounded()))),
                ], timeout: .seconds(5))
                if result.accepted {
                    self.note = nil
                    self.qsySentTo = callsign
                } else if !result.reason.isEmpty {
                    self.note = result.reason
                }
            } catch {
                Self.logger.info("A QSY request had no answer from the Core")
            }
        }
        if sliceReason == nil {
            slices.tap(to: hz.rounded())
        }
    }

    /// The FreeDV page opens: its console is asked for. It closes: it stops.
    func consoleOpened() {
        consoleOpen = true
        if runsReporter {
            records?.want(Self.consoleStream, backlog: Self.consoleBacklog, by: self)
        }
    }

    func consoleClosed() {
        consoleOpen = false
        records?.unwant(Self.consoleStream, by: self)
    }

    private func send(_ verb: String, _ arguments: [CommandArgument], onSourcePage: Bool,
                      answered: (@MainActor (_ accepted: Bool) -> Void)? = nil) {
        guard let commands else {
            answered?(false)
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: .seconds(5))
                let words: String? = result.accepted ? nil : (result.reason.isEmpty ? nil : result.reason)
                answered?(result.accepted)
                if onSourcePage {
                    self?.sourceNote = words
                } else {
                    self?.note = words
                }
            } catch {
                answered?(false)
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            }
        }
    }
}
