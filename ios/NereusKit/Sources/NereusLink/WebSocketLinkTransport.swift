// NereusSDR for iOS: the TLS WebSocket to a Core, on Network framework
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Network
import os

/// The control connection (link document section 2): a WebSocket over TLS
/// 1.2 or later. Under a certificate trust the Core's certificate is
/// checked against the pin inside the TLS handshake, so a wrong certificate
/// ends the connection before any byte of the Core's `hello` is read. Under
/// an identity or pairing trust any certificate is accepted here and its
/// SHA-256 reported: the session checks the Core's identity key and its
/// binding of that certificate before it sends anything. Inbound messages are capped at
/// 8 MiB by the WebSocket itself, and the Core's pings are answered by it.
/// The opening request names the Core in its Host header as RFC 7230
/// requires, and an opening that has not finished by the connect deadline
/// fails as "no reply".
public final class WebSocketLinkTransport: LinkTransport, @unchecked Sendable {
    /// The app's inbound cap, applied by the WebSocket before a frame is read.
    public static let maxInboundMessageBytes = 8 * 1024 * 1024

    /// The ping payload the link uses; it is never inspected.
    static let pingPayload = Data("nereus".utf8)

    /// How long an opening may take from the moment the app dials: the
    /// link's connect deadline (section 12.2). The desktop's StationClient
    /// arms its kStationHandshakeDeadlineMs (30000) at the dial too, before
    /// the WebSocket upgrade, because the upgrade is part of what stalls.
    /// A session and a pairing run the same bound from the dial on their
    /// own clock, which covers the opening and the rest of the connect
    /// sequence together; this one ends the opening itself for any caller.
    public static let openDeadline: Duration = StationSession.connectDeadline

    /// Makes the real connection for a session.
    public static let factory: LinkTransportFactory = { endpoint, trust in
        WebSocketLinkTransport(endpoint: endpoint, trust: trust)
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.transport")

    private let endpoint: StationEndpoint
    private let trust: StationTrust
    private let openDeadline: Duration
    private let proxyResolver: SystemProxyResolver
    private let traffic: TrafficCounter
    private let openingObserver: WebSocketOpeningObserver?
    private let queue = DispatchQueue(label: "NereusSDR.link.websocket")

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var connection: NWConnection?
    private var presentedSHA256: Data?
    private var opening: CheckedContinuation<Data, Error>?
    private var events: AsyncStream<LinkTransportEvent>.Continuation?
    private var openingTask: Task<Data, Error>?
    private var attemptID = 0
    private var activeRoute: SystemProxyRoute?
    private var established = false
    private var finished = false
    private var binaryReceiver: (@Sendable (Data) -> Void)?
    private struct BinaryItem {
        let frame: Data
        let ownership: BinaryMediaOwnership?
    }
    private var binaryQueue: [BinaryItem] = []
    private var binaryQueuedBytes = 0
    private var binarySending = false
    private let trafficLifetime = UUID()
    private var receivedPayloadBytes: UInt64 = 0
    private var acceptedPayloadBytes: UInt64 = 0

    public var trafficObservation: LinkTrafficObservation? {
        lock.withLock {
            LinkTrafficObservation(lifetime: trafficLifetime, active: !finished && established && connection != nil,
                                   receivedPayloadBytes: receivedPayloadBytes,
                                   acceptedPayloadBytes: acceptedPayloadBytes)
        }
    }

    public convenience init(endpoint: StationEndpoint, trust: StationTrust, openDeadline: Duration = WebSocketLinkTransport.openDeadline) {
        self.init(endpoint: endpoint, trust: trust, openDeadline: openDeadline, proxyResolver: SystemProxyResolver())
    }

    init(endpoint: StationEndpoint, trust: StationTrust, openDeadline: Duration,
         proxyResolver: SystemProxyResolver, traffic: TrafficCounter = .shared,
         openingObserver: WebSocketOpeningObserver? = nil) {
        self.endpoint = endpoint
        self.trust = trust
        self.openDeadline = openDeadline
        self.proxyResolver = proxyResolver
        self.traffic = traffic
        self.openingObserver = openingObserver
    }

    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let startedAt = ContinuousClock().now
        openingObserver?(.init(phase: "open.stamp", budget: openDeadline, startedAt: startedAt))
        guard endpoint.port != 0 else {
            openingObserver?(.init(phase: "open.error", reason: "noPort"))
            throw LinkTransportError.failed("no port")
        }
        let (stream, continuation) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        Task {
            for await event in stream {
                await onEvent(event)
            }
        }

        guard case .url(let url) = Self.url(for: endpoint) else {
            continuation.finish()
            openingObserver?(.init(phase: "open.error", reason: "invalidAddress"))
            throw LinkTransportError.failed("not a Core address")
        }
        let task = Task { [self] in
            openingObserver?(.init(phase: "open.task.entry"))
            return try await SystemProxyWebSocketOpening.run(
                target: url, timeout: openDeadline, startedAt: startedAt, resolver: proxyResolver,
                observer: openingObserver
            ) { route, remaining in
                try await self.openAttempt(route: route, timeout: remaining, events: continuation)
            }
        }
        let closed = lock.withLock { () -> Bool in
            if finished { return true }
            openingTask = task
            events = continuation
            return false
        }
        if closed {
            continuation.finish()
            task.cancel()
        }
        return try await withTaskCancellationHandler {
            defer { lock.withLock { openingTask = nil } }
            do {
                let digest = try await task.value
                if lock.withLock({ finished }) { throw LinkTransportError.failed("closed") }
                openingObserver?(.init(phase: "open.success"))
                return digest
            } catch {
                let wasClosed = lock.withLock { finished }
                close()
                if wasClosed || Task.isCancelled || error is CancellationError {
                    openingObserver?(.init(phase: "open.error", reason: "closed"))
                    throw LinkTransportError.failed("closed")
                }
                if let proxyError = error as? SystemProxyError {
                    openingObserver?(.init(phase: "open.error", reason: WebSocketOpeningReceipt.reason(for: proxyError)))
                    throw LinkTransportError.failed(proxyError.openingFailureText)
                }
                openingObserver?(.init(phase: "open.error", reason: WebSocketOpeningReceipt.reason(for: error)))
                throw error
            }
        } onCancel: {
            close()
        }
    }

