// NereusSDR for iOS: an in-process connection the tests play the Core's end of
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One connection with its far end in the test. Every call that delivers
/// something to the session returns once the session has handled it.
public final class ScriptedTransport: LinkTransport, @unchecked Sendable {
    private let lock = NSLock()
    private let presentedSHA256: Data
    private let openFailure: LinkTransportError?
    private var handler: (@Sendable (LinkTransportEvent) async -> Void)?
    private var binaryReceiver: (@Sendable (Data) -> Void)?
    private var sentBinary: [Data] = []
    private var sent: [String] = []
    private var owedPongs = 0
    private var pings = 0
    private var closedByApp = false
    private var dropped = false
    private let waiters = ConditionWaiters()

    public init(presentedSHA256: Data, openFailure: LinkTransportError?) {
        self.presentedSHA256 = presentedSHA256
        self.openFailure = openFailure
    }

    // MARK: The app's end

    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        if let openFailure {
            throw openFailure
        }
        lock.withLock { handler = onEvent }
        return presentedSHA256
    }

    @discardableResult public func send(_ text: String) -> Bool {
        let admitted = lock.withLock { () -> Bool in
            guard !closedByApp, !dropped else { return false }
            sent.append(text)
            return true
        }
        if admitted { waiters.release() }
        return admitted
    }

    public func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {
        lock.withLock { binaryReceiver = receiver }
    }

    @discardableResult public func sendBinary(_ frame: Data) -> Bool {
        lock.withLock {
            guard !closedByApp else { return false }
            sentBinary.append(frame)
            return true
        }
    }

    public var binaryFrames: [Data] { lock.withLock { sentBinary } }
    public func deliverBinary(_ frame: Data) { lock.withLock { binaryReceiver }?(frame) }

    public func ping() {
        lock.withLock {
            pings += 1
            owedPongs += 1
        }
    }

    public func close() {
        lock.withLock { closedByApp = true; binaryReceiver = nil }
    }

    // MARK: The Core's end

    /// Frames the app sent and the test has not taken yet.
    public var pending: [String] { lock.withLock { sent } }
    /// Waits until the app has sent a frame the test has not taken yet,
    /// without polling; returns false only if none came by `timeout`.
    @discardableResult
    public func waitForSent(within timeout: Duration = .seconds(30)) async -> Bool {
        await waiters.wait(within: timeout) { [self] in !pending.isEmpty }
    }

    public var pingCount: Int { lock.withLock { pings } }
    public var isClosedByApp: Bool { lock.withLock { closedByApp } }
    /// True while neither end has closed the connection.
    public var isOpen: Bool { lock.withLock { !closedByApp && !dropped } }

    /// Takes the oldest frame the app sent, if any.
    public func takeSent() -> String? {
        lock.withLock { sent.isEmpty ? nil : sent.removeFirst() }
    }

    public func deliver(_ text: String) async {
        await lock.withLock { handler }?(.text(text))
    }

    public func deliver(_ message: LinkMessage) async {
        await deliver(LinkCodec.encode(message))
    }

    /// Answers every ping sent so far, as a WebSocket stack does.
    public func answerPings() async {
        while let handler = lock.withLock({ () -> (@Sendable (LinkTransportEvent) async -> Void)? in
            guard owedPongs > 0 else {
                return nil
            }
            owedPongs -= 1
            return self.handler
        }) {
            await handler(.pong)
        }
    }

    /// Delivers an already queued or duplicate pong on this physical route.
    public func deliverPong() async {
        await lock.withLock { handler }?(.pong)
    }

    /// The Core closes the connection, or the link drops.
    public func dropLink() async {
        lock.withLock { dropped = true }
        await lock.withLock { handler }?(.closed)
    }
}

/// Makes a `ScriptedTransport` for each attempt a session dials.
public final class ScriptedStation: @unchecked Sendable {
    /// A certificate digest; the pin tests give the session.
    public let certificateSHA256: Data
    private let lock = NSLock()
    private var made: [ScriptedTransport] = []
    private var failure: LinkTransportError?

    public init(certificateSHA256: Data = Data((0..<32).map { _ in UInt8.random(in: 0...255) })) {
        self.certificateSHA256 = certificateSHA256
    }

    /// The trust that pins this station's certificate.
    public var trust: StationTrust { .certificate(pinSHA256: certificateSHA256) }

    /// Makes the next attempts fail to open with `failure`, or open again with nil.
    public func failOpens(with failure: LinkTransportError?) {
        lock.withLock { self.failure = failure }
    }

    public var factory: LinkTransportFactory {
        { [self] _, _ in
            lock.withLock {
                let transport = ScriptedTransport(presentedSHA256: certificateSHA256, openFailure: failure)
                made.append(transport)
                return transport
            }
        }
    }

    /// How many connections the session has dialled.
    public var dialCount: Int { lock.withLock { made.count } }

    /// Connections dialled that neither end has closed.
    public var openConnections: Int { lock.withLock { made.filter(\.isOpen).count } }

    /// The newest connection.
    public var latest: ScriptedTransport? { lock.withLock { made.last } }
}
