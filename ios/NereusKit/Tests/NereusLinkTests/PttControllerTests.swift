// NereusSDR for iOS: the PTT's state machine, keepalive and the Core's transmit state, against a Core that keys as the Core does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// The phone's transmit boundary (R-IOS-13; link document sections 18.6 to
/// 18.8): what the PTT sends, in what order, and what it never sends,
/// proved against a Core that keys as `RemoteKeying` does, so a test fails
/// when the radio would be left keyed, not only when a message differs.
@Suite struct PttControllerTests {
    /// This device's side of the Core's keying (src/core/session/RemoteKeying.cpp):
    /// one key per device, which PTT, TUNE and two-tone share; a key while
    /// the device's key is on answers that key's epoch; an unkey older than
    /// the live key is ignored, one with nothing on is accepted and changes
    /// nothing; TUNE and two-tone off end only their own key. While another
    /// device holds transmit, a release with nothing of this device's on is
    /// refused `otherDeviceHolds` (RemoteKeying::stopFrom,
    /// RemoteKeying.cpp:549-556). The Core acts on each command as it
    /// arrives; its answer can be held back.
    actor RecordingCore: TransmitCommandSending {
        struct Sent: Equatable {
            let verb: TransmitVerb
            let copies: Int
        }

        enum Key: Equatable {
            case none
            case on(epoch: Int64, kind: PttController.KeyKind)
        }

        private(set) var sent: [Sent] = []
        private(set) var keepalives: [(sequence: Int64, epoch: Int64)] = []
        /// The radio, for this device: keyed or not, and by which kind.
        private(set) var key: Key = .none
        private var lastEpoch: Int64 = 0
        private var scripted: [TransmitAnswer] = []
        private var held: [CheckedContinuation<Void, Never>] = []
        private var holding = false
        /// Another device that holds transmit, by name, or nil.
        private(set) var otherHolder: String?

        /// The next commands are answered with these, and change nothing
        /// (the Core refused them, or gave no answer).
        func script(_ next: [TransmitAnswer]) {
            scripted += next
        }

        /// Answers wait until ``release()``.
        func hold() {
            holding = true
        }

        func release() {
            holding = false
            let waiting = held
            held = []
            for continuation in waiting {
                continuation.resume()
            }
        }

        /// The Core stops the key on its own (a watchdog, the time-out);
        /// returns its epoch.
        func stopOnItsOwn() -> Int64 {
            guard case .on(let epoch, _) = key else {
                return 0
            }
            key = .none
            return epoch
        }

        var keyed: Bool { key != .none }

        /// Another device takes transmit: this device's key ends at once.
        func takenBy(_ name: String) {
            otherHolder = name
            key = .none
        }

        func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
            sent.append(Sent(verb: verb, copies: copies))
            let answer = scripted.isEmpty ? act(verb) : scripted.removeFirst()
            return TransmitPending { [self] in
                await self.waitIfHeld()
                return answer
            }
        }

        private func waitIfHeld() async {
            if holding {
                await withCheckedContinuation { held.append($0) }
            }
        }

        /// RemoteKeying::keyNow, unkey, stopFrom, tune and twoTone for one device.
        private func act(_ verb: TransmitVerb) -> TransmitAnswer {
            switch verb {
            case .key:
                return start(.ptt)
            case .tune(let on):
                return on ? start(.tune) : stop(.tune)
            case .twoTone(let on):
                return on ? start(.twoTone) : stop(.twoTone)
            case .tunerTune(let on):
                // RemoteKeying::tunerTune (RemoteKeying.cpp:654-735): never
                // started on the air, this device's own key included.
                if on, case .on(_, let live) = key, live != .tunerTune {
                    return .refused(TxRefusalInfo(reason: "The radio is on the air. Try again when it stops.",
                                                  code: "holderOnAir"))
                }
                return on ? start(.tunerTune) : stop(.tunerTune)
            case .unkey(let epoch):
                if case .on(let live, _) = key {
                    if epoch < live {
                        return .accepted(epoch: nil)
                    }
                    key = .none
                    return .accepted(epoch: nil)
                }
                return nothingOn()
            }
        }

        /// RemoteKeying::stopFrom with nothing of this device's on: from a
        /// device that does not hold transmit the release is refused and
        /// the transmission continues (contract 18.6).
        private func nothingOn() -> TransmitAnswer {
            if let otherHolder {
                return .refused(TxRefusalInfo(reason: otherHolder + " has the transmitter. Take it to stop the transmission.",
                                              code: "otherDeviceHolds", fix: TxRefusalInfo.takeTransmit))
            }
            return .accepted(epoch: nil)
        }

        private func start(_ kind: PttController.KeyKind) -> TransmitAnswer {
            if case .on(let live, _) = key {
                return .accepted(epoch: live)
            }
            lastEpoch += 1
            key = .on(epoch: lastEpoch, kind: kind)
            return .accepted(epoch: lastEpoch)
        }

        private func stop(_ kind: PttController.KeyKind) -> TransmitAnswer {
            if case .on(_, let live) = key, live == kind {
                key = .none
                return .accepted(epoch: nil)
            }
            return nothingOn()
        }

        func sendKeepalive(sequence: Int64, epoch: Int64) async {
            keepalives.append((sequence, epoch))
        }

        private(set) var heartbeatGates: [TransmitHeartbeatGate] = []
        func sendKeepalive(sequence: Int64, epoch: Int64, gate: TransmitHeartbeatGate,
                           stillAllowed: @escaping @Sendable () async -> Bool) async {
            guard await stillAllowed(), gate.isOpen else { return }
            heartbeatGates.append(gate)
            await sendKeepalive(sequence: sequence, epoch: epoch)
        }

