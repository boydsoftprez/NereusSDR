// NereusSDR for iOS: a sign-in and a pairing each give up 30 s after the dial, however slow the opening
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// The connect deadline (section 12.2) runs on one clock started at the
/// dial, as the desktop's StationClient arms kStationHandshakeDeadlineMs
/// before the WebSocket upgrade: a Core that is slow to open leaves the
/// connect sequence only what is left of the 30 s, so the operator hears
/// it is not answering no later than 30 s after the dial, never after an
/// opening's 30 s plus the sequence's own.
@Suite struct ConnectDeadlineFromDialTests {
    private static let stationHello = LinkMessage.hello(
        LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1], features: [:]))
    private static let accepted = LinkMessage.authResult(
        LinkMessage.AuthResult(accepted: true, reason: "", retryable: false))

    private static func session(clock: ManualLinkClock, trust: StationTrust,
                                factory: @escaping LinkTransportFactory, receipts: TestReceipts) -> StationSession {
        StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: trust,
                       authenticator: TokenAuthenticator(token: "conformance-token"),
                       clock: ReceiptClock(base: clock, receipts: receipts), transportFactory: factory)
    }

    /// A factory that wraps each of a scripted station's connections so its
    /// opening waits for the test.
    private final class SlowStation: @unchecked Sendable {
        let station = ScriptedStation()
        private let lock = NSLock()
        private var made: [SlowOpeningTransport] = []
        private let creation: TestPhase<SlowOpeningTransport>
        private let receipts: TestReceipts

        init(receipts: TestReceipts) {
            self.receipts = receipts
            creation = TestPhase(receipts: receipts, phase: "transport creation")
        }

        var factory: LinkTransportFactory {
            { [self] endpoint, trust in
                receipts.mark("session factory entered")
                let slow = SlowOpeningTransport(station.factory(endpoint, trust), receipts: receipts)
                lock.withLock { made.append(slow) }
                creation.finish(.success(slow))
                receipts.mark("session factory returning")
                return slow
            }
        }

        var latest: SlowOpeningTransport? { lock.withLock { made.last } }

        func opening() async throws -> SlowOpeningTransport {
            let deadline = ContinuousClock.now + .seconds(10)
            receipts.mark("session observer deadline captured: \(receipts.offset(of: deadline))")
            defer { receipts.mark("session opening observer exited") }
            let transport = try await creation.wait(until: deadline)
            try await transport.openingEntry.wait(until: deadline)
            return transport
        }
    }

    // MARK: A sign-in

    @Test func aCoreThatOpensAndNeverSaysHelloEndsThirtySecondsAfterTheDial() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let station = ScriptedStation()
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: station.trust, factory: station.factory, receipts: receipts)
        await session.connect()
        let transport = try #require(station.latest)
        await clock.advance(by: 29_999)
        #expect(await session.state == .connecting)
        await clock.advance(by: 1)
        #expect(transport.isClosedByApp)
        #expect(await session.state == .waitingToRetry(seconds: 1))
    }

    @Test func aCoreSlowToOpenThenSilentEndsAtTheSameBound() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let slow = SlowStation(receipts: receipts)
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory, receipts: receipts)
        receipts.mark("connecting Task submitting")
        let connecting = Task {
            receipts.mark("connecting Task entered")
            defer { receipts.mark("connecting Task settled") }
            await session.connect()
        }
        let transport: SlowOpeningTransport
        do { transport = try await slow.opening() }
        catch {
            connecting.cancel()
            slow.latest?.close()
            await session.disconnect()
            throw error
        }
        await clock.advance(by: 10_000)
        #expect(await session.state == .connecting)
        transport.finishOpening()
        await connecting.value
        // Opened at 10 s: 20 s are left, not another 30.
        await clock.advance(by: 19_999)
        #expect(await session.state == .connecting)
        await clock.advance(by: 1)
        #expect(transport.isClosedByApp)
        #expect(await session.state == .waitingToRetry(seconds: 1))
    }

    @Test func aCoreThatNeverOpensEndsAtTheSameBound() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let slow = SlowStation(receipts: receipts)
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory, receipts: receipts)
        receipts.mark("connecting Task submitting")
        let connecting = Task {
            receipts.mark("connecting Task entered")
            defer { receipts.mark("connecting Task settled") }
            await session.connect()
        }
        let transport: SlowOpeningTransport
        do { transport = try await slow.opening() }
        catch {
            connecting.cancel()
            slow.latest?.close()
            await session.disconnect()
            throw error
        }
        await clock.advance(by: 29_999)
        #expect(await session.state == .connecting)
        await clock.advance(by: 1)
        #expect(transport.isClosedByApp)
        #expect(await session.state == .waitingToRetry(seconds: 1))
        // Without the bound the opening would wait for ever; stopping the
        // session ends it, so a failure fails rather than hangs.
        await session.disconnect()
        await connecting.value
    }

    /// Over a real socket: a Core that accepts TCP and TLS and never answers
    /// the opening. The transport's own bound is set far beyond the test, so
    /// only the session's clock, moved by hand, can end it.
    @Test(arguments: ["::1", "127.0.0.1"])
    func aSilentCoreOverTLSEndsAtTheSessionsBound(address: String) async throws {
        let receipts = TestReceipts(caseID: "\(#function) \(address)")
        defer { receipts.flush() }
        let listener = try SilentTLSListener(address: address, observe: receipts.mark)
        let port = try await listener.start()
        defer { listener.stop() }
        let clock = ManualLinkClock()
        let session = StationSession(
            endpoint: StationEndpoint(host: address, port: port),
            trust: .identity(publicKey: Data(repeating: 4, count: 65)),
            authenticator: TokenAuthenticator(token: "conformance-token"), clock: ReceiptClock(base: clock, receipts: receipts),
            transportFactory: { endpoint, trust in
                receipts.mark("session TLS factory entered")
                let transport = ReceiptTransport(WebSocketLinkTransport(endpoint: endpoint, trust: trust,
                                                                         openDeadline: .seconds(600)), receipts: receipts)
                receipts.mark("session TLS factory returning")
                return transport
            })
        let recorder = EventRecorder(session)
        receipts.mark("connecting Task submitting")
        let connecting = Task {
            receipts.mark("connecting Task entered")
            defer { receipts.mark("connecting Task settled") }
            await session.connect()
        }
        let started = ContinuousClock.now
        receipts.mark("TLS request observer window began; deadline=\(receipts.offset(of: started + .seconds(10)))", at: started)
        while listener.receivedRequests.isEmpty && ContinuousClock.now - started < .seconds(10) {
            try await Task.sleep(for: .milliseconds(20))
        }
        receipts.mark("TLS request observer exited; request published=\(!listener.receivedRequests.isEmpty)")
        try #require(!listener.receivedRequests.isEmpty, "the listener saw no opening, so nothing was waited on")
        #expect(await session.state == .connecting)
        let advanced = ContinuousClock.now
        await clock.advance(by: 30_000)
        let ended = await recorder.wait(timeout: .seconds(10)) { events in
            events.contains(.stateChanged(.waitingToRetry(seconds: 1)))
        }
        #expect(ended)
        await session.disconnect()
        await connecting.value
        #expect(ContinuousClock.now - advanced < .seconds(5))
    }

    @Test func aNormalConnectAfterASlowOpeningIsUnaffected() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let slow = SlowStation(receipts: receipts)
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory, receipts: receipts)
        receipts.mark("connecting Task submitting")
        let connecting = Task {
            receipts.mark("connecting Task entered")
            defer { receipts.mark("connecting Task settled") }
            await session.connect()
        }
        let transport: SlowOpeningTransport
        do { transport = try await slow.opening() }
        catch {
            connecting.cancel()
            slow.latest?.close()
            await session.disconnect()
            throw error
        }
        await clock.advance(by: 10_000)
        transport.finishOpening()
        await connecting.value
        let scripted = try #require(slow.station.latest)
        await clock.advance(by: 19_000) { await scripted.answerPings() }
        await scripted.deliver(Self.stationHello)
        await scripted.deliver(Self.accepted)
        await scripted.deliver(.capabilities(LinkMessage.Capabilities(properties: [])))
        await scripted.deliver(.snapshotComplete)
        #expect(await session.state == .ready)
        await clock.advance(by: 120_000) { await scripted.answerPings() }
        #expect(await session.state == .ready)
        #expect(!transport.isClosedByApp)
    }

    // MARK: A pairing

    private static func device() throws -> DeviceIdentity {
        try DeviceIdentity.load(store: InMemoryKeyStore())
    }

    private static func client(clock: ManualLinkClock, transport: any LinkTransport,
                               receipts: TestReceipts) throws -> PairingClient {
        try PairingClient(identity: try device(), name: "Shack iPhone", kind: .phone, clock: ReceiptClock(base: clock, receipts: receipts),
                          transportFactory: { _, _ in
                              receipts.mark("pairing factory entered")
                              receipts.mark("pairing factory returning")
                              return transport
                          })
    }

    /// The error `task` ended with, or nil when it succeeded. Without the
    /// bound a pairing would wait for ever: after 10 s of real time this
    /// cancels it and closes `transport`, so a failure fails rather than
    /// hangs, and the result is then a cancellation, never a pass.
    private static func failure(_ task: Task<PairedStation, Error>,
                                closing transport: any LinkTransport) async -> PairingError? {
        let backstop = Task {
            try await Task.sleep(for: .seconds(10))
            task.cancel()
            transport.close()
        }
        defer { backstop.cancel() }
        do {
            _ = try await task.value
            return nil
        } catch {
            return error as? PairingError
        }
    }

    /// Each test its own Core, as pairing runs one at a time per Core.
    private static func uniqueEndpoint() -> StationEndpoint {
        StationEndpoint(host: "core-\(UUID().uuidString.lowercased()).example.net")
    }

    @Test func aPairingSlowToOpenThenSilentEndsThirtySecondsAfterTheDial() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let core = PairingTestTransport()
        let identity = TestStationIdentity()
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(core, receipts: receipts)
        let client = try Self.client(clock: clock, transport: slow, receipts: receipts)
        let endpoint = Self.uniqueEndpoint()
        receipts.mark("pairing Task submitting")
        let pairing = Task {
            receipts.mark("pairing Task entered")
            defer { receipts.mark("pairing Task settled") }
            return try await client.pairOnThisNetwork(endpoint: endpoint)
        }
        // An entry assertion can throw before the normal deadline/cancel path.
        // Scope cleanup ends that task without supplying any assertion result.
        defer { pairing.cancel(); slow.close() }
        _ = try #require(await SlowOpeningTransport.opening(in: { slow }))
        #expect(clock.pendingDueTimes == [30_000])
        await clock.advance(by: 10_000)
        slow.finishOpening()
        core.deliver(.hello(LinkMessage.Hello(
            major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
            features: ["deviceAuth": 1, "pairing": 1],
            identity: try identity.claim(certificateSHA256: core.certificateSHA256),
            challenge: TestStationIdentity.newChallenge())))
        #expect(await core.nextSent()?.kind == .hello)
        #expect(await core.nextSent()?.kind == .pairStart)
        // Opened at 10 s: the deadline is still the one armed at the dial.
        #expect(clock.pendingDueTimes == [30_000])
        await clock.advance(by: 19_999)
        #expect(!core.isClosedByApp)
        await clock.advance(by: 1)
        #expect(await Self.failure(pairing, closing: slow) == .timedOut)
        #expect(core.isClosedByApp)
    }

    @Test func aPairingNobodyOpensEndsAtTheSameBoundAndFreesTheCore() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(PairingTestTransport(), receipts: receipts)
        let endpoint = Self.uniqueEndpoint()
        let first = try Self.client(clock: clock, transport: slow, receipts: receipts)
        receipts.mark("pairing Task submitting")
        let pairing = Task {
            receipts.mark("pairing Task entered")
            defer { receipts.mark("pairing Task settled") }
            return try await first.pairOnThisNetwork(endpoint: endpoint)
        }
        // Close even when the fixture entry assertion throws.
        defer { pairing.cancel(); slow.close() }
        _ = try #require(await SlowOpeningTransport.opening(in: { slow }))
        await clock.advance(by: 29_999)
        #expect(!slow.isClosedByApp)
        await clock.advance(by: 1)
        // Closed by the deadline itself, before the test's backstop could.
        #expect(slow.isClosedByApp)
        #expect(await Self.failure(pairing, closing: slow) == .didNotOpen(localNetworkDenied: false))
        #expect(clock.pendingDueTimes.isEmpty)
        // The Core's gate is free: a new pairing dials rather than being
        // refused as one still running, and has its own 30 s.
        let next = SlowOpeningTransport(PairingTestTransport(), receipts: receipts, phase: "retry slow opening")
        let second = try Self.client(clock: clock, transport: next, receipts: receipts)
        receipts.mark("retry Task submitting")
        let retry = Task {
            receipts.mark("retry Task entered")
            defer { receipts.mark("retry Task settled") }
            return try await second.pairOnThisNetwork(endpoint: endpoint)
        }
        defer { retry.cancel(); next.close() }
        _ = try #require(await SlowOpeningTransport.opening(in: { next }))
        #expect(clock.pendingDueTimes == [60_000])
        await clock.advance(by: 30_000)
        #expect(await Self.failure(retry, closing: next) == .didNotOpen(localNetworkDenied: false))
    }

    @Test func aPairingCancelledWhileDiallingThrowsCancellationAndClosesAtOnce() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(PairingTestTransport(), receipts: receipts)
        let endpoint = Self.uniqueEndpoint()
        let client = try Self.client(clock: clock, transport: slow, receipts: receipts)
        receipts.mark("pairing Task submitting")
        let pairing = Task {
            receipts.mark("pairing Task entered")
            defer { receipts.mark("pairing Task settled") }
            return try await client.pairOnThisNetwork(endpoint: endpoint)
        }
        // An entry assertion can throw before the normal deadline/cancel path.
        // Scope cleanup ends that task without supplying any assertion result.
        defer { pairing.cancel(); slow.close() }
        _ = try #require(await SlowOpeningTransport.opening(in: { slow }))
        pairing.cancel()
        // Without the cancel closing it, only this would end the opening,
        // and the pairing would then fail as one that never opened.
        let backstop = Task {
            try await Task.sleep(for: .seconds(10))
            slow.close()
        }
        defer { backstop.cancel() }
        // A cancel, not a connection that never opened.
        await #expect(throws: CancellationError.self) { try await pairing.value }
        #expect(slow.isClosedByApp)
        #expect(clock.pendingDueTimes.isEmpty)
    }

    @Test func aPairingIOSKeepsOffTheNetworkSaysSo() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let clock = ManualLinkClock()
        let client = try Self.client(clock: clock, transport: FailingOpenTransport(.localNetworkDenied), receipts: receipts)
        receipts.mark("pairing Task submitting")
        let pairing = Task {
            receipts.mark("pairing Task entered")
            defer { receipts.mark("pairing Task settled") }
            return try await client.pairOnThisNetwork(endpoint: Self.uniqueEndpoint())
        }
        #expect(await Self.failure(pairing, closing: FailingOpenTransport(.localNetworkDenied))
            == .didNotOpen(localNetworkDenied: true))
        let refused = try Self.client(clock: clock, transport: FailingOpenTransport(.failed("refused")), receipts: receipts)
        receipts.mark("again Task submitting")
        let again = Task {
            receipts.mark("again Task entered")
            defer { receipts.mark("again Task settled") }
            return try await refused.pairOnThisNetwork(endpoint: Self.uniqueEndpoint())
        }
        #expect(await Self.failure(again, closing: FailingOpenTransport(.failed("refused")))
            == .didNotOpen(localNetworkDenied: false))
        #expect(clock.pendingDueTimes.isEmpty)
    }

    @Test(arguments: ["::1", "127.0.0.1"])
    func aPairingWithASilentCoreOverTLSEndsAtThePairingsBound(address: String) async throws {
        let receipts = TestReceipts(caseID: "\(#function) \(address)")
        defer { receipts.flush() }
        let listener = try SilentTLSListener(address: address, observe: receipts.mark)
        let port = try await listener.start()
        defer { listener.stop() }
        let clock = ReceiptClock(receipts: receipts)
        let made = Made()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone, clock: clock,
                                       transportFactory: { endpoint, trust in
                                           receipts.mark("pairing TLS factory entered")
                                           let transport = made.keep(ReceiptTransport(WebSocketLinkTransport(endpoint: endpoint, trust: trust,
                                                                            openDeadline: .seconds(600)), receipts: receipts))
                                           receipts.mark("pairing TLS factory returning")
                                           return transport
                                       })
        receipts.mark("pairing Task submitting")
        let pairing = Task {
            receipts.mark("pairing Task entered")
            do {
                let result = try await client.pairOnThisNetwork(endpoint: StationEndpoint(host: address, port: port))
                receipts.mark("pairing task settled successfully")
                return result
            } catch {
                receipts.mark("pairing task settled: \(error)")
                throw error
            }
        }
        let started = ContinuousClock.now
        receipts.mark("TLS request observer window began; deadline=\(receipts.offset(of: started + .seconds(10)))", at: started)
        while listener.receivedRequests.isEmpty && ContinuousClock.now - started < .seconds(10) {
            try await Task.sleep(for: .milliseconds(20))
        }
        receipts.mark("TLS request observer exited; request published=\(!listener.receivedRequests.isEmpty)")
        try #require(!listener.receivedRequests.isEmpty, "the listener saw no opening, so nothing was waited on")
        let advanced = ContinuousClock.now
        receipts.mark("observer advancing pairing clock")
        await clock.base.advance(by: 30_000)
        receipts.mark("observer clock advance returned")
        let transport = try #require(made.latest)
        #expect(await Self.failure(pairing, closing: transport) == .didNotOpen(localNetworkDenied: false))
        receipts.mark("pairing observer resumed")
        // Ended by the pairing's bound, not by the test's backstop.
        #expect(ContinuousClock.now - advanced < .seconds(5))
    }

    @Test func fixturePhaseAlreadyExpiredRegistrationStaysFailedAfterLateEntry() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let expired = TestPhase<Void>(receipts: receipts, phase: "expired control")
        await #expect(throws: TestPhase<Void>.Failure.noEntry) {
            try await expired.wait(until: ContinuousClock.now - .seconds(1))
        }
        expired.finish(.success(()))
        await #expect(throws: TestPhase<Void>.Failure.noEntry) {
            try await expired.wait(until: ContinuousClock.now + .seconds(10))
        }
    }

    @Test func fixturePhaseExpiryAlsoSettlesAnAlreadyRegisteredObserver() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let phase = TestPhase<Void>(receipts: receipts, phase: "phase control")
        let earlier = Task { try await phase.wait(until: ContinuousClock.now + .seconds(10)) }
        defer { earlier.cancel() }
        let entryDeadline = ContinuousClock.now + .seconds(2)
        while phase.pendingWaiterCount == 0 && ContinuousClock.now < entryDeadline { await Task.yield() }
        try #require(phase.pendingWaiterCount == 1)
        await #expect(throws: TestPhase<Void>.Failure.noEntry) {
            try await phase.wait(until: ContinuousClock.now - .seconds(1))
        }
        phase.finish(.success(()))
        let settled = TestPhase<Bool>(receipts: receipts, phase: "settled control")
        let observer = Task {
            do { try await earlier.value; settled.finish(.success(false)) }
            catch { settled.finish(.success(error as? TestPhase<Void>.Failure == .noEntry)) }
        }
        defer { observer.cancel() }
        #expect(try await settled.wait(until: ContinuousClock.now + .seconds(2)))
    }

    @Test func fixturePhaseMissingEntryAndCancellationFailRatherThanHang() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let missing = TestPhase<Void>(receipts: receipts, phase: "missing control")
        await #expect(throws: TestPhase<Void>.Failure.noEntry) {
            try await missing.wait(until: ContinuousClock.now + .milliseconds(20))
        }
        // Late acknowledgement does not turn the failed phase into success.
        missing.finish(.success(()))
        await #expect(throws: TestPhase<Void>.Failure.noEntry) {
            try await missing.wait(until: ContinuousClock.now + .seconds(10))
        }
        let cancelled = TestPhase<Void>(receipts: receipts, phase: "cancelled control")
        let waiting = Task { try await cancelled.wait(until: ContinuousClock.now + .seconds(10)) }
        waiting.cancel()
        await #expect(throws: TestPhase<Void>.Failure.cancelled) { try await waiting.value }
    }

    @Test func aClosedSlowOpeningCannotSatisfyTheFixtureEntryBarrier() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let slow = SlowOpeningTransport(PairingTestTransport(), receipts: receipts)
        slow.close()
        await #expect(throws: TestPhase<Void>.Failure.closed) {
            try await slow.openingEntry.wait(until: ContinuousClock.now + .seconds(10))
        }
    }

}

