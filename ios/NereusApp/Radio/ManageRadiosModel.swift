// NereusSDR for iOS: Manage Radios: the radios the Core can see, and choosing, scanning for and forgetting them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror
import NereusModels
import os

/// Manage Radios (spec section 5.2 item 5; the desktop's Radio > Manage
/// Radios and This Core's Change radio): the radios the Core can see, from
/// its `stationRadios` stream (link document section 7.7), asked for only
/// while the page is on screen (the Radio tab showing it, the app in front); Choose makes one the Core's
/// (`station.selectRadio`), Scan again looks for radios
/// (`station.rescanRadios`) and Forget removes one the Core does not run
/// (`station.forgetRadio`). With `radioModelsVersion` 1 the Core names each
/// radio's model and lists the models its board can run as; Change model
/// saves one for that radio (`station.setRadioModel`), offering only those.
/// The Core answers each; a refusal shows in its own words. A choice that reaches another device's slices is asked about
/// first, on this phone's question sheet. An accepted choice restarts the
/// Core's run on the new radio, and this app reconnects by itself.
@MainActor
final class ManageRadiosModel: ObservableObject {
    /// The radios, the Core's first.
    @Published private(set) var radios: [StationRadio] = []
    /// Why Choose, Scan again and Forget cannot run now; nil when they can.
    @Published private(set) var actionReason: String?
    /// Why the list is empty or missing: waiting for it, or the Core's refusal.
    @Published private(set) var listNote: String?
    /// Why the Core has no radio, in its words.
    @Published private(set) var waiting: String?
    /// The Core's refusal of the last request, in its words.
    @Published private(set) var refusal: String?
    /// What the last accepted request is doing now.
    @Published private(set) var note: String?
    /// A request is on its way.
    @Published private(set) var busy = false
    /// The Core names each radio's model and the models it can run as (`radioModelsVersion` 1).
    @Published private(set) var namesModels = false

    // MARK: Words

    static let notConnectedReason = RadioMenu.notConnectedReason
    static let olderCoreReason = RadioMenu.olderCoreRadioReason
    /// The Core's words for the radio on the air and a pairing-token sign-in (link 9.1).
    static let onAirReason = "The radio is on the air. Try again when it stops."
    static let pairedDeviceReason = "Change the Core's radio from a paired device."
    static let listWaitingReason = "The Core has not listed its radios."
    static let scanningNote = "The Core is looking for radios."
    static let forgottenNote = "The Core forgot that radio."
    static let unknownModelText = "Unknown model"
    static let olderModelsReason =
        "This Core does not say which models this radio can run as. Updating the Core may help."
    static let noModelsReason = "The Core has not listed the models this radio can run as."
    static let noAnswerReason = "The Core did not answer this request."
    static let notSentReason = "The request could not be sent to the Core."
    static let linkLostReason = "The connection to the Core was lost."

    static func switchingNote(_ name: String) -> String {
        "The Core is switching to \(name). This app reconnects by itself."
    }

    static func oneModelReason(_ model: String) -> String {
        "This radio runs only as \(model)."
    }

    static func modelSavedNote(radio: String, model: String) -> String {
        "The Core saved \(model) for \(radio). It applies the next time the Core connects to that radio."
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "radio.manage")

    private let mirror: MirrorStore
    private let commands: CommandClient
    private let records: RecordStreamClient
    private let catalogFeed: CatalogFeed
    private let signedInWithDeviceKey: () -> Bool
    private let timeout: Duration
    private var watch: ToolMirrorWatch?
    /// The page is on screen.
    private(set) var isOpen = false

