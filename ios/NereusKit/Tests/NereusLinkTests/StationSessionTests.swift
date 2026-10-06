// NereusSDR for iOS: the session's versions, sign-in, refusals, heartbeat, deadline and redial rules
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// The session against a scripted Core, with time moved by hand.
@Suite struct StationSessionTests {
    private struct Rig {
        let station: ScriptedStation
        let clock: ManualLinkClock
        let session: StationSession
        let recorder: EventRecorder
        let token = "conformance-token"

        init(trust: StationTrust? = nil, authenticator: (any StationAuthenticator)? = nil) {
            station = ScriptedStation()
            clock = ManualLinkClock()
            session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"),
                                     trust: trust ?? station.trust,
                                     authenticator: authenticator ?? TokenAuthenticator(token: "conformance-token"),
                                     clock: clock, transportFactory: station.factory)
            recorder = EventRecorder(session)
        }

        var transport: ScriptedTransport {
            get throws {
                try #require(station.latest)
            }
        }
    }

    private static func stationHello(majors: [UInt16]? = [1], major: UInt16 = 1) -> LinkMessage {
        .hello(LinkMessage.Hello(major: major, minor: 11, settingsSchema: 0, peer: "nereusd", majors: majors,
                                 features: [:]))
    }

    private static let accepted = LinkMessage.authResult(LinkMessage.AuthResult(accepted: true, reason: "",
                                                                               retryable: false))

    /// Connects and walks the connect sequence to ready.
    private func ready(_ rig: Rig) async throws {
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)
        await transport.deliver(.capabilities(LinkMessage.Capabilities(properties: [])))
        await transport.deliver(.snapshotComplete)
        #expect(await rig.session.state == .ready)
    }

    private func refused(_ rig: Rig) async -> [Refusal] {
        await rig.recorder.settle { events in
            events.contains { if case .refused = $0 { return true } else { return false } }
        }
        return rig.recorder.refusals
    }

    private static func held(_ revision: UInt32 = 4) -> LinkMessage {
        .sessionHeld(.init(devices: [.init(
            deviceId: "mac-id", name: "MacBook Pro", shortName: "MacBook", kind: "computer",
            state: .listening, from: "relay", replaceable: true, holdsTransmit: false,
            lastActivitySeconds: 120, connectedForSeconds: 3600, awayForSeconds: 0,
            transmittingForSeconds: 0, listeningOn: [])], revision: revision))
    }

    @Test func heldPausesConnectTimerAndKeepsOriginalAnswerDeadline() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256),
                                                   sessionHolder: true))
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)
        await rig.clock.advance(by: 29_000) { await transport.answerPings() }
        await transport.deliver(Self.held())
        #expect(await rig.session.heldQuestion?.revision == 4)
        await rig.clock.advance(by: 59_000) { await transport.answerPings() }
        #expect(!transport.isClosedByApp)
        await transport.deliver(Self.held(5))
        #expect(await rig.session.heldQuestion?.revision == 5)
        await rig.clock.advance(by: 1_000) { await transport.answerPings() }
        #expect(transport.isClosedByApp)
        #expect(await refused(rig).first?.code?.wireName == "coreFull")
    }

    @Test func heldAnswerMustNameCurrentReplaceableEntryAndCapabilitiesEndQuestion() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256),
                                                   sessionHolder: true))
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)
        await #expect(throws: LinkSendError.self) {
            try await rig.session.send(.sessionTakeover(.init(deviceId: "mac-id", revision: 4)))
        }
        await transport.deliver(Self.held())
        await #expect(throws: LinkSendError.self) {
            try await rig.session.send(.sessionTakeover(.init(deviceId: "mac-id", revision: 3)))
        }
        try await rig.session.send(.sessionTakeover(.init(deviceId: "mac-id", revision: 4)))
        #expect(try LinkCodec.decode(try #require(transport.takeSent()))
                == .sessionTakeover(.init(deviceId: "mac-id", revision: 4)))
        await transport.deliver(.capabilities(.init(properties: [])))
        #expect(await rig.session.heldQuestion == nil)
        await transport.deliver(.snapshotComplete)
        #expect(await rig.session.state == .ready)
        await rig.clock.advance(by: 60_000) { await transport.answerPings() }
        #expect(await rig.session.state == .ready)
    }

    @Test func heldQuestionRequiresThePairedFeatureGate() async throws {
        let tokenRig = Rig()
        await tokenRig.session.connect()
        let tokenTransport = try tokenRig.transport
        await tokenTransport.deliver(Self.stationHello())
        _ = tokenTransport.takeSent()
        _ = tokenTransport.takeSent()
        await tokenTransport.deliver(Self.accepted)
        await tokenTransport.deliver(Self.held())
        #expect(tokenTransport.isClosedByApp)
        #expect(await tokenRig.session.heldQuestion == nil)
        #expect(await refused(tokenRig).first?.code == .protocolError)

        let core = TestStationIdentity()
        let pairedRig = try Self.pairedRig(core)
        await pairedRig.session.connect()
        let pairedTransport = try pairedRig.transport
        await pairedTransport.deliver(Self.identityHello(
            try core.claim(certificateSHA256: pairedRig.station.certificateSHA256)))
        _ = pairedTransport.takeSent()
        _ = pairedTransport.takeSent()
        await pairedTransport.deliver(Self.accepted)
        await pairedTransport.deliver(Self.held())
        #expect(pairedTransport.isClosedByApp)
        #expect(await pairedRig.session.heldQuestion == nil)
        #expect(await refused(pairedRig).first?.code == .protocolError)
    }

    @Test func heldAnswerRejectsUnavailableEntryButCanExplicitlyCancel() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256),
                                                   sessionHolder: true))
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)
        guard case .sessionHeld(var held) = Self.held() else {
            Issue.record("expected held fixture")
            return
        }
        held.devices[0].replaceable = false
        await transport.deliver(.sessionHeld(held))
        await #expect(throws: LinkSendError.self) {
            try await rig.session.send(.sessionTakeover(.init(deviceId: "mac-id", revision: held.revision)))
        }
        #expect(transport.pending.isEmpty)
        try await rig.session.send(.sessionTakeover(.init(deviceId: "", revision: held.revision)))
        #expect(try LinkCodec.decode(try #require(transport.takeSent()))
                == .sessionTakeover(.init(deviceId: "", revision: held.revision)))
    }

    @Test func startedNativeDeadlineCannotEndAQuestionThatPausedIt() async throws {
        let core = TestStationIdentity()
        let station = ScriptedStation()
        let clock = GatedConnectDeadlineClock()
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: core.trust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                               kind: .phone),
                                     clock: clock, transportFactory: station.factory)
        await session.connect()
        let transport = try #require(station.latest)
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: station.certificateSHA256),
                                                   sessionHolder: true))
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)

        // The timer callback is already running. Cancellation can no longer
        // remove it from the clock's pending queue, but the actor has not
        // processed it yet.
        let firing = Task { await clock.advance(by: 30_000) { await transport.answerPings() } }
        await clock.gate.waitUntilStarted()
        await transport.deliver(Self.held())
        #expect(await session.heldQuestion?.revision == 4)
        await clock.gate.release()
        await firing.value
        #expect(!transport.isClosedByApp)
        #expect(await session.heldQuestion?.revision == 4)
        await clock.advance(by: 59_999) { await transport.answerPings() }
        #expect(!transport.isClosedByApp)
        await clock.advance(by: 1) { await transport.answerPings() }
        #expect(transport.isClosedByApp)
    }

    @Test func admissionResumesOnlyTheOriginalOpeningBudget() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256),
                                                   sessionHolder: true))
        _ = transport.takeSent()
        _ = transport.takeSent()
        await transport.deliver(Self.accepted)
        await rig.clock.advance(by: 29_000) { await transport.answerPings() }
        await transport.deliver(Self.held())
        await rig.clock.advance(by: 10_000) { await transport.answerPings() }
        await transport.deliver(.capabilities(.init(properties: [])))
        #expect(await rig.session.heldQuestion == nil)
        await rig.clock.advance(by: 999) { await transport.answerPings() }
        #expect(!transport.isClosedByApp)
        await rig.clock.advance(by: 1) { await transport.answerPings() }
        #expect(transport.isClosedByApp)
    }

    @Test func startedDeadlineFromRetiredAttemptCannotEndItsReplacement() async throws {
        let station = ScriptedStation()
        let clock = GatedConnectDeadlineClock()
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: clock, transportFactory: station.factory)
        await session.connect()
        let first = try #require(station.latest)
        let firing = Task { await clock.advance(by: 30_000) { await first.answerPings() } }
        await clock.gate.waitUntilStarted()
        await session.disconnect()
        await session.connect()
        let replacement = try #require(station.latest)
        #expect(replacement !== first)
        await clock.gate.release()
        await firing.value
        #expect(!replacement.isClosedByApp)
        #expect(await session.state == .connecting)
    }

    // MARK: The connect sequence and versions

    @Test func theAppAnswersTheHelloWithItsOwnThenItsToken() async throws {
        let rig = Rig()
        await rig.session.connect()
        #expect(await rig.session.state == .connecting)
        let transport = try rig.transport
        #expect(transport.pending.isEmpty)
        await transport.deliver(Self.stationHello(majors: [1, 2]))
        let sent = transport.pending
        try #require(sent.count == 2)
        guard case .hello(let hello) = try LinkCodec.decode(sent[0]),
              case .authRequest(let request) = try LinkCodec.decode(sent[1]) else {
            Issue.record("expected hello then auth.request: \(sent)")
            return
        }
        #expect(hello == LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "NereusSDR iPhone",
                                           majors: [1], features: ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                                                   "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1]))
        #expect(request.token == rig.token)
        #expect(await rig.session.state == .authenticating)
        #expect(await rig.session.agreedMajor == 1)
        await transport.deliver(Self.accepted)
        #expect(await rig.session.state == .receivingSnapshot)
        await transport.deliver(.snapshotComplete)
        #expect(await rig.session.state == .ready)
    }

    @Test func anOlderCoresHelloWithoutMajorsStandsForItsMajor() async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello(majors: nil, major: 1))
        #expect(transport.pending.count == 2)
        #expect(await rig.session.state == .authenticating)
    }

    @Test func theAgreedMinorIsTheLowerOfTheTwo() async throws {
        let rig = Rig()
        await rig.session.connect()
        try await rig.transport.deliver(.hello(LinkMessage.Hello(major: 1, minor: 4, settingsSchema: 0,
                                                                 peer: "nereusd", majors: [1])))
        #expect(await rig.session.agreedMinor == 4)
    }

    @Test(arguments: [
        ([UInt16(2), 3], Refusal.Reason.appTooOld(station: [2, 3], app: [1])),
        ([UInt16(0)], Refusal.Reason.stationTooOld(station: [0], app: [1])),
    ])
    func noSharedMajorRefusesSendsNothingAndDoesNotRetry(station majors: [UInt16], reason: Refusal.Reason) async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello(majors: majors, major: majors[0]))
        #expect(transport.pending.isEmpty)
        #expect(transport.isClosedByApp)
        #expect(await refused(rig) == [Refusal(reason)])
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    @Test func theVersionTableMatchesTheCores() {
        #expect(LinkVersionPolicy.agree(ours: [1], theirs: [1]) == 1)
        #expect(LinkVersionPolicy.agree(ours: [2, 3], theirs: [1, 2]) == 2)
        #expect(LinkVersionPolicy.agree(ours: [2, 3], theirs: [3, 4]) == 3)
        #expect(LinkVersionPolicy.agree(ours: [1], theirs: [2, 3]) == nil)
        #expect(LinkVersionPolicy.agree(ours: [2, 3], theirs: [1]) == nil)
        #expect(LinkVersionPolicy.refusal(station: [2, 3], app: [1]) == .appTooOld(station: [2, 3], app: [1]))
        #expect(LinkVersionPolicy.refusal(station: [1], app: [2, 3]) == .stationTooOld(station: [1], app: [2, 3]))
        #expect(LinkVersionPolicy.supportedMajors == [1])
        #expect(LinkFeatures.app == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                     "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
        #expect(LinkFeatures.app["pairing"] == nil)
    }

    @Test func onlyADebugBuildTakesTheMajorsArgument() {
        let arguments = ["NereusSDR", "-NereusLinkMajors", "2,1,2"]
        #expect(LinkVersionPolicy.majors(fromArguments: arguments, overrideAllowed: true) == [1, 2])
        #expect(LinkVersionPolicy.majors(fromArguments: arguments, overrideAllowed: false) == [1])
        #expect(LinkVersionPolicy.majors(fromArguments: ["NereusSDR", "-NereusLinkMajors", "0"],
                                         overrideAllowed: true) == [1])
        #expect(LinkVersionPolicy.majors(fromArguments: ["NereusSDR", "-NereusLinkMajors"],
                                         overrideAllowed: true) == [1])
        #expect(LinkVersionPolicy.parseMajors("1, 2") == [1, 2])
        #expect(LinkVersionPolicy.parseMajors("1,x") == nil)
        #expect(LinkVersionPolicy.parseMajors("65536") == nil)
    }

    // MARK: The certificate

    @Test func aCertificateOtherThanThePinIsRefusedAndNothingIsRead() async throws {
        let rig = Rig(trust: .certificate(pinSHA256: Data(repeating: 7, count: 32)))
        await rig.session.connect()
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText), code: .identityChanged)])
        #expect(await rig.session.state == .stopped)
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        #expect(transport.pending.isEmpty)
        #expect(rig.recorder.messages.isEmpty)
    }

    @Test func aMismatchInTheHandshakeIsRefusedWithoutRetrying() async throws {
        let rig = Rig()
        rig.station.failOpens(with: .certificateMismatch)
        await rig.session.connect()
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText), code: .identityChanged)])
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    // MARK: A paired Core's identity

    /// A session to a Core paired as `core`, signing in with a device key.
    private static func pairedRig(_ core: TestStationIdentity) throws -> Rig {
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        return Rig(trust: core.trust,
                   authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone", kind: .phone))
    }

    /// A Core's hello carrying `identity` (none when nil) and a fresh challenge.
    private static func identityHello(_ identity: LinkMessage.StationIdentityClaim?,
                                      sessionHolder: Bool = false) -> LinkMessage {
        .hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                 features: sessionHolder ? ["deviceAuth": 1, "pairing": 1, "sessionHolder": 1]
                                     : ["deviceAuth": 1, "pairing": 1], identity: identity,
                                 challenge: TestStationIdentity.newChallenge()))
    }

    @Test func thePairedCoreIsSignedInToWithTheDeviceKey() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        let hello = Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256))
        await transport.deliver(hello)
        let sent = transport.pending
        try #require(sent.count == 2)
        guard case .hello(let ours) = try LinkCodec.decode(sent[0]),
              case .authRequest(let request) = try LinkCodec.decode(sent[1]),
              case .hello(let theirs) = hello, let challenge = theirs.challenge.flatMap(Base64URL.decode) else {
            Issue.record("expected hello then auth.request: \(sent)")
            return
        }
        #expect(ours.features == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                  "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
        #expect(request.token == "")
        let block = try #require(request.device)
        #expect(block.shortName == "iPhone")
        guard case .object(let fields) = LinkCodec.json(.authRequest(request)), let json = fields["device"] else {
            Issue.record("no device block")
            return
        }
        #expect(DeviceBlockCheck.failure(json, challenge: challenge, certificateSHA256: rig.station.certificateSHA256,
                                         stationKey: core.publicKey) == nil)
        #expect(await rig.session.state == .authenticating)
    }

    @Test func anotherIdentityIsRefusedBeforeAnythingIsSent() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        let impostor = TestStationIdentity()
        await transport.deliver(Self.identityHello(try impostor.claim(certificateSHA256: rig.station.certificateSHA256)))
        #expect(transport.pending.isEmpty)
        #expect(transport.isClosedByApp)
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText),
                                               code: .identityChanged)])
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    @Test func aBindingOfAnotherCertificateIsRefusedBeforeAnythingIsSent() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        let other = Data(repeating: 9, count: 32)
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: other)))
        #expect(transport.pending.isEmpty)
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText),
                                               code: .identityChanged)])
        #expect(await rig.session.state == .stopped)
    }

    @Test func aPairedCoreWhoseHelloHasNoIdentityIsRefused() async throws {
        let rig = try Self.pairedRig(TestStationIdentity())
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(nil))
        #expect(transport.pending.isEmpty)
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText),
                                               code: .identityChanged)])
    }

    @Test func aBindingThatIsNotBase64URLIsRefused() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        var claim = try core.claim(certificateSHA256: rig.station.certificateSHA256)
        claim.certBinding += "="
        await transport.deliver(Self.identityHello(claim))
        #expect(transport.pending.isEmpty)
        #expect(await refused(rig).first?.code == .identityChanged)
    }

    @Test func anImpostorsMessagesBeforeItsHelloAreNeverPassedOn() async throws {
        let rig = try Self.pairedRig(TestStationIdentity())
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            LinkMessage.PropertyEntry(name: "stationName", value: .utf8("Impostor")),
        ])))
        await transport.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel",
                                                                       properties: [])))
        await transport.deliver(.snapshotComplete)
        await transport.deliver(Self.identityHello(try TestStationIdentity().claim(
            certificateSHA256: rig.station.certificateSHA256)))
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText),
                                               code: .identityChanged)])
        // Nothing from the impostor, its hello included, reached the client.
        #expect(rig.recorder.messages.isEmpty)
        #expect(transport.pending.isEmpty)
    }

    @Test(arguments: [true, false])
    func anEndBeforeTheHelloIsCheckedHasNoCodeAndNoCoreWords(retryable: Bool) async throws {
        let rig = try Self.pairedRig(TestStationIdentity())
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "Anything at all.", retryable: retryable,
                                                                   code: "deviceRemoved")))
        let refusal = try #require(await refused(rig).first)
        #expect(refusal.code == nil)
        #expect(refusal == Refusal(.ended(StationSession.endedBeforeCheckText, retryable: retryable)))
        #expect(rig.recorder.messages.isEmpty)
        if retryable {
            #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        } else {
            #expect(await rig.session.state == .stopped)
        }
    }

    @Test func aCheckedCoresEndKeepsItsWordsAndCode() async throws {
        let core = TestStationIdentity()
        let rig = try Self.pairedRig(core)
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.identityHello(try core.claim(certificateSHA256: rig.station.certificateSHA256)))
        let reason = "This device was removed from the Core."
        await transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: reason, retryable: false,
                                                                   code: "deviceRemoved")))
        #expect(await refused(rig) == [Refusal(.ended(reason, retryable: false), code: .deviceRemoved)])
    }

    @Test func aDeviceKeyIsNeverUsedWithoutAnIdentityTrust() async throws {
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let rig = Rig(authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone", kind: .phone))
        await rig.session.connect()
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.signInFailedText))])
        #expect(rig.station.dialCount == 0)
        #expect(await rig.session.state == .stopped)
    }

    @Test func pinsParseAndPrintAsTheCoreWritesThem() {
        let digest = Data((0..<32).map { UInt8($0 * 7) })
        let written = CertificatePin.format(digest)
        #expect(written.count == 95)
        #expect(CertificatePin.parse(written) == digest)
        #expect(CertificatePin.parse(written.lowercased()) == digest)
        #expect(CertificatePin.parse("AB:CD") == nil)
    }

    @Test func anIPv6EndpointBecomesABracketedURL() {
        let url = WebSocketLinkTransport.url(for: StationEndpoint(host: "fe80::1%en0", port: 5000))
        #expect(url.map { "\($0)" }?.contains("[fe80::1%25en0]:5000") == true)
    }

    // MARK: Sign-in

    @Test func aWrongTokenIsRefusedWithTheCoresWordsAndNotRetried() async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        let reason = "The Core did not accept this app's pairing token. Check the token saved for this Core."
        await transport.deliver(.authResult(LinkMessage.AuthResult(accepted: false, reason: reason, retryable: false)))
        await transport.dropLink()
        #expect(await refused(rig) == [Refusal(.authentication(reason))])
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    @Test func aLockoutIsRetried() async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        let reason = "The Core is refusing pairing tokens for a while after too many wrong ones. Try again later."
        await transport.deliver(.authResult(LinkMessage.AuthResult(accepted: false, reason: reason, retryable: true)))
        #expect(await refused(rig) == [Refusal(.authentication(reason))])
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.clock.advance(by: 1_000)
        #expect(rig.station.dialCount == 2)
    }

    @Test func anAuthenticatorFailureStopsWithItsWords() async throws {
        struct Failing: StationAuthenticator {
            func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
                -> LinkMessage.AuthRequest {
                throw StationAuthenticationError(reason: "Pair with this Core first.")
            }
        }
        let rig = Rig(authenticator: Failing())
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        #expect(transport.pending.isEmpty)
        #expect(await refused(rig) == [Refusal(.authentication("Pair with this Core first."))])
        #expect(await rig.session.state == .stopped)
    }

    // MARK: End codes

    @Test func aRefusedSignInCarriesItsCode() async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        let reason = "This Core uses paired devices. Pair this device first."
        await transport.deliver(.authResult(LinkMessage.AuthResult(accepted: false, reason: reason, retryable: false,
                                                                   code: "pairingRequired")))
        #expect(await refused(rig) == [Refusal(.authentication(reason), code: .pairingRequired)])
        #expect(await rig.session.state == .stopped)
    }

    @Test(arguments: ["takenOver", "linkVersion", "pairingRequired", "wrongToken", "deviceNotPaired",
                      "deviceProofFailed", "deviceRemoved", "identityChanged", "protocolError", "aNewerCode"])
    func aSessionEndCarriesItsCode(code: String) async throws {
        let rig = Rig()
        try await ready(rig)
        try await rig.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "Gone.", retryable: false,
                                                                           code: code)))
        let refusal = try #require(await refused(rig).first)
        #expect(refusal.code?.wireName == code)
        if code == "aNewerCode" || code == "identityChanged" {
            // A station never sends identityChanged (it is the app's own end).
            #expect(refusal.code == .other(code))
        } else {
            #expect(refusal.code == Refusal.Code(wireName: code))
        }
    }

    @Test func anEndWithoutACodeHasNone() async throws {
        let rig = Rig()
        try await ready(rig)
        try await rig.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "The Core is shutting down.",
                                                                           retryable: true)))
        #expect(await refused(rig) == [Refusal(.ended("The Core is shutting down.", retryable: true))])
    }

    // MARK: Ending and redialling

    @Test func theSessionNeverHoldsTwoConnectionsAtOnce() async throws {
        let rig = Rig()
        await rig.session.connect()
        #expect(rig.station.openConnections <= 1)
        // A connection the Core turns away as over its cap is retryable.
        let full = "The Core already has as many connections as it allows. Try again shortly."
        try await rig.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: full, retryable: true)))
        #expect(rig.station.openConnections == 0)
        // Connecting again while it waits dials once, and a second connect
        // while that one is still connecting dials nothing more.
        await rig.session.connect()
        await rig.session.connect()
        #expect(rig.station.dialCount == 2)
        #expect(rig.station.openConnections == 1)
        try await rig.transport.dropLink()
        #expect(rig.station.openConnections == 0)
        for _ in 0..<6 {
            await rig.clock.advance(by: 60_000)
            #expect(rig.station.openConnections <= 1)
        }
    }

    @Test func aSessionEndBeforeTheHelloFollowsItsRetryable() async throws {
        let full = "The Core already has as many connections as it allows. Try again shortly."
        let retrying = Rig()
        await retrying.session.connect()
        try await retrying.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: full, retryable: true)))
        #expect(await refused(retrying) == [Refusal(.ended(full, retryable: true))])
        #expect(await retrying.session.state == .waitingToRetry(seconds: 1))

        let final = Rig()
        await final.session.connect()
        try await final.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "Gone.", retryable: false)))
        #expect(await refused(final) == [Refusal(.ended("Gone.", retryable: false))])
        #expect(await final.session.state == .stopped)
        await final.clock.advance(by: 600_000)
        #expect(final.station.dialCount == 1)
    }

    @Test func aPreemptionStopsTheSession() async throws {
        let rig = Rig()
        try await ready(rig)
        let reason = "Another app at 192.0.2.9 connected to the Core and took over. Connect again to take it back."
        try await rig.transport.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: reason, retryable: false)))
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    @Test func aLostLinkRedialsOnTheScheduleAndReadyStartsItOver() async throws {
        let rig = Rig()
        try await ready(rig)
        rig.station.failOpens(with: .failed("unreachable"))
        try await rig.transport.dropLink()

        var waits: [Int] = []
        for _ in 0..<8 {
            guard case .waitingToRetry(let seconds) = await rig.session.state else {
                Issue.record("not waiting to retry")
                return
            }
            waits.append(seconds)
            await rig.clock.advance(by: Int64(seconds) * 1_000)
        }
        #expect(waits == [1, 2, 5, 10, 30, 60, 60, 60])
        #expect(rig.station.dialCount == 9)

        // The next attempt gets through; reaching ready starts the schedule over.
        rig.station.failOpens(with: nil)
        await rig.clock.advance(by: 60_000)
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        await transport.deliver(Self.accepted)
        await transport.deliver(.snapshotComplete)
        #expect(await rig.session.state == .ready)
        await transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    // MARK: Holding and redialling at once (the phone's network)

    @Test func heldRetriesDialNothingUntilARedial() async throws {
        let rig = Rig()
        try await ready(rig)
        await rig.session.holdRetries()
        rig.station.failOpens(with: .failed("unreachable"))
        try await rig.transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        #expect(rig.clock.pendingDueTimes.isEmpty)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
        await rig.session.redialNow()
        #expect(rig.station.dialCount == 2)
        // The hold is lifted: the schedule runs again from its first step.
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.clock.advance(by: 1_000)
        #expect(rig.station.dialCount == 3)
        #expect(await rig.session.state == .waitingToRetry(seconds: 2))
    }

    @Test func holdingCancelsARetryAlreadyScheduled() async throws {
        let rig = Rig()
        try await ready(rig)
        rig.station.failOpens(with: .failed("unreachable"))
        try await rig.transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.session.holdRetries()
        #expect(rig.clock.pendingDueTimes.isEmpty)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    @Test func aRedialGoesAtOnceAndStartsTheScheduleOver() async throws {
        let rig = Rig()
        try await ready(rig)
        rig.station.failOpens(with: .failed("unreachable"))
        try await rig.transport.dropLink()
        for wait in [1, 2, 5, 10] {
            #expect(await rig.session.state == .waitingToRetry(seconds: wait))
            await rig.clock.advance(by: Int64(wait) * 1_000)
        }
        #expect(await rig.session.state == .waitingToRetry(seconds: 30))
        #expect(rig.station.dialCount == 5)
        await rig.session.redialNow()
        #expect(rig.station.dialCount == 6)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        #expect(rig.clock.pendingDueTimes == [rig.clock.now + 1_000])
    }

    @Test func aRedialOnAReadySessionPingsAtOnceAndKeepsAnAnsweredLink() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await rig.session.redialNow()
        // Checked at once, not at the heartbeat's first tick 20 s in.
        #expect(transport.pingCount == 1)
        #expect(rig.station.dialCount == 1)
        await transport.answerPings()
        await rig.clock.advance(by: 5_000)
        #expect(await rig.session.state == .ready)
        #expect(!transport.isClosedByApp)
        // The heartbeat carries on as before.
        await rig.clock.advance(by: 15_000)
        #expect(transport.pingCount == 2)
        #expect(await rig.session.state == .ready)
    }

    @Test func aRedialOnAReadySessionWhoseLinkIsGoneLosesItWithinFiveSeconds() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await rig.clock.advance(by: 1_000)
        await rig.session.redialNow()
        #expect(transport.pingCount == 1)
        await rig.clock.advance(by: 4_999)
        #expect(await rig.session.state == .ready)
        await rig.clock.advance(by: 1)
        #expect(transport.isClosedByApp)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func aRedialLeavesAStoppedSessionAlone() async throws {
        let rig = Rig()
        try await ready(rig)
        await rig.session.disconnect()
        await rig.session.redialNow()
        #expect(rig.station.dialCount == 1)
        #expect(await rig.session.state == .stopped)
    }

    // MARK: Media and the redial schedule

    /// Two failed dials (1 s, then 2 s) move the schedule on; the third
    /// gets through and reaches ready with `capabilities` offering media or not.
    private func readyAfterTwoFailures(_ rig: Rig, mediaVersion: Int64) async throws -> ScriptedTransport {
        rig.station.failOpens(with: .failed("unreachable"))
        await rig.session.connect()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.clock.advance(by: 1_000)
        #expect(await rig.session.state == .waitingToRetry(seconds: 2))
        rig.station.failOpens(with: nil)
        await rig.clock.advance(by: 2_000)
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        await transport.deliver(Self.accepted)
        await transport.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            LinkMessage.PropertyEntry(name: "remoteMediaVersion", value: .i64(mediaVersion)),
        ])))
        await transport.deliver(.snapshotComplete)
        #expect(await rig.session.state == .ready)
        return transport
    }

    @Test func withMediaTheScheduleStartsOverOnlyWhenTheMediaConnectionIsUp() async throws {
        let rig = Rig()
        await rig.session.setWaitsForMedia(true)
        let transport = try await readyAfterTwoFailures(rig, mediaVersion: 1)
        // The media connection never came up: the schedule carries on.
        await transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 5))

        let second = Rig()
        await second.session.setWaitsForMedia(true)
        let reached = try await readyAfterTwoFailures(second, mediaVersion: 1)
        await second.session.mediaConnectionUp()
        await reached.dropLink()
        #expect(await second.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func aCoreWithoutMediaStartsTheScheduleOverAtTheSnapshot() async throws {
        let rig = Rig()
        await rig.session.setWaitsForMedia(true)
        let transport = try await readyAfterTwoFailures(rig, mediaVersion: 0)
        await transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))

        // Media withdrawn while the schedule waited for it counts as up.
        let withdrawn = Rig()
        await withdrawn.session.setWaitsForMedia(true)
        let link = try await readyAfterTwoFailures(withdrawn, mediaVersion: 1)
        await link.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            LinkMessage.PropertyEntry(name: "remoteMediaVersion", value: .i64(0)),
        ])))
        await link.dropLink()
        #expect(await withdrawn.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func withoutMediaWaitingTheSnapshotStartsTheScheduleOver() async throws {
        let rig = Rig()
        let transport = try await readyAfterTwoFailures(rig, mediaVersion: 1)
        await transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func disconnectDuringAWaitStopsWithNoFurtherAttempt() async throws {
        let rig = Rig()
        try await ready(rig)
        try await rig.transport.dropLink()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.session.disconnect()
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
        #expect(rig.clock.pendingDueTimes.isEmpty)
    }

    // MARK: The deadline and the heartbeat

    @Test func anAttemptShortOfTheSnapshotAfterThirtySecondsEndsAndRetries() async throws {
        let rig = Rig()
        await rig.session.connect()
        let transport = try rig.transport
        await transport.deliver(Self.stationHello())
        await transport.deliver(Self.accepted)
        await rig.clock.advance(by: 29_999) { await transport.answerPings() }
        #expect(await rig.session.state == .receivingSnapshot)
        await rig.clock.advance(by: 1) { await transport.answerPings() }
        #expect(transport.isClosedByApp)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func theDeadlineStopsAtReady() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await rig.clock.advance(by: 120_000) { await transport.answerPings() }
        #expect(await rig.session.state == .ready)
        #expect(transport.pingCount == 6)
    }

    @Test func twoMissedPongsLoseTheLink() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await rig.clock.advance(by: 40_000)
        #expect(transport.pingCount == 2)
        #expect(await rig.session.state == .ready)
        await rig.clock.advance(by: 20_000)
        #expect(transport.pingCount == 2)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func anAnsweredPingGivesTheRoundTrip() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        var times = rig.session.roundTrips.makeAsyncIterator()
        await rig.clock.advance(by: 20_000)
        #expect(transport.pingCount == 1)
        await transport.answerPings()
        let first = await times.next()
        #expect(first.map { $0 >= .zero } == true)
    }

    @Test func aPongAfterTwoPingsGivesNoRoundTrip() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        let times = RoundTrips()
        let reader = Task { for await time in rig.session.roundTrips { times.append(time) } }
        await rig.clock.advance(by: 40_000)
        #expect(transport.pingCount == 2)
        // The pong cannot say which ping it answers: nothing is timed.
        await transport.answerPings()
        for _ in 0..<1_000 {
            await Task.yield()
        }
        #expect(times.count == 0)
        // The next ping goes out alone, so its answer is timed.
        await rig.clock.advance(by: 20_000)
        await transport.answerPings()
        for _ in 0..<10_000 where times.count == 0 {
            await Task.yield()
        }
        #expect(times.count == 1)
        reader.cancel()
    }

    /// Round-trip times read from a session, across tasks.
    final class RoundTrips: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [Duration] = []

        var count: Int { lock.withLock { stored.count } }

        func append(_ time: Duration) {
            lock.withLock { stored.append(time) }
        }
    }

    @Test func aLatePongStillCounts() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await rig.clock.advance(by: 40_000)
        await transport.answerPings()
        await rig.clock.advance(by: 20_000)
        #expect(await rig.session.state == .ready)
    }

    // MARK: What send refuses

    @Test func settingsWritesAndRemovalsHonorPermitAtTransportHandoff() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        let messages: [LinkMessage] = [
            .settingsWrite(.init(key: "CWPitch", origin: "phone", properties: [
                .init(name: "CWPitch", value: .utf8("650"))])),
            .settingsRemove(.init(key: "CWPitch")),
        ]
        for message in messages {
            let revoked = CommandSendPermit()
            revoked.revoke()
            await #expect(throws: LinkSendError.notConnected) {
                try await rig.session.send(message, permit: revoked)
            }
            #expect(transport.takeSent() == nil)
            try await rig.session.send(message, permit: CommandSendPermit())
            #expect(try LinkCodec.decode(try #require(transport.takeSent())) == message)
        }
    }

    @Test func sendRefusesWhatTheCoreWouldEndTheSessionFor() async throws {
        let rig = Rig()
        let write = LinkMessage.propertyWrite(LinkMessage.PropertyWrite(key: "slice:0", writeId: 1, properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_074_000)),
        ]))
        let media = LinkMessage.mediaControl(LinkMessage.MediaControl(payload: ["op": .string("start"),
                                                                                "connectionId": .number(1)]))

        await #expect(throws: LinkSendError.notConnected) { try await rig.session.send(write) }
        await rig.session.connect()
        let transport = try rig.transport
        await #expect(throws: LinkSendError.beforeSignIn) { try await rig.session.send(write) }
        await transport.deliver(Self.stationHello())
        _ = transport.takeSent()
        _ = transport.takeSent()

        await #expect(throws: LinkSendError.secondHello) {
            try await rig.session.send(Self.stationHello())
        }
        await #expect(throws: LinkSendError.authRequestOutOfOrder) {
            try await rig.session.send(.authRequest(LinkMessage.AuthRequest(token: "again")))
        }
        await #expect(throws: LinkSendError.beforeSignIn) { try await rig.session.send(write) }

        await transport.deliver(Self.accepted)
        await #expect(throws: LinkSendError.beforeSnapshotComplete) { try await rig.session.send(media) }
        try await rig.session.send(write)
        #expect(transport.takeSent() != nil)

        await transport.deliver(.snapshotComplete)
        try await rig.session.send(media)
        #expect(transport.takeSent() != nil)

        await #expect(throws: LinkSendError.notAClientMessage) {
            try await rig.session.send(.snapshotComplete)
        }
        let big = String(repeating: "x", count: StationSession.maxOutboundMessageBytes)
        let oversized = LinkMessage.settingsWrite(LinkMessage.SettingsWrite(key: "StationCallsign", origin: "a",
            properties: [LinkMessage.PropertyEntry(name: "StationCallsign", value: .utf8(big))]))
        #expect(await refusal(of: oversized, by: rig.session) == .tooLarge(bytes: LinkCodec.encode(oversized).utf8.count))
        let pad = LinkJSON.string(String(repeating: "x", count: LinkCodec.mediaControlCapBytes))
        let bigMedia = LinkMessage.mediaControl(LinkMessage.MediaControl(payload: ["op": .string("x"), "pad": pad]))
        #expect(await refusal(of: bigMedia, by: rig.session) == .tooLarge(bytes: LinkCodec.encode(bigMedia).utf8.count))
        let numbered = LinkMessage.commandInvoke(LinkMessage.CommandInvoke(verb: "nnr.resetTuning", id: 0, args: []))
        guard case .unreadable? = await refusal(of: numbered, by: rig.session) else {
            Issue.record("an nnr command with id 0 was not refused as unreadable")
            return
        }
        let zeroWrite = LinkMessage.propertyWrite(LinkMessage.PropertyWrite(key: "slice:0", writeId: 0, properties: []))
        guard case .unreadable? = await refusal(of: zeroWrite, by: rig.session) else {
            Issue.record("a writeId of 0 was not refused as unreadable")
            return
        }
        #expect(transport.pending.isEmpty)
        #expect(await rig.session.state == .ready)
    }

    @Test func locallyRefusedCommandHasNoHandoffReceiptBeforeCloseEvent() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        transport.close()
        let receipt = CommandHandoffReceipt()
        let permit = CommandSendPermit(handoffReceipt: receipt)
        let leave = LinkMessage.commandInvoke(.init(verb: "session.leave", id: 1, args: []))
        await #expect(throws: LinkSendError.notConnected) {
            try await rig.session.send(leave, permit: permit)
        }
        #expect(!receipt.wasSent)
        #expect(transport.pending.isEmpty)
        #expect(await rig.session.state == .ready)
    }

    private func refusal(of message: LinkMessage, by session: StationSession) async -> LinkSendError? {
        do {
            try await session.send(message)
            return nil
        } catch {
            return error as? LinkSendError
        }
    }

    // MARK: Unknown kinds

    @Test func aKindTheAppDoesNotKnowIsIgnored() async throws {
        let rig = Rig()
        try await ready(rig)
        let transport = try rig.transport
        await transport.deliver(#"{"type":"conformance.future","value":1}"#)
        await transport.deliver(#"{"type":"auth.request","token":"meant for the Core"}"#)
        await transport.deliver("not json")
        let delta = LinkMessage.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 15, name: "signalStrengthDbm", value: .f64(-73.5)),
        ]))
        await transport.deliver(delta)
        #expect(await rig.session.state == .ready)
        await rig.recorder.settle { $0.contains(.message(delta)) }
        #expect(rig.recorder.messages.last == delta)
        #expect(transport.pending.isEmpty)
    }
}