/// The connections a factory made, for a test to close.
private final class Made: @unchecked Sendable {
    private let lock = NSLock()
    private var transports: [any LinkTransport] = []

    func keep(_ transport: any LinkTransport) -> any LinkTransport {
        lock.withLock { transports.append(transport) }
        return transport
    }

    var latest: (any LinkTransport)? { lock.withLock { transports.last } }
}

/// Wraps a connection so its opening waits until the test finishes it, or
/// fails when the app closes it first, as a real opening does.
final class SlowOpeningTransport: LinkTransport, @unchecked Sendable {
    private let inner: any LinkTransport
    private let lock = NSLock()
    private var waiter: CheckedContinuation<Void, Error>?
    let openingEntry: TestPhase<Void>
    let receipts: TestReceipts
    private let phase: String
    private var released = false
    private var closed = false

    init(_ inner: any LinkTransport, receipts: TestReceipts, phase: String = "slow opening") {
        self.inner = inner
        self.receipts = receipts
        self.phase = phase
        openingEntry = TestPhase(receipts: receipts, phase: phase)
    }

    /// Waits, up to 10 s of real time, for the transport `latest` names to
    /// be inside its opening.
    static func opening(in latest: @escaping @Sendable () -> SlowOpeningTransport?) async -> SlowOpeningTransport? {
        let giveUp = ContinuousClock.now + .seconds(10)
        let receipts = latest()?.receipts
        if let receipts {
            receipts.mark("pairing observer deadline captured: \(receipts.offset(of: giveUp))")
        }
        defer { receipts?.mark("pairing opening observer exited") }
        while ContinuousClock.now < giveUp {
            if let transport = latest(), transport.isOpening {
                transport.receipts.mark("pairing observer saw opening")
                return transport
            }
            try? await Task.sleep(for: .milliseconds(2))
        }
        receipts?.mark("pairing observer reached bound; returning nil")
        return nil
    }

