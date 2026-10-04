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
                                factory: @escaping LinkTransportFactory) -> StationSession {
        StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: trust,
                       authenticator: TokenAuthenticator(token: "conformance-token"),
                       clock: clock, transportFactory: factory)
    }

    /// A factory that wraps each of a scripted station's connections so its
    /// opening waits for the test.
    private final class SlowStation: @unchecked Sendable {
        let station = ScriptedStation()
        private let lock = NSLock()
        private var made: [SlowOpeningTransport] = []

        var factory: LinkTransportFactory {
            { [self] endpoint, trust in
                let slow = SlowOpeningTransport(station.factory(endpoint, trust))
                lock.withLock { made.append(slow) }
                return slow
            }
        }

        var latest: SlowOpeningTransport? { lock.withLock { made.last } }
    }

    // MARK: A sign-in

    @Test func aCoreThatOpensAndNeverSaysHelloEndsThirtySecondsAfterTheDial() async throws {
        let station = ScriptedStation()
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: station.trust, factory: station.factory)
        await session.connect()
        let transport = try #require(station.latest)
        await clock.advance(by: 29_999)
        #expect(await session.state == .connecting)
        await clock.advance(by: 1)
        #expect(transport.isClosedByApp)
        #expect(await session.state == .waitingToRetry(seconds: 1))
    }

    @Test func aCoreSlowToOpenThenSilentEndsAtTheSameBound() async throws {
        let slow = SlowStation()
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory)
        let connecting = Task { await session.connect() }
        let transport = try #require(await SlowOpeningTransport.opening(in: { slow.latest }))
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
        let slow = SlowStation()
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory)
        let connecting = Task { await session.connect() }
        let transport = try #require(await SlowOpeningTransport.opening(in: { slow.latest }))
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
        let listener = try SilentTLSListener(address: address)
        let port = try await listener.start()
        defer { listener.stop() }
        let clock = ManualLinkClock()
        let session = StationSession(
            endpoint: StationEndpoint(host: address, port: port),
            trust: .identity(publicKey: Data(repeating: 4, count: 65)),
            authenticator: TokenAuthenticator(token: "conformance-token"), clock: clock,
            transportFactory: { endpoint, trust in
                WebSocketLinkTransport(endpoint: endpoint, trust: trust, openDeadline: .seconds(600))
            })
        let recorder = EventRecorder(session)
        let connecting = Task { await session.connect() }
        let started = ContinuousClock.now
        while listener.receivedRequests.isEmpty && ContinuousClock.now - started < .seconds(10) {
            try await Task.sleep(for: .milliseconds(20))
        }
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
        let slow = SlowStation()
        let clock = ManualLinkClock()
        let session = Self.session(clock: clock, trust: slow.station.trust, factory: slow.factory)
        let connecting = Task { await session.connect() }
        let transport = try #require(await SlowOpeningTransport.opening(in: { slow.latest }))
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

    private static func client(clock: ManualLinkClock, transport: any LinkTransport) throws -> PairingClient {
        try PairingClient(identity: try device(), name: "Shack iPhone", kind: .phone, clock: clock,
                          transportFactory: { _, _ in transport })
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
        let core = PairingTestTransport()
        let identity = TestStationIdentity()
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(core)
        let client = try Self.client(clock: clock, transport: slow)
        let endpoint = Self.uniqueEndpoint()
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: endpoint) }
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
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(PairingTestTransport())
        let endpoint = Self.uniqueEndpoint()
        let first = try Self.client(clock: clock, transport: slow)
        let pairing = Task { try await first.pairOnThisNetwork(endpoint: endpoint) }
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
        let next = SlowOpeningTransport(PairingTestTransport())
        let second = try Self.client(clock: clock, transport: next)
        let retry = Task { try await second.pairOnThisNetwork(endpoint: endpoint) }
        _ = try #require(await SlowOpeningTransport.opening(in: { next }))
        #expect(clock.pendingDueTimes == [60_000])
        await clock.advance(by: 30_000)
        #expect(await Self.failure(retry, closing: next) == .didNotOpen(localNetworkDenied: false))
    }

    @Test func aPairingCancelledWhileDiallingThrowsCancellationAndClosesAtOnce() async throws {
        let clock = ManualLinkClock()
        let slow = SlowOpeningTransport(PairingTestTransport())
        let endpoint = Self.uniqueEndpoint()
        let client = try Self.client(clock: clock, transport: slow)
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: endpoint) }
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
        let clock = ManualLinkClock()
        let client = try Self.client(clock: clock, transport: FailingOpenTransport(.localNetworkDenied))
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: Self.uniqueEndpoint()) }
        #expect(await Self.failure(pairing, closing: FailingOpenTransport(.localNetworkDenied))
            == .didNotOpen(localNetworkDenied: true))
        let refused = try Self.client(clock: clock, transport: FailingOpenTransport(.failed("refused")))
        let again = Task { try await refused.pairOnThisNetwork(endpoint: Self.uniqueEndpoint()) }
        #expect(await Self.failure(again, closing: FailingOpenTransport(.failed("refused")))
            == .didNotOpen(localNetworkDenied: false))
        #expect(clock.pendingDueTimes.isEmpty)
    }

    @Test(arguments: ["::1", "127.0.0.1"])
    func aPairingWithASilentCoreOverTLSEndsAtThePairingsBound(address: String) async throws {
        let listener = try SilentTLSListener(address: address)
        let port = try await listener.start()
        defer { listener.stop() }
        let clock = ManualLinkClock()
        let made = Made()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone, clock: clock,
                                       transportFactory: { endpoint, trust in
                                           made.keep(WebSocketLinkTransport(endpoint: endpoint, trust: trust,
                                                                            openDeadline: .seconds(600)))
                                       })
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: StationEndpoint(host: address, port: port)) }
        let started = ContinuousClock.now
        while listener.receivedRequests.isEmpty && ContinuousClock.now - started < .seconds(10) {
            try await Task.sleep(for: .milliseconds(20))
        }
        try #require(!listener.receivedRequests.isEmpty, "the listener saw no opening, so nothing was waited on")
        let advanced = ContinuousClock.now
        await clock.advance(by: 30_000)
        let transport = try #require(made.latest)
        #expect(await Self.failure(pairing, closing: transport) == .didNotOpen(localNetworkDenied: false))
        // Ended by the pairing's bound, not by the test's backstop.
        #expect(ContinuousClock.now - advanced < .seconds(5))
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
    private var released = false
    private var closed = false

    init(_ inner: any LinkTransport) {
        self.inner = inner
    }

    /// Waits, up to 10 s of real time, for the transport `latest` names to
    /// be inside its opening.
    static func opening(in latest: @escaping @Sendable () -> SlowOpeningTransport?) async -> SlowOpeningTransport? {
        let giveUp = ContinuousClock.now + .seconds(10)
        while ContinuousClock.now < giveUp {
            if let transport = latest(), transport.isOpening {
                return transport
            }
            try? await Task.sleep(for: .milliseconds(2))
        }
        return nil
    }

    var isOpening: Bool { lock.withLock { waiter != nil } }
    var isClosedByApp: Bool { lock.withLock { closed } }

    /// The Core answers the opening.
    func finishOpening() {
        let resume = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            released = true
            defer { waiter = nil }
            return waiter
        }
        resume?.resume()
    }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            let outcome = lock.withLock { () -> Bool? in
                if closed {
                    return false
                }
                if released {
                    return true
                }
                waiter = continuation
                return nil
            }
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
        let resume = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            closed = true
            defer { waiter = nil }
            return waiter
        }
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
