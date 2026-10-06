// NereusSDR for iOS: taking transmit from the device that holds it, asked first as the desktop asks
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror
import os

/// Take transmit on this phone (R-IOS-02, R-IOS-03): `tx.take`, as the
/// desktop's pan TX pill and TX applet send it. While another device holds
/// transmit the phone first asks, from what it shows of the holder (its
/// name, whether it is on the air), and sends that holder's epoch and
/// whether it was keyed with the take, so the Core takes at once only if
/// nothing changed since. When something did, the Core asks its own
/// question, which shows in the same sheet with the Core's words.
///
/// A take started from a slice's TX button ends with `tx.setTxSlice` for
/// that slice once the Core has given this phone transmit, so one tap
/// moves transmit here and onto the slice. The take never keys: the phone
/// sends no `tx.key` for it, and PTT keys only when pressed.
///
/// The take is offered where the desktop offers it
/// (StationClient::transmitTakeAvailable): a Core that shares itself, takes
/// this phone's keys at `remoteTxVersion` 2, reports who holds transmit at
/// `txStateVersion` 2, and is not a receive-only station. Elsewhere the TX
/// button sends `tx.setTxSlice` alone, as before, and every Take transmit
/// button is drawn disabled with ``unavailableReason``.
///
/// A flag's TX badge (JJ's ruling of 2026-09-30, the desktop's at Core
/// trunk 01d797e56) takes what its slice needs, in order: on a slice
/// another device controls, the slice first (`slice.takeControl`, as Take
/// control sends it, with no question), then transmit as the TX button
/// takes it (asked first while another device holds it), then
/// `tx.setTxSlice`. A slice take never carries transmit, so these are
/// requests in sequence, each ended by its own answer; nothing keys.
@MainActor
final class TransmitTakeModel: ObservableObject {
    /// The phone's own question, before `tx.take` is sent: who holds
    /// transmit, as the phone shows it, and the epoch and keyed state the
    /// take carries.
    struct Asked: Equatable, Identifiable {
        let id: Int
        let holder: SeveralDevices.Holder
        let epoch: Int64
        let keyed: Bool
    }

    /// Where the phone's own question has got to.
    enum Answering: Equatable {
        case idle
        /// `tx.take` was sent; the Core has not answered.
        case taking
        /// The Core refused the take, in its words.
        case refused(String)
    }

    static let takeVerb = "tx.take"
    /// The capability and version that bring `tx.take` (link section 18.9).
    static let takeCapability = "remoteTxVersion"
    static let takeVersion: Int64 = 2
    /// The `txState` version that names the holder (StationClient::knowsTransmitHolder).
    static let holderCapability = "txStateVersion"
    static let holderVersion: Int64 = 2
    /// A receive-only station's refusal code (TxRefusal.h kStationReceiveOnly).
    static let receiveOnlyCode = "stationReceiveOnly"
    /// The Core's unkey handover, at its longest: the unkey-confirmed gate
    /// waits up to 2000 ms for receive, then stops the radio again and
    /// waits up to 2000 ms more (link document section 18.2, "Every change
    /// of holder ... is a transfer"). A take's answer arrives when that
    /// transfer ends (section 18.9, "The take").
    static let unkeyHandover: Duration = .milliseconds(2000 + 2000)
    /// How long a take waits for its answer: the usual answer's wait and
    /// the longest unkey handover on top, so a slow handover is not
    /// reported as the Core not answering.
    static let takeTimeout: Duration = SeveralDevicesClient.answerTimeout + unkeyHandover
    /// The desktop's words while transmit moves between devices
    /// (MultiDeviceController::askTakeTransmit).
    static let changingHandsText = "Transmit is changing hands. Try again in a moment."

    // Why Take transmit is greyed, in the phone's words.
    static let olderCoreText = "This Core can't hand transmit over from here. Updating the Core may help."
    static let notSharedText = "This Core isn't shared between devices, so there is no transmit to take over."
    static let receiveOnlyText = "This Core only listens, so there is no transmit to take."
    static let holderUnknownText = "The Core hasn't said who holds transmit."
    static let notConnectedText = "Connect to a Core to take transmit."

