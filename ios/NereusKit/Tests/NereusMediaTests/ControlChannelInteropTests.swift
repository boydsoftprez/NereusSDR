// NereusSDR for iOS: the phone's session through the remote access service against the Core's own implementation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// iPhone app plan Task 28a (R-IOS-16): the phone's dial, control peer and
/// session over the data channel, against the Core's own StationRendezvous,
/// DataChannelTransport and StationServer (`nereus_rendezvous_peer core`),
/// through the real rendezvous service, with a STUN and TURN fake; all on
/// this computer (``LocalRendezvous``). And the TURN change: the phone's
/// allocation given back when its connection closes.
@Suite(.serialized,
       .enabled(if: LocalRendezvous.peerPath != nil && LocalRendezvous.pythonReady,
                "set NEREUS_RENDEZVOUS_PEER (ios/scripts/interop-test.sh), with python3's websockets and cryptography"))
struct ControlChannelInteropTests {
    /// The pieces of one run.
    final class Bench: @unchecked Sendable {
        let directory: URL
        let turn: LocalRendezvous.TurnFake
        let service: LocalRendezvous.Service
        let core: LocalRendezvous.Core
        let device: DeviceIdentity

        init() async throws {
            directory = try LocalRendezvous.scratch()
            turn = try await LocalRendezvous.TurnFake(in: directory)
            service = try await LocalRendezvous.Service(in: directory, turn: turn)
            device = try DeviceIdentity.load(store: InMemoryKeyStore())
            core = try await LocalRendezvous.Core(in: directory, service: service, pairedDevice: device.publicKey)
        }

        func stop() {
            core.stop()
            service.stop()
            turn.stop()
            LocalRendezvous.remove(directory)
        }

        /// A dialer to the Core through the local service; with `relayOnly`
        /// only relay candidates pass either way, so the path must go
        /// through the relay.
        func dialer(relayOnly: Bool = false) -> RendezvousDialer {
            RendezvousDialer(servers: [service.server], stationId: core.stationId, device: device,
                             transportFactory: { RendezvousWebSocket(server: $0, plainOnLoopback: true) },
                             localFamilies: { .ipv4Only },
                             passesCandidate: { relayOnly ? MediaPeer.candidateType($0) == "relay" : true })
        }

        /// A session to the Core over the control channel, signing in with
        /// this phone's device key against the Core's identity.
        func session(_ dialer: RendezvousDialer) throws -> StationSession {
            StationSession(trust: .identity(publicKey: core.identityKey),
                           authenticator: try DeviceKeyAuthenticator(identity: device, name: "Interop iPhone",
                                                                      kind: .phone),
                           transport: DataChannelSessionTransport.factory(dialer.connector))
        }
    }

    /// Waits until the session reaches `state` or stops.
    private func settle(_ session: StationSession, at wanted: StationSession.State,
                        within timeout: Duration = .seconds(90)) async -> StationSession.State {
        let deadline = ContinuousClock.now + timeout
        while ContinuousClock.now < deadline {
            let state = await session.state
            if state == wanted || state == .stopped {
                return state
            }
            try? await Task.sleep(for: .milliseconds(100))
        }
        return await session.state
    }

