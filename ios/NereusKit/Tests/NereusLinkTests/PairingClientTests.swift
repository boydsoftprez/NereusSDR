// NereusSDR for iOS: pairing by one tap and by code against a Core the test plays, with every refusal
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-08, R-IOS-16: ``PairingClient`` against a Core played by hand
/// (link document section 3.6), the code made at run time and never printed.
@Suite struct PairingClientTests {
    private static let endpoint = StationEndpoint(host: "shack.example.net")

    /// The Core's side of one pairing: its identity, its hello and its code.
    private struct Core {
        let transport = PairingTestTransport()
        let identity = TestStationIdentity()
        let clock = ManualLinkClock()
        let code: String
        /// Each Core its own, as pairing runs one at a time per Core.
        let endpoint = StationEndpoint(host: "core-\(UUID().uuidString.lowercased()).example.net")

        init() {
            let words = PairingCodeText.words
            code = "\(Int.random(in: 1...99))-\(words.randomElement()!)-\(words.randomElement()!)"
        }

        func hello(features: [String: Int] = ["deviceAuth": 1, "pairing": 1],
                   identity claim: LinkMessage.StationIdentityClaim? = nil) throws -> LinkMessage {
            .hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                     features: features,
                                     identity: try claim ?? identity.claim(certificateSHA256: transport.certificateSHA256),
                                     challenge: TestStationIdentity.newChallenge()))
        }

        var claim: LinkMessage.StationIdentityClaim {
            get throws { try identity.claim(certificateSHA256: transport.certificateSHA256) }
        }

        func client(device: DeviceIdentity, name: String = "Shack iPhone") throws -> PairingClient {
            try PairingClient(identity: device, name: name, kind: .phone, clock: clock,
                              transportFactory: transport.factory)
        }
    }

    private static func device() throws -> DeviceIdentity {
        try DeviceIdentity.load(store: InMemoryKeyStore())
    }

    /// The error `task` ended with, or nil when it succeeded.
    private static func failure(_ task: Task<PairedStation, Error>) async -> PairingError? {
        do {
            _ = try await task.value
            return nil
        } catch {
            return error as? PairingError
        }
    }

    /// Takes the app's hello and pair.start, checking both.
    private static func takeOpening(_ core: Core, mode: LinkMessage.PairStart.Mode,
                                    device: DeviceIdentity) async throws -> LinkMessage.PairStart {
        let hello = await core.transport.nextSent()
        guard case .hello(let own)? = hello else {
            Issue.record("the app's first message is not its hello")
            throw PairingError.ended(reason: nil)
        }
        #expect(own.features == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                 "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
        guard case .pairStart(let start)? = await core.transport.nextSent() else {
            Issue.record("the app's second message is not pair.start")
            throw PairingError.ended(reason: nil)
        }
        #expect(start.mode == mode)
        #expect(start.device.publicKey == Base64URL.encode(device.publicKey))
        #expect(start.device.name == "Shack iPhone")
        #expect(start.device.kind == "phone")
        return start
    }

    /// Plays the Core's code exchange up to step 2 with `stationCode`;
    /// returns the Core's exchange and what the app sent after step 2.
    private static func playToStep2(_ core: Core, stationCode: String) async throws
        -> (SpakeExchange, LinkMessage?) {
        let stored = try #require(SpakeExchange.storedData(code: stationCode))
        let spake = SpakeExchange(role: .station)
        let step0 = try #require(spake.stationStep0(stored: stored))
        core.transport.deliver(.pairSpake(LinkMessage.PairSpake(step: 0, data: Base64URL.encode(step0))))
        guard case .pairSpake(let step1)? = await core.transport.nextSent(), step1.step == 1,
              let response1 = Base64URL.decode(step1.data) else {
            Issue.record("the app did not send step 1")
            return (spake, nil)
        }
        #expect(response1.count == 32)
        let step2 = try #require(spake.stationStep2(stored: stored, response1: response1))
        core.transport.deliver(.pairSpake(LinkMessage.PairSpake(step: 2, data: Base64URL.encode(step2))))
        return (spake, await core.transport.nextSent())
    }

    // MARK: One tap

    @Test func oneTapPairsWithTheCoreItsHelloNames() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .lan, device: device)
        core.transport.deliver(.pairAccept(LinkMessage.PairAccept(identity: try core.claim, label: "")))
        core.transport.dropLink()
        let paired = try await pairing.value
        #expect(paired.identityKey == core.identity.publicKey)
        #expect(paired.label == "")
        #expect(paired.endpoints == [core.endpoint])
        #expect(paired.trust == core.identity.trust)
        #expect(core.transport.dialledTrusts == [.pairing])
        #expect(core.transport.pending.isEmpty)
        #expect(await core.transport.waitUntilClosed())
    }

    @Test func oneTapRefusedCarriesTheCoresWordsAndWait() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .lan, device: device)
        let reason = "This Core pairs only with its code. Use the pairing code the Core shows."
        core.transport.deliver(.pairFail(LinkMessage.PairFail(reason: reason, retryAfterMs: 0)))
        #expect(await Self.failure(pairing) == .refused(reason: reason, retryAfter: .zero))
    }

    @Test func anAnswerNamingAnotherIdentityIsRefused() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .lan, device: device)
        let impostor = try TestStationIdentity().claim(certificateSHA256: core.transport.certificateSHA256)
        core.transport.deliver(.pairAccept(LinkMessage.PairAccept(identity: impostor, label: "Shack")))
        #expect(await Self.failure(pairing) == .identityMismatch)
    }

    @Test func aHelloWhoseBindingIsForAnotherCertificateIsRefusedBeforeAnythingIsSent() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        let other = try core.identity.claim(certificateSHA256: Data(repeating: 7, count: 32))
        core.transport.deliver(try core.hello(identity: other))
        #expect(await Self.failure(pairing) == .identityMismatch)
        #expect(core.transport.pending.isEmpty)
    }

    @Test func aCoreThatDoesNotDeclarePairingIsNeverAsked() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello(features: ["deviceAuth": 1]))
        #expect(await Self.failure(pairing) == .cannotPair)
        #expect(core.transport.pending.isEmpty)
    }

    // MARK: The code

    @Test func theCodePairsAndBothBoxesCarryWhatTheyShould() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        // Typed loosely: the app normalises it, and both ends hash the same text.
        let typed = "  " + core.code.uppercased().replacingOccurrences(of: "-", with: " . ") + " "
        let pairing = Task { try await client.pair(code: typed, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        let start = try await Self.takeOpening(core, mode: .code, device: device)
        let (spake, afterStep2) = try await Self.playToStep2(core, stationCode: core.code)
        guard case .pairSpake(let step3)? = afterStep2, step3.step == 3,
              let response3 = Base64URL.decode(step3.data) else {
            Issue.record("the app did not send step 3")
            return
        }
        #expect(spake.stationStep4(response3: response3))
        guard case .pairConfirm(let confirm)? = await core.transport.nextSent(),
              let box = Base64URL.decode(confirm.box) else {
            Issue.record("the app did not send its pair.confirm")
            return
        }
        let opened = try #require(spake.open(box))
        let contents = try #require(PairingBox.deviceContents(opened))
        #expect(contents == PairingBox.DeviceContents(publicKey: start.device.publicKey, name: "Shack iPhone",
                                                      kind: "phone"))
        let answer = PairingBox.StationContents(identity: try core.claim, label: "Shack Core")
        let sealed = try #require(spake.seal(PairingBox.encode(answer)))
        core.transport.deliver(.pairConfirm(LinkMessage.PairConfirm(box: Base64URL.encode(sealed))))
        core.transport.dropLink()
        let paired = try await pairing.value
        #expect(paired.identityKey == core.identity.publicKey)
        #expect(paired.label == "Shack Core")
        // Nothing else, and no session.end, from the app.
        #expect(core.transport.pending.isEmpty)
    }

    @Test func aWrongCodeSaysSoInPlaceOfStep3AndCarriesTheCoresWait() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .code, device: device)
        var other = core.code
        while other == core.code {
            let words = PairingCodeText.words
            other = "\(Int.random(in: 1...99))-\(words.randomElement()!)-\(words.randomElement()!)"
        }
        let (_, afterStep2) = try await Self.playToStep2(core, stationCode: other)
        guard case .pairFail(let fail)? = afterStep2 else {
            Issue.record("the app did not send pair.fail in place of step 3")
            return
        }
        #expect(fail.retryAfterMs == 0)
        #expect(!fail.reason.isEmpty)
        let reason = "The pairing code was not right. A new code will appear on the Core."
        core.transport.deliver(.pairFail(LinkMessage.PairFail(reason: reason, retryAfterMs: 5000)))
        #expect(await Self.failure(pairing) == .wrongCode(retryAfter: .milliseconds(5000), reason: reason))
        #expect(core.transport.pending.isEmpty)
    }

    @Test func weakerHashSettingsAreRefusedBeforeTheCodeIsHashed() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .code, device: device)
        let stored = try #require(SpakeExchange.storedData(code: core.code))
        var step0 = try #require(SpakeExchange(role: .station).stationStep0(stored: stored))
        // Step 0's opslimit (u64 LE at bytes 4 to 11) lowered to 1.
        step0.replaceSubrange(4..<12, with: [1, 0, 0, 0, 0, 0, 0, 0])
        core.transport.deliver(.pairSpake(LinkMessage.PairSpake(step: 0, data: Base64URL.encode(step0))))
        #expect(await Self.failure(pairing) == .weakHashSettings)
        #expect(core.transport.pending.isEmpty)
        #expect(core.transport.isClosedByApp)
    }

    @Test func aRefusalAfterTheAppsConfirmIsTheCoresRefusal() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .code, device: device)
        let (_, afterStep2) = try await Self.playToStep2(core, stationCode: core.code)
        guard case .pairSpake? = afterStep2, case .pairConfirm? = await core.transport.nextSent() else {
            Issue.record("the app did not send step 3 and its pair.confirm")
            return
        }
        let reason = "This Core is not taking new devices. Open pairing on the Core or on a paired device first."
        core.transport.deliver(.pairFail(LinkMessage.PairFail(reason: reason, retryAfterMs: 0)))
        #expect(await Self.failure(pairing) == .refused(reason: reason, retryAfter: .zero))
    }

    @Test func theCoresWaitBeforeStep0IsARefusal() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .code, device: device)
        let reason = "Another device is pairing with this Core right now. Try again shortly."
        core.transport.deliver(.pairFail(LinkMessage.PairFail(reason: reason, retryAfterMs: 5000)))
        #expect(await Self.failure(pairing) == .refused(reason: reason, retryAfter: .milliseconds(5000)))
    }

    /// Yields until `condition` holds (a bounded number of times).
    private static func until(_ condition: () -> Bool) async {
        for _ in 0..<100_000 where !condition() {
            await Task.yield()
        }
    }

    /// Plays a wrong code up to the app's own pair.fail.
    private static func playToTheAppsPairFail(_ core: Core, device: DeviceIdentity) async throws -> Bool {
        _ = try await Self.takeOpening(core, mode: .code, device: device)
        var other = core.code
        while other == core.code {
            let words = PairingCodeText.words
            other = "\(Int.random(in: 1...99))-\(words.randomElement()!)-\(words.randomElement()!)"
        }
        let (_, afterStep2) = try await Self.playToStep2(core, stationCode: other)
        guard case .pairFail? = afterStep2 else {
            Issue.record("the app did not send pair.fail in place of step 3")
            return false
        }
        return true
    }

    @Test func theFifthWrongCodeInARowCarriesNoWait() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        guard try await Self.playToTheAppsPairFail(core, device: device) else {
            return
        }
        let reason = "The pairing code was not right. A new code will appear on the Core."
        core.transport.deliver(.pairFail(LinkMessage.PairFail(reason: reason, retryAfterMs: 0)))
        #expect(await Self.failure(pairing) == .wrongCode(retryAfter: .zero, reason: reason))
    }

    @Test func aBareCloseAfterTheAppsPairFailIsAnEndNotAWrongCode() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        guard try await Self.playToTheAppsPairFail(core, device: device) else {
            return
        }
        core.transport.dropLink()
        #expect(await Self.failure(pairing) == .ended(reason: PairingClient.endedAfterWrongCodeText))
    }

    @Test func aSessionEndAfterTheAppsPairFailKeepsTheCoresWords() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        guard try await Self.playToTheAppsPairFail(core, device: device) else {
            return
        }
        core.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "The Core is shutting down.",
                                                                  retryable: true)))
        #expect(await Self.failure(pairing) == .ended(reason: "The Core is shutting down."))
    }

    @Test func aSecondPairingWithTheSameCoreIsRefusedBeforeItDials() async throws {
        let core = Core()
        let first = try core.client(device: try Self.device())
        let pairing = Task { try await first.pairOnThisNetwork(endpoint: core.endpoint) }
        // The first pairing has dialled and waits for the Core's hello.
        await Self.until { !core.transport.dialledTrusts.isEmpty }
        #expect(core.transport.dialledTrusts == [.pairing])

        let other = PairingTestTransport()
        let second = try PairingClient(identity: try Self.device(), name: "Shack iPad", kind: .tablet,
                                       clock: core.clock, transportFactory: other.factory)
        await #expect(throws: PairingError.alreadyPairing) {
            try await second.pair(code: core.code, via: .direct(core.endpoint))
        }
        await #expect(throws: PairingError.alreadyPairing) {
            try await second.pairOnThisNetwork(endpoint: core.endpoint)
        }
        #expect(other.dialledTrusts.isEmpty)
        #expect(core.transport.dialledTrusts == [.pairing])

        // Another Core is not held up by it.
        let elsewhere = Core()
        let third = try elsewhere.client(device: try Self.device())
        let pairingElsewhere = Task { try await third.pairOnThisNetwork(endpoint: elsewhere.endpoint) }
        elsewhere.transport.deliver(try elsewhere.hello())
        _ = await elsewhere.transport.nextSent()
        _ = await elsewhere.transport.nextSent()
        elsewhere.transport.deliver(.pairAccept(LinkMessage.PairAccept(identity: try elsewhere.claim, label: "")))
        #expect(try await pairingElsewhere.value.identityKey == elsewhere.identity.publicKey)

        core.transport.dropLink()
        #expect(await Self.failure(pairing) == .ended(reason: nil))
        // Once it ends, the Core can be paired with again.
        let again = PairingTestTransport()
        let fourth = try PairingClient(identity: try Self.device(), name: "Shack iPad", kind: .tablet,
                                       clock: core.clock, transportFactory: again.factory)
        let retry = Task { try await fourth.pairOnThisNetwork(endpoint: core.endpoint) }
        await Self.until { !again.dialledTrusts.isEmpty }
        #expect(again.dialledTrusts == [.pairing])
        again.dropLink()
        _ = await Self.failure(retry)
    }

    @Test func oneCoreSpelledTwoWaysSharesOneGate() async throws {
        let core = Core()
        let spelled = StationEndpoint(host: "Shack-\(UUID().uuidString).Example.NET.", port: 50055)
        let first = try core.client(device: try Self.device())
        let pairing = Task { try await first.pairOnThisNetwork(endpoint: spelled) }
        await Self.until { !core.transport.dialledTrusts.isEmpty }

        let other = PairingTestTransport()
        let second = try PairingClient(identity: try Self.device(), name: "Shack iPad", kind: .tablet,
                                       clock: core.clock, transportFactory: other.factory)
        let lowered = StationEndpoint(host: String(spelled.host.lowercased().dropLast()), port: 50055)
        await #expect(throws: PairingError.alreadyPairing) {
            try await second.pairOnThisNetwork(endpoint: lowered)
        }
        // The same host on another port is another Core.
        let elsewhere = Task { try await second.pairOnThisNetwork(endpoint: StationEndpoint(host: lowered.host, port: 47910)) }
        await Self.until { !other.dialledTrusts.isEmpty }
        #expect(other.dialledTrusts == [.pairing])
        other.dropLink()
        _ = await Self.failure(elsewhere)
        core.transport.dropLink()
        _ = await Self.failure(pairing)

        #expect(PairingClient.gateKey(StationEndpoint(host: "[2001:DB8::10]", port: 1))
            == StationEndpoint(host: "2001:db8::10", port: 1))
        #expect(PairingClient.gateKey(StationEndpoint(host: "core.example.", port: 1))
            == StationEndpoint(host: "core.example", port: 1))
    }

    @Test func aCancelledPairingLetsTheCoreBePairedAgainAtOnce() async throws {
        let core = Core()
        let first = try core.client(device: try Self.device())
        let pairing = Task { try await first.pair(code: core.code, via: .direct(core.endpoint)) }
        // Dialled, and waiting for a hello that never comes.
        await Self.until { !core.transport.dialledTrusts.isEmpty }
        pairing.cancel()
        await #expect(throws: CancellationError.self) { try await pairing.value }
        #expect(core.transport.isClosedByApp)

        // Straight away, with no Core's end and no deadline, it pairs again.
        let again = PairingTestTransport()
        let second = try PairingClient(identity: try Self.device(), name: "Shack iPad", kind: .tablet,
                                       clock: core.clock, transportFactory: again.factory)
        let retry = Task { try await second.pairOnThisNetwork(endpoint: core.endpoint) }
        await Self.until { !again.dialledTrusts.isEmpty }
        #expect(again.dialledTrusts == [.pairing])
        again.dropLink()
        #expect(await Self.failure(retry) == .ended(reason: nil))
    }

    // MARK: Ends

    @Test func aSessionEndIsAFailureWithTheCoresWords() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        let pairing = Task { try await client.pair(code: core.code, via: .direct(core.endpoint)) }
        core.transport.deliver(try core.hello())
        _ = await core.transport.nextSent()
        _ = await core.transport.nextSent()
        core.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "The Core is shutting down.",
                                                                  retryable: true)))
        #expect(await Self.failure(pairing) == .ended(reason: "The Core is shutting down."))
    }

    @Test func aBareCloseIsAFailure() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = await core.transport.nextSent()
        _ = await core.transport.nextSent()
        core.transport.dropLink()
        #expect(await Self.failure(pairing) == .ended(reason: nil))
    }

    @Test func aMessageTheAppCannotReadIsIgnored() async throws {
        let core = Core()
        let device = try Self.device()
        let client = try core.client(device: device)
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = try await Self.takeOpening(core, mode: .lan, device: device)
        core.transport.deliverText(#"{"type":"pair.newer","secret":"x"}"#)
        core.transport.deliver(.pairAccept(LinkMessage.PairAccept(identity: try core.claim, label: "Shack")))
        #expect(try await pairing.value.label == "Shack")
    }

    @Test func thePairingEndsAtTheConnectDeadline() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        let pairing = Task { try await client.pairOnThisNetwork(endpoint: core.endpoint) }
        core.transport.deliver(try core.hello())
        _ = await core.transport.nextSent()
        _ = await core.transport.nextSent()
        #expect(core.clock.pendingDueTimes == [30_000])
        await core.clock.advance(by: 30_000)
        #expect(await Self.failure(pairing) == .timedOut)
    }

    // MARK: Before anything is sent

    @Test func aTypingSlipIsCaughtBeforeAnythingIsDialled() async throws {
        let core = Core()
        let client = try core.client(device: try Self.device())
        await #expect(throws: PairingError.notACode) {
            try await client.pair(code: "7-anvil-harbour", via: .direct(core.endpoint))
        }
        #expect(core.transport.dialledTrusts.isEmpty)
    }

    @Test func aNameTheAppWouldRefuseAtSignInIsRefusedFirst() throws {
        let core = Core()
        let device = try Self.device()
        #expect(throws: DeviceKeyAuthenticator.UnusableName.self) {
            try core.client(device: device, name: "   ")
        }
        #expect(throws: DeviceKeyAuthenticator.UnusableName.self) {
            try core.client(device: device, name: "Shack\u{200B}iPhone")
        }
    }

    @Test func aSessionNeverSendsAPairingMessage() async throws {
        let station = ScriptedStation()
        let session = StationSession(endpoint: Self.endpoint, trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: ManualLinkClock(), transportFactory: station.factory)
        let messages: [LinkMessage] = [
            .pairStart(LinkMessage.PairStart(mode: .lan, device: LinkMessage.PairDevice(publicKey: "k", name: "n",
                                                                                        kind: "phone"))),
            .pairSpake(LinkMessage.PairSpake(step: 1, data: "AA")),
            .pairConfirm(LinkMessage.PairConfirm(box: "AA")),
            .pairFail(LinkMessage.PairFail(reason: "no", retryAfterMs: 0)),
            .pairAccept(LinkMessage.PairAccept(identity: LinkMessage.StationIdentityClaim(publicKey: "k",
                                                                                         certBinding: "b"),
                                               label: "")),
        ]
        for message in messages {
            await #expect(throws: LinkSendError.pairingOnSession) {
                try await session.send(message)
            }
        }
    }
}
