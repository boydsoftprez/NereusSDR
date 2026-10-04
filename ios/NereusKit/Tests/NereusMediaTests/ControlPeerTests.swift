// NereusSDR for iOS: the control connection's peer: its offer, gathering held for the relay, the DTLS certificate and the path
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin
import NereusLink
import Testing
import CPeerLifetimeTestSupport
import CDataChannel
@testable import NereusMedia

/// iPhone app plan Task 28a (R-IOS-16), link document section 20: what the
/// control peer offers and takes, and what it reports once connected.
/// Serialized, since the peers share libdatachannel's process-wide state.
@Suite(.serialized) struct ControlPeerTests {
    static let ice = IceSettings(stunUrls: ["stun:127.0.0.1:9"], local: .ipv4Only)

    @Test func observationNeedsOpenNominatedControlPeer() throws {
        let peer = try ControlPeer(ice: Self.ice) { _ in }
        #expect(peer.selectedRouteObservation == .unavailable(.notReady))
        peer.close()
        #expect(peer.selectedRouteObservation == .unavailable(.retired))
    }

    @Test func mediaClaimWaitsForNativePeerDestruction() async throws {
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "mediaCloseGrant", expires: 2_000_000_000)
        let context = try RelayRouteContext(grant: grant, socketFactory: nil,
                                            clock: SystemLinkClock()) { _ in }
        let id = UUID()
        let peer = MediaPeer(route: .relay(context))
        let gates = MediaFeatureGates(agreedMinor: 11) { name in
            name == "remoteMediaVersion" || name == "mediaRelayRoutingVersion" ? 1 : 0
        }
        try await peer.prepare(connectionId: id, gates: gates)
        await #expect(throws: RelayICEError.alreadyClaimed) { try await context.claimMedia(connectionId: id) }
        // Answering an offer creates the native ICE agent and hands it the
        // claimed candidate. Without an agent the claim is free as soon as
        // the peer is deleted (lifetimeCompletesForNoIceAndCreationFailure),
        // and the held queue below would hold nothing of this peer's.
        try peer.setRemoteDescription(try await Self.audioOffer())
        let held = try #require(nereusTestHoldPeerTeardown())
        peer.close()
        await peer.waitUntilNativeDeleteReturned()
        await #expect(throws: RelayICEError.alreadyClaimed) { try await context.claimMedia(connectionId: id) }
        nereusTestReleasePeerTeardown(held)
        await peer.closeWhenDeleted()
        let reclaimed = try await context.claimMedia(connectionId: id)
        await context.releaseMedia(reclaimed)
        await context.cancel()
    }

    /// An offer with one audio line, mid "audio", as the Core's media
    /// offer has, made by a libdatachannel peer in this process.
    static func audioOffer() async throws -> String {
        final class Offered: @unchecked Sendable {
            private let lock = NSLock()
            private var stored: String?
            var sdp: String? { lock.withLock { stored } }
            func record(_ event: RtcEvent) {
                if case .localDescription(let sdp, "offer") = event { lock.withLock { stored = sdp } }
            }
        }
        let offered = Offered()
        let offerer = try RtcBridge(settings: DataChannelTestPair.settings) { offered.record($0) }
        defer { offerer.close() }
        _ = try offerer.describeTrack("m=audio 9 UDP/TLS/RTP/SAVPF 111\r\na=mid:audio\r\na=sendonly\r\n"
                                      + "a=rtpmap:111 opus/48000/2\r\n")
        try offerer.setLocalDescription(type: "offer")
        let deadline = ContinuousClock.now + .seconds(30)
        while offered.sdp == nil && ContinuousClock.now < deadline {
            try await Task.sleep(for: .milliseconds(5))
        }
        let sdp = try #require(offered.sdp, "the in-process offerer made no offer")
        return MediaPeer.withoutCandidates(sdp)
    }

    @Test func claimedEndpointAloneDefinesWebRelayRank() async throws {
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "rankGrant", expires: 2_000_000_000)
        let context = try RelayRouteContext(grant: grant, socketFactory: nil,
                                            clock: SystemLinkClock()) { _ in }
        let claim = try await context.claimControl()
        let selected = claim.candidate
        let other = "candidate:other 1 UDP 1 127.0.0.1 50999 typ host"
        #expect(ControlPeer.isWebRelay(pair: (local: other, remote: selected), claim: claim))
        #expect(!ControlPeer.isWebRelay(pair: (local: selected, remote: other), claim: claim))
        #expect(!ControlPeer.isWebRelay(pair: (local: selected, remote: other), claim: nil))
        await context.releaseControl(claim)
        await context.cancel()
    }

    @Test func gatheredAgentKeepsControlClaimWhileNativeTeardownIsHeld() async throws {
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "closeGrant", expires: 2_000_000_000)
        let web = LifetimeRelaySocket()
        let (events, sink) = AsyncStream.makeStream(of: RelayLegEvent.self)
        let context = try RelayRouteContext(grant: grant, socketFactory: { _ in web },
                                            clock: SystemLinkClock()) { sink.yield($0) }
        await context.start()
        var lifecycle = events.makeAsyncIterator()
        #expect(await lifecycle.next() == .ready(peerPresent: true))
        let claim = try await context.claimControl()
        let peer = try ControlPeer(ice: Self.ice) { _ in }
        defer { peer.close() }
        let signals = Signals(peer)
        try await signals.waitUntil("offer before teardown") { signals.offer != nil }
        let core = try TestControlAnswerer()
        defer { core.close() }
        try peer.acceptAnswer(try await core.answer(try #require(signals.offer)))
        #expect(peer.attachRelay(context, claim: claim))
        #expect(peer.addRemoteCandidate(claim.candidate))
        try peer.gather(with: Self.ice)
        try await signals.waitUntil("gathered native agent") { !signals.candidates.isEmpty }
        #expect(peer.admittedRelayCandidateCount == 1)
        try await signals.waitUntil("old agent's real STUN check on the claimed socket") {
            web.snapshot.contains { $0.count > 20 && $0.first == RelayLane.control.rawValue }
        }

        let gate = try #require(nereusTestHoldPeerTeardown())
        peer.close()
        // This is the formerly incorrect release boundary. The native queue
        // cannot reach IceTransport's destructor while the gate is held.
        await peer.waitUntilNativeDeleteReturned()
        await #expect(throws: RelayICEError.alreadyClaimed) { try await context.claimControl() }
        // Cancelling a waiter cannot release the claim before native teardown.
        let waiter = Task { await peer.closeWhenDeleted() }
        waiter.cancel()
        await #expect(throws: RelayICEError.alreadyClaimed) { try await context.claimControl() }
        nereusTestReleasePeerTeardown(gate)
        await peer.closeWhenDeleted()
        await waiter.value
        let next = try await context.claimControl()
        #expect(next.port == claim.port)
        await peer.closeWhenDeleted()
        await #expect(throws: RelayICEError.alreadyClaimed) { try await context.claimControl() }
        // The original agent is destroyed and release drained its queued
        // checks. A new UDP source can become the persistent socket's owner.
        let replacement = try LifetimeUDPProbe()
        try replacement.send(Data([0x55, 0x66]), to: next.port)
        try await signals.waitUntil("replacement sender admitted after native destruction") {
            web.snapshot.contains(Data([1, 0x55, 0x66]))
        }
        web.receive(.binary(Data([1, 0x77])))
        #expect(await replacement.receive() == Data([0x77]))
        await context.releaseControl(next)
        await context.cancel()
    }

    @Test func lifetimeCompletesForNoIceAndCreationFailure() async throws {
        let lifetime = RtcPeerLifetime()
        let bridge = try RtcBridge(settings: TestControlAnswerer.settings, lifetime: lifetime) { _ in }
        bridge.close()
        bridge.close()
        await lifetime.wait()
        await lifetime.wait()

        // A malformed ICE URL fails during C configuration parsing, before
        // a peer or agent exists. C still consumes the retained Swift box.
        let failedLifetime = RtcPeerLifetime()
        var settings = TestControlAnswerer.settings
        settings.iceServers = ["stun:"]
        #expect(throws: MediaPeerError.self) {
            _ = try RtcBridge(settings: settings, lifetime: failedLifetime) { _ in }
        }
        await failedLifetime.wait()
    }

    final class NativeCallbackCount: @unchecked Sendable {
        private let lock = NSLock()
        private var count = 0
        private let completed: AsyncStream<Void>
        private let sink: AsyncStream<Void>.Continuation
        init() { (completed, sink) = AsyncStream.makeStream(of: Void.self) }
        func increment() {
            lock.withLock { count += 1 }
            sink.finish()
        }
        func wait() async { for await _ in completed {} }
        var value: Int { lock.withLock { count } }
    }

    @Test func nativeFailureConsumesLifetimeExactlyOnce() {
        let count = NativeCallbackCount()
        let pointer = Unmanaged.passRetained(count).toOpaque()
        var config = rtcConfiguration()
        let result = "stun:".withCString { server in
            var servers: [UnsafePointer<CChar>?] = [server]
            return servers.withUnsafeMutableBufferPointer { buffer in
                config.iceServers = buffer.baseAddress
                config.iceServersCount = 1
                return rtcCreatePeerConnectionWithLifetime(&config, { pointer in
                    guard let pointer else { return }
                    Unmanaged<NativeCallbackCount>.fromOpaque(pointer).takeRetainedValue().increment()
                }, pointer)
            }
        }
        #expect(result < 0)
        #expect(count.value == 1)
    }

    @Test func nativeNoIceCloseConsumesLifetimeExactlyOnce() async {
        let count = NativeCallbackCount()
        var config = rtcConfiguration()
        config.disableAutoGathering = true
        let peer = rtcCreatePeerConnectionWithLifetime(&config, { pointer in
            guard let pointer else { return }
            Unmanaged<NativeCallbackCount>.fromOpaque(pointer).takeRetainedValue().increment()
        }, Unmanaged.passRetained(count).toOpaque())
        #expect(peer >= 0)
        #expect(count.value == 0)
        #expect(rtcClosePeerConnection(peer) == 0)
        #expect(rtcClosePeerConnection(peer) == 0)
        #expect(rtcDeletePeerConnection(peer) == 0)
        #expect(rtcDeletePeerConnection(peer) < 0)
        await count.wait()
        #expect(count.value == 1)
    }

    final class Signals: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [ControlPeer.Signal] = []
        private var reader: Task<Void, Never>?

        init(_ peer: ControlPeer) {
            reader = Task { [weak self] in
                for await signal in peer.signals {
                    self?.append(signal)
                }
            }
        }

        private func append(_ signal: ControlPeer.Signal) {
            lock.withLock { stored.append(signal) }
        }

        var all: [ControlPeer.Signal] { lock.withLock { stored } }

        var offer: String? {
            for signal in all {
                if case .offer(let sdp) = signal { return sdp }
            }
            return nil
        }

        var candidates: [String] {
            all.compactMap { signal in
                if case .candidate(let candidate) = signal { return candidate }
                return nil
            }
        }

        func waitUntil(_ what: String, _ condition: () -> Bool) async throws {
            for _ in 0..<2_000 {
                if condition() {
                    return
                }
                try await Task.sleep(for: .milliseconds(5))
            }
            throw TestControlAnswerer.TimedOut(description: what)
        }
    }

    /// The offer is one application line with no candidate, and nothing is
    /// gathered until the relay is known: no candidate appears before
    /// gather(with:), and host candidates do after it, then the end.
    @Test func theOfferIsOneApplicationLineAndGatheringWaitsForTheRelay() async throws {
        let peer = try ControlPeer(ice: Self.ice) { _ in }
        defer { peer.close() }
        let signals = Signals(peer)
        try await signals.waitUntil("the offer") { signals.offer != nil }
        let offer = try #require(signals.offer)
        #expect(ControlPeer.isControlDescription(offer))
        #expect(offer.contains("a=mid:"))
        #expect(!MediaPeer.embedsCandidates(offer))
        // Libjuice would have its host candidates within milliseconds; none
        // may come while gathering is held.
        try await Task.sleep(for: .milliseconds(300))
        #expect(signals.candidates.isEmpty)

        var ice = Self.ice
        ice.setRelay(nil, families: 1)
        try peer.gather(with: ice)
        try await signals.waitUntil("a host candidate") { !signals.candidates.isEmpty }
        #expect(signals.candidates.allSatisfy { $0.hasPrefix("candidate:") && !$0.hasPrefix("a=") })
        // Gathering once only.
        #expect(throws: ControlPeer.PeerError.self) { try peer.gather(with: ice) }
    }

    /// A description with a media line, with more than one line, or with a
    /// candidate in it is not a control connection's.
    @Test func onlyOneApplicationLineWithoutCandidatesIsAControlDescription() {
        let base = "v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\n"
        let application = "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\nc=IN IP4 0.0.0.0\r\na=mid:0\r\n"
        #expect(ControlPeer.isControlDescription(base + application))
        #expect(!ControlPeer.isControlDescription(base + "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\na=mid:0\r\n"))
        #expect(!ControlPeer.isControlDescription(base + application + "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"))
        #expect(!ControlPeer.isControlDescription(base + application
                                                  + "a=candidate:1 1 UDP 1 127.0.0.1 5000 typ host\r\n"))
        #expect(!ControlPeer.isControlDescription(base + application + "a=end-of-candidates\r\n"))
        #expect(!ControlPeer.isControlDescription(""))
    }

    /// The Core's answer must be a control connection's; the peer refuses
    /// any other and takes candidates only after one.
    @Test func theAnswerMustBeAControlConnectionsAndCandidatesFollowIt() async throws {
        let peer = try ControlPeer(ice: Self.ice) { _ in }
        defer { peer.close() }
        let signals = Signals(peer)
        try await signals.waitUntil("the offer") { signals.offer != nil }
        let host = "candidate:1 1 UDP 2122317823 127.0.0.1 50000 typ host"
        #expect(!peer.addRemoteCandidate(host), "before the answer")
        #expect(throws: ControlPeer.PeerError.notAControlDescription) {
            try peer.acceptAnswer("v=0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n")
        }
        let core = try TestControlAnswerer()
        defer { core.close() }
        try peer.acceptAnswer(try await core.answer(try #require(signals.offer)))
        #expect(peer.addRemoteCandidate(host))
    }

    /// With the relay not allowed, no relay candidate of the Core's is used.
    @Test func aRelayCandidateIsRefusedWhenTheRelayIsNotAllowed() async throws {
        let denied = IceSettings(stunUrls: ["stun:127.0.0.1:9"], relayAllowed: false, local: .ipv4Only)
        let peer = try ControlPeer(ice: denied) { _ in }
        defer { peer.close() }
        let signals = Signals(peer)
        try await signals.waitUntil("the offer") { signals.offer != nil }
        let core = try TestControlAnswerer()
        defer { core.close() }
        try peer.acceptAnswer(try await core.answer(try #require(signals.offer)))
        #expect(!peer.addRemoteCandidate("candidate:2 1 UDP 16777215 192.0.2.10 50001 typ relay raddr 0.0.0.0 rport 0"))
        #expect(peer.addRemoteCandidate("candidate:1 1 UDP 2122317823 127.0.0.1 50000 typ host"))
    }

    /// The fingerprint the DTLS verifier reports is read as the
    /// certificate's SHA-256, and nothing else is.
    @Test func theDtlsFingerprintReadsAsTheCertificatesDigest() {
        let hex = (0..<32).map { String(format: "%02X", $0 * 7 % 256) }
        let digest = ControlPeer.sha256(fromFingerprint: "sha-256 " + hex.joined(separator: ":"))
        #expect(digest == Data((0..<32).map { UInt8($0 * 7 % 256) }))
        #expect(ControlPeer.sha256(fromFingerprint: "sha-1 " + hex.prefix(20).joined(separator: ":")) == nil)
        #expect(ControlPeer.sha256(fromFingerprint: "sha-256 " + hex.prefix(31).joined(separator: ":")) == nil)
        #expect(ControlPeer.sha256(fromFingerprint: "sha-256 " + hex.joined()) == nil)
        #expect(ControlPeer.sha256(fromFingerprint: "") == nil)
    }

    /// A pair is relayed when either end's candidate is a relay one, or the
    /// Core's relay was learned as peer-reflexive; otherwise direct.
    @Test func aPathIsRelayedByEitherEndsRelayOrTheCoresRelayLearnedAsPeerReflexive() {
        let host = "candidate:1 1 UDP 2122317823 192.0.2.1 50000 typ host"
        let srflx = "candidate:2 1 UDP 1686052607 198.51.100.1 50001 typ srflx raddr 192.0.2.1 rport 50000"
        let relay = "candidate:3 1 UDP 16777215 203.0.113.9 61000 typ relay raddr 198.51.100.1 rport 50001"
        let prflx = "candidate:4 1 UDP 1845501695 203.0.113.9 61000 typ prflx"
        #expect(!ControlPeer.isRelayed(local: host, remote: srflx, farEndRelays: []))
        #expect(ControlPeer.isRelayed(local: relay, remote: host, farEndRelays: []))
        #expect(ControlPeer.isRelayed(local: host, remote: relay, farEndRelays: []))
        #expect(ControlPeer.isRelayed(local: host, remote: prflx, farEndRelays: ["203.0.113.9:61000"]))
        #expect(!ControlPeer.isRelayed(local: host, remote: prflx, farEndRelays: ["203.0.113.9:61001"]))
    }

    /// The media connection's settings follow the control path: the relay
    /// kept while it is relayed or not yet known, dropped when direct, and
    /// the Core's relay candidates accepted either way.
    @Test func mediaTakesTheRelayOnlyWhenTheControlPathIsRelayed() throws {
        var ice = IceSettings(stunUrls: ["stun:rv4.example.net:3478"], local: .ipv4Only)
        // Credentials made for the run.
        let password = Data((0..<20).map { _ in UInt8.random(in: 0...255) }).base64EncodedString()
        ice.setRelay(RendezvousTurn(username: "1:conformance", password: password, expires: 1,
                                    urls: ["turn:rv4.example.net:3478?transport=udp"]), families: 1)
        #expect(ice.relays.count == 1)
        #expect(ice.forMedia(controlPathRelayed: true).relays.count == 1)
        #expect(ice.forMedia(controlPathRelayed: nil).relays.count == 1)
        let direct = ice.forMedia(controlPathRelayed: false)
        #expect(direct.relays.isEmpty)
        #expect(direct.stun == ice.stun)
        #expect(direct.acceptsRemoteCandidate("candidate:3 1 UDP 16777215 203.0.113.9 61000 typ relay"))
        #expect(try MediaPeer.Configuration.throughRendezvous(direct).iceServers == ["stun:rv4.example.net:3478"])
        #expect(ice.stunServerUrls == ["stun:rv4.example.net:3478"])
        #expect(ice.relayServerUrls.count == 1 && ice.relayServerUrls[0].hasPrefix("turn:"))
    }
}

private final class LifetimeRelaySocket: RelayBinarySocket, @unchecked Sendable {
    private let events: AsyncStream<RelaySocketEvent>
    private let sink: AsyncStream<RelaySocketEvent>.Continuation
    private let lock = NSLock()
    private var messages: [Data] = []
    var snapshot: [Data] { lock.withLock { messages } }

    init() { (events, sink) = AsyncStream.makeStream(of: RelaySocketEvent.self) }
    func send(_ message: Data) async throws {
        lock.withLock { messages.append(message) }
        if message.first == 0x80 { sink.yield(.binary(Data([0x81, 1, 1]))) }
    }
    func nextEvent() async -> RelaySocketEvent? {
        var iterator = events.makeAsyncIterator()
        return await iterator.next()
    }
    func receive(_ event: RelaySocketEvent) { sink.yield(event) }
    func close() async { sink.finish() }
}

private final class LifetimeUDPProbe: @unchecked Sendable {
    private let fd: Int32
    init() throws {
        fd = Darwin.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard fd >= 0 else { throw RelayICEError.socketUnavailable }
        var timeout = timeval(tv_sec: 3, tv_usec: 0)
        _ = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, socklen_t(MemoryLayout<timeval>.size))
    }
    deinit { Darwin.close(fd) }
    func send(_ bytes: Data, to port: UInt16) throws {
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_port = port.bigEndian
        address.sin_addr = in_addr(s_addr: UInt32(0x7f000001).bigEndian)
        let count = withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { target in
                bytes.withUnsafeBytes {
                    Darwin.sendto(fd, $0.baseAddress, bytes.count, 0, target,
                                  socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
        guard count == bytes.count else { throw RelayICEError.socketUnavailable }
    }
    func receive() async -> Data? {
        await Task.detached { [self] in
            var buffer = [UInt8](repeating: 0, count: 1600)
            let count = buffer.withUnsafeMutableBytes { Darwin.recv(fd, $0.baseAddress, $0.count, 0) }
            return count > 0 ? Data(buffer.prefix(count)) : nil
        }.value
    }
}
