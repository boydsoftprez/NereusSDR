// NereusSDR for iOS: this device's slices on one band, read from the mirror, and the touches that tune them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
import os

/// The slices one band shows (R-IOS-11, R-IOS-12): each `SliceModel` the
/// Core mirrors on this pan, with what its flag reads, and which one is
/// active. It also carries the band's touches to the Core: a tap writes the
/// active slice's frequency, a drag on a flag or its passband that slice's
/// (at most every 50 ms, then its final value), a typed frequency one
/// write whose answer the pad shows (D74); frequency writes go one at a
/// time so they stay in order. A tap on another slice's flag makes it
/// active, the flag's step menu writes the slice's step, and a pan past
/// the receiver's window asks the Core to move it, as the desktop's remote
/// window does. Nothing changes here until the Core says so, except the
/// frequency shown under a finger that is still dragging. When the Core
/// refuses one of these writes, its words show over the band as it sent
/// them (``refusal``), and the slice shows the Core's value again.
@MainActor
final class BandSlicesModel: ObservableObject {
    /// One slice and what its flag reads.
    struct Entry: Equatable, Identifiable {
        var slice: BandSlice
        var rxAntenna: String
        var txAntenna: String
        /// The catalogue's label for the slice's mode, `LSB`.
        var modeLabel: String
        /// The Core pan that owns this slice.
        var panKey: String?
        var signalDbm: Double?
        /// The receiver's peak reading (`signalPeakDbm`), which the iPad's
        /// analog S-meter shows for Signal and Signal Peak.
        var signalPeakDbm: Double?
        /// The receiver's average reading (`signalAverageDbm`), the S-meter's Sig Avg.
        var signalAverageDbm: Double?
        var stepHz: Double?
        var sampleRateHz: Double?
        var locked: Bool
        var muted: Bool
        /// The receiver's window stays where it is while the slice tunes
        /// inside it (`streamCtunPinned`).
        var receiverPinned: Bool = false
        /// The Core's band number for the slice's frequency (`band`), the
        /// catalogue's band grid's id; nil while the Core says none.
        var band: Int?
        /// The slice's mode (`dspMode`, the catalogue's `modes` id); nil while the Core says none.
        var mode: Int? = nil
        /// What the flag's RADE row reads while the slice is in RADE-U or
        /// RADE-L (spec section 5.1 item 4); nil in every other mode.
        var rade: RadeReception? = nil
        /// Who controls the slice (R-IOS-42): this phone, or another device
        /// or nobody while this phone listens to it.
        var control: Control = .here
        /// The slice's `access:<id>`, from a Core that shares slices; nil otherwise.
        var access: SliceAccess.State? = nil
        /// Why Take control on this listened slice is greyed, in the Core's
        /// words (``SliceAccess/takeRefusal(_:version:devices:)``); nil
        /// when it can be pressed.
        var takeRefusal: String? = nil
        /// The receiver that hears the slice (`streamIndex`), from 0; -1 while the Core says none.
        var streamIndex: Int = -1
        /// The slice's own AF gain at the Core (`afGain`, 0 to 100): where
        /// this phone's own volume starts when it listens in.
        var afGain: Double? = nil

        var id: Int { slice.id }

        /// The slice's short owner words for its flag's owner row,
        /// "Shack desktop controls A"; nil for a slice this phone controls.
        var ownerWords: String? {
            listening ? ownerWordsText : nil
        }
        /// The owner words as the model worded them with the devices it knows.
        var ownerWordsText: String? = nil
        /// The negotiated radio summary names this exact live slice, including paused/listened states.
        var diversityOn: Bool = false

        /// The Core's full line for who controls it, for a tap on a dimmed control or the band.
        var ownerLine: String? {
            if case .listening(let line) = control {
                return line
            }
            return nil
        }

        /// This phone only listens to the slice: another device or nobody controls it.
        var listening: Bool { control != .here }
    }

    /// Who controls a slice this band shows (R-IOS-42, D115).
    enum Control: Equatable {
        /// This phone: every slice from a Core that does not share slices.
        case here
        /// Another device, or nobody; the Core's line for it, "Slice B is
        /// controlled by Shack desktop. Take control to change it."
        case listening(ownerLine: String)
    }

    /// The mirrored class and property names this model reads.
    static let sliceClass = "SliceModel"
    static let activateVerb = "setActiveSliceById"
    static let removeVerb = "removeSlice"
    /// The sample rate of the receiver that hears a slice.
    static let sampleRateVerb = "requestSliceSampleRate"
    /// The verbs the desktop's remote window moves a receiver's window
    /// with, and the capability and agreed minor they need (link document
    /// section 9).
    static let pinVerb = "requestStreamCtunPinned"
    static let centreVerb = "requestStreamCentre"
    static let receiverWindowCapability = "remoteCtunVersion"
    static let receiverWindowMinor: UInt16 = 2
    /// The capability and agreed minor under which a Core sends each RADE
    /// slice's `radeSynced` and `radeFreqOffsetHz` (the Core's RADE note,
    /// rade-flag-for-phone.md); an older Core leaves the row greyed.
    static let radeStatusCapability = "radeStatusVersion"
    static let radeStatusMinor: UInt16 = 11
    /// The capability under which a Core sends each RADE slice's
    /// `radeReason` (link document section 6.3, "The RADE reason"), to a
    /// phone that declared `radeReason` 1.
    static let radeReasonCapability = "radeReasonVersion"
    static let commandTimeout: Duration = .seconds(5)
    /// The verb that makes one of this phone's slices its transmit slice,
    /// and the capability and agreed minor it needs (link section 18).
    static let setTxSliceVerb = "tx.setTxSlice"
    static let remoteTxCapability = "remoteTxVersion"
    static let remoteTxMinor: UInt16 = 11
    /// The Core's code on a key refused until the taken slice's TX button
    /// is pressed (link section 18, `chooseTransmitSlice`).
    static let chooseTransmitSliceCode = "chooseTransmitSlice"

    @Published private(set) var entries: [Entry] = []
    @Published private(set) var activeSliceId: Int?

    // MARK: Showing another slice's band (R-IOS-42, JJ's rulings of 2026-09-30)

    /// A slice this phone made active on this phone alone: one it listens
    /// to, or one it took while showing it. The controls follow it; the
    /// Core's own `active` flag stays with this phone's own slice. Nil
    /// while this phone's own active slice is the active one.
    @Published private(set) var shownSliceId: Int?
    /// The phone's own slice whose band it left for `shownSliceId`: Back to
    /// your band makes it active again.
    @Published private(set) var returnSliceId: Int?
    /// This phone's own active slice, as the Core's `active` flag marks it.
    @Published private(set) var ownActiveSliceId: Int?
    /// This phone's own volume and mute for each slice it listens to
    /// (`slice.setListenLevel`); the Core's own level for the slice is
    /// untouched.
    @Published private(set) var listenLevels: [Int: ListenLevel] = [:]
    /// The band shows another pan's slice (``jumped``), as published.
    @Published private(set) var showingAnotherBand = false
    /// A slice this phone asked to listen to, shown once the Core joins it.
    private var pendingShowId: Int?
    /// The Core has joined ``pendingShowId`` since the phone asked, heard
    /// as each list of keys lands (``heardKeys(_:)``), not on the rebuild a
    /// turn later: a join and a leave in one turn still end the wait.
    private var pendingJoined = false

