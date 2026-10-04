// NereusSDR for iOS: the app's client of the remote access service: introductions and pairing mailboxes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The app in the client role of the remote access service (the
/// rendezvous document, `docs/architecture/2026-09-23-rendezvous-v1.md`;
/// iPhone app plan Task 27a, R-IOS-08 and R-IOS-16; the pairing design,
/// section 5.3). One request at a time on one connection:
///
/// - ``introduce(stationId:device:offer:)`` asks the Core registered under
///   a rendezvous id for a connection, signed with this device's key over
///   "NereusSDR introduce v1\n" || the Core's id || this connection's hello
///   nonce (section 4.4), and returns the Core's answer with the relay
///   credentials, if any. Both ends' candidates then trickle through
///   ``events`` and ``sendCandidate(_:)``.
/// - ``connect()`` opens the first service that answers and returns its
///   hello's STUN URLs, so a control connection's peer is made with its
///   STUN server before the offer goes into the introduction (Task 28a).
/// - ``openMailbox(nameplate:)`` opens the pairing mailbox on the number in
///   a pairing code; bodies then travel through ``events`` and
///   ``sendMailbox(_:)`` (section 6.5).
///
/// Services are an ordered list, the operator's own first: each is tried in
/// turn and the first that answers with its `hello` is used. A service that
/// says the Core is not there (`offline`) or no Core shows that number
/// (`nameplateUnknown`) sends the request on to the next, since the Core
/// may be registered further down the list.
///
/// The rendezvous only introduces. Nothing here carries a session and
/// nothing here ends one: a session on a connection an introduction set up
/// keeps running when the service goes away, and the app simply closes
/// this client once its session runs (section 6.3). Nothing secret is
/// logged: no id, key, nonce, signature, SDP, candidate, body, credential or
/// nameplate number (the number is part of a pairing code).
public actor RendezvousClient {
    /// How long one service may take to open and send its hello before the
    /// next is tried: the service's own handshake time (section 9.1).
    public static let helloTimeout: Duration = .seconds(10)
    /// How long the app waits for the Core's answer, or for the service's
    /// answer to `mailbox.open`. A Core stays silent towards a device it has
    /// not paired, so this is also how long that silence takes to show.
    /// NereusSDR's own bound: the link's connect deadline (link document
    /// section 12.2), well inside the introduction's 120 s lifetime.
    public static let answerTimeout: Duration = StationSession.connectDeadline
    /// A silent older Core cannot hold the service rung beyond this time
    /// after its introduction was sent (station link section 21.1).
    public static let introductionAnswerTimeout: Duration = .seconds(10)

    /// What arrives once a request has been answered.
    public enum Event: Sendable, Equatable {
        /// One of the Core's candidates; the empty string ends them.
        case candidate(String)
        /// One service-issued grant after the answer to this introduction.
        case relayGrant(RelayGrant)
        /// The introduction ended (`stationLeft`, `expired`, or a code this
        /// app does not know). A session it set up is not touched.
        case introductionEnded(code: String)
        /// One mailbox message from the Core, untouched.
        case mailbox(body: String)
        /// The mailbox closed (`closed`, `peerClosed`, `peerLeft`,
        /// `released`, `expired`, or a code this app does not know).
        case mailboxClosed(code: String)
        /// An error the service sent outside a request, in its own words.
        case serviceError(RendezvousMessage.ServiceError)
        /// The connection to the service ended.
        case connectionLost
    }

    /// The Core's answer to an introduction.
    public struct Answer: Sendable, Equatable {
        /// The Core's SDP answer, untouched.
        public var sdp: String
        /// The relay credentials; nil when the Core keeps off the relay or
        /// the service has none.
        public var turn: RendezvousTurn?
        /// The STUN servers the service's hello listed, in its order.
        public var stun: [String]
        /// The service that carried it.
        public var server: RendezvousServer
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "rendezvous")

    /// Every event, in order, for one reader. Finishes when ``close()`` runs.
    public nonisolated let events: AsyncStream<Event>
    private let eventSink: AsyncStream<Event>.Continuation

    private let servers: [RendezvousServer]
    private let clock: any LinkClock
    private let makeTransport: RendezvousTransportFactory
    private let answerTimeout: Duration

    private enum Request {
        case connect(reply: CheckedContinuation<[String], Error>)
        case introduce(stationId: String, device: DeviceIdentity, offer: String,
                       reply: CheckedContinuation<Answer, Error>)
        case openMailbox(nameplate: Int, reply: CheckedContinuation<RendezvousServer, Error>)
    }

    private enum Phase {
        case idle
        case awaitingHello
        case awaitingReply
        case ready
    }

    private var phase = Phase.idle
    private var pending: Request?
    private var serverIndex = -1
    /// Moves with every connection, so a stale connection's event is ignored.
    private var generation = 0
    private var transport: (any LinkTransport)?
    private var helloTimer: (any LinkTimer)?
    private var replyTimer: (any LinkTimer)?
    private var helloNonce: Data?
    /// The STUN URLs the current service's hello listed.
    public private(set) var stunUrls: [String] = []
    /// Why the last service said no, for when none says yes.
    private var lastRefusal: RendezvousError?
    private var introductionLive = false
    private var receivedRelayGrant = false
    private var mailboxOpen = false
    private var closed = false

    /// `servers` in the order to try them; the NereusSDR service when the
    /// operator has named none.
    public init(servers: [RendezvousServer] = RendezvousServer.defaults, clock: any LinkClock = SystemLinkClock(),
                transportFactory: @escaping RendezvousTransportFactory = RendezvousWebSocket.factory,
                answerTimeout: Duration = RendezvousClient.answerTimeout) {
        self.servers = servers
        self.clock = clock
        makeTransport = transportFactory
        self.answerTimeout = answerTimeout
        (events, eventSink) = AsyncStream.makeStream(of: Event.self)
    }

    /// The service in use, nil when none is.
    public var currentServer: RendezvousServer? {
        transport != nil && servers.indices.contains(serverIndex) ? servers[serverIndex] : nil
    }

    // MARK: Requests

    /// Opens the first service in the list that answers and returns the
    /// STUN URLs its hello lists, in its order, before any request: a
    /// control connection's peer is made with its STUN server before the
    /// offer the introduction carries (link document section 20).
    /// ``introduce(stationId:device:offer:)`` then uses this connection.
    /// Throws ``RendezvousError/unreachable`` (or the last service's
    /// refusal) when none answers.
    public func connect() async throws -> [String] {
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (reply: CheckedContinuation<[String], Error>) in
                start(.connect(reply: reply))
            }
        } onCancel: {
            Task { await self.close() }
        }
    }

    /// Asks the Core registered as `stationId` for a connection, as
    /// `device`, with `offer`. Returns the Core's answer. Throws
    /// ``RendezvousError/offline(reason:)`` when no service has the Core,
    /// ``RendezvousError/noAnswer`` when the Core stays silent until
    /// ``answerTimeout``, and the service's refusal otherwise.
    public func introduce(stationId: String, device: DeviceIdentity, offer: String) async throws -> Answer {
        guard RendezvousIdentity.isStationId(stationId), (1...RendezvousMessage.maxSdpBytes).contains(offer.utf8.count) else {
            throw RendezvousError.protocolViolation
        }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (reply: CheckedContinuation<Answer, Error>) in
                start(.introduce(stationId: stationId, device: device, offer: offer, reply: reply))
            }
        } onCancel: {
            Task { await self.close() }
        }
    }

    /// Opens the pairing mailbox on `nameplate`, the number in a pairing
    /// code. Returns the service it opened on.
    public func openMailbox(nameplate: Int) async throws -> RendezvousServer {
        guard (1...RendezvousMessage.maxNameplate).contains(nameplate) else {
            throw RendezvousError.protocolViolation
        }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (reply: CheckedContinuation<RendezvousServer, Error>) in
                start(.openMailbox(nameplate: nameplate, reply: reply))
            }
        } onCancel: {
            Task { await self.close() }
        }
    }

    /// One of this device's candidates for its live introduction, or the
    /// empty string that ends them. An `a=` in front is removed. False when
    /// nothing was sent.
    @discardableResult
    public func sendCandidate(_ candidate: String) -> Bool {
        guard introductionLive else {
            return false
        }
        let value = candidate.hasPrefix("a=") ? String(candidate.dropFirst(2)) : candidate
        return send(.candidate(value))
    }

    /// One body into the open mailbox. False when nothing was sent.
    @discardableResult
    public func sendMailbox(_ body: String) -> Bool {
        guard mailboxOpen else {
            return false
        }
        return send(.mailbox(body: body))
    }

    /// Closes the open mailbox.
    public func closeMailbox() {
        guard mailboxOpen else {
            return
        }
        mailboxOpen = false
        _ = send(.mailboxClose)
    }

    /// Ends a request still waiting as not answered in time: with the last
    /// refusal a service gave, when one did, and as
    /// ``RendezvousError/noAnswer`` otherwise. Then closes.
    public func giveUp() {
        finish(with: lastRefusal ?? .noAnswer)
        close()
    }

    /// Closes the connection. The client is finished: a request still
    /// waiting fails, and ``events`` ends.
    public func close() {
        guard !closed else {
            return
        }
        closed = true
        dropConnection()
        finish(with: .connectionLost)
        eventSink.finish()
    }

    // MARK: The server list

    private func start(_ request: Request) {
        guard !closed else {
            fail(request, with: .connectionLost)
            return
        }
        guard pending == nil else {
            fail(request, with: .busy)
            return
        }
        if case .introduce = request, introductionLive {
            fail(request, with: .busy)
            return
        }
        if case .connect(let reply) = request, phase == .ready, transport != nil {
            reply.resume(returning: stunUrls)
            return
        }
        pending = request
        lastRefusal = nil
        if phase == .ready, transport != nil {
            sendRequest()
        } else {
            serverIndex = -1
            tryNextServer()
        }
    }

    private func tryNextServer() {
        dropConnection()
        serverIndex += 1
        guard servers.indices.contains(serverIndex) else {
            finish(with: lastRefusal ?? .unreachable)
            return
        }
        let server = servers[serverIndex]
        generation += 1
        let current = generation
        phase = .awaitingHello
        // The hello's timer before the connection exists (a load finding,
        // Task 56): a service that never greets is always timed out, however
        // late this runs against the clock.
        helloTimer = clock.schedule(after: Self.helloTimeout) { [weak self] in
            await self?.helloTimedOut(current)
        }
        let transport = makeTransport(server)
        self.transport = transport
        Self.logger.info("Opening the remote access service at \(server.host, privacy: .public)")
        Task { [weak self] in
            do {
                _ = try await transport.open { [weak self] event in
                    await self?.received(event, on: current)
                }
            } catch {
                await self?.openFailed(current)
            }
        }
    }

    private func dropConnection() {
        generation += 1
        helloTimer?.cancel()
        helloTimer = nil
        replyTimer?.cancel()
        replyTimer = nil
        transport?.close()
        transport = nil
        helloNonce = nil
        phase = .idle
        introductionLive = false
        receivedRelayGrant = false
        mailboxOpen = false
    }

    private func helloTimedOut(_ current: Int) {
        guard current == generation, phase == .awaitingHello else {
            return
        }
        Self.logger.info("The remote access service did not greet in time; trying the next")
        tryNextServer()
    }

    private func openFailed(_ current: Int) {
        guard current == generation, phase == .awaitingHello else {
            return
        }
        tryNextServer()
    }

    private func replyTimedOut(_ current: Int) {
        guard current == generation, phase == .awaitingReply else {
            return
        }
        Self.logger.info("No answer through the remote access service in time")
        finish(with: .noAnswer)
        dropConnection()
    }

    // MARK: Receiving

    private func received(_ event: LinkTransportEvent, on current: Int) {
        guard current == generation else {
            return
        }
        switch event {
        case .pong:
            return
        case .closed:
            connectionEnded()
        case .text(let text):
            guard text.utf8.count <= RendezvousMessage.maxReceivedBytes else {
                Self.logger.info("Ignoring a message over the size the app accepts")
                return
            }
            do {
                handle(try RendezvousMessage.decode(text, direction: .toClient))
            } catch {
                // Section 5.3: logged and ignored.
                Self.logger.info("Ignoring a message from the remote access service the app cannot read")
            }
        }
    }

    private func handle(_ message: RendezvousMessage) {
        switch message {
        case .hello(let hello):
            guard phase == .awaitingHello, let nonce = Base64URL.decode(hello.nonce),
                  nonce.count == RendezvousIdentity.nonceBytes else {
                return
            }
            helloTimer?.cancel()
            helloTimer = nil
            helloNonce = nonce
            stunUrls = hello.stun
            phase = .ready
            if case .connect(let reply)? = pending {
                pending = nil
                Self.logger.info("The remote access service greeted this phone")
                reply.resume(returning: stunUrls)
            } else if pending != nil {
                sendRequest()
            }
        case .error(let error):
            handleError(error)
        case .answer(let sdp, let turn):
            guard case .introduce(_, _, _, let reply)? = pending, phase == .awaitingReply else {
                return
            }
            replyTimer?.cancel()
            replyTimer = nil
            pending = nil
            phase = .ready
            introductionLive = true
            receivedRelayGrant = false
            Self.logger.info("The Core answered through the remote access service (relay offered: \(turn != nil, privacy: .public))")
            reply.resume(returning: Answer(sdp: sdp, turn: turn, stun: stunUrls, server: servers[serverIndex]))
        case .mailboxOpened(let opened):
            guard case .openMailbox(let asked, let reply)? = pending, phase == .awaitingReply else {
                return
            }
            // A mailbox on another number is not the one asked for.
            guard opened == asked else {
                Self.logger.warning("The remote access service opened another mailbox than the one asked for")
                finish(with: .protocolViolation)
                dropConnection()
                return
            }
            replyTimer?.cancel()
            replyTimer = nil
            pending = nil
            phase = .ready
            mailboxOpen = true
            Self.logger.info("The pairing mailbox opened")
            reply.resume(returning: servers[serverIndex])
        case .candidate(let candidate):
            if introductionLive {
                eventSink.yield(.candidate(candidate))
            }
        case .relayGrant(let grant):
            if introductionLive && !receivedRelayGrant {
                receivedRelayGrant = true
                eventSink.yield(.relayGrant(grant))
            }
        case .introductionEnd(let code):
            if case .introduce? = pending, phase == .awaitingReply {
                finish(with: .introductionEnded(code: code))
                dropConnection()
                return
            }
            if introductionLive {
                introductionLive = false
                receivedRelayGrant = false
                Self.logger.info("The introduction ended: \(code, privacy: .public)")
                eventSink.yield(.introductionEnded(code: code))
            }
        case .mailbox(let body):
            if mailboxOpen {
                eventSink.yield(.mailbox(body: body))
            }
        case .mailboxClosed(let code):
            mailboxOpen = false
            Self.logger.info("The pairing mailbox closed: \(code, privacy: .public)")
            eventSink.yield(.mailboxClosed(code: code))
        case .introduce, .mailboxOpen, .mailboxClose:
            // A client's kinds never decode in this direction.
            return
        }
    }

    private func handleError(_ error: RendezvousMessage.ServiceError) {
        Self.logger.info("The remote access service said \(error.code, privacy: .public)")
        let wait = Duration.milliseconds(error.retryAfterMs)
        switch (pending, phase) {
        case (.introduce?, .awaitingReply) where error.code == "offline":
            // The Core may be registered with a service further down the list.
            lastRefusal = .offline(reason: error.reason)
            tryNextServer()
        case (.openMailbox?, .awaitingReply) where error.code == "nameplateUnknown":
            lastRefusal = .nameplateUnknown(reason: error.reason)
            tryNextServer()
        case (.some, .awaitingReply):
            finish(with: .refused(code: error.code, reason: error.reason, retryAfter: wait))
            dropConnection()
        case (.some, .awaitingHello):
            // Sent instead of hello (tooManyConnections, overloaded); the
            // close follows, and then the next service is tried.
            lastRefusal = .refused(code: error.code, reason: error.reason, retryAfter: wait)
        default:
            eventSink.yield(.serviceError(error))
        }
    }

    private func connectionEnded() {
        switch phase {
        case .awaitingHello:
            tryNextServer()
        case .awaitingReply:
            finish(with: .connectionLost)
            dropConnection()
        case .ready, .idle:
            dropConnection()
            Self.logger.info("The connection to the remote access service ended")
            eventSink.yield(.connectionLost)
        }
    }

    // MARK: Sending

    private func sendRequest() {
        guard let request = pending, let nonce = helloNonce else {
            return
        }
        let message: RendezvousMessage
        switch request {
        case .introduce(let stationId, let device, let offer, _):
            guard let signature = try? RendezvousIdentity.introduceSignature(device: device, stationId: stationId,
                                                                             nonce: nonce) else {
                finish(with: .protocolViolation)
                return
            }
            message = .introduce(RendezvousMessage.Introduce(id: stationId, device: device.id,
                                                             deviceSignature: Base64URL.encode(signature),
                                                             offer: offer))
        case .openMailbox(let nameplate, _):
            message = .mailboxOpen(nameplate: nameplate)
        case .connect:
            // Answered by the hello itself; nothing to send.
            return
        }
        guard send(message) else {
            finish(with: .protocolViolation)
            return
        }
        phase = .awaitingReply
        let current = generation
        let timeout: Duration
        if case .introduce = request {
            timeout = min(answerTimeout, Self.introductionAnswerTimeout)
        } else {
            timeout = answerTimeout
        }
        replyTimer = clock.schedule(after: timeout) { [weak self] in
            await self?.replyTimedOut(current)
        }
    }

    /// Sends `message` when it keeps the sender's rules (section 2).
    private func send(_ message: RendezvousMessage) -> Bool {
        guard let transport, let text = message.encodedForSending(.toService) else {
            return false
        }
        return transport.send(text)
    }

    // MARK: Ending a request

    private func finish(with error: RendezvousError) {
        guard let request = pending else {
            return
        }
        pending = nil
        if phase == .awaitingReply {
            phase = transport == nil ? .idle : .ready
        }
        fail(request, with: error)
    }

    private func fail(_ request: Request, with error: RendezvousError) {
        switch request {
        case .connect(let reply):
            reply.resume(throwing: error)
        case .introduce(_, _, _, let reply):
            reply.resume(throwing: error)
        case .openMailbox(_, let reply):
            reply.resume(throwing: error)
        }
    }
}
