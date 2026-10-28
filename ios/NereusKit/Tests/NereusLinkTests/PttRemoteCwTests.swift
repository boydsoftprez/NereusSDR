// NereusSDR for iOS: CW uses the PTT's existing timer without microphone/MOX
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Testing
import Foundation
import LinkSessionTestSupport
@testable import NereusLink

@Suite struct PttRemoteCwTests {
    struct Record: Equatable, Sendable { let sequence: Int64; let key: Int64; let cw: Int64 }
    actor Wire: TransmitCommandSending {
        var records: [Record] = []
        func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending { TransmitPending { .accepted(epoch: 17) } }
        func sendKeepalive(sequence: Int64, epoch: Int64) async {}
        func sendKeepalive(sequence: Int64, epoch: Int64, cwEpoch: Int64, gate: TransmitHeartbeatGate,
                           stillAllowed: @escaping @Sendable () async -> Bool) async {
            guard await stillAllowed(), gate.handoff({ true }) else { return }
            records.append(Record(sequence: sequence, key: epoch, cw: cwEpoch))
        }
    }

    @Test func pendingCwOnlyThenImmediateAcceptedEpochAndOriginalGrid() async {
        let wire = Wire()
        let clock = ManualLinkClock()
        let ptt = PttController(commands: wire, clock: clock)
        await ptt.linkChanged(up: true)
        let session = await ptt.sessionGeneration
        await ptt.setRemoteCwActivity(epoch: 0, session: session)
        #expect(await ptt.snapshot.keepaliveRunning)
        #expect(await !ptt.snapshot.microphoneWanted)
        #expect(await !ptt.snapshot.transmitting)
        #expect(await wire.records == [Record(sequence: 1, key: 0, cw: 0)])
        await clock.advance(by: 100)
        #expect(await wire.records == [Record(sequence: 1, key: 0, cw: 0), Record(sequence: 2, key: 0, cw: 0)])
        await clock.advance(by: 35)
        await ptt.setRemoteCwActivity(epoch: 9, session: session)
        #expect(await wire.records.last == Record(sequence: 3, key: 0, cw: 9))
        await clock.advance(by: 65)
        #expect(await wire.records.last == Record(sequence: 4, key: 0, cw: 9))
        await ptt.setRemoteCwActivity(epoch: nil, session: session)
        await clock.advance(by: 500)
        #expect(await wire.records.count == 4)
        #expect(await !ptt.snapshot.keepaliveRunning)
    }

    @Test func micReleaseDoesNotEndCwAndCwCompletionDoesNotEndMic() async {
        let wire = Wire()
        let clock = ManualLinkClock()
        let ptt = PttController(commands: wire, clock: clock)
        await ptt.linkChanged(up: true)
        let session = await ptt.sessionGeneration
        await ptt.tap()
        await ptt.settle()
        await ptt.setRemoteCwActivity(epoch: 33, session: session)
        #expect(await wire.records.last?.key == 17)
        #expect(await wire.records.last?.cw == 33)
        await ptt.tap()
        await ptt.settle()
        await clock.advance(by: 100)
        #expect(await wire.records.last?.cw == 33)
        #expect(await wire.records.last?.key == 0)
        #expect(await !ptt.snapshot.microphoneWanted)
        #expect(await ptt.snapshot.keepaliveRunning)
        await ptt.tap()
        await ptt.settle()
        await ptt.setRemoteCwActivity(epoch: nil, session: session)
        await clock.advance(by: 100)
        #expect(await wire.records.last?.cw == 0)
        #expect(await wire.records.last?.key == 17)
        #expect(await ptt.snapshot.microphoneWanted)
        #expect(await ptt.snapshot.keepaliveRunning)
        await ptt.stopAll()
        await ptt.settle()
    }