    /// This phone's own volume for a listened slice: 0 to 1, and its mute.
    struct ListenLevel: Equatable {
        var level: Double
        var muted: Bool
    }

    /// The band shows another pan's slice: Back to your band is up.
    var jumped: Bool {
        guard let shown = shownEntry, let home = homeEntry else {
            return false
        }
        return !Self.sameReceiver(shown, home)
    }

    /// The slice whose band the phone shows while jumped.
    var shownEntry: Entry? {
        shownSliceId.flatMap { id in entries.first { $0.id == id } }
    }

    /// The phone's own band: the slice it left, or its own active slice.
    var homeEntry: Entry? {
        (returnSliceId ?? ownActiveSliceId).flatMap { id in entries.first { $0.id == id } }
    }

    /// Whether the slice is on the phone's own band ("This pan").
    func isHere(_ entry: Entry) -> Bool {
        guard let home = homeEntry else {
            return true
        }
        return Self.sameReceiver(entry, home)
    }

    /// Two slices on the same pan: the same pan key, else the same receiver;
    /// with neither known, the same band.
    static func sameReceiver(_ a: Entry, _ b: Entry) -> Bool {
        if let left = a.panKey, let right = b.panKey, !left.isEmpty, !right.isEmpty {
            return left == right
        }
        if a.streamIndex >= 0, b.streamIndex >= 0 {
            return a.streamIndex == b.streamIndex
        }
        return true
    }

    /// What the band draws, for a view of `viewHz`: the slices whose flags
    /// show, and those at its edges (``EdgeMark``). While jumped, the shown
    /// slice's pan alone. At home, this pan's slices and every other slice
    /// this phone is in whose frequency is inside the view; the others sit
    /// at the band's edge on the side their frequency lies.
    func bandEntries(viewHz: ClosedRange<Double>) -> (flags: [Entry], edges: [EdgeMark]) {
        if jumped, let shown = shownEntry {
            return (entries.filter { Self.sameReceiver($0, shown) }, [])
        }
        var flags: [Entry] = []
        var edges: [EdgeMark] = []
        for entry in entries {
            let hz = entry.slice.frequencyHz
            if isHere(entry) || viewHz.contains(hz) {
                flags.append(entry)
            } else {
                edges.append(EdgeMark(entry: entry, below: hz < viewHz.lowerBound))
            }
        }
        return (flags, edges)
    }

    /// A slice this phone is in on another pan, outside the band's view: a
    /// marker at the band's edge on the side its frequency lies, dashed
    /// while listened; a tap shows its band.
    struct EdgeMark: Equatable, Identifiable {
        let entry: Entry
        /// Its frequency is below the band: the left edge; else the right.
        let below: Bool

        var id: Int { entry.id }
    }

    /// The Core's words for one of this phone's slice writes it refused.
    struct Refusal: Equatable, Identifiable {
        let id: Int
        let text: String
        /// A refusal on taking control or choosing the transmit slice:
        /// drawn with the board's amber dot and plain words (R-IOS-42).
        var takeOver = false
        /// The fix the Core offers with it (`refusalFix`), `takeTransmit`
        /// or `operateAmp`; empty for none.
        var fix = ""
        /// The slice whose TX button was refused, which a Take transmit
        /// from this refusal moves transmit onto.
        var sliceId: Int? = nil
    }

    /// The Core's words for the last slice write it refused, shown over the
    /// band until closed or until the Core takes the next one.
    @Published private(set) var refusal: Refusal?
    private var refusals = 0
    /// What shows when the Core refuses without words: the phone's words
    /// for any request the Core refused.
    static let refusedText = "The Core refused this request."

    /// Take control was sent for these slices and the Core has not answered.
    @Published private(set) var taking: Set<Int> = []
    /// The slices this phone took from another device and has not chosen
    /// for transmit since: the Core refuses this phone's first key on one
    /// until its TX button is pressed (D110). Cleared by that press, by
    /// the slice closing and by control moving away.
    @Published private(set) var takenHere: Set<Int> = []
    /// Where every full flag sits on the band, each with its column of
    /// round buttons, in the band's points: the band's notices keep clear
    /// of the flags' buttons and of Take control on a listened slice's flag.
    @Published var flagRects: [CGRect] = []
    /// For each slice in `takenHere`, the control revision the take made:
    /// until the mirror reaches it the slice may still read as listened.
    private var takenAtRevision: [Int: Int64] = [:]

    /// This phone's device id, as the Core's `access:<id>` names a
    /// controller; nil until the phone has its key, when every slice reads
    /// as this phone's.
    var thisDeviceId: String? {
        didSet {
            if thisDeviceId != oldValue {
                rebuild()
            }
        }
    }

    /// The Core's catalogue: the slices' colours, the modes' sidebands and labels.
    var catalog: StationCatalog? {
        didSet {
            if catalog != oldValue {
                rebuild()
            }
        }
    }

    /// The pan this band draws; nil shows every slice.
    var panKey: String? {
        didSet {
            if panKey != oldValue {
                rebuild()
            }
        }
    }