        var verbs: [TransmitVerb] { sent.map(\.verb) }
        var keepaliveSequences: [Int64] { keepalives.map(\.sequence) }
        var keepaliveEpochs: [Int64] { keepalives.map(\.epoch) }
    }

    /// A gate for the microphone's start, which a test can hold closed.
    actor Gate {
        private var open = true
        private var waiting: [CheckedContinuation<Void, Never>] = []
        private(set) var passes = 0

        func close() {
            open = false
        }

        func openUp() {
            open = true
            let all = waiting
            waiting = []
            for continuation in all {
                continuation.resume()
            }
        }

        func pass() async -> MicrophoneStart {
            passes += 1
            if !open {
                await withCheckedContinuation { waiting.append($0) }
            }
            return .started
        }
    }

    struct Rig {
        let core = RecordingCore()
        let clock = ManualLinkClock()
        let microphone = Gate()
        let ptt: PttController

        init() {
            let microphone = microphone
            ptt = PttController(commands: core, clock: clock, startMicrophone: { await microphone.pass() })
        }

        /// A PTT whose microphone start is `start`.
        init(startMicrophone start: @escaping @Sendable () async -> MicrophoneStart) {
            ptt = PttController(commands: core, clock: clock, startMicrophone: start)
        }

        /// Waits until something is scheduled `ms` from now on the clock.
        func waitScheduled(in ms: Int64) async {
            let due = clock.now + ms
            while !clock.pendingDueTimes.contains(due) {
                await Task.yield()
            }
        }

        /// Waits until the Core has had `count` keying commands.
        func waitSent(_ count: Int) async {
            while await core.sent.count < count {
                await Task.yield()
            }
        }

        /// The link up and the Core's first transmit state, nobody holding transmit.
        func connected() async {
            await ptt.linkChanged(up: true)
            await ptt.update(TransmitStateReport())
        }
    }

    static let stopText = "Transmit stopped after 3:00, the Core's time-out for phones and tablets."

    static func stop(serial: Int64, epoch: Int64, reason: String = "timeOut",
                     text: String = stopText) -> TransmitStateReport {
        var report = TransmitStateReport()
        report.stopReason = reason
        report.stopText = text
        report.stopSerial = serial
        report.stopEpoch = epoch
        return report
    }

    // MARK: Keying and releasing

    @Test func oneTapKeysAndASecondUnkeys() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        #expect(await rig.ptt.state == .keying)
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.keyed)
        #expect(await rig.core.sent == [.init(verb: .key(trigger: "screen"), copies: 3)])
        // The microphone starts at the tap, before the key goes.
        #expect(await rig.microphone.passes == 1)
        await rig.ptt.tap()
        #expect(await rig.ptt.state == .unkeying)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await !rig.core.keyed)
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        #expect(await rig.core.sent.allSatisfy { $0.copies == 3 })
    }

    /// M3: the phone's microphone stopped by itself while keyed (it could
    /// not be built again after a route change): the key is released as a
    /// tap would release it, and the band says why.
    @Test func aLostMicrophoneReleasesTheKeyAndSaysWhy() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)

        await rig.ptt.microphoneLost("The microphone stopped.")
        #expect(await rig.ptt.state == .unkeying)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await !rig.core.keyed)
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        let snapshot = await rig.ptt.snapshot
        #expect(!snapshot.keepaliveRunning)
        #expect(!snapshot.microphoneWanted)
        #expect(snapshot.stop?.text == "The microphone stopped.")
        #expect(snapshot.stop?.reason == PttController.microphoneLostReason)
    }

    /// Fix wave (fix-tx2 item 5): the Core closed the microphone line's
    /// track while the link stays up: the PTT's key goes nowhere, so it is
    /// released as a tap releases it, and the band says why in `text`.
    @Test func theMicrophoneLineClosingReleasesAPttKeyAndSaysWhy() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.microphoneLineClosed("The line went."))
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await !rig.core.keyed)
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        let snapshot = await rig.ptt.snapshot
        #expect(!snapshot.keepaliveRunning)
        #expect(snapshot.stop?.text == "The line went.")
        #expect(snapshot.stop?.reason == PttController.microphoneLostReason)
    }

    /// TUNE, two-tone and the tuner's tune carry no microphone: the line
    /// closing ends none of them; with nothing keyed it does nothing.
    @Test func theMicrophoneLineClosingLeavesTuneAndIdleAlone() async {
        let rig = Rig()
        await rig.connected()
        #expect(await !rig.ptt.microphoneLineClosed("The line went."))
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        #expect(await !rig.ptt.microphoneLineClosed("The line went."))
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.verbs == [.tune(on: true)])
        #expect(await rig.ptt.snapshot.stop == nil)
    }

    /// M3: a lost microphone with nothing keyed sends nothing and keys nothing.
    @Test func aLostMicrophoneWithNothingKeyedSendsNothing() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.microphoneLost("The microphone stopped.")
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.core.verbs.isEmpty)
        #expect(await rig.ptt.snapshot.stop?.text == "The microphone stopped.")
        // Each loss reads as a new stop.
        let first = await rig.ptt.snapshot.stop?.serial
        await rig.ptt.microphoneLost("The microphone stopped.")
        #expect(await rig.ptt.snapshot.stop?.serial != first)
    }

    /// M3: the microphone line going with the link (the media connection
    /// ends after the link went) leaves the link-lost words on the screen:
    /// no stop notice of the phone's own covers them, and nothing is sent.
    @Test func aLostMicrophoneAfterTheLinkWentLeavesTheLinkLost() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        await rig.ptt.linkChanged(up: false)
        #expect(await rig.ptt.state == .linkLost)
        let verbs = await rig.core.verbs
        await rig.ptt.microphoneLost("The microphone stopped.")
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .linkLost)
        #expect(await rig.ptt.snapshot.stop == nil)
        #expect(await rig.core.verbs == verbs)
    }

    /// C1: a double tap while the microphone starts. The release must never
    /// reach the Core before its key; here the key is never sent at all.
    @Test func aDoubleTapWhileTheMicrophoneStartsLeavesTheRadioUnkeyed() async {
        let rig = Rig()
        await rig.connected()
        await rig.microphone.close()
        await rig.ptt.tap()
        await rig.ptt.tap()
        await rig.microphone.openUp()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        #expect(await rig.core.verbs == [.unkey(epoch: PttController.unansweredEpoch)])
        #expect(await rig.ptt.state == .idle)
        await rig.clock.advance(by: 1000)
        #expect(await rig.core.keepalives.isEmpty)
    }

    /// C1: a double tap before the key's answer: the release follows the key.
    @Test func aDoubleTapBeforeTheAnswerLeavesTheRadioUnkeyed() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        await rig.waitSent(1)
        await rig.ptt.tap()
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        let verbs = await rig.core.verbs
        #expect(verbs.first == .key(trigger: "screen"))
        #expect(verbs.dropFirst().allSatisfy {
            if case .unkey = $0 { return true } else { return false }
        })
        #expect(await rig.ptt.state == .idle)
    }

    /// C1, every interleaving of the microphone and the answer: two taps
    /// always leave the radio unkeyed and the PTT idle.
    @Test(arguments: [(false, false), (true, false), (false, true), (true, true)])
    func aDoubleTapInAnyInterleavingLeavesTheRadioUnkeyed(microphoneHeld: Bool, answerHeld: Bool) async {
        let rig = Rig()
        await rig.connected()
        if microphoneHeld {
            await rig.microphone.close()
        }
        if answerHeld {
            await rig.core.hold()
        }
        await rig.ptt.tap()
        await rig.ptt.tap()
        await rig.microphone.openUp()
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
        await rig.clock.advance(by: 500)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
    }

    @Test func stopWhileKeyingLeavesTheRadioUnkeyed() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        await rig.waitSent(1)
        await rig.ptt.stopAll()
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
    }

    /// A reconnect while a key waits to go: nothing goes on the new session.
    @Test func aReconnectWhileAKeyWaitsSendsNothing() async {
        let rig = Rig()
        await rig.connected()
        await rig.microphone.close()
        await rig.ptt.tap()
        await rig.ptt.linkChanged(up: false)
        await rig.ptt.linkChanged(up: true)
        await rig.microphone.openUp()
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
    }

    /// The old Core accepted a key, but its answer reaches the PTT only
    /// after that session ended. A cleanup unkey must stay on OLD.
    @Test func staleAcceptedKeyAnswerCannotUnkeyReplacementSession() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        await rig.waitSent(1)
        #expect(await rig.core.keyed)
        await rig.ptt.linkChanged(up: false)
        _ = await rig.core.stopOnItsOwn()
        await rig.ptt.linkChanged(up: true)
        await rig.core.release()
        await rig.ptt.settle()
        let verbs = await rig.core.verbs
        #expect(verbs.count == 1)
    }

    @Test func aRefusalShowsTheCoresWordsAndFix() async {
        let rig = Rig()
        await rig.connected()
        let refusal = TxRefusalInfo(reason: "The amplifier is in standby. Operate it, or change the interlock in Setup.",
                                    code: "ampStandby", fix: TxRefusalInfo.operateAmp)
        await rig.core.script([.refused(refusal)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        // Nothing is keyed, so no keepalive goes.
        await rig.clock.advance(by: 500)
        #expect(await rig.core.keepalives.isEmpty)
        // The next tap tries again.
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
    }

    @Test func aVoiceKeyWithNoMicrophoneLineIsRefusedAndNeverPretendsToKey() async throws {
        let rig = Rig()
        await rig.connected()
        // The Core's words for this refusal, as its fixture sends them.
        let words = try #require(try SessionFixtures.refusalReason(code: PttController.microphoneRefusalCode))
        let refusal = TxRefusalInfo(reason: words, code: PttController.microphoneRefusalCode)
        await rig.core.script([.refused(refusal)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        #expect(await rig.ptt.snapshot.transmitting == false)
    }

    /// The Core's words for `noTransmitSlice` (link section 18.3; the
    /// slice-access phone contract, keying).
    static let noTransmitSliceWords = "There is no slice to transmit on. Add a slice first."

    /// The first-key rule (Core 5cdb9a1ab, src/core/safety/TxRefusal.h
    /// `chooseTransmitSlice`): after a take, the transmit flag stays where
    /// it was, and a key before this phone chooses the taken slice for
    /// transmit is refused with the Core's words and code. PTT stays
    /// unkeyed and shows them as sent (R-IOS-13, R-IOS-42).
    static let chooseTransmitSliceWords =
        "You took this slice from another device. Choose it for transmit first with its TX button."

    @Test func aFirstKeyAfterATakeIsRefusedWithTheCoresWordsAndCode() async {
        let rig = Rig()
        await rig.connected()
        let refusal = TxRefusalInfo(reason: Self.chooseTransmitSliceWords, code: "chooseTransmitSlice")
        await rig.core.script([.refused(refusal)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        let snapshot = await rig.ptt.snapshot
        #expect(!snapshot.transmitting && snapshot.keyKind == nil)
        #expect(await rig.core.keepalives.isEmpty)
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await !rig.core.keyed)
    }

    /// The keying rule (Core 5cdb9a1ab): a key whose transmit flag sits on
    /// another device's slice, with no slice of this device's to move it to,
    /// is refused with `noTransmitSlice` and the Core sends no transmit
    /// change. PTT stays unkeyed, shows the Core's words, sends no release
    /// and runs no keepalive; the next tap tries again.
    @Test func aKeyRefusedForNoTransmitSliceStaysUnkeyedAndShowsTheCoresWords() async {
        let rig = Rig()
        await rig.connected()
        let refusal = TxRefusalInfo(reason: Self.noTransmitSliceWords, code: "noTransmitSlice")
        await rig.core.script([.refused(refusal)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        let snapshot = await rig.ptt.snapshot
        #expect(!snapshot.transmitting && snapshot.keyKind == nil && !snapshot.microphoneWanted)
        // No transmit change comes from the Core; the refusal stands.
        await rig.clock.advance(by: 500)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        #expect(await rig.core.keepalives.isEmpty)
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await !rig.core.keyed)
        // Dismissed, PTT reads Tap again, and the next tap keys.
        await rig.ptt.dismissNotice()
        #expect(await rig.ptt.state == .idle)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
    }

    /// The keying rule (Core 5cdb9a1ab): with a slice of this device's to
    /// transmit on, the Core moves the unkeyed flag there once it admits the
    /// key and the key goes ahead. The key's answer and the Core's transmit
    /// state, now held here, key the PTT as any key, and nothing else is sent.
    @Test func aKeyWhoseFlagTheCoreMovesToThisDevicesSliceKeys() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        var onAir = TransmitStateReport()
        onAir.keyed = true
        onAir.held = true
        onAir.heldHere = true
        onAir.holderEpoch = 1
        await rig.ptt.update(onAir)
        #expect(await rig.ptt.state.isKeyed)
        await rig.clock.advance(by: 100)
        await rig.ptt.settle()
        #expect(await rig.core.keepaliveEpochs.last == 1)
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        #expect(await !rig.core.keyed)
    }

    @Test func anUnkeyTheCoreRefusesShowsItsWordsAndStopsTheKeepalive() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        // The Core's own release refusal for this device's key
        // (TxRefusals::stopNotConfirmed, TxRefusal.cpp:183-187). A release
        // refused otherDeviceHolds means this device holds nothing and is
        // not a failed off (contract 18.6; see the tests below).
        let refusal = TxRefusalInfo(reason: "The radio did not confirm it stopped transmitting.",
                                    code: "stopNotConfirmed")
        await rig.core.script([.refused(refusal)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        if case .refused(let shown) = await rig.ptt.state {
            #expect(shown.reason.contains(refusal.reason))
            #expect(shown.reason.contains(PttController.unresolvedOffText))
            #expect(shown.fix.isEmpty)
        } else {
            Issue.record("Unconfirmed off must ask for a reconnect")
        }
        let count = await rig.core.keepalives.count
        await rig.clock.advance(by: 500)
        #expect(await rig.core.keepalives.count == count)
    }

    /// M2: a key with no answer while the link stays up is released.
    @Test func aKeyWithNoAnswerIsReleasedOnceTheLinkIsStillUp() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.script([.noAnswer])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .keying)
        await rig.clock.advance(by: 500)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: PttController.unansweredEpoch)])
    }

    /// M2: a key with no answer because the link went shows the link lost.
    @Test func aKeyWithNoAnswerAsTheLinkGoesShowsTheLinkLost() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.script([.noAnswer])
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.linkChanged(up: false)
        #expect(await rig.ptt.state == .linkLost)
    }

    /// C1: a key the Core accepts after it stopped being wanted is released
    /// with its own epoch.
    @Test func aLateAcceptedKeyIsReleasedWithItsEpoch() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        await rig.waitSent(1)
        // Taken meanwhile: the PTT lets its key go without a release of its own.
        var taken = TransmitStateReport()
        taken.held = true
        taken.holderShortName = "MacBook"
        taken.holderSource = "device"
        await rig.core.takenBy("MacBook")
        await rig.ptt.update(taken)
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        #expect(await !rig.core.keyed)
        // The Core refused that release otherDeviceHolds: this device holds
        // nothing, so the PTT shows the holder, not a reconnect fence.
        await rig.ptt.update(taken)
        #expect(await rig.ptt.state == .heldElsewhere(device: taken.holderLabel, onAir: false))
        // Once transmit is free again the PTT keys as before.
        await rig.ptt.update(TransmitStateReport())
        #expect(await rig.ptt.state == .idle)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
    }

    /// Q1: a release that crosses a take is refused otherDeviceHolds by the
    /// Core (RemoteKeying.cpp:549-556), which says only when nothing of this
    /// device's is on: the PTT counts it as released, not as a failed off.
    @Test func aReleaseRefusedBecauseAnotherDeviceHoldsCountsAsReleased() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        // Taken by the MacBook at the Core; the report has not come yet.
        await rig.core.takenBy("MacBook")
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1)])
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
        let count = await rig.core.keepalives.count
        await rig.clock.advance(by: 500)
        #expect(await rig.core.keepalives.count == count)
        var taken = TransmitStateReport()
        taken.held = true
        taken.holderShortName = "MacBook"
        taken.holderSource = "device"
        await rig.ptt.update(taken)
        #expect(await rig.ptt.state == .heldElsewhere(device: taken.holderLabel, onAir: false))
    }

    /// M5: an epoch below 1 is never a real one; it counts as unanswered.
    @Test func anEpochBelowOneCountsAsUnanswered() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.script([.accepted(epoch: 0)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        #expect(await rig.core.keepaliveEpochs == [PttController.unansweredEpoch])
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.verbs.last == .unkey(epoch: PttController.unansweredEpoch))
    }

    // MARK: One key: PTT and TUNE

    /// C2: during this device's TUNE a PTT tap ends TUNE; it never keys.
    @Test func aPttTapDuringTuneEndsTuneAndNeverKeys() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.setTune(true)
        await rig.ptt.settle()
        #expect(await rig.core.key == .on(epoch: 1, kind: .tune))
        #expect(await rig.ptt.snapshot.tuning)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        #expect(await rig.core.verbs == [.tune(on: true), .tune(on: false)])
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.ptt.snapshot.tuning == false)
        // The third tap keys the PTT afresh, and the fourth ends it.
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.key == .on(epoch: 2, kind: .ptt))
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        await rig.clock.advance(by: 500)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
    }

    /// I1: TUNE while the PTT is keyed sends nothing and leaves the key and
    /// its keepalive alone.
    @Test func tuneWhileKeyedLeavesTheKeyAndItsKeepalive() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.setTune(true)
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await rig.ptt.state.isKeyed)
        await rig.clock.advance(by: 300)
        #expect(await rig.core.keepalives.count == 3)
    }

    /// I1: a refused TUNE says why and keys nothing.
    @Test func aRefusedTuneSaysWhy() async {
        let rig = Rig()
        await rig.connected()
        let refusal = TxRefusalInfo(reason: "The transmit interlock is holding transmit off. Check it in Setup.",
                                    code: "interlock")
        await rig.core.script([.refused(refusal)])
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .refused(refusal))
        #expect(await rig.ptt.snapshot.tuning == false)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
    }

    @Test func tuneKeysWithItsEpochAndItsOwnOffEndsIt() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        #expect(await rig.core.verbs == [.tune(on: true)])
        #expect(await rig.core.keepaliveEpochs == [1])
        #expect(await rig.ptt.snapshot.transmitting)
        await rig.ptt.toggleTune()
        await rig.clock.advance(by: 300)
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.tune(on: true), .tune(on: false)])
        #expect(await rig.core.keepalives.count == 1)
        #expect(await !rig.core.keyed)
    }

    /// A2: the Tuner Genius's TUNE is a key of its own kind
    /// (`tx.tunerTune`, link 18.6 at remoteTxVersion 2): it goes through the
    /// one queue, without the microphone, its keepalive runs, and its own
    /// off ends it, as the desktop's remote window sends it
    /// (RemoteTransmitClient.cpp:378-394).
    @Test func tunerTuneKeysThroughTheQueueAndItsOwnOffEndsIt() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        #expect(await rig.core.verbs == [.tunerTune(on: true)])
        #expect(await rig.core.keepaliveEpochs == [1])
        #expect(await rig.ptt.snapshot.tunerTuning)
        #expect(await !rig.ptt.snapshot.tuning)
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        #expect(await rig.microphone.passes == 0)
        await rig.ptt.toggleTunerTune()
        await rig.clock.advance(by: 300)
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false)])
        #expect(await rig.core.keepalives.count == 1)
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
    }

    /// A2: PTT and Stop end the tuner's tune as any key of this device's,
    /// with the tuner's own off.
    @Test func pttAndStopEndTheTunersTune() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.ptt.stopAll()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false),
                                         .tunerTune(on: true), .tunerTune(on: false)])
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.state == .idle)
    }

    /// A2: a tuner's tune the Core accepts after it stopped being wanted is
    /// ended with the tuner's own off: an unkey would leave the cycle
    /// waiting for the amplifier, to key later (RemoteKeying.cpp:444-455,
    /// 535-547 against 654-668).
    @Test func aLateAcceptedTunerTuneIsEndedWithItsOwnOff() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.toggleTunerTune()
        await rig.waitSent(1)
        await rig.ptt.stopAll()
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await rig.core.verbs.first == .tunerTune(on: true))
        #expect(await rig.core.verbs.last == .tunerTune(on: false))
        #expect(await !rig.core.verbs.contains { if case .unkey = $0 { return true }; return false })
        #expect(await !rig.core.keyed)
    }

    /// A2: a tuner's tune with no answer while the link stays up is ended
    /// with the tuner's own off.
    @Test func aTunerTuneWithNoAnswerIsEndedWithItsOwnOff() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.script([.noAnswer])
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.clock.advance(by: 500)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false)])
    }

    /// A2: the Core's `tuneEnded` notice (the cycle ended without keying)
    /// ends the tuner's tune here through its own off, which ends nothing
    /// more at the Core; the keepalive stops.
    @Test func theCoresTuneEndedNoticeEndsTheTunersTuneHere() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        _ = await rig.core.stopOnItsOwn()
        await rig.ptt.tunerTuneEndedUnkeyed()
        #expect(await !rig.ptt.snapshot.keepaliveRunning)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        await rig.clock.advance(by: 500)
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false)])
        #expect(await rig.core.keepalives.isEmpty)
    }

    /// The tuner's carrier deadline in the clock's milliseconds.
    static let carrierDeadlineMs: Int64 = {
        let parts = PttController.tunerCarrierDeadline.components
        return parts.seconds * 1000 + parts.attoseconds / 1_000_000_000_000_000
    }()

    /// Fix wave (fix-tx2 item 2): the Core ends a device's tuner cycle
    /// before its carrier keys without any notice when it has no reason to
    /// give (RemoteKeying.cpp:155-160; RadioModel.cpp:2909, :30764,
    /// :30849). The carrier keys within the Core's 1.5 s amplifier standby
    /// wait or the cycle is over, so a tuner tune with no carrier seen by
    /// the deadline ends here through its own off, with no reason shown.
    @Test func aTunerTuneWhoseCarrierNeverComesEndsAtTheDeadline() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        await rig.clock.advance(by: Self.carrierDeadlineMs - 1)
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.verbs == [.tunerTune(on: true)])
        await rig.clock.advance(by: 1)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false)])
        let snapshot = await rig.ptt.snapshot
        #expect(!snapshot.keepaliveRunning)
        #expect(snapshot.stop == nil)
    }

    /// The deadline is for a carrier that never came: a tuner tune whose
    /// carrier was seen on the air ends when the radio goes off the air.
    @Test func aTunerTuneOnTheAirOutlastsTheDeadline() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        var onAir = TransmitStateReport()
        onAir.keyed = true
        await rig.ptt.update(onAir)
        await rig.clock.advance(by: Self.carrierDeadlineMs * 2)
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.verbs == [.tunerTune(on: true)])
        await rig.ptt.update(TransmitStateReport())
        #expect(await rig.ptt.state == .idle)
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.tunerTune(on: true)])
    }

    /// An earlier tuner tune's deadline ends nothing of a later one.
    @Test func anEarlierTunerTunesDeadlineLeavesALaterOneOn() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.ptt.stopAll()
        await rig.ptt.settle()
        await rig.clock.advance(by: Self.carrierDeadlineMs / 2)
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        await rig.clock.advance(by: Self.carrierDeadlineMs / 2 + 1)
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.verbs == [.tunerTune(on: true), .tunerTune(on: false), .tunerTune(on: true)])
    }

    /// A2: the notice touches only the tuner's tune: a PTT key stays on.
    @Test func aTuneEndedNoticeLeavesAPttKeyOn() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.tunerTuneEndedUnkeyed()
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.core.keyed)
    }

    /// A2: while the PTT's key is on, the tuner's TUNE starts nothing.
    @Test func tunerTuneWhileKeyedSendsNothing() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.toggleTunerTune()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await rig.ptt.snapshot.pttKeyed)
    }

    @Test func stopEndsWhateverIsKeyed() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.stopAll()
        await rig.ptt.settle()
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        await rig.ptt.stopAll()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .unkey(epoch: 1), .tune(on: true),
                                         .tune(on: false)])
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.snapshot.transmitting == false)
    }

    // MARK: The keepalive

    @Test func theKeepaliveGoesEvery100MsWhileKeyedAndStopsAtTheRelease() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        // Before the key's answer the keepalive carries 4294967295.
        await rig.clock.advance(by: 100)
        #expect(await rig.core.keepaliveEpochs == [PttController.unansweredEpoch])
        await rig.core.release()
        await rig.ptt.settle()
        await rig.clock.advance(by: 300)
        #expect(await rig.core.keepaliveSequences == [1, 2, 3, 4])
        #expect(await rig.core.keepaliveEpochs == [PttController.unansweredEpoch, 1, 1, 1])
        await rig.clock.advance(by: 50)
        await rig.ptt.tap()
        // Within one interval of the release none goes, nor ever after.
        await rig.clock.advance(by: 1000)
        #expect(await rig.core.keepalives.count == 4)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
        await rig.ptt.settle()
        await rig.clock.advance(by: 1000)
        #expect(await rig.core.keepalives.count == 4)
    }

    @Test func heartbeatActivityIdentityMatchesRealControllerWindows() async throws {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 200)
        let firstWindow = await rig.core.heartbeatGates
        #expect(firstWindow.count == 2)
        let first = try #require(firstWindow.first)
        #expect(firstWindow.allSatisfy { $0 === first })
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(!first.isOpen)
        await rig.clock.advance(by: 5000)
        #expect(await rig.core.heartbeatGates.count == 2)
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        let second = try #require(await rig.core.heartbeatGates.last)
        #expect(second !== first)
        await rig.ptt.linkChanged(up: false)
        #expect(!second.isOpen)
        await rig.ptt.linkChanged(up: true)
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        let third = try #require(await rig.core.heartbeatGates.last)
        #expect(third !== second && third !== first)
        await rig.ptt.tap()
        await rig.ptt.settle()
    }

    @Test func noKeepaliveGoesWhileUnkeyed() async {
        let rig = Rig()
        await rig.connected()
        await rig.clock.advance(by: 5000)
        #expect(await rig.core.keepalives.isEmpty)
    }

    @Test func theSequenceGoesOnAcrossKeysAndStartsAgainInANewSession() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 200)
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        #expect(await rig.core.keepaliveSequences == [1, 2, 3])
        #expect(await rig.core.keepaliveEpochs == [1, 1, 2])
        await rig.ptt.linkChanged(up: false)
        await rig.ptt.linkChanged(up: true)
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 100)
        #expect(await rig.core.keepaliveSequences == [1, 2, 3, 1])
    }

    @Test func armedVoxKeepsTheKeepaliveGoing() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.setVoxArmed(true)
        await rig.clock.advance(by: 200)
        #expect(await rig.core.keepaliveEpochs == [PttController.unansweredEpoch, PttController.unansweredEpoch])
        await rig.ptt.setVoxArmed(false)
        await rig.clock.advance(by: 500)
        #expect(await rig.core.keepalives.count == 2)
        #expect(await rig.core.sent.isEmpty)
    }

    // MARK: The Core's stops

    @Test func aTimeOutStopEndsTheKeyWithoutAnUnkeyAndSaysWhy() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        let epoch = await rig.core.stopOnItsOwn()
        await rig.ptt.update(Self.stop(serial: 1, epoch: epoch))
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.ptt.snapshot.stop == TransmitStopNotice(reason: "timeOut", text: Self.stopText, serial: 1))
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        await rig.clock.advance(by: 500)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
        // PTT reads Tap: one tap keys afresh, and the notice goes.
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen"), .key(trigger: "screen")])
        #expect(await rig.core.keyed)
        #expect(await rig.ptt.snapshot.stop == nil)
    }

    /// M3: the Core's stop arrives before the key's answer.
    @Test func aStopBeforeTheKeysAnswerEndsItWithoutAKeepalive() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.hold()
        await rig.ptt.tap()
        await rig.waitSent(1)
        let epoch = await rig.core.stopOnItsOwn()
        await rig.ptt.update(Self.stop(serial: 1, epoch: epoch, reason: "micStarved",
                                       text: "No microphone audio arrived from Phone, so the Core stopped transmitting."))
        await rig.core.release()
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.ptt.snapshot.stop?.reason == "micStarved")
        await rig.clock.advance(by: 500)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
    }

    /// C2: the radio goes off the air for this key without a stop or a
    /// release of the phone's: the key is over here too.
    @Test func theRadioGoingOffTheAirEndsTheKey() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.toggleTune()
        await rig.ptt.settle()
        var report = TransmitStateReport()
        report.keyed = true
        await rig.ptt.update(report)
        _ = await rig.core.stopOnItsOwn()
        report.keyed = false
        await rig.ptt.update(report)
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.ptt.snapshot.tuning == false)
        await rig.clock.advance(by: 500)
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
        #expect(await rig.core.verbs == [.tune(on: true)])
    }

    @Test func aStopOfAnOlderKeyLeavesANewerKeyOn() async {
        let rig = Rig()
        await rig.connected()
        await rig.core.script([.accepted(epoch: 5)])
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.update(Self.stop(serial: 1, epoch: 4, reason: "linkLost",
                                       text: "The link to Phone went quiet, so the Core stopped transmitting."))
        #expect(await rig.ptt.state.isKeyed)
        #expect(await rig.ptt.snapshot.stop == nil)
    }

    @Test func theFirstStateOnlyRecordsTheLastStop() async {
        let rig = Rig()
        await rig.ptt.linkChanged(up: true)
        await rig.ptt.update(Self.stop(serial: 7, epoch: 3, reason: "micStarved",
                                       text: "No microphone audio arrived from Phone, so the Core stopped transmitting."))
        #expect(await rig.ptt.snapshot.stop == nil)
    }

    // MARK: The link

    @Test func theLinkGoingWhileKeyedSaysTheCoreStopsAndComingBackReadsTap() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.clock.advance(by: 200)
        await rig.ptt.linkChanged(up: false)
        #expect(await rig.ptt.state == .linkLost)
        let sentBefore = await rig.core.keepalives.count
        await rig.clock.advance(by: 2000)
        #expect(await rig.core.keepalives.count == sentBefore)
        // A tap while the link is down sends nothing.
        await rig.ptt.tap()
        // The new session's snapshot says why the Core stopped; nothing keys by itself.
        await rig.ptt.update(Self.stop(serial: 1, epoch: 1, reason: "linkLost",
                                       text: "The link to Phone went quiet, so the Core stopped transmitting."))
        await rig.ptt.linkChanged(up: true)
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.ptt.snapshot.stop?.reason == "linkLost")
        await rig.clock.advance(by: 2000)
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await rig.core.keepalives.count == sentBefore)
    }

    /// M1: after a reconnect, another device's later stop is never shown as this phone's.
    @Test func afterAReconnectAnotherDevicesStopIsNotThisPhones() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        await rig.ptt.linkChanged(up: false)
        // The new session's first state: no stop happened while away.
        await rig.ptt.update(TransmitStateReport())
        await rig.ptt.linkChanged(up: true)
        #expect(await rig.ptt.snapshot.stop == nil)
        // Later, another device's key is stopped.
        await rig.ptt.update(Self.stop(serial: 1, epoch: 9, reason: "linkLost",
                                       text: "The link to MacBook Pro went quiet, so the Core stopped transmitting."))
        #expect(await rig.ptt.snapshot.stop == nil)
    }

    // MARK: Transmit held elsewhere

    static func heldByMacBook(onAir: Bool) -> TransmitStateReport {
        var report = TransmitStateReport()
        report.held = true
        report.heldHere = false
        report.holderName = "MacBook Pro"
        report.holderShortName = "MacBook"
        report.holderSource = "device"
        report.holderEpoch = 2
        report.keyed = onAir
        return report
    }

    /// T3: each report travels to the actor on its own task, so an older
    /// one may land after a newer one. The newer stands: an older report
    /// arriving late changes nothing.
    @Test func anOlderTransmitStateReportArrivingLateChangesNothing() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.update(Self.heldByMacBook(onAir: false), serial: 1)
        await rig.ptt.update(TransmitStateReport(), serial: 2)
        #expect(await rig.ptt.state == .idle)
        // Serial 1's twin, late: transmit is free, as serial 2 said.
        await rig.ptt.update(Self.heldByMacBook(onAir: true), serial: 1)
        #expect(await rig.ptt.state == .idle)
        // Newer reports still apply.
        await rig.ptt.update(Self.heldByMacBook(onAir: false), serial: 3)
        #expect(await rig.ptt.state == .heldElsewhere(device: "MacBook", onAir: false))
        await rig.ptt.update(TransmitStateReport(), serial: 4)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.ptt.state.isKeyed, "the stale holder never blocks a key")
    }

    @Test func aTapWhileAnotherDeviceHoldsTransmitSendsNothing() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.update(Self.heldByMacBook(onAir: false))
        #expect(await rig.ptt.state == .heldElsewhere(device: "MacBook", onAir: false))
        await rig.ptt.tap()
        await rig.ptt.setTune(true)
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
        await rig.ptt.update(Self.heldByMacBook(onAir: true))
        #expect(await rig.ptt.state == .heldElsewhere(device: "MacBook", onAir: true))
        // Transmit comes free: PTT reads Tap and one tap keys.
        await rig.ptt.update(TransmitStateReport())
        #expect(await rig.ptt.state == .idle)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
    }

    @Test func transmitTakenWhileKeyedEndsHereWithoutAnUnkey() async {
        let rig = Rig()
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        let epoch = await rig.core.stopOnItsOwn()
        var taken = Self.stop(serial: 1, epoch: epoch, reason: "takenOver",
                              text: "Radio took transmit, so the Core stopped transmitting.")
        taken.held = true
        taken.holderName = "Radio"
        taken.holderShortName = "Radio"
        taken.holderSource = TransmitStateReport.radioPttSource
        taken.keyed = true
        await rig.ptt.update(taken)
        #expect(await rig.ptt.state == .heldElsewhere(device: "Radio", onAir: true))
        #expect(await rig.ptt.snapshot.stop?.reason == "takenOver")
        await rig.clock.advance(by: 500)
        await rig.ptt.settle()
        #expect(await rig.core.verbs == [.key(trigger: "screen")])
        #expect(await rig.ptt.snapshot.keepaliveRunning == false)
    }

    @Test func whileTransmitChangesHandsPttWaits() async {
        let rig = Rig()
        await rig.connected()
        var report = Self.heldByMacBook(onAir: false)
        report.holderTransferring = true
        await rig.ptt.update(report)
        #expect(await rig.ptt.state == .waiting)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
        report.holderTransferring = false
        report.held = true
        report.heldHere = true
        await rig.ptt.update(report)
        #expect(await rig.ptt.state == .idle)
    }

    @Test func holdingTransmitHereLeavesThePttAlone() async {
        let rig = Rig()
        await rig.connected()
        var report = TransmitStateReport()
        report.held = true
        report.heldHere = true
        await rig.ptt.update(report)
        #expect(await rig.ptt.state == .idle)
    }

    @Test func theSnapshotsHolderStandsWhenTheLinkComesUp() async {
        let rig = Rig()
        // The Core's snapshot carries txState before the session is ready.
        await rig.ptt.update(Self.heldByMacBook(onAir: false))
        await rig.ptt.linkChanged(up: true)
        #expect(await rig.ptt.state == .heldElsewhere(device: "MacBook", onAir: false))
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
    }

    @Test func aHolderAwayIsStillElsewhere() async {
        let rig = Rig()
        await rig.connected()
        var report = Self.heldByMacBook(onAir: false)
        report.holderAway = true
        await rig.ptt.update(report)
        #expect(await rig.ptt.state == .heldElsewhere(device: "MacBook", onAir: false))
    }

    // MARK: The microphone (Task 55)

    /// A start that fails sends nothing, and PTT says why under the Core's
    /// own code for a key without its microphone.
    @Test func aMicrophoneThatDoesNotStartSendsNothingAndSaysWhy() async {
        let reason = "NereusSDR can't use this phone's microphone."
        let rig = Rig(startMicrophone: { .failed(reason: reason) })
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
        #expect(await rig.ptt.state == .refused(TxRefusalInfo(reason: reason, code: "micNotReady")))
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        await rig.clock.advance(by: 1000)
        #expect(await rig.core.keepalives.isEmpty)
    }

    /// The re-review's finding: a start that does not return is given up
    /// after its deadline, the key is never sent and PTT says so.
    @Test func aMicrophoneStartThatNeverEndsIsGivenUpAtItsDeadline() async {
        let rig = Rig()
        await rig.microphone.close()
        await rig.connected()
        await rig.ptt.tap()
        await rig.waitScheduled(in: 2000)
        #expect(await rig.ptt.state == .keying)
        await rig.clock.advance(by: 2000)
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
        #expect(await rig.ptt.state == .refused(TxRefusalInfo(reason: PttController.microphoneDidNotStartText,
                                                               code: PttController.microphoneRefusalCode)))
        // A start that ends after its deadline changes nothing.
        await rig.microphone.openUp()
        await rig.ptt.settle()
        #expect(await rig.core.sent.isEmpty)
    }

    /// A release queued behind a start that never returns does not hold PTT
    /// in unkeying: at the deadline the key is dropped unsent and the release
    /// goes, and PTT is idle.
    @Test func aReleaseBehindAStuckMicrophoneStartSettlesAtTheDeadline() async {
        let rig = Rig()
        await rig.microphone.close()
        await rig.connected()
        await rig.ptt.tap()
        await rig.waitScheduled(in: 2000)
        await rig.ptt.tap()
        #expect(await rig.ptt.state == .unkeying)
        await rig.clock.advance(by: 2000)
        await rig.ptt.settle()
        #expect(await rig.ptt.state == .idle)
        #expect(await rig.core.verbs == [.unkey(epoch: PttController.unansweredEpoch)])
        #expect(await !rig.core.keyed)
        await rig.microphone.openUp()
    }

    /// The microphone runs while the PTT's key is on or on its way and while
    /// VOX is armed; it stops at the release, and TUNE never wants it.
    @Test func theMicrophoneIsWantedWhileThePttKeyOrVoxIsOn() async {
        let rig = Rig()
        await rig.connected()
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.tap()
        #expect(await rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.settle()
        #expect(await rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.tap()
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.settle()

        await rig.ptt.setTune(true)
        await rig.ptt.settle()
        #expect(await rig.ptt.snapshot.tuning)
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.setTune(false)
        await rig.ptt.settle()

        await rig.ptt.setVoxArmed(true)
        #expect(await rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.setVoxArmed(false)
        #expect(await !rig.ptt.snapshot.microphoneWanted)
        await rig.ptt.setVoxArmed(true)
        await rig.ptt.linkChanged(up: false)
        #expect(await !rig.ptt.snapshot.microphoneWanted)
    }

    @Test func stopAndMicrophoneLossRejectRetiredOwnerOrAuthorityAtAdmission() async {
        let rig = Rig()
        await rig.ptt.logicalSessionChanged(2)
        await rig.connected()
        await rig.ptt.tap()
        await rig.ptt.settle()
        let before = await rig.core.sent.count
        let old = CommandSendPermit()
        await rig.ptt.stopAll(owner: 1, authority: old)
        await rig.ptt.microphoneLost("Old microphone loss", owner: 1, authority: old)
        let revoked = CommandSendPermit()
        revoked.revoke()
        await rig.ptt.stopAll(owner: 2, authority: revoked)
        await rig.ptt.microphoneLost("Retired microphone loss", owner: 2, authority: revoked)
        await rig.ptt.settle()
        #expect(await rig.core.sent.count == before)
        #expect(await rig.core.keyed)
        #expect(await rig.ptt.snapshot.state.isKeyed)
        #expect(await rig.ptt.snapshot.stop == nil)

        let current = CommandSendPermit()
        await rig.ptt.microphoneLost("Current microphone loss", owner: 2, authority: current)
        await rig.ptt.settle()
        #expect(await !rig.core.keyed)
        #expect(await rig.ptt.snapshot.stop?.text == "Current microphone loss")
    }

    @Test func stopAuthorityIsRetainedFromActorAdmissionThroughOffHandoff() async {
        let core = StopHandoffCore()
        let ptt = PttController(commands: core, clock: ManualLinkClock())
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        await ptt.settle()
        let authority = CommandSendPermit()
        await ptt.stopAll(owner: 1, authority: authority)
        await core.boundary.whenWaiting()
        // Retirement wins even before the actor receives a replacement owner.
        authority.revoke()
        await core.boundary.open()
        await ptt.settle()
        #expect(!core.receipt.wasSent)

        await ptt.logicalSessionChanged(2)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        await ptt.settle()
        await ptt.stopAll(owner: 2, authority: CommandSendPermit())
        await ptt.settle()
        #expect(core.receipt.wasSent, "a valid same-session off still reaches handoff")
    }

    private actor StopHandoffCore: TransmitCommandSending {
        nonisolated let boundary = StopHandoffBarrier()
        nonisolated let receipt = CommandHandoffReceipt()

        func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
            await send(verb, copies: copies, authority: CommandSendPermit())
        }

        func send(_ verb: TransmitVerb, copies: Int, authority: CommandSendPermit) async -> TransmitPending {
            if case .unkey = verb {
                await boundary.wait()
                let final = CommandSendPermit(parents: [authority], handoffReceipt: receipt)
                let handedOff = final.handoff { true }
                return TransmitPending { handedOff ? .accepted(epoch: nil) : .noAnswer }
            }
            return TransmitPending { .accepted(epoch: 7) }
        }

        func sendKeepalive(sequence: Int64, epoch: Int64) async {}
    }

    private actor StopHandoffBarrier {
        private var waiting = false
        private var isOpen = false
        private var entrance: CheckedContinuation<Void, Never>?
        private var release: CheckedContinuation<Void, Never>?

        func whenWaiting() async {
            if !waiting { await withCheckedContinuation { entrance = $0 } }
        }

        func wait() async {
            waiting = true
            entrance?.resume()
            entrance = nil
            if !isOpen { await withCheckedContinuation { release = $0 } }
        }

        func open() {
            isOpen = true
            release?.resume()
            release = nil
        }
    }

    @Test func activeKeyOrTuneWinsBeforeIdleSleepAdmission() async {
        for tune in [false, true] {
            let rig = Rig()
            let permit = CommandSendPermit()
            await rig.ptt.logicalSessionChanged(1)
            await rig.connected()
            if tune { await rig.ptt.setTune(true) } else { await rig.ptt.tap() }
            #expect(await !rig.ptt.beginIdleSleepRetirement(owner: 1, authority: permit))
            await rig.ptt.settle()
            #expect(await rig.core.keyed)
            await rig.ptt.stopAll()
            await rig.ptt.settle()
            #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: permit))
            await rig.ptt.endIdleSleepRetirement(owner: 1, authority: permit)
        }
    }

    @Test func idleSleepAdmissionBlocksNewKeyTuneAndVoxUntilMatchingRelease() async {
        let rig = Rig()
        let old = CommandSendPermit()
        let other = CommandSendPermit()
        await rig.ptt.logicalSessionChanged(1)
        await rig.connected()
        await rig.ptt.setVoxArmed(true)
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: old))
        await rig.ptt.setVoxArmed(false, owner: 1, authority: old)
        #expect(await !rig.ptt.mayArmVox(owner: 1, authority: old))
        await rig.ptt.tap()
        await rig.ptt.setTune(true)
        await rig.ptt.setTwoTone(true)
        await rig.ptt.setVoxArmed(true, owner: 1, authority: old)
        await rig.ptt.settle()
        #expect(await rig.core.verbs.isEmpty)
        #expect(await !rig.ptt.snapshot.voxArmed)
        #expect(await rig.ptt.snapshot.state == .idle)
        await rig.ptt.endIdleSleepRetirement(owner: 1, authority: other)
        #expect(await !rig.ptt.mayArmVox(owner: 1, authority: old))
        await rig.ptt.endIdleSleepRetirement(owner: 1, authority: old)
        #expect(await rig.ptt.mayArmVox(owner: 1, authority: old))
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.keyed)
    }

    @Test func replacedOwnerRejectsAndCannotBeClearedByOldSleepCompletion() async {
        let rig = Rig()
        let old = CommandSendPermit()
        let fresh = CommandSendPermit()
        await rig.ptt.logicalSessionChanged(1)
        await rig.connected()
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: old))
        await rig.ptt.logicalSessionChanged(2)
        await rig.ptt.linkChanged(up: true)
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 2, authority: fresh))
        await rig.ptt.endIdleSleepRetirement(owner: 1, authority: old)
        #expect(await !rig.ptt.mayArmVox(owner: 2, authority: fresh))
        await rig.ptt.endIdleSleepRetirement(owner: 2, authority: fresh)
        #expect(await rig.ptt.mayArmVox(owner: 2, authority: fresh))
    }

    @Test func linkCycleRevokesHeldIdleSleepAuthorityBeforeNewKey() async {
        let rig = Rig()
        let old = CommandSendPermit()
        await rig.ptt.logicalSessionChanged(1)
        await rig.connected()
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: old))
        await rig.ptt.linkChanged(up: false)
        #expect(old.isRevoked)
        await rig.ptt.linkChanged(up: true)
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.keyed)
    }

    @Test func cancelledIdleSleepGateDoesNotDelayNewKeyOrExpiry() async {
        let rig = Rig()
        let old = CommandSendPermit()
        let fresh = CommandSendPermit()
        await rig.ptt.logicalSessionChanged(1)
        await rig.connected()
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: old))
        old.revoke()
        #expect(await rig.ptt.beginIdleSleepRetirement(owner: 1, authority: fresh))
        fresh.revoke()
        await rig.ptt.tap()
        await rig.ptt.settle()
        #expect(await rig.core.keyed)
    }
}
