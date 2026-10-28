// NereusSDR for iOS: deterministic CW request/result/report retirement tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Testing
@testable import NereusLink

@MainActor @Suite struct RemoteCwLeaseTests {
    @Test func reportBeforeResultKeepsOneLeaseThroughHoldAndRepeatGap() {
        let lease = RemoteCwLease()
        var activities: [RemoteCwLease.Activity] = []
        lease.onActivity = { activities.append($0) }
        lease.reset(session: 7)
        let request = lease.begin()
        #expect(activities.last?.epoch == 0)
        lease.observe(.init(deviceId: "0011", epoch: 8, active: true), thisDeviceId: "0011")
        #expect(lease.epoch == 8)
        lease.complete(request, acceptedEpoch: 8, active: true)
        // Hold, MOX and microphone state are deliberately absent from this
        // lifecycle: only sender completion or a local Stop ends the lease.
        #expect(lease.epoch == 8 && activities.last?.epoch == 8)
        lease.observe(.init(deviceId: "0011", epoch: 8, active: false), thisDeviceId: "0011")
        #expect(lease.epoch == nil && activities.last?.epoch == nil)
    }

    @Test func stopRetiresDelayedResultAndUnknownOldTupleBeforeNewResult() {
        let lease = RemoteCwLease()
        lease.reset(session: 1)
        let old = lease.begin()
        lease.stop()
        #expect(old.authority.isRevoked)
        lease.complete(old, acceptedEpoch: 12, active: true)
        lease.observe(.init(deviceId: "00aa", epoch: 12, active: true), thisDeviceId: "00aa")
        #expect(lease.epoch == nil)
        let next = lease.begin()
        // No request id exists on the tuple: after Stop, wait for the new
        // command result before adopting any previously unknown epoch.
        lease.observe(.init(deviceId: "00aa", epoch: 12, active: true), thisDeviceId: "00aa")
        #expect(lease.epoch == nil)
        lease.complete(next, acceptedEpoch: 13, active: true)
        lease.observe(.init(deviceId: "00aa", epoch: 12, active: false), thisDeviceId: "00aa")
        #expect(lease.epoch == 13)
    }

    @Test func completionBeforeResultCannotReviveAnEpoch() {
        let lease = RemoteCwLease()
        let request = lease.begin()
        lease.observe(.init(deviceId: "aa", epoch: 22, active: true), thisDeviceId: "aa")
        lease.observe(.init(deviceId: "aa", epoch: 22, active: false), thisDeviceId: "aa")
        lease.complete(request, acceptedEpoch: 22, active: true)
        #expect(lease.epoch == nil)
    }

    @Test func sessionRetirementRevokesBothStartsAndReleaseAndRejectsTheirAnswers() {
        let lease = RemoteCwLease()
        lease.reset(session: 1)
        let old = lease.begin()
        let release = lease.releaseAuthority
        lease.reset(session: 2)
        lease.complete(old, acceptedEpoch: 34, active: true)
        #expect(old.authority.isRevoked && release.isRevoked && lease.epoch == nil)
        let next = lease.begin()
        lease.complete(next, acceptedEpoch: 1, active: true)
        #expect(lease.epoch == 1)
    }

    @Test func inactiveOldEpochAndOtherDeviceCannotEndOrReplaceNewerContent() {
        let lease = RemoteCwLease()
        let first = lease.begin()
        lease.complete(first, acceptedEpoch: 100, active: true)
        let next = lease.begin()
        lease.complete(next, acceptedEpoch: 101, active: true)
        lease.observe(.init(deviceId: "aa", epoch: 100, active: false), thisDeviceId: "aa")
        lease.observe(.init(deviceId: "bb", epoch: 101, active: false), thisDeviceId: "aa")
        #expect(lease.epoch == 101)
    }

    @Test func newerOtherOwnerRetiresOurContentButOlderActiveTupleCannot() {
        let lease = RemoteCwLease()
        let request = lease.begin()
        lease.complete(request, acceptedEpoch: 101, active: true)
        lease.observe(.init(deviceId: "bb", epoch: 100, active: true), thisDeviceId: "aa")
        #expect(lease.epoch == 101)
        let append = lease.begin()
        lease.observe(.init(deviceId: "bb", epoch: 102, active: true), thisDeviceId: "aa")
        #expect(lease.epoch == nil && append.authority.isRevoked)
        lease.complete(append, acceptedEpoch: 101, active: true)
        lease.observe(.init(deviceId: "aa", epoch: 101, active: true), thisDeviceId: "aa")
        #expect(lease.epoch == nil)
    }

    @Test(arguments: [Int64(11), Int64(13)], [false, true])
    func foreignSuccessorBeforeFirstResultRetiresOnlyOlderAcceptedEpochs(acceptedEpoch: Int64, foreignActive: Bool) {
        let lease = RemoteCwLease()
        let request = lease.begin()
        lease.observe(.init(deviceId: "bb", epoch: 12, active: foreignActive), thisDeviceId: "aa")
        lease.complete(request, acceptedEpoch: acceptedEpoch, active: true)
        #expect(lease.epoch == (acceptedEpoch > 12 ? acceptedEpoch : nil))
        #expect(lease.hasActivity == (acceptedEpoch > 12))
    }

    @Test func invalidForeignTupleCannotInventARetirementWatermark() {
        let lease = RemoteCwLease()
        let request = lease.begin()
        lease.observe(nil, thisDeviceId: "aa")
        lease.observe(.init(deviceId: "", epoch: 12, active: false), thisDeviceId: "aa")
        lease.observe(.init(deviceId: "bb", epoch: 0, active: true), thisDeviceId: "aa")
        lease.observe(.init(deviceId: "bb", epoch: Int64(UInt32.max) + 1, active: false), thisDeviceId: "aa")
        lease.complete(request, acceptedEpoch: 11, active: true)
        #expect(lease.epoch == 11)
    }

    @Test(arguments: [Int64(-1), 0, Int64(UInt32.max) + 1])
    func malformedAcceptedEpochCannotCreateLiveness(epoch: Int64) {
        let lease = RemoteCwLease()
        let request = lease.begin()
        lease.complete(request, acceptedEpoch: epoch, active: true)
        #expect(lease.epoch == nil)
    }

    @Test func revocationClosesCapturedHeartbeatAtSynchronousStopBoundary() {
        let lease = RemoteCwLease()
        var latest: RemoteCwLease.Activity?
        lease.onActivity = { latest = $0 }
        let request = lease.begin()
        lease.complete(request, acceptedEpoch: 90, active: true)
        let gate = TransmitHeartbeatGate(authorities: [latest!.authority])
        gate.setOpen(true)
        #expect(gate.handoff { true })
        lease.stop()
        #expect(!gate.handoff { Issue.record("retired CW record reached handoff"); return true })
    }
}
