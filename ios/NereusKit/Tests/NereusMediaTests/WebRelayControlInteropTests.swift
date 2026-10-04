// NereusSDR for iOS: selected web relay control path against frozen Core and service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

@Suite(.serialized,
       .enabled(if: LocalRendezvous.peerPath != nil && LocalRendezvous.pythonReady
                && LocalRendezvous.frozenSource != nil,
                "set NEREUS_RENDEZVOUS_PEER and NEREUS_FROZEN_RENDEZVOUS_SOURCE"))
struct WebRelayControlInteropTests {
    final class Snapshot: @unchecked Sendable {
        private let lock = NSLock()
        private var message: LinkMessage.Capabilities?
        private var sawComplete = false
        private var sawReady = false
        func take(_ value: LinkMessage.Capabilities) { lock.withLock { message = value } }
        func complete() { lock.withLock { sawComplete = true } }
        func ready() { lock.withLock { sawReady = true } }
        var value: LinkMessage.Capabilities? { lock.withLock { message } }
        var finished: Bool { lock.withLock { message != nil && sawComplete && sawReady } }
    }

    @Test func selectedWebRelayCarriesAuthenticatedControlSnapshot() async throws {
        let source = try #require(LocalRendezvous.frozenSource)
        #expect(FileManager.default.fileExists(atPath: source.appendingPathComponent("manifest.json").path))
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let marker = "relay-auth-\(UUID().uuidString)"
        let web = try await LocalRendezvous.WebRelay(in: directory, source: source)
        defer { web.stop() }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let service = try await LocalRendezvous.Service(in: directory, turn: turn, webRelay: web,
                                                        source: source, turnEnabled: false)
        defer { service.stop() }
        try await web.startFront(service: service, marker: Data(marker.utf8))
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let core = try await LocalRendezvous.Core(in: directory, service: service,
                                                  pairedDevice: device.publicKey,
                                                  authority: web.authority)
        defer { core.stop() }

        // Neither end exchanges an ordinary ICE candidate. Each must use
        // its own service-issued web relay claim to reach the other.
        let dialer = RendezvousDialer(
            servers: [service.server], stationId: core.stationId, device: device,
            transportFactory: { RendezvousWebSocket(server: $0, plainOnLoopback: true) },
            relaySocketFactory: { try await ScopedRelaySocket.connect(to: $0, authority: web.authority) },
            localFamilies: { .ipv4Only }, passesCandidate: { _ in false })
        let session = StationSession(trust: .identity(publicKey: core.identityKey),
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: marker,
                                                                                kind: .phone),
                                     transport: DataChannelSessionTransport.factory(dialer.connector))
        let snapshot = Snapshot()
        let reader = Task {
            for await event in session.events {
                switch event {
                case .message(.capabilities(let value)): snapshot.take(value)
                case .message(.snapshotComplete): snapshot.complete()
                case .stateChanged(.ready): snapshot.ready()
                default: break
                }
            }
        }
        defer { reader.cancel() }
        await session.connect()
        do {
            try await LocalRendezvous.waitUntil("authenticated complete control snapshot", within: .seconds(90)) {
                snapshot.finished || dialer.lastError != nil
            }
            #expect(snapshot.finished, "the session received snapshot.complete and reached ready")
            let version = snapshot.value?.properties.first { $0.name == "controlChannelVersion" }?.value
            #expect(version == .i64(1))
            #expect(core.sessions >= 1)
            #expect(dialer.lastPathRank == 4,
                    "rank 4 requires the selected remote candidate to equal this peer's claimed relay loopback port")
            #expect(dialer.attemptTry?.path == .relay)
            #expect(dialer.mediaIceSettings()?.relays.isEmpty == true,
                    "the service offered no TURN credentials")
            #expect(dialer.mediaRelayContext() != nil)
            #expect(turn.count("ALLOCATED") == 0)
            try await LocalRendezvous.waitUntil("encrypted web relay control frames", within: .seconds(10)) {
                web.report().controlDatagrams > 0
            }
            let report = web.report()
            #expect(report.connections >= 2, "Core and phone each opened a relay leg")
            #expect(report.controlDatagrams > 0)
            #expect(report.markerHits == 0, "the authenticated name was not plaintext in captured relay frames")
            print("WEB_RELAY_INTEROP rank=\(dialer.lastPathRank ?? -1) authenticated=\(core.sessions) relayConnections=\(report.connections) encryptedControlFrames=\(report.controlDatagrams) plaintextMarkerHits=\(report.markerHits)")
        } catch {
            await session.disconnect()
            throw error
        }
        await session.disconnect()
        #expect(dialer.lastPathRank == 4)
        #expect(dialer.attemptTry?.path == .relay)
        // The old ICE agent must release its control loopback claim after
        // native teardown. Reclaiming it proves disconnect cleanup without
        // relying on the front's cumulative connection count.
        let context = try #require(dialer.mediaRelayContext())
        let deadline = ContinuousClock.now + .seconds(10)
        var reclaimed: RelayICEClaim?
        while reclaimed == nil && ContinuousClock.now < deadline {
            reclaimed = try? await context.claimControl()
            if reclaimed == nil { try await Task.sleep(for: .milliseconds(50)) }
        }
        let claim = try #require(reclaimed, "the old control claim was not released")
        await context.releaseControl(claim)
    }
}

#endif
