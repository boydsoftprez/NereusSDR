// NereusSDR for iOS: Setup's Devices page's state: who is on the Core, who is paired, the key backup and the pairing window
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels
import os

/// What Setup, Devices shows and does (R-IOS-08, R-IOS-17, D57; spec
/// section 5.2 item 8, section 5.8 item 13, section 5.9 item 4; picture
/// 22), read from the Core's `devices` and `connectedDevices` objects:
///
/// - the Core's name, and the reminder to back up its key until the Core
///   says the backup was confirmed;
/// - Connected now: each device in the order the Core let it in, with its
///   slice letters in their colours and their bands, how long it has been
///   connected (or an amber "away for" while its link is down), and TX on
///   the one with transmit;
/// - Paired: the paired devices not connected now, with when each paired
///   and was last seen;
/// - one-tap Revoke on every device but this phone. Where the Core says a
///   device can't be removed (the computer running the Core, a computer
///   signed in with the pairing token) Revoke is greyed with the reason; a
///   refused revoke shows the Core's words under that device;
/// - Add a device, which opens pairing and shows the Core's code, and
///   Close pairing.
///
/// Nothing changes here until the Core says so: a revoked device leaves
/// the list when the Core's objects drop it.
@MainActor
final class DevicesModel: ObservableObject {
    /// One slice a device listens on: its letter, its colour and its band.
    struct SliceTag: Equatable, Sendable {
        let letter: String
        let colour: String
        let band: String
    }

    /// Whether a device's Revoke can be used.
    enum Revoke: Equatable, Sendable {
        /// This phone's own row carries none.
        case none
        case available
        /// Greyed, with the reason.
        case unavailable(String)
    }

    /// A device connected now.
    struct ConnectedRow: Identifiable, Equatable, Sendable {
        let id: String
        let name: String
        let isThisPhone: Bool
        let holdsTransmit: Bool
        let away: Bool
        let slices: [SliceTag]
        /// After the slices: "this phone", how long it has been connected,
        /// or "away for 1 minute".
        let line: String
        let revoke: Revoke
    }

    /// A device paired and not connected now.
    struct PairedRow: Identifiable, Equatable, Sendable {
        let id: String
        let name: String
        let isThisPhone: Bool
        /// "Paired Jun 30 \u{00B7} last seen 41 days ago".
        let line: String
        let revoke: Revoke
    }

    @Published private(set) var coreName: String?
    @Published private(set) var connected: [ConnectedRow] = []
    @Published private(set) var paired: [PairedRow] = []
    /// The Core lists who is connected (`connectedDevices`).
    @Published private(set) var listsConnected = false
    /// The Core has the devices object and takes the device verbs.
    @Published private(set) var administers = false
    /// The Core opens and closes pairing for this app.
    @Published private(set) var pairs = false
    @Published private(set) var keyBackupNeeded = false
    @Published private(set) var keyPath = ""
    @Published private(set) var pairingOpen = false
    /// The Core's pairing code while the window is open; never logged.
    @Published private(set) var pairingCode = ""
    /// The Core's words for a refused revoke, by device id.
    @Published private(set) var revokeProblems: [String: String] = [:]
    /// Revokes the Core has not answered.
    @Published private(set) var revoking: Set<String> = []
    @Published private(set) var pairingProblem: String?
    @Published private(set) var pairingBusy = false
    @Published private(set) var backupProblem: String?
    /// The device limit the Core reports.
    @Published private(set) var deviceLimit: Int64 = 4

    static let needsNewerCoreText = CatalogFeed.needsNewerCoreText
    static let hostsCoreReason = "This computer runs the Core, so it can't be removed from here."
    static let tokenReason = "This computer signs in with the Core's pairing token, not as a paired device, so there is nothing to remove."
    static let noAnswerText = "The Core didn't answer. Try again."
    static let refusedText = "The Core refused."
    static let commandTimeout: Duration = .seconds(10)

    private let mirror: MirrorStore
    private let commands: CommandClient
    private let catalogFeed: CatalogFeed
    private let thisDeviceId: () -> String?
    private let now: () -> Date
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watched: [String: ObjectIdentifier] = [:]
    private var refreshQueued = false
    private static let logger = Logger(subsystem: "NereusSDR", category: "setup.devices")

