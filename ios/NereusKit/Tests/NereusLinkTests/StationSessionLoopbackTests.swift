// NereusSDR for iOS: the session over a real TLS WebSocket to a Core on 127.0.0.1
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// The real transport against `LoopbackStation`. Time for the session's
/// own timers moves by hand; only the sockets run in real time, so the
/// waits here are bounded waits for the far end, never timing.
@Suite(.serialized) struct StationSessionLoopbackTests {
    private struct Rig {
        let station: LoopbackStation
        let clock: ManualLinkClock
        let session: StationSession
        let recorder: EventRecorder
        let token: String
    }

    private func rig(pin: Data? = nil) async throws -> Rig {
        let station = try LoopbackStation()
        let endpoint = try await station.start()
        let clock = ManualLinkClock()
        let token = SessionFixturePlayer.randomToken()
        let session = StationSession(endpoint: endpoint, trust: .certificate(pinSHA256: pin ?? station.pin),
                                     authenticator: TokenAuthenticator(token: token), clock: clock)
        return Rig(station: station, clock: clock, session: session, recorder: EventRecorder(session), token: token)
    }

    private func ready(_ rig: Rig) async throws {
        await rig.session.connect()
        let reached = await rig.recorder.wait { $0.contains(.stateChanged(.ready)) }
        try #require(reached, "the session did not reach ready: \(rig.recorder.events)")
    }

    /// Waits in real time for `condition`, up to ten seconds.
    private func eventually(_ condition: @Sendable () async -> Bool) async -> Bool {
        let deadline = ContinuousClock.now.advanced(by: .seconds(10))
        while ContinuousClock.now < deadline {
            if await condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
        return await condition()
    }

    @Test func reachesReadyThroughTheConnectSequence() async throws {
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)

        let texts = rig.station.receivedTexts
        try #require(texts.count == 2)
        guard case .hello(let hello) = try LinkCodec.decode(texts[0]),
              case .authRequest(let request) = try LinkCodec.decode(texts[1]) else {
            Issue.record("expected hello then auth.request, got \(texts)")
            return
        }
        #expect(hello.major == 1)
        #expect(hello.minor == 11)
        #expect(hello.majors == [1])
        #expect(hello.features == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                   "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
        #expect(request.token == rig.token)
        #expect(rig.recorder.states == [.connecting, .authenticating, .receivingSnapshot, .ready])
        #expect(rig.recorder.messages.contains(.snapshotComplete))
        await rig.session.disconnect()
    }

