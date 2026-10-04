// NereusSDR for iOS: selected encrypted media through the Core's web relay and direct tunnel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

@Suite(.serialized,
       .enabled(if: LocalRendezvous.peerPath != nil && LocalRendezvous.pythonReady
                && LocalRendezvous.frozenSource != nil,
                "set NEREUS_RENDEZVOUS_PEER to the 5068cfca helper and NEREUS_FROZEN_RENDEZVOUS_SOURCE"))
struct MediaFloorCoreInteropTests {
    private final class CountedWebSocket: LinkTransport, @unchecked Sendable {
        private let inner: WebSocketLinkTransport
        private let lock = NSLock()
        private var outbound = 0
        private var inbound = 0
        private var badFrames = 0

        init(endpoint: StationEndpoint, trust: StationTrust) {
            inner = WebSocketLinkTransport(endpoint: endpoint, trust: trust)
        }

        var counts: (outbound: Int, inbound: Int, bad: Int) {
            lock.withLock { (outbound, inbound, badFrames) }
        }

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await inner.open(onEvent: onEvent)
        }
        @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() { inner.close() }
        func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {
            inner.setBinaryReceiver { [weak self] frame in
                self?.record(frame, outgoing: false)
                receiver?(frame)
            }
        }
        @discardableResult func sendBinary(_ frame: Data) -> Bool {
            let accepted = inner.sendBinary(frame)
            if accepted { record(frame, outgoing: true) }
            return accepted
        }
        @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
            let accepted = inner.sendBinary(frame, ownership: ownership)
            if accepted { record(frame, outgoing: true) }
            return accepted
        }
        func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }

        private func record(_ frame: Data, outgoing: Bool) {
            let marker = Data("NEREUS-PLAINTEXT-MARKER".utf8)
            lock.withLock {
                if outgoing { outbound += 1 } else { inbound += 1 }
                if frame.count < 18 || frame.count > 1501 || frame.first != 2
                    || frame.range(of: marker) != nil { badFrames += 1 }
            }
        }
    }

    private final class Observed: @unchecked Sendable {
        private let lock = NSLock()
        private var ready = false
        private var capabilities: LinkMessage.Capabilities?
        private var completeCount = 0
        private var authCount = 0
        private var mediaConnected = 0
        private var audioEnabled = 0
        private var latestAudioContext: MediaControlEvent.AudioContext?
        private var displayFrames = 0
        private var rejected: [String] = []
        private var peers: [MediaPeer] = []
        private var ticket: LinkMessage.CommandResult?
        private var commits = 0

        func take(_ event: StationSession.Event) {
            lock.withLock {
                switch event {
                case .stateChanged(.ready): ready = true
                case .message(.capabilities(let value)): capabilities = value
                case .message(.snapshotComplete): completeCount += 1
                case .message(.authResult(let value)) where value.accepted: authCount += 1
                case .message(.commandResult(let value)) where value.id == 1: ticket = value
                default: break
                }
            }
        }

        func take(_ event: MediaControlEvent) {
            lock.withLock {
                switch event {
                case .mediaState(.connected): mediaConnected += 1
                case .audioContext(let value) where value.enabled:
                    audioEnabled += 1
                    latestAudioContext = value
                case .displayFrame: displayFrames += 1
                case .rejected(let value): rejected.append(value.reason)
                default: break
                }
            }
        }

        func add(_ peer: MediaPeer) { lock.withLock { peers.append(peer) } }
        func committed() { lock.withLock { commits += 1 } }
        var snapshotReady: Bool { lock.withLock { ready && capabilities != nil && completeCount == 1 } }
        var mediaReady: Bool { lock.withLock { mediaConnected > 0 && audioEnabled > 0 } }
        var displayCount: Int { lock.withLock { displayFrames } }
        var audioContext: MediaControlEvent.AudioContext? { lock.withLock { latestAudioContext } }
        var allPeers: [MediaPeer] { lock.withLock { peers } }
        var refusalReasons: [String] { lock.withLock { rejected } }
        var pathTicket: LinkMessage.CommandResult? { lock.withLock { ticket } }
        var counts: (auth: Int, snapshot: Int, media: Int, commits: Int) {
            lock.withLock { (authCount, completeCount, mediaConnected, commits) }
        }
        var tunnelVersion: LinkMessage.PropertyValue? {
            lock.withLock { capabilities?.properties.first { $0.name == "mediaTunnelVersion" }?.value }
        }
        var routingVersion: LinkMessage.PropertyValue? {
            lock.withLock { capabilities?.properties.first { $0.name == "mediaRelayRoutingVersion" }?.value }
        }
    }

    private static func subscription() -> DisplaySubscription {
        let plane = DisplaySubscription.Plane(detector: 0, averageMode: -1, averageAlpha: 0)
        return DisplaySubscription(endpointId: 1, revision: 1, sliceId: 0, tier: .wide,
                                   fftSize: 1024, windowType: 0, centreHz: 14_225_000,
                                   spanHz: 48_000, pixels: 128, fps: 30, framesPerLine: 1,
                                   trace: plane, waterfall: plane, minDbm: -180, maxDbm: 0,
                                   wideSpanFactor: 0)
    }

    private static func renderedAudio(_ playback: AudioPlaybackCore) -> Bool {
        playback.pumpNow()
        let samples = UnsafeMutablePointer<Float>.allocate(capacity: 960 * 2)
        defer { samples.deallocate() }
        samples.initialize(repeating: 0, count: 960 * 2)
        playback.render(into: samples, frames: 960)
        return (0..<(960 * 2)).contains { abs(samples[$0]) > 0.01 }
    }

    private static func emptyRenderedAudio(_ playback: AudioPlaybackCore) {
        // More than the ring's full 8192 frames. The post-drain sample
        // therefore cannot be a packet buffered before replacement.
        let samples = UnsafeMutablePointer<Float>.allocate(capacity: 960 * 2)
        defer { samples.deallocate() }
        for _ in 0..<20 { playback.render(into: samples, frames: 960) }
    }

    private static func relayMediaDatagrams(_ web: LocalRendezvous.WebRelay) -> Int {
        guard let data = try? Data(contentsOf: web.reportFile),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let datagrams = object["datagrams"] as? [String: Int] else { return 0 }
        return datagrams["2"] ?? 0
    }

    private static func sendMarkerWhenReady(_ peer: MediaPeer) async -> Bool {
        let marker = Data("NEREUS-PLAINTEXT-MARKER".utf8)
        let deadline = ContinuousClock.now + .seconds(3)
        while ContinuousClock.now < deadline {
            if (try? peer.sendTx(marker)) != nil { return true }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return false
    }

    @Test func selectedRelayThenDirectTunnelCarryDecodedMedia() async throws {
        let source = try #require(LocalRendezvous.frozenSource)
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let web = try await LocalRendezvous.WebRelay(in: directory, source: source)
        defer { web.stop() }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let service = try await LocalRendezvous.Service(in: directory, turn: turn, webRelay: web,
                                                        source: source, turnEnabled: false)
        defer { service.stop() }
        try await web.startFront(service: service, marker: Data("NEREUS-PLAINTEXT-MARKER".utf8))
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let listenPort = try LocalRendezvous.freeTcpPort()
        let core = try await LocalRendezvous.Core(in: directory, service: service,
                                                  pairedDevice: device.publicKey,
                                                  authority: web.authority,
                                                  listenPort: listenPort, listenLoopback: true,
                                                  media: true)
        defer { core.stop() }
        let dialer = RendezvousDialer(
            servers: [service.server], stationId: core.stationId, device: device,
            transportFactory: { RendezvousWebSocket(server: $0, plainOnLoopback: true) },
            relaySocketFactory: { try await ScopedRelaySocket.connect(to: $0, authority: web.authority) },
            localFamilies: { .ipv4Only }, passesCandidate: { _ in false })
        let session = StationSession(trust: .identity(publicKey: core.identityKey),
                                     authenticator: try DeviceKeyAuthenticator(identity: device,
                                                                               name: "Media floor phone", kind: .phone),
                                     transport: DataChannelSessionTransport.factory(dialer.connector))
        let seen = Observed()
        let playback = try AudioPlaybackCore()
        playback.start()
        defer { playback.stop() }
        await session.setWaitsForMedia(true)
        let media = MediaControlClient(send: { message in
            // Only each peer's internally claimed loopback candidate may win
            // media ICE in this loopback fixture. Ordinary trickle is dropped
            // in both directions; answers/offers must have no embedded ICE.
            if case .mediaControl(let control) = message {
                if control.payload["op"] == .string("candidate") { return }
                if control.payload["op"] == .string("description"),
                   case .string(let sdp) = control.payload["sdp"] {
                    #expect(!MediaPeer.embedsCandidates(sdp), "phone media answers must trickle ICE")
                }
            }
            try await session.send(message)
        }, peerFactory: {
            let settings = dialer.mediaIceSettings()
            let peer = MediaPeer(configuration: .init(iceServers: settings?.libdatachannelServers ?? [],
                                                      mtu: IceSettings.mtu, hostCandidatesOnly: false,
                                                      refusesRelayCandidates: false),
                                 route: dialer.mediaRelayContext().map(MediaPeer.Route.relay) ?? .direct)
            seen.add(peer)
            return peer
        }, playback: playback, onMediaConnected: { await session.mediaConnectionUp() })
        let reader = Task {
            for await event in session.events {
                seen.take(event)
                if case .message(.mediaControl(let control)) = event {
                    if control.payload["op"] == .string("candidate") { continue }
                    if control.payload["op"] == .string("description"),
                       case .string(let sdp) = control.payload["sdp"] {
                        #expect(!MediaPeer.embedsCandidates(sdp), "Core media offers must trickle ICE")
                    }
                }
                await media.handle(event)
            }
        }
        let mediaReader = Task { for await event in media.events { seen.take(event) } }
        let tunnel = MediaTunnelContext(session: session)
        func retireFixture() async {
            // Stop the sole ordered session reader before injecting the
            // terminal media event; no old event may restart media afterward.
            reader.cancel()
            await reader.value
            mediaReader.cancel()
            await mediaReader.value
            await session.disconnect()
            await media.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            for peer in seen.allPeers { await peer.closeWhenDeleted() }
        }
        await media.setAudioEnabled(true)
        await session.connect()
        do {
            try await LocalRendezvous.waitUntil("the authenticated media snapshot", within: .seconds(90),
                                                whileAlive: { core.exitDiagnostic }) {
                seen.snapshotReady || dialer.lastError != nil
            }
            #expect(seen.snapshotReady)
            #expect(dialer.lastPathRank == 4)
            #expect(seen.tunnelVersion == .i64(1) && seen.routingVersion == .i64(1))
            try await LocalRendezvous.waitUntil("the selected web relay media peer", within: .seconds(45),
                                                whileAlive: { core.exitDiagnostic }) {
                seen.allPeers.first?.selectedRelayOrTunnel == true && seen.mediaReady
            }
            let first = try #require(seen.allPeers.first)
            #expect(first.selectedRelayOrTunnel)
            #expect(!first.selectedTunnel)
            #expect(turn.count("ALLOCATED") == 0)
            try await media.subscribe(Self.subscription())
            try await LocalRendezvous.waitUntil("decoded display and Opus playback", within: .seconds(20),
                                                whileAlive: { core.exitDiagnostic }) {
                seen.displayCount > 0 && first.receiveCounts.audio.accepted > 10
                    && Self.renderedAudio(playback)
            }
            #expect(seen.displayCount > 0)
            #expect(first.receiveCounts.audio.accepted > 10)
            #expect(seen.refusalReasons.isEmpty, "\(seen.refusalReasons)")
            let relayMarkerSent = await Self.sendMarkerWhenReady(first)
            #expect(relayMarkerSent)
            try await Task.sleep(for: .milliseconds(200))
            let report = web.report()
            #expect(report.markerHits == 0)
            #expect(Self.relayMediaDatagrams(web) > 0)
            #expect(seen.counts.auth == 1 && seen.counts.snapshot == 1)
            let oldId = try #require(await media.connectionId)
            let initialFrames = seen.displayCount
            let initialAudioPackets = first.receiveCounts.audio.accepted
            print("MEDIA_FLOOR_RELAY rank=\(dialer.lastPathRank ?? -1) selectedRelay=\(first.selectedRelayOrTunnel) audioPackets=\(initialAudioPackets) displayFrames=\(initialFrames) mediaDatagrams=\(Self.relayMediaDatagrams(web)) markerSent=\(relayMarkerSent) auth=\(seen.counts.auth) snapshots=\(seen.counts.snapshot)")

            // One authenticated session crosses the Core's path.switch
            // barrier. Its new media peer must use the same session's binary
            // WSS tunnel, while OLD relay media drains for two seconds.
            let relayContext = try #require(dialer.mediaRelayContext())
            let directIce = try #require(dialer.mediaIceSettings()).forMedia(controlPathRelayed: false)
            #expect(directIce.relays.isEmpty)
            let counted = CountedWebSocket(endpoint: StationEndpoint(host: "127.0.0.1", port: listenPort),
                                           trust: .identity(publicKey: core.identityKey))
            let direct = PreauthenticatedTransport(counted)
            let (hello, digest) = try await direct.inspect(clock: SystemLinkClock(), deadline: .seconds(30))
            #expect(StationTrust.identityVerifies(hello.identity, publicKey: core.identityKey,
                                                  certificateSHA256: digest))
            let moved = await session.moveControl(to: direct, safetyCheck: { true }, requestTicket: {
                try await session.send(.commandInvoke(.init(verb: "session.pathTicket", id: 1, args: [])))
                try await LocalRendezvous.waitUntil("the exact path ticket", within: .seconds(5)) {
                    seen.pathTicket != nil
                }
                guard let result = seen.pathTicket, result.verb == "session.pathTicket",
                      result.id == 1, result.accepted, result.reason.isEmpty,
                      result.affected.isEmpty, let values = result.values, values.count == 2,
                      let secret = values.first(where: { $0.ordinal == 0 && $0.name == "ticket" }),
                      let expiry = values.first(where: { $0.ordinal == 1 && $0.name == "expiresInMs" }),
                      case .utf8(let key) = secret.value, case .i64(let milliseconds) = expiry.value,
                      let ticket = PathTicket(secret: key, expiresInMs: milliseconds) else {
                    throw LocalRendezvous.Failure(description: "Core returned an invalid path ticket")
                }
                return ticket
            }, onRouteCommit: {
                seen.committed()
                await media.controlRouteDidMove(peerFactory: {
                    let peer = MediaPeer(configuration: .init(iceServers: directIce.libdatachannelServers,
                                                              mtu: IceSettings.mtu, hostCandidatesOnly: false),
                                         route: .tunnel(tunnel))
                    seen.add(peer)
                    return peer
                }, connectDeadline: .seconds(30), safeToReplace: { true })
            })
            #expect(moved)
            let replacementDeadline = ContinuousClock.now + .seconds(30)
            var replacementId: String?
            while replacementId == nil && ContinuousClock.now < replacementDeadline {
                if let second = seen.allPeers.dropFirst().first,
                   let id = await media.connectionId, id != oldId, second.selectedTunnel {
                    replacementId = id
                } else {
                    if let failure = core.exitDiagnostic {
                        throw LocalRendezvous.Failure(description: "direct replacement: \(failure)")
                    }
                    try await Task.sleep(for: .milliseconds(50))
                }
            }
            let second = try #require(seen.allPeers.dropFirst().first)
            let newId = try #require(replacementId, "the direct tunnel replacement was not acknowledged")
            #expect(newId != oldId && second.selectedTunnel)
            try await LocalRendezvous.waitUntil("NEW tunnel RTP and display datagrams", within: .seconds(20),
                                                whileAlive: { core.exitDiagnostic }) {
                second.receiveCounts.audio.accepted > 10
                    && second.receiveCounts.display.accepted > 0
                    && counted.counts.inbound > 0 && counted.counts.outbound > 0
            }
            // Let OLD's bounded drain finish and its native ICE agent retire.
            try await Task.sleep(for: .seconds(3))
            let relayContextId = try #require(UUID(uuidString: oldId))
            let deadline = ContinuousClock.now + .seconds(10)
            var reclaimed: RelayICEClaim?
            while reclaimed == nil && ContinuousClock.now < deadline {
                reclaimed = try? await relayContext.claimMedia(connectionId: relayContextId)
                if reclaimed == nil { try await Task.sleep(for: .milliseconds(50)) }
            }
            let claim = try #require(reclaimed, "OLD media relay UUID was not reclaimed")
            await relayContext.releaseMedia(claim)

            // The equivalent stream keeps its original timing fields on
            // replace; Core does not send a second audio-context. For this
            // proof only, restart the decoder with NEW's deterministic SSRC
            // so neither held Opus nor rendered OLD PCM can satisfy it.
            let timing = try #require(seen.audioContext?.anchor)
            let newAnchor = AudioStreamAnchor(generation: timing.generation,
                                              ssrc: MediaControlClient.audioSsrc(forConnection: newId),
                                              firstSequence: timing.firstSequence,
                                              firstTimestamp: timing.firstTimestamp)
            playback.reanchor(nil)
            playback.flush()
            Self.emptyRenderedAudio(playback)
            playback.reanchor(newAnchor)
            playback.flush()
            let afterDrainAudioPackets = second.receiveCounts.audio.accepted
            let afterDrainFrames = seen.displayCount
            try await LocalRendezvous.waitUntil("post-drain decoded display and PCM", within: .seconds(15),
                                                whileAlive: { core.exitDiagnostic }) {
                seen.displayCount > afterDrainFrames
                    && second.receiveCounts.audio.accepted > afterDrainAudioPackets + 10
                    && Self.renderedAudio(playback)
            }
            #expect(seen.counts.auth == 1 && seen.counts.snapshot == 1 && seen.counts.commits == 1)
            #expect(core.sessions == 1)
            let tunnelMarkerSent = await Self.sendMarkerWhenReady(second)
            #expect(tunnelMarkerSent)
            try await Task.sleep(for: .milliseconds(200))
            #expect(counted.counts.bad == 0)
            #expect(seen.refusalReasons.isEmpty, "\(seen.refusalReasons)")
            print("MEDIA_FLOOR_TUNNEL moved=\(moved) selectedTunnel=\(second.selectedTunnel) newAudioPackets=\(second.receiveCounts.audio.accepted) newDisplayDatagrams=\(second.receiveCounts.display.accepted) postDrainFrames=\(seen.displayCount - afterDrainFrames) binaryOut=\(counted.counts.outbound) binaryIn=\(counted.counts.inbound) markerSent=\(tunnelMarkerSent) auth=\(seen.counts.auth) snapshots=\(seen.counts.snapshot)")

            await retireFixture()
            let tunnelDeadline = ContinuousClock.now + .seconds(10)
            var tunnelReclaimed: MediaTunnelClaim?
            while tunnelReclaimed == nil && ContinuousClock.now < tunnelDeadline {
                tunnelReclaimed = try? await tunnel.claimMedia(connectionId: UUID(uuidString: newId)!)
                if tunnelReclaimed == nil { try await Task.sleep(for: .milliseconds(50)) }
            }
            let tunnelClaim = try #require(tunnelReclaimed, "NEW tunnel UUID was not reclaimed")
            await tunnel.releaseMedia(tunnelClaim)
            await tunnel.close()
            print("MEDIA_FLOOR_CLEANUP oldRelayUUIDReclaimed=true newTunnelUUIDReclaimed=true")
        } catch {
            let peer = seen.allPeers.first
            let report = web.report()
            print("MEDIA_FLOOR_DIAGNOSTIC mediaConnected=\(seen.counts.media) audioEnabled=\(seen.mediaReady) peerCount=\(seen.allPeers.count) audioPackets=\(peer?.receiveCounts.audio.accepted ?? -1) displayFrames=\(seen.displayCount) refusals=\(seen.refusalReasons) controlDatagrams=\(report.controlDatagrams) mediaDatagrams=\(Self.relayMediaDatagrams(web)) markerHits=\(report.markerHits) core=\(core.exitDiagnostic ?? "running")")
            await retireFixture()
            await tunnel.close()
            print("MEDIA_FLOOR_FAILURE_CLEANUP nativePeersRetired=\(seen.allPeers.count) tunnelClosed=true")
            throw error
        }
    }
}

#endif