    /// The whole thing, direct: the introduction, the Core's answer over its
    /// own persistent certificate, the checks, the Core's hello bound to the
    /// certificate the phone read in DTLS, the device-key sign-in and the
    /// snapshot. The Core declares the control channel (version 1).
    @Test func aSessionThroughTheServiceReachesReadyAgainstTheCore() async throws {
        let bench = try await Bench()
        defer { bench.stop() }
        let dialer = bench.dialer()
        let session = try bench.session(dialer)
        let capabilities = Capture()
        let reader = Task {
            for await event in session.events {
                if case .message(.capabilities(let message)) = event {
                    capabilities.set(message)
                }
            }
        }
        await session.connect()
        let state = await settle(session, at: .ready)
        #expect(state == .ready, "the session is \(state)")
        #expect(bench.core.sessions >= 1)
        let version = capabilities.value?.properties.first { $0.name == "controlChannelVersion" }?.value
        #expect(version == .i64(1))
        #expect(dialer.attemptTry?.outcome == .connected)
        #expect(dialer.attemptTry?.path == .direct)
        // A direct control path: media takes no relay of its own.
        #expect(dialer.mediaIceSettings()?.relays.isEmpty == true)
        // Task 56: the session's media peer is made from these settings
        // (the app's AppModel.remoteMediaPeer): the service's STUN server,
        // every candidate type, the MTU the Core uses.
        let ice = try #require(dialer.mediaIceSettings())
        let media = try MediaPeer.Configuration.throughRendezvous(ice)
        #expect(media.iceServers.first?.hasPrefix("stun:") == true)
        #expect(!media.hostCandidatesOnly)
        #expect(media.mtu == IceSettings.mtu)
        await session.disconnect()
        // What the connection was when it opened outlives its close (review
        // of Task 28a): the path and the media settings are not re-read
        // from a closed peer.
        #expect(dialer.attemptTry?.path == .direct)
        #expect(dialer.mediaIceSettings() == ice)
        reader.cancel()
    }

    /// With the relay in play (only relay candidates exchanged), each end
    /// allocates on the relay, and when the session ends both ends give
    /// their allocations back (RELEASED from the fake). On one computer the
    /// checks still find a direct loopback path from the peer-reflexive
    /// addresses the relay reveals, so which path is selected is not held
    /// here; the media settings follow whichever it is.
    @Test func aSessionWithTheRelayInPlayGivesItsAllocationsBack() async throws {
        let bench = try await Bench()
        defer { bench.stop() }
        let dialer = bench.dialer(relayOnly: true)
        let session = try bench.session(dialer)
        await session.connect()
        let state = await settle(session, at: .ready)
        #expect(state == .ready, "the session is \(state)")
        let relayed = dialer.attemptTry?.path == .relay
        #expect(dialer.mediaIceSettings()?.relays.count == (relayed ? 1 : 0))
        #expect(dialer.lastPathRelayed == relayed)
        let allocated = bench.turn.count("ALLOCATED")
        #expect(allocated >= 2, "one allocation at each end")
        await session.disconnect()
        // Read when the connection opened: closing it changes neither.
        #expect((dialer.attemptTry?.path == .relay) == relayed)
        #expect(dialer.mediaIceSettings()?.relays.count == (relayed ? 1 : 0))
        try await bench.turn.waitUntilReleased(allocated, within: .seconds(20))
    }

    /// A Core that has not registered: the service says it is not there
    /// and the dial fails at once in the service's words.
    @Test func aCoreNotOnTheServiceFailsAtOnce() async throws {
        let bench = try await Bench()
        defer { bench.stop() }
        let stranger = RendezvousIdentity.stationId(spki: try DeviceIdentity.load(store: InMemoryKeyStore()).publicKey)
        let dialer = RendezvousDialer(servers: [bench.service.server], stationId: stranger, device: bench.device,
                                      transportFactory: { RendezvousWebSocket(server: $0, plainOnLoopback: true) })
        let started = ContinuousClock.now
        do {
            _ = try await dialer.dial { _ in }
            Issue.record("the dial opened")
        } catch let error as RendezvousDialError {
            guard case .service(.offline(let reason)) = error else {
                Issue.record("the dial failed as \(error)")
                return
            }
            #expect(!reason.isEmpty)
            #expect(error.operatorText == reason)
            #expect(dialer.lastError == error)
            #expect(dialer.attemptTry?.outcome == .noAnswer)
        }
        #expect(ContinuousClock.now - started < .seconds(15))
    }

