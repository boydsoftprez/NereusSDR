// NereusSDR for iOS: deterministic observed-route diagnostics and carrier await boundaries
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import NereusKitTesting
import NereusLink
import NereusMedia
import Testing
@testable import NereusSDR

@Suite("Observed route diagnostics", .serialized)
@MainActor
struct LinkDiagnosticsCollectorTests {
    private final class Recorded: @unchecked Sendable {
        private let lock = NSLock()
        private var values: [LinkDiagnostics.Event] = []
        func append(_ event: LinkDiagnostics.Event) { lock.withLock { values.append(event) } }
        var events: [LinkDiagnostics.Event] { lock.withLock { values } }
        var media: [LinkDiagnostics.Event] { events.filter {
            if case .observedRoute(connection: .media, kind: _, rank: _, milliseconds: _) = $0 { return true }
            return false
        } }
        var control: [LinkDiagnostics.Event] { events.filter {
            if case .observedRoute(connection: .control, kind: _, rank: _, milliseconds: _) = $0 { return true }
            return false
        } }
    }

    private actor Barrier {
        private var continuation: CheckedContinuation<Void, Never>?
        private(set) var entered = false
        private(set) var completed = false
        func pause() async {
            entered = true
            await withCheckedContinuation { continuation = $0 }
            completed = true
        }
        func resume() { continuation?.resume(); continuation = nil }
    }

    private final class PeerHolder: @unchecked Sendable {
        private let lock = NSLock()
        private var peer: TunnelPeer
        init(_ peer: TunnelPeer) { self.peer = peer }
        var current: TunnelPeer { lock.withLock { peer } }
        func replace(_ peer: TunnelPeer) { lock.withLock { self.peer = peer } }
    }

    // Only the typed observation differs from the existing no-network fake peer.
    // The app still creates/owns its real MediaTunnelContext and MediaControlClient.
    private final class TunnelPeer: MediaPeerConnection, @unchecked Sendable {
        private let inner = FakeMediaPeer()
        private let lock = NSLock()
        private var reading: SelectedRouteObservation
        init(route: SelectedRouteObservation) { reading = route }
        var localDescription: AsyncStream<String> { inner.localDescription }
        var localCandidates: AsyncStream<(candidate: String, mid: String)> { inner.localCandidates }
        var audioPackets: AsyncStream<RtpPacket> { inner.audioPackets }
        var displayDatagrams: AsyncStream<Data> { inner.displayDatagrams }
        var state: AsyncStream<MediaPeer.State> { inner.state }
        var microphoneLineClosed: AsyncStream<Void> { inner.microphoneLineClosed }
        var selectedTunnel: Bool { lock.withLock { if case .available(let route) = reading { return route.kind == .wssTunnel }; return false } }
        var selectedRelayOrTunnel: Bool { lock.withLock {
            if case .available(let route) = reading { return [.wssTunnel, .turnRelay, .webRelay].contains(route.kind) }
            return false
        } }
        var selectedRouteObservation: SelectedRouteObservation { lock.withLock { reading } }
        var trafficObservation: MediaTrafficObservation? { inner.trafficObservation }
        func observe(_ route: SelectedRouteObservation) { lock.withLock { reading = route } }
        func setRemoteDescription(_ description: String) throws { try inner.setRemoteDescription(description) }
        func addRemoteCandidate(_ candidate: String, mid: String) throws { try inner.addRemoteCandidate(candidate, mid: mid) }
        func setExpectedAudioSsrc(_ ssrc: UInt32?) { inner.setExpectedAudioSsrc(ssrc) }
        func close() { inner.close() }
    }

