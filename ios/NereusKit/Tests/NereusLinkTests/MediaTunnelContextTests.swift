// NereusSDR for iOS: tunnel route ownership and UUID isolation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct MediaTunnelContextTests {
    private actor Gate {
        private var count = 0
        private var open = false
        private var waiting: [CheckedContinuation<Void, Never>] = []
        private var countWaiters: [(Int, CheckedContinuation<Void, Never>)] = []

        func hold() async {
            count += 1
            for (target, waiter) in countWaiters where count >= target { waiter.resume() }
            countWaiters.removeAll { count >= $0.0 }
            if !open { await withCheckedContinuation { waiting.append($0) } }
        }
        func waitFor(_ target: Int) async {
            if count >= target { return }
            await withCheckedContinuation { countWaiters.append((target, $0)) }
        }
        func release() {
            open = true
            for waiter in waiting { waiter.resume() }
            waiting.removeAll()
        }
    }

    private func session() -> StationSession {
        let identity = TestStationIdentity()
        return StationSession(trust: identity.trust,
                              authenticator: TokenAuthenticator(token: "test-token"),
                              transport: { ScriptedTransport(presentedSHA256: Data(repeating: 1, count: 32),
                                                             openFailure: nil) })
    }

    @Test func pendingClaimsReserveAllThreeSlotsAndTheUUID() async throws {
        let gate = Gate()
        let context = MediaTunnelContext(session: session(), beforeRegistration: { await gate.hold() })
        let ids = (0..<4).map { _ in UUID() }
        let tasks = ids.prefix(3).map { id in Task { try await context.claimMedia(connectionId: id) } }
        await gate.waitFor(3)
        await #expect(throws: MediaTunnelError.self) { _ = try await context.claimMedia(connectionId: ids[3]) }
        await #expect(throws: MediaTunnelError.self) { _ = try await context.claimMedia(connectionId: ids[0]) }
        await gate.release()
        let claims = try await tasks.asyncMap { try await $0.value }
        #expect(Set(claims.map(\.connectionId)).count == 3)
        for claim in claims { await context.releaseMedia(claim) }
    }

    @Test func lateClaimAfterCloseCannotOverwriteNewReceiver() async throws {
        let session = session()
        let gate = Gate()
        let context = MediaTunnelContext(session: session, beforeRegistration: { await gate.hold() })
        let claiming = Task { try await context.claimMedia(connectionId: UUID()) }
        await gate.waitFor(1)
        await context.close()
        let newerOwner = UUID()
        await session.setMediaTunnelReceiver(owner: newerOwner) { _ in }
        await gate.release()
        await #expect(throws: MediaTunnelError.self) { _ = try await claiming.value }
        #expect(await session.mediaTunnelReceiverOwner == newerOwner)
    }

    @Test func queuedOldUUIDFramesCannotBecomeNewClaimFrames() async {
        let ingress = MediaTunnelInbound()
        let id = UUID()
        let old = BinaryMediaOwnership()
        let newer = BinaryMediaOwnership()
        let gate = Gate()
        let received = LockedOwners()
        ingress.setHandler { _, owner in
            received.append(owner)
            if owner === old { await gate.hold() }
        }
        ingress.register(id, ownership: old)
        let frame = Self.frame(id)
        ingress.offer(frame)
        await gate.waitFor(1)
        ingress.offer(frame)
        #expect(ingress.queuedCount == 1)
        old.retire()
        ingress.unregister(id, ownership: old)
        ingress.register(id, ownership: newer)
        #expect(ingress.queuedCount == 0)
        ingress.offer(frame)
        await gate.release()
        for _ in 0..<100 where received.count < 2 { await Task.yield() }
        #expect(received.values.count == 2)
        #expect(received.values[0] === old)
        #expect(received.values[1] === newer)
    }

    private static func frame(_ id: UUID) -> Data {
        var raw = id.uuid
        var result = Data([2])
        result.append(withUnsafeBytes(of: &raw) { Data($0) })
        result.append(0x99)
        return result
    }

    private final class LockedOwners: @unchecked Sendable {
        private let lock = NSLock()
        private var held: [BinaryMediaOwnership] = []
        func append(_ owner: BinaryMediaOwnership) { lock.withLock { held.append(owner) } }
        var values: [BinaryMediaOwnership] { lock.withLock { held } }
        var count: Int { lock.withLock { held.count } }
    }
}

private extension Sequence {
    func asyncMap<T>(_ transform: (Element) async throws -> T) async rethrows -> [T] {
        var result: [T] = []
        for item in self { result.append(try await transform(item)) }
        return result
    }
}
