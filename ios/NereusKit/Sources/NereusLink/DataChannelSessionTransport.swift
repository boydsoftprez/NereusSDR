// NereusSDR for iOS: the control session over a data channel, for a Core reached through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// A ``SessionTransport`` over the control data channel (link document
/// section 20; iPhone app plan Task 28a, R-IOS-16; the remote design,
/// section 10.4). A session through the remote access service runs exactly
/// as over the WebSocket: the same messages, the same identity check before
/// anything is sent, the same caps, heartbeat, deadlines and endings. What
/// differs is here:
///
/// - Opening dials through the connector (`RendezvousDialer` in NereusMedia)
///   and returns the SHA-256 of the certificate the Core presented in DTLS,
///   which the session checks the Core's `hello` binding against at the same
///   gate as a WebSocket's certificate, before it sends anything.
/// - Each session message goes out as chunks (``ControlChannelFraming``),
///   never as a text data-channel message, and inbound chunks are joined
///   under the app's 8 MiB cap (``ControlChannelReassembler``). A message
///   over the cap, or anything the channel may not carry, ends the
///   connection as the WebSocket's cap does: the session sees the link lost.
/// - The heartbeat is 5-byte pings and pongs. A ping from the Core is
///   answered at once, here, as a WebSocket stack answers one; a pong
///   answering a ping this end sent is the only thing reported as `.pong`,
///   and the session's rule (a ping every 20 s, the link dead at a tick
///   with two unanswered) is unchanged.
/// - The Core sends its `hello` the moment its end opens, which can be
///   before ``open(onEvent:)`` returns. Everything the channel receives is
///   passed on in order from the start, and the session holds what arrives
///   while it is still opening.
public final class DataChannelSessionTransport: SessionTransport, @unchecked Sendable {
    /// The app's inbound cap, the same as the WebSocket's (section 12.3).
    public static let maxInboundMessageBytes = WebSocketLinkTransport.maxInboundMessageBytes
    /// Pings remembered while unanswered; far more than the session lets go
    /// unanswered before it declares the link dead.
    static let maxUnansweredPings = 16

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.controlchannel")

    private let connect: ControlChannelConnector

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var reassembler: ControlChannelReassembler
    private var channel: (any ControlChannel)?
    private var events: AsyncStream<LinkTransportEvent>.Continuation?
    /// Pongs owed for pings that arrived before the channel was handed over.
    private var owedPongs: [Data] = []
    private var unansweredPings: [UInt32] = []
    private var nextPingId: UInt32 = 1
    private var dialling: Task<ControlChannelConnection, Error>?
    private var finished = false
    private let trafficLifetime = UUID()
    private var receivedPayloadBytes: UInt64 = 0
    private var acceptedPayloadBytes: UInt64 = 0

    /// `connect` makes the control connection when the session opens this
    /// transport; `maxInboundMessageBytes` is the inbound cap.
    public init(maxInboundMessageBytes: Int = DataChannelSessionTransport.maxInboundMessageBytes,
                connect: @escaping ControlChannelConnector) {
        self.connect = connect
        reassembler = ControlChannelReassembler(maxMessageBytes: maxInboundMessageBytes)
    }

    /// A factory for ``StationSession/init(trust:authenticator:clock:transport:features:)``:
    /// a new transport, with its own connection, for every attempt.
    public static func factory(_ connect: @escaping ControlChannelConnector) -> @Sendable () -> any SessionTransport {
        { DataChannelSessionTransport(connect: connect) }
    }

    // MARK: SessionTransport

