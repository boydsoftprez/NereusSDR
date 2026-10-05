// NereusSDR for iOS: the app's runner of the rendezvous session fixtures, playing the service towards its client
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
@testable import NereusLink

/// Runs one session fixture marked `"app"` (the rendezvous document,
/// section 10.4): it plays the service towards a ``RendezvousClient`` on
/// the connection named `client`, drives the client to each of its
/// behaviour messages and matches what it sends, and fills the messages of
/// every other connection only to record their placeholders. Station keys
/// and the relay secret are made at run time; the client's device key is
/// its own, made at run time too.
final class RendezvousSessionRunner {
    typealias Malformed = LinkFixtureLoader.Malformed

    private struct RelaySession {
        let bytes: Data
        let station: String
        let expires: UInt32
    }

    /// What the client did with one request.
    private enum Outcome {
        case answered(RendezvousClient.Answer)
        case opened
        case failed(Error)
    }

    /// The client's events, gathered as they come.
    private final class EventBox: @unchecked Sendable {
        private let lock = NSLock()
        private var events: [RendezvousClient.Event] = []
        let waiters = ConditionWaiters()

        func put(_ event: RendezvousClient.Event) {
            lock.withLock { events.append(event) }
            waiters.release()
        }

        func next(within timeout: Duration = .seconds(10)) async -> RendezvousClient.Event? {
            _ = await waiters.wait(within: timeout) { [self] in lock.withLock { !events.isEmpty } }
            return lock.withLock { events.isEmpty ? nil : events.removeFirst() }
        }
    }

    private let id: String
    private let steps: [[String: Any]]
    private let turnUrls: [String]
    private let turnTtlSeconds: Int64
    private let relayUrl: String
    private let relayTtlSeconds: Int64
    private let wallClock: Int64
    private let offerText: String
    private let answerText: String

    private let service = RendezvousTestService()
    private let clock = ManualLinkClock()
    private let device: DeviceIdentity
    private let turnSecret = UUID().uuidString
    /// Fixture-only secret. The client treats grants as opaque; only this
    /// service stand-in mints and checks their section 12.2 contents.
    private let relaySecret = Data("NereusSDR app conformance relay secret v1".utf8)
    private var stationKeys: [String: P256.Signing.PrivateKey] = [:]
    private var relaySessions: [String: RelaySession] = [:]
    private var activeStationName: String?
    private var clientGrantSessionName: String?
    private var record: [String: Any] = [:]
    /// The value each signature placeholder took, for later messages that name it.
    private var signatures: [String: String] = [:]
    private var advancedMs: Int64 = 0

    private var client: RendezvousClient?
    private var connection: RendezvousTestService.Connection?
    private var request: Task<Outcome, Never>?
    private var requestSettled = false
    private let events = EventBox()

    init(id: String, fixture: [String: Any]) throws {
        self.id = id
        guard let runs = fixture["runs"] as? [String], runs.contains("app"),
              let steps = fixture["steps"] as? [[String: Any]] else {
            throw Malformed(description: "\(id): not an app fixture with steps")
        }
        let allowed: Set<String> = ["runs", "serverSetup", "pairedDevices", "steps"]
        if let key = Set(fixture.keys).subtracting(allowed).sorted().first {
            throw Malformed(description: "\(id): unknown key \(key)")
        }
        self.steps = steps
        let setup = fixture["serverSetup"] as? [String: Any] ?? [:]
        turnUrls = setup["turnUrls"] as? [String]
            ?? ["turn:rv4.conformance.invalid:3478?transport=udp", "turn:rv6.conformance.invalid:3478?transport=udp"]
        turnTtlSeconds = (setup["turnTtlSeconds"] as? NSNumber)?.int64Value ?? 86_400
        relayUrl = setup["relayUrl"] as? String ?? "wss://rv.conformance.invalid/v1/relay"
        relayTtlSeconds = (setup["relayTtlSeconds"] as? NSNumber)?.int64Value ?? 120
        wallClock = (setup["wallClock"] as? NSNumber)?.int64Value ?? 1_800_000_000
        offerText = try RendezvousFixtures.text("sdp/offer.sdp")
        answerText = try RendezvousFixtures.text("sdp/answer.sdp")
        device = try DeviceIdentity.load(store: InMemoryKeyStore())
    }

    /// Plays every step; throws at the first that does not hold, naming it.
    func run() async throws {
        for (index, step) in steps.enumerated() {
            let label = "\(id) step \(index + 1)"
            try await play(step, index: index, label: label)
        }
        if let client {
            await client.close()
        }
    }