    var isOpening: Bool { lock.withLock { waiter != nil } }
    var isClosedByApp: Bool { lock.withLock { closed } }

    /// The Core answers the opening.
    func finishOpening() {
        receipts.mark("\(phase) release entered")
        let resume = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            released = true
            defer { waiter = nil }
            return waiter
        }
        resume?.resume()
    }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        receipts.mark("\(phase) open entered")
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            var installedAt: ContinuousClock.Instant?
            let outcome = lock.withLock { () -> Bool? in
                if closed {
                    return false
                }
                if released {
                    return true
                }
                waiter = continuation
                installedAt = ContinuousClock.now
                // Acknowledge only after the opening hold exists.
                openingEntry.finish(.success(()))
                return nil
            }
            if let installedAt { receipts.mark("\(phase) waiter installed", at: installedAt) }
            switch outcome {
            case true?:
                continuation.resume()
            case false?:
                continuation.resume(throwing: LinkTransportError.failed("closed"))
            case nil:
                break
            }
        }
        return try await inner.open(onEvent: onEvent)
    }

    @discardableResult func send(_ text: String) -> Bool {
        inner.send(text)
    }

    func ping() {
        inner.ping()
    }

    func close() {
        receipts.mark("\(phase) close entered")
        let resume = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            closed = true
            defer { waiter = nil }
            return waiter
        }
        openingEntry.finish(.failure(.closed))
        resume?.resume(throwing: LinkTransportError.failed("closed"))
        inner.close()
    }
}

