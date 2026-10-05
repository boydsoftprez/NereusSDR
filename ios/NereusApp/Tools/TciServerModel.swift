// NereusSDR for iOS: the TCI server the Core runs for its station: its switch, port and options, and the apps connected to it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import NereusLink
import os

/// The TCI Server page's model (spec section 5.2 item 4, R-IOS-18): the
/// Core's own TCI server (the accessory control document, "The `stationTci`
/// object and the station TCI server"): its switch and port, turned on and
/// off and moved with `setStationTci` at `stationTciVersion` 1, whether it
/// listens and where, and at version 2 the apps connected to it (the
/// `tciClients` record stream), each of which can be closed with
/// `disconnectStationTciClient`, and the server's four options for apps
/// that connect (``StationTciOptions``, `setStationTciOptions`), as the
/// desktop's TCI Server page offers them. The Core refuses the options on
/// the air, so they grey then. The list is asked for only while the page
/// is open, as the desktop's clients applet does.
@MainActor
final class TciServerModel: ObservableObject {
    /// The agreed minor the station TCI server arrived in.
    static let minor: UInt16 = 11
    static let timeout: Duration = .seconds(10)

    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let noServerReason = "This Core does not run a TCI server. Updating the Core may help."
    static let noClientsReason = "This Core does not list the apps connected to its TCI server. Updating the Core may help."
    static let notRunningText = "The Core's TCI server is not running."
    static let noClientsText = "No apps are connected."
    static let noOptionsReason = "This Core does not let this app change its TCI server's options. Updating the Core may help."
    static let onAirReason = AccessoriesModel.onAirReason
    static let optionsNote = "The options apply to apps that connect from now on."

    /// One option as the page shows it: the desktop's name and a plain line.
    struct Option: Identifiable {
        let id: WritableKeyPath<StationTciOptions, Bool>
        let title: String
        let detail: String
        let identifier: String
    }

    /// The options in the desktop's order, with its names.
    static let optionRows: [Option] = [
        Option(id: \.emulateExpertSdr3, title: "Emulate ExpertSDR3 protocol",
               detail: "For apps made for ExpertSDR3", identifier: "tci.option.expertSdr3"),
        Option(id: \.emulateSunSdr2Pro, title: "Emulate SunSDR2 PRO device",
               detail: "Apps are told the radio is a SunSDR2 PRO", identifier: "tci.option.sunSdr2Pro"),
        Option(id: \.cwluBecomesCw, title: "CWL/CWU becomes CW",
               detail: "Apps see CW for both CW modes", identifier: "tci.option.cwluBecomesCw"),
        Option(id: \.sendInitialState, title: "Send initial state on connect",
               detail: "A new app is sent the radio's state", identifier: "tci.option.sendInitialState"),
    ]

    // MARK: State