    /// The dial has its own bound (``RendezvousDialer``'s 79 s in
    /// NereusMedia): the session's 30 s connect deadline counts from the
    /// channel opening, as the desktop arms its handshake deadline when the
    /// dialer hands the channel over (StationClient::attachTransport).
    public var boundsItsOwnOpening: Bool { true }
    public var trafficObservation: LinkTrafficObservation? {
        lock.withLock {
            LinkTrafficObservation(lifetime: trafficLifetime, active: !finished && channel != nil,
                                   receivedPayloadBytes: receivedPayloadBytes,
                                   acceptedPayloadBytes: acceptedPayloadBytes)
        }
    }
    public var selectedRouteObservation: SelectedRouteObservation {
        let current = lock.withLock { finished ? nil : channel }
        guard let current else {
            return .unavailable(lock.withLock { finished ? .retired : .notReady })
        }
        let reading = current.selectedRouteObservation
        return lock.withLock { !finished && channel === current } ? reading : .unavailable(.retired)
    }

    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let (stream, continuation) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        let alreadyClosed: Bool = lock.withLock {
            guard !finished else {
                return true
            }
            events = continuation
            return false
        }
        guard !alreadyClosed else {
            continuation.finish()
            throw LinkTransportError.failed("closed before it opened")
        }
        Task {
            for await event in stream {
                await onEvent(event)
            }
        }
        // The dial runs as a task of its own, so closing the transport while
        // it is still dialling calls it off (a session redialling after a
        // network change closes an attempt that is still opening).
        let connect = self.connect
        let onEvent: @Sendable (ControlChannelEvent) -> Void = { [weak self] event in
            self?.received(event)
        }
        let dial = Task {
            try await connect(onEvent)
        }
        let closedWhileStarting: Bool = lock.withLock {
            guard !finished else {
                return true
            }
            dialling = dial
            return false
        }
        if closedWhileStarting {
            dial.cancel()
        }
        let connection: ControlChannelConnection
        do {
            connection = try await withTaskCancellationHandler {
                try await dial.value
            } onCancel: {
                dial.cancel()
            }
        } catch {
            finish(reportClosed: false)
            throw error
        }
        let taken: Bool = lock.withLock {
            dialling = nil
            guard !finished else {
                return false
            }
            channel = connection.channel
            for pong in owedPongs {
                _ = connection.channel.send(pong)
            }
            owedPongs = []
            return true
        }
        guard taken else {
            // Closed while dialling, or the channel ended as it opened.
            connection.channel.close()
            throw LinkTransportError.failed("the control connection closed as it opened")
        }
        return connection.certificateSHA256
    }

    @discardableResult public func send(_ text: String) -> Bool {
        let bytes = Data(text.utf8)
        let (admitted, failed): (Bool, Bool) = lock.withLock {
            guard !finished, let channel else {
                return (false, false)
            }
            for chunk in ControlChannelFraming.chunks(of: bytes) where !channel.send(chunk) {
                return (false, true)
            }
            acceptedPayloadBytes &+= UInt64(bytes.count)
            return (true, false)
        }
        if failed {
            Self.logger.info("A message could not be sent on the control channel")
            finish(reportClosed: true)
        }
        return admitted
    }

    public func ping() {
        let failed: Bool = lock.withLock {
            guard !finished, let channel else {
                return false
            }
            let id = nextPingId
            nextPingId = nextPingId == UInt32.max ? 1 : nextPingId + 1
            unansweredPings.append(id)
            if unansweredPings.count > Self.maxUnansweredPings {
                unansweredPings.removeFirst(unansweredPings.count - Self.maxUnansweredPings)
            }
            return !channel.send(ControlChannelFraming.ping(id: id))
        }
        if failed {
            finish(reportClosed: true)
        }
    }

    public func close() {
        finish(reportClosed: false)
    }

    // MARK: Receiving (on the library's threads)

    private func received(_ event: ControlChannelEvent) {
        var refusal: String?
        lock.withLock {
            guard !finished else {
                return
            }
            let result: ControlChannelReassembler.Result
            switch event {
            case .binary(let frame):
                result = reassembler.feed(frame)
            case .text:
                result = reassembler.textMessage()
            case .closed:
                refusal = ""
                return
            }
            switch result {
            case .pending:
                break
            case .message(let bytes):
                guard let text = String(data: bytes, encoding: .utf8) else {
                    refusal = "a message that is not UTF-8"
                    return
                }
                receivedPayloadBytes &+= UInt64(bytes.count)
                events?.yield(.text(text))
            case .ping(let id):
                let pong = ControlChannelFraming.pong(id: id)
                if let channel {
                    _ = channel.send(pong)
                } else {
                    owedPongs.append(pong)
                }
            case .pong(let id):
                // Only a pong for a ping this end sent says the link is alive.
                guard let index = unansweredPings.firstIndex(of: id) else {
                    return
                }
                unansweredPings.removeSubrange(...index)
                events?.yield(.pong)
            case .refused(let why):
                refusal = why
            }
        }
        guard let refusal else {
            return
        }
        if refusal.isEmpty {
            Self.logger.info("The control channel closed")
        } else {
            Self.logger.warning("The control channel ended: \(refusal, privacy: .public)")
        }
        finish(reportClosed: true)
    }

    /// Ends the connection once: closes the channel and ends the events,
    /// reporting `.closed` first when the session did not close it itself.
    private func finish(reportClosed: Bool) {
        let (channel, events, dial) = lock.withLock {
            () -> ((any ControlChannel)?, AsyncStream<LinkTransportEvent>.Continuation?,
                   Task<ControlChannelConnection, Error>?) in
            guard !finished else {
                return (nil, nil, nil)
            }
            finished = true
            defer {
                self.channel = nil
                self.events = nil
                dialling = nil
                owedPongs = []
            }
            return (self.channel, self.events, dialling)
        }
        dial?.cancel()
        channel?.close()
        if reportClosed {
            events?.yield(.closed)
        }
        events?.finish()
    }
}