    init(mirror: MirrorStore, commands: CommandClient, catalogFeed: CatalogFeed,
         thisDeviceId: @escaping () -> String?, now: @escaping () -> Date = Date.init) {
        self.mirror = mirror
        self.commands = commands
        self.catalogFeed = catalogFeed
        self.thisDeviceId = thisDeviceId
        self.now = now
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        refresh()
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

    /// Follows one object's changes, re-watching when the Core makes it anew.
    private func watch(_ key: String) -> MirrorObject? {
        let object = mirror.object(key)
        let identity = object.map(ObjectIdentifier.init)
        if watched[key] != identity {
            watched[key] = identity
            objectWatches[key] = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRefresh() }
        }
        return object
    }

    /// Reads the Core's objects again.
    func refresh() {
        let devices = watch(StationDevices.key)
        let connectedObject = watch(SeveralDevices.connectedDevicesKey)
        set(\.administers, devices != nil && StationDevices.administers(mirror))
        set(\.pairs, devices != nil && StationDevices.pairs(mirror))
        var label = ""
        if case .text(let text)? = devices?["stationLabel"] {
            label = text
        }
        set(\.coreName, label.isEmpty ? nil : label)
        set(\.keyBackupNeeded, devices != nil && devices?["keyBackupAcknowledged"] != .bool(true))
        var path = ""
        if case .text(let text)? = devices?["keyPath"] {
            path = text
        }
        set(\.keyPath, path)
        set(\.pairingOpen, devices?["pairingWindowOpen"] == .bool(true))
        var code = ""
        if case .text(let text)? = devices?["pairingCode"] {
            code = text
        }
        set(\.pairingCode, pairingOpen ? code : "")
        if case .int(let limit)? = connectedObject?["deviceLimit"] {
            set(\.deviceLimit, limit)
        }
        let lists = connectedObject != nil && SeveralDevices.available(in: mirror)
        set(\.listsConnected, lists)
        let me = thisDeviceId()
        let present = lists ? SeveralDevices.connectedDevices(in: mirror) : []
        set(\.connected, present.map { row($0, me: me) })
        let presentIds = Set(present.map(\.deviceId))
        let pairedDevices = StationDevices.pairedDevices(in: mirror)
        // Without the Core's list of who is connected, every paired device
        // is listed here, this phone's own among them.
        let rest = lists ? pairedDevices.filter { !presentIds.contains($0.id) && !($0.connected && $0.id == me) }
            : pairedDevices
        set(\.paired, rest.map { row($0, me: me) })
        // A device that has gone takes its problem with it.
        let known = presentIds.union(pairedDevices.map(\.id))
        let problems = revokeProblems.filter { known.contains($0.key) }
        set(\.revokeProblems, problems)
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<DevicesModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }

    // MARK: Rows

    private func row(_ device: SeveralDevices.ConnectedDevice, me: String?) -> ConnectedRow {
        let mine = device.deviceId == me
        let away = device.state == .away
        let line: String
        if mine {
            line = "this phone"
        } else if away {
            line = "away for \(SeveralDevicesWords.duration(seconds: device.awayForSeconds))"
        } else {
            line = SeveralDevicesWords.duration(seconds: device.connectedForSeconds)
        }
        return ConnectedRow(id: device.deviceId, name: device.name.isEmpty ? device.shortName : device.name,
                            isThisPhone: mine, holdsTransmit: device.holdsTransmit, away: away,
                            slices: device.listeningOn.map(tag), line: line,
                            revoke: mine ? .none : revoke(connected: device))
    }

    private func row(_ device: StationDevices.PairedDevice, me: String?) -> PairedRow {
        let mine = device.id == me
        var parts: [String] = []
        if mine {
            parts.append("This phone")
        }
        if let pairedAt = device.pairedAt {
            parts.append("Paired \(Self.day(pairedAt, now: now()))")
        }
        if device.connected {
            parts.append("connected now")
        } else if let lastSeen = device.lastSeen {
            parts.append("last seen \(Self.ago(lastSeen, now: now()))")
        } else {
            parts.append("never seen")
        }
        return PairedRow(id: device.id, name: device.name.isEmpty ? device.shortName : device.name,
                         isThisPhone: mine, line: parts.joined(separator: " \u{00B7} "),
                         revoke: mine ? .none : (administers ? .available : .unavailable(Self.needsNewerCoreText)))
    }

    private func revoke(connected device: SeveralDevices.ConnectedDevice) -> Revoke {
        guard administers else {
            return .unavailable(Self.needsNewerCoreText)
        }
        if device.revocable {
            return .available
        }
        return .unavailable(device.hostsCore ? Self.hostsCoreReason : Self.tokenReason)
    }