    // MARK: Steps

    private func play(_ step: [String: Any], index: Int, label: String) async throws {
        if let name = step["connect"] as? String {
            if name == "client" {
                try await startClient(before: index, label: label)
            }
        } else if let from = step["from"] as? String, from == "server" {
            guard let to = step["to"] as? String, let message = step["message"] as? [String: Any] else {
                throw Malformed(description: "\(label): a server step needs to and message")
            }
            let filled = try fill(message, label: label)
            if to == "client" {
                try await deliver(filled, fixtureMessage: message, label: label)
            }
        } else if let from = step["from"] as? String {
            guard let role = step["role"] as? String, let message = step["message"] as? [String: Any] else {
                throw Malformed(description: "\(label): a sending step needs role and message")
            }
            if from == "client" {
                if role == "behaviour" {
                    try await behave(message, label: label)
                }
            } else {
                _ = try fill(message, label: label)
            }
        } else if let ms = step["advanceMs"] as? NSNumber {
            advancedMs += ms.int64Value
            await clock.advance(by: ms.int64Value)
        } else if let name = step["disconnect"] as? String {
            if name == "client" {
                await client?.close()
            }
        } else if let name = step["expectClosed"] as? String {
            if name == "client" {
                connection?.drop()
                try await expectSilence(label: label)
            }
        } else if step["expectSilent"] is String {
            try await expectSilence(label: label)
        } else {
            throw Malformed(description: "\(label): a step of no known kind")
        }
    }

    /// Starts the client on what its next behaviour step asks for.
    private func startClient(before index: Int, label: String) async throws {
        guard let next = steps[(index + 1)...].first(where: {
            $0["from"] as? String == "client" && $0["role"] as? String == "behaviour"
        }), let message = next["message"] as? [String: Any], let type = message["type"] as? String else {
            throw Malformed(description: "\(label): the client is never asked to do anything")
        }
        let client = RendezvousClient(servers: [RendezvousServer(host: "rv.conformance.invalid")], clock: clock,
                                      transportFactory: service.factory)
        self.client = client
        let box = events
        Task {
            for await event in client.events {
                box.put(event)
            }
        }
        requestSettled = false
        switch type {
        case "introduce":
            guard let placeholder = message["id"] as? String, let key = Self.keyName(placeholder) else {
                throw Malformed(description: "\(label): introduce names no station key")
            }
            let stationId = RendezvousIdentity.stationId(spki: stationKey(key).publicKey.derRepresentation)
            activeStationName = key
            let device = device
            let offer = offerText
            request = Task {
                do {
                    return .answered(try await client.introduce(stationId: stationId, device: device, offer: offer))
                } catch {
                    return .failed(error)
                }
            }
        case "mailbox.open":
            guard let nameplate = (message["nameplate"] as? NSNumber)?.intValue else {
                throw Malformed(description: "\(label): mailbox.open without a nameplate")
            }
            request = Task {
                do {
                    _ = try await client.openMailbox(nameplate: nameplate)
                    return .opened
                } catch {
                    return .failed(error)
                }
            }
        default:
            throw Malformed(description: "\(label): the client cannot be asked to start with \(type)")
        }
        guard let connection = await service.connection(0) else {
            throw Malformed(description: "\(label): the client never dialled")
        }
        self.connection = connection
    }

    /// Drives the client to a message the fixture says it sends, and matches it.
    private func behave(_ message: [String: Any], label: String) async throws {
        guard let client, let connection, let type = message["type"] as? String else {
            throw Malformed(description: "\(label): the client sends before it connected")
        }
        switch type {
        case "introduce", "mailbox.open":
            break
        case "mailbox":
            guard let body = message["body"] as? String else {
                throw Malformed(description: "\(label): mailbox without a body")
            }
            let sent = await client.sendMailbox(body)
            try check(sent, "\(label): the client did not send its mailbox body")
        case "mailbox.close":
            await client.closeMailbox()
        case "candidate":
            guard let candidate = try fill(message, label: label)["candidate"] as? String else {
                throw Malformed(description: "\(label): candidate without a value")
            }
            let sent = await client.sendCandidate(candidate)
            try check(sent, "\(label): the client did not send its candidate")
        default:
            throw Malformed(description: "\(label): the client cannot be asked to send \(type)")
        }
        guard let text = await connection.nextSent() else {
            throw Malformed(description: "\(label): the client sent nothing")
        }
        try check(RendezvousMessage.decode(text, direction: .toService).encodedForSending(.toService) != nil,
                  "\(label): the client sent a message the service would refuse")
        guard let sent = try JSONSerialization.jsonObject(with: Data(text.utf8)) as? [String: Any] else {
            throw Malformed(description: "\(label): the client sent something other than an object")
        }
        try match(message, sent, at: label)
    }

