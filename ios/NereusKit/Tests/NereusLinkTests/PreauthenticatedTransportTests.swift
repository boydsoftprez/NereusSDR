// NereusSDR for iOS: a raced, already open transport reaches exactly one session
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct PreauthenticatedTransportTests {
    private actor SuspendedAuthenticator: StationAuthenticator {
        private var continuation: CheckedContinuation<Void, Never>?
        private var didEnter = false
        var entered: Bool { didEnter }
        func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
            -> LinkMessage.AuthRequest {
            didEnter = true
            await withCheckedContinuation { continuation = $0 }
            return LinkMessage.AuthRequest(token: "never-send")
        }
        func resume() {
            continuation?.resume()
            continuation = nil
        }
    }

    private final class Immediate: LinkTransport, @unchecked Sendable {
        let digest = Data(repeating: 7, count: 32)
        let hello: String
        let closeBeforeReturn: Bool
        let extraEvents: Int
        private let lock = NSLock()
        private var sent: [String] = []
        private var closed = false
        private var callback: (@Sendable (LinkTransportEvent) async -> Void)?
        private let trafficLifetime = UUID()

        init(closeBeforeReturn: Bool = false, extraEvents: Int = 0) {
            self.closeBeforeReturn = closeBeforeReturn
            self.extraEvents = extraEvents
            hello = LinkCodec.encode(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0,
                                                                peer: "nereusd", majors: [1])))
        }

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            lock.withLock { callback = onEvent }
            await onEvent(.text(hello))
            for _ in 0..<extraEvents { await onEvent(.pong) }
            if closeBeforeReturn { await onEvent(.closed) }
            return digest
        }
        @discardableResult func send(_ text: String) -> Bool {
            lock.withLock {
                guard !closed else { return false }
                sent.append(text)
                return true
            }
        }
        func ping() {}
        func close() { lock.withLock { closed = true } }
        var selectedRouteObservation: SelectedRouteObservation {
            .fromReadyWebSocket(pathEndpoint: .hostPort(host: "192.0.2.9", port: 47910), usesSystemProxy: false)
        }
        var trafficObservation: LinkTrafficObservation? {
            lock.withLock {
                LinkTrafficObservation(lifetime: trafficLifetime, active: !closed,
                    receivedPayloadBytes: UInt64(hello.utf8.count),
                    acceptedPayloadBytes: UInt64(sent.reduce(0) { $0 + $1.utf8.count }))
            }
        }
        var messages: [String] { lock.withLock { sent } }
        var isClosed: Bool { lock.withLock { closed } }
        func emit(_ event: LinkTransportEvent) async {
            let callback = lock.withLock { self.callback }
            await callback?(event)
        }
    }

    private actor CallbackGate {
        private var waiting: CheckedContinuation<Void, Never>?
        private var blocked = false
        private(set) var received: [String] = []
        var entered: Bool { blocked }
        func handle(_ event: LinkTransportEvent) async {
            guard case .text(let text) = event else { return }
            received.append(text)
            if !blocked {
                blocked = true
                await withCheckedContinuation { waiting = $0 }
            }
        }
        func resume() {
            waiting?.resume()
            waiting = nil
        }
    }

    private actor CompletionFlag {
        private(set) var done = false
        func finish() { done = true }
    }

    private final class OwnBoundedOpening: LinkTransport, @unchecked Sendable {
        let boundsItsOwnOpening = true
        let digest = Data(repeating: 9, count: 32)
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Data, Error>?
        private var started = false
        private var closed = false
        var hasStarted: Bool { lock.withLock { started } }
        var isClosed: Bool { lock.withLock { closed } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            let digest = try await withCheckedThrowingContinuation { continuation in
                lock.withLock {
                    started = true
                    waiting = continuation
                }
            }
            await onEvent(.text(LinkCodec.encode(.hello(LinkMessage.Hello(major: 1, minor: 11,
                                                                          settingsSchema: 0, peer: "nereusd",
                                                                          majors: [1])))))
            return digest
        }
        func finishOpening() {
            let waiter = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                let taken = waiting
                waiting = nil
                return taken
            }
            waiter?.resume(returning: digest)
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {
            let waiter = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                closed = true
                let taken = waiting
                waiting = nil
                return taken
            }
            waiter?.resume(throwing: LinkTransportError.failed("closed"))
        }
    }

    private final class HeldHello: LinkTransport, @unchecked Sendable {
        let digest = Data(repeating: 5, count: 32)
        private let lock = NSLock()
        private var callback: (@Sendable (LinkTransportEvent) async -> Void)?
        private var sent: [String] = []
        private var closed = false
        var isOpen: Bool { lock.withLock { callback != nil } }
        var isClosed: Bool { lock.withLock { closed } }
        var acceptedCount: Int { lock.withLock { sent.count } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            lock.withLock { callback = onEvent }
            return digest
        }
        func sendHello() async {
            let callback = lock.withLock { self.callback }
            await callback?(.text(LinkCodec.encode(.hello(LinkMessage.Hello(major: 1, minor: 11,
                                                                          settingsSchema: 0, peer: "nereusd",
                                                                          majors: [1])))))
        }
        @discardableResult func send(_ text: String) -> Bool {
            lock.withLock {
                guard !closed, callback != nil else { return false }
                sent.append(text)
                return true
            }
        }
        func ping() {}
        func close() { lock.withLock { closed = true } }
    }

    @Test func aClosedHelloCannotAuthenticateDuringHandoff() async throws {
        let inner = Immediate(closeBeforeReturn: true)
        let lease = PreauthenticatedTransport(inner)
        do {
            _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
            Issue.record("the closed lease was inspected as live")
        } catch {}
        let session = StationSession(trust: .certificate(pinSHA256: inner.digest),
                                     authenticator: TokenAuthenticator(token: "unused"),
                                     transport: { lease })
        await session.connect()
        #expect(inner.messages.isEmpty)
        #expect(await session.state != .ready)
    }

    @Test func unverifiedEventFloodClosesTheLeaseBeforeAuthentication() async throws {
        let inner = Immediate(extraEvents: 17)
        let lease = PreauthenticatedTransport(inner)
        do {
            _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
            Issue.record("unverified flood was accepted")
        } catch {}
        #expect(inner.isClosed)
        #expect(inner.messages.isEmpty)
    }

    @Test func aLiveLeaseReplaysItsHelloOnceOnTheSameTransport() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        #expect(inner.messages.isEmpty)
        let session = StationSession(trust: .certificate(pinSHA256: inner.digest),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     transport: { lease })
        await session.connect()
        #expect(inner.messages.count == 2)
        #expect((try? LinkCodec.decode(inner.messages[0]))?.kind == .hello)
        #expect((try? LinkCodec.decode(inner.messages[1]))?.kind == .authRequest)
        do {
            _ = try await lease.open(onEvent: { _ in })
            Issue.record("the lease was adopted twice")
        } catch {}
        await session.disconnect()
    }

    @Test func selectedLeaseForwardsOnlyRealLocalTextAdmission() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        #expect(!lease.send("before adoption"))
        _ = try await lease.open(onEvent: { _ in })
        #expect(lease.send("admitted"))
        #expect(inner.messages == ["admitted"])
        inner.close()
        #expect(!lease.send("inner closed"))
        lease.close()
        #expect(!lease.send("lease closed"))
    }

    @Test func closingASelectedLeaseBeforeSessionSetupSendsNothing() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        lease.close()
        let session = StationSession(trust: .certificate(pinSHA256: inner.digest),
                                     authenticator: TokenAuthenticator(token: "unused"),
                                     transport: { lease })
        await session.connect()
        #expect(inner.messages.isEmpty)
    }

    @Test func closingWhileAuthenticationIsSuspendedSendsNothing() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        let signer = SuspendedAuthenticator()
        let session = StationSession(trust: .certificate(pinSHA256: inner.digest),
                                     authenticator: signer, transport: { lease })
        let connecting = Task { await session.connect() }
        let deadline = Date().addingTimeInterval(2)
        while !(await signer.entered) && Date() < deadline { await Task.yield() }
        #expect(await signer.entered)
        lease.close()
        await signer.resume()
        await connecting.value
        #expect(inner.messages.isEmpty)
        await session.disconnect()
    }

    @Test func anIntroducedTransportKeepsItsOwnDialDeadlineThenStartsHelloDeadline() async throws {
        let inner = OwnBoundedOpening()
        let lease = PreauthenticatedTransport(inner)
        let clock = ManualLinkClock()
        let inspecting = Task { try await lease.inspect(clock: clock, deadline: .seconds(30)) }
        let wait = Date().addingTimeInterval(2)
        while !inner.hasStarted && Date() < wait { await Task.yield() }
        #expect(inner.hasStarted)
        #expect(clock.pendingDueTimes.isEmpty)
        await clock.advance(by: 40_000)
        #expect(!inner.isClosed)
        inner.finishOpening()
        let (_, digest) = try await inspecting.value
        #expect(digest == inner.digest)
        #expect(clock.pendingDueTimes == [70_000])
        lease.close()
        #expect(clock.pendingDueTimes.isEmpty)
    }

    @Test func aLateDirectHelloDoesNotRenewTheOriginalThirtySecondSnapshotBudget() async throws {
        let inner = HeldHello()
        let lease = PreauthenticatedTransport(inner)
        let clock = ManualLinkClock()
        let inspecting = Task { try await lease.inspect(clock: clock, deadline: .seconds(30)) }
        let wait = Date().addingTimeInterval(2)
        while !inner.isOpen && Date() < wait { await Task.yield() }
        #expect(inner.isOpen)
        await clock.advance(by: 29_000)
        await inner.sendHello()
        _ = try await inspecting.value
        let session = StationSession(trust: .certificate(pinSHA256: inner.digest),
                                     authenticator: TokenAuthenticator(token: "test"), clock: clock,
                                     transport: { lease })
        await session.connect()
        #expect(inner.acceptedCount == 2)
        #expect(clock.pendingDueTimes.contains(30_000))
        #expect(!clock.pendingDueTimes.contains(59_000))
        await clock.advance(by: 1_000)
        let stop = Date().addingTimeInterval(2)
        while !inner.isClosed && Date() < stop { await Task.yield() }
        #expect(inner.isClosed)
        #expect(await session.state != .ready)
        await session.disconnect()
    }

    @Test func fifthDeviceQuestionPausesTheLeasesRemainingSnapshotBudget() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        let clock = ManualLinkClock()
        _ = try await lease.inspect(clock: clock, deadline: .seconds(30))
        await clock.advance(by: 29_000)
        lease.pauseDeadline()
        #expect(clock.pendingDueTimes.isEmpty)
        await clock.advance(by: 60_000)
        #expect(!inner.isClosed)
        lease.resumeDeadline()
        #expect(clock.pendingDueTimes == [90_000])
        await clock.advance(by: 999)
        #expect(!inner.isClosed)
        await clock.advance(by: 1)
        #expect(inner.isClosed)
    }

    @Test func releasedEventsPreserveCallbackBackpressureAcrossALargeSnapshot() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        let gate = CallbackGate()
        _ = try await lease.open(onEvent: { await gate.handle($0) })
        let releasing = Task { await lease.releaseEvents() }
        let deadline = Date().addingTimeInterval(2)
        while !(await gate.entered) && Date() < deadline { await Task.yield() }
        #expect(await gate.entered)
        let flag = CompletionFlag()
        let producing = Task {
            for index in 0..<1_025 { await inner.emit(.text("snapshot\(index)")) }
            await flag.finish()
        }
        await Task.yield()
        #expect(!(await flag.done))
        #expect(await gate.received == [inner.hello])
        await gate.resume()
        #expect(await releasing.value)
        await producing.value
        #expect(await flag.done)
        let expected = [inner.hello] + (0..<1_025).map { "snapshot\($0)" }
        #expect(await gate.received == expected)
        lease.close()
    }

    @Test func closingWhileReleasedHelloWaitsUnblocksAQueuedProducer() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        let gate = CallbackGate()
        _ = try await lease.open(onEvent: { await gate.handle($0) })
        let releasing = Task { await lease.releaseEvents() }
        let deadline = Date().addingTimeInterval(2)
        while !(await gate.entered) && Date() < deadline { await Task.yield() }
        #expect(await gate.entered)
        let flag = CompletionFlag()
        let producing = Task {
            await inner.emit(.text("snapshot"))
            await flag.finish()
        }
        await Task.yield()
        #expect(!(await flag.done))
        lease.close()
        let unblocking = Date().addingTimeInterval(2)
        while !(await flag.done) && Date() < unblocking { await Task.yield() }
        let producerResumedBeforeSigner = await flag.done
        await gate.resume()
        await producing.value
        _ = await releasing.value
        #expect(producerResumedBeforeSigner)
        #expect(await gate.received == [inner.hello])
    }

    @Test func routeFollowsOnlyAdoptedLiveInnerTransport() async throws {
        let inner = Immediate()
        let lease = PreauthenticatedTransport(inner)
        #expect(lease.selectedRouteObservation == .unavailable(.notReady))
        #expect(lease.trafficObservation == nil)
        _ = try await lease.inspect(clock: ManualLinkClock(), deadline: .seconds(30))
        #expect(lease.selectedRouteObservation == .unavailable(.notReady))
        #expect(lease.trafficObservation == nil)
        _ = try await lease.open { _ in }
        guard case .available(let route) = lease.selectedRouteObservation else {
            Issue.record("adopted route unavailable")
            return
        }
        #expect(route.coreEndpoint?.address == "192.0.2.9")
        #expect(lease.trafficObservation == inner.trafficObservation)
        lease.close()
        #expect(lease.selectedRouteObservation == .unavailable(.retired))
        #expect(lease.trafficObservation == nil)
    }
}