/// A connection whose opening fails at once with `error`, as a refused
/// dial or iOS's Local Network refusal does.
final class FailingOpenTransport: LinkTransport, @unchecked Sendable {
    private let error: LinkTransportError

    init(_ error: LinkTransportError) {
        self.error = error
    }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        throw error
    }

    @discardableResult func send(_ text: String) -> Bool { false }

    func ping() {}

    func close() {}
}


/// Observe the existing manual timer callback without changing its clock.
private final class ReceiptClock: LinkClock, @unchecked Sendable {
    let base: ManualLinkClock
    let receipts: TestReceipts
    init(base: ManualLinkClock = ManualLinkClock(), receipts: TestReceipts) {
        self.base = base
        self.receipts = receipts
    }
    var nowMilliseconds: Int64 { base.nowMilliseconds }
    func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
        let receipts = receipts
        receipts.mark("timer schedule entered (delay \(delay)); manual now=\(base.nowMilliseconds)")
        let timer = base.schedule(after: delay) {
            receipts.mark("timer callback entered (delay \(delay))")
            await action()
            receipts.mark("timer callback returned")
        }
        receipts.mark("timer registered; manual due times=\(base.pendingDueTimes)")
        return timer
    }
}

/// Pairing uses only opening, text and close here; delegate all transport
/// capabilities as well so this observation cannot change the route policy.
private final class ReceiptTransport: LinkTransport, @unchecked Sendable {
    let inner: any LinkTransport
    let receipts: TestReceipts
    init(_ inner: any LinkTransport, receipts: TestReceipts) { self.inner = inner; self.receipts = receipts }
    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        receipts.mark("transport opening entered")
        do {
            let result = try await inner.open(onEvent: onEvent)
            receipts.mark("transport opening settled successfully")
            return result
        } catch {
            receipts.mark("transport opening settled: \(error)")
            throw error
        }
    }
    @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
    func ping() { inner.ping() }
    func close() {
        receipts.mark("transport close entered")
        inner.close()
        receipts.mark("transport close returned")
    }
    var boundsItsOwnOpening: Bool { inner.boundsItsOwnOpening }
    var selectedRouteObservation: SelectedRouteObservation { inner.selectedRouteObservation }
    var diagnosticServiceRank: Int? { inner.diagnosticServiceRank }
    var trafficObservation: LinkTrafficObservation? { inner.trafficObservation }
    func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { inner.setBinaryReceiver(receiver) }
    @discardableResult func sendBinary(_ frame: Data) -> Bool { inner.sendBinary(frame) }
    @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        inner.sendBinary(frame, ownership: ownership)
    }
    func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }
}