    /// Gives the client a message from the service, and checks what it made of it.
    private func deliver(_ message: [String: Any], fixtureMessage: [String: Any], label: String) async throws {
        guard let connection, let type = message["type"] as? String else {
            throw Malformed(description: "\(label): a message to a client that never connected")
        }
        let text = String(decoding: try JSONSerialization.data(withJSONObject: message), as: UTF8.self)
        connection.deliver(text)
        switch type {
        case "hello":
            return
        case "answer":
            guard case .answered(let answer) = await settle(label) else {
                throw Malformed(description: "\(label): the client did not take the answer")
            }
            try check(answer.sdp == message["answer"] as? String, "\(label): the answer's SDP changed")
            try check(Self.turnObject(answer.turn) == (message["turn"] as? NSObject ?? NSNull()),
                      "\(label): the relay credentials changed")
            try check(answer.stun == RendezvousTestService.stunUrls, "\(label): the hello's STUN list was not kept")
        case "mailbox.opened":
            guard case .opened = await settle(label) else {
                throw Malformed(description: "\(label): the client did not take mailbox.opened")
            }
        case "relay.grant":
            guard let parts = (fixtureMessage["token"] as? String)?.split(separator: ":").map(String.init),
                  parts.count == 4,
                  let token = message["token"] as? String,
                  let url = message["url"] as? String,
                  let expires = (message["expires"] as? NSNumber)?.int64Value,
                  let station = activeStationName,
                  parts[0] == "$relayToken", parts[1] == "device", parts[3] == station,
                  url == relayUrl,
                  expires == wallClock + advancedMs / 1000 + relayTtlSeconds,
                  token == (try relayToken(leg: "device", session: parts[2], station: station, label: label)) else {
                throw Malformed(description: "\(label): the client's relay grant is not bound to this introduction")
            }
            clientGrantSessionName = parts[2]
            let expectedGrant = try RelayGrant(urlString: url, token: token, expires: UInt32(expires))
            try check(await events.next() == .relayGrant(expectedGrant),
                      "\(label): the client did not take its exact relay grant")
        case "error" where !requestSettled:
            guard case .failed(let error) = await settle(label) else {
                throw Malformed(description: "\(label): the client took an error as success")
            }
            let code = message["code"] as? String ?? ""
            let reason = message["reason"] as? String ?? ""
            let expected: RendezvousError
            switch code {
            case "offline":
                expected = .offline(reason: reason)
            case "nameplateUnknown":
                expected = .nameplateUnknown(reason: reason)
            default:
                expected = .refused(code: code, reason: reason,
                                    retryAfter: .milliseconds((message["retryAfterMs"] as? NSNumber)?.int64Value ?? 0))
            }
            try check(error as? RendezvousError == expected, "\(label): the client's error is \(error)")
        default:
            let event = await events.next()
            let expected: RendezvousClient.Event?
            switch type {
            case "candidate":
                expected = .candidate(message["candidate"] as? String ?? "")
            case "introduction.end":
                expected = .introductionEnded(code: message["code"] as? String ?? "")
            case "mailbox":
                expected = .mailbox(body: message["body"] as? String ?? "")
            case "mailbox.closed":
                expected = .mailboxClosed(code: message["code"] as? String ?? "")
            case "error":
                expected = .serviceError(RendezvousMessage.ServiceError(
                    code: message["code"] as? String ?? "", reason: message["reason"] as? String ?? "",
                    retryAfterMs: (message["retryAfterMs"] as? NSNumber)?.int64Value ?? 0))
            default:
                throw Malformed(description: "\(label): a message of kind \(type) to a client")
            }
            try check(event == expected, "\(label): the client reported \(String(describing: event))")
        }
    }

    private func settle(_ label: String) async -> Outcome {
        requestSettled = true
        guard let request else {
            return .failed(RendezvousError.connectionLost)
        }
        return await request.value
    }

    /// Nothing from the client within 1 s of real time (section 10.4).
    private func expectSilence(label: String) async throws {
        if let connection, let text = await connection.nextSent(within: .seconds(1)) {
            throw Malformed(description: "\(label): the client sent \(text.prefix(40)) when silence was expected")
        }
    }

