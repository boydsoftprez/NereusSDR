// NereusSDR for iOS: in-memory transmit diagnostic handoff regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@Suite struct TransmitDiagnosticHandoffTests {
    private enum HandoffFailure: Error { case refused }
    private final class Wire: @unchecked Sendable {
        private let lock = NSLock()
        private var media = true
        private var fail = false
        private var mediaCount = 0
        private var postCount = 0
        func configure(media: Bool, fail: Bool) { lock.withLock { self.media = media; self.fail = fail } }
        func mediaHandoff() -> Bool {
            lock.withLock { guard media else { return false }; mediaCount += 1; return true }
        }
        func postHandoff() throws {
            try lock.withLock { guard !fail else { throw HandoffFailure.refused }; postCount += 1 }
        }
        var counts: [Int] { lock.withLock { [mediaCount, postCount] } }
    }
    private final class Recorded: @unchecked Sendable {
        private let lock = NSLock()
        private var held: [LinkDiagnostics.Event] = []
        func append(_ event: LinkDiagnostics.Event) { lock.withLock { held.append(event) } }
        var events: [LinkDiagnostics.Event] { lock.withLock { held } }
    }

    @Test(arguments: [false, true])
    func failedFallbackDoesNotAdvanceSuccessfulSendBaseline(gated: Bool) async {
        let clock = ManualLinkClock()
        let wire = Wire()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let command = CommandClient(clock: clock, send: { _ in try wire.postHandoff() })
        await command.handle(.stateChanged(.ready))
        let client = TransmitCommandClient(commands: command, keepaliveOnMedia: { _, _ in wire.mediaHandoff() },
                                           diagnostics: diagnostics)
        let gate = TransmitHeartbeatGate()
        gate.setOpen(true)
        await send(client, gated: gated, gate: gate, sequence: 1)
        await clock.advance(by: 200)
        wire.configure(media: false, fail: true)
        await send(client, gated: gated, gate: gate, sequence: 2)
        #expect(recorded.events.isEmpty)
        await clock.advance(by: 200)
        wire.configure(media: false, fail: false)
        await send(client, gated: gated, gate: gate, sequence: 3)
        #expect(recorded.events == [.keepaliveGap(milliseconds: 400, scene: .inactive, suppressed: 0, observedAt: 400)])
        #expect(wire.counts == [1, 1])
    }

    @Test func deniedGatedSendAndIntentionalIdleDoNotCreateGap() async {
        let clock = ManualLinkClock()
        let wire = Wire()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let command = CommandClient(clock: clock, send: { _ in try wire.postHandoff() })
        await command.handle(.stateChanged(.ready))
        let client = TransmitCommandClient(commands: command, keepaliveOnMedia: { _, _ in wire.mediaHandoff() },
                                           diagnostics: diagnostics)
        let first = TransmitHeartbeatGate()
        first.setOpen(true)
        await send(client, gated: true, gate: first, sequence: 1)
        await clock.advance(by: 200)
        await client.sendKeepalive(sequence: 2, epoch: 1, gate: first, stillAllowed: { false })
        first.retire()
        await clock.advance(by: 5000)
        let second = TransmitHeartbeatGate()
        second.setOpen(true)
        await send(client, gated: true, gate: second, sequence: 3)
        #expect(recorded.events.isEmpty)
        #expect(wire.counts == [2, 0])
    }

    @Test func innerPostSuccessHookCapturesTimeBeforeCallerProgress() async throws {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let activity = TransmitHeartbeatGate()
        let wire = Wire()
        let command = CommandClient(clock: clock, send: { _ in
            try wire.postHandoff()
            if wire.counts[1] == 2 { await clock.advance(by: 300) }
        })
        await command.handle(.stateChanged(.ready))
        for _ in 0..<2 {
            try await command.post("tx.keepalive", arguments: [],
                onSuccessfulHandoff: { diagnostics.successfulKeepaliveHandoff(activity: activity) })
        }
        await clock.advance(by: 500)
        #expect(recorded.events == [.keepaliveGap(milliseconds: 300, scene: .inactive, suppressed: 0, observedAt: 300)])
    }

    @Test func ungatedStructCopyKeepsItsDiagnosticActivity() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let command = CommandClient(clock: clock, send: { _ in })
        let client = TransmitCommandClient(commands: command, keepaliveOnMedia: { _, _ in true }, diagnostics: diagnostics)
        let copy = client
        await client.sendKeepalive(sequence: 1, epoch: 1)
        await clock.advance(by: 200)
        await copy.sendKeepalive(sequence: 2, epoch: 1)
        #expect(recorded.events == [.keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 0, observedAt: 200)])
    }

    @Test func twoAdaptersShareOneLimiterAndLatestScene() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let command = CommandClient(clock: clock, send: { _ in })
        let first = TransmitCommandClient(commands: command, keepaliveOnMedia: { _, _ in true }, diagnostics: diagnostics)
        let second = TransmitCommandClient(commands: command, keepaliveOnMedia: { _, _ in true }, diagnostics: diagnostics)
        let gate = TransmitHeartbeatGate()
        gate.setOpen(true)
        await send(first, gated: true, gate: gate, sequence: 1)
        await clock.advance(by: 200)
        await send(second, gated: true, gate: gate, sequence: 2)
        await clock.advance(by: 200)
        await send(first, gated: true, gate: gate, sequence: 3)
        diagnostics.sceneChanged(to: .background)
        await clock.advance(by: 800)
        await send(second, gated: true, gate: gate, sequence: 4)
        let gaps = recorded.events.filter { if case .keepaliveGap = $0 { return true }; return false }
        #expect(gaps == [
            .keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 0, observedAt: 200),
            .keepaliveGap(milliseconds: 800, scene: .background, suppressed: 1, observedAt: 1200),
        ])
    }

    private func send(_ client: TransmitCommandClient, gated: Bool,
                      gate: TransmitHeartbeatGate, sequence: Int64) async {
        if gated { await client.sendKeepalive(sequence: sequence, epoch: 1, gate: gate, stillAllowed: { true }) }
        else { await client.sendKeepalive(sequence: sequence, epoch: 1) }
    }
}