    /// The Core's switch and port, and whether its server listens.
    @Published private(set) var enabled: Bool?
    @Published private(set) var port: Int64?
    @Published private(set) var listening = false
    /// The station network address it listens on; empty when only the Core's own computer.
    @Published private(set) var stationAddress = ""
    /// Why it could not listen, as the Core's computer said.
    @Published private(set) var error = ""
    /// The server's options as the page shows them: a change on its way, else the Core's.
    @Published private(set) var options: StationTciOptions?
    /// Why the options cannot change now; nil when they can.
    @Published private(set) var optionsReason: String?
    /// The apps connected, in the order the Core listed them.
    @Published private(set) var clients: [StationTciClient] = []
    /// Why the switch and port, or the list, cannot be used now; nil when they can.
    @Published private(set) var reason: String?
    @Published private(set) var clientsReason: String?
    /// Why a connected app cannot be disconnected now (the Core refuses it
    /// on the air); nil when it can.
    @Published private(set) var disconnectReason: String?
    /// A switch or port change is on its way to the Core.
    @Published private(set) var sending = false
    /// The Core's words for the last request it refused.
    @Published private(set) var note: String?
    /// The number pad open over the page, if any.
    @Published private(set) var pad: ValuePadModel?
    /// The app the operator asked to close, waiting for the confirmation.
    @Published var closing: StationTciClient?

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.tci")

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient?
    private var watch: ToolMirrorWatch?
    private var open = false
    /// This model asked for the list, so it stops asking when the page closes.
    private var wanting = false
    /// The options as the Core last sent them.
    private var mirrored: StationTciOptions?
    /// The options the operator chose, shown until the Core answers them or refuses.
    private let pending = PendingChoice<StationTciOptions>()
    /// A choice made while another was on its way, sent after it.
    private var queuedOptions: StationTciOptions?
    private var sendingOptions = false
    private let serverOutcomeOwner = ControlOutcomeOwner()

    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient?) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        if let records {
            watch.watch(records.$streams)
            watch.watch(records.$available)
            watch.watch(records.$refusals)
        }
        refresh()
    }

    /// The page opened or closed: the list is asked for only while it shows.
    func setOpen(_ shown: Bool) {
        open = shown
        refresh()
    }

    static let radioKey = "radio"
    static let txStateKey = "txState"

    private var version: Int64 {
        (mirror.agreedMinor ?? 0) >= Self.minor ? mirror.capabilityVersion(StationTciClient.capabilityName) : 0
    }

    func refresh() {
        let object = watch?.object(StationTciClient.objectKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<TciServerModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        let serves = connected && version >= 1 && object != nil
        set(\.enabled, serves ? ToolValue.flag(object?["enabled"]) : nil)
        set(\.port, serves ? ToolValue.whole(object?["port"]) : nil)
        set(\.listening, serves && ToolValue.flag(object?["listening"]) == true)
        set(\.stationAddress, serves ? ToolValue.text(object?["stationAddress"]) ?? "" : "")
        set(\.error, serves ? ToolValue.text(object?["error"]) ?? "" : "")
        set(\.reason, !connected ? Self.notConnectedReason : serves ? nil : Self.noServerReason)
        let offers = serves && version >= StationTciOptions.version
        mirrored = offers ? object.flatMap { StationTciOptions(values: $0.values) } : nil
        pending.follow(object) { values in StationTciOptions(values: values) }
        if !offers {
            pending.drop()
            queuedOptions = nil
        } else {
            pending.settle(mirrored: mirrored, sending: sendingOptions)
        }
        let radio = watch?.object(Self.radioKey)
        let state = watch?.object(Self.txStateKey)
        let onAir = ToolValue.flag(radio?["transmitting"]) == true || ToolValue.flag(state?["keyed"]) == true
            || ToolValue.flag(state?["tuning"]) == true || ToolValue.flag(state?["twoTone"]) == true
        set(\.options, offers ? (pending.value ?? mirrored) : nil)
        set(\.optionsReason, !connected ? Self.notConnectedReason
            : !serves ? Self.noServerReason
            : !offers || mirrored == nil ? Self.noOptionsReason
            : onAir ? Self.onAirReason : nil)
        let listsClients = serves && version >= StationTciClient.clientsVersion && records?.available == true
        let clientsWhy: String? = !connected ? Self.notConnectedReason
            : !serves ? Self.noServerReason
            : !listsClients ? Self.noClientsReason
            : records?.refusals[StationTciClient.streamName]
        set(\.clientsReason, clientsWhy)
        set(\.disconnectReason, clientsWhy == nil && onAir ? Self.onAirReason : nil)
        if listsClients && open {
            wanting = true
            records?.want(StationTciClient.streamName, backlog: StationTciClient.capacity, by: self)
        } else if wanting {
            wanting = false
            records?.unwant(StationTciClient.streamName, by: self)
        }
        let listed = listsClients ? (records?.records(StationTciClient.streamName) ?? [])
            .map(StationTciClient.init(record:)) : []
        set(\.clients, listed)
    }

    /// The server's state in a line.
    var stateText: String {
        guard let enabled else {
            return "--"
        }
        if !enabled {
            return "Off"
        }
        return listening ? "Listening on port \(port.map { "\($0)" } ?? "--")" : Self.notRunningText
    }

    // MARK: Changing

    func setEnabled(_ on: Bool) {
        guard reason == nil, let port, !sending else {
            return
        }
        Task { _ = await send(enabled: on, port: port) }
    }

    func openPortPad() {
        guard reason == nil, enabled != nil else { return }
        pad?.retire()
        pad = ValuePadModel(title: "TCI port", unit: "", range: StationTciClient.portRange, current: port,
                            sendWithLate: { [weak self] next, late in
                                // Use the Core's current switch at submission.
                                guard let self, self.reason == nil, let enabled = self.enabled else { return nil }
                                return await self.send(enabled: enabled, port: next, fromPad: true, onLateOutcome: late)
                            }, readCurrent: { [weak self] in
                                ToolValue.number(self?.mirror.object(StationTciClient.objectKey)?["port"])
                            }, close: { [weak self] in self?.pad = nil })
    }

    /// Turns one option on or off; the Core takes all four at once.
    func setOption(_ option: WritableKeyPath<StationTciOptions, Bool>, _ on: Bool) {
        guard optionsReason == nil, var next = options else {
            return
        }
        next[keyPath: option] = on
        pending.choose(next)
        set(options: next)
        if sendingOptions {
            queuedOptions = next
            return
        }
        sendOptions(next)
    }

    private func set(options next: StationTciOptions) {
        if options != next {
            options = next
        }
    }

    private func sendOptions(_ next: StationTciOptions) {
        guard let commands else {
            pending.drop()
            refresh()
            return
        }
        sendingOptions = true
        Task { [weak self] in
            var current: StationTciOptions? = next
            while let sending = current {
                var accepted = false
                do {
                    let result = try await commands.invoke(StationTciOptions.verb, arguments: sending.arguments,
                                                           timeout: Self.timeout)
                    accepted = result.accepted
                    self?.noteResult(result.accepted, result.reason)
                } catch {
                    Self.logger.info("A TCI option change had no answer from the Core")
                }
                guard let self else {
                    return
                }
                current = self.queuedOptions
                self.queuedOptions = nil
                if !accepted && current == nil {
                    // Refused or unanswered: show the Core's options again.
                    self.pending.drop()
                }
            }
            self?.sendingOptions = false
            self?.refresh()
        }
    }

    /// Asks the Core to close one app, after the operator confirmed it.
    func disconnect(_ client: StationTciClient) {
        closing = nil
        if let disconnectReason {
            // Went on the air while the confirmation was open.
            note = disconnectReason
            return
        }
        guard clientsReason == nil, let commands else {
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(StationTciClient.disconnectVerb,
                                                       arguments: [CommandArgument(name: "id", value: .text(client.id))],
                                                       timeout: Self.timeout)
                self?.noteResult(result.accepted, result.reason)
            } catch {
                Self.logger.info("A TCI app's disconnect had no answer from the Core")
            }
        }
    }

    private func send(enabled on: Bool, port next: Int64, fromPad: Bool = false,
                      onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome? {
        guard let commands else { return .notSent }
        let edit = serverOutcomeOwner.begin()
        let snapshot = mirror.snapshotIdentity
        let submitted = fromPad ? pad : nil
        let entry = submitted?.outcomeIdentity
        let owns: @MainActor () -> Bool = { [weak self, weak submitted] in
            guard let self, self.serverOutcomeOwner.isCurrent(edit), self.mirror.snapshotIdentity == snapshot else { return false }
            if fromPad { return submitted != nil && submitted === self.pad && submitted?.outcomeIdentity == entry }
            return true
        }
        let receive: @MainActor (Result<CommandResult, CommandError>) -> PropertyWriteOutcome = { [weak self] answer in
            let outcome = PropertyWriteOutcome(answer)
            guard let self, owns(), !outcome.answeredByCore || (self.mirror.isSnapshotComplete && !self.mirror.isStale) else {
                return PropertyWriteOutcome(accepted: outcome.accepted, reason: outcome.reason, value: nil,
                                            answeredByCore: outcome.answeredByCore, isCurrent: false)
            }
            if !outcome.heldForQuestion { self.note = outcome.noteText(refused: BandSlicesModel.refusedText) }
            return outcome
        }
        sending = true
        defer { sending = false }
        do {
            let result = try await commands.invokeHeld(StationTciClient.setVerb, arguments: [
                CommandArgument(name: "enabled", value: .bool(on)),
                CommandArgument(name: "port", value: .int(next)),
            ], timeout: MirrorStore.answerDeadline, onLateOutcome: { answer in
                await MainActor.run {
                    let outcome = receive(answer)
                    if outcome.isCurrent { onLateOutcome?(outcome) }
                }
            })
            return receive(.success(result))
        } catch let error as CommandError {
            return receive(.failure(error))
        } catch {
            return receive(.failure(.notSent))
        }
    }

    private func noteResult(_ accepted: Bool, _ reason: String) {
        if accepted {
            if note != nil {
                note = nil
            }
        } else {
            // A refusal with no words still says it was refused.
            note = reason.isEmpty ? BandSlicesModel.refusedText : reason
        }
    }
}