    // MARK: Placeholders

    private func stationKey(_ name: String) -> P256.Signing.PrivateKey {
        if let key = stationKeys[name] {
            return key
        }
        let key = P256.Signing.PrivateKey()
        stationKeys[name] = key
        return key
    }

    /// `a` of `"$key:a:id"`.
    private static func keyName(_ placeholder: String) -> String? {
        let parts = placeholder.split(separator: ":")
        return parts.count == 3 && parts[0] == "$key" ? String(parts[1]) : nil
    }

    private func fill(_ message: [String: Any], label: String) throws -> [String: Any] {
        guard let filled = try fillValue(message, label: label) as? [String: Any] else {
            throw Malformed(description: "\(label): a message that is not an object")
        }
        return filled
    }

    private func fillValue(_ value: Any, label: String) throws -> Any {
        if let object = value as? [String: Any] {
            var out: [String: Any] = [:]
            for (key, element) in object {
                out[key] = try fillValue(element, label: label)
            }
            return out
        }
        if let array = value as? [Any] {
            return try array.map { try fillValue($0, label: label) }
        }
        guard let text = value as? String, text.hasPrefix("$") else {
            return value
        }
        let parts = text.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        switch parts[0] {
        case "$b64":
            guard parts.count == 3, let count = Int(parts[1]) else {
                throw Malformed(description: "\(label): \(text)")
            }
            let filled = Base64URL.encode(Data((0..<count).map { _ in UInt8.random(in: 0...255) }))
            record[parts[2]] = filled
            return filled
        case "$key":
            let key = stationKey(parts[1]).publicKey.derRepresentation
            return parts[2] == "id" ? RendezvousIdentity.stationId(spki: key) : Base64URL.encode(key)
        case "$device":
            return device.id
        case "$ref":
            guard let recorded = record[parts[1]] else {
                throw Malformed(description: "\(label): nothing recorded as \(parts[1])")
            }
            return recorded
        case "$sdp":
            let sdp = parts[1] == "offer" ? offerText : answerText
            record[parts[2]] = sdp
            return sdp
        case "$candidate":
            let candidate = "candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host"
            record[parts[1]] = candidate
            return candidate
        case "$turn":
            let expires = wallClock + advancedMs / 1000 + turnTtlSeconds
            let stationId = RendezvousIdentity.stationId(spki: stationKey(parts[1]).publicKey.derRepresentation)
            let username = "\(expires):\(stationId)"
            let turn: [String: Any] = [
                "username": username,
                "password": RendezvousFixtures.turnPassword(secret: turnSecret, username: username),
                "expires": NSNumber(value: expires),
                "urls": turnUrls,
            ]
            record[parts[2]] = turn
            return turn
        case "$relayToken":
            guard parts.count == 4 else {
                throw Malformed(description: "\(label): malformed relay token placeholder \(text)")
            }
            if parts[1] == "core", let clientGrantSessionName,
               (parts[2] != clientGrantSessionName || parts[3] != activeStationName) {
                throw Malformed(description: "\(label): the Core grant names another introduction")
            }
            return try relayToken(leg: parts[1], session: parts[2], station: parts[3], label: label)
        case "$introduce", "$register":
            if let known = signatures[text] {
                return known
            }
            guard parts.count >= 4, let nonce = (record[parts[parts.count - 2]] as? String).flatMap(Base64URL.decode) else {
                throw Malformed(description: "\(label): \(text) names no recorded nonce")
            }
            let signed: Data
            if parts[0] == "$register" {
                signed = try stationKey(parts[1]).signature(
                    for: Data("NereusSDR rendezvous register v1\n".utf8) + nonce).rawRepresentation
            } else {
                let stationId = RendezvousIdentity.stationId(spki: stationKey(parts[2]).publicKey.derRepresentation)
                signed = try RendezvousIdentity.introduceSignature(device: device, stationId: stationId, nonce: nonce)
            }
            let filled = Base64URL.encode(signed)
            signatures[text] = filled
            return filled
        case "$string", "$any":
            return ""
        case "$int":
            return 0
        default:
            throw Malformed(description: "\(label): a placeholder this runner does not know: \(text)")
        }
    }