    /// The Core takes `tx.take` from this phone.
    @Published private(set) var available = false
    /// Why Take transmit is greyed; nil while ``available``.
    @Published private(set) var unavailableReason: String? = TransmitTakeModel.notConnectedText
    /// Another device, or the radio's own PTT, holds transmit and is on the air.
    @Published private(set) var holderOnAir = false
    /// The phone's own question, shown until answered or cancelled.
    @Published private(set) var asked: Asked?
    @Published private(set) var answering: Answering = .idle
    /// A take is on its way to the Core, with or without a question. Set
    /// as the take starts, before anything is sent, so a second tap
    /// finds it.
    @Published private(set) var inFlight = false
    /// The TX badge's take on a slice another device controls, while it is
    /// on its way; nil otherwise.
    @Published private(set) var badge: BadgeTake?

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let captureTakeSender: CommandClient.CaptureSender?
    private let slices: BandSlicesModel
    private let transmit: TransmitModel
    private let devices: SeveralDevicesClient?
    /// Waits out the holder's change after a badge's slice take: the
    /// clock, or the suite's own in the tests.
    var holderWait: (Duration) async -> Void
    /// The slice whose TX button began the take: it gets `tx.setTxSlice`
    /// once the take is accepted, by `tx.take` itself or by the Core's
    /// question about that `tx.take` (``takeCommandId``), never another's.
    private(set) var pendingSliceId: Int?
    /// What runs once the Core gave this phone transmit for the take on its
    /// way (VOX's arm); dropped with the take.
    private var pendingGrant: (() -> Void)?
    /// This phone's `tx.take` the Core holds for its question: its id,
    /// which that question names as `forCommandId`.
    private(set) var takeCommandId: Int64?
    /// The Core's question about ``takeCommandId`` is up.
    @Published private var coreAsking = false
    /// A question about the take is up: the phone's own or the Core's.
    var questionUp: Bool { asked != nil || coreAsking }
    private var questions = 0
    /// Moves on at every reset: an answer to a take from before it changes nothing.
    private var generation = 0
    private struct TakeIntent {
        let sender: CommandClient.CommandSender?
        let authority: CommandSendPermit
    }
    private var takeIntent: TakeIntent?
    #if DEBUG
    /// Parks the pending intent before CommandClient admission.
    var beforeTakeAdmissionForTesting: (@MainActor () async -> Void)?
    private(set) var lastTakeOperationForTesting: Task<Void, Never>?
    #endif
    /// Moves on whenever a badge take starts or ends: what an earlier one
    /// left waiting does nothing.
    private var badgeSerial = 0
    private var watches: Set<AnyCancellable> = []
    private static let logger = Logger(subsystem: "NereusSDR", category: "tx.take")

