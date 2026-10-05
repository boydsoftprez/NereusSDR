// NereusSDR for iOS: move one authenticated Core session from selected web relay to direct WSS
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

@Suite(.serialized,
       .enabled(if: LocalRendezvous.peerPath != nil && LocalRendezvous.pythonReady
                && LocalRendezvous.frozenSource != nil,
                "set NEREUS_RENDEZVOUS_PEER to the 5068cfca loopback helper and NEREUS_FROZEN_RENDEZVOUS_SOURCE"))
struct ControlSwitchCoreInteropTests {
    /// The fixture owns its two command IDs and matches only the exact result.
    /// It also keeps the received snapshot in one place across the switch.
    private final class Observed: @unchecked Sendable {
        private let lock = NSLock()
        private var capabilities: LinkMessage.Capabilities?
        private var completeCount = 0
        private var readyCount = 0
        private var acceptedAuthCount = 0
        private var objects: Set<String> = []
        private var results: [UInt32: LinkMessage.CommandResult] = [:]
        private var commitCount = 0

        func take(_ event: StationSession.Event) {
            lock.withLock {
                switch event {
                case .message(.capabilities(let value)): capabilities = value
                case .message(.authResult(let value)) where value.accepted: acceptedAuthCount += 1
                case .message(.snapshotComplete): completeCount += 1
                case .message(.objectCreate(let value)): objects.insert(value.key)
                case .message(.objectDestroy(let value)): objects.remove(value.key)
                case .message(.commandResult(let value)) where value.id == 1 || value.id == 2:
                    results[value.id] = value
                case .stateChanged(.ready): readyCount += 1
                default: break
                }
            }
        }

        func committed() { lock.withLock { commitCount += 1 } }
        var ready: Bool { lock.withLock { capabilities != nil && completeCount == 1 && readyCount == 1 } }
        var switchVersion: LinkMessage.PropertyValue? {
            lock.withLock { capabilities?.properties.first { $0.name == "controlSwitchVersion" }?.value }
        }
        var objectKeys: Set<String> { lock.withLock { objects } }
        var counts: (auth: Int, snapshot: Int, ready: Int, commit: Int) {
            lock.withLock { (acceptedAuthCount, completeCount, readyCount, commitCount) }
        }
        func result(_ id: UInt32) -> LinkMessage.CommandResult? { lock.withLock { results[id] } }
    }