    /// Matches what the client sent against the fixture's message.
    private func match(_ expected: Any, _ actual: Any, at label: String) throws {
        if let object = expected as? [String: Any] {
            guard let sent = actual as? [String: Any], Set(sent.keys) == Set(object.keys) else {
                throw Malformed(description: "\(label): the client's keys differ")
            }
            for (key, element) in object {
                try match(element, sent[key] as Any, at: "\(label).\(key)")
            }
            return
        }
        guard let text = expected as? String, text.hasPrefix("$") else {
            try check((expected as? NSObject)?.isEqual(actual) == true, "\(label): the client sent another value")
            return
        }
        guard let sent = actual as? String else {
            throw Malformed(description: "\(label): the client sent a non-string for \(text)")
        }
        let parts = text.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        switch parts[0] {
        case "$key", "$device", "$ref":
            try check(sent == (try fillValue(text, label: label) as? String), "\(label): not \(text)")
        case "$introduce":
            guard parts.count == 5, parts[4] == "signed",
                  let nonce = (record[parts[3]] as? String).flatMap(Base64URL.decode),
                  let transcript = RendezvousIdentity.introduceTranscript(
                    stationId: RendezvousIdentity.stationId(spki: stationKey(parts[2]).publicKey.derRepresentation),
                    nonce: nonce),
                  let signature = Base64URL.decode(sent) else {
                throw Malformed(description: "\(label): \(text) cannot be checked")
            }
            try check(P256Wire.verify(signature: signature, over: transcript, spki: device.publicKey),
                      "\(label): the device's signature does not verify over this connection's transcript")
            signatures[text] = sent
        case "$sdp", "$string":
            try check(!sent.isEmpty || parts[0] == "$string", "\(label): an empty SDP")
            if parts.count == 3 {
                record[parts[2]] = sent
            }
        case "$b64":
            try check(Base64URL.decode(sent)?.count == Int(parts[1]), "\(label): not \(parts[1]) bytes")
            record[parts[2]] = sent
        case "$candidate":
            try check(sent.hasPrefix("candidate:"), "\(label): not a candidate")
            record[parts[1]] = sent
        case "$any":
            return
        default:
            throw Malformed(description: "\(label): a placeholder this runner cannot match: \(text)")
        }
    }

    private func check(_ condition: Bool, _ description: @autoclosure () -> String) throws {
        if !condition {
            throw Malformed(description: description())
        }
    }

    private func relayToken(leg: String, session name: String, station: String, label: String) throws -> String {
        let legByte: UInt8
        switch leg {
        case "core": legByte = 1
        case "device": legByte = 2
        default: throw Malformed(description: "\(label): unknown relay leg \(leg)")
        }
        let session: RelaySession
        if let known = relaySessions[name] {
            guard known.station == station else {
                throw Malformed(description: "\(label): one relay session names two stations")
            }
            session = known
        } else {
            // Section 10.4 gives each name a fresh 16-byte session. Keep it
            // for the other leg, and never reuse another name's session.
            let expiry = wallClock + advancedMs / 1000 + relayTtlSeconds
            guard let expiry32 = UInt32(exactly: expiry) else {
                throw Malformed(description: "\(label): relay expiry is outside the token range")
            }
            var bytes: Data
            repeat {
                bytes = Data((0..<16).map { _ in UInt8.random(in: 0...255) })
            } while relaySessions.values.contains(where: { $0.bytes == bytes })
            session = RelaySession(
                bytes: bytes,
                station: station, expires: expiry32)
            relaySessions[name] = session
        }
        let stationId = RendezvousIdentity.stationId(spki: stationKey(station).publicKey.derRepresentation)
        let key = SymmetricKey(data: relaySecret)
        let stationCode = HMAC<SHA256>.authenticationCode(
            for: Data("NereusSDR relay station v1\n".utf8) + Data(stationId.utf8), using: key)
        var payload = Data([1, legByte]) + session.bytes + Data(stationCode.prefix(8))
        var bigEndianExpiry = session.expires.bigEndian
        withUnsafeBytes(of: &bigEndianExpiry) { payload.append(contentsOf: $0) }
        let mac = HMAC<SHA256>.authenticationCode(
            for: Data("NereusSDR relay grant v1\n".utf8) + payload, using: key)
        return Base64URL.encode(payload + Data(mac))
    }

    /// The relay credentials as the JSON object they came in, for comparison.
    private static func turnObject(_ turn: RendezvousTurn?) -> NSObject {
        guard let turn else {
            return NSNull()
        }
        return [
            "username": turn.username, "password": turn.password,
            "expires": NSNumber(value: turn.expires), "urls": turn.urls,
        ] as NSDictionary
    }
}
