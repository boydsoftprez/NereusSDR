// NereusSDR for iOS: one pan's band stack: the Core's document as this phone reads it, and the verbs that change it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import os

/// One pan's band stack. The stack is the Core's: seeds, cycling, wrap and
/// duplicates all happen there, and this model only reads the mirrored
/// document (``stack``), sends the `bandStack.*` verbs and shows the Core's
/// answer (``note``). A verb on a slice acts on the pan's active slice and
/// carries the pan's view, which the Core saves with the spot.
///
/// A press the Core holds for a question sets no note: the Core's
/// `confirm.request` opens the sheet (``ConfirmationLayer``), and its Cancel
/// changes nothing here.
@MainActor
final class BandStackModel: ObservableObject {
    enum Availability: Equatable {
        case ready, needsNewerCore, notConnected
    }

    enum Direction: String {
        case next, previous
    }

    /// Why the stack is greyed on a Core without band stacks.
    static let needsNewerCoreText = "This Core does not have band stacks. Updating the Core may help."
    /// Why the stack is greyed with no radio to read it from.
    static let notConnectedText = "Connect to a radio to use band stacks."
    /// The agreed minor the `bandStack.*` verbs need (`surface.json`).
    static let minimumMinor: UInt16 = 11
    static let commandTimeout: Duration = .seconds(5)

    @Published private(set) var availability: Availability = .notConnected
    @Published private(set) var stack: BandStack?
    /// The Core's words for what it last refused, as sent, until closed or
    /// until the Core answers the next verb.
    @Published var note: String?

    /// Why the stack is greyed; nil while it is ready.
    var unavailableReason: String? {
        switch availability {
        case .ready:
            return nil
        case .needsNewerCore:
            return Self.needsNewerCoreText
        case .notConnected:
            return Self.notConnectedText
        }
    }

    private let store: MirrorStore
    private let slices: BandSlicesModel
    private let commands: CommandClient?
    /// The pan whose view a verb carries and a recall moves; nil has no view.
    private let band: BandModel?
    private var watches: [AnyCancellable] = []
    private var documentWatch: AnyCancellable?
    private var watchedDocument: MirrorObject?
    /// The last `lastRecall.seq` read; nil until a document says what has
    /// already happened, which is not news.
    private var seenRecallSeq: Int?
    /// The newest verb sent: an older one's answer does not replace its note.
    private var touches: UInt64 = 0
    private static let logger = Logger(subsystem: "NereusSDR", category: "band.stack")

