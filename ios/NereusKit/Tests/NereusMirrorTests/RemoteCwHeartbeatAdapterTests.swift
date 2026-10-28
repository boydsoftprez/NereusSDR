// NereusSDR for iOS: CW v2 heartbeat fanout and final handoff fencing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@Suite struct RemoteCwHeartbeatAdapterTests {
    private final class Media: @unchecked Sendable {
        private let lock = NSLock()
        private var values: [[Int64]] = []
        var records: [[Int64]] { lock.withLock { values } }
        func send(_ sequence: Int64, _ epoch: Int64, _ cwEpoch: Int64) -> Bool {
            lock.withLock { values.append([sequence, epoch, cwEpoch]) }
            return true
        }
    }

    private final class Handoffs: @unchecked Sendable {
        private let lock = NSLock()
        private var sent: [LinkMessage] = []
        var messages: [LinkMessage] { lock.withLock { sent } }
        func append(_ message: LinkMessage) { lock.withLock { sent.append(message) } }
    }

    private actor Barrier {
        private var entered = false
        private var enterWaiters: [CheckedContinuation<Void, Never>] = []
        private var releaseWaiter: CheckedContinuation<Void, Never>?
        func park() async {
            entered = true
            for waiter in enterWaiters { waiter.resume() }
            enterWaiters.removeAll()
            await withCheckedContinuation { releaseWaiter = $0 }
        }
        func waitUntilEntered() async {
            if entered { return }
            await withCheckedContinuation { enterWaiters.append($0) }
        }
        func release() { releaseWaiter?.resume(); releaseWaiter = nil }
    }

    @Test func bothOpenPathsReceiveIdenticalRecordAndPrimarySurvivesMissingMedia() async throws {
        let sent = SentMessages()
        let command = CommandClient(clock: ManualLinkClock(), send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let media = Media()
        let client = TransmitCommandClient(commands: command, keepaliveOnMediaV2: media.send,
                                            remoteCwV2: { true })
        let gate = TransmitHeartbeatGate()
        gate.setOpen(true)
        await client.sendKeepalive(sequence: 0x01020304, epoch: 7, cwEpoch: 99, gate: gate, stillAllowed: { true })
        #expect(media.records == [[0x01020304, 7, 99]])
        #expect(sent.messages.count == 1)
        guard case .commandInvoke(let invoke)? = sent.messages.first else {
            Issue.record("primary path did not receive CW keepalive"); return
        }
        #expect(invoke.verb == "tx.keepalive")
        #expect(invoke.args == [
            .init(name: "sequence", value: .i64(0x01020304)),
            .init(name: "epoch", value: .i64(7)),
            .init(name: "cwEpoch", value: .i64(99)),
        ])
        let noMedia = TransmitCommandClient(commands: command, keepaliveOnMediaV2: { _, _, _ in false },
                                             remoteCwV2: { true })
        await noMedia.sendKeepalive(sequence: 0x01020305, epoch: 7, cwEpoch: 99, gate: gate, stillAllowed: { true })
        #expect(sent.messages.count == 2)
    }

    @Test func oldCoreUsesUnchangedLegacyRecordWithoutCwArgument() async {
        let sent = SentMessages()
        let command = CommandClient(clock: ManualLinkClock(), send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let media = Media()
        let client = TransmitCommandClient(commands: command, keepaliveOnMedia: { media.send($0, $1, 0) },
                                            keepaliveOnMediaV2: { _, _, _ in Issue.record("v1 used v2 media"); return true },
                                            remoteCwV2: { false })
        let gate = TransmitHeartbeatGate()
        gate.setOpen(true)
        await client.sendKeepalive(sequence: 1, epoch: 7, cwEpoch: 0, gate: gate, stillAllowed: { true })
        #expect(media.records == [[1, 7, 0]] && sent.messages.isEmpty)
    }

    @Test func stopDuringPrimaryBackpressureCannotSendCapturedCwAfterResume() async {
        let sent = SentMessages()
        let media = Media()
        let handoffs = Handoffs()
        let barrier = Barrier()
        let command = CommandClient(clock: ManualLinkClock(), send: sent.sender, captureGuardedSender: {
            { message, gate in
                await barrier.park()
                _ = gate.handoff { handoffs.append(message); return true }
            }
        })
        await command.handle(.stateChanged(.ready))
        let client = TransmitCommandClient(commands: command, keepaliveOnMediaV2: media.send,
                                            remoteCwV2: { true })
        let authority = CommandSendPermit()
        let gate = TransmitHeartbeatGate(authorities: [authority])
        gate.setOpen(true)
        let operation = Task {
            await client.sendKeepalive(sequence: 1, epoch: 0, cwEpoch: 99, gate: gate, stillAllowed: { true })
        }
        await barrier.waitUntilEntered()
        authority.revoke()
        await barrier.release()
        await operation.value
        #expect(media.records == [[1, 0, 99]])
        #expect(handoffs.messages.isEmpty)
    }
}