    private let store: MirrorStore
    private let directOutcomeOwner = ControlOutcomeOwner()
    private let commands: CommandClient?
    private let now: () -> TimeInterval
    private var watching: [String: AnyCancellable] = [:]
    private var keysWatch: AnyCancellable?
    private var capabilitiesWatch: AnyCancellable?
    private var throttle = TuneThrottle()
    /// While the Core has a question open, a drag sends only its final
    /// value: each write would replace the question it answers (ruling 7.6).
    var sendsOnlyFinalValue = false
    /// A drag's latest value while only the final one is sent.
    private var finalOnly: Double?
    private var dragSliceId: Int?
    private var heldWrite: Task<Void, Never>?
    /// A frequency to write, and who waits for the Core's answer.
    private struct PendingWrite {
        let sliceId: Int
        let hz: Double
        let edit: UInt64?
        let noteEdit: UInt64
        let onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)?
        let answered: CheckedContinuation<PropertyWriteOutcome, Never>?
    }

    /// The frequency writer, while it has writes to send or answers to wait for.
    private var writer: Task<Void, Never>?
    /// The next frequency to write, once the one in flight is answered.
    private var queuedWrite: PendingWrite?
    /// A request to move a receiver's window is waiting for the Core.
    private var windowMoveInFlight = false
    /// The next window move, once the one in flight is answered.
    private var queuedWindowMove: (sliceId: Int, centreHz: Double, refused: @MainActor (String) -> Void)?
    /// The frequency shown for a slice under a finger, until the Core answers.
    private var shown: [Int: Double] = [:]
    private static let logger = Logger(subsystem: "NereusSDR", category: "band.slices")

    init(store: MirrorStore, commands: CommandClient?, catalog: StationCatalog? = nil,
         now: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) {
        self.store = store
        self.commands = commands
        self.catalog = catalog
        self.now = now
        keysWatch = store.$objectKeys.sink { [weak self] keys in
            MainActor.assumeIsolated {
                self?.heardKeys(keys)
            }
            Task { @MainActor in self?.watchSlices() }
        }
        // The RADE row's gate arrives with the capabilities, which can
        // follow the slices; redraw when it does.
        capabilitiesWatch = store.$capabilities.dropFirst().sink { [weak self] _ in
            Task { @MainActor in self?.rebuild() }
        }
        watchSlices()
    }

    /// The active slice's entry.
    var active: Entry? {
        entries.first { $0.id == activeSliceId }
    }

    // MARK: Tuning

    /// A tap on the band: one write of `hz` to the active slice. On a
    /// slice this phone only listens to, the Core's line for who controls
    /// it shows instead, and nothing is sent.
    func tap(to hz: Double) {
        guard let entry = active else {
            return
        }
        guard isTunable(entry) else {
            if let line = entry.ownerLine {
                showReason(line)
            }
            return
        }
        write(hz, to: entry.id)
    }

    /// One write of the slice's mode (`dspMode`, the catalogue's `modes` id).
    func setMode(_ mode: Int, sliceId: Int) {
        writeDirect("dspMode", .enumeration(Int64(mode)), sliceId: sliceId)
    }

    /// Whether this phone may change the slice: not while its frequency is
    /// locked, and not while another device or nobody controls it.
    func isTunable(_ entry: Entry) -> Bool {
        !entry.locked && !entry.listening
    }

    /// The slices this phone may change.
    var tunableIds: Set<Int> {
        Set(entries.filter(isTunable).map(\.id))
    }

    /// A drag on the band moved to `hz` for the active slice.
    func drag(to hz: Double) {
        guard let id = activeSliceId else {
            return
        }
        drag(sliceId: id, to: hz)
    }

    /// A drag on a slice's flag or passband moved to `hz`: written at most
    /// every 50 ms.
    func drag(sliceId: Int, to hz: Double) {
        guard let entry = entries.first(where: { $0.id == sliceId }), isTunable(entry) else {
            return
        }
        if dragSliceId != entry.id {
            finishDrag()
            dragSliceId = entry.id
        }
        shown[entry.id] = hz
        rebuild()
        if sendsOnlyFinalValue {
            finalOnly = hz
            return
        }
        switch throttle.offer(hz, at: now()) {
        case .send(let value):
            write(value, to: entry.id)
        case .hold(let until):
            scheduleHeld(at: until)
        case .nothing:
            break
        }
    }

    /// The finger lifted: the final value goes, if it has not already.
    func finishDrag() {
        heldWrite?.cancel()
        heldWrite = nil
        guard let id = dragSliceId else {
            return
        }
        dragSliceId = nil
        if let final = finalOnly {
            finalOnly = nil
            _ = throttle.finish()
            write(final, to: id)
        } else if let final = throttle.finish() {
            write(final, to: id)
        }
        // Show the Core's value again once it has answered the last write.
        let last = writer
        Task {
            await last?.value
            self.release(id)
        }
    }

    /// A frequency typed on the pad: one write, after any still in flight,
    /// and the Core's answer.
    func enter(_ hz: Double, sliceId: Int,
               onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        await withCheckedContinuation { answered in
            write(hz, to: sliceId, answered: answered, onLateOutcome: onLateOutcome)
        }
    }

    /// The flag's step menu: the slice's tuning step, which the Core keeps.
    func setStep(_ hz: Double, sliceId: Int) {
        writeDirect("stepHz", .int(Int64(hz.rounded())), sliceId: sliceId)
    }

    /// Whether the Core moves a receiver's window for this phone.
    var canMoveReceiverWindow: Bool {
        commands != nil && (store.agreedMinor ?? 0) >= Self.receiverWindowMinor
            && store.capabilityVersion(Self.receiverWindowCapability) >= 1
    }

    /// A pan past the receiver's window: asks the Core to centre the
    /// window that hears `sliceId` on `centreHz`, keeping it there while the
    /// slices tune inside it, as the desktop's remote window does. One
    /// request at a time; a newer one replaces any still waiting. The Core
    /// refuses a centre that would leave one of the window's slices outside
    /// it, and `refused` then gets its words.
    func moveReceiverWindow(sliceId: Int, centreHz: Double, refused: @escaping @MainActor (String) -> Void) {
        guard canMoveReceiverWindow, let commands else {
            refused("")
            return
        }
        guard !windowMoveInFlight else {
            queuedWindowMove = (sliceId, centreHz, refused)
            return
        }
        windowMoveInFlight = true
        Task {
            var next: (sliceId: Int, centreHz: Double, refused: @MainActor (String) -> Void)? = (sliceId, centreHz, refused)
            var pinned = Set<Int>()
            while let current = next {
                let id = Int64(current.sliceId)
                if entries.first(where: { $0.id == current.sliceId })?.receiverPinned == false,
                   !pinned.contains(current.sliceId) {
                    pinned.insert(current.sliceId)
                    let pin = try? await commands.invoke(Self.pinVerb, arguments: [
                        CommandArgument(name: "sliceId", value: .int(id)),
                        CommandArgument(name: "pinned", value: .bool(true)),
                    ], timeout: Self.commandTimeout)
                    if pin?.accepted != true {
                        Self.logger.info("The Core did not keep the receiver's window still: \(pin?.reason ?? "", privacy: .private)")
                    }
                    if let pin {
                        note(pin)
                    }
                }
                let result: CommandResult?
                do {
                    result = try await commands.invoke(Self.centreVerb, arguments: [
                        CommandArgument(name: "sliceId", value: .int(id)),
                        CommandArgument(name: "centreHz", value: .double(current.centreHz.rounded())),
                    ], timeout: Self.commandTimeout)
                } catch {
                    Self.logger.info("\(Self.centreVerb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
                    result = nil
                }
                if let result {
                    note(result)
                }
                if result?.accepted != true {
                    current.refused(result?.reason ?? "")
                    queuedWindowMove = nil
                    break
                }
                next = queuedWindowMove
                queuedWindowMove = nil
            }
            windowMoveInFlight = false
        }
    }

    /// A tap on another slice's flag or tag: it becomes the active slice.
    /// One of this phone's own slices on its own band waits for the Core's
    /// `active` flag (and leaves any slice it showed); a slice it only
    /// listens to, or one on another pan, is shown at once
    /// (``show(_:)``).
    func activate(_ sliceId: Int) {
        guard sliceId != activeSliceId, let entry = entries.first(where: { $0.id == sliceId }) else {
            return
        }
        if entry.listening || !isHere(entry) {
            show(sliceId)
            return
        }
        if shownSliceId != nil {
            // Back on its own band, with this slice: the Core hears this phone on it again.
            shownSliceId = nil
            returnSliceId = nil
            settleActive()
        }
        invoke(Self.activateVerb, sliceId: sliceId)
    }

    /// Listen, the list's Show its band, an edge marker or a listened
    /// flag's tap: the slice becomes active on this phone alone, and on
    /// another pan the band jumps to its display. The Core hears this
    /// phone on it (`setActiveSliceById`, which moves only this device's
    /// receive slice). A slice not joined here is shown once the Core
    /// joins it.
    func show(_ sliceId: Int) {
        guard let entry = entries.first(where: { $0.id == sliceId }) else {
            waitForJoin(sliceId)
            return
        }
        pendingShowId = nil
        if !entry.listening, isHere(entry) {
            activate(sliceId)
            return
        }
        if shownSliceId == nil {
            returnSliceId = ownActiveSliceId
        }
        shownSliceId = sliceId
        settleActive()
        invoke(Self.activateVerb, sliceId: sliceId)
    }

    /// Back to your band: the slice the phone left is active again, and
    /// the band shows its display; every slice it listens to stays.
    func back() {
        guard shownSliceId != nil else {
            return
        }
        let home = returnSliceId ?? ownActiveSliceId
        shownSliceId = nil
        returnSliceId = nil
        settleActive()
        if let home {
            invoke(Self.activateVerb, sliceId: home)
        }
    }

    /// PTT while the phone shows another slice: back to the transmit
    /// slice's band, which becomes active. The listened slice stays
    /// listened, at the band's edge.
    func backForTransmit() {
        guard let shown = shownSliceId else {
            return
        }
        let transmit = entries.first { $0.slice.txSlice && !$0.listening }
        guard transmit?.id != shown else {
            return
        }
        if let transmit {
            returnSliceId = transmit.id
        }
        back()
    }

    /// The Core refused the shown slice's display (its receiver is not on
    /// the Core, or there is no room): the phone stays on its own band,
    /// shows the Core's words as sent, and keeps hearing the slice, whose
    /// marker sits at the band's edge.
    func displayRefused(sliceId: Int, reason: String) {
        guard shownSliceId == sliceId, jumped else {
            return
        }
        back()
        refused(reason, takeOver: true)
    }

    /// The flag's close button: the Core closes the slice.
    func close(_ sliceId: Int) {
        invoke(Self.removeVerb, sliceId: sliceId)
    }

    /// The flag's Sample rate items: the Core moves the receiver that hears
    /// `sliceId` to `rateHz` (`requestSliceSampleRate`), which every slice on
    /// that receiver shares.
    func requestSampleRate(_ rateHz: Int, sliceId: Int) {
        // Shown at the touch on every slice the receiver hears, kept until
        // the Core answers (StationClient.cpp:1040-1068).
        let stream = entries.first { $0.id == sliceId }?.streamIndex ?? -1
        let ids = Set([sliceId] + entries.filter { stream >= 0 && $0.streamIndex == stream }.map(\.id))
        invoke(Self.sampleRateVerb, sliceId: sliceId,
               extra: [CommandArgument(name: "rateHz", value: .int(Int64(rateHz)))],
               shows: ids.sorted().map { .init("slice:\($0)", "sampleRateHz", .int(Int64(rateHz))) })
    }

    /// A greyed flag button's reason, shown over the band as the Core's
    /// words for a refusal are, until closed or replaced.
    func showReason(_ text: String) {
        refused(text, takeOver: true)
    }

    /// The Core's words for a take of transmit it refused, or the phone's
    /// when it never answered, shown over the band as a TX button's refusal.
    func showTakeRefusal(_ text: String) {
        refused(text, takeOver: true)
    }

    /// CTUN on the Core: keeps the window that hears `sliceId` still while
    /// it tunes (`pinned`), or lets it follow, as the desktop's remote
    /// window asks when its CTUN changes. Nothing is sent to a Core that
    /// does not take it (``canMoveReceiverWindow``).
    func setReceiverPinned(_ pinned: Bool, sliceId: Int) {
        guard canMoveReceiverWindow else {
            return
        }
        invoke(Self.pinVerb, sliceId: sliceId, extra: [CommandArgument(name: "pinned", value: .bool(pinned))],
               shows: [.init("slice:\(sliceId)", "streamCtunPinned", .bool(pinned))])
    }

    /// The lock on the flag: the Core decides.
    func setLocked(_ locked: Bool, sliceId: Int) {
        writeDirect("locked", .bool(locked), sliceId: sliceId)
    }

    private func writeDirect(_ property: String, _ value: MirrorValue, sliceId: Int) {
        let key = "slice:\(sliceId)"
        let edit = store.hold(key, property: property, value: value)
        let noteEdit = directOutcomeOwner.begin()
        let store = store
        Task { [weak self] in
            let outcome = await store.write(key, property: property, value: value, edit: edit,
                                            onLateOutcome: { [weak self] outcome in
                guard let self, self.directOutcomeOwner.isCurrent(noteEdit) else { return }
                self.note(outcome)
            })
            guard let self, self.directOutcomeOwner.isCurrent(noteEdit) else { return }
            self.note(outcome)
        }
    }

    /// The operator closed the refusal over the band.
    func dismissRefusal() {
        refusal = nil
    }

    // MARK: Taking control

    /// Take control on a slice this phone listens to (R-IOS-42, D109):
    /// `slice.takeControl` with the slice's id, incarnation and control
    /// revision as the phone last saw them. The flag says "Taking
    /// control..." until the Core answers. Accepted, the slice is this
    /// phone's as it was, every setting kept, and transmit stays where it
    /// is; refused, the Core's words show over the band and the button
    /// stays. `answered` hears whether the Core took it (false for a
    /// refusal, no answer or nothing sent), after the refusal shows; the
    /// flag's TX badge goes on to transmit from there. Returns whether
    /// `slice.takeControl` was sent.
    @discardableResult
    func takeControl(_ sliceId: Int, answered: ((Bool) -> Void)? = nil) -> Bool {
        guard let commands, !taking.contains(sliceId) else {
            return false
        }
        let access: SliceAccess.State
        if let entry = entries.first(where: { $0.id == sliceId }) {
            // A slice joined here: only one this phone listens to, and not while it is greyed.
            guard entry.listening, let state = entry.access, entry.takeRefusal == nil else {
                return false
            }
            access = state
        } else {
            // A slice this phone is not in (the slice list's rows): every
            // slice can be taken, greyed only while it is on the air.
            guard let state = SliceAccess.states(in: store)[sliceId],
                  SliceAccess.takeRefusal(state, version: SliceAccess.version(in: store),
                                          devices: SeveralDevices.connectedDevices(in: store)) == nil else {
                return false
            }
            access = state
        }
        taking.insert(sliceId)
        Task { @MainActor in
            let outcome = await SliceAccess.takeControl(access, commands: commands)
            self.taking.remove(sliceId)
            switch outcome {
            case .accepted(let revision):
                self.refusal = nil
                self.tookControl(sliceId, revision: revision)
            case .refused(let reason):
                Self.logger.info("The Core refused the take: \(reason, privacy: .private)")
                self.refused(reason, takeOver: true)
            case .notAnswered:
                self.refused(SeveralDevicesClient.noAnswerText, takeOver: true)
            case .notSent:
                break
            }
            if case .accepted = outcome {
                answered?(true)
            } else {
                answered?(false)
            }
        }
        return true
    }

    // MARK: Listening

    /// The slice's `access:<id>`, from the slice joined here or the Core's list of every slice.
    func accessState(_ sliceId: Int) -> SliceAccess.State? {
        entries.first { $0.id == sliceId }?.access ?? SliceAccess.states(in: store)[sliceId]
    }

    /// Listen on a slice this phone is not in (`slice.listen`): the Core
    /// joins it, and the band shows it once joined (on another pan, the
    /// band jumps to its display). `incarnation` is the one the phone saw;
    /// with none, the slice's `access:<id>`'s. The Core's refusal comes
    /// back in its words, as sent; nil once accepted.
    func listen(_ sliceId: Int, incarnation: Int64? = nil) async -> String? {
        guard let commands else {
            return nil
        }
        if entries.contains(where: { $0.id == sliceId }) {
            // Joined already: show it.
            show(sliceId)
            return nil
        }
        guard let incarnation = incarnation ?? accessState(sliceId)?.incarnation else {
            return nil
        }
        waitForJoin(sliceId)
        let outcome = await SliceAccess.listen(sliceId: sliceId, incarnation: incarnation, commands: commands)
        return words(outcome, pending: sliceId)
    }

    /// Stop listening (`slice.stopListening`): the phone leaves the slice;
    /// if it showed the slice, it goes back to its own band first.
    func stopListening(_ sliceId: Int) async -> String? {
        guard let commands, let access = accessState(sliceId) else {
            return nil
        }
        if shownSliceId == sliceId {
            back()
        }
        let outcome = await SliceAccess.stopListening(access, commands: commands)
        if case .accepted = outcome {
            listenLevels[sliceId] = nil
        }
        return words(outcome, pending: nil)
    }

    /// Release (`slice.release`): this phone gives up control and stops
    /// listening; the slice stays for its listeners.
    func releaseSlice(_ sliceId: Int) async -> String? {
        guard let commands, let access = accessState(sliceId) else {
            return nil
        }
        if shownSliceId == sliceId {
            back()
        }
        return words(await SliceAccess.release(access, commands: commands), pending: nil)
    }

    /// This phone's own volume for a slice it listens to: where it starts
    /// (the slice's own AF gain) until the phone changes it.
    func listenLevel(_ sliceId: Int) -> ListenLevel {
        if let kept = listenLevels[sliceId] {
            return kept
        }
        let gain = entries.first { $0.id == sliceId }?.afGain ?? Self.startingGain
        return ListenLevel(level: min(max(gain / 100, 0), 1), muted: false)
    }

    /// Where a listened slice's volume starts when the Core sends no AF gain.
    static let startingGain: Double = 62

    /// This phone's own volume and mute for a listened slice
    /// (`slice.setListenLevel`): shown at once, sent one at a time, the
    /// newest replacing any still waiting. It changes only what this
    /// phone hears.
    func setListenLevel(_ sliceId: Int, level: Double, muted: Bool) {
        let next = ListenLevel(level: min(max(level, 0), 1), muted: muted)
        listenLevels[sliceId] = next
        guard let commands, let access = accessState(sliceId) else {
            return
        }
        guard !levelsInFlight.contains(sliceId) else {
            queuedLevels[sliceId] = next
            return
        }
        levelsInFlight.insert(sliceId)
        Task { @MainActor in
            var send: ListenLevel? = next
            while let current = send {
                let outcome = await SliceAccess.setListenLevel(access, level: current.level, muted: current.muted,
                                                               commands: commands)
                if case .refused(let reason) = outcome {
                    Self.logger.info("The Core kept its level: \(reason, privacy: .private)")
                    self.refused(reason, takeOver: true)
                }
                send = self.queuedLevels.removeValue(forKey: sliceId)
            }
            self.levelsInFlight.remove(sliceId)
        }
    }

    private var levelsInFlight: Set<Int> = []
    private var queuedLevels: [Int: ListenLevel] = [:]

    /// A listen verb's outcome: the Core's words as sent when refused, the
    /// phone's when it never answered; nil when accepted or never sent.
    private func words(_ outcome: SliceAccess.TakeOutcome, pending: Int?) -> String? {
        switch outcome {
        case .accepted:
            return nil
        case .refused(let reason):
            if let pending, pendingShowId == pending {
                pendingShowId = nil
            }
            return reason.isEmpty ? Self.refusedText : reason
        case .notAnswered:
            if let pending, pendingShowId == pending {
                pendingShowId = nil
            }
            return SeveralDevicesClient.noAnswerText
        case .notSent:
            return nil
        }
    }

    /// This phone took control of the slice, by Take control or Take it
    /// back: its first key on it waits for the slice's TX button.
    /// `revision` is the control revision the Core answered with; without
    /// one, the next after the revision the phone last saw.
    func tookControl(_ sliceId: Int, revision: Int64?) {
        let seen = entries.first { $0.id == sliceId }?.access?.controlRevision
        takenAtRevision[sliceId] = revision ?? seen.map { $0 + 1 } ?? 0
        takenHere.insert(sliceId)
    }

    /// Whether the PTT's last key was refused with the Core's first-key
    /// code (`chooseTransmitSlice`): its words show on the TX notice as
    /// sent, and the TX button of each slice this phone took is ringed.
    static func refusedForTransmitChoice(_ ptt: PttController.Snapshot) -> Bool {
        if case .refused(let refusal) = ptt.state {
            return refusal.code == chooseTransmitSliceCode
        }
        return false
    }

    /// Whether the Core takes a transmit slice choice from this phone.
    var canSelectTransmitSlice: Bool {
        commands != nil && (store.agreedMinor ?? 0) >= Self.remoteTxMinor
            && store.capabilityVersion(Self.remoteTxCapability) >= 1
    }

    /// Takes transmit before a slice's TX button chooses it, where the
    /// Core takes `tx.take` and this phone does not hold transmit
    /// (``TransmitTakeModel/transmitOn(sliceId:)``): true when the take
    /// began, and it sends `tx.setTxSlice` for the slice once accepted.
    var takeTransmitFirst: ((Int) -> Bool)?

    /// The TX button on one of this phone's slices: `tx.setTxSlice`, which
    /// makes it the transmit slice (the only way this phone moves
    /// transmit). Nothing is sent for a slice another device controls.
    /// While another device holds transmit, or nobody does, the take comes
    /// first (``takeTransmitFirst``); otherwise nothing is sent for a slice
    /// already the transmit slice. The Core's refusal shows over the band.
    func selectForTransmit(_ sliceId: Int) {
        guard canSelectTransmitSlice,
              let entry = entries.first(where: { $0.id == sliceId }), !entry.listening else {
            return
        }
        if takeTransmitFirst?(sliceId) == true {
            return
        }
        guard !entry.slice.txSlice else {
            return
        }
        sendTransmitSlice(sliceId)
    }

    /// `tx.setTxSlice` for one of this phone's slices, after a take or
    /// from its TX button; nothing for a listened slice or one already the
    /// transmit slice. `taken` lets through a slice this phone took whose
    /// control the mirror has not caught up with (``takenHere``), as the
    /// flag's TX badge sends it once its take of transmit is done.
    func sendTransmitSlice(_ sliceId: Int, taken: Bool = false) {
        guard canSelectTransmitSlice, let commands,
              let entry = entries.first(where: { $0.id == sliceId }), !entry.slice.txSlice,
              !entry.listening || (taken && takenHere.contains(sliceId)) else {
            return
        }
        // The TX badge moves at the touch and stays until the Core answers
        // (StationClient.cpp:1040-1068).
        let shows = entries.filter { $0.slice.txSlice && $0.id != sliceId }
            .map { CommandHoldQueue.Shown("slice:\($0.id)", "txSlice", .bool(false)) }
            + [CommandHoldQueue.Shown("slice:\(sliceId)", "txSlice", .bool(true))]
        commandHolds.send(Self.setTxSliceVerb, shows: shows, invokeWithLate: { late in
            try await commands.invokeHeld(Self.setTxSliceVerb, arguments: [
                CommandArgument(name: "sliceId", value: .int(Int64(sliceId))),
            ], timeout: Self.commandTimeout, onLateOutcome: { outcome in
                await MainActor.run { late(outcome) }
            })
        }, onOutcome: { [weak self] outcome in
            guard let self else {
                return
            }
            switch outcome {
            case .success(let result) where !result.accepted && SeveralDevices.waitsForConfirmation(result):
                // Held for this phone's question, not refused (StationClient.cpp:7308-7317).
                break
            case .success(let result):
                if result.accepted {
                    self.takenHere.remove(sliceId)
                    self.takenAtRevision[sliceId] = nil
                    self.refusal = nil
                } else {
                    Self.logger.info("The Core refused the transmit slice: \(result.reason, privacy: .private)")
                    var fix = ""
                    if case .text(let text)? = result.values["refusalFix"] {
                        fix = text
                    }
                    self.refused(result.reason, takeOver: true, fix: fix, sliceId: sliceId)
                }
            case .failure(let error):
                Self.logger.info("\(Self.setTxSliceVerb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
                self.note(PropertyWriteOutcome(outcome))
            }
        })
    }

    // MARK: Inside

    /// A command that changes values the Core mirrors (`shows`): shown at
    /// the touch and kept until the Core answers (``CommandHoldQueue``).
    private func invoke(_ verb: String, sliceId: Int, extra: [CommandArgument], shows: [CommandHoldQueue.Shown]) {
        guard let commands else {
            note(.notSent)
            return
        }
        let arguments = [CommandArgument(name: "sliceId", value: .int(Int64(sliceId)))] + extra
        commandHolds.send("\(verb):\(sliceId)", shows: shows, invokeWithLate: { late in
            try await commands.invokeHeld(verb, arguments: arguments, timeout: Self.commandTimeout, onLateOutcome: { outcome in
                await MainActor.run { late(outcome) }
            })
        }, onOutcome: { [weak self] outcome in
            if case .success(let result) = outcome, !result.accepted {
                Self.logger.info("The Core refused \(verb, privacy: .public): \(result.reason, privacy: .private)")
            } else if case .failure(let error) = outcome {
                Self.logger.info("\(verb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
            }
            self?.note(PropertyWriteOutcome(outcome))
        })
    }

    private lazy var commandHolds = CommandHoldQueue(store: store)

    private func invoke(_ verb: String, sliceId: Int, extra: [CommandArgument] = []) {
        guard let commands else {
            return
        }
        Task { @MainActor in
            do {
                let result = try await commands.invoke(verb, arguments: [CommandArgument(name: "sliceId",
                                                                                         value: .int(Int64(sliceId)))]
                                                           + extra,
                                                       timeout: Self.commandTimeout)
                if !result.accepted {
                    Self.logger.info("The Core refused \(verb, privacy: .public): \(result.reason, privacy: .private)")
                }
                self.note(result)
            } catch {
                Self.logger.info("\(verb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
            }
        }
    }

    private func scheduleHeld(at until: TimeInterval) {
        guard heldWrite == nil else {
            return
        }
        let delay = max(0, until - now())
        heldWrite = Task { [weak self] in
            try? await Task.sleep(for: .seconds(delay))
            guard !Task.isCancelled, let self else {
                return
            }
            self.heldWrite = nil
            if let id = self.dragSliceId, let value = self.throttle.due(at: self.now()) {
                self.write(value, to: id)
            }
        }
    }

    /// Frequency writes go one at a time, each after the Core has answered
    /// the one before, so they reach it in order and the last value is the
    /// one it keeps. A value that arrives while one is in flight replaces
    /// any other still waiting (whose waiter is told nothing was sent).
    private func write(_ hz: Double, to sliceId: Int,
                       answered: CheckedContinuation<PropertyWriteOutcome, Never>? = nil,
                       onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) {
        let edit = store.hold("slice:\(sliceId)", property: "frequency", value: .double(hz))
        let pending = PendingWrite(sliceId: sliceId, hz: hz, edit: edit, noteEdit: directOutcomeOwner.begin(),
                                   onLateOutcome: onLateOutcome, answered: answered)
        guard writer == nil else {
            queuedWrite?.answered?.resume(returning: Self.replaced)
            queuedWrite = pending
            return
        }
        let store = store
        writer = Task { [weak self] in
            var next: PendingWrite? = pending
            while let current = next {
                let outcome = await store.write("slice:\(current.sliceId)", property: "frequency",
                                                value: .double(current.hz), edit: current.edit,
                                                onLateOutcome: { [weak self] outcome in
                    guard let self, self.directOutcomeOwner.isCurrent(current.noteEdit) else { return }
                    if let late = current.onLateOutcome { late(outcome) } else { self.note(outcome) }
                    if outcome.isCurrent && outcome.answeredByCore && !outcome.accepted && !outcome.heldForQuestion {
                        self.returnToCore(current.sliceId)
                    }
                })
                if !outcome.accepted {
                    Self.logger.info("The Core kept the frequency: \(outcome.reason, privacy: .private)")
                }
                let currentTouch = self?.directOutcomeOwner.isCurrent(current.noteEdit) == true
                if current.answered == nil, currentTouch { self?.note(outcome) }
                if outcome.isCurrent, currentTouch, !outcome.heldForQuestion,
                   !outcome.accepted, outcome.answeredByCore, self?.queuedWrite?.sliceId != current.sliceId {
                    // The frequency shown under the finger goes back to the Core's.
                    self?.returnToCore(current.sliceId)
                }
                current.answered?.resume(returning: outcome)
                if !outcome.answeredByCore, outcome.reason == PropertyWriteOutcome.linkLost.reason,
                   let waiting = self?.queuedWrite,
                   !(waiting.edit.map { store.isCurrent("slice:\(waiting.sliceId)", property: "frequency", edit: $0) } ?? false) {
                    self?.queuedWrite = nil
                    waiting.answered?.resume(returning: .linkLost)
                    if self?.directOutcomeOwner.isCurrent(waiting.noteEdit) == true { self?.note(.linkLost) }
                }
                next = self?.queuedWrite
                self?.queuedWrite = nil
            }
            self?.writer = nil
        }
    }

    /// The Core refused a frequency for `sliceId`: the slice shows the
    /// Core's own again, until the finger moves on.
    private func returnToCore(_ sliceId: Int) {
        guard shown.removeValue(forKey: sliceId) != nil else {
            return
        }
        rebuild()
    }

    /// The Core's answer to a property write: a refusal shows its words; a
    /// kept value closes the last refusal. A write the Core never answered
    /// shows nothing.
    private func note(_ outcome: PropertyWriteOutcome) {
        guard outcome.isCurrent else { return }
        if outcome.heldForQuestion {
            // Held for this phone's question, not refused (StationClient.cpp:7308-7317).
            return
        }
        if outcome.accepted {
            refusal = nil
        } else if outcome.answeredByCore {
            refused(outcome.reason)
        } else if !outcome.reason.isEmpty {
            // Not sent, not answered in time, or cut off by a dropped link:
            // the operator's value stays, or the Core's returns, and says so.
            Self.logger.info("A slice write: \(outcome.reason, privacy: .private)")
            refused(outcome.reason)
        }
    }

    /// The Core's answer to a command: as for a write.
    private func note(_ result: CommandResult) {
        if result.accepted {
            refusal = nil
        } else {
            refused(result.reason)
        }
    }

    private func refused(_ reason: String, takeOver: Bool = false, fix: String = "", sliceId: Int? = nil) {
        refusals += 1
        refusal = Refusal(id: refusals, text: reason.isEmpty ? Self.refusedText : reason, takeOver: takeOver,
                          fix: fix, sliceId: sliceId)
    }

    /// What a replaced write's waiter hears: nothing was sent for it.
    private static let replaced = PropertyWriteOutcome(accepted: false, reason: "", value: nil,
                                                       answeredByCore: false)

    /// The Core has answered the drag's last write: show its value again,
    /// unless another drag has begun on the slice.
    private func release(_ sliceId: Int) {
        guard dragSliceId != sliceId, shown.removeValue(forKey: sliceId) != nil else {
            return
        }
        rebuild()
    }

    private func watchSlices() {
        // The slices, who controls each (`access:<id>`), and who is on the
        // Core (the holder's name in the owner line).
        var slices = store.objects(ofClass: Self.sliceClass) + store.objects(ofClass: SliceAccess.accessClass)
        if let radio = store.object("radio") { slices.append(radio) }
        if let devices = store.object(SeveralDevices.connectedDevicesKey) {
            slices.append(devices)
        }
        let keys = Set(slices.map(\.key))
        for key in watching.keys where !keys.contains(key) {
            watching[key] = nil
        }
        for object in slices where watching[object.key] == nil {
            watching[object.key] = object.$values.dropFirst().sink { [weak self] _ in
                Task { @MainActor in self?.rebuild() }
            }
        }
        rebuild()
    }

    private func rebuild() {
        var next: [Entry] = []
        var nextActive: Int?
        var anyActive: Int?
        let radeStatus = sendsRadeStatus
        let radeReason = store.capabilityVersion(Self.radeReasonCapability) >= 1
        let access = SliceAccess.states(in: store)
        let devices = access.isEmpty ? [] : SeveralDevices.connectedDevices(in: store)
        let accessVersion = SliceAccess.version(in: store)
        let diversity = DiversityState.available(in: store)
            ? store.object("radio")?[DiversityState.propertyName]?.text.flatMap(DiversityState.init(json:)) : nil
        var present = Set<Int>()
        for object in store.objects(ofClass: Self.sliceClass) {
            guard let id = Self.sliceId(object) else {
                continue
            }
            present.insert(id)
            if let panKey, let pan = object["panKey"]?.text, !pan.isEmpty, pan != panKey {
                continue
            }
            guard let frequency = object["frequency"]?.number else {
                continue
            }
            let mode = Int(object["dspMode"]?.number ?? -1)
            let modes = catalog?.modes ?? []
            let modeLabel = modes.first(where: { $0.id == mode })?.label ?? ""
            let slice = BandSlice(id: id, frequencyHz: shown[id] ?? frequency,
                                  filterLowHz: object["filterLow"]?.number ?? 0,
                                  filterHighHz: object["filterHigh"]?.number ?? 0,
                                  colour: BandSlice.colour(forIndex: id, in: catalog?.sliceColours ?? []),
                                  lowerSideband: BandSlice.isLowerSideband(mode: mode, in: modes),
                                  txSlice: object["txSlice"]?.flag ?? false)
            next.append(Entry(slice: slice, rxAntenna: object["rxAntenna"]?.text ?? "",
                              txAntenna: object["txAntenna"]?.text ?? "",
                              modeLabel: modeLabel,
                              panKey: object["panKey"]?.text,
                              signalDbm: object["signalStrengthDbm"]?.number,
                              signalPeakDbm: object["signalPeakDbm"]?.number,
                              signalAverageDbm: object["signalAverageDbm"]?.number,
                              stepHz: object["stepHz"]?.number,
                              sampleRateHz: object["sampleRateHz"]?.number, locked: object["locked"]?.flag ?? false,
                              muted: object["muted"]?.flag ?? false,
                              receiverPinned: object["streamCtunPinned"]?.flag ?? false,
                              band: object["band"]?.number.map { Int($0) },
                              mode: mode >= 0 ? mode : nil,
                              rade: Self.radeReception(object, modeLabel: modeLabel,
                                                          radeStatus: radeStatus, radeReason: radeReason),
                              control: control(access[id], devices: devices), access: access[id],
                              takeRefusal: takeRefusal(access[id], version: accessVersion, devices: devices),
                              streamIndex: object["streamIndex"]?.number.map { Int($0) } ?? -1,
                              afGain: object["afGain"]?.number,
                              ownerWordsText: access[id].map { SliceAccess.ownerWords($0, devices: devices) },
                              diversityOn: DiversityState.available(in: store)
                                ? diversity?.live.map { $0.identity.sliceId == id
                                    && (access[id] == nil || access[id]?.incarnation == $0.identity.incarnation) } ?? false
                                : id == 0 && object["diversityEnabled"]?.flag == true))
            if object["active"]?.flag == true {
                // This phone's own active slice; a listened slice's flag is its owner's.
                if next.last?.listening == false {
                    nextActive = nextActive ?? id
                } else {
                    anyActive = anyActive ?? id
                }
            }
        }
        next.sort { $0.id < $1.id }
        if next != entries {
            entries = next
        }
        // This phone's own volume goes with its slice: a slice the Core
        // opens later under the same id starts at its own AF gain.
        let keptLevels = listenLevels.filter { present.contains($0.key) }
        if keptLevels.count != listenLevels.count {
            listenLevels = keptLevels
        }
        // The first-key mark goes with the slice, and when control moves
        // away after the take (a listened slice below the take's revision
        // is a take the mirror has not caught up with).
        let stillTaken = takenHere.filter { id in
            guard let entry = next.first(where: { $0.id == id }) else {
                return false
            }
            return !entry.listening || (entry.access?.controlRevision ?? .max) < (takenAtRevision[id] ?? 0)
        }
        if stillTaken != takenHere {
            takenHere = stillTaken
            takenAtRevision = takenAtRevision.filter { stillTaken.contains($0.key) }
        }
        let own = nextActive ?? anyActive
        if own != ownActiveSliceId {
            ownActiveSliceId = own
        }
        if let shown = shownSliceId, !next.contains(where: { $0.id == shown }) {
            // The shown slice closed, or the phone left it: back to its own band.
            let home = returnSliceId ?? own
            shownSliceId = nil
            returnSliceId = nil
            if let home, home != shown {
                invoke(Self.activateVerb, sliceId: home)
            }
        }
        if let pending = pendingShowId, next.contains(where: { $0.id == pending }) {
            // Joined: the slice the phone asked to listen to shows, unless
            // it left (or another was asked for) before this turn came.
            Task { @MainActor [weak self] in
                guard let self, self.pendingShowId == pending else {
                    return
                }
                self.show(pending)
            }
        }
        settleActive()
    }

    /// Shows `sliceId` once the Core joins it; a join already in the
    /// mirror counts.
    private func waitForJoin(_ sliceId: Int) {
        pendingShowId = sliceId
        pendingJoined = store.objectKeys.contains(Self.sliceKey(sliceId))
    }

    /// Each list of the mirror's keys as it lands: the slice the phone
    /// waits for joins, or leaves again before it shows, which ends the
    /// wait, so a later join of that slice is not shown unasked.
    private func heardKeys(_ keys: [String]) {
        guard let pending = pendingShowId else {
            return
        }
        if keys.contains(Self.sliceKey(pending)) {
            pendingJoined = true
        } else if pendingJoined {
            pendingShowId = nil
            pendingJoined = false
        }
    }

    /// A slice's key in the mirror.
    private static func sliceKey(_ sliceId: Int) -> String {
        "slice:\(sliceId)"
    }

    /// The active slice: the one shown on this phone alone, else this
    /// phone's own active slice.
    private func settleActive() {
        let next = shownSliceId ?? ownActiveSliceId
        if next != activeSliceId {
            activeSliceId = next
        }
        let away = jumped
        if away != showingAnotherBand {
            showingAnotherBand = away
        }
    }

    /// Who controls a slice: this phone, unless the Core shares slices, the
    /// phone knows its own id and the slice's controller is another.
    private func control(_ access: SliceAccess.State?, devices: [SeveralDevices.ConnectedDevice]) -> Control {
        guard let access, let me = thisDeviceId, access.controllerDeviceId != me else {
            return .here
        }
        return .listening(ownerLine: SliceAccess.ownerLine(access, devices: devices))
    }

    /// Why Take control is greyed on a slice this phone listens to
    /// (R-IOS-42): at `sliceAccessVersion` 3 only while it is on the air;
    /// below 3 on the Core's own slice with no desktop hosting the Core.
    private func takeRefusal(_ access: SliceAccess.State?, version: Int64,
                             devices: [SeveralDevices.ConnectedDevice]) -> String? {
        guard let access, case .listening = control(access, devices: devices) else {
            return nil
        }
        return SliceAccess.takeRefusal(access, version: version, devices: devices)
    }

    /// Whether the Core sends RADE sync and offset: `radeStatusVersion` 1
    /// at agreed minor 11 or later.
    private var sendsRadeStatus: Bool {
        (store.agreedMinor ?? 0) >= Self.radeStatusMinor
            && store.capabilityVersion(Self.radeStatusCapability) >= 1
    }

    /// The RADE row's readings from the slice's mirror, while it is in
    /// RADE, read by name as the Core sends them: `snrDb` (a NaN is no
    /// reading) and `lastRadeRxCallsign`; and, from a Core that sends RADE
    /// status, `radeSynced` (missing reads not synced) and
    /// `radeFreqOffsetHz` (a NaN is no offset). The Core holds `snrDb` and
    /// the offset after sync is lost, so the row reads `radeSynced` to hide
    /// them. From an older Core both stay nil and the row greys. From a Core
    /// that sends `radeReasonVersion` 1, `radeReason` (empty is none) says
    /// why the slice has no working decoder.
    private static func radeReception(_ object: MirrorObject, modeLabel: String,
                                      radeStatus: Bool, radeReason: Bool) -> RadeReception? {
        guard RadeReception.modeLabels.contains(modeLabel) else {
            return nil
        }
        let snr = object["snrDb"]?.number.flatMap { $0.isNaN ? nil : $0 }
        let synced = radeStatus ? (object["radeSynced"]?.flag ?? false) : nil
        let offset = radeStatus ? object["radeFreqOffsetHz"]?.number.flatMap { $0.isNaN ? nil : $0 } : nil
        let reason = radeReason ? object["radeReason"]?.text.flatMap { $0.isEmpty ? nil : $0 } : nil
        return RadeReception(callsign: object["lastRadeRxCallsign"]?.text ?? "", snrDb: snr, synced: synced,
                             offsetHz: offset, reason: reason)
    }

    private static func sliceId(_ object: MirrorObject) -> Int? {
        if let index = object["sliceIndex"]?.number {
            return Int(index)
        }
        guard object.key.hasPrefix("slice:") else {
            return nil
        }
        return Int(object.key.dropFirst("slice:".count))
    }
}

private extension MirrorValue {
    var number: Double? {
        switch self {
        case .double(let value):
            return value
        case .int(let value), .enumeration(let value):
            return Double(value)
        case .bool, .text:
            return nil
        }
    }

    var text: String? {
        if case .text(let value) = self {
            return value
        }
        return nil
    }

    var flag: Bool? {
        if case .bool(let value) = self {
            return value
        }
        return nil
    }
}