    init(store: MirrorStore, slices: BandSlicesModel, commands: CommandClient?, band: BandModel? = nil) {
        self.store = store
        self.slices = slices
        self.commands = commands
        self.band = band
        // Each new session's first document is not news either.
        store.$isSnapshotComplete.dropFirst().sink { [weak self] complete in
            if !complete {
                MainActor.assumeIsolated {
                    self?.seenRecallSeq = nil
                }
            }
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        store.$isStale.dropFirst().sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        store.$capabilities.dropFirst().sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        store.$objectKeys.dropFirst().sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        refresh()
    }

    // MARK: The verbs

    /// A band button: on another band the Core saves and leaves, and on this
    /// band it steps.
    func press(bandKey: String, _ direction: Direction) {
        sendOnSlice("bandStack.press", [text("band", bandKey), text("direction", direction.rawValue)])
    }

    func step(_ direction: Direction) {
        sendOnSlice("bandStack.step", [text("direction", direction.rawValue)])
    }

    func goTo(_ entryId: String) {
        sendOnSlice("bandStack.goTo", [text("entryId", entryId)])
    }

    func add() {
        sendOnSlice("bandStack.add", [])
    }

    func saveHere(_ entryId: String) {
        sendOnSlice("bandStack.saveHere", [text("entryId", entryId)])
    }

    func edit(_ entryId: String, name: String?, locked: Bool?) {
        var arguments = [text("entryId", entryId)]
        if let name {
            arguments.append(text("name", name))
        }
        if let locked {
            arguments.append(CommandArgument(name: "locked", value: .bool(locked)))
        }
        send("bandStack.edit", arguments)
    }

    func delete(_ entryId: String) {
        send("bandStack.delete", [text("entryId", entryId)])
    }

    func reorder(bandKey: String, ids: [String]) {
        guard let list = Self.idList(ids) else {
            return
        }
        send("bandStack.reorder", [text("band", bandKey), text("ids", list)])
    }

    func setReturn(bandKey: String, mode: BandStack.ReturnMode, entryId: String?) {
        var arguments = [text("band", bandKey), text("mode", mode.rawValue)]
        if let entryId, !entryId.isEmpty {
            arguments.append(text("entryId", entryId))
        }
        send("bandStack.setReturn", arguments)
    }

    /// Removes the unlocked entries of one band, or of every band.
    func clear(bandKey: String?) {
        send("bandStack.clear", bandKey.map { [text("band", $0)] } ?? [])
    }

    func reseed() {
        send("bandStack.reseed", [])
    }

    /// Nothing is sent when no option is given.
    func options(dupeProtection: Bool?, expiryMinutes: Int?, dwellSeconds: Int?) {
        var arguments: [CommandArgument] = []
        if let dupeProtection {
            arguments.append(CommandArgument(name: "dupeProtection", value: .bool(dupeProtection)))
        }
        if let expiryMinutes {
            arguments.append(CommandArgument(name: "expiryMinutes", value: .int(Int64(expiryMinutes))))
        }
        if let dwellSeconds {
            arguments.append(CommandArgument(name: "dwellSeconds", value: .int(Int64(dwellSeconds))))
        }
        guard !arguments.isEmpty else {
            return
        }
        send("bandStack.options", arguments)
    }

    func restoreExpired(batchId: String) {
        send("bandStack.restoreExpired", [text("batchId", batchId)])
    }

    /// `ids` goes as the text of a JSON array of strings, since the link has
    /// no list kind, as `bandStackIdList` reads it
    /// (src/core/session/SessionCommandDispatcher.cpp:4743).
    static func idList(_ ids: [String]) -> String? {
        let encoder = JSONEncoder()
        encoder.outputFormatting = .withoutEscapingSlashes
        return (try? encoder.encode(ids)).flatMap { String(data: $0, encoding: .utf8) }
    }

    // MARK: Following a recall

    /// Where a recall leaves the pan's view, or nil when there is no view to
    /// ask for. This is the phone's whole reading of spec V14: the zoom comes
    /// back only on a pan the slice has to itself, and there, with click
    /// tune, the saved center too, unless that puts the frequency off screen.
    /// Everything else centers on the frequency at the span it has.
    static func recalledView(_ recall: BandStack.LastRecall, frequencyHz: Double, current: TuneGestures.View,
                             alone: Bool, clickTune: Bool, spanLimits: ClosedRange<Double>) -> TuneGestures.View? {
        var span = current.spanHz
        if alone, recall.spanHz > 0 {
            span = min(max(Double(recall.spanHz), spanLimits.lowerBound), spanLimits.upperBound)
        }
        guard span > 0, frequencyHz > 0 else {
            return nil
        }
        let saved = Double(recall.centerHz)
        if alone, clickTune, saved > 0, abs(frequencyHz - saved) <= span / 2 {
            return TuneGestures.View(centerHz: saved, spanHz: span)
        }
        return TuneGestures.View(centerHz: frequencyHz, spanHz: span)
    }

    /// Applies a new `lastRecall` to this pan, once per `seq`. A pan's view
    /// belongs to each device, so only the one controlling the slice follows,
    /// and only on the band that shows it.
    private func follow(_ recall: BandStack.LastRecall?) {
        guard let recall else {
            seenRecallSeq = nil
            return
        }
        let isNew = seenRecallSeq.map { $0 != recall.seq } ?? false
        seenRecallSeq = recall.seq
        guard isNew, recall.applyView, let band, !band.keyedView else {
            return
        }
        slices.catchUp()
        guard let entry = slices.entries.first(where: { $0.id == recall.sliceId }), !entry.listening else {
            return
        }
        if let shows = slices.shownEntry ?? slices.homeEntry, !BandSlicesModel.sameReceiver(entry, shows) {
            return
        }
        let alone = !slices.entries.contains { other in
            other.id != entry.id && !other.listening && BandSlicesModel.sameReceiver(other, entry)
        }
        if let view = Self.recalledView(recall, frequencyHz: entry.slice.frequencyHz, current: band.view, alone: alone,
                                        clickTune: band.settings.ctun,
                                        spanLimits: band.spanLimits(sampleRateHz: entry.sampleRateHz)) {
            band.requestView(view)
        }
    }

    // MARK: Inside

    private func text(_ name: String, _ value: String) -> CommandArgument {
        CommandArgument(name: name, value: .text(value))
    }

    /// The pan's view as the Core saves it. It is left out while the band
    /// shows the transmit view, or none yet, and the entry keeps its own.
    private var viewArguments: [CommandArgument] {
        guard let band, !band.keyedView else {
            return []
        }
        let view = band.view
        guard view.centerHz.isFinite, view.spanHz.isFinite, view.centerHz > 0, view.spanHz > 0 else {
            return []
        }
        return [CommandArgument(name: "viewCenterHz", value: .int(Int64(view.centerHz.rounded()))),
                CommandArgument(name: "viewSpanHz", value: .int(Int64(view.spanHz.rounded())))]
    }

    /// A verb on the pan's active slice. The Core decides who may change it,
    /// and with no active slice there is nothing to act on.
    private func sendOnSlice(_ verb: String, _ arguments: [CommandArgument]) {
        refresh()
        guard availability == .ready else {
            note = unavailableReason
            return
        }
        guard let sliceId = slices.activeSliceId else {
            return
        }
        send(verb, [CommandArgument(name: "sliceId", value: .int(Int64(sliceId)))] + arguments + viewArguments)
    }

    private func send(_ verb: String, _ arguments: [CommandArgument]) {
        refresh()
        guard availability == .ready, let commands else {
            note = unavailableReason
            return
        }
        touches &+= 1
        let touch = touches
        Task { @MainActor [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: Self.commandTimeout)
                self?.answered(result, touch: touch, verb: verb)
            } catch {
                Self.logger.info("\(verb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
                if self?.touches == touch {
                    self?.note = SeveralDevicesClient.noAnswerText
                }
            }
        }
    }

    private func answered(_ result: CommandResult, touch: UInt64, verb: String) {
        guard touches == touch else {
            return
        }
        if result.accepted || SeveralDevices.waitsForConfirmation(result) {
            note = nil
        } else {
            Self.logger.info("The Core refused \(verb, privacy: .public): \(result.reason, privacy: .private)")
            note = result.reason.isEmpty ? BandSlicesModel.refusedText : result.reason
        }
    }

    private func refresh() {
        watchDocument()
        let next = store.bandStack
        if next != stack {
            stack = next
        }
        let state = readAvailability(next)
        if state != availability {
            availability = state
        }
        follow(next?.lastRecall)
    }

    private func readAvailability(_ document: BandStack?) -> Availability {
        guard commands != nil, store.isSnapshotComplete, !store.isStale else {
            return .notConnected
        }
        guard (store.agreedMinor ?? 0) >= Self.minimumMinor, store.capabilityVersion(BandStack.capability) >= 1 else {
            return .needsNewerCore
        }
        // The Core has band stacks, and no radio yet.
        return document == nil ? .notConnected : .ready
    }

    private func watchDocument() {
        let object = store.object(BandStack.objectKey)
        guard object !== watchedDocument else {
            return
        }
        watchedDocument = object
        documentWatch = object?.$values.dropFirst().sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }
}