    @Test func aCertificateOtherThanThePinIsRefusedBeforeAnythingIsRead() async throws {
        let rig = try await rig(pin: Data(repeating: 0xAB, count: 32))
        defer { rig.station.stop() }
        await rig.session.connect()
        let refused = await rig.recorder.wait { events in
            events.contains { if case .refused = $0 { return true } else { return false } }
        }
        #expect(refused)
        #expect(rig.recorder.refusals == [Refusal(.authentication(StationSession.certificateMismatchText),
                                                  code: .identityChanged)])
        // The Core's hello was never read, and the app sent nothing.
        #expect(rig.recorder.messages.isEmpty)
        #expect(rig.station.receivedTexts.isEmpty)
        #expect(await rig.session.state == .stopped)
    }

    // MARK: A paired Core, over the real transport

    /// A session that trusts `trusted` and signs in with a new device key,
    /// to a loopback Core whose hello carries `presented`.
    private func pairedRig(trusted: TestStationIdentity, presented: TestStationIdentity)
        async throws -> (Rig, DeviceIdentity) {
        let station = try LoopbackStation(identity: presented)
        let endpoint = try await station.start()
        let clock = ManualLinkClock()
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let session = StationSession(endpoint: endpoint, trust: trusted.trust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                               kind: .phone),
                                     clock: clock)
        return (Rig(station: station, clock: clock, session: session, recorder: EventRecorder(session), token: ""),
                device)
    }

    @Test func aPairedCoreIsReachedWhateverItsCertificateAndSignedInToByKey() async throws {
        let core = TestStationIdentity()
        let (rig, device) = try await pairedRig(trusted: core, presented: core)
        defer { rig.station.stop() }
        try await ready(rig)

        let texts = rig.station.receivedTexts
        try #require(texts.count == 2)
        guard case .authRequest(let request) = try LinkCodec.decode(texts[1]),
              case .object(let fields) = try LinkJSON.parse(texts[1]), let block = fields["device"] else {
            Issue.record("expected an auth.request with a device block, got \(texts)")
            return
        }
        #expect(request.token == "")
        #expect(request.device?.id == device.id)
        let challenge = try #require(rig.station.sentChallenges.first)
        // The certificate the transport reported is the one the Core presented.
        #expect(DeviceBlockCheck.failure(block, challenge: challenge, certificateSHA256: rig.station.pin,
                                         stationKey: core.publicKey) == nil)
        await rig.session.disconnect()
    }

    @Test func aCoreWithAnotherIdentityIsRefusedBeforeAnythingIsSent() async throws {
        let (rig, _) = try await pairedRig(trusted: TestStationIdentity(), presented: TestStationIdentity())
        defer { rig.station.stop() }
        await rig.session.connect()
        let refused = await rig.recorder.wait { events in
            events.contains { if case .refused = $0 { return true } else { return false } }
        }
        #expect(refused)
        #expect(rig.recorder.refusals == [Refusal(.authentication(StationSession.certificateMismatchText),
                                                  code: .identityChanged)])
        #expect(rig.station.receivedTexts.isEmpty)
        #expect(await rig.session.state == .stopped)
    }

    @Test func theCoresPingsAreAnswered() async throws {
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)
        rig.station.ping()
        #expect(await eventually { rig.station.pongCount == 1 })
        await rig.session.disconnect()
    }

    @Test func theAppPingsEveryTwentySecondsAndTwoMissedPongsLoseTheLink() async throws {
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)

        // Answered: three intervals pass with the link up.
        for count in 1...3 {
            await rig.clock.advance(by: 20_000)
            #expect(await eventually { rig.station.pingCount == count })
            #expect(await eventually { await rig.session.pingsAwaitingPong == 0 })
        }
        #expect(await rig.session.state == .ready)

        // Unanswered: two pings go out, and the third tick declares the link lost.
        rig.station.setAnswersPings(false)
        await rig.clock.advance(by: 20_000)
        await rig.clock.advance(by: 20_000)
        #expect(await eventually { rig.station.pingCount == 5 })
        #expect(await rig.session.state == .ready)
        await rig.clock.advance(by: 20_000)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        await rig.session.disconnect()
    }

    @Test func messagesUpToEightMiBArriveAndLargerOnesEndTheLink() async throws {
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)

        let large = String(repeating: "x", count: 3 * 1024 * 1024)
        rig.station.send(.settingsSnapshot(LinkMessage.SettingsSnapshot(properties: [
            LinkMessage.PropertyEntry(name: "Large", value: .utf8(large)),
        ])))
        let arrived = await rig.recorder.wait { events in
            events.contains { event in
                if case .message(.settingsSnapshot(let snapshot)) = event {
                    return snapshot.properties.first?.value == .utf8(large)
                }
                return false
            }
        }
        #expect(arrived)

        rig.station.sendText(#"{"type":"settings.snapshot","properties":[{"ordinal":0,"name":"Over","kind":"utf8","value":""#
            + String(repeating: "y", count: 9 * 1024 * 1024) + #""}]}"#)
        let lost = await rig.recorder.wait { $0.contains(.stateChanged(.waitingToRetry(seconds: 1))) }
        #expect(lost)
        let overDelivered = rig.recorder.messages.contains { message in
            if case .settingsSnapshot(let snapshot) = message {
                return snapshot.properties.first?.name == "Over"
            }
            return false
        }
        #expect(!overDelivered)
        await rig.session.disconnect()
    }

    /// Builds compact JSON independently of the production codec, including
    /// its JSON overhead in the byte count. The value needs no JSON escaping.
    private func boundarySnapshot(encodedByteCount: Int, name: String) throws -> (text: String, value: String) {
        let prefix = #"{"type":"settings.snapshot","properties":[{"ordinal":0,"name":""#
            + name + #"","kind":"utf8","value":""#
        let suffix = #""}]}"#
        let overheadBytes = prefix.utf8.count + suffix.utf8.count
        let valueBytes = encodedByteCount - overheadBytes
        try #require(valueBytes > 0)

        let multibyteCharacter = "é"
        let characterBytes = multibyteCharacter.utf8.count
        let value = String(repeating: multibyteCharacter, count: valueBytes / characterBytes)
            + String(repeating: "x", count: valueBytes % characterBytes)
        let fullEncodedString = prefix + value + suffix
        try #require(fullEncodedString.utf8.count == encodedByteCount)
        try #require(fullEncodedString.count < 8 * 1024 * 1024)
        return (fullEncodedString, value)
    }

    @Test func aSnapshotOfExactlyEightMiBInUTF8ArrivesIntactAndKeepsTheLinkReady() async throws {
        let fixture = try boundarySnapshot(encodedByteCount: 8 * 1024 * 1024, name: "ExactEightMiB")
        let expected = LinkMessage.SettingsSnapshot(properties: [
            LinkMessage.PropertyEntry(ordinal: 0, name: "ExactEightMiB", value: .utf8(fixture.value)),
        ])
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)

        rig.station.sendText(fixture.text)
        let arrived = await rig.recorder.wait { $0.contains(.message(.settingsSnapshot(expected))) }
        #expect(arrived)
        #expect(await rig.session.state == .ready)
        await rig.session.disconnect()
        #expect(await eventually { await rig.session.state == .stopped })
    }

    @Test func aSnapshotOneUTF8ByteOverEightMiBEndsTheLinkWithoutDelivery() async throws {
        let fixture = try boundarySnapshot(encodedByteCount: 8 * 1024 * 1024 + 1, name: "OneByteOverEightMiB")
        let rig = try await rig()
        defer { rig.station.stop() }
        try await ready(rig)

        rig.station.sendText(fixture.text)
        let lost = await rig.recorder.wait { $0.contains(.stateChanged(.waitingToRetry(seconds: 1))) }
        #expect(lost)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        let overDelivered = rig.recorder.messages.contains { message in
            if case .settingsSnapshot(let snapshot) = message {
                return snapshot.properties.contains { $0.name == "OneByteOverEightMiB" }
            }
            return false
        }
        #expect(!overDelivered)
        await rig.session.disconnect()
        #expect(await eventually { await rig.session.state == .stopped })
    }
}
