// NereusSDR for iOS: the app's client of the remote access service against its conformance suite and a scripted service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import Testing
@testable import NereusLink

/// R-IOS-08, R-IOS-16 (iPhone app plan Task 27a): ``RendezvousClient`` held
/// to the rendezvous conformance suite (`rendezvous/conformance/v1`, the
/// rendezvous document, section 10): its crypto vectors, the control
/// fixtures a client sends and receives, and the session fixtures marked
/// `"app"`; then the introduction's signature by known answer, the ordered
/// server list, and a code pairing carried through a mailbox.
@Suite struct RendezvousClientTests {
    /// Longer than the test's own time limit: a wait for what the manual
    /// clock sets off, which only the test's limit may end.
    static let untilItHappens: Duration = .seconds(3600)

    private static func device() throws -> DeviceIdentity {
        try DeviceIdentity.load(store: InMemoryKeyStore())
    }

    private static func vectors(_ name: String) throws -> [String: Any] {
        try RendezvousFixtures.object("crypto/\(name).json")
    }

    private static func cases(_ object: [String: Any]) throws -> [[String: Any]] {
        try #require(object["cases"] as? [[String: Any]])
    }

    // MARK: The manifest

    @Test func theManifestListsEveryFileOnce() throws {
        let entries = try RendezvousFixtures.manifest()
        let listed = entries.map(\.file)
        #expect(Set(listed).count == listed.count)
        var onDisk: [String] = []
        for folder in ["crypto", "control", "sessions"] {
            let url = RendezvousFixtures.root.appendingPathComponent(folder)
            onDisk += try FileManager.default.contentsOfDirectory(atPath: url.path)
                .filter { $0.hasSuffix(".json") }.map { "\(folder)/\($0)" }
        }
        #expect(Set(listed) == Set(onDisk))
    }

    // MARK: Crypto vectors (section 10.2)

    @Test func strictBase64URLAcceptsExactlyTheValidTexts() throws {
        for entry in try Self.cases(Self.vectors("base64url")) {
            let text = try #require(entry["text"] as? String)
            let decoded = Base64URL.decode(text)
            if entry["valid"] as? Bool == true {
                #expect(decoded == RendezvousFixtures.hex(entry["bytesHex"] as? String ?? "x"), "\(text)")
            } else {
                #expect(decoded == nil, "\(text) should be refused")
            }
        }
    }

    @Test func onlyTheCanonicalP256KeyIsAccepted() throws {
        for entry in try Self.cases(Self.vectors("p256-spki")) {
            let spki = try #require(RendezvousFixtures.hex(entry["spkiHex"] as? String ?? ""))
            #expect(P256Wire.isCanonicalKey(spki) == (entry["valid"] as? Bool), "\(entry["name"] ?? "")")
        }
    }

    @Test func theRendezvousIdIsDerivedAsTheDocumentSays() throws {
        let vectors = try Self.vectors("rendezvous-id")
        #expect(RendezvousFixtures.hex(vectors["prefixHex"] as? String ?? "") == RendezvousIdentity.idPrefix)
        let entries = try Self.cases(vectors)
        #expect(!entries.isEmpty)
        for entry in entries {
            let spki = try #require(Base64URL.decode(entry["publicKey"] as? String ?? ""))
            let digest = Data(SHA256.hash(data: RendezvousIdentity.idPrefix + spki))
            #expect(digest == RendezvousFixtures.hex(entry["digestHex"] as? String ?? ""))
            let id = RendezvousIdentity.stationId(spki: spki)
            #expect(id == entry["id"] as? String)
            #expect(RendezvousIdentity.isStationId(id))
        }
    }

    @Test func theRegistrationProofVerifiesExactlyWhenValid() throws {
        let vectors = try Self.vectors("register-proof")
        let key = try #require(vectors["key"] as? [String: Any])
        let fixed = try #require(Base64URL.decode(key["publicKey"] as? String ?? ""))
        #expect(RendezvousIdentity.stationId(spki: fixed) == key["id"] as? String)
        let nonce = try #require(Base64URL.decode(vectors["nonce"] as? String ?? ""))
        let transcript = Data("NereusSDR rendezvous register v1\n".utf8) + nonce
        #expect(transcript == RendezvousFixtures.hex(vectors["transcriptHex"] as? String ?? ""))
        for entry in try Self.cases(vectors) {
            let name = entry["name"] as? String ?? ""
            let verifies: Bool
            if let spki = Base64URL.decode(entry["publicKey"] as? String ?? ""), P256Wire.isCanonicalKey(spki),
               let signature = Base64URL.decode(entry["signature"] as? String ?? "") {
                verifies = P256Wire.verify(signature: signature, over: transcript, spki: spki)
            } else {
                verifies = false
            }
            #expect(verifies == (entry["valid"] as? Bool), "\(name)")
        }
    }

