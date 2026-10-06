// NereusSDR for iOS: the PTT's state machine: keying, releasing, the keepalive and what the Core says of transmit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The phone's PTT (R-IOS-13; link document sections 18.6 to 18.8; spec
/// section 5.1 items 12 and 13). A toggle: one tap keys, a second unkeys.
///
/// One key: the Core keeps one key per device (`RemoteKeying`): PTT, TUNE
/// and two-tone are that one key, and the Core answers a key while this
/// device's TUNE is on with TUNE's epoch and ends TUNE on its next unkey.
/// So the phone keeps one key too, of one kind, with one epoch: while it
/// is on, a tap on PTT (or Stop) releases it, whichever kind it is, and
/// nothing else starts until it has ended.
///
/// Order: every keying verb goes through one queue, so a release always
/// reaches the Core after its key's copies, and a key the operator has
/// already released, or that belongs to a link that has gone, is never
/// sent. A key the Core accepts that is no longer wanted is released with
/// its epoch. Each key and release goes three times as one command.
///
/// Keying: a tap asks the microphone to start, so the Core's buffer is
/// filling when the key arrives, then sends `tx.key {trigger}`. A start
/// that fails, or that has not ended within ``microphoneStartDeadline``,
/// sends nothing and PTT shows why, so a release queued behind it never
/// waits on it. The snapshot's ``Snapshot/microphoneWanted`` says when the
/// microphone should run: while the PTT's key is on or on its way, and
/// while this device's VOX is armed. TUNE sends
/// `tx.tune {on}`, two-tone `tx.twoTone {on}` and the Tuner Genius's TUNE
/// `tx.tunerTune {on}` (`remoteTxVersion` 2). A release sends
/// `tx.unkey {epoch}` for PTT, with 4294967295 before the key's answer,
/// and TUNE, two-tone or the tuner's tune off for those.
///
/// The keepalive: while the key is on or on its way, or this device's VOX
/// is armed, and the link is up, it sends `tx.keepalive {sequence, epoch}`
/// once every 100 ms, the sequence never restarting within a session, the
/// epoch the key's, or 4294967295 before its answer. A release stops it at
/// once.
///
/// What the Core says: a `txState` stop whose `stopEpoch` is this device's
/// key's or newer ends the key here too, without an unkey, even when the
/// stop arrives before the key's answer; so does the radio going off the
/// air after it was on for this key. Transmit held by another device, or
/// by the radio's own PTT, is `.heldElsewhere`: a tap sends nothing, and a
/// key this device had on when transmit was taken simply ends here. While
/// transmit changes hands PTT waits.
///
/// The link: when it goes while a key is on, PTT says the Core stops
/// transmitting on its own; when the session is back, PTT is idle and never
/// keys by itself: the next key is the operator's next tap.
public actor PttController {
    /// Where the PTT is.
    public enum State: Equatable, Sendable {
        /// Nothing of this device's is keyed: PTT reads Tap.
        case idle
        /// The key has been sent, or is on its way; its answer has not come.
        case keying
        /// On the air since `since`.
        case keyed(since: ContinuousClock.Instant)
        /// The Core is ending the transmission (`txState.txEnding`): "TX ending".
        case ending
        /// The release has been sent; its answer has not come.
        case unkeying
        /// The Core refused the key, or the release.
        case refused(TxRefusalInfo)
        /// The link went while a key was on; the Core stops on its own.
        case linkLost
        /// Another device, or the radio's own PTT (`device` "Radio"), holds
        /// transmit, and is on the air when `onAir`.
        case heldElsewhere(device: String, onAir: Bool)
        /// Transmit is changing hands: every key is refused until it settles.
        case waiting

        /// Keyed, whenever it began.
        public var isKeyed: Bool {
            if case .keyed = self {
                return true
            }
            return false
        }
    }

    /// What the key is: the PTT (and MOX), TUNE, the two-tone test, or the
    /// Tuner Genius's autotune (its carrier keys as this device once the
    /// amplifier is in standby).
    public enum KeyKind: Equatable, Sendable {
        case ptt
        case tune
        case twoTone
        case tunerTune
    }

    /// Everything the screens show of the PTT.
    public struct Snapshot: Equatable, Sendable {
        public var state: State = .idle
        public var logicalSessionOwner: UInt64?
        /// The kind of this device's key while it is on, on its way or ending.
        public var keyKind: KeyKind?
        /// The newest stop of this device's key by the Core, until the next tap.
        public var stop: TransmitStopNotice?
        /// The keepalive is being sent.
        public var keepaliveRunning = false
        /// The microphone should run: the PTT's key is on or on its way, or
        /// this device's VOX is armed, and the link is up. When it falls the
        /// microphone stops.
        public var microphoneWanted = false
        /// This device's VOX is armed (its write of `transmit.voxEnabled`
        /// on was accepted) and the link is up.
        public var voxArmed = false

        public init() {}

        /// This device's key is on or on its way: the screen stays awake,
        /// the keyed view shows and Disconnect is not offered.
        public var transmitting: Bool {
            switch state {
            case .keying, .keyed, .unkeying, .ending:
                return true
            case .idle, .refused, .linkLost, .heldElsewhere, .waiting:
                return false
            }
        }

        /// TUNE of this device's is on or on its way.
        public var tuning: Bool {
            keyKind == .tune && transmitting
        }

        /// Two-tone of this device's is on or on its way.
        public var twoTone: Bool {
            keyKind == .twoTone && transmitting
        }

        /// The Tuner Genius's autotune of this device's is on or on its way.
        public var tunerTuning: Bool {
            keyKind == .tunerTune && transmitting
        }

        /// The PTT's key (not TUNE or two-tone) is on or on its way.
        public var pttKeyed: Bool {
            keyKind == .ptt && transmitting
        }
    }

    /// How often the keepalive goes (section 18.7).
    public static let keepaliveInterval: Duration = .milliseconds(100)
    /// Each key and unkey goes this many times as one command (section 18.6).
    public static let copies = 3
    /// The epoch a keepalive and a release carry before a key's answer comes.
    public static let unansweredEpoch: Int64 = 4_294_967_295
    /// How long a command with no answer waits for the link's own news
    /// before the PTT decides the link is still up.
    public static let lostAnswerGrace: Duration = .milliseconds(500)
    /// How long this device's accepted tuner tune waits for its carrier
    /// before it ends here through its own off. The Core keys a device's
    /// cycle once its amplifier reports standby, waiting at most 1.5 s for
    /// that before it ends the cycle (src/models/RadioModel.cpp:30698-30720,
    /// continueTgxlAutotuneAfterStandby at :30759), and some of its ends
    /// send no notice (src/core/session/RemoteKeying.cpp:155-160); the
    /// rest of this wait is for the link.
    public static let tunerCarrierDeadline: Duration = .seconds(5)
    /// How long a key waits for the microphone to start before it is
    /// given up, unsent: an audio engine starts in well under a second.
    public static let microphoneStartDeadline: Duration = .seconds(2)
    /// PTT's words when the microphone did not start in time.
    public static let microphoneDidNotStartText = "This phone's microphone did not start. Try again."
    /// The refusal code a failed microphone start shows under, the Core's
    /// own for a key its microphone line could not carry (section 18.3).
    public static let microphoneRefusalCode = "micNotReady"
    /// The stop reason ``microphoneLost(_:)`` shows under: this phone's own,
    /// never one the Core sends.
    public static let microphoneLostReason = "phoneMicrophoneLost"
    /// The words PTT shows when the link goes while a key is on (spec
    /// section 5.1 item 13).
    public static let linkLostText = "The Core stops transmitting on its own when the link goes."
    public static let unresolvedOffText = "Transmit stop was not confirmed. Reconnect to this Core before transmitting again."

    private static let logger = Logger(subsystem: "NereusSDR", category: "tx.ptt")

    /// Every change of the PTT's snapshot, newest kept.
    public nonisolated let snapshots: AsyncStream<Snapshot>
    private nonisolated let snapshotSink: AsyncStream<Snapshot>.Continuation

    private let commands: any TransmitCommandSending
    private let clock: any LinkClock
    private let now: @Sendable () -> ContinuousClock.Instant
    private let startMicrophone: @Sendable () async -> MicrophoneStart

    public private(set) var snapshot = Snapshot()
    private var linkUp = false
    /// Moves with each link change, so nothing queued in one session is
    /// sent in the next.
    private var session = 0
    private var logicalSessionOwner: UInt64?
    private var nextOff = 0
    private var unresolvedOffs: Set<Int> = []
    private var failedOff = false
    /// Moves with each key and release, so a late answer to an earlier one
    /// changes nothing, and a key released before it went is never sent.
    private var generation = 0
    /// The key's epoch, once its answer came.
    private var keyEpoch: Int64?
    /// The radio was seen on the air for this key.
    private var sawKeyed = false
    /// A stop the Core reported while the key's answer was on its way.
    private var earlyStop: (epoch: Int64, notice: TransmitStopNotice)?
    /// The last release was this device's own, for the "TX ending" tail.
    private var releasedOwn = false
    /// The newest report's serial (``update(_:serial:)``).
    private var reportSerial: UInt64 = 0
    /// Serials for this device's own stop notices: below zero, so they
    /// never meet the Core's `stopSerial`.
    private var localStopSerial: Int64 = 0
    private var voxArmed = false
    /// The last keepalive's sequence in this session.
    private var sequence: Int64 = 0
    private var keepaliveTimer: (any LinkTimer)?
    private var keepaliveGridStartMilliseconds: Int64 = 0
    private var keepaliveToken = 0
    private var heartbeatGate = TransmitHeartbeatGate()
    private var commandAuthority = CommandSendPermit()
    /// Idle sleep expiry owns this gate until its old session leaves or the
    /// choice is cancelled. A key that reaches this actor first wins instead.
    private var idleSleepRetirement: (owner: UInt64, authority: CommandSendPermit)?
    private var graceTimer: (any LinkTimer)?
    /// A tuner tune's wait for its carrier (``tunerCarrierDeadline``).
    private var carrierTimer: (any LinkTimer)?
    /// The newest `stopSerial` seen in this session.
    private var lastStopSerial: Int64?
    /// The epoch of the key that was on when the link went (4294967295 when
    /// its answer had not come), so the stop the Core reports on the next
    /// session's first transmit state is shown, and only that one.
    private var keyAtLinkLoss: Int64?
    /// The next transmit state is the first of a new session.
    private var firstReportOfSession = true
    private var report = TransmitStateReport()
    /// The keying verbs' queue: each waits for the one before to be sent.
    private var sendTail: Task<Void, Never>?
    /// Commands still waiting for their answers (the tests wait on them).
    private var inFlight: [Int: Task<Void, Never>] = [:]
    private var nextTaskId = 0

    public init(commands: any TransmitCommandSending, clock: any LinkClock = SystemLinkClock(),
                now: @escaping @Sendable () -> ContinuousClock.Instant = { ContinuousClock.now },
                startMicrophone: @escaping @Sendable () async -> MicrophoneStart = { .started }) {
        self.commands = commands
        self.clock = clock
        self.now = now
        self.startMicrophone = startMicrophone
        (snapshots, snapshotSink) = AsyncStream.makeStream(of: Snapshot.self, bufferingPolicy: .bufferingNewest(1))
    }

    deinit {
        keepaliveTimer?.cancel()
        graceTimer?.cancel()
        carrierTimer?.cancel()
        snapshotSink.finish()
    }

    /// The PTT's state now.
    public var state: State { snapshot.state }

    /// Keepalives sent in this session.
    public var keepaliveSequence: Int64 { sequence }

    // MARK: The operator

    /// The PTT, or the TX panel's MOX: keys when nothing is keyed, releases
    /// this device's key (PTT, TUNE or two-tone) when it is on or on its
    /// way, and sends nothing while another device holds transmit, transmit
    /// changes hands or a release is on its way.
    public func tap(trigger: String = TransmitVerb.screenTrigger) {
        guard linkUp else {
            return
        }
        switch snapshot.state {
        case .idle, .refused:
            start(.ptt, trigger: trigger)
        case .keying, .keyed:
            release()
        case .unkeying, .ending, .linkLost, .heldElsewhere, .waiting:
            break
        }
    }

    /// TUNE on or off (the TX panel's TUNE). On starts only when nothing of
    /// this device's is keyed; off ends only this device's TUNE.
    public func setTune(_ on: Bool) {
        setKind(.tune, on: on)
    }

    /// The TX panel's TUNE button: ends this device's TUNE when it is on or
    /// on its way, else starts it. It reads the PTT's own state, never a
    /// screen's copy that may lag.
    public func toggleTune() {
        if snapshot.tuning {
            stopAll()
        } else {
            setTune(true)
        }
    }

    /// Two-tone on or off, as TUNE.
    public func setTwoTone(_ on: Bool) {
        setKind(.twoTone, on: on)
    }

    /// The Tuner Genius's TUNE (`tx.tunerTune`): ends this device's tuner
    /// tune when it is on or on its way, else starts it when nothing of
    /// this device's is keyed, as the TX panel's TUNE does.
    public func toggleTunerTune() {
        if snapshot.tunerTuning {
            stopAll()
        } else {
            setKind(.tunerTune, on: true)
        }
    }

    /// The Core told this device its accepted `tx.tunerTune` ended without
    /// keying (the `tuneEnded` notice; RemoteKeying.cpp:148-162,
    /// StationServer.cpp:2719-2729): the tune ends here through its own
    /// off, as a tap would end it, so a notice that crossed a newer cycle
    /// can never leave that cycle running unwatched; for a cycle already
    /// over the Core accepts the off and changes nothing. Nothing else of
    /// this device's is touched, nor a tuner tune whose carrier was seen on
    /// the air (the Core sends the notice only for a cycle that never keyed).
    public func tunerTuneEndedUnkeyed() {
        guard snapshot.keyKind == .tunerTune, !sawKeyed else {
            return
        }
        stopAll()
    }

    /// Stop, on the TX pill, and the phone being locked: ends this
    /// device's key, whatever kind it is.
    public func stopAll() {
        switch snapshot.state {
        case .keying, .keyed:
            release()
        default:
            break
        }
    }

    /// Async stop intents retain their logical owner through actor admission.
    public func stopAll(owner: UInt64?, authority: CommandSendPermit) {
        guard owner == logicalSessionOwner, !authority.isRevoked else { return }
        switch snapshot.state {
        case .keying, .keyed:
            release(authority: authority)
        default:
            break
        }
    }

    /// This device's microphone stopped by itself while it was wanted (the
    /// phone's input could not be built again after a route change): a key
    /// on or on its way is released as ``stopAll()`` releases it, and the
    /// screen shows `text` as a stop until the next key or a dismiss.
    /// Nothing here keys. With the link down it only stops: the screen
    /// says the link went, and the Core stops transmitting on its own then.
    public func microphoneLost(_ text: String) {
        stopAll()
        guard linkUp else {
            return
        }
        localStopSerial -= 1
        snapshot.stop = TransmitStopNotice(reason: Self.microphoneLostReason, text: text, serial: localStopSerial)
        changed()
    }

    /// A delayed microphone loss cannot stop or notify a replacement owner.
    public func microphoneLost(_ text: String, owner: UInt64?, authority: CommandSendPermit) {
        guard owner == logicalSessionOwner, !authority.isRevoked else { return }
        stopAll(owner: owner, authority: authority)
        guard linkUp else { return }
        localStopSerial -= 1
        snapshot.stop = TransmitStopNotice(reason: Self.microphoneLostReason, text: text, serial: localStopSerial)
        changed()
    }

    #if DEBUG
    private var beforeMicrophoneCloseAdmissionForTesting: (@Sendable () async -> Void)?

    public func setMicrophoneCloseAdmissionForTesting(_ barrier: (@Sendable () async -> Void)?) {
        beforeMicrophoneCloseAdmissionForTesting = barrier
    }
    #endif

    /// The Core closed this phone's microphone line's track while the link
    /// and the media connection stay up: a PTT key on or on its way carries
    /// a microphone the Core no longer hears, so it is released as a tap
    /// releases it, and the screen shows `text` as a stop until the next key
    /// or a dismiss. TUNE, two-tone and the tuner's tune carry no microphone
    /// and are left on; with nothing keyed nothing changes. Returns whether
    /// a key was released. Nothing here keys.
    @discardableResult
    public func microphoneLineClosed(_ text: String) async -> Bool {
        #if DEBUG
        if let barrier = beforeMicrophoneCloseAdmissionForTesting { await barrier() }
        #endif
        return closeMicrophoneLine(text, authority: nil)
    }

    /// A buffered close and its release remain bound to their source owner
    /// and intent authority through admission and final transport handoff.
    @discardableResult
    public func microphoneLineClosed(_ text: String, owner: UInt64?, authority: CommandSendPermit) async -> Bool {
        #if DEBUG
        if let barrier = beforeMicrophoneCloseAdmissionForTesting { await barrier() }
        #endif
        guard owner == logicalSessionOwner, !authority.isRevoked else { return false }
        return closeMicrophoneLine(text, authority: authority)
    }

    private func closeMicrophoneLine(_ text: String, authority: CommandSendPermit?) -> Bool {
        guard !(authority?.isRevoked ?? false), linkUp, snapshot.keyKind == .ptt else {
            return false
        }
        switch snapshot.state {
        case .keying, .keyed:
            break
        default:
            return false
        }
        guard release(authority: authority), !(authority?.isRevoked ?? false) else { return false }
        localStopSerial -= 1
        snapshot.stop = TransmitStopNotice(reason: Self.microphoneLostReason, text: text, serial: localStopSerial)
        changed()
        return true
    }

    /// This device armed VOX (its write of `transmit.voxEnabled` on was
    /// accepted), or it went off: while armed the keepalive runs.
    public func setVoxArmed(_ armed: Bool) {
        if armed { pruneCancelledIdleSleepRetirement() }
        if armed, idleSleepRetirement != nil { return }
        let next = armed && linkUp
        guard next != voxArmed else {
            return
        }
        voxArmed = next
        changed()
    }

    /// Async app completions retain the logical owner across actor admission.
    public func setVoxArmed(_ armed: Bool, owner: UInt64?, authority: CommandSendPermit? = nil) {
        guard owner == logicalSessionOwner, !(authority?.isRevoked ?? false) else { return }
        setVoxArmed(armed)
    }

    /// The actor's idle state is the admission point for sleep expiry.
    /// PTT, TUNE, two-tone and a current VOX key all make it wait.
    public func beginIdleSleepRetirement(owner: UInt64, authority: CommandSendPermit) -> Bool {
        pruneCancelledIdleSleepRetirement()
        guard logicalSessionOwner == owner, linkUp, !authority.isRevoked,
              !snapshot.transmitting, !(report.keyed && report.heldHere),
              idleSleepRetirement == nil else { return false }
        idleSleepRetirement = (owner, authority)
        return true
    }

    /// A delayed old completion cannot release a replacement's gate.
    public func endIdleSleepRetirement(owner: UInt64, authority: CommandSendPermit) {
        guard idleSleepRetirement?.owner == owner,
              idleSleepRetirement?.authority === authority else { return }
        idleSleepRetirement = nil
    }

    public func ownsIdleSleepRetirement(owner: UInt64, authority: CommandSendPermit) -> Bool {
        logicalSessionOwner == owner && linkUp && idleSleepRetirement?.owner == owner
            && idleSleepRetirement?.authority === authority && !authority.isRevoked
            && !snapshot.transmitting && !(report.keyed && report.heldHere)
    }

    /// VOX arming checks this before starting the microphone. Its bound
    /// property write still checks authority at the final transport handoff.
    public func mayArmVox(owner: UInt64?, authority: CommandSendPermit) -> Bool {
        pruneCancelledIdleSleepRetirement()
        return owner == logicalSessionOwner && linkUp && idleSleepRetirement == nil && !authority.isRevoked
    }

    private func pruneCancelledIdleSleepRetirement() {
        if idleSleepRetirement?.authority.isRevoked == true {
            idleSleepRetirement = nil
        }
    }

    /// Clears the refusal or the stop notice the screen shows.
    public func dismissNotice() {
        var dirty = false
        if snapshot.stop != nil {
            snapshot.stop = nil
            dirty = true
        }
        if case .refused = snapshot.state {
            if unresolvedOffs.isEmpty, !failedOff {
                snapshot.state = .idle
                followHolder(report)
                dirty = true
            }
        }
        if dirty {
            changed()
        }
    }

    // MARK: The link and the Core

    /// The session is ready (`up`) or not. A new session starts idle, its
    /// keepalive sequence from 1, and keys nothing by itself; nothing
    /// queued before is sent in it.
    public func linkChanged(up: Bool) {
        guard up != linkUp else {
            return
        }
        linkUp = up
        idleSleepRetirement?.authority.revoke()
        idleSleepRetirement = nil
        commandAuthority.revoke()
        commandAuthority = CommandSendPermit()
        heartbeatGate.retire()
        heartbeatGate = TransmitHeartbeatGate()
        session += 1
        generation += 1
        graceTimer?.cancel()
        graceTimer = nil
        if up {
            sequence = 0
            snapshot.state = .idle
            snapshot.keyKind = nil
            if !unresolvedOffs.isEmpty || failedOff {
                snapshot.state = .refused(TxRefusalInfo(reason: Self.unresolvedOffText,
                                                        code: "reconnectRequired"))
            }
            // The snapshot's transmit state may already say who holds it.
            followHolder(report)
        } else {
            if snapshot.transmitting {
                keyAtLinkLoss = keyEpoch ?? Self.unansweredEpoch
                snapshot.state = .linkLost
            } else if case .linkLost = snapshot.state {
                // Still lost.
            } else {
                snapshot.state = .idle
            }
            snapshot.keyKind = nil
            keyEpoch = nil
            earlyStop = nil
            // The Core disarms a device's VOX when its session ends.
            voxArmed = false
            firstReportOfSession = true
        }
        changed()
    }

    /// Called with AppModel's logical owner, which stays fixed through a
    /// physical path upgrade. Only a replacement owner clears an off fence.
    public func logicalSessionChanged(_ owner: UInt64) {
        guard logicalSessionOwner != owner else { return }
        logicalSessionOwner = owner
        idleSleepRetirement?.authority.revoke()
        idleSleepRetirement = nil
        commandAuthority.revoke()
        commandAuthority = CommandSendPermit()
        heartbeatGate.retire()
        heartbeatGate = TransmitHeartbeatGate()
        session += 1
        generation += 1
        linkUp = false
        keepaliveTimer?.cancel()
        keepaliveTimer = nil
        keepaliveToken += 1
        graceTimer?.cancel()
        graceTimer = nil
        unresolvedOffs.removeAll()
        failedOff = false
        snapshot.state = .idle
        snapshot.keyKind = nil
        keyEpoch = nil
        earlyStop = nil
        voxArmed = false
        changed()
    }

    /// The Core's transmit state, numbered in the order this device read
    /// it: each report reaches the actor on a task of its own, so an older
    /// one can land after a newer one, and then it changes nothing.
    public func update(_ next: TransmitStateReport, serial: UInt64) {
        guard serial > reportSerial else {
            return
        }
        reportSerial = serial
        update(next)
    }

    /// The Core's transmit state changed.
    public func update(_ next: TransmitStateReport) {
        let previous = report
        report = next
        noteStop(next)
        followKey(next)
        followHolder(next)
        if next.txEnding {
            if releasedOwn, snapshot.state == .unkeying || snapshot.state == .idle, previous.keyed || next.keyed {
                snapshot.state = .ending
            }
        } else if snapshot.state == .ending {
            snapshot.state = .idle
            snapshot.keyKind = nil
        }
        changed()
    }

    /// Waits until every command sent so far has its answer (the tests).
    public func settle() async {
        while let task = inFlight.values.first {
            await task.value
        }
    }

    // MARK: Keying

    /// TUNE and two-tone may start: nobody else holds transmit, and it is
    /// not changing hands.
    private var mayStart: Bool {
        !report.heldElsewhere && !report.holderTransferring
    }

    private func setKind(_ kind: KeyKind, on: Bool) {
        guard linkUp else {
            return
        }
        if on {
            switch snapshot.state {
            case .idle, .refused:
                guard mayStart else {
                    return
                }
                start(kind, trigger: TransmitVerb.screenTrigger)
            default:
                break
            }
        } else if snapshot.keyKind == kind {
            stopAll()
        }
    }

    private func start(_ kind: KeyKind, trigger: String) {
        pruneCancelledIdleSleepRetirement()
        guard idleSleepRetirement == nil else { return }
        guard unresolvedOffs.isEmpty, !failedOff else {
            snapshot.state = .refused(TxRefusalInfo(reason: Self.unresolvedOffText, code: "reconnectRequired"))
            changed()
            return
        }
        generation += 1
        let mine = generation
        snapshot.state = .keying
        snapshot.keyKind = kind
        snapshot.stop = nil
        keyEpoch = nil
        sawKeyed = false
        earlyStop = nil
        releasedOwn = false
        changed()
        let verb: TransmitVerb
        switch kind {
        case .ptt:
            verb = .key(trigger: trigger)
        case .tune:
            verb = .tune(on: true)
        case .twoTone:
            verb = .twoTone(on: true)
        case .tunerTune:
            verb = .tunerTune(on: true)
        }
        let keySession = session
        enqueue(verb, generation: mine, microphone: kind == .ptt) { controller, answer in
            await controller.settleKey(answer, generation: mine, session: keySession, kind: kind)
        }
    }

    private func settleKey(_ answer: TransmitAnswer, generation mine: Int, session keySession: Int,
                           kind: KeyKind) {
        guard keySession == session else { return }
        guard mine == generation, snapshot.state == .keying else {
            // No longer wanted (released, stopped, taken or its link gone):
            // a key the Core took is released with its own epoch. A tuner
            // tune is ended with its own off: an unkey leaves a cycle that
            // waits for the amplifier to key later (RemoteKeying.cpp:654-668).
            if case .accepted(let epoch) = answer, linkUp {
                enqueueOff(kind == .tunerTune ? .tunerTune(on: false) : .unkey(epoch: Self.answered(epoch)))
            }
            return
        }
        switch answer {
        case .accepted(let epoch):
            let epoch = Self.answered(epoch)
            keyEpoch = epoch
            if let stop = earlyStop, epoch <= stop.epoch {
                // The Core stopped it before its answer came: it is over.
                earlyStop = nil
                end(notice: stop.notice)
            } else {
                earlyStop = nil
                snapshot.state = .keyed(since: now())
                if kind == .tunerTune {
                    awaitCarrier(generation: mine)
                }
            }
        case .refused(let refusal):
            snapshot.state = .refused(refusal)
            snapshot.keyKind = nil
        case .noAnswer:
            awaitLinkNews(generation: mine)
        }
        changed()
    }

    @discardableResult
    private func release(authority: CommandSendPermit? = nil) -> Bool {
        guard !(authority?.isRevoked ?? false) else { return false }
        let kind = snapshot.keyKind ?? .ptt
        let epoch = keyEpoch ?? Self.unansweredEpoch
        generation += 1
        let mine = generation
        let off = registerOff()
        snapshot.state = .unkeying
        // The release ends the Core's watch on the key at once; so does
        // this device's keepalive for it.
        keyEpoch = nil
        earlyStop = nil
        changed()
        let verb = Self.offVerb(kind, epoch: epoch)
        enqueue(verb, generation: nil, microphone: false, authority: authority) { controller, answer in
            await controller.settleRelease(answer, generation: mine, off: off)
        }
        return true
    }

    /// The release for a key of `kind`: `tx.unkey {epoch}` for the PTT,
    /// each other kind's own off.
    static func offVerb(_ kind: KeyKind, epoch: Int64) -> TransmitVerb {
        switch kind {
        case .ptt:
            return .unkey(epoch: epoch)
        case .tune:
            return .tune(on: false)
        case .twoTone:
            return .twoTone(on: false)
        case .tunerTune:
            return .tunerTune(on: false)
        }
    }

    private func settleRelease(_ answer: TransmitAnswer, generation mine: Int, off: Int) {
        finishOff(off, answer: answer)
        guard mine == generation, snapshot.state == .unkeying else {
            return
        }
        if Self.holdsNothing(answer) {
            // Nothing of this device's was on and another device holds
            // transmit: the release has nothing left to end.
            snapshot.state = !unresolvedOffs.isEmpty || failedOff
                ? .refused(TxRefusalInfo(reason: Self.unresolvedOffText, code: "reconnectRequired"))
                : .idle
            snapshot.keyKind = nil
            followHolder(report)
            changed()
            return
        }
        switch answer {
        case .accepted:
            releasedOwn = true
            snapshot.state = !unresolvedOffs.isEmpty || failedOff
                ? .refused(TxRefusalInfo(reason: Self.unresolvedOffText, code: "reconnectRequired"))
                : report.txEnding ? .ending : .idle
            if snapshot.state == .idle {
                snapshot.keyKind = nil
            }
            followHolder(report)
        case .refused(let refusal):
            snapshot.state = .refused(TxRefusalInfo(reason: refusal.reason + " " + Self.unresolvedOffText,
                                                    code: "reconnectRequired"))
            snapshot.keyKind = nil
        case .noAnswer:
            snapshot.state = .refused(TxRefusalInfo(reason: Self.unresolvedOffText,
                                                    code: "reconnectRequired"))
            snapshot.keyKind = nil
        }
        changed()
    }

    /// A key or release with no answer: the link may be going. After a
    /// moment with the link still up, the key is released and PTT is idle;
    /// if the link went meanwhile, PTT already says so.
    private func awaitLinkNews(generation mine: Int) {
        graceTimer?.cancel()
        graceTimer = clock.schedule(after: Self.lostAnswerGrace) { [weak self] in
            await self?.linkNewsWaited(generation: mine)
        }
    }

    private func linkNewsWaited(generation mine: Int) {
        guard mine == generation, linkUp else {
            return
        }
        switch snapshot.state {
        case .keying, .unkeying:
            Self.logger.warning("A keying command had no answer from the Core; releasing it")
            generation += 1
            let kind = snapshot.keyKind ?? .ptt
            snapshot.state = .idle
            snapshot.keyKind = nil
            keyEpoch = nil
            // Whatever the Core did with it, end it: nothing stays keyed unasked.
            enqueueOff(Self.offVerb(kind, epoch: Self.unansweredEpoch))
            followHolder(report)
            changed()
        default:
            break
        }
    }

    /// A tuner tune the Core accepted keys its carrier within the Core's
    /// own wait or its cycle is over, and the Core does not always say so
    /// (``tunerCarrierDeadline``): with no carrier seen by then, the tune
    /// ends here through its own off, as the `tuneEnded` notice ends it.
    /// The off keys nothing: for a cycle already over the Core accepts it
    /// and changes nothing, and one still running is cancelled. A later
    /// key moves the generation, so this wait never touches it.
    private func awaitCarrier(generation mine: Int) {
        carrierTimer?.cancel()
        carrierTimer = clock.schedule(after: Self.tunerCarrierDeadline) { [weak self] in
            await self?.carrierWaited(generation: mine)
        }
    }

    private func carrierWaited(generation mine: Int) {
        carrierTimer = nil
        guard mine == generation, linkUp, snapshot.keyKind == .tunerTune, snapshot.state.isKeyed,
              !sawKeyed else {
            return
        }
        Self.logger.info("The tuner's carrier did not come; ending the tuner tune")
        stopAll()
    }

    /// Ends this device's key without sending anything: the Core ended it.
    private func end(notice: TransmitStopNotice?) {
        generation += 1
        keyEpoch = nil
        earlyStop = nil
        snapshot.state = .idle
        snapshot.keyKind = nil
        if let notice, !notice.text.isEmpty {
            snapshot.stop = notice
        }
    }

    // MARK: The queue

    /// Sends `verb` after every verb queued before it has gone. A key
    /// (`generation` set) goes only while it is still the one wanted and
    /// its session still stands; after the microphone's start, both are
    /// checked again. Every verb goes only in the session it was queued in.
    private func enqueue(_ verb: TransmitVerb, generation mine: Int?, microphone: Bool,
                         authority intentAuthority: CommandSendPermit? = nil,
                         then settle: @escaping @Sendable (PttController, TransmitAnswer) async -> Void) {
        let previous = sendTail
        let commands = commands
        let startMicrophone = startMicrophone
        let clock = clock
        let queuedIn = session
        let authority = CommandSendPermit(parents: [commandAuthority] + [intentAuthority].compactMap { $0 })
        let step = Task { [weak self] () -> TransmitPending? in
            await previous?.value
            guard !authority.isRevoked, let self, await self.maySend(generation: mine, session: queuedIn) else {
                return nil
            }
            if microphone {
                let started = await Self.startWithin(Self.microphoneStartDeadline, clock: clock, startMicrophone)
                if case .failed(let reason) = started {
                    await self.microphoneFailed(reason, generation: mine)
                    return nil
                }
                guard await self.maySend(generation: mine, session: queuedIn) else {
                    return nil
                }
            }
            return await commands.send(verb, copies: Self.copies, authority: authority)
        }
        sendTail = Task {
            _ = await step.value
        }
        run { [weak self] in
            guard let pending = await step.value else {
                return
            }
            let answer = await pending.answer()
            if let self, await self.session == queuedIn {
                await settle(self, answer)
            }
        }
    }

    /// The microphone did not start for a key still wanted: nothing was
    /// sent, and PTT shows why.
    private func microphoneFailed(_ reason: String, generation mine: Int?) {
        guard let mine, mine == generation, snapshot.state == .keying else {
            return
        }
        Self.logger.warning("The microphone did not start; the key was not sent")
        generation += 1
        snapshot.state = .refused(TxRefusalInfo(reason: reason, code: Self.microphoneRefusalCode))
        snapshot.keyKind = nil
        changed()
    }

    /// Runs `start`, giving up on it after `deadline`: a start that never
    /// ends cannot hold the queue, and a release behind it, forever.
    private static func startWithin(_ deadline: Duration, clock: any LinkClock,
                                    _ start: @escaping @Sendable () async -> MicrophoneStart) async -> MicrophoneStart {
        let first = FirstAnswer<MicrophoneStart>()
        return await withCheckedContinuation { continuation in
            first.wait(continuation)
            let timer = clock.schedule(after: deadline) {
                first.answer(.failed(reason: microphoneDidNotStartText))
            }
            Task {
                let result = await start()
                timer.cancel()
                first.answer(result)
            }
        }
    }

    private func maySend(generation mine: Int?, session queuedIn: Int) -> Bool {
        guard linkUp, queuedIn == session else {
            return false
        }
        if let mine {
            return mine == generation && snapshot.state == .keying
        }
        return true
    }

    // MARK: What the Core says

    /// A stop of this device's key: its epoch is the stop's or older (a key
    /// pressed after the stop, answered before the stop's update came, goes
    /// on). The Core has already unkeyed it, so nothing is sent.
    private func noteStop(_ next: TransmitStateReport) {
        let first = firstReportOfSession
        firstReportOfSession = false
        let last = lastStopSerial
        lastStopSerial = next.stopSerial
        let notice = TransmitStopNotice(reason: next.stopReason, text: next.stopText, serial: next.stopSerial)
        if first {
            // A new session: its first state only records the last stop,
            // unless it is the stop of the key the link took away.
            if let lost = keyAtLinkLoss, let last, next.stopSerial != last,
               lost == Self.unansweredEpoch || lost <= next.stopEpoch, !next.stopText.isEmpty {
                snapshot.stop = notice
            }
            keyAtLinkLoss = nil
            return
        }
        guard let last, next.stopSerial != last else {
            return
        }
        switch snapshot.state {
        case .keyed:
            if let epoch = keyEpoch, epoch <= next.stopEpoch {
                end(notice: notice)
            }
        case .keying:
            // The key's answer has not come: keep the stop for it.
            earlyStop = (next.stopEpoch, notice)
        default:
            break
        }
    }

    /// The radio went off the air after it was on for this device's key,
    /// with no release of this device's: the Core ended it.
    private func followKey(_ next: TransmitStateReport) {
        guard snapshot.state.isKeyed else {
            return
        }
        if next.keyed {
            sawKeyed = true
        } else if sawKeyed {
            end(notice: nil)
        }
    }

    /// Transmit held elsewhere, changing hands, or free again.
    private func followHolder(_ next: TransmitStateReport) {
        if !unresolvedOffs.isEmpty || failedOff {
            if snapshot.state == .unkeying || !linkUp { return }
            if case .refused(let refusal) = snapshot.state, refusal.code == "reconnectRequired" { return }
            snapshot.state = .refused(TxRefusalInfo(reason: Self.unresolvedOffText,
                                                    code: "reconnectRequired"))
            snapshot.keyKind = nil
            return
        }
        if next.heldElsewhere {
            switch snapshot.state {
            case .keying, .keyed:
                // Taken from this device while its key was on: the Core has
                // already unkeyed it, so nothing is sent.
                generation += 1
                keyEpoch = nil
                earlyStop = nil
                snapshot.keyKind = nil
            case .unkeying, .linkLost:
                return
            default:
                break
            }
            snapshot.keyKind = nil
            snapshot.state = next.holderTransferring
                ? .waiting : .heldElsewhere(device: next.holderLabel, onAir: next.keyed)
            return
        }
        if next.holderTransferring {
            switch snapshot.state {
            case .idle, .heldElsewhere, .waiting:
                snapshot.state = .waiting
            default:
                break
            }
            return
        }
        switch snapshot.state {
        case .heldElsewhere, .waiting:
            snapshot.state = .idle
        default:
            break
        }
    }

    /// An epoch as the PTT keeps it: an answer without one, or one below 1
    /// (never a real epoch), counts as not answered, which is never older.
    static func answered(_ epoch: Int64?) -> Int64 {
        guard let epoch, epoch >= 1, epoch <= unansweredEpoch else {
            return unansweredEpoch
        }
        return epoch
    }

    // MARK: The keepalive

    /// The epoch the keepalive carries: the key's, or 4294967295 while its
    /// answer has not come and for VOX alone.
    private var keepaliveEpoch: Int64 {
        snapshot.state.isKeyed ? keyEpoch ?? Self.unansweredEpoch : Self.unansweredEpoch
    }

    private var microphoneWanted: Bool {
        guard linkUp, unresolvedOffs.isEmpty, !failedOff else {
            return false
        }
        switch snapshot.state {
        case .keying, .keyed:
            return snapshot.keyKind == .ptt || voxArmed
        default:
            return voxArmed
        }
    }

    private var keepaliveWanted: Bool {
        guard linkUp, unresolvedOffs.isEmpty, !failedOff else {
            return false
        }
        switch snapshot.state {
        case .keying, .keyed:
            return true
        default:
            return voxArmed
        }
    }

    private func refreshKeepalive() {
        let wanted = keepaliveWanted
        if !wanted {
            heartbeatGate.retire()
            heartbeatGate = TransmitHeartbeatGate()
        }
        heartbeatGate.setOpen(wanted)
        if wanted, keepaliveTimer == nil {
            keepaliveToken += 1
            keepaliveGridStartMilliseconds = clock.nowMilliseconds
            scheduleKeepalive(keepaliveToken)
        } else if !wanted, let timer = keepaliveTimer {
            timer.cancel()
            keepaliveTimer = nil
            keepaliveToken += 1
        }
        snapshot.keepaliveRunning = keepaliveTimer != nil
    }

    private func scheduleKeepalive(_ token: Int) {
        // R-IOS-13: resume at the next deadline on the original grid when
        // dispatch runs late. Missed deadlines never become extra sends.
        let interval = Self.keepaliveInterval.components
        let intervalMilliseconds = interval.seconds * 1000
            + interval.attoseconds / 1_000_000_000_000_000
        let elapsed = max(0, clock.nowMilliseconds - keepaliveGridStartMilliseconds)
        let delay = intervalMilliseconds - elapsed % intervalMilliseconds
        keepaliveTimer = clock.schedule(after: .milliseconds(delay)) { [weak self] in
            await self?.keepaliveTick(token)
        }
    }

    private func keepaliveTick(_ token: Int) async {
        guard token == keepaliveToken, keepaliveWanted else {
            return
        }
        scheduleKeepalive(token)
        sequence += 1
        let currentSession = session
        await commands.sendKeepalive(sequence: sequence, epoch: keepaliveEpoch,
                                     gate: heartbeatGate) { [weak self] in
            guard let self else { return false }
            return await self.keepaliveAllowed(token: token, session: currentSession)
        }
    }

    private func keepaliveAllowed(token: Int, session expected: Int) -> Bool {
        token == keepaliveToken && expected == session && keepaliveWanted
    }

    private func registerOff() -> Int {
        nextOff += 1
        unresolvedOffs.insert(nextOff)
        refreshKeepalive()
        return nextOff
    }

    /// The Core's refusal of a release from a device that does not hold
    /// transmit (link 18.6, "Who may release"): RemoteKeying::stopFrom
    /// gives it only when nothing of this device's is on and another device
    /// holds transmit (src/core/session/RemoteKeying.cpp:549-556), so this
    /// device holds nothing to release.
    static func holdsNothing(_ answer: TransmitAnswer) -> Bool {
        if case .refused(let refusal) = answer, refusal.code == otherDeviceHoldsCode {
            return true
        }
        return false
    }

    /// The link document's code for that refusal (section 18.3).
    static let otherDeviceHoldsCode = "otherDeviceHolds"

    private func finishOff(_ off: Int, answer: TransmitAnswer) {
        guard unresolvedOffs.contains(off) else { return }
        if case .accepted = answer {
            unresolvedOffs.remove(off)
        } else if Self.holdsNothing(answer) {
            unresolvedOffs.remove(off)
        } else {
            failedOff = true
        }
        if unresolvedOffs.isEmpty, !failedOff,
           case .refused(let refusal) = snapshot.state,
           refusal.code == "reconnectRequired" {
            snapshot.state = .idle
            followHolder(report)
        }
        changed()
    }

    private func enqueueOff(_ verb: TransmitVerb) {
        let off = registerOff()
        changed()
        enqueue(verb, generation: nil, microphone: false) { controller, answer in
            await controller.finishOff(off, answer: answer)
        }
    }

    // MARK: Inside

    private func changed() {
        refreshKeepalive()
        snapshot.logicalSessionOwner = logicalSessionOwner
        snapshot.microphoneWanted = microphoneWanted
        snapshot.voxArmed = voxArmed
        snapshotSink.yield(snapshot)
    }

    private func run(_ work: @escaping @Sendable () async -> Void) {
        let id = nextTaskId
        nextTaskId += 1
        inFlight[id] = Task { [weak self] in
            await work()
            await self?.finished(id)
        }
    }

    private func finished(_ id: Int) {
        inFlight[id] = nil
    }
}