    private final class ObservingTransport: LinkTransport, @unchecked Sendable {
        private let inner: any LinkTransport
        private let lock = NSLock()
        private var reading: SelectedRouteObservation = .unavailable(.noSelectedPair)
        init(_ inner: any LinkTransport) { self.inner = inner }
        var selectedRouteObservation: SelectedRouteObservation { lock.withLock { reading } }
        func observe(_ value: SelectedRouteObservation) { lock.withLock { reading = value } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data { try await inner.open(onEvent: onEvent) }
        func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() { inner.close() }
        func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { inner.setBinaryReceiver(receiver) }
        func sendBinary(_ frame: Data) -> Bool { inner.sendBinary(frame) }
        func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool { inner.sendBinary(frame, ownership: ownership) }
        func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }
        var trafficObservation: LinkTrafficObservation? { inner.trafficObservation }
    }

    private final class Service: CoreServiceRoute, @unchecked Sendable {
        let transport: ObservingTransport
        init(_ transport: ObservingTransport) { self.transport = transport }
        func makeTransport() -> any SessionTransport { transport }
        func mediaIceSettings() -> IceSettings? { nil }
        var lastTry: ConnectionAttempt.Try? { nil }
        var lastError: RendezvousDialError? { nil }
    }

    // A lease-inspection/move stand-in following SessionTransportSwitchTests.
    // It invokes only in-memory LinkTransport callbacks and stores counts, never text logs.
    private final class Candidate: LinkTransport, @unchecked Sendable {
        private let digest: Data
        private let hello: LinkMessage.Hello
        private let lock = NSLock()
        private var closed = false
        private var count = 0
        init(station: FakeStation, minor: UInt16) throws {
            let digest = Data(repeating: 9, count: 32)
            self.digest = digest
            hello = LinkMessage.Hello(major: 1, minor: minor, settingsSchema: 0,
                peer: "nereusd", majors: [1], identity: try station.identity.claim(certificateSHA256: digest))
        }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            await onEvent(.text(LinkCodec.encode(.hello(hello))))
            return digest
        }
        @discardableResult func send(_ text: String) -> Bool {
            lock.withLock { guard !closed else { return false }; count += 1; return true }
        }
        func ping() {}
        func close() { lock.withLock { closed = true } }
        var sentCount: Int { lock.withLock { count } }
    }

    private struct Rig {
        let station: FakeStation
        let app: AppModel
        let collector: ConnectionPerformanceModel
        let session: StationSession
        let peer: TunnelPeer
        let owner: UInt64
        let clock: TestLinkClock
        let recorded: Recorded
        let observedControl: ObservingTransport?
        let peers: PeerHolder
    }

    // Builds a numeric fixture endpoint from Darwin's typed loopback constant.
    // No literal endpoint or generated value is printed, logged or sent.
    private func route(kind: SelectedRouteKind = .wssTunnel,
                       remote: SelectedICECandidateType? = nil, identity: UInt16 = 1) throws -> SelectedRouteObservation {
        var address = in6addr_loopback
        var text = [CChar](repeating: 0, count: Int(INET6_ADDRSTRLEN))
        let size = socklen_t(text.count)
        let result = withUnsafePointer(to: &address) { inet_ntop(AF_INET6, $0, &text, size) }
        _ = try #require(result)
        let endpoint = try #require(NumericRouteEndpoint(address: String(cString: text), port: identity))
        return .available(.init(kind: kind, transport: .iceUDP, selectedEndpoint: endpoint,
            endpointRole: kind == .wssTunnel ? .claimedLocalShim : .nominatedICEPeer,
            coreEndpoint: nil, remoteCandidateType: remote))
    }

    private func settle(_ condition: () async -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if await condition() { return true }
            await Task.yield()
        }
        return await condition()
    }

    private func ready(service: Bool = false, mediaRoute: SelectedRouteObservation? = nil) async throws -> Rig {
        let station = try FakeStation()
        let clock = TestLinkClock()
        let recorded = Recorded()
        let peer = TunnelPeer(route: try mediaRoute ?? route())
        let peers = PeerHolder(peer)
        let app = AppModel(mediaPeerFactory: { peers.current }, remoteMediaPeerFactory: { _ in peers.current }, mirrorClock: clock)
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let collector = ConnectionPerformanceModel(app: app, playback: nil,
            nowMilliseconds: { clock.nowMilliseconds }, diagnostics: diagnostics)
        app.connectionPerformance = collector
        let observedControl: ObservingTransport?
        if service {
            let transport = ObservingTransport(station.transportFactory(station.endpoint, station.identityTrust))
            observedControl = transport
            await app.connect(through: Service(transport), name: nil, trust: station.identityTrust,
                authenticator: station.authenticator, clock: clock)
        } else {
            observedControl = nil
            await app.connect(to: station.endpoint, trust: station.identityTrust,
                authenticator: station.authenticator, transportFactory: station.transportFactory, clock: clock)
        }
        _ = try #require(await settle { app.connection == .connected && collector.inFlightArmForTesting == nil })
        collector.stopAutomaticTicksForTesting()
        let session = try #require(app.session)
        let owner = try #require(app.mediaOwnerForTesting)
        // Preserve the media gate while explicitly offering control-switch version 1.
        await station.deliver(.capabilities(.init(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "controlSwitchVersion", value: .i64(1)),
        ])))
        #expect(await session.agreedMinor == 11)
        // Connected control and assigned ownership precede admitted media.
        // Wait for the real current peer before asking its collector to sample.
        _ = try #require(await settle {
            let current = await app.media.selectedMediaDiagnostics(owner: owner)
            if case .available = current.route { return current.mediaID != nil && app.mirror.isSnapshotComplete }
            return false
        })
        #expect(await settle { collector.inFlightArmForTesting == nil })
        return Rig(station: station, app: app, collector: collector, session: session,
                   peer: peer, owner: owner, clock: clock, recorded: recorded, observedControl: observedControl, peers: peers)
    }

    private func sample(_ rig: Rig) async throws {
        rig.collector.tickForTesting()
        _ = try #require(await settle { rig.collector.inFlightArmForTesting == nil })
    }

    @Test func retiredOwnerReadCannotLogOrClearTheNewOwnersMarker() async throws {
        let rig = try await ready()
        defer { rig.collector.close(); Task { await rig.app.disconnect() } }
        try await sample(rig)
        _ = try #require(rig.recorded.media.count == 1)
        rig.collector.restartObservationWindowForTesting()
        let old = Barrier()
        let new = Barrier()
        let warming = Barrier()
        defer { Task { await old.resume(); await new.resume(); await warming.resume() } }
        rig.collector.finalMediaReadForTesting = {
            await old.pause()
            return await rig.app.media.selectedMediaDiagnostics(owner: rig.owner)
        }
        rig.collector.tickForTesting()
        _ = try #require(await settle { await old.entered })
        let oldMarker = rig.collector.inFlightArmForTesting
        await rig.app.disconnect()
        rig.collector.finalMediaReadForTesting = nil
        // Hold NEW's automatic bootstrap read while its media is admitted.
        rig.collector.beforePublicationForTesting = { await warming.pause() }
        let next = try FakeStation()
        rig.peers.replace(TunnelPeer(route: try route()))
        await rig.app.connect(to: next.endpoint, trust: next.identityTrust, authenticator: next.authenticator,
            transportFactory: next.transportFactory, clock: rig.clock)
        _ = try #require(await settle { rig.app.connection == .connected })
        let nextOwner = try #require(rig.app.mediaOwnerForTesting)
        await next.deliver(.capabilities(.init(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "controlSwitchVersion", value: .i64(1)),
        ])))
        _ = try #require(await settle {
            let current = await rig.app.media.selectedMediaDiagnostics(owner: nextOwner)
            if case .available = current.route { return current.mediaID != nil }
            return false
        })
        rig.collector.beforePublicationForTesting = nil
        rig.collector.restartObservationWindowForTesting()
        await warming.resume()
        rig.collector.beforePublicationForTesting = { await new.pause() }
        rig.collector.tickForTesting()
        _ = try #require(await settle { await new.entered })
        let newMarker = rig.collector.inFlightArmForTesting
        _ = try #require(newMarker != nil && newMarker != oldMarker)
        await old.resume()
        _ = try #require(await settle { await old.completed })
        for _ in 0..<1000 { await Task.yield() }
        #expect(rig.recorded.media.count == 1)
        #expect(rig.collector.inFlightArmForTesting == newMarker)
        rig.collector.beforePublicationForTesting = nil
        await new.resume()
        _ = try #require(await settle { rig.collector.inFlightArmForTesting == nil })
        #expect(rig.recorded.media.count == 2)
        #expect(rig.recorded.media.last == .observedRoute(connection: .media, kind: .wssTunnel,
            rank: .otherWebSocket, milliseconds: 0))
        try await sample(rig)
        #expect(rig.recorded.media.count == 2)
    }

    @Test func unavailableTunnelRankDoesNotConsumeFutureKnownObservation() async throws {
        let rig = try await ready(service: true)
        defer { rig.collector.close(); Task { await rig.app.disconnect() } }
        let transport = try #require(rig.observedControl)
        let before = await rig.session.diagnosticsSnapshot()
        try await sample(rig)
        #expect(rig.recorded.media.isEmpty)
        transport.observe(try route(kind: .direct, remote: .host))
        try await sample(rig)
        let after = await rig.session.diagnosticsSnapshot()
        #expect(after.attemptGeneration == before.attemptGeneration && after.routeID == before.routeID)
        #expect(rig.recorded.media == [.observedRoute(connection: .media, kind: .wssTunnel,
            rank: .serviceDirect, milliseconds: 0)])
        try await sample(rig)
        #expect(rig.recorded.media.count == 1)
    }

    @Test func controlAndDirectMediaChangesHaveIndependentPrivateIdentities() async throws {
        let rig = try await ready(service: true, mediaRoute: route(kind: .direct, remote: .prflx))
        defer { rig.collector.close(); Task { await rig.app.disconnect() } }
        let transport = try #require(rig.observedControl)
        transport.observe(try route(kind: .direct, remote: .host))
        try await sample(rig)
        #expect(rig.recorded.control.last == .observedRoute(connection: .control, kind: .host, rank: .serviceDirect, milliseconds: 0))
        #expect(rig.recorded.media.last == .observedRoute(connection: .media, kind: .peerReflexive, rank: .serviceDirect, milliseconds: 0))
        let mediaCount = rig.recorded.media.count
        let controlCount = rig.recorded.control.count
        transport.observe(try route(kind: .direct, remote: .host, identity: 2))
        try await sample(rig)
        #expect(rig.recorded.control.count == controlCount + 1)
        #expect(rig.recorded.media.count == mediaCount)
        rig.peer.observe(try route(kind: .webRelay, remote: .relay))
        try await sample(rig)
        #expect(rig.recorded.control.count == controlCount + 1)
        #expect(rig.recorded.media.last == .observedRoute(connection: .media, kind: .webRelay, rank: .webRelay, milliseconds: 0))
        try await sample(rig)
        #expect(rig.recorded.media.count == mediaCount + 1)
        for (type, expected) in [(SelectedICECandidateType.srflx, LinkDiagnostics.RouteKind.serverReflexive), (.prflx, .peerReflexive)] {
            transport.observe(try route(kind: .direct, remote: type))
            try await sample(rig)
            #expect(rig.recorded.control.last == .observedRoute(connection: .control, kind: expected, rank: .serviceDirect, milliseconds: 0))
        }
        transport.observe(try route(kind: .turnRelay, remote: .relay))
        try await sample(rig)
        #expect(rig.recorded.control.last == .observedRoute(connection: .control, kind: .relay, rank: .serviceRelayed, milliseconds: 0))
    }

    @Test func connectedTunnelEmitsOnceAndSameObservationIsDeduplicated() async throws {
        let rig = try await ready()
        defer { rig.collector.close(); Task { await rig.app.disconnect() } }
        try await sample(rig)
        #expect(rig.recorded.media == [.observedRoute(connection: .media, kind: .wssTunnel,
            rank: .otherWebSocket, milliseconds: 0)])
        try await sample(rig)
        #expect(rig.recorded.media.count == 1)
    }

    @Test(arguments: [LinkDiagnostics.RouteRank.localWebSocket, .otherWebSocket])
    func moveDuringFinalMediaAwaitSuppressesOldCarrierThenEmitsNewCarrierOnce(newRank: LinkDiagnostics.RouteRank) async throws {
        let rig = try await ready()
        defer { rig.collector.close(); Task { await rig.app.disconnect() } }
        try await sample(rig)
        // Required positive control: absent route logging must not pass the race test.
        _ = try #require(rig.recorded.media.last == .observedRoute(connection: .media, kind: .wssTunnel,
            rank: .otherWebSocket, milliseconds: 0))
        let before = await rig.session.diagnosticsSnapshot()
        let mediaBefore = await rig.app.media.selectedMediaDiagnostics(owner: rig.owner)
        let initialCount = rig.recorded.media.count
        // Real collector retirement/reattachment clears its private ledgers.
        // This next first observation must not log the old carrier, even though
        // a separate positive-control window above proved logging is live.
        rig.collector.restartObservationWindowForTesting()
        let mediaBarrier = Barrier()
        let commitBarrier = Barrier()
        defer {
            rig.collector.finalMediaReadForTesting = nil
            Task { await mediaBarrier.resume(); await commitBarrier.resume() }
        }
        rig.collector.finalMediaReadForTesting = {
            await mediaBarrier.pause()
            return await rig.app.media.selectedMediaDiagnostics(owner: rig.owner)
        }
        rig.collector.tickForTesting()
        _ = try #require(await settle { await mediaBarrier.entered })
        let raw = try Candidate(station: rig.station, minor: try #require(await rig.session.agreedMinor))
        let lease = PreauthenticatedTransport(raw)
        _ = try await lease.inspect(clock: rig.clock, deadline: .seconds(30))
        lease.recordDiagnosticServiceRank(newRank.rawValue)
        let move = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true }, requestTicket: {
                try #require(PathTicket(secret: Base64URL.encode(Data(repeating: 0x5a, count: 32)), expiresInMs: 10_000))
            }, onRouteCommit: { await commitBarrier.pause() })
        }
        _ = try #require(await settle { raw.sentCount >= 2 })
        await rig.station.deliver(.pathSwitch)
        _ = try #require(await settle { await commitBarrier.entered })
        let during = await rig.session.diagnosticsSnapshot()
        #expect(during.attemptGeneration == before.attemptGeneration)
        #expect(during.routeID != before.routeID)
        #expect(during.serviceRank == newRank.rawValue)
        await mediaBarrier.resume()
        _ = try #require(await settle { rig.collector.inFlightArmForTesting == nil })
        #expect(rig.recorded.media.count == initialCount,
            "A same-session carrier moved inside final media await: the old-carrier event must be suppressed")
        let mediaAfter = await rig.app.media.selectedMediaDiagnostics(owner: rig.owner)
        #expect(mediaAfter.owner == mediaBefore.owner)
        #expect(mediaAfter.mediaID == mediaBefore.mediaID)
        #expect(mediaAfter.routeGeneration == mediaBefore.routeGeneration)
        rig.collector.finalMediaReadForTesting = nil
        await commitBarrier.resume()
        #expect(await move.value)
        try await sample(rig)
        #expect(Array(rig.recorded.media.dropFirst(initialCount)) == [
            .observedRoute(connection: .media, kind: .wssTunnel, rank: newRank, milliseconds: 0),
        ])
        try await sample(rig)
        #expect(rig.recorded.media.count == initialCount + 1)
    }
}