    @Test func selectedRelayMovesToDirectCoreWithoutSecondSignInOrSnapshot() async throws {
        let source = try #require(LocalRendezvous.frozenSource)
        #expect(FileManager.default.fileExists(atPath: source.appendingPathComponent("manifest.json").path))
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let web = try await LocalRendezvous.WebRelay(in: directory, source: source)
        defer { web.stop() }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let service = try await LocalRendezvous.Service(in: directory, turn: turn, webRelay: web,
                                                        source: source, turnEnabled: false)
        defer { service.stop() }
        try await web.startFront(service: service, marker: Data("switch-interop-\(UUID())".utf8))
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let listenPort = try LocalRendezvous.freeTcpPort()
        let core = try await LocalRendezvous.Core(in: directory, service: service,
                                                  pairedDevice: device.publicKey,
                                                  authority: web.authority,
                                                  listenPort: listenPort, listenLoopback: true)
        defer { core.stop() }

        // Suppress both directions' ordinary candidates. The selected rank,
        // not the existence of relay connections, proves the initial route.
        let dialer = RendezvousDialer(
            servers: [service.server], stationId: core.stationId, device: device,
            transportFactory: { RendezvousWebSocket(server: $0, plainOnLoopback: true) },
            relaySocketFactory: { try await ScopedRelaySocket.connect(to: $0, authority: web.authority) },
            localFamilies: { .ipv4Only }, passesCandidate: { _ in false })
        let session = StationSession(trust: .identity(publicKey: core.identityKey),
                                     authenticator: try DeviceKeyAuthenticator(identity: device,
                                                                               name: "Switch interop phone",
                                                                               kind: .phone),
                                     transport: DataChannelSessionTransport.factory(dialer.connector))
        let seen = Observed()
        let reader = Task {
            for await event in session.events { seen.take(event) }
        }
        defer { reader.cancel() }
        await session.connect()
        do {
            try await LocalRendezvous.waitUntil("the initial Core snapshot", within: .seconds(90),
                                                whileAlive: { core.exitDiagnostic }) {
                seen.ready || dialer.lastError != nil
            }
            #expect(seen.ready)
            #expect(seen.switchVersion == .i64(1))
            #expect(dialer.lastPathRank == 4)
            #expect(dialer.attemptTry?.path == .relay)
            #expect(core.sessions == 1)
            #expect(turn.count("ALLOCATED") == 0)
            let initialKeys = seen.objectKeys
            #expect(!initialKeys.isEmpty, "the mirrored Core snapshot contains objects")

            let direct = PreauthenticatedTransport(WebSocketLinkTransport(
                endpoint: StationEndpoint(host: "127.0.0.1", port: listenPort),
                trust: .identity(publicKey: core.identityKey)))
            let (hello, digest) = try await direct.inspect(clock: SystemLinkClock(), deadline: .seconds(30))
            #expect(StationTrust.identityVerifies(hello.identity, publicKey: core.identityKey,
                                                  certificateSHA256: digest))
            let moved = await session.moveControl(to: direct, safetyCheck: { true }, requestTicket: {
                try await session.send(.commandInvoke(.init(verb: "session.pathTicket", id: 1, args: [])))
                try await LocalRendezvous.waitUntil("the correlated path ticket", within: .seconds(5)) {
                    seen.result(1) != nil
                }
                guard let result = seen.result(1), result.verb == "session.pathTicket",
                      result.id == 1, result.accepted, result.reason.isEmpty,
                      result.affected.isEmpty, let values = result.values, values.count == 2,
                      let secretField = values.first(where: { $0.ordinal == 0 && $0.name == "ticket" }),
                      let expiryField = values.first(where: { $0.ordinal == 1 && $0.name == "expiresInMs" }),
                      case .utf8(let secret) = secretField.value,
                      case .i64(let expiry) = expiryField.value,
                      let ticket = PathTicket(secret: secret, expiresInMs: expiry) else {
                    throw LocalRendezvous.Failure(description: "Core returned an invalid path ticket")
                }
                return ticket
            }, onRouteCommit: {
                seen.committed()
            })
            #expect(moved, "Core and phone completed the OLD path.switch barrier")
            #expect(seen.counts.commit == 1, "the route context was committed")
            #expect(seen.objectKeys == initialKeys, "the mirror survived without replacement")

            // An unknown read-only test verb is safely refused by Core. Its
            // matching reply on NEW proves the active authenticated route.
            try await session.send(.commandInvoke(.init(verb: "interop.noSuchRead", id: 2, args: [])))
            try await LocalRendezvous.waitUntil("an authenticated NEW-path reply", within: .seconds(5)) {
                seen.result(2) != nil
            }
            let reply = try #require(seen.result(2))
            #expect(reply.id == 2 && reply.verb == "interop.noSuchRead" && !reply.accepted)
            #expect(!reply.reason.isEmpty)
            #expect(seen.counts.auth == 1 && seen.counts.snapshot == 1 && seen.counts.ready == 1)
            #expect(core.sessions == 1, "Core authenticated the device only once")
            #expect(seen.objectKeys == initialKeys)

            // Core closes OLD after the phone's barrier, releasing the old
            // ICE control claim while the direct session stays alive.
            let context = try #require(dialer.mediaRelayContext())
            let deadline = ContinuousClock.now + .seconds(10)
            var reclaimed: RelayICEClaim?
            while reclaimed == nil && ContinuousClock.now < deadline {
                reclaimed = try? await context.claimControl()
                if reclaimed == nil { try await Task.sleep(for: .milliseconds(50)) }
            }
            let claim = try #require(reclaimed, "OLD relay control claim was not released")
            await context.releaseControl(claim)
            print("CONTROL_SWITCH_CORE_INTEROP initialRank=\(dialer.lastPathRank ?? -1) auth=\(core.sessions) snapshots=\(seen.counts.snapshot) commits=\(seen.counts.commit) oldClaimReclaimed=true")
        } catch {
            await session.disconnect()
            throw error
        }
        await session.disconnect()
    }
}

#endif
