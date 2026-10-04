// NereusSDR for iOS: a Core that fails the identity check never fills the mirror
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct MirrorIdentityTests {
    @Test func anImpostorsSnapshotBeforeAFailedHelloLeavesTheMirrorEmpty() async throws {
        let station = ScriptedStation()
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"),
                                     trust: TestStationIdentity().trust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                               kind: .phone),
                                     clock: ManualLinkClock(), transportFactory: station.factory)
        let mirror = MirrorStore(session: session)
        let recorder = EventRecorder(session) { event in
            await MainActor.run { mirror.handle(event) }
        }
        await session.connect()
        let transport = try #require(station.latest)
        for message in try FixtureReplay.stationMessages("session-connect-connectable")
        where message.kind != .hello && message.kind != .authResult {
            await transport.deliver(message)
        }
        let impostor = TestStationIdentity()
        await transport.deliver(.hello(LinkMessage.Hello(
            major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1], features: ["deviceAuth": 1],
            identity: try impostor.claim(certificateSHA256: station.certificateSHA256),
            challenge: TestStationIdentity.newChallenge())))
        let ended = await recorder.handled { events in
            events.contains { if case .refused = $0 { return true } else { return false } }
        }
        #expect(ended)
        #expect(recorder.refusals.first?.code == .identityChanged)
        #expect(mirror.objectKeys.isEmpty)
        #expect(!mirror.isSnapshotComplete)
        #expect(mirror.capabilities.isEmpty)
        #expect(transport.pending.isEmpty)
    }
}