    private func openAttempt(route: SystemProxyRoute, timeout: Duration,
                             events continuation: AsyncStream<LinkTransportEvent>.Continuation) async throws -> Data {
        let deadline = ContinuousClock().now + timeout
        openingObserver?(.init(phase: "attempt.entry", budget: timeout, reason: WebSocketOpeningReceipt.route(route)))
        guard let port = NWEndpoint.Port(rawValue: endpoint.port) else { throw LinkTransportError.failed("no port") }
        let id = lock.withLock { () -> Int in
            attemptID += 1
            return attemptID
        }

        let tls = NWProtocolTLS.Options()
        sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)
        // Every connection makes a full handshake: a resumed TLS session
        // presents no certificate, and the session needs this connection's
        // certificate to check the pin or the Core's binding of it.
        sec_protocol_options_set_tls_resumption_enabled(tls.securityProtocolOptions, false)
        sec_protocol_options_set_tls_tickets_enabled(tls.securityProtocolOptions, false)
        sec_protocol_options_set_verify_block(tls.securityProtocolOptions, { [weak self] _, secTrust, complete in
            complete(self?.verify(secTrust, attemptID: id) ?? false)
        }, queue)

        let webSocket = NWProtocolWebSocket.Options()
        webSocket.autoReplyPing = true
        webSocket.maximumMessageSize = Self.maxInboundMessageBytes
        // Network framework writes the Host header as the bare address with
        // no port (`::1` for `wss://[::1]:47910/`), which a Core cannot read
        // as an IPv6 literal and never answers. This one replaces it.
        webSocket.setAdditionalHeaders([("Host", Self.hostHeader(for: endpoint))])