    init(mirror: MirrorStore, commands: CommandClient, records: RecordStreamClient, catalogFeed: CatalogFeed,
         signedInWithDeviceKey: @escaping () -> Bool, timeout: Duration = .seconds(10)) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        self.catalogFeed = catalogFeed
        self.signedInWithDeviceKey = signedInWithDeviceKey
        self.timeout = timeout
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        watch.watch(records.$streams)
        watch.watch(records.$refusals)
        watch.watch(records.$available)
        watch.watch(catalogFeed.$catalog)
        self.watch = watch
        refresh()
    }

    convenience init(app: AppModel) {
        self.init(mirror: app.mirror, commands: app.commands, records: app.records, catalogFeed: app.main.catalogFeed,
                  signedInWithDeviceKey: { [weak app] in app?.session?.signsWithDeviceKey ?? false })
    }

    // MARK: The page

    /// The page came on screen: ask the Core for its radios, now and after
    /// every reconnect. The list is asked for once however many ask for it.
    func open() {
        guard !isOpen else { return }
        isOpen = true
        records.want(StationRadio.streamName, backlog: StationRadio.capacity, by: self)
        refresh()
    }

    /// The page left the screen: this page stops asking; the list runs on
    /// while anything else still asks for it.
    func close() {
        guard isOpen else { return }
        isOpen = false
        records.unwant(StationRadio.streamName, by: self)
        refresh()
    }

    /// The model the Core runs `radio` as, by name: the Core's own name
    /// for it (`radioModelsVersion` 1), else, for the radio it runs, the
    /// catalogue's; nil when neither names it.
    func modelName(_ radio: StationRadio) -> String? {
        if namesModels, !radio.modelLabel.isEmpty {
            return radio.modelLabel
        }
        guard radio.inUse, let board = catalogFeed.catalog?.board, let model = radio.model,
              Int64(board.model) == model, !board.productLabel.isEmpty else {
            return nil
        }
        return board.productLabel
    }

    /// The model as the page shows it: its name, else "Unknown model".
    func modelText(_ radio: StationRadio) -> String {
        modelName(radio) ?? Self.unknownModelText
    }

    /// The models `radio` can run as, in the Core's order: exactly what
    /// Change model offers. Empty when the Core does not list them.
    func modelChoices(_ radio: StationRadio) -> [StationRadio.ModelChoice] {
        namesModels ? (radio.models ?? []) : []
    }

    /// Why Change model cannot run for `radio`; nil when it can.
    func changeModelReason(_ radio: StationRadio) -> String? {
        if let actionReason {
            return actionReason
        }
        guard namesModels else {
            return Self.olderModelsReason
        }
        let choices = modelChoices(radio)
        if choices.isEmpty {
            return Self.noModelsReason
        }
        if choices.count == 1 {
            return Self.oneModelReason(choices[0].label)
        }
        return nil
    }

    func clearRefusal() {
        refusal = nil
    }

    // MARK: Asking the Core

    func choose(_ radio: StationRadio) {
        send(StationRadio.selectVerb, mac: radio.mac) { [weak self] result in
            guard let self else { return }
            if result.accepted {
                self.note = radio.inUse ? nil : Self.switchingNote(radio.name.isEmpty ? radio.mac : radio.name)
            } else if Self.waitsForYou(result) {
                // This phone's question sheet asks; the Core acts once it is
                // answered. Not a note or a refusal, which would stay after
                // the question closes (the desktop, StationClient.cpp:7308-7317).
                return
            } else {
                self.refusal = Self.words(result)
            }
        }
    }

    /// Saves `choice` as the model the Core runs `radio` as, from its next
    /// connection to it. Only a model the Core lists for the radio is sent.
    func setModel(_ radio: StationRadio, _ choice: StationRadio.ModelChoice) {
        guard changeModelReason(radio) == nil, modelChoices(radio).contains(choice) else { return }
        let name = radio.name.isEmpty ? radio.mac : radio.name
        send(StationRadio.setModelVerb, mac: radio.mac,
             extra: [CommandArgument(name: "model", value: .int(choice.model))]) { [weak self] result in
            guard let self else { return }
            if result.accepted {
                self.note = Self.modelSavedNote(radio: name, model: choice.label)
            } else {
                self.refusal = Self.words(result)
            }
        }
    }

    func rescan() {
        send(StationRadio.rescanVerb, mac: nil) { [weak self] result in
            guard let self else { return }
            if result.accepted {
                self.note = Self.scanningNote
            } else {
                self.refusal = Self.words(result)
            }
        }
    }

    func forget(_ radio: StationRadio) {
        send(StationRadio.forgetVerb, mac: radio.mac) { [weak self] result in
            guard let self else { return }
            if result.accepted {
                self.note = Self.forgottenNote
            } else {
                self.refusal = Self.words(result)
            }
        }
    }

    // MARK: Inside

    func refresh() {
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        let chooses = mirror.capabilityVersion(StationRadio.capabilityName) >= 1
            && mirror.capabilityVersion(RecordStreamClient.capabilityName) >= 1
        var transmitting = false
        var waitingWords: String?
        if let radio = watch?.object("radio") {
            if case .bool(let on)? = radio["transmitting"] {
                transmitting = on
            }
            if case .text(let words)? = radio[StationRadio.waitingProperty], !words.isEmpty {
                waitingWords = words
            }
        }
        let reason: String?
        if !connected {
            reason = Self.notConnectedReason
        } else if !chooses {
            reason = Self.olderCoreReason
        } else if transmitting {
            reason = Self.onAirReason
        } else if !signedInWithDeviceKey() {
            reason = Self.pairedDeviceReason
        } else {
            reason = nil
        }
        let next = records.records(StationRadio.streamName).map(StationRadio.init(record:))
        let listNote: String?
        if !connected {
            listNote = Self.notConnectedReason
        } else if !chooses {
            listNote = Self.olderCoreReason
        } else if let refused = records.refusals[StationRadio.streamName], !refused.isEmpty {
            listNote = refused
        } else if records.streams[StationRadio.streamName] == nil {
            listNote = Self.listWaitingReason
        } else {
            listNote = nil
        }
        set(\.radios, next)
        set(\.namesModels, connected && chooses
            && mirror.capabilityVersion(StationRadio.modelsCapabilityName) >= 1)
        set(\.actionReason, reason)
        set(\.listNote, listNote)
        set(\.waiting, connected ? waitingWords : nil)
        if !connected, note != nil {
            note = nil
        }
    }

    private func send(_ verb: String, mac: String?, extra: [CommandArgument] = [],
                      answered: @escaping (CommandResult) -> Void) {
        guard actionReason == nil, !busy else { return }
        busy = true
        refusal = nil
        note = nil
        let arguments = (mac.map { [CommandArgument(name: "mac", value: .text($0))] } ?? []) + extra
        let commands = commands
        let timeout = timeout
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: timeout)
                guard let self else { return }
                self.busy = false
                answered(result)
            } catch {
                Self.logger.info("\(verb, privacy: .public) had no answer from the Core: \(String(describing: error), privacy: .public)")
                guard let self else { return }
                self.busy = false
                switch error as? CommandError {
                case .notSent: self.refusal = Self.notSentReason
                case .linkLost: self.refusal = Self.linkLostReason
                case .timedOut, nil: self.refusal = Self.noAnswerReason
                }
            }
        }
    }

    private static func waitsForYou(_ result: CommandResult) -> Bool {
        if case .text("needsConfirmation")? = result.values["phase"] {
            return true
        }
        return result.reason == SeveralDevices.waitingReason
    }

    private static func words(_ result: CommandResult) -> String {
        result.reason.isEmpty ? "The Core refused this request." : result.reason
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<ManageRadiosModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