    private func tag(_ slice: SeveralDevices.ListeningSlice) -> SliceTag {
        let catalog = catalogFeed.catalog
        return SliceTag(letter: slice.letter.isEmpty ? BandSlice.letter(forIndex: slice.sliceId) : slice.letter,
                        colour: BandSlice.colour(forIndex: slice.sliceId, in: catalog?.sliceColours ?? []),
                        band: Self.bandText(slice.band, bands: catalog?.bands ?? []))
    }

    /// A band as the devices list names it: "40 m", or the catalogue's
    /// label when it is not in metres ("WWV"); "" when the Core's catalogue
    /// does not name it.
    static func bandText(_ band: Int, bands: [StationCatalog.Band]) -> String {
        guard let label = bands.first(where: { $0.id == band })?.label, !label.isEmpty else {
            return ""
        }
        return label.allSatisfy(\.isNumber) ? "\(label) m" : label
    }

    /// "Jun 30", with the year when it is not this one.
    static func day(_ date: Date, now: Date) -> String {
        let calendar = Calendar.current
        if calendar.component(.year, from: date) == calendar.component(.year, from: now) {
            return date.formatted(.dateTime.month(.abbreviated).day())
        }
        return date.formatted(.dateTime.month(.abbreviated).day().year())
    }

    /// "just now", "3 minutes ago", "5 hours ago", "yesterday", "41 days ago".
    static func ago(_ date: Date, now: Date) -> String {
        let seconds = Int64(max(0, now.timeIntervalSince(date)))
        let days = seconds / 86_400
        if days == 1 {
            return "yesterday"
        }
        if days > 1 {
            return "\(days) days ago"
        }
        return SeveralDevicesWords.ago(seconds: seconds)
    }

    // MARK: Actions

    /// Revoke: the Core removes the device and drops it at once. Refused,
    /// the Core's words show under the device.
    func revoke(_ id: String) async {
        guard administers, id != thisDeviceId(), !revoking.contains(id) else {
            return
        }
        revoking.insert(id)
        revokeProblems[id] = nil
        defer { revoking.remove(id) }
        do {
            let result = try await commands.invoke(StationDevices.revokeVerb, arguments: [
                CommandArgument(name: "id", value: .text(id)),
            ], timeout: Self.commandTimeout)
            if !result.accepted {
                revokeProblems[id] = result.reason.isEmpty ? Self.refusedText : result.reason
            }
        } catch {
            Self.logger.info("devices.revoke had no answer: \(String(describing: error), privacy: .public)")
            revokeProblems[id] = Self.noAnswerText
        }
    }

    /// Add a device: the Core opens pairing, and its code shows here.
    func openPairing() async {
        await pairingCommand(StationDevices.openPairingVerb)
    }

    /// Close pairing: the code stops working.
    func closePairing() async {
        await pairingCommand(StationDevices.closePairingVerb)
    }

    private func pairingCommand(_ verb: String) async {
        guard pairs, !pairingBusy else {
            return
        }
        pairingBusy = true
        pairingProblem = nil
        defer { pairingBusy = false }
        do {
            let result = try await commands.invoke(verb, arguments: [], timeout: Self.commandTimeout)
            if !result.accepted {
                pairingProblem = result.reason.isEmpty ? Self.refusedText : result.reason
            } else if verb == StationDevices.openPairingVerb, case .text(let code)? = result.values["code"],
                      pairingCode.isEmpty {
                // The code, until the devices object's delta brings it too.
                pairingCode = code
                pairingOpen = true
            }
        } catch {
            // The verb only; never the code.
            Self.logger.info("\(verb, privacy: .public) had no answer: \(String(describing: error), privacy: .public)")
            pairingProblem = Self.noAnswerText
        }
    }

    /// I've backed it up: the Core records the backup, and the reminder goes.
    func acknowledgeBackup() async {
        guard administers else {
            return
        }
        backupProblem = nil
        do {
            let result = try await commands.invoke(StationDevices.acknowledgeKeyBackupVerb, arguments: [],
                                                   timeout: Self.commandTimeout)
            if !result.accepted {
                backupProblem = result.reason.isEmpty ? Self.refusedText : result.reason
            }
        } catch {
            backupProblem = Self.noAnswerText
        }
    }
}
