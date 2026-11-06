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

    /// Where a block is drawn. An iPad can show one pan's stack twice, in the
    /// Pan sheet or BAND menu and in its applet, on this one model, so a
    /// transient state remembers which it was started from: closing a sheet
    /// ends only the sheet's, and only that block types a name.
    enum Surface {
        case sheet, applet
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
    /// The band whose list this device is reading, when it is not the active
    /// slice's: this device's own, and never sent. Set from "40m \u{25BE}" or a
    /// hold on a band button; it ends when the slice gets there.
    @Published private(set) var viewedBand: String?
    /// The active slice's band key, nil with no active slice or a band the
    /// Core has not said.
    @Published private(set) var currentBand: String?
    /// There is an active slice for a verb to act on.
    @Published private(set) var hasActiveSlice = false
    /// The pan's active slice, for the applet's letters.
    @Published private(set) var activeSliceId: Int?
    /// This pan's slices and the bands they are on, for a row's letters.
    @Published private(set) var slicesOnBands: [SliceOnBand] = []

    /// Reorder's working order, which nothing sends until Done; nil out of the
    /// mode. It is held here because the sheets redraw the block when it
    /// starts to scroll, and the mode outlives that.
    @Published private(set) var reorder: Reorder?

    /// The entry whose name is being typed in its row, and what is typed. Held
    /// here for the same reason as ``reorder``.
    @Published private(set) var renaming: String?
    @Published var renameDraft = ""
    /// The block whose row has the name field.
    @Published private(set) var renamingSurface = Surface.sheet
    /// The block that picked ``viewedBand``.
    private var viewedSurface = Surface.sheet
    /// The block that started ``reorder``.
    private var reorderSurface = Surface.sheet

    struct Reorder: Equatable {
        let band: String
        var ids: [String]
    }

    /// A slice of this pan's and where it is.
    struct SliceOnBand: Equatable {
        let id: Int
        let letter: String
        let colour: String
        let bandKey: String?
    }

    /// How long a hold on a band button takes to open its list.
    static let holdSeconds = 0.5

    /// The list on show: the one picked, else the active slice's band.
    var shownBand: String? {
        viewedBand ?? currentBand
    }

    /// The list on show is not the slice's own, so the arrows and add wait.
    var isViewingAnotherBand: Bool {
        viewedBand != nil
    }

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
    private var sliceReadQueued = false
    private var lastActiveSliceId: Int?
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
        // A slice's reading moves often and a document is costly to read, so
        // these read only the slices, once per turn of the main queue.
        slices.$entries.dropFirst().sink { [weak self] _ in
            self?.queueSliceRead()
        }.store(in: &watches)
        slices.$activeSliceId.dropFirst().sink { [weak self] _ in
            self?.queueSliceRead()
        }.store(in: &watches)
        band?.$catalog.dropFirst().sink { [weak self] _ in
            self?.objectWillChange.send()
            self?.queueSliceRead()
        }.store(in: &watches)
        band?.$stationPlanName.dropFirst().sink { [weak self] _ in
            self?.objectWillChange.send()
        }.store(in: &watches)
        readSlices()
        refresh()
    }

    // MARK: What the block shows

    /// The Core's key for a band button's label: the grid's "40" is the
    /// stack's "40m", and "WWV" is itself
    /// (src/models/BandGrid.h:56-72, its two label columns).
    static func bandKey(label: String) -> String {
        !label.isEmpty && label.allSatisfy(\.isNumber) ? label + "m" : label
    }

    /// A band number outside the grid that a slice can still stand on.
    // From src/models/Band.h:119-133 (enum class Band: GEN 11, XVTR 13)
    private static let offGridBandKeys = [11: "GEN", 13: "XVTR"]

    /// The Core's key for a slice's band number, nil for one this phone cannot name.
    static func bandKey(id: Int?, bands: [(id: Int, label: String)]) -> String? {
        guard let id else {
            return nil
        }
        return bands.first { $0.id == id }.map { bandKey(label: $0.label) } ?? offGridBandKeys[id]
    }

    /// The bands "40m \u{25BE}" lists: the Core's grid in its order, then any
    /// other band the stack or the slice is on.
    var pickerBands: [String] {
        var keys = band?.catalog?.bands?.map { Self.bandKey(label: $0.label) } ?? []
        for key in (stack?.entries.map(\.band) ?? []) + [currentBand].compactMap({ $0 }) where !keys.contains(key) {
            keys.append(key)
        }
        return keys
    }

    /// The active slice's place in a band's list: "6 of 6", with a dash where
    /// it has none, or how many are saved on a band it is not on.
    func countText(band key: String) -> String {
        let entries = stack?.entries(band: key) ?? []
        guard key == currentBand else {
            return "\(entries.count) saved"
        }
        if let id = activeSlicePlace(band: key), let index = entries.firstIndex(where: { $0.id == id }) {
            return "\(index + 1) of \(entries.count)"
        }
        return "- of \(entries.count)"
    }

    /// The entry the active slice stands on in a band.
    func activeSlicePlace(band key: String) -> String? {
        guard let id = slices.activeSliceId else {
            return nil
        }
        return stack?.place(slice: BandSlice.letter(forIndex: id), band: key)
    }

    /// The letters of this pan's other slices standing on an entry.
    func otherSlices(on entry: BandStack.Entry) -> [SliceOnBand] {
        slicesOnBands.filter { slice in
            slice.id != slices.activeSliceId && slice.bandKey == entry.band
                && stack?.place(slice: slice.letter, band: entry.band) == entry.id
        }
    }

    /// The colour of the band plan's segment at a frequency: the same plan the
    /// band's strip draws (``BandModel/shownPlan``).
    func tintColour(hz: Int64) -> String? {
        band?.shownPlan?.segments.first { $0.lowHz <= Double(hz) && Double(hz) <= $0.highHz }?.colour
    }

    /// Shows a band's list, the slice's own being no view at all.
    func showBand(_ key: String?, from surface: Surface = .sheet) {
        viewedBand = key == currentBand ? nil : key
        viewedSurface = surface
        renaming = nil
        endReorderIfElsewhere()
    }

    /// The sheet or menu showing the block has closed. What the applet
    /// started stays, since the applet is still there.
    func endViewing(on surface: Surface = .sheet) {
        if viewedSurface == surface {
            viewedBand = nil
        }
        if reorderSurface == surface {
            reorder = nil
        }
        if renamingSurface == surface {
            renaming = nil
        }
    }

    /// Rename: the row takes the entry's name to edit. A name still being
    /// typed in another row is saved first.
    func startRename(_ entry: BandStack.Entry, from surface: Surface = .sheet) {
        finishRename()
        renameDraft = entry.name
        renamingSurface = surface
        renaming = entry.id
    }

    /// The name field for an entry belongs in `surface`'s row.
    func isRenaming(_ entryId: String, on surface: Surface) -> Bool {
        renaming == entryId && renamingSurface == surface
    }

    /// Ends Rename, sending the name only when it has changed. An empty name clears it.
    func finishRename() {
        guard let id = renaming else {
            return
        }
        renaming = nil
        let name = renameDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        if stack?.entries.first(where: { $0.id == id })?.name != name {
            edit(id, name: name, locked: nil)
        }
    }

    /// Reorder's button: into the mode for the list on show, or Done, which
    /// sends every id.
    func toggleReorder(from surface: Surface = .sheet) {
        if let reorder {
            self.reorder = nil
            self.reorder(bandKey: reorder.band, ids: reorderIds(working: reorder.ids, band: reorder.band))
        } else if let band = shownBand {
            reorderSurface = surface
            reorder = Reorder(band: band, ids: stack?.entries(band: band).map(\.id) ?? [])
        }
    }

    /// Moves a row in Reorder's working order, within the list's ends.
    func moveInReorder(_ id: String, by places: Int) {
        guard var ids = reorder?.ids, let from = ids.firstIndex(of: id) else {
            return
        }
        ids.remove(at: from)
        ids.insert(id, at: max(0, min(ids.count, from + places)))
        reorder?.ids = ids
    }

    /// The list on show changed or went: the mode ends and nothing is sent.
    private func endReorderIfElsewhere() {
        if let reorder, reorder.band != shownBand || availability != .ready {
            self.reorder = nil
        }
    }

    /// What Done sends: the order made here, without what has gone since and
    /// with what has come since at the foot, since the Core takes a band's
    /// whole list and only that.
    func reorderIds(working: [String], band key: String) -> [String] {
        let current = stack?.entries(band: key).map(\.id) ?? []
        return working.filter(current.contains) + current.filter { !working.contains($0) }
    }

    private func queueSliceRead() {
        guard !sliceReadQueued else {
            return
        }
        sliceReadQueued = true
        Task { @MainActor [weak self] in
            self?.sliceReadQueued = false
            self?.readSlices()
        }
    }

    private func readSlices() {
        let bands = band?.catalog?.bands?.map { (id: $0.id, label: $0.label) } ?? []
        let activeId = slices.activeSliceId
        if activeId != lastActiveSliceId {
            lastActiveSliceId = activeId
            viewedBand = nil
        }
        let on = slices.entries.map { entry in
            SliceOnBand(id: entry.id, letter: BandSlice.letter(forIndex: entry.id), colour: entry.slice.colour,
                        bandKey: Self.bandKey(id: entry.band, bands: bands))
        }
        if on != slicesOnBands {
            slicesOnBands = on
        }
        let current = activeId == nil ? nil : on.first { $0.id == activeId }?.bandKey
        if current != currentBand {
            currentBand = current
        }
        if hasActiveSlice != (activeId != nil) {
            hasActiveSlice = activeId != nil
        }
        if activeSliceId != activeId {
            activeSliceId = activeId
        }
        // The slice has got to the list being read.
        if viewedBand != nil, viewedBand == currentBand {
            viewedBand = nil
        }
        endReorderIfElsewhere()
    }

    /// Makes one of this pan's slices the one the stack acts on (the applet's letters).
    func activate(sliceId: Int) {
        slices.activate(sliceId)
    }

    // MARK: The verbs

    /// A band button: on another band the Core saves and leaves, and on this
    /// band it steps.
    func press(bandKey: String, _ direction: Direction) {
        viewedBand = nil
        endReorderIfElsewhere()
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
        endReorderIfElsewhere()
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