    init(mirror: MirrorStore, commands: CommandClient?, slices: BandSlicesModel, transmit: TransmitModel,
         devices: SeveralDevicesClient?, captureTakeSender: CommandClient.CaptureSender? = nil,
         holderWait: @escaping (Duration) async -> Void = { try? await Task.sleep(for: $0) }) {
        self.mirror = mirror
        self.commands = commands
        self.captureTakeSender = captureTakeSender
        self.slices = slices
        self.transmit = transmit
        self.devices = devices
        self.holderWait = holderWait
        mirror.$capabilities.sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }.store(in: &watches)
        transmit.$report.sink { [weak self] report in
            let onAir = report.heldElsewhere && report.keyed
            if self?.holderOnAir != onAir {
                self?.holderOnAir = onAir
            }
            // The badges follow who holds transmit, and a badge's take
            // goes on once the report is in place.
            self?.objectWillChange.send()
            Task { @MainActor in self?.continueBadge() }
        }.store(in: &watches)
        transmit.$permitted.removeDuplicates().sink { [weak self] _ in
            self?.objectWillChange.send()
        }.store(in: &watches)
        transmit.$permission.removeDuplicates().sink { [weak self] _ in
            self?.objectWillChange.send()
        }.store(in: &watches)
        slices.$taking.removeDuplicates().sink { [weak self] _ in
            self?.objectWillChange.send()
        }.store(in: &watches)
        devices?.$question.sink { [weak self] question in
            MainActor.assumeIsolated {
                self?.questionChanged(question)
            }
        }.store(in: &watches)
        // The slice's TX button takes transmit first where it must.
        slices.takeTransmitFirst = { [weak self] sliceId in
            self?.transmitOn(sliceId: sliceId) ?? false
        }
        refresh()
    }

    // MARK: Starting

    /// The TX button on one of this phone's slices, while the Core takes
    /// `tx.take`: begins the take, and returns true, unless this phone
    /// already holds transmit (the button then sends `tx.setTxSlice` alone).
    func transmitOn(sliceId: Int) -> Bool {
        guard available, !transmit.report.heldHere else {
            return false
        }
        begin(sliceId: sliceId)
        return true
    }

    /// Take transmit, from a slice's TX button (`sliceId`, `offered`
    /// false) or a refusal's or the TX panel's Take transmit (`offered`
    /// true). While another device holds transmit the phone asks first;
    /// with nobody holding it the take is sent at once, as there is nobody
    /// to take it from. A Take transmit the operator was offered always
    /// sends `tx.take`, even where this phone believes it holds transmit:
    /// the Core answers a holder's take accepted and changes nothing
    /// (link section 18.9), and one that no longer counts this phone as
    /// the holder asks or takes as usual.
    ///
    /// Returns true when the take started (asked, or sent): a refusal it
    /// answers may close. False leaves it up, with any reason this shows.
    /// `granted` runs once the Core gave this phone transmit for this take
    /// (at once when it already holds it); a take cancelled, refused or
    /// replaced runs nothing.
    @discardableResult
    func begin(sliceId: Int?, offered: Bool = false, granted then: (() -> Void)? = nil) -> Bool {
        guard available, !inFlight, asked == nil, !coreAsking else {
            return false
        }
        if let taking = badge, taking.stage != .transmit || taking.sliceId != sliceId || offered {
            // Another take replaces the badge's, as another request does on the desktop.
            endBadge()
        }
        let report = transmit.report
        if report.heldHere && !offered {
            if let sliceId {
                slices.sendTransmitSlice(sliceId)
            }
            then?()
            return true
        }
        if report.holderTransferring {
            slices.showTakeRefusal(Self.changingHandsText)
            return false
        }
        takeIntent?.authority.revoke()
        let intent = TakeIntent(sender: captureTakeSender?(), authority: CommandSendPermit())
        takeIntent = intent
        pendingSliceId = sliceId
        pendingGrant = then
        takeCommandId = nil
        answering = .idle
        guard report.heldElsewhere else {
            inFlight = true
            let generation = generation
            let operation = Task { await send(epoch: report.holderEpoch, keyed: report.keyed, asked: false, generation: generation, intent: intent) }
            #if DEBUG
            lastTakeOperationForTesting = operation
            #endif
            return true
        }
        questions += 1
        asked = Asked(id: questions, holder: shownHolder(report), epoch: report.holderEpoch, keyed: report.keyed)
        return true
    }

    // MARK: The phone's own question

    /// Take transmit (or Unkey and take over) on the phone's question:
    /// `tx.take` with the holder's epoch and keyed state as shown.
    func confirm() async {
        guard let shown = asked, let intent = takeIntent, answering != .taking, !inFlight else {
            return
        }
        answering = .taking
        inFlight = true
        await send(epoch: shown.epoch, keyed: shown.keyed, asked: true, generation: generation, intent: intent)
    }

    /// Cancel on the phone's question: nothing is sent, nothing changes.
    func cancel() {
        guard answering != .taking else {
            return
        }
        clearQuestion()
    }

    /// Close, after the Core refused the take.
    func close() {
        clearQuestion()
    }

    // MARK: The Core's question

    /// Confirm on the Core's `takeTransmit` question: `confirm.proceed`;
    /// accepted, the slice whose TX button began the take gets transmit,
    /// when the question is the one the Core asked about this phone's own
    /// `tx.take`.
    func proceedCore() async {
        guard let devices else {
            return
        }
        let owns = ownsQuestion(devices.question)
        let sliceId = owns ? pendingSliceId : nil
        let then = owns ? pendingGrant : nil
        let generation = generation
        guard await devices.proceed(timeout: Self.takeTimeout), generation == self.generation else {
            return
        }
        pendingSliceId = nil
        pendingGrant = nil
        takeCommandId = nil
        granted(sliceId, then: then)
    }

    /// Cancel on the Core's question: `confirm.cancel`, and nothing changes.
    func cancelCore() {
        forgetTake()
        devices?.cancel()
    }

    /// Close on the Core's question, after it refused the answer.
    func closeCore() {
        forgetTake()
        devices?.closeQuestion()
    }

    // MARK: Resets

    /// The session changed: anything asked, sent or waiting belongs to the
    /// session that was, so a lost link, a reconnect or another Core
    /// starts clean, and a late answer changes nothing.
    func sessionChanged(_ state: StationSession.State) {
        if state != .ready {
            reset()
        }
    }

    private func reset() {
        generation += 1
        asked = nil
        answering = .idle
        inFlight = false
        forgetTake()
        endBadge()
    }

    // MARK: The flag's TX badge (JJ's ruling of 2026-09-30)

    /// A TX badge's take on a slice another device controls: the slice,
    /// then the holder's change, then transmit.
    struct BadgeTake: Equatable {
        enum Stage: Equatable {
            /// `slice.takeControl` is on its way.
            case slice
            /// The slice is taken; the Core frees the former controller's
            /// transmit when that slice was its transmit slice, and the
            /// badge waits for the holder's change (``holderWaitTime`` at most).
            case holder
            /// Transmit: asked, on its way, or granted.
            case transmit
        }

        let sliceId: Int
        var stage: Stage
        /// The Core gave this phone transmit for this take (its own
        /// `tx.take`, or the Core's question about it): the slice becomes
        /// the transmit slice once this phone shows as the holder.
        var granted = false
    }

    /// What a flag's TX badge does now.
    enum BadgeOffer: Equatable {
        /// A tap makes the slice the transmit slice, as the TX button always has.
        case choose
        /// A tap takes what the slice needs; `hint` says what, in the desktop's words.
        case take(hint: String)
        /// Greyed: a tap shows `reason`, and nothing is sent.
        case held(reason: String)
    }

    /// The refusal codes a take answers: another device holds transmit, or
    /// is on the air with it (TxRefusal.h kOtherDeviceHolds, kHolderOnAir).
    static let holderRefusalCodes: Set<String> = ["otherDeviceHolds", "holderOnAir"]
    /// How long a badge waits after its slice take for the holder's change
    /// (MainWindow::txBadgeSliceAnswered, kTxBadgeHolderWaitMs).
    static let holderWaitTime: Duration = .milliseconds(1000)

    /// This phone may take transmit now: the Core takes `tx.take`, this
    /// phone does not hold it, and it may transmit, or only the holder's
    /// refusal stands in the way (MainWindow::applyTxBadgeOffer).
    var takesTransmit: Bool {
        let report = transmit.report
        guard available, !report.heldHere else {
            return false
        }
        if transmit.permitted {
            return true
        }
        return report.heldElsewhere && Self.holderRefusalCodes.contains(transmit.permission?.code ?? "")
    }

    /// What the TX badge on `entry`'s flag offers (MainWindow::applyTxBadgeOffer).
    func badgeOffer(_ entry: BandSlicesModel.Entry) -> BadgeOffer {
        if let pending = badgePending(entry.id) {
            return .held(reason: pending)
        }
        let holds = transmit.report.heldHere
        let takes = takesTransmit
        let holder = transmit.report.heldElsewhere ? holderName() : ""
        if entry.listening {
            // Case 3: a slice another device controls. On the air, the Core's words.
            if let refusal = entry.takeRefusal {
                return .held(reason: refusal)
            }
            if entry.access != nil, slices.canSelectTransmitSlice, holds || takes {
                return .take(hint: !holds && !holder.isEmpty ? Self.takeSliceFromText(holder) : Self.takeSliceText)
            }
            return .held(reason: BandGestureLayer.transmitReason(transmit, take: self) ?? entry.ownerLine ?? "")
        }
        if takes {
            // Case 2: this phone's slice, and another device holds transmit or nobody does.
            return .take(hint: holder.isEmpty ? Self.takeTransmitText : Self.takeTransmitFromText(holder))
        }
        if let reason = BandGestureLayer.transmitReason(transmit, take: self) {
            return .held(reason: reason)
        }
        // Case 1, and a Core that takes no take: the choice as before.
        return .choose
    }

    // The badge's words, the desktop's (MainWindow::applyTxBadgeOffer).
    static let takeSliceText = "Take control of this slice and make it the TX slice"
    static func takeSliceFromText(_ holder: String) -> String {
        "Take control of this slice, then take transmit from \(holder)"
    }
    static let takeTransmitText = "Take transmit and make this the TX slice"
    static func takeTransmitFromText(_ holder: String) -> String {
        "Take transmit from \(holder) and make this the TX slice"
    }

    /// A tap on a flag's TX badge: what ``badgeOffer(_:)`` offers. Case 1
    /// and case 2 are the TX button's own choice and take; case 3 takes the
    /// slice first, as Take control does, then goes on to transmit.
    func badgeTapped(_ sliceId: Int) {
        guard let entry = slices.entries.first(where: { $0.id == sliceId }) else {
            return
        }
        switch badgeOffer(entry) {
        case .held(let reason):
            slices.showReason(reason)
            return
        case .choose:
            slices.selectForTransmit(sliceId)
            return
        case .take:
            break
        }
        guard entry.listening else {
            slices.selectForTransmit(sliceId)
            return
        }
        // A new tap replaces a badge take still waiting.
        endBadge()
        badge = BadgeTake(sliceId: sliceId, stage: .slice)
        let serial = badgeSerial
        let sent = slices.takeControl(sliceId) { [weak self] accepted in
            self?.badgeSliceAnswered(accepted, serial: serial)
        }
        if !sent, serial == badgeSerial {
            endBadge()
        }
    }

    /// Why this flag's badge is held while a take on it is on its way:
    /// that take's own words.
    private func badgePending(_ sliceId: Int) -> String? {
        if slices.taking.contains(sliceId) || (badge?.sliceId == sliceId && badge?.stage == .slice) {
            return VfoFlagView.takingControlTitle
        }
        if badge?.sliceId == sliceId || (pendingSliceId == sliceId && (inFlight || asked != nil || coreAsking)) {
            return TakeTransmitSheet.busyTitle(onAir: holderOnAir)
        }
        return nil
    }

    /// The Core answered the badge's slice take (MainWindow::txBadgeSliceAnswered).
    private func badgeSliceAnswered(_ accepted: Bool, serial: Int) {
        guard serial == badgeSerial, var taking = badge, taking.stage == .slice else {
            return
        }
        guard accepted else {
            // The Core's refusal shows over the band; nothing else changes.
            endBadge()
            return
        }
        // The former controller transmitting on this slice loses transmit
        // with it (ruling Q8), and the Core's word of that follows its
        // answer: wait for it, so the question is asked only of a holder
        // that still holds.
        transmit.refresh()
        if holdsOnTheSlice(taking.sliceId) {
            taking.stage = .holder
            badge = taking
            Task { @MainActor [weak self] in
                await self?.holderWait(Self.holderWaitTime)
                // Past this, the take goes on with what the Core last said.
                guard let self, serial == self.badgeSerial, self.badge?.stage == .holder else {
                    return
                }
                self.badgeTransmit()
            }
            return
        }
        badgeTransmit()
    }

    /// Another device holds transmit on `sliceId`, as the Core last said.
    private func holdsOnTheSlice(_ sliceId: Int) -> Bool {
        guard transmit.report.heldElsewhere else {
            return false
        }
        switch mirror.object(TransmitModel.txStateKey)?["txSliceId"] {
        case .int(let txSlice)?, .enumeration(let txSlice)?:
            return txSlice == Int64(sliceId)
        default:
            return false
        }
    }

    /// The report moved on (MainWindow::continueTxBadgeTake): the holder's
    /// change ends the wait, a question no longer needed gives way to the
    /// take at once, and a granted take ends on its slice.
    private func continueBadge() {
        guard let taking = badge else {
            return
        }
        switch taking.stage {
        case .slice:
            return
        case .holder:
            if holdsOnTheSlice(taking.sliceId) {
                return
            }
            badgeTransmit()
        case .transmit:
            if asked != nil, answering == .idle, !inFlight, !transmit.report.heldElsewhere {
                // Nobody else holds transmit now: the question is not needed.
                asked = nil
                badgeTransmit()
                return
            }
            finishBadgeIfHeld()
        }
    }

    /// The badge's transmit step: nothing to take while this phone holds
    /// transmit; otherwise the TX button's take, asked first while another
    /// device holds it.
    private func badgeTransmit() {
        guard var taking = badge else {
            return
        }
        taking.stage = .transmit
        badge = taking
        transmit.refresh()
        if transmit.report.heldHere {
            taking.granted = true
            badge = taking
            finishBadgeIfHeld()
            return
        }
        if !begin(sliceId: taking.sliceId), badge?.granted != true {
            endBadge()
        }
    }

    /// The Core gave this phone transmit for the take `sliceId` began;
    /// `then` is what waited for it.
    private func granted(_ sliceId: Int?, then: (() -> Void)? = nil) {
        then?()
        guard let sliceId else {
            return
        }
        if var taking = badge, taking.sliceId == sliceId, taking.stage == .transmit {
            taking.granted = true
            badge = taking
            finishBadgeIfHeld()
            return
        }
        slices.sendTransmitSlice(sliceId)
    }

    /// Once this phone shows as the holder through the badge's own take,
    /// the slice becomes the transmit slice (MainWindow::finishTxBadgeTakeIfHeld).
    private func finishBadgeIfHeld() {
        guard let taking = badge, taking.stage == .transmit, taking.granted, transmit.report.heldHere else {
            return
        }
        endBadge()
        let serial = badgeSerial
        // On the next turn: the Core binds a new holder's transmit slice as
        // the take completes, and the choice made here comes after it.
        Task { @MainActor [weak self] in
            guard let self, serial == self.badgeSerial, self.transmit.report.heldHere else {
                return
            }
            self.slices.sendTransmitSlice(taking.sliceId, taken: true)
        }
    }

    /// The badge's take is over, or replaced: nothing it waits on does anything now.
    private func endBadge() {
        badgeSerial += 1
        if badge != nil {
            badge = nil
        }
    }

    /// The holder as the badge names it: the radio's own PTT as Radio, the
    /// Core's own position by the desktop hosting it, else its short name.
    private func holderName() -> String {
        let report = transmit.report
        var holderId = ""
        if case .text(let id)? = mirror.object(TransmitModel.txStateKey)?["holderDeviceId"] {
            holderId = id
        }
        let devices = SeveralDevices.connectedDevices(in: mirror)
        if report.holderSource != TransmitStateReport.radioPttSource, holderId == SliceAccess.stationDeviceId {
            return SliceAccess.deviceWords(holderId, devices: devices, short: true)
        }
        let label = report.holderLabel
        return label.isEmpty ? SliceAccess.deviceWords(holderId, devices: devices, short: true) : label
    }

    // MARK: Inside

    private func clearQuestion() {
        asked = nil
        answering = .idle
        forgetTake()
    }

    /// The take is over: answered, cancelled, refused or replaced. A
    /// badge's take that did not get transmit ends with it; one the Core
    /// granted waits only for this phone to show as the holder.
    private func forgetTake() {
        takeIntent?.authority.revoke()
        takeIntent = nil
        pendingSliceId = nil
        pendingGrant = nil
        takeCommandId = nil
        coreAsking = false
        if let taking = badge, !taking.granted {
            endBadge()
        }
    }

    /// The Core's question is the one about this phone's own `tx.take`.
    private func ownsQuestion(_ question: SeveralDevices.Question?) -> Bool {
        guard let question, question.kind == .takeTransmit, let takeCommandId else {
            return false
        }
        return question.forCommandId == takeCommandId
    }

    /// The Core's open question changed: its question about this phone's
    /// `tx.take` came up, or closed by any route (answered, cancelled,
    /// replaced, expired or dropped), which ends the take's slice.
    private func questionChanged(_ question: SeveralDevices.Question?) {
        guard takeCommandId != nil else {
            return
        }
        if ownsQuestion(question) {
            coreAsking = true
        } else if coreAsking || question != nil {
            forgetTake()
        }
    }

    private func ownsTake(_ intent: TakeIntent, generation: Int) -> Bool {
        generation == self.generation && takeIntent?.authority === intent.authority && !intent.authority.isRevoked
    }

    private func send(epoch: Int64, keyed: Bool, asked shown: Bool, generation: Int, intent: TakeIntent) async {
        guard ownsTake(intent, generation: generation) else { return }
        guard let commands else {
            inFlight = false
            return
        }
        let sliceId = pendingSliceId
        let then = pendingGrant
        let result: CommandResult
        #if DEBUG
        await beforeTakeAdmissionForTesting?()
        #endif
        guard ownsTake(intent, generation: generation) else { return }
        do {
            let arguments = [
                CommandArgument(name: "holderEpoch", value: .int(epoch)),
                CommandArgument(name: "shownKeyed", value: .bool(keyed)),
            ]
            let started: CommandClient.Started
            if let sender = intent.sender {
                started = await commands.startBound(Self.takeVerb, arguments: arguments, copies: 1, timeout: Self.takeTimeout,
                                                    sender: sender, stillAllowed: { !intent.authority.isRevoked },
                                                    authority: intent.authority)
            } else {
                started = await commands.start(Self.takeVerb, arguments: arguments, copies: 1,
                                               timeout: Self.takeTimeout, authority: intent.authority)
            }
            guard ownsTake(intent, generation: generation) else { return }
            takeCommandId = Int64(started.commandId)
            await started.sent()
            result = try await started.result()
        } catch {
            guard ownsTake(intent, generation: generation) else {
                return
            }
            inFlight = false
            Self.logger.info("\(Self.takeVerb, privacy: .public) did not reach the Core: \(String(describing: error), privacy: .public)")
            refused(SeveralDevicesClient.noAnswerText, shown: shown)
            return
        }
        guard ownsTake(intent, generation: generation) else {
            return
        }
        inFlight = false
        if result.accepted {
            asked = nil
            answering = .idle
            granted(sliceId, then: then)
            forgetTake()
            return
        }
        if SeveralDevices.waitsForConfirmation(result) {
            // Something changed since the phone asked: the Core asks its
            // own question about this take, which carries it on. It is
            // sent after this answer (section 7.5) and names this take's
            // id, which ``questionChanged(_:)`` matches.
            asked = nil
            answering = .idle
            return
        }
        Self.logger.info("The Core refused the take: \(result.reason, privacy: .private)")
        refused(result.reason, shown: shown)
    }

    private func refused(_ reason: String, shown: Bool) {
        takeCommandId = nil
        // A refused take changes nothing: a badge's take ends with it.
        if badge != nil {
            endBadge()
        }
        if shown, asked != nil {
            answering = .refused(reason.isEmpty ? BandSlicesModel.refusedText : reason)
        } else {
            forgetTake()
            slices.showTakeRefusal(reason)
        }
    }

    /// The holder as this phone shows it: `txState`'s holder, with its
    /// kind and times from `connectedDevices`; the radio's own PTT is Radio.
    private func shownHolder(_ report: TransmitStateReport) -> SeveralDevices.Holder {
        let state = mirror.object(TransmitModel.txStateKey)
        var holderId = ""
        if case .text(let id)? = state?["holderDeviceId"] {
            holderId = id
        }
        var kind = ""
        if case .text(let text)? = state?["holderKind"] {
            kind = text
        }
        let radio = report.holderSource == TransmitStateReport.radioPttSource
        var holder = SeveralDevices.Holder(
            deviceId: holderId,
            name: radio ? TransmitStateReport.radioLabel : report.holderName,
            shortName: radio ? TransmitStateReport.radioLabel : report.holderShortName,
            kind: radio ? "station" : kind, radioPtt: radio,
            state: report.holderAway ? .away : report.keyed ? .transmitting : .listening, keyed: report.keyed,
            transmittingForSeconds: report.keyed ? transmit.keyedForSeconds : 0)
        if let device = SeveralDevices.connectedDevices(in: mirror).first(where: { $0.deviceId == holderId }) {
            if holder.kind.isEmpty {
                holder.kind = device.kind
            }
            holder.connectedForSeconds = device.connectedForSeconds
            holder.lastActivitySeconds = device.lastActivitySeconds
            holder.awayForSeconds = device.awayForSeconds
        }
        return holder
    }

    /// Why the take is not offered, in the order the desktop's gate reads
    /// it (StationClient::transmitTakeAvailable and knowsTransmitHolder);
    /// nil when it is.
    private func reasonUnavailable() -> String? {
        guard commands != nil else {
            return Self.notConnectedText
        }
        if mirror.capabilityVersion(Self.takeCapability) < Self.takeVersion
            || mirror.capabilityVersion(Self.holderCapability) < Self.holderVersion {
            return Self.olderCoreText
        }
        if !SeveralDevices.available(in: mirror) {
            return Self.notSharedText
        }
        if case .text(let code)? = mirror.capabilities["txRefusalCode"], code == Self.receiveOnlyCode {
            return Self.receiveOnlyText
        }
        if mirror.object(TransmitModel.txStateKey) == nil {
            return Self.holderUnknownText
        }
        return nil
    }

    private func refresh() {
        let reason = reasonUnavailable()
        if unavailableReason != reason {
            unavailableReason = reason
        }
        let next = reason == nil
        if available != next {
            available = next
            if !next {
                // Nothing asked or waiting outlives the take being offered.
                reset()
            }
        }
    }
}