/// Holds a native deadline callback after it has left the timer queue and
/// started running, just before that callback enters the session actor.
private final class GatedConnectDeadlineClock: LinkClock, @unchecked Sendable {
    actor Gate {
        private var started = false
        private var startedWaiter: CheckedContinuation<Void, Never>?
        private var releaseWaiter: CheckedContinuation<Void, Never>?
        private var released = false

        func waitBeforeActorHop() async {
            started = true
            startedWaiter?.resume()
            startedWaiter = nil
            if !released {
                await withCheckedContinuation { releaseWaiter = $0 }
            }
        }

        func waitUntilStarted() async {
            if started { return }
            await withCheckedContinuation { startedWaiter = $0 }
        }

        func release() {
            released = true
            releaseWaiter?.resume()
            releaseWaiter = nil
        }
    }

    private let base = ManualLinkClock()
    private let lock = NSLock()
    private var capturedFirstConnectDeadline = false
    let gate = Gate()

    var nowMilliseconds: Int64 { base.nowMilliseconds }

    func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
        let capture = lock.withLock { () -> Bool in
            guard delay == StationSession.connectDeadline, !capturedFirstConnectDeadline else { return false }
            capturedFirstConnectDeadline = true
            return true
        }
        if capture {
            return base.schedule(after: delay) { [gate] in
                await gate.waitBeforeActorHop()
                await action()
            }
        }
        return base.schedule(after: delay, action)
    }

    func advance(by milliseconds: Int64, afterEach: @escaping @Sendable () async -> Void = {}) async {
        await base.advance(by: milliseconds, afterEach: afterEach)
    }
}