    @Test func holdAndRepeatGapRetainCwButOldSessionAndRevokedAdmissionDoNot() async {
        let wire = Wire()
        let clock = ManualLinkClock()
        let ptt = PttController(commands: wire, clock: clock)
        await ptt.linkChanged(up: true)
        let session = await ptt.sessionGeneration
        await ptt.setRemoteCwActivity(epoch: 44, session: session)
        var held = TransmitStateReport()
        held.rfHeld = true
        await ptt.update(held)
        await clock.advance(by: 300)
        #expect(await wire.records.map(\.cw) == [44, 44, 44, 44])
        await ptt.linkChanged(up: false)
        await ptt.linkChanged(up: true)
        await ptt.setRemoteCwActivity(epoch: 44, session: session)
        let revoked = CommandSendPermit()
        revoked.revoke()
        await ptt.setRemoteCwActivity(epoch: 55, session: ptt.sessionGeneration, authority: revoked)
        await clock.advance(by: 300)
        #expect(await wire.records.count == 4)
    }
    @MainActor @Test func stoppedLeaseResultAndTupleCannotRenewReleasedMicOrReviveCw() async {
        let wire = Wire()
        let clock = ManualLinkClock()
        let ptt = PttController(commands: wire, clock: clock)
        await ptt.linkChanged(up: true)
        let lease = RemoteCwLease()
        var admissions: [Task<Void, Never>] = []
        lease.onActivity = { activity in
            admissions.append(Task {
                await ptt.setRemoteCwActivity(epoch: activity.epoch, session: activity.session,
                                             authority: activity.authority)
            })
        }
        lease.reset(session: await ptt.sessionGeneration)
        let request = lease.begin()
        lease.complete(request, acceptedEpoch: 50, active: true)
        for admission in admissions { await admission.value }
        await ptt.tap()
        await ptt.settle()
        let delayedAppend = lease.begin()
        lease.stop()
        lease.complete(delayedAppend, acceptedEpoch: 50, active: true)
        lease.observe(.init(deviceId: "aa", epoch: 50, active: true), thisDeviceId: "aa")
        for admission in admissions { await admission.value }
        await clock.advance(by: 100)
        #expect(await wire.records.last?.cw == 0)
        #expect(await wire.records.last?.key == 17)
        await ptt.tap()
        await ptt.settle()
        let stoppedCount = await wire.records.count
        await clock.advance(by: 500)
        #expect(await wire.records.count == stoppedCount)
        #expect(await !ptt.snapshot.keepaliveRunning)
    }

    @MainActor @Test(arguments: [false, true])
    func foreignSuccessorBeforeFirstResultCannotCreatePhantomHeartbeat(foreignActive: Bool) async {
        let wire = Wire()
        let clock = ManualLinkClock()
        let ptt = PttController(commands: wire, clock: clock)
        await ptt.linkChanged(up: true)
        let lease = RemoteCwLease()
        var admissions: [Task<Void, Never>] = []
        lease.onActivity = { activity in
            admissions.append(Task {
                await ptt.setRemoteCwActivity(epoch: activity.epoch, session: activity.session,
                                             authority: activity.authority)
            })
        }
        lease.reset(session: await ptt.sessionGeneration)
        let delayed = lease.begin()
        for admission in admissions { await admission.value }
        #expect(await ptt.snapshot.keepaliveRunning)
        lease.observe(.init(deviceId: "bb", epoch: 12, active: foreignActive), thisDeviceId: "aa")
        lease.complete(delayed, acceptedEpoch: 11, active: true)
        for admission in admissions { await admission.value }
        #expect(lease.epoch == nil)
        #expect(await !ptt.snapshot.keepaliveRunning)
        let retiredCount = await wire.records.count
        await clock.advance(by: 300)
        #expect(await wire.records.count == retiredCount)
        #expect(await wire.records.allSatisfy { $0.cw == 0 })
        // The watermark rejects old ownership, not a legitimate successor.
        let successor = lease.begin()
        lease.complete(successor, acceptedEpoch: 13, active: true)
        for admission in admissions { await admission.value }
        #expect(lease.epoch == 13)
        #expect(await wire.records.last?.cw == 13)
        #expect(await ptt.snapshot.keepaliveRunning)
        lease.stop()
        for admission in admissions { await admission.value }
    }

}
