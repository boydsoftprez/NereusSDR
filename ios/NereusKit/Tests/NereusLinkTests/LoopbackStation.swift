// NereusSDR for iOS: a TLS WebSocket Core on 127.0.0.1, written with Network framework for the tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Network
@testable import NereusLink

/// A small stand-in Core: it listens on 127.0.0.1 with a generated
/// certificate, sends its `hello` as soon as a connection opens, and
/// answers the app's `auth.request` with the rest of the connect sequence.
/// It records what it received and answers pings itself only when told to.
/// Given an identity, its `hello` carries that identity's key and binding
/// of its certificate, a fresh challenge per connection, and `deviceAuth` 1.
final class LoopbackStation: @unchecked Sendable {
    let certificate: TestCertificate
    let identity: TestStationIdentity?
    private var challenges: [Data] = []
    private let queue = DispatchQueue(label: "NereusSDR.tests.loopback-station")
    private let listener: NWListener
    private let lock = NSLock()
    private var connections: [NWConnection] = []
    private var received: [String] = []
    private var receivedBinary = 0
    private var pingsReceived = 0
    private var pongsReceived = 0
    private var answersPings = true

    /// The certificate's pin, as the app holds it.
    var pin: Data { CertificatePin.sha256(ofCertificate: certificate.der) }

    init(identity: TestStationIdentity? = nil) throws {
        self.identity = identity
        certificate = try TestCertificate.make()
        let tls = NWProtocolTLS.Options()
        guard let identity = sec_identity_create(certificate.identity) else {
            throw TestCertificate.Failure(description: "could not wrap the identity")
        }
        sec_protocol_options_set_local_identity(tls.securityProtocolOptions, identity)
        sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)
        let webSocket = NWProtocolWebSocket.Options()
        // Pings from the app are recorded and answered by hand below.
        webSocket.autoReplyPing = false
        let parameters = NWParameters(tls: tls, tcp: NWProtocolTCP.Options())
        parameters.defaultProtocolStack.applicationProtocols.insert(webSocket, at: 0)
        parameters.requiredLocalEndpoint = .hostPort(host: .ipv4(.loopback), port: .any)
        listener = try NWListener(using: parameters)
    }

    /// Starts listening; returns the endpoint to dial.
    func start() async throws -> StationEndpoint {
        listener.newConnectionHandler = { [weak self] connection in
            self?.accept(connection)
        }
        let port: UInt16 = try await withCheckedThrowingContinuation { continuation in
            let once = Once()
            listener.stateUpdateHandler = { [listener] state in
                switch state {
                case .ready:
                    if once.first(), let port = listener.port?.rawValue {
                        continuation.resume(returning: port)
                    }
                case .failed(let error):
                    if once.first() {
                        continuation.resume(throwing: error)
                    }
                default:
                    break
                }
            }
            listener.start(queue: queue)
        }
        return StationEndpoint(host: "127.0.0.1", port: port)
    }

    func stop() {
        listener.cancel()
        lock.withLock { connections }.forEach { $0.cancel() }
    }

    func setAnswersPings(_ answers: Bool) {
        lock.withLock { answersPings = answers }
    }

    var receivedTexts: [String] { lock.withLock { received } }
    var receivedBinaryFrames: Int { lock.withLock { receivedBinary } }
    var pingCount: Int { lock.withLock { pingsReceived } }
    var pongCount: Int { lock.withLock { pongsReceived } }
    /// The challenge each connection's hello carried, in order.
    var sentChallenges: [Data] { lock.withLock { challenges } }

    // MARK: The connect sequence

    private func accept(_ connection: NWConnection) {
        lock.withLock { connections.append(connection) }
        connection.stateUpdateHandler = { [weak self] state in
            switch state {
            case .ready:
                guard let self else {
                    return
                }
                self.send(.hello(self.hello()), on: connection)
                self.receive(on: connection)
            default:
                break
            }
        }
        connection.start(queue: queue)
    }

    private func hello() -> LinkMessage.Hello {
        guard let identity, let claim = try? identity.claim(certificateSHA256: pin) else {
            return LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1], features: [:])
        }
        let challenge = TestStationIdentity.newChallenge()
        lock.withLock { challenges.append(Base64URL.decode(challenge) ?? Data()) }
        return LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                 features: ["deviceAuth": 1, "pairing": 1], identity: claim, challenge: challenge)
    }

    private func receive(on connection: NWConnection) {
        connection.receiveMessage { [weak self] content, context, _, error in
            guard let self, error == nil else {
                return
            }
            let metadata = context?.protocolMetadata(definition: NWProtocolWebSocket.definition)
                as? NWProtocolWebSocket.Metadata
            switch metadata?.opcode {
            case .text?:
                if let content, let text = String(data: content, encoding: .utf8) {
                    self.lock.withLock { self.received.append(text) }
                    self.answer(text, on: connection)
                }
            case .binary?:
                if content != nil { self.lock.withLock { self.receivedBinary += 1 } }
            case .ping?:
                let answer = self.lock.withLock { () -> Bool in
                    self.pingsReceived += 1
                    return self.answersPings
                }
                if answer {
                    self.sendFrame(.pong, content ?? Data(), on: connection)
                }
            case .close?:
                return
            default:
                break
            }
            self.receive(on: connection)
        }
    }

    private func answer(_ text: String, on connection: NWConnection) {
        guard case .authRequest? = try? LinkCodec.decode(text) else {
            return
        }
        send(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)), on: connection)
        send(.capabilities(LinkMessage.Capabilities(properties: [
            LinkMessage.PropertyEntry(name: "stationName", value: .utf8("Loopback")),
        ])), on: connection)
        send(.settingsSnapshot(LinkMessage.SettingsSnapshot(properties: [])), on: connection)
        send(.snapshotComplete, on: connection)
    }

    // MARK: Sending

    func send(_ message: LinkMessage, on connection: NWConnection? = nil) {
        sendText(LinkCodec.encode(message), on: connection)
    }

    /// Sends one text frame on `connection`, or on the newest connection.
    func sendText(_ text: String, on connection: NWConnection? = nil) {
        guard let target = connection ?? lock.withLock({ connections.last }) else {
            return
        }
        sendFrame(.text, Data(text.utf8), on: target)
    }

    func sendBinary(_ frame: Data) {
        guard let target = lock.withLock({ connections.last }) else { return }
        sendFrame(.binary, frame, on: target)
    }

    /// Pings the app on the newest connection; its pong is counted.
    func ping() {
        guard let connection = lock.withLock({ connections.last }) else {
            return
        }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .ping)
        metadata.setPongHandler(queue) { [weak self] error in
            if error == nil {
                self?.lock.withLock { self?.pongsReceived += 1 }
            }
        }
        let context = NWConnection.ContentContext(identifier: "ping", metadata: [metadata])
        connection.send(content: Data("nereus".utf8), contentContext: context, isComplete: true,
                        completion: .idempotent)
    }

    private func sendFrame(_ opcode: NWProtocolWebSocket.Opcode, _ content: Data, on connection: NWConnection) {
        let metadata = NWProtocolWebSocket.Metadata(opcode: opcode)
        let context = NWConnection.ContentContext(identifier: "frame", metadata: [metadata])
        connection.send(content: content, contentContext: context, isComplete: true, completion: .idempotent)
    }

    /// Lets one of several callbacks through.
    private final class Once: @unchecked Sendable {
        private let lock = NSLock()
        private var done = false

        func first() -> Bool {
            lock.withLock {
                defer { done = true }
                return !done
            }
        }
    }
}
