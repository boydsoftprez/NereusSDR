// NereusSDR for iOS: an in-process control data channel the tests play the Core's end of
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The session's real ``DataChannelSessionTransport`` over a control
/// channel whose far end the test plays as the Core does (link document
/// section 20): every session message the Core sends crosses as chunks, the
/// app's chunks are joined under the Core's own 1 MiB cap, and the heartbeat
/// is 5-byte pings and pongs. It offers what ``ScriptedTransport`` offers, so
/// a session fixture runs over either (``SessionFixturePlayer``'s
/// data-channel mode). Every call that delivers something to the session
/// returns once the session has handled it.
public final class ScriptedDataChannel: SessionTransport, @unchecked Sendable {
    /// The Core's inbound cap (link document section 12.3).
    public static let coreInboundBytes = StationSession.maxOutboundMessageBytes

    private let presentedSHA256: Data
    private var transport: DataChannelSessionTransport!

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var toApp: (@Sendable (ControlChannelEvent) -> Void)?
    private var reassembler = ControlChannelReassembler(maxMessageBytes: ScriptedDataChannel.coreInboundBytes)
    private var sent: [String] = []
    private var frames: [Data] = []
    private var owedPongs: [UInt32] = []
    private var pings = 0
    private var refusedWhy: String?
    private var closedByApp = false
    private var dropped = false
    /// Events the session has handled, counted by the wrapper around its
    /// handler, and how many the far end has caused.
    private var handled = 0
    private var caused = 0
    private let waiters = ConditionWaiters()

    public init(presentedSHA256: Data) {
        self.presentedSHA256 = presentedSHA256
        transport = DataChannelSessionTransport { [weak self] onEvent in
            guard let self else {
                throw LinkTransportError.failed("gone")
            }
            self.lock.withLock { self.toApp = onEvent }
            return ControlChannelConnection(channel: ChannelEnd(owner: self), certificateSHA256: self.presentedSHA256)
        }
    }

    // MARK: The session's end (SessionTransport)

    public var boundsItsOwnOpening: Bool { transport.boundsItsOwnOpening }
    public var trafficObservation: LinkTrafficObservation? { transport.trafficObservation }

    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        try await transport.open { [weak self] event in
            await onEvent(event)
            self?.handledOne()
        }
    }

    @discardableResult public func send(_ text: String) -> Bool {
        transport.send(text)
    }

    public func ping() {
        transport.ping()
    }

    public func close() {
        transport.close()
    }

    // MARK: The channel, as the transport sees it

    /// The channel end the transport holds.
    private final class ChannelEnd: ControlChannel, @unchecked Sendable {
        weak var owner: ScriptedDataChannel?

        init(owner: ScriptedDataChannel) {
            self.owner = owner
        }

        func send(_ frame: Data) -> Bool {
            owner?.appSent(frame) ?? false
        }

        func close() {
            owner?.appClosed()
        }
    }

    fileprivate func appSent(_ frame: Data) -> Bool {
        let accepted: Bool = lock.withLock {
            guard !closedByApp, !dropped, refusedWhy == nil else {
                return false
            }
            frames.append(frame)
            switch reassembler.feed(frame) {
            case .pending, .pong:
                break
            case .message(let bytes):
                sent.append(String(decoding: bytes, as: UTF8.self))
            case .ping(let id):
                pings += 1
                owedPongs.append(id)
            case .refused(let why):
                refusedWhy = why
            }
            return true
        }
        waiters.release()
        return accepted
    }

    fileprivate func appClosed() {
        lock.withLock { closedByApp = true }
        waiters.release()
    }

    // MARK: The Core's end

    /// Session messages the app sent and the test has not taken yet.
    public var pending: [String] { lock.withLock { sent } }
    /// Every data-channel message the app sent, in order.
    public var sentFrames: [Data] { lock.withLock { frames } }
    /// Why the Core's end would have ended the connection, if it would.
    public var refusal: String? { lock.withLock { refusedWhy } }

    /// Waits until the app has sent a message the test has not taken yet.
    @discardableResult
    public func waitForSent(within timeout: Duration = .seconds(30)) async -> Bool {
        await waiters.wait(within: timeout) { [self] in !pending.isEmpty }
    }

    public var pingCount: Int { lock.withLock { pings } }
    public var isClosedByApp: Bool { lock.withLock { closedByApp } }
    public var isOpen: Bool { lock.withLock { !closedByApp && !dropped } }

    /// Takes the oldest message the app sent, if any.
    public func takeSent() -> String? {
        lock.withLock { sent.isEmpty ? nil : sent.removeFirst() }
    }

    /// The Core sends one session message, as chunks.
    public func deliver(_ text: String) async {
        let chunks = ControlChannelFraming.chunks(of: Data(text.utf8))
        await deliverFrames(chunks, events: 1)
    }

    public func deliver(_ message: LinkMessage) async {
        await deliver(LinkCodec.encode(message))
    }

    /// The Core sends `frames` as they are; `events` is how many session
    /// events they cause, to wait for.
    public func deliverFrames(_ frames: [Data], events: Int) async {
        guard let toApp = lock.withLock({ () -> (@Sendable (ControlChannelEvent) -> Void)? in
            caused += events
            return self.toApp
        }) else {
            return
        }
        for frame in frames {
            toApp(.binary(frame))
        }
        await waitForHandled()
    }

    /// Answers every ping the app sent so far, oldest first, as the Core's
    /// transport does.
    public func answerPings() async {
        let (ids, toApp) = lock.withLock { () -> ([UInt32], (@Sendable (ControlChannelEvent) -> Void)?) in
            let ids = owedPongs
            owedPongs = []
            caused += ids.count
            return (ids, self.toApp)
        }
        guard let toApp else {
            return
        }
        for id in ids {
            toApp(.binary(ControlChannelFraming.pong(id: id)))
        }
        await waitForHandled()
    }

    /// The Core closes the connection, or the link drops.
    public func dropLink() async {
        let toApp = lock.withLock { () -> (@Sendable (ControlChannelEvent) -> Void)? in
            dropped = true
            caused += 1
            return self.toApp
        }
        toApp?(.closed)
        await waitForHandled()
    }

    private func handledOne() {
        lock.withLock { handled += 1 }
        waiters.release()
    }

    private func waitForHandled() async {
        _ = await waiters.wait(within: .seconds(30)) { [self] in
            lock.withLock { handled >= caused || closedByApp }
        }
    }
}

/// Makes a ``ScriptedDataChannel`` for each attempt a session dials.
public final class ScriptedDataChannelStation: @unchecked Sendable {
    /// The certificate digest the Core presents in DTLS.
    public let certificateSHA256: Data
    private let lock = NSLock()
    private var made: [ScriptedDataChannel] = []

    public init(certificateSHA256: Data = Data((0..<32).map { _ in UInt8.random(in: 0...255) })) {
        self.certificateSHA256 = certificateSHA256
    }

    /// The trust that pins this station's certificate.
    public var trust: StationTrust { .certificate(pinSHA256: certificateSHA256) }

    public var factory: @Sendable () -> any SessionTransport {
        { [self] in
            lock.withLock {
                let channel = ScriptedDataChannel(presentedSHA256: certificateSHA256)
                made.append(channel)
                return channel
            }
        }
    }

    /// How many connections the session has dialled.
    public var dialCount: Int { lock.withLock { made.count } }

    /// The newest connection.
    public var latest: ScriptedDataChannel? { lock.withLock { made.last } }
}