    @Test func theIntroductionSignatureVerifiesExactlyWhenValid() throws {
        let vectors = try Self.vectors("introduce-signature")
        let device = try #require(vectors["device"] as? [String: Any])
        let spki = try #require(Base64URL.decode(device["publicKey"] as? String ?? ""))
        #expect(P256Wire.deviceId(spki: spki) == device["id"] as? String)
        let stationId = try #require(vectors["stationId"] as? String)
        let nonce = try #require(Base64URL.decode(vectors["nonce"] as? String ?? ""))
        let transcript = try #require(RendezvousIdentity.introduceTranscript(stationId: stationId, nonce: nonce))
        #expect(transcript == RendezvousFixtures.hex(vectors["transcriptHex"] as? String ?? ""))
        #expect(transcript.count == 81)
        for entry in try Self.cases(vectors) {
            let signature = Base64URL.decode(entry["signature"] as? String ?? "") ?? Data()
            #expect(P256Wire.verify(signature: signature, over: transcript, spki: spki) == (entry["valid"] as? Bool),
                    "\(entry["name"] ?? "")")
        }
    }

    @Test func theRelayCredentialsAreTheDocumentsHMAC() throws {
        for entry in try Self.cases(Self.vectors("turn-credentials")) {
            let secret = try #require(entry["secret"] as? String)
            let expires = try #require((entry["expires"] as? NSNumber)?.int64Value)
            let stationId = try #require(entry["stationId"] as? String)
            let username = "\(expires):\(stationId)"
            #expect(username == entry["username"] as? String)
            #expect(RendezvousFixtures.turnPassword(secret: secret, username: username) == entry["password"] as? String)
        }
    }

    // MARK: Control fixtures (section 10.3)

    @Test func everyControlFixtureAClientSendsOrReceivesDecodesAsItSays() throws {
        var checked = 0
        for entry in try RendezvousFixtures.manifest() where entry.kind == "control" {
            let fixture = try RendezvousFixtures.object(entry.file)
            let from = try #require(fixture["from"] as? String)
            let direction: RendezvousMessage.Direction
            if from == "client" {
                direction = .toService
            } else if from == "server", fixture["to"] as? String == "client" {
                direction = .toClient
            } else {
                continue
            }
            let wire = try #require(fixture["wire"] as? [String: Any])
            let text = String(decoding: try JSONSerialization.data(withJSONObject: wire), as: UTF8.self)
            let decodes = try #require(fixture["decodes"] as? Bool)
            let decoded = try? RendezvousMessage.decode(text, direction: direction)
            #expect((decoded != nil) == decodes, "\(entry.id)")
            if let decoded, decodes {
                // Encoded again, it equals the wire on the keys its kind lists.
                let again = try #require(try JSONSerialization.jsonObject(with: Data(decoded.encoded.utf8))
                    as? [String: Any])
                #expect(Self.listed(wire, keys: Array(again.keys)) as NSDictionary == again as NSDictionary,
                        "\(entry.id)")
                if direction == .toService {
                    // What a client would send, it may send.
                    #expect(decoded.encodedForSending(.toService) != nil, "\(entry.id)")
                }
            }
            checked += 1
        }
        // Five kinds from a client and eight to one, with their refusals.
        #expect(checked >= 13)
    }

    /// `wire` kept to `keys`, and a `turn` object kept to its four.
    private static func listed(_ wire: [String: Any], keys: [String]) -> [String: Any] {
        var kept: [String: Any] = [:]
        for key in keys {
            kept[key] = wire[key]
        }
        if let turn = kept["turn"] as? [String: Any] {
            kept["turn"] = turn.filter { ["username", "password", "expires", "urls"].contains($0.key) }
        }
        return kept
    }

    @Test func aClientRefusesToSendWhatBreaksTheSendersRule() throws {
        let tooLong = String(repeating: "a", count: RendezvousMessage.maxBodyBytes + 1)
        #expect(RendezvousMessage.mailbox(body: tooLong).encodedForSending(.toService) == nil)
        #expect(RendezvousMessage.mailbox(body: "").encodedForSending(.toService) == nil)
        #expect(RendezvousMessage.candidate("a=candidate:1 1 UDP 1 192.0.2.1 1 typ host").encodedForSending(.toService) == nil)
        #expect(RendezvousMessage.mailboxOpen(nameplate: 0).encodedForSending(.toService) == nil)
        // Fits its field cap but not the message cap once escaped.
        let escaped = String(repeating: "\u{1}", count: 30_000)
        #expect(RendezvousMessage.mailbox(body: escaped).encodedForSending(.toService) == nil)
        #expect(RendezvousMessage.mailbox(body: "{\"type\":\"pair.start\"}").encodedForSending(.toService) != nil)
    }

    @Test func relayGrantWireChecksFieldsDirectionAndRedaction() throws {
        let token = "SecretRelayToken_42"
        let base = "{\"type\":\"relay.grant\",\"url\":\"wss://relay.example.test/path?q=1\",\"token\":\"\(token)\",\"expires\":4294967295}"
        guard case .relayGrant(let grant) = try RendezvousMessage.decode(base, direction: .toClient) else {
            Issue.record("relay grant did not decode")
            return
        }
        #expect(grant.expires == UInt32.max)
        #expect(grant.url.absoluteString == "wss://relay.example.test/path?q=1")
        #expect(RendezvousMessage.relayGrant(grant).encodedForSending(.toService) == nil)
        #expect(throws: RendezvousMessage.DecodeError.self) {
            try RendezvousMessage.decode(base, direction: .toService)
        }
        let extra = base.dropLast() + ",\"future\":{\"token\":\"ignored\"}}"
        #expect(try RendezvousMessage.decode(String(extra), direction: .toClient) == .relayGrant(grant))
        for changed in [
            base.replacingOccurrences(of: "4294967295", with: "4294967296"),
            base.replacingOccurrences(of: "4294967295", with: "1.0"),
            base.replacingOccurrences(of: "4294967295", with: "1e0"),
            base.replacingOccurrences(of: "4294967295", with: "true"),
            base.replacingOccurrences(of: "4294967295", with: "\"1\""),
            base.replacingOccurrences(of: "wss://", with: "https://"),
            base.replacingOccurrences(of: token, with: "padded="),
            base.replacingOccurrences(of: token, with: "bad token"),
            base.replacingOccurrences(of: "wss://relay.example.test", with: "wss://user:pass@relay.example.test"),
            base.replacingOccurrences(of: "path?q=1", with: "path#fragment"),
            base.replacingOccurrences(of: "\"url\":", with: "\"missingUrl\":")
        ] {
            #expect(throws: RendezvousMessage.DecodeError.self) {
                try RendezvousMessage.decode(changed, direction: .toClient)
            }
        }
        let longPath = String(repeating: "a", count: 512 - "wss://relay.example.test/".utf8.count)
        let longestUrl = "wss://relay.example.test/" + longPath
        #expect(try RendezvousMessage.decode("{\"type\":\"relay.grant\",\"url\":\"\(longestUrl)\",\"token\":\"T\",\"expires\":0}",
                                             direction: .toClient).kindName == "relay.grant")
        #expect(throws: RendezvousMessage.DecodeError.self) {
            try RendezvousMessage.decode("{\"type\":\"relay.grant\",\"url\":\"\(longestUrl)a\",\"token\":\"T\",\"expires\":0}",
                                         direction: .toClient)
        }
        #expect(throws: RendezvousMessage.DecodeError.self) {
            try RendezvousMessage.decode("{\"type\":\"relay.grant\",\"url\":\"wss://relay.example.test/\",\"token\":\"\(String(repeating: "T", count: 513))\",\"expires\":0}",
                                         direction: .toClient)
        }
        let message = RendezvousMessage.relayGrant(grant)
        #expect(!String(describing: grant).contains(token))
        #expect(!String(reflecting: grant).contains(token))
        #expect(!String(describing: message).contains(token))
        #expect(!String(reflecting: message).contains(token))
        #expect(!String(describing: RendezvousClient.Event.relayGrant(grant)).contains(token))
        #expect(!String(reflecting: RendezvousClient.Event.relayGrant(grant)).contains(token))
    }

    @Test func relayGrantArrivesOnceInOrderOnlyForAnAnsweredIntroduction() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let grant = try RelayGrant(urlString: "wss://relay.example.test/", token: "Token_42", expires: 123)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        connection.deliver(.relayGrant(grant)) // Before answer: unsolicited.
        let turn = RendezvousTurn(username: "user", password: "pass", expires: 123, urls: [])
        connection.deliver(.answer(sdp: "v=0", turn: turn))
        #expect(try await introduction.value.turn == turn)
        connection.deliver(.candidate("candidate:1"))
        connection.deliver(.relayGrant(grant))
        connection.deliver(.relayGrant(grant)) // Duplicate must not reach events.
        connection.deliver(.candidate("candidate:2"))
        connection.deliver(.introductionEnd(code: "expired"))
        connection.deliver(.relayGrant(grant)) // Ended introduction.
        var events = client.events.makeAsyncIterator()
        #expect(await events.next() == .candidate("candidate:1"))
        #expect(await events.next() == .relayGrant(grant))
        #expect(await events.next() == .candidate("candidate:2"))
        #expect(await events.next() == .introductionEnded(code: "expired"))
        await client.close()
    }

    @Test func answerWithoutTurnCanStillReceiveRelayGrant() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let grant = try RelayGrant(urlString: "wss://relay.example.test/", token: "Token_42", expires: 123)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        connection.deliver(.answer(sdp: "v=0", turn: nil))
        #expect(try await introduction.value.turn == nil)
        connection.deliver(.relayGrant(grant))
        connection.deliver(.candidate("candidate:1"))
        var events = client.events.makeAsyncIterator()
        #expect(await events.next() == .relayGrant(grant))
        #expect(await events.next() == .candidate("candidate:1"))
        connection.deliver(.introductionEnd(code: "expired"))
        #expect(await events.next() == .introductionEnded(code: "expired"))
        let opening = Task { try await client.openMailbox(nameplate: 123456) }
        #expect(await connection.nextMessage() == .mailboxOpen(nameplate: 123456))
        connection.deliver(.mailboxOpened(nameplate: 123456))
        _ = try await opening.value
        connection.deliver(.relayGrant(grant))
        connection.deliver(.mailbox(body: "pair.message"))
        #expect(await events.next() == .mailbox(body: "pair.message"))
        await client.close()
    }

    @Test func directAnswerWithoutGrantStillTricklesCandidates() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        connection.deliver(.answer(sdp: "v=0", turn: nil))
        #expect(try await introduction.value.turn == nil)
        connection.deliver(.candidate("candidate:1"))
        connection.deliver(.introductionEnd(code: "expired"))
        var events = client.events.makeAsyncIterator()
        #expect(await events.next() == .candidate("candidate:1"))
        #expect(await events.next() == .introductionEnded(code: "expired"))
        await client.close()
    }

    @Test func oldConnectionCannotInjectRelayGrantIntoNewIntroduction() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(servers: [.nereus, RendezvousServer(host: "backup.example.test")],
                                      clock: ManualLinkClock(), transportFactory: service.factory)
        let grant = try RelayGrant(urlString: "wss://relay.example.test/", token: "Token_42", expires: 123)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let first = try #require(await service.connection(0))
        first.greet()
        _ = await first.nextMessage()
        first.deliver(.error(.init(code: "offline", reason: "not here", retryAfterMs: 0)))
        let second = try #require(await service.connection(1))
        second.greet()
        _ = await second.nextMessage()
        let turn = RendezvousTurn(username: "user", password: "pass", expires: 123, urls: [])
        second.deliver(.answer(sdp: "v=0", turn: turn))
        _ = try await introduction.value
        first.deliver(.relayGrant(grant))
        second.deliver(.candidate("candidate:1"))
        var events = client.events.makeAsyncIterator()
        #expect(await events.next() == .candidate("candidate:1"))
        await client.close()
    }

    // MARK: Session fixtures (section 10.4)

    static let appFixtures: [String] = {
        let entries = (try? RendezvousFixtures.manifest()) ?? []
        return entries.filter { entry in
            guard entry.kind == "session", let fixture = try? RendezvousFixtures.object(entry.file),
                  let runs = fixture["runs"] as? [String] else {
                return false
            }
            return runs.contains("app")
        }.map(\.id).sorted()
    }()

    @Test func theAppRunsEveryFixtureMarkedForIt() {
        // Every session fixture that names the app runner.
        #expect(Self.appFixtures == ["introduce-answer-direct", "introduce-answer-relay", "introduce-offline",
                                     "introduce-relay-grant", "introduce-relay-grant-direct",
                                     "introduce-station-leaves", "introduction-expires", "mailbox-exchange",
                                     "mailbox-release", "mailbox-station-closes", "mailbox-station-leaves",
                                     "mailbox-unknown"])
    }

    @Test(arguments: RendezvousClientTests.appFixtures)
    func sessionFixture(_ id: String) async throws {
        let fixture = try RendezvousFixtures.object("sessions/\(id).json")
        try await RendezvousSessionRunner(id: id, fixture: fixture).run()
    }

    /// The runner is not a rubber stamp: a fixture altered in memory fails,
    /// naming the step.
    @Test func anAlteredFixtureFailsAtTheStepThatDiffers() async throws {
        var fixture = try RendezvousFixtures.object("sessions/introduce-answer-relay.json")
        var steps = try #require(fixture["steps"] as? [[String: Any]])
        let index = try #require(steps.firstIndex { ($0["message"] as? [String: Any])?["type"] as? String == "introduce" })
        var message = try #require(steps[index]["message"] as? [String: Any])
        // Signed over the station's connection's nonce instead of the client's.
        message["deviceSignature"] = "$introduce:d:a:stationNonce:signed"
        steps[index]["message"] = message
        fixture["steps"] = steps
        do {
            try await RendezvousSessionRunner(id: "altered", fixture: fixture).run()
            Issue.record("an altered fixture passed")
        } catch {
            #expect("\(error)".contains("altered step \(index + 1)"))
        }

        var mailbox = try RendezvousFixtures.object("sessions/mailbox-exchange.json")
        var mailboxSteps = try #require(mailbox["steps"] as? [[String: Any]])
        // A key the client's mailbox.close never carries.
        let close = try #require(mailboxSteps.firstIndex { $0["from"] as? String == "client"
            && ($0["message"] as? [String: Any])?["type"] as? String == "mailbox.close" })
        mailboxSteps[close]["message"] = ["type": "mailbox.close", "code": "closed"]
        mailbox["steps"] = mailboxSteps
        do {
            try await RendezvousSessionRunner(id: "altered", fixture: mailbox).run()
            Issue.record("an altered mailbox fixture passed")
        } catch {
            #expect("\(error)".contains("altered step"))
        }

        var grantFixture = try RendezvousFixtures.object("sessions/introduce-relay-grant.json")
        var grantSteps = try #require(grantFixture["steps"] as? [[String: Any]])
        let grantIndex = try #require(grantSteps.firstIndex { $0["to"] as? String == "client"
            && ($0["message"] as? [String: Any])?["type"] as? String == "relay.grant" })
        var grantMessage = try #require(grantSteps[grantIndex]["message"] as? [String: Any])
        grantMessage["token"] = "$relayToken:core:s:a"
        grantSteps[grantIndex]["message"] = grantMessage
        grantFixture["steps"] = grantSteps
        do {
            try await RendezvousSessionRunner(id: "altered-grant", fixture: grantFixture).run()
            Issue.record("an altered relay grant passed")
        } catch {
            #expect("\(error)".contains("altered-grant step \(grantIndex + 1)"))
        }
    }

    // MARK: The introduction

    @Test func theIntroductionSignatureIsTheDevicesOverExactlyTheStatedTranscript() async throws {
        let service = RendezvousTestService()
        let device = try Self.device()
        let stationKey = P256.Signing.PrivateKey().publicKey.derRepresentation
        let stationId = RendezvousIdentity.stationId(spki: stationKey)
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let introduction = Task { try await client.introduce(stationId: stationId, device: device, offer: "v=0\r\n") }
        let connection = try #require(await service.connection(0))
        #expect(connection.server == RendezvousServer.nereus)
        let nonce = connection.greet()
        guard case .introduce(let sent)? = await connection.nextMessage() else {
            Issue.record("the client did not introduce itself")
            return
        }
        #expect(sent.id == stationId)
        #expect(sent.device == device.id)
        #expect(sent.offer == "v=0\r\n")
        let signature = try #require(Base64URL.decode(sent.deviceSignature))
        // "NereusSDR introduce v1\n" (23 bytes) || the id (26) || the nonce's 32 raw bytes.
        var transcript = Data("NereusSDR introduce v1".utf8)
        transcript.append(0x0A)
        transcript.append(Data(stationId.utf8))
        transcript.append(nonce)
        #expect(transcript.count == 81)
        #expect(P256Wire.verify(signature: signature, over: transcript, spki: device.publicKey))
        // Not over the nonce's text, nor for another Core.
        var overText = Data("NereusSDR introduce v1\n".utf8) + Data(stationId.utf8)
        overText.append(Data(Base64URL.encode(nonce).utf8))
        #expect(!P256Wire.verify(signature: signature, over: overText, spki: device.publicKey))
        let otherId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let other = try #require(RendezvousIdentity.introduceTranscript(stationId: otherId, nonce: nonce))
        #expect(!P256Wire.verify(signature: signature, over: other, spki: device.publicKey))
        // No public key is sent: the Core looks the device up by its id.
        #expect(!(connection.allSent.first ?? "").contains(Base64URL.encode(device.publicKey)))
        connection.deliver(.answer(sdp: "v=0\r\nanswer", turn: nil))
        let answer = try await introduction.value
        #expect(answer.sdp == "v=0\r\nanswer")
        #expect(answer.turn == nil)
        #expect(answer.stun == RendezvousTestService.stunUrls)
        await client.close()
    }

    @Test func candidatesTrickleBothWaysAfterTheAnswer() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        let turn = RendezvousTurn(username: "1800086400:\(stationId)", password: "cGFzcw==", expires: 1_800_086_400,
                                  urls: ["turn:rv4.conformance.invalid:3478?transport=udp"])
        connection.deliver(.answer(sdp: "v=0", turn: turn))
        #expect(try await introduction.value.turn == turn)
        // An a= in front is removed; the end of candidates is the empty string.
        #expect(await client.sendCandidate("a=candidate:1 1 UDP 2122317823 192.0.2.7 50123 typ host"))
        #expect(await connection.nextMessage() == .candidate("candidate:1 1 UDP 2122317823 192.0.2.7 50123 typ host"))
        #expect(await client.sendCandidate(""))
        #expect(await connection.nextMessage() == .candidate(""))
        var events = client.events.makeAsyncIterator()
        connection.deliver(.candidate("candidate:2 1 UDP 1 198.51.100.4 3478 typ relay raddr 0.0.0.0 rport 0"))
        #expect(await events.next() == .candidate("candidate:2 1 UDP 1 198.51.100.4 3478 typ relay raddr 0.0.0.0 rport 0"))
        // A kind the app does not know is ignored, and the connection goes on.
        connection.deliver("{\"type\":\"somethingNewer\",\"x\":1}")
        connection.deliver(.introductionEnd(code: "stationLeft"))
        #expect(await events.next() == .introductionEnded(code: "stationLeft"))
        #expect(await client.sendCandidate("candidate:3 1 UDP 1 192.0.2.7 1 typ host") == false)
        await client.close()
    }

    /// Task 28a: connect() returns the STUN URLs the service's hello lists,
    /// before any request, and the introduction then goes on that same
    /// connection; with no service answering it is unreachable.
    @Test func connectGivesTheHellosStunAndTheIntroductionUsesThatConnection() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(transportFactory: service.factory)
        let connecting = Task { try await client.connect() }
        let connection = try #require(await service.connection(0))
        connection.greet()
        #expect(try await connecting.value == RendezvousTestService.stunUrls)
        // Already greeted: at once.
        #expect(try await client.connect() == RendezvousTestService.stunUrls)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        guard case .introduce(let sent)? = await connection.nextMessage() else {
            Issue.record("no introduction on the greeted connection")
            return
        }
        #expect(sent.id == stationId)
        #expect(service.connections.count == 1)
        connection.deliver(.answer(sdp: "v=0", turn: nil))
        #expect(try await introduction.value.stun == RendezvousTestService.stunUrls)

        let unreachable = RendezvousTestService()
        unreachable.refuse(RendezvousServer.nereus)
        let lonely = RendezvousClient(transportFactory: unreachable.factory)
        await #expect(throws: RendezvousError.unreachable) { try await lonely.connect() }
    }

    @Test func aSilentCoreIsNoAnswerAtTheAnswerDeadline() async throws {
        let service = RendezvousTestService()
        let clock = ManualLinkClock()
        let client = RendezvousClient(clock: clock, transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        await clock.advance(by: 29_999)
        await clock.advance(by: 1)
        await #expect(throws: RendezvousError.noAnswer) { try await introduction.value }
        // The app leaves, so the service ends the introduction.
        #expect(await connection.waitUntilClosed())
    }

    // MARK: Pairing by code through a mailbox

    /// A code made at run time, never printed: its number and its text.
    /// The number is one no other pairing in the process holds, so the
    /// client's one-pairing-per-number gate never refuses these tests.
    private static func code() -> (nameplate: Int, text: String) {
        let words = PairingCodeText.words
        let nameplate = MailboxNameplates.unshown()
        return (nameplate, "\(nameplate)-\(words.randomElement()!)-\(words.randomElement()!)")
    }

    /// The Core's next message out of the mailbox, as the link reads it.
    private static func nextPairing(_ connection: RendezvousTestService.Connection) async -> (String, LinkMessage)? {
        guard case .mailbox(let body)? = await connection.nextMessage(),
              let message = try? LinkCodec.decode(body) else {
            return nil
        }
        return (body, message)
    }

    private static func toCore(_ message: LinkMessage, _ connection: RendezvousTestService.Connection) {
        connection.deliver(.mailbox(body: LinkCodec.encode(message)))
    }

    @Test func aCodePairsThroughAMailboxCarryingThePairingMessagesUnchanged() async throws {
        let receipts = TestReceipts(caseID: #function)
        defer { receipts.flush() }
        let service = RendezvousTestService()
        let device = try Self.device()
        let core = TestStationIdentity()
        let (nameplate, code) = Self.code()
        let name = "Shack Handheld"
        let client = try PairingClient(identity: device, name: name, kind: .phone, clock: ManualLinkClock(),
                                       rendezvousTransportFactory: service.factory)
        receipts.mark("mailbox pairing Task submitting")
        let pairing = Task {
            receipts.mark("mailbox pairing Task entered")
            defer { receipts.mark("mailbox pairing Task settled") }
            return try await client.pair(code: code, via: .rendezvous(server: [.nereus], nameplate: nameplate))
        }
        let connection = try #require(await service.connection(0))
        connection.greet()
        // The number, and only the number, goes to the service.
        #expect(await connection.nextMessage() == .mailboxOpen(nameplate: nameplate))
        connection.deliver(.mailboxOpened(nameplate: nameplate))

        // No hello: pair.start in code mode is the first body.
        guard let (_, first) = await Self.nextPairing(connection), case .pairStart(let start) = first else {
            Issue.record("the first body is not pair.start")
            return
        }
        #expect(start.mode == .code)
        #expect(start.device.publicKey == Base64URL.encode(device.publicKey))
        // The service never learns the device's name: the plain pair.start
        // carries the model's word, and the name travels sealed.
        #expect(start.device.name == "iPhone")
        #expect(start.device.kind == "phone")

        receipts.mark("residual direct hash begin")
        let stored = try #require(SpakeExchange.storedData(code: code))
        receipts.mark("residual direct hash returned and required")
        let spake = SpakeExchange(role: .station)
        let step0 = try #require(spake.stationStep0(stored: stored))
        Self.toCore(.pairSpake(LinkMessage.PairSpake(step: 0, data: Base64URL.encode(step0))), connection)
        guard let (_, step1Message) = await Self.nextPairing(connection), case .pairSpake(let step1) = step1Message,
              step1.step == 1, let response1 = Base64URL.decode(step1.data) else {
            Issue.record("the app did not send step 1")
            return
        }
        let step2 = try #require(spake.stationStep2(stored: stored, response1: response1))
        Self.toCore(.pairSpake(LinkMessage.PairSpake(step: 2, data: Base64URL.encode(step2))), connection)
        guard let (_, step3Message) = await Self.nextPairing(connection), case .pairSpake(let step3) = step3Message,
              step3.step == 3, let response3 = Base64URL.decode(step3.data) else {
            Issue.record("the app did not send step 3")
            return
        }
        #expect(spake.stationStep4(response3: response3))
        guard let (_, confirmMessage) = await Self.nextPairing(connection), case .pairConfirm(let confirm) = confirmMessage,
              let box = Base64URL.decode(confirm.box), let opened = spake.open(box) else {
            Issue.record("the app did not send its pair.confirm")
            return
        }
        #expect(PairingBox.deviceContents(opened) == PairingBox.DeviceContents(publicKey: start.device.publicKey,
                                                                                name: name, kind: "phone"))
        // The Core's certificate is its own business here; its binding is
        // checked at the first sign-in.
        let claim = try core.claim(certificateSHA256: Data((0..<32).map { _ in UInt8.random(in: 0...255) }))
        let sealed = try #require(spake.seal(PairingBox.encode(PairingBox.StationContents(identity: claim,
                                                                                           label: "Rock Core"))))
        Self.toCore(.pairConfirm(LinkMessage.PairConfirm(box: Base64URL.encode(sealed))), connection)

        let paired = try await pairing.value
        #expect(paired.identityKey == core.publicKey)
        #expect(paired.label == "Rock Core")
        #expect(paired.endpoints.isEmpty)
        #expect(paired.trust == core.trust)
        // The app closes the mailbox and leaves.
        #expect(await connection.nextMessage() == .mailboxClose)
        #expect(await connection.waitUntilClosed())

        // Every body is one pair.* message, as the link writes it; the code
        // is in none of them, nor in anything else sent to the service.
        let sent = connection.allSent
        let bodies = sent.compactMap { text -> String? in
            guard case .mailbox(let body)? = try? RendezvousMessage.decode(text, direction: .toService) else {
                return nil
            }
            return body
        }
        // pair.start, steps 1 and 3, pair.confirm.
        #expect(bodies.count == 4)
        for body in bodies {
            let message = try LinkCodec.decode(body)
            #expect(message.kind.rawValue.hasPrefix("pair."))
            #expect(LinkCodec.encode(message) == body)
        }
        let words = code.split(separator: "-").dropFirst().joined(separator: "-")
        for text in sent {
            #expect(!text.contains(code))
            #expect(!text.contains(words))
            #expect(!text.contains(name))
        }
    }

    @Test func aMailboxOnAnotherNumberThanTheCodesIsNeverOpened() async throws {
        let service = RendezvousTestService()
        let (nameplate, code) = Self.code()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone,
                                       clock: ManualLinkClock(), rendezvousTransportFactory: service.factory)
        let other = nameplate == 1 ? 2 : nameplate - 1
        await #expect(throws: PairingError.notACode) {
            try await client.pair(code: code, via: .rendezvous(server: [.nereus], nameplate: other))
        }
        #expect(service.connections.isEmpty)
    }

    @Test func aNumberNoCoreShowsIsTheServicesWords() async throws {
        let service = RendezvousTestService()
        let (nameplate, code) = Self.code()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone,
                                       clock: ManualLinkClock(), rendezvousTransportFactory: service.factory)
        let pairing = Task { try await client.pair(code: code, via: .rendezvous(server: [.nereus], nameplate: nameplate)) }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        let words = "No Core is showing that pairing code right now. Check the code and try again."
        connection.deliver(.error(RendezvousMessage.ServiceError(code: "nameplateUnknown", reason: words, retryAfterMs: 0)))
        await #expect(throws: PairingError.refused(reason: words, retryAfter: .zero)) { try await pairing.value }
    }

    @Test func aMailboxOpenedOnAnotherNumberIsRefused() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let opening = Task { try await client.openMailbox(nameplate: 9) }
        let connection = try #require(await service.connection(0))
        connection.greet()
        #expect(await connection.nextMessage() == .mailboxOpen(nameplate: 9))
        connection.deliver(.mailboxOpened(nameplate: 10))
        await #expect(throws: RendezvousError.protocolViolation) { try await opening.value }
        #expect(await connection.waitUntilClosed())
        // Nothing can be sent into a mailbox that was never this one.
        #expect(await client.sendMailbox("{\"type\":\"pair.start\"}") == false)
    }

    @Test func aMailboxNotOpenedByTheDeadlineIsNoAnswerInTime() async throws {
        let service = RendezvousTestService()
        let clock = ManualLinkClock()
        let (nameplate, code) = Self.code()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone, clock: clock,
                                       rendezvousTransportFactory: service.factory)
        let pairing = Task { try await client.pair(code: code, via: .rendezvous(server: [.nereus], nameplate: nameplate)) }
        let connection = try #require(await service.connection(0))
        connection.greet()
        #expect(await connection.nextMessage() == .mailboxOpen(nameplate: nameplate))
        await clock.advance(by: 30_000)
        await #expect(throws: PairingError.ended(reason: PairingClient.noAnswerText)) { try await pairing.value }
    }

    @Test func aMailboxNotOpenedByTheDeadlineKeepsAServicesRefusal() async throws {
        let service = RendezvousTestService()
        let clock = ManualLinkClock()
        let own = RendezvousServer(host: "rv.example.net")
        let (nameplate, code) = Self.code()
        let client = try PairingClient(identity: try Self.device(), name: "Shack iPhone", kind: .phone, clock: clock,
                                       rendezvousTransportFactory: service.factory)
        let pairing = Task {
            try await client.pair(code: code, via: .rendezvous(server: [own, .nereus], nameplate: nameplate))
        }
        // The operator's own service is full: its refusal comes instead of
        // hello, then the close.
        let first = try #require(await service.connection(0))
        let busy = "The remote access service is busy. Try again shortly."
        first.deliver(.error(RendezvousMessage.ServiceError(code: "overloaded", reason: busy, retryAfterMs: 5000)))
        first.drop()
        // The next one greets, and then never opens the mailbox.
        let second = try #require(await service.connection(1))
        second.greet()
        #expect(await second.nextMessage() == .mailboxOpen(nameplate: nameplate))
        await clock.advance(by: 30_000)
        await #expect(throws: PairingError.refused(reason: busy, retryAfter: .milliseconds(5000))) {
            try await pairing.value
        }
    }

    @Test func aMailboxCarriesPairingMessagesAndNothingElse() {
        #expect(RendezvousMailboxTransport.isPairingMessage("{\"type\":\"pair.spake\",\"step\":1,\"data\":\"AA\"}"))
        #expect(!RendezvousMailboxTransport.isPairingMessage("{\"type\":\"hello\"}"))
        #expect(!RendezvousMailboxTransport.isPairingMessage("{\"type\":\"session.end\",\"reason\":\"x\"}"))
        #expect(!RendezvousMailboxTransport.isPairingMessage("pair.start"))
    }

    // MARK: Cached addresses and the attempt record

    /// A home network 192.168.1.0/24 with a global IPv6 address.
    private static let home = LocalNetworks(entries: [
        LocalNetworks.Entry(address: [192, 168, 1, 30], prefixLength: 24),
        LocalNetworks.Entry(address: [0x20, 0x01, 0x0d, 0xb8] + [UInt8](repeating: 0, count: 11) + [7],
                            prefixLength: 64),
        LocalNetworks.Entry(address: [127, 0, 0, 1], prefixLength: 8, isLoopback: true),
    ])

    @Test func withTheServiceUnreachableACachedAddressIsTriedFirstAndConnects() async throws {
        let service = RendezvousTestService()
        service.refuse(.nereus)
        let rendezvous = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let cached = StationEndpoint(host: "shack.example.net")
        let station = PairedStation(identityKey: TestStationIdentity().publicKey, label: "Rock",
                                    endpoints: [cached])
        let attempt = await ConnectionAttempt.reach(endpoints: station.endpoints, networks: Self.home,
                                                    dial: { $0 == cached ? .connected : .noAnswer },
                                                    throughService: {
            do {
                _ = try await rendezvous.introduce(stationId: station.rendezvousId, device: try Self.device(),
                                                   offer: "v=0")
                return (.relay, "the remote access service", .connected)
            } catch {
                return (.relay, "the remote access service", .failed)
            }
        })
        #expect(attempt.connected)
        #expect(attempt.tries == [ConnectionAttempt.Try(path: .direct, address: "shack.example.net:47910",
                                                        outcome: .connected)])
        // The service was never asked.
        #expect(service.connections.isEmpty)
        await rendezvous.close()
    }

    @Test func theAttemptRecordListsEachPathInTheOrderTried() async throws {
        let service = RendezvousTestService()
        service.refuse(.nereus)
        let rendezvous = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let lan = StationEndpoint(host: "192.168.1.20")
        let away = StationEndpoint(host: "2001:db8:ffff::20", port: 50055)
        let moved = StationEndpoint(host: "203.0.113.5")
        let outcomes: [StationEndpoint: ConnectionAttempt.Outcome] = [lan: .noAnswer, away: .timedOut,
                                                                     moved: .notThisCore]
        let device = try Self.device()
        let stationId = RendezvousIdentity.stationId(spki: TestStationIdentity().publicKey)
        let attempt = await ConnectionAttempt.reach(endpoints: [lan, away, moved], networks: Self.home,
                                                    dial: { outcomes[$0] ?? .failed },
                                                    throughService: {
            do {
                _ = try await rendezvous.introduce(stationId: stationId, device: device, offer: "v=0")
                return (.relay, "the remote access service", .connected)
            } catch {
                return (.relay, "the remote access service", .noAnswer)
            }
        })
        #expect(!attempt.connected)
        #expect(attempt.tries.map(\.path) == [.thisNetwork, .direct, .direct, .relay])
        #expect(attempt.tries.map(\.outcome) == [.noAnswer, .timedOut, .notThisCore, .noAnswer])
        #expect(attempt.summary == "Tried this network (192.168.1.20:47910): no answer; "
                + "direct ([2001:db8:ffff::20]:50055): no answer in time; "
                + "direct (203.0.113.5:47910): another computer answered; "
                + "relay (the remote access service): no answer.")
        await rendezvous.close()
    }

    @Test func pathsFollowThisPhonesNetworks() {
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "192.168.1.200"), networks: Self.home) == .thisNetwork)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "192.168.2.200"), networks: Self.home) == .direct)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "2001:db8::99"), networks: Self.home) == .thisNetwork)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "[2001:db8:1::99]"), networks: Self.home) == .direct)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "::ffff:192.168.1.9"), networks: Self.home) == .thisNetwork)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "Rock.local."), networks: Self.home) == .thisNetwork)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "127.0.0.1"), networks: LocalNetworks(entries: []))
                == .thisNetwork)
        #expect(ConnectionAttempt.path(for: StationEndpoint(host: "shack.example.net"), networks: Self.home) == .direct)
        #expect(Self.home.usableFamilies == .both)
        #expect(LocalNetworks(entries: [LocalNetworks.Entry(address: [169, 254, 3, 3], prefixLength: 16)])
            .usableFamilies == .none)
    }

    @Test func theLastGoodAddressGoesFirstAndIsKept() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let key = TestStationIdentity().publicKey
        let first = StationEndpoint(host: "192.168.1.20")
        try store.save(PairedStation(identityKey: key, label: "Rock", endpoints: [first]))
        let reached = StationEndpoint(host: "2001:db8::20")
        var updated = try #require(try store.recordReached(identityKey: key, at: reached, by: .direct))
        #expect(updated.endpoints == [reached, first])
        #expect(updated.lastPath == "direct")
        #expect(try store.station(identityKey: key)?.endpoints == [reached, first])
        // The same Core spelled another way is one address, and at most four are kept.
        for host in ["a.example", "b.example", "c.example", "A.EXAMPLE."] {
            updated = try #require(try store.recordReached(identityKey: key, at: StationEndpoint(host: host), by: .direct))
        }
        #expect(updated.endpoints.map(\.host) == ["A.EXAMPLE.", "c.example", "b.example", "2001:db8::20"])
        #expect(try store.recordReached(identityKey: TestStationIdentity().publicKey, at: first, by: .thisNetwork) == nil)
        #expect(updated.rendezvousId == RendezvousIdentity.stationId(spki: key))
    }

    // MARK: The ordered server list

    @Test func aServiceThatDoesNotAnswerIsPassedForTheNext() async throws {
        let service = RendezvousTestService()
        let own = RendezvousServer(host: "rv.example.net")
        service.refuse(own)
        let client = RendezvousClient(servers: [own, .nereus], clock: ManualLinkClock(), transportFactory: service.factory)
        let opening = Task { try await client.openMailbox(nameplate: 42) }
        let second = try #require(await service.connection(1))
        #expect(second.server == .nereus)
        second.greet()
        #expect(await second.nextMessage() == .mailboxOpen(nameplate: 42))
        second.deliver(.mailboxOpened(nameplate: 42))
        #expect(try await opening.value == .nereus)
        await client.close()
    }

    @Test func aServiceThatSaysTheCoreIsOfflineSendsTheIntroductionOnToTheNext() async throws {
        let service = RendezvousTestService()
        let own = RendezvousServer(host: "rv.example.net", port: 8443)
        let client = RendezvousClient(servers: [own, .nereus], clock: ManualLinkClock(), transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let device = try Self.device()
        let introduction = Task { try await client.introduce(stationId: stationId, device: device, offer: "v=0") }
        let first = try #require(await service.connection(0))
        #expect(first.server == own)
        first.greet()
        _ = await first.nextMessage()
        first.deliver(.error(RendezvousMessage.ServiceError(
            code: "offline",
            reason: "The Core is not reachable right now. Check that it is running and connected to the internet.",
            retryAfterMs: 0)))
        let second = try #require(await service.connection(1))
        #expect(await first.waitUntilClosed())
        let nonce = second.greet()
        guard case .introduce(let sent)? = await second.nextMessage() else {
            Issue.record("the introduction did not reach the second service")
            return
        }
        // Signed again over the second connection's own nonce.
        let transcript = try #require(RendezvousIdentity.introduceTranscript(stationId: stationId, nonce: nonce))
        let signature = try #require(Base64URL.decode(sent.deviceSignature))
        #expect(P256Wire.verify(signature: signature, over: transcript, spki: device.publicKey))
        second.deliver(.answer(sdp: "v=0 answer", turn: nil))
        let answer = try await introduction.value
        #expect(answer.server == .nereus)
        await client.close()
    }

    @Test func whenEveryServiceSaysOfflineTheLastWordsAreKept() async throws {
        let service = RendezvousTestService()
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        let stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        let introduction = Task { try await client.introduce(stationId: stationId, device: try Self.device(), offer: "v=0") }
        let connection = try #require(await service.connection(0))
        connection.greet()
        _ = await connection.nextMessage()
        let words = "The Core is not reachable right now. Check that it is running and connected to the internet."
        connection.deliver(.error(RendezvousMessage.ServiceError(code: "offline", reason: words, retryAfterMs: 0)))
        await #expect(throws: RendezvousError.offline(reason: words)) { try await introduction.value }
    }

    @Test func noServiceAnsweringIsUnreachable() async throws {
        let service = RendezvousTestService()
        service.refuse(.nereus)
        let client = RendezvousClient(clock: ManualLinkClock(), transportFactory: service.factory)
        await #expect(throws: RendezvousError.unreachable) { try await client.openMailbox(nameplate: 5) }
    }

    /// Driven on the manual clock alone (a load finding, Task 56): what the
    /// clock sets off (the hello timer's hop, the next service's opening,
    /// the first one's close) is awaited with no wall-clock bound of its
    /// own, so a starved machine only makes the test slower; the test's
    /// time limit stops a real hang.
    @Test(.timeLimit(.minutes(5)))
    func aServiceThatNeverGreetsIsPassedAtTheHelloTimeout() async throws {
        let service = RendezvousTestService()
        let clock = ManualLinkClock()
        let own = RendezvousServer(host: "rv.example.net")
        let client = RendezvousClient(servers: [own, .nereus], clock: clock, transportFactory: service.factory)
        let opening = Task { try await client.openMailbox(nameplate: 9) }
        let first = try #require(await service.connection(0, within: Self.untilItHappens))
        await clock.advance(by: 9_999)
        #expect(service.connections.count == 1)
        await clock.advance(by: 1)
        let second = try #require(await service.connection(1, within: Self.untilItHappens))
        #expect(await first.waitUntilClosed(within: Self.untilItHappens))
        second.greet()
        _ = await second.nextMessage(within: Self.untilItHappens)
        second.deliver(.error(RendezvousMessage.ServiceError(code: "rateLimited",
                                                             reason: "Too many attempts. Try again in a minute.",
                                                             retryAfterMs: 1234)))
        await #expect(throws: RendezvousError.refused(code: "rateLimited",
                                                     reason: "Too many attempts. Try again in a minute.",
                                                     retryAfter: .milliseconds(1234))) {
            try await opening.value
        }
    }
}