        let parameters = NWParameters(tls: tls, tcp: NWProtocolTCP.Options())
        parameters.defaultProtocolStack.applicationProtocols.insert(webSocket, at: 0)
        SystemProxyNetworkAdapter.apply(route, to: parameters)
        // The WebSocket needs a URL endpoint to make its opening request.
        let target = Self.url(for: endpoint) ?? .hostPort(host: NWEndpoint.Host(endpoint.host), port: port)
        let connection = NWConnection(to: target, using: parameters)

        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (opening: CheckedContinuation<Data, Error>) in
                let alreadyClosed = lock.withLock { () -> Bool in
                    if finished || Task.isCancelled {
                        return true
                    }
                    self.connection = connection
                    self.activeRoute = route
                    self.presentedSHA256 = nil
                    self.events = continuation
                    self.opening = opening
                    return false
                }
                if alreadyClosed {
                    continuation.finish()
                    openingObserver?(.init(phase: "attempt.settle.error", reason: "closedBeforeOpening"))
                    opening.resume(throwing: LinkTransportError.failed("closed before opening"))
                    return
                }
                connection.stateUpdateHandler = { [weak self] state in
                    self?.stateChanged(state, on: connection)
                }
                openingObserver?(.init(phase: "attempt.networkStart.before"))
                connection.start(queue: queue)
                openingObserver?(.init(phase: "attempt.networkStart.after"))
                let remaining = deadline - ContinuousClock().now
                guard remaining > .zero else {
                    openingObserver?(.init(phase: "attempt.timeout.afterSetup", budget: remaining))
                    failAttempt(.noReply, on: connection)
                    return
                }
                let (seconds, attoseconds) = remaining.components
                let nanoseconds = Int(seconds) * 1_000_000_000 + Int(attoseconds / 1_000_000_000)
                queue.asyncAfter(deadline: .now() + .nanoseconds(nanoseconds)) { [weak self] in
                    self?.openDeadlinePassed(on: connection)
                }
                openingObserver?(.init(phase: "attempt.timerBudget", budget: remaining))
            }
        } onCancel: {
            failAttempt(.failed("cancelled"), on: connection)
        }
    }

    @discardableResult public func send(_ text: String) -> Bool {
        guard let connection = lock.withLock({ finished ? nil : connection }) else {
            return false
        }
        let bytes = Data(text.utf8)
        let metadata = NWProtocolWebSocket.Metadata(opcode: .text)
        let context = NWConnection.ContentContext(identifier: "text", metadata: [metadata])
        connection.send(content: bytes, contentContext: context, isComplete: true,
                        completion: .contentProcessed { [weak self] error in
                            if error == nil {
                                self?.traffic.sent(bytes.count, as: .control)
                                self?.lock.withLock {
                                    guard let self, !self.finished, self.connection === connection else { return }
                                    self.acceptedPayloadBytes &+= UInt64(bytes.count)
                                }
                            } else {
                                Self.logger.warning("Sending to the Core failed")
                                self?.finish(on: connection)
                            }
                        })
        return true
    }

    public func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {
        lock.withLock { binaryReceiver = receiver }
    }

    @discardableResult public func sendBinary(_ frame: Data) -> Bool {
        let accepted = enqueueBinary(frame, ownership: nil)
        if accepted { drainBinary() }
        return accepted
    }

    @discardableResult public func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        let accepted = ownership.withActive { enqueueBinary(frame, ownership: ownership) } ?? false
        // Queue admission is atomic with retirement, but physical handoff
        // checks the permit again. Do not reacquire its nonrecursive lock
        // while admission still holds it.
        if accepted { drainBinary() }
        return accepted
    }

    private func enqueueBinary(_ frame: Data, ownership: BinaryMediaOwnership?) -> Bool {
        guard (18...1501).contains(frame.count), frame.first == 2 else { return false }
        let accepted = lock.withLock { () -> Bool in
            guard !finished, connection != nil else { return false }
            binaryQueue.append(BinaryItem(frame: frame, ownership: ownership))
            binaryQueuedBytes += frame.count
            while binaryQueue.count > 64 || binaryQueuedBytes > 24_576 {
                binaryQueuedBytes -= binaryQueue.removeFirst().frame.count
            }
            return true
        }
        return accepted
    }

    private func drainBinary() {
        let next = lock.withLock { () -> (NWConnection, BinaryItem)? in
            guard !finished, !binarySending, let connection, !binaryQueue.isEmpty else { return nil }
            binarySending = true
            let item = binaryQueue.removeFirst()
            binaryQueuedBytes -= item.frame.count
            return (connection, item)
        }
        guard let (connection, item) = next else { return }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .binary)
        let context = NWConnection.ContentContext(identifier: "media", metadata: [metadata])
        let handoff = { [weak self] in
            connection.send(content: item.frame, contentContext: context, isComplete: true,
                            completion: .contentProcessed { [weak self] error in
                                guard let self else { return }
                                self.lock.withLock { self.binarySending = false }
                                if error == nil { self.drainBinary() }
                                else { self.finish(on: connection) }
                            })
        }
        if let owner = item.ownership {
            guard owner.withActive(handoff) != nil else {
                lock.withLock { binarySending = false }
                drainBinary()
                return
            }
        } else { handoff() }
    }

    public func discardBinary(ownership: BinaryMediaOwnership) {
        lock.withLock {
            binaryQueue.removeAll { $0.ownership === ownership }
            binaryQueuedBytes = binaryQueue.reduce(0) { $0 + $1.frame.count }
        }
    }

    public func ping() {
        guard let connection = lock.withLock({ finished ? nil : connection }) else {
            return
        }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .ping)
        metadata.setPongHandler(queue) { [weak self] error in
            if error == nil {
                self?.emit(.pong)
            }
        }
        let context = NWConnection.ContentContext(identifier: "ping", metadata: [metadata])
        connection.send(content: Self.pingPayload, contentContext: context, isComplete: true, completion: .idempotent)
    }

    public var selectedRouteObservation: SelectedRouteObservation {
        let snapshot = lock.withLock { () -> (NWConnection, SystemProxyRoute)? in
            guard !finished, established, let connection, let activeRoute else { return nil }
            return (connection, activeRoute)
        }
        guard let (current, route) = snapshot else {
            return .unavailable(lock.withLock { finished ? .retired : .notReady })
        }
        guard current.state == .ready else { return .unavailable(.notReady) }
        let reading = SelectedRouteObservation.fromReadyWebSocket(
            pathEndpoint: current.currentPath?.remoteEndpoint, usesSystemProxy: route != .direct)
        return lock.withLock { !finished && connection === current && activeRoute == route && current.state == .ready }
            ? reading : .unavailable(.retired)
    }

    public func close() {
        let (connection, opening, task) = lock.withLock { () -> (NWConnection?, CheckedContinuation<Data, Error>?, Task<Data, Error>?) in
            finished = true
            let taken = (self.connection, self.opening, self.openingTask)
            self.opening = nil
            self.connection = nil
            self.activeRoute = nil
            self.openingTask = nil
            binaryReceiver = nil
            binaryQueue.removeAll()
            binaryQueuedBytes = 0
            events?.finish()
            events = nil
            return taken
        }
        task?.cancel()
        if opening != nil {
            openingObserver?(.init(phase: "attempt.settle.error", reason: "closed"))
        }
        opening?.resume(throwing: LinkTransportError.failed("closed"))
        guard let connection else {
            return
        }
        guard connection.state == .ready else {
            connection.cancel()
            return
        }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .close)
        metadata.closeCode = .protocolCode(.normalClosure)
        let context = NWConnection.ContentContext(identifier: "close", metadata: [metadata])
        connection.send(content: nil, contentContext: context, isComplete: true,
                        completion: .contentProcessed { _ in connection.cancel() })
    }

    /// `wss://host:port/`, with an IPv6 literal in brackets and its zone,
    /// if any, escaped as a URL requires (`%25`).
    static func url(for endpoint: StationEndpoint) -> NWEndpoint? {
        guard let url = URL(string: "wss://\(hostHeader(for: endpoint))/") else {
            return nil
        }
        return .url(url)
    }

    /// The Host header's value (RFC 7230 section 5.4): an IPv6 literal in
    /// brackets with its zone, if any, as `%25` (RFC 6874), a name or IPv4
    /// address bare, then `:port`.
    static func hostHeader(for endpoint: StationEndpoint) -> String {
        let host = endpoint.host.contains(":")
            ? "[" + endpoint.host.replacingOccurrences(of: "%", with: "%25") + "]"
            : endpoint.host
        return "\(host):\(endpoint.port)"
    }

    // MARK: Network framework callbacks, on `queue`

    /// The opening is still in progress at the deadline: nothing answered.
    private func openDeadlinePassed(on connection: NWConnection) {
        let pending = lock.withLock { self.connection === connection && self.opening != nil }
        guard pending else {
            return
        }
        openingObserver?(.init(phase: "attempt.timeout.callback"))
        Self.logger.info("The Core did not answer the connection in time")
        failAttempt(LinkTransportError.noReply, on: connection)
    }

    /// Checks the certificate the Core presented against the pin, or, under
    /// an identity or pairing trust, only records its digest.
    private func verify(_ secTrust: sec_trust_t, attemptID: Int) -> Bool {
        let trustRef = sec_trust_copy_ref(secTrust).takeRetainedValue()
        guard let chain = SecTrustCopyCertificateChain(trustRef) as? [SecCertificate], let leaf = chain.first else {
            return false
        }
        let digest = CertificatePin.sha256(ofCertificate: SecCertificateCopyData(leaf) as Data)
        let current = lock.withLock { () -> Bool in
            guard !finished && self.attemptID == attemptID else { return false }
            presentedSHA256 = digest
            return true
        }
        guard current else { return false }
        guard case .certificate(let pin) = trust else {
            return true
        }
        let matches = CertificatePin.matches(digest, pin: pin)
        if !matches {
            Self.logger.warning("The Core's certificate does not match the pin; closing before reading anything")
        }
        return matches
    }

    private func stateChanged(_ state: NWConnection.State, on connection: NWConnection) {
        openingObserver?(Self.receipt(for: state))
        guard lock.withLock({ self.connection === connection && !finished }) else { return }
        let route = lock.withLock { activeRoute }
        switch state {
        case .ready:
            let (opening, digest) = lock.withLock { () -> (CheckedContinuation<Data, Error>?, Data?) in
                guard self.connection === connection && !finished else { return (nil, nil) }
                let taken = self.opening
                self.opening = nil
                if taken != nil { established = true }
                return (taken, presentedSHA256)
            }
            guard let opening else {
                return
            }
            guard let digest else {
                // No certificate was presented; the Core cannot be checked.
                Self.logger.warning("The Core presented no certificate; closing before reading anything")
                openingObserver?(.init(phase: "attempt.settle.error", reason: "noCertificate"))
                opening.resume(throwing: LinkTransportError.failed("no certificate presented"))
                finish(on: connection)
                connection.cancel()
                return
            }
            openingObserver?(.init(phase: "attempt.settle.success"))
            opening.resume(returning: digest)
            receiveNext(on: connection)
        case .waiting(let error):
            // A ready session stays with session policy; only an unopened
            // connection can try the next configured proxy choice.
            let reason: LinkTransportError = Self.isLocalNetworkDenied(error, on: connection)
                ? .localNetworkDenied : Self.failure(for: error, route: route)
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt(reason, on: connection) }
        case .failed(let error):
            let reason: LinkTransportError = Self.isLocalNetworkDenied(error, on: connection)
                ? .localNetworkDenied : Self.failure(for: error, route: route)
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt(reason, on: connection) }
        case .cancelled:
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt(LinkTransportError.failed("cancelled"), on: connection) }
        default:
            break
        }
    }

    // Diagnostic state labels deliberately omit endpoints and error descriptions.
    private static func receipt(for state: NWConnection.State) -> WebSocketOpeningReceipt {
        switch state {
        case .setup: return .init(phase: "network.state.setup")
        case .preparing: return .init(phase: "network.state.preparing")
        case .ready: return .init(phase: "network.state.ready")
        case .cancelled: return .init(phase: "network.state.cancelled")
        case .waiting(let error): return receipt(phase: "network.state.waiting", error: error)
        case .failed(let error): return receipt(phase: "network.state.failed", error: error)
        @unknown default: return .init(phase: "network.state.other")
        }
    }

    private static func receipt(phase: String, error: NWError) -> WebSocketOpeningReceipt {
        switch error {
        case .posix(let code): return .init(phase: phase, reason: "posix", code: code.rawValue)
        case .tls(let code): return .init(phase: phase, reason: "tls", code: code)
        case .dns(let code): return .init(phase: phase, reason: "dns", code: code)
        @unknown default: return .init(phase: phase, reason: "other")
        }
    }

    /// True when iOS refused the connection because the app may not reach
    /// devices on this network: the path says so, or the refusal is the
    /// policy's.
    private static func isLocalNetworkDenied(_ error: NWError, on connection: NWConnection) -> Bool {
        if connection.currentPath?.unsatisfiedReason == .localNetworkDenied {
            return true
        }
        if case .dns(let code) = error, code == DNSServiceErrorType(kDNSServiceErr_PolicyDenied) {
            return true
        }
        return false
    }

    /// Fails an open still in progress, as a mismatch when the certificate
    /// the Core presented was not the pinned one, and as a refused local
    /// network when iOS would not let the app try.
    private func failAttempt(_ reason: LinkTransportError, on connection: NWConnection) {
        let (opening, digest) = lock.withLock { () -> (CheckedContinuation<Data, Error>?, Data?) in
            guard self.connection === connection && !established else { return (nil, nil) }
            let taken = self.opening
            self.opening = nil
            self.connection = nil
            self.activeRoute = nil
            return (taken, presentedSHA256)
        }
        guard let opening else {
            return
        }
        if let digest, case .certificate(let pin) = trust, !CertificatePin.matches(digest, pin: pin) {
            openingObserver?(.init(phase: "attempt.settle.error", reason: "link.certificateMismatch"))
            opening.resume(throwing: LinkTransportError.certificateMismatch)
        } else {
            openingObserver?(.init(phase: "attempt.settle.error", reason: WebSocketOpeningReceipt.reason(for: reason)))
            opening.resume(throwing: reason)
        }
        connection.cancel()
    }

    /// Why an opening failed, as the attempt record words it: on a direct
    /// route, a refused connection, no route to the address, or no reply;
    /// otherwise the network's failure. Through a proxy the answer is the
    /// proxy's, not the Core's.
    static func failure(for error: NWError, route: SystemProxyRoute?) -> LinkTransportError {
        if route == .direct, case .posix(let code) = error {
            switch code {
            case .ECONNREFUSED:
                return .refused
            case .ENETUNREACH, .EHOSTUNREACH, .ENETDOWN, .EHOSTDOWN, .EADDRNOTAVAIL:
                return .unreachable
            case .ETIMEDOUT:
                return .noReply
            default:
                break
            }
        }
        return .failed(networkFailureText(error, route: route))
    }

    private static func networkFailureText(_ error: NWError, route: SystemProxyRoute?) -> String {
        // Network does not identify a captive portal reliably from a socket
        // error. Keep the observed failure separate from possible causes.
        if case .tls = error {
            return "TLS connection failed; network inspection or a sign-in page may be involved"
        }
        if route != .direct {
            return "the network or its configured proxy did not open the connection"
        }
        return "the network did not open the connection"
    }

    private func receiveNext(on connection: NWConnection) {
        connection.receiveMessage { [weak self] content, context, isComplete, error in
            guard let self else {
                return
            }
            if let error {
                // A last message that came with the end is still the Core's:
                // it goes first (a pairing's confirmation just before the close).
                let metadata = context?.protocolMetadata(definition: NWProtocolWebSocket.definition)
                    as? NWProtocolWebSocket.Metadata
                if metadata?.opcode == .text, let content, let text = String(data: content, encoding: .utf8) {
                    self.emitText(text, byteCount: content.count, on: connection)
                }
                Self.logger.info("The connection to the Core ended: \(String(describing: error), privacy: .public)")
                self.finish(on: connection)
                connection.cancel()
                return
            }
            let metadata = context?.protocolMetadata(definition: NWProtocolWebSocket.definition)
                as? NWProtocolWebSocket.Metadata
            switch metadata?.opcode {
            case .text?:
                if let content, let text = String(data: content, encoding: .utf8) {
                    self.emitText(text, byteCount: content.count, on: connection)
                }
            case .binary?:
                if let content, (18...1501).contains(content.count), content.first == 2 {
                    let receiver = self.lock.withLock { self.finished ? nil : self.binaryReceiver }
                    receiver?(content)
                }
            case .close?:
                self.finish(on: connection)
                connection.cancel()
                return
            case nil where isComplete && content == nil:
                self.finish(on: connection)
                connection.cancel()
                return
            default:
                // Binary frames are not part of the link; pings are answered
                // by the WebSocket itself.
                break
            }
            self.receiveNext(on: connection)
        }
    }

    private func emit(_ event: LinkTransportEvent) {
        let events = lock.withLock { finished ? nil : self.events }
        events?.yield(event)
    }

    private func emitText(_ text: String, byteCount: Int, on connection: NWConnection) {
        let events = lock.withLock { () -> AsyncStream<LinkTransportEvent>.Continuation? in
            guard !finished, self.connection === connection else { return nil }
            receivedPayloadBytes &+= UInt64(byteCount)
            return self.events
        }
        guard let events else { return }
        traffic.received(byteCount, as: .control)
        events.yield(.text(text))
    }

    /// Reports the connection gone, once.
    private func finish(on expectedConnection: NWConnection) {
        let (events, connection) = lock.withLock { () -> (AsyncStream<LinkTransportEvent>.Continuation?, NWConnection?) in
            guard !finished && self.connection === expectedConnection else {
                return (nil, nil)
            }
            finished = true
            let taken = self.events
            let connection = self.connection
            self.events = nil
            self.connection = nil
            return (taken, connection)
        }
        connection?.cancel()
        events?.yield(.closed)
        events?.finish()
    }
}