    /// The phone's own relay allocation, made by its control peer's
    /// gathering alone, is given back with a Refresh of LIFETIME 0 when the
    /// peer closes (ios/patches/libjuice/0001).
    @Test func theControlPeerGivesItsRelayAllocationBackWhenItCloses() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let credentials = turn.credentials(user: "interop")
        var ice = IceSettings(stunUrls: ["stun:127.0.0.1:\(turn.port)"], local: .ipv4Only)
        ice.setRelay(RendezvousTurn(username: credentials.username, password: credentials.password, expires: 0,
                                    urls: ["turn:127.0.0.1:\(turn.port)?transport=udp"]), families: 1)
        let peer = try ControlPeer(ice: ice) { _ in }
        let relayed = Capture()
        let reader = Task {
            for await signal in peer.signals {
                if case .offer = signal {
                    try? peer.gather(with: ice)
                }
                if case .candidate(let candidate) = signal, MediaPeer.candidateType(candidate) == "relay" {
                    relayed.mark()
                }
            }
        }
        try await LocalRendezvous.waitUntil("a relay candidate", within: .seconds(20)) { relayed.marked }
        #expect(turn.count("ALLOCATED") == 1)
        #expect(turn.count("RELEASED") == 0)
        peer.close()
        try await LocalRendezvous.waitUntil("the allocation to be given back", within: .seconds(20)) {
            turn.count("RELEASED") == 1
        }
        reader.cancel()
    }

    /// The old one-shot destroy loses an allocation deterministically when
    /// the first zero-lifetime Refresh is dropped. Losing its answer must
    /// also lead to a retransmission with the same transaction ID.
    @Test(arguments: ["--drop-first-release-request", "--drop-first-release-response"])
    func aDroppedReleaseDatagramIsRetried(_ option: String) async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(in: directory, options: [option])
        defer { turn.stop() }
        let credentials = turn.credentials(user: "interop")
        var ice = IceSettings(stunUrls: ["stun:127.0.0.1:\(turn.port)"], local: .ipv4Only)
        ice.setRelay(RendezvousTurn(username: credentials.username, password: credentials.password, expires: 0,
                                    urls: ["turn:127.0.0.1:\(turn.port)?transport=udp"]), families: 1)
        let peer = try ControlPeer(ice: ice) { _ in }
        let relayed = Capture()
        let reader = Task {
            for await signal in peer.signals {
                if case .offer = signal {
                    try? peer.gather(with: ice)
                }
                if case .candidate(let candidate) = signal, MediaPeer.candidateType(candidate) == "relay" {
                    relayed.mark()
                }
            }
        }
        defer { reader.cancel() }
        try await LocalRendezvous.waitUntil("a relay candidate", within: .seconds(20)) { relayed.marked }
        #expect(turn.count("ALLOCATED") == 1)
        peer.close()
        try await turn.waitUntilReleased(1, within: .seconds(8))
        try await LocalRendezvous.waitUntil("a release retransmission", within: .seconds(8)) {
            turn.releaseTransactions.count >= 2
        }
        let transactions = turn.releaseTransactions
        #expect(transactions.count >= 2)
        #expect(transactions[0] == transactions[1], "a datagram retransmission keeps its transaction ID")
        #expect(turn.allocationCount("created") == 1)
        #expect(turn.allocationCount("released") == 1)
        #expect(turn.allocationCount("live") == 0)
    }

    @Test func aLateAuthenticatedAllocateSuccessStartsAFreshRelease() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let barrier = directory.appendingPathComponent("send-held-allocate")
        let turn = try await LocalRendezvous.TurnFake(
            in: directory, options: ["--hold-allocate-request-file", barrier.path])
        defer { turn.stop() }
        let credentials = turn.credentials(user: "interop")
        var ice = IceSettings(stunUrls: ["stun:127.0.0.1:\(turn.port)"], local: .ipv4Only)
        ice.setRelay(RendezvousTurn(username: credentials.username, password: credentials.password, expires: 0,
                                    urls: ["turn:127.0.0.1:\(turn.port)?transport=udp"]), families: 1)
        let peer = try ControlPeer(ice: ice) { _ in }
        let reader = Task {
            for await signal in peer.signals {
                if case .offer = signal {
                    try? peer.gather(with: ice)
                }
            }
        }
        defer { reader.cancel() }
        try await LocalRendezvous.waitUntil("the held authenticated Allocate request", within: .seconds(20)) {
            turn.saw("HELD_ALLOCATE_REQUEST")
        }
        #expect(turn.count("ALLOCATED") == 0)
        peer.close()
        // No allocation existed when these four releases were answered.
        // After the fourth scheduled send at 3.5 s, only handling the
        // delayed Allocate success can issue another release.
        try await LocalRendezvous.waitUntil("four pending-allocation cleanup sends",
                                            within: .seconds(6)) {
            turn.releaseTransactions.count >= 4
        }
        #expect(turn.count("ALLOCATED") == 0)
        let before = turn.releaseTransactions
        try Data("go".utf8).write(to: barrier)
        try await turn.waitUntilReleased(1, within: .seconds(2))
        #expect(turn.saw("SENT_HELD_ALLOCATE_REQUEST"))
        let after = turn.releaseTransactions
        #expect(after.count >= before.count + 1, "the late success must trigger a fifth release")
        #expect(after.last != before.last, "the late success starts a fresh transaction")
        #expect(turn.allocationCount("created") == 1)
        #expect(turn.allocationCount("released") == 1)
        #expect(turn.allocationCount("live") == 0)
    }

    @Test(arguments: ["stale-nonce", "repeat-stale-nonce", "wrong-id", "wrong-source",
                      "bad-integrity", "unsigned-absent", "wrong-method", "missing-lifetime",
                      "nonzero-lifetime"])
    func onlyAnAuthenticatedMatchingReleaseReplyCompletesClose(_ fault: String) async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(
            in: directory, options: ["--release-fault", fault])
        defer { turn.stop() }
        let credentials = turn.credentials(user: "interop")
        var ice = IceSettings(stunUrls: ["stun:127.0.0.1:\(turn.port)"], local: .ipv4Only)
        ice.setRelay(RendezvousTurn(username: credentials.username, password: credentials.password, expires: 0,
                                    urls: ["turn:127.0.0.1:\(turn.port)?transport=udp"]), families: 1)
        let peer = try ControlPeer(ice: ice) { _ in }
        let reader = Task {
            for await signal in peer.signals {
                if case .offer = signal {
                    try? peer.gather(with: ice)
                }
            }
        }
        defer { reader.cancel() }
        try await LocalRendezvous.waitUntil("the relay allocation", within: .seconds(20)) {
            turn.count("ALLOCATED") == 1
        }
        peer.close()
        try await turn.waitUntilReleased(1, within: .seconds(8))
        let transactions = turn.releaseTransactions
        #expect(transactions.count >= 2, "\(fault) cannot acknowledge the first release")
        if transactions.count >= 2 {
            #expect((transactions[0] != transactions[1]) ==
                    (fault == "stale-nonce" || fault == "repeat-stale-nonce"),
                    "only a signed nonce challenge starts a new transaction")
        }
        if fault == "repeat-stale-nonce" {
            #expect(turn.saw("RELEASE_FAULT repeat-stale-nonce id="))
            #expect(transactions.count >= 3)
            if transactions.count >= 3 {
                #expect(transactions[1] == transactions[2],
                        "the second signed nonce challenge cannot start a third transaction")
            }
        }
        #expect(turn.allocationCount("live") == 0)
    }

    final class Capture: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: LinkMessage.Capabilities?
        private var flag = false

        func set(_ value: LinkMessage.Capabilities) {
            lock.withLock { stored = value }
        }

        var value: LinkMessage.Capabilities? { lock.withLock { stored } }

        func mark() {
            lock.withLock { flag = true }
        }

        var marked: Bool { lock.withLock { flag } }
    }
}

#endif
