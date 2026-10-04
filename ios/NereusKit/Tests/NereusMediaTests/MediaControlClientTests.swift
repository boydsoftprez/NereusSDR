// NereusSDR for iOS: the media control client's operations, gates, endpoint checks, keyframes, contexts and reconnects
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// Spec section 4.2 (R3 display endpoints sized by the app) and R-IOS-10
/// (sound only), against the media control document and the link
/// document's section 11. The client runs over a scripted peer, so nothing
/// here opens a network connection.
@Suite struct MediaControlClientTests {
    private actor PreparationGate {
        private var entered = false
        private var enteredWaiter: CheckedContinuation<Void, Never>?
        private var resume: CheckedContinuation<Void, Never>?
        func hold() async {
            entered = true
            enteredWaiter?.resume()
            enteredWaiter = nil
            await withCheckedContinuation { resume = $0 }
        }
        func waitUntilEntered() async {
            if entered { return }
            await withCheckedContinuation { enteredWaiter = $0 }
        }
        func release() { resume?.resume(); resume = nil }
    }
    private actor HitCounter {
        private(set) var count = 0
        func hit() { count += 1 }
    }

    private final class CapturedRoute: @unchecked Sendable {}
    private final class WeakRoute {
        weak var value: CapturedRoute?
        init(_ value: CapturedRoute) { self.value = value }
    }
    private final class WeakSession {
        weak var value: StationSession?
        init(_ value: StationSession) { self.value = value }
    }

    /// A callback can have been taken by a timer executor just before its
    /// owner cancels it. Keep that started action to replay after a new owner.
    private final class StartedClock: LinkClock, @unchecked Sendable {
        private struct Entry {
            let id: Int
            let ms: Int64
            let action: @Sendable () async -> Void
        }
        private struct Timer: LinkTimer {
            let clock: StartedClock
            let id: Int
            func cancel() { clock.lock.withLock { clock.entries.removeAll { $0.id == id } } }
        }
        private let lock = NSLock()
        private var entries: [Entry] = []
        private var serial = 0

        func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
            let parts = delay.components
            let ms = parts.seconds * 1_000 + parts.attoseconds / 1_000_000_000_000_000
            return lock.withLock {
                serial += 1
                entries.append(Entry(id: serial, ms: ms, action: action))
                return Timer(clock: self, id: serial)
            }
        }
        func take(after ms: Int64) -> (@Sendable () async -> Void)? {
            lock.withLock {
                guard let index = entries.firstIndex(where: { $0.ms == ms }) else { return nil }
                return entries.remove(at: index).action
            }
        }
        func count(after ms: Int64) -> Int { lock.withLock { entries.filter { $0.ms == ms }.count } }
    }

    @Test func endedSessionCannotSendStartAfterAsyncRoutePreparation() async {
        let gate = PreparationGate()
        let peer = ScriptedMediaPeer(prepareHook: { _, _ in await gate.hold() })
        let recorder = MediaControlRecorder()
        let client = MediaControlClient(send: recorder.sender, peerFactory: { peer },
                                        clock: recorder.clock, timers: ManualLinkClock())
        await client.handle(.stateChanged(.authenticating))
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                 peer: "nereusd", majors: [1], features: [:]))))
        await client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))))
        let finishing = Task { await client.handle(.message(.snapshotComplete)) }
        await gate.waitUntilEntered()
        await client.handle(.stateChanged(.stopped))
        await gate.release()
        await finishing.value
        #expect(recorder.sent("start").isEmpty)
        #expect(peer.isClosed)
    }

    @Test func newerLogicalOwnerInvalidatesSuspendedHandleAndStaleFactory() async {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let first = session()
        let second = session()
        let gate = PreparationGate()
        let stalePeer = ScriptedMediaPeer(prepareHook: { _, _ in await gate.hold() })
        let currentPeer = ScriptedMediaPeer()
        let client = MediaControlClient(send: { _ in }, peerFactory: { stalePeer },
                                        timers: ManualLinkClock())
        #expect(await client.activateLogicalSession(first, owner: 1))
        await client.handle(.stateChanged(.authenticating), owner: 1)
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                peer: "nereusd", majors: [1], features: [:]))), owner: 1)
        await client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))), owner: 1)
        let staleHandle = Task { await client.handle(.message(.snapshotComplete), owner: 1) }
        await gate.waitUntilEntered()
        #expect(await client.activateLogicalSession(second, owner: 2))
        #expect(!(await client.activateLogicalSession(first, owner: 1)))
        #expect(!(await client.usePeers({ stalePeer }, connectDeadline: .seconds(5), owner: 1)))
        #expect(await client.usePeers({ currentPeer }, connectDeadline: .seconds(5), owner: 2))
        await gate.release()
        await staleHandle.value
        #expect(stalePeer.isClosed)
        #expect(await client.connectionId == nil)
        await client.handle(.stateChanged(.authenticating), owner: 2)
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                peer: "nereusd", majors: [1], features: [:]))), owner: 2)
        await client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))), owner: 2)
        await client.handle(.message(.snapshotComplete), owner: 2)
        #expect(currentPeer.preparedConnectionId != nil)
        #expect(currentPeer.preparedConnectionId != stalePeer.preparedConnectionId)
        #expect(!currentPeer.isClosed)
    }

    @Test func mediaObservationStaysOnCurrentOwnerAndAcceptedPeer() async throws {
        let station = ScriptedStation()
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: ManualLinkClock(), transportFactory: station.factory)
        let rig = try Rig()
        #expect(await rig.client.activateLogicalSession(session, owner: 7))
        await rig.open(minor: 11, capabilities: Self.mediaOnly.merging(["mediaReplaceVersion": 1]) { _, new in new })
        let oldId = try await rig.connectionId
        let oldPeer = try rig.peer
        let oldRoute = SelectedRouteObservation.fromSelectedICEPair(
            local: "candidate:a 1 UDP 1 192.0.2.1 5000 typ host",
            remote: "candidate:b 1 UDP 1 198.51.100.2 5001 typ host")
        oldPeer.observe(oldRoute)
        let oldTraffic = MediaTrafficObservation(lifetime: UUID(), active: true,
            receivedDisplayPayloadBytes: 21, receivedRtpBytes: 34,
            submittedRtpBytes: 0, submittedTxChannelBytes: 0)
        oldPeer.observeTraffic(oldTraffic)
        #expect(await rig.client.selectedMediaRoute(owner: 7).observation == .unavailable(.noCurrentMedia))
        #expect(await rig.client.selectedMediaTraffic(owner: 7).observation == nil)
        oldPeer.become(.connected)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.connected)) }
        let current = await rig.client.selectedMediaRoute(owner: 7)
        #expect(current.mediaID == oldId && current.owner == 7 && current.observation == oldRoute)
        #expect(await rig.client.selectedMediaRoute(owner: 6).observation == .unavailable(.retired))
        #expect(await rig.client.selectedMediaTraffic(owner: 7).observation == oldTraffic)
        #expect(await rig.client.selectedMediaTraffic(owner: 6).observation == nil)
        await rig.client.setAudioEnabled(true)
        let audioSsrc = MediaControlClient.audioSsrc(forConnection: oldId)
        await rig.client.handle(.message(.mediaControl(.init(payload: Self.audioContext(
            oldId, revision: 1, generation: 1, ssrc: audioSsrc)))), owner: 7)
        let beforeContextChange = await rig.client.selectedMediaDiagnostics(owner: 7)
        #expect(beforeContextChange.acceptedAudioGeneration == 1)
        await rig.client.handle(.message(.mediaControl(.init(payload: Self.audioContext(
            oldId, revision: 1, generation: 2, ssrc: audioSsrc)))), owner: 7)
        let afterContextChange = await rig.client.selectedMediaDiagnostics(owner: 7)
        #expect(afterContextChange.mediaID == beforeContextChange.mediaID)
        #expect(afterContextChange.routeGeneration == beforeContextChange.routeGeneration)
        #expect(afterContextChange.acceptedAudioGeneration == 2,
                "a held collector read must reject the changed stream on the same peer")

        let newPeer = ScriptedMediaPeer()
        let newRoute = SelectedRouteObservation.fromSelectedICEPair(
            local: "candidate:c 1 UDP 1 192.0.2.1 5002 typ host",
            remote: "candidate:d 1 UDP 1 198.51.100.3 5003 typ host")
        newPeer.observe(newRoute)
        let newTraffic = MediaTrafficObservation(lifetime: UUID(), active: true,
            receivedDisplayPayloadBytes: 0, receivedRtpBytes: 0,
            submittedRtpBytes: 0, submittedTxChannelBytes: 0)
        newPeer.observeTraffic(newTraffic)
        #expect(await rig.client.controlRouteDidMove(peerFactory: { newPeer }, connectDeadline: .seconds(5),
                                                     safeToReplace: { true }, owner: 7))
        #expect(await rig.client.selectedMediaRoute(owner: 7).mediaID == oldId)
        #expect(await rig.client.selectedMediaTraffic(owner: 7).observation == oldTraffic)
        let newId = try #require(newPeer.preparedConnectionId?.uuidString.lowercased())
        await rig.client.handle(.message(.mediaControl(.init(payload: [
            "op": "replace", "connectionId": .string(newId), "replaces": .string(oldId),
        ]))), owner: 7)
        let moved = await rig.client.selectedMediaRoute(owner: 7)
        #expect(moved.mediaID == newId && moved.observation == newRoute)
        let movedTraffic = await rig.client.selectedMediaTraffic(owner: 7)
        #expect(movedTraffic.mediaID == newId && movedTraffic.observation == newTraffic)
        oldPeer.observeTraffic(MediaTrafficObservation(lifetime: oldTraffic.lifetime, active: true,
            receivedDisplayPayloadBytes: 9_999, receivedRtpBytes: 9_999,
            submittedRtpBytes: 0, submittedTxChannelBytes: 0))
        #expect(await rig.client.selectedMediaTraffic(owner: 7).observation == newTraffic)
        #expect(moved.routeGeneration >= current.routeGeneration)
        #expect(await rig.client.retireLogicalSession(owner: 7))
        #expect(await rig.client.selectedMediaRoute(owner: 7).observation == .unavailable(.retired))
        #expect(await rig.client.selectedMediaTraffic(owner: 7).observation == nil)
    }

    @Test func retiredLogicalOwnerReleasesSessionAndRouteCapturesButKeepsWatermark() async {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let client = MediaControlClient(send: { _ in }, timers: ManualLinkClock())
        var oldSession: StationSession? = session()
        let releasedSession = WeakSession(oldSession!)
        #expect(await client.activateLogicalSession(oldSession!, owner: 41))
        func bindCapturedRoute() async -> WeakRoute {
            let captured = CapturedRoute()
            let weak = WeakRoute(captured)
            let moved = await client.controlRouteDidMove(peerFactory: { [captured] in
                _ = captured
                return ScriptedMediaPeer()
            }, connectDeadline: .seconds(5), safeToReplace: { [captured] in
                _ = captured
                return true
            }, owner: 41)
            #expect(moved)
            return weak
        }
        let releasedRoute = await bindCapturedRoute()
        oldSession = nil
        #expect(releasedSession.value != nil && releasedRoute.value != nil)
        #expect(await client.retireLogicalSession(owner: 41))
        #expect(releasedSession.value == nil, "retired logical session must not be kept by media")
        #expect(releasedRoute.value == nil, "peer factory and route safety closure must release their captures")
        #expect(!(await client.retireLogicalSession(owner: 41)))
        #expect(!(await client.activateLogicalSession(session(), owner: 41)))
        let newSession = session()
        #expect(await client.activateLogicalSession(newSession, owner: 42))
        #expect(!(await client.retireLogicalSession(owner: 41)), "late OLD teardown must spare NEW")
        let newPeer = ScriptedMediaPeer()
        #expect(await client.usePeers({ newPeer }, connectDeadline: .seconds(5), owner: 42))
        await client.handle(.stateChanged(.authenticating), owner: 42)
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                peer: "nereusd", majors: [1], features: [:]))), owner: 42)
        await client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))), owner: 42)
        await client.handle(.message(.snapshotComplete), owner: 42)
        #expect(newPeer.preparedConnectionId != nil && !newPeer.isClosed)
    }

    @Test func retirementInvalidatesSuspendedPreparationAndLateOldEvents() async {
        let station = ScriptedStation()
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: ManualLinkClock(), transportFactory: station.factory)
        let gate = PreparationGate()
        let oldPeer = ScriptedMediaPeer(prepareHook: { _, _ in await gate.hold() })
        let recorder = MediaControlRecorder()
        let client = MediaControlClient(send: recorder.sender, peerFactory: { oldPeer },
                                        timers: ManualLinkClock())
        #expect(await client.activateLogicalSession(session, owner: 7))
        await client.handle(.stateChanged(.authenticating), owner: 7)
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                peer: "nereusd", majors: [1], features: [:]))), owner: 7)
        await client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))), owner: 7)
        let preparing = Task { await client.handle(.message(.snapshotComplete), owner: 7) }
        await gate.waitUntilEntered()
        #expect(await client.retireLogicalSession(owner: 7))
        await gate.release()
        await preparing.value
        await client.handle(.message(.snapshotComplete), owner: 7)
        #expect(recorder.sent("start").isEmpty)
        #expect(oldPeer.isClosed)
        #expect(await client.connectionId == nil)
        #expect(!(await client.usePeers({ oldPeer }, connectDeadline: .seconds(5), owner: 7)))
        #expect(!(await client.selectedTunnel(owner: 7)))
    }

    @Test func retirementDropsHeldOldRouteMoveWithoutFallingBackToSender() async throws {
        let station = ScriptedStation()
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: ManualLinkClock(), transportFactory: station.factory)
        let rig = try Rig()
        #expect(await rig.client.activateLogicalSession(session, owner: 9))
        await rig.open(minor: 11, capabilities: Self.everyFeature.merging(["mediaReplaceVersion": 1]) { _, new in new })
        try rig.peer.become(.connected)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.connected)) }
        let gate = PreparationGate()
        let latePeer = ScriptedMediaPeer()
        let moving = Task {
            await rig.client.controlRouteDidMove(peerFactory: { latePeer }, connectDeadline: .seconds(5),
                                                  safeToReplace: { await gate.hold(); return true }, owner: 9)
        }
        await gate.waitUntilEntered()
        #expect(await rig.client.retireLogicalSession(owner: 9))
        await gate.release()
        #expect(!(await moving.value))
        #expect(latePeer.preparedConnectionId == nil)
        #expect(rig.recorder.sent("replace").isEmpty,
                "OLD work must not fall back to the initializer sender after logicalSession clears")
        #expect(!(await rig.client.controlRouteDidMove(peerFactory: { latePeer },
                                                       connectDeadline: .seconds(5),
                                                       safeToReplace: { true }, owner: 9)))
    }

    @Test func newerLogicalOwnerInvalidatesSuspendedRouteMove() async throws {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let rig = try Rig()
        #expect(await rig.client.activateLogicalSession(session(), owner: 1))
        await rig.open(minor: 11, capabilities: Self.everyFeature.merging(["mediaReplaceVersion": 1]) { _, new in new })
        try rig.peer.become(.connected)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.connected)) }
        let gate = PreparationGate()
        let stalePeer = ScriptedMediaPeer()
        let moving = Task {
            await rig.client.controlRouteDidMove(peerFactory: { stalePeer }, connectDeadline: .seconds(5),
                                                  safeToReplace: { await gate.hold(); return true }, owner: 1)
        }
        await gate.waitUntilEntered()
        #expect(await rig.client.activateLogicalSession(session(), owner: 2))
        await gate.release()
        #expect(!(await moving.value))
        #expect(stalePeer.preparedConnectionId == nil)
        #expect(!(await rig.client.controlRouteDidMove(peerFactory: { stalePeer },
                                                       connectDeadline: .seconds(5),
                                                       safeToReplace: { true }, owner: 1)))
    }

    @Test func startedOldPollCannotClearNewOwnersPoll() async throws {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let clock = StartedClock()
        let recorder = MediaControlRecorder()
        let client = MediaControlClient(send: recorder.sender, peerFactory: recorder.peerFactory,
                                        timers: clock)
        recorder.listen(to: client)
        func open(_ owner: UInt64, _ session: StationSession) async throws {
            #expect(await client.activateLogicalSession(session, owner: owner))
            await client.handle(.stateChanged(.authenticating), owner: owner)
            await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                    peer: "nereusd", majors: [1], features: [:]))), owner: owner)
            await client.handle(.message(.capabilities(Self.capabilities(
                ["remoteMediaVersion": 1, "mediaReplaceVersion": 1]))), owner: owner)
            await client.handle(.message(.snapshotComplete), owner: owner)
            let peer = try #require(recorder.peers.last)
            let connectedBefore = recorder.events.filter { $0 == .mediaState(.connected) }.count
            peer.become(.connected)
            #expect(await recorder.settle {
                recorder.events.filter { $0 == .mediaState(.connected) }.count > connectedBefore
            })
        }
        try await open(1, session())
        await client.controlRouteDidMove(peerFactory: recorder.peerFactory, connectDeadline: .seconds(5),
                                         safeToReplace: { false }, owner: 1)
        let oldAction = try #require(clock.take(after: 500))
        try await open(2, session())
        await client.controlRouteDidMove(peerFactory: recorder.peerFactory, connectDeadline: .seconds(5),
                                         safeToReplace: { false }, owner: 2)
        #expect(clock.count(after: 500) == 1)
        await oldAction()
        #expect(clock.count(after: 500) == 1, "old callback cannot clear the current poll")
    }

    @Test func oldTransmitSilenceCannotSuppressNewOwnersAudioStall() async throws {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let rig = try Rig()
        #expect(await rig.client.activateLogicalSession(session(), owner: 1))
        #expect(await rig.client.setExpectedAudioSilenceDuringTransmit(true, owner: 1))
        #expect(await rig.client.activateLogicalSession(session(), owner: 2))
        #expect(!(await rig.client.setExpectedAudioSilenceDuringTransmit(true, owner: 1)))
        #expect(!(await rig.client.selectedTunnel(owner: 1)))
        await rig.client.handle(.stateChanged(.authenticating), owner: 2)
        await rig.client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                     peer: "nereusd", majors: [1], features: [:]))), owner: 2)
        await rig.client.handle(.message(.capabilities(Self.capabilities(Self.mediaOnly))), owner: 2)
        await rig.client.handle(.message(.snapshotComplete), owner: 2)
        let peer = try rig.peer
        peer.selectCarrier(true)
        peer.become(.connected)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.connected)) }
        await rig.client.setAudioEnabled(true)
        let id = try await rig.connectionId
        await rig.client.handle(.message(.mediaControl(.init(payload:
            Self.audioContext(id, revision: 1, generation: 1,
                              ssrc: MediaControlClient.audioSsrc(forConnection: id))))), owner: 2)
        peer.receiveAudio(RtpPacket(payloadType: 111, sequence: 1, timestamp: 0,
                                    ssrc: MediaControlClient.audioSsrc(forConnection: id),
                                    payload: Data([0xf8, 0xff, 0xfe])))
        await rig.recorder.settle { rig.timers.pendingDueTimes.contains(3_000) }
        #expect(rig.timers.pendingDueTimes.contains(3_000))
    }

    @Test func startedOldDrainCannotConsumeNewOwnersPendingMove() async throws {
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        let rig = try Rig()
        #expect(await rig.client.activateLogicalSession(session(), owner: 1))
        await rig.open(minor: 11, capabilities: Self.mediaOnly.merging(["mediaReplaceVersion": 1]) { _, new in new })
        let oldId = try await rig.connectionId
        let old = try rig.peer
        let oldHandled = await rig.client.mediaUpHandled
        old.become(.connected)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.connected)) }
        await rig.mediaUpHandled(after: oldHandled)
        let middle = ScriptedMediaPeer()
        _ = await rig.client.controlRouteDidMove(peerFactory: { middle }, connectDeadline: .seconds(5),
                                                 safeToReplace: { true }, owner: 1)
        let middleId = try #require(middle.preparedConnectionId?.uuidString.lowercased())
        await rig.client.handle(.message(.mediaControl(.init(payload: [
            "op": "replace", "connectionId": .string(middleId), "replaces": .string(oldId),
        ]))), owner: 1)
        let gate = PreparationGate()
        _ = await rig.client.controlRouteDidMove(peerFactory: { ScriptedMediaPeer() },
                                                 connectDeadline: .seconds(5),
                                                 safeToReplace: { await gate.hold(); return true }, owner: 1)
        let draining = Task { await rig.timers.advance(by: 2_000) }
        await gate.waitUntilEntered()
        #expect(await rig.client.activateLogicalSession(session(), owner: 2))
        let current = ScriptedMediaPeer()
        #expect(await rig.client.usePeers({ current }, connectDeadline: .seconds(5), owner: 2))
        await rig.open(minor: 11, capabilities: Self.mediaOnly.merging(["mediaReplaceVersion": 1]) { _, new in new })
        let connectedBefore = rig.recorder.events.filter { $0 == .mediaState(.connected) }.count
        let currentHandled = await rig.client.mediaUpHandled
        current.become(.connected)
        #expect(await rig.recorder.settle {
            rig.recorder.events.filter { $0 == .mediaState(.connected) }.count > connectedBefore
        })
        await rig.mediaUpHandled(after: currentHandled)
        let calls = HitCounter()
        _ = await rig.client.controlRouteDidMove(peerFactory: { ScriptedMediaPeer() },
                                                 connectDeadline: .seconds(5), safeToReplace: {
                                                     await calls.hit(); return false
                                                 }, owner: 2)
        #expect(await calls.count == 1)
        await gate.release()
        await draining.value
        #expect(await calls.count == 1, "OLD drain cannot enter NEW owner's pending move")
    }
    // MARK: Rig

    /// Every capability of the link document's section 6.3 table that
    /// media control reads, at its value with every feature on.
    static let everyFeature: [String: Int64] = [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 1, "remoteAudioStatusVersion": 1,
        "spectrumGrantVersion": 1, "remoteDisplayBudgetVersion": 1, "audioProfileVersion": 1,
        "audioClockVersion": 1, "receiverAudioVersion": 1, "headphonesMixVersion": 1,
    ]
    static let mediaOnly: [String: Int64] = ["remoteMediaVersion": 1]

    @Test func tunnelAndRelayRoutingRequireTypedMinor11AndStartDeclaresBoth() async throws {
        let offered = ["remoteMediaVersion": Int64(1), "mediaTunnelVersion": 1,
                       "mediaRelayRoutingVersion": 1]
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: offered)
        #expect(await rig.client.gates.mediaTunnel)
        #expect(await rig.client.gates.mediaRelayRouting)
        let id = try await rig.connectionId
        #expect(rig.recorder.sent("start").first == [
            "op": .string("start"), "connectionId": .string(id),
            "mediaTunnelVersion": .number(1), "mediaRelayRoutingVersion": .number(1),
        ])
        let older = try Rig()
        await older.open(minor: 10, capabilities: offered)
        #expect(!(await older.client.gates.mediaTunnel))
        #expect(!(await older.client.gates.mediaRelayRouting))
        #expect(older.recorder.sent("start").first?["mediaTunnelVersion"] == nil)
        #expect(older.recorder.sent("start").first?["mediaRelayRoutingVersion"] == nil)
        let mistyped = MediaFeatureGates(agreedMinor: 11) { key in
            key == "remoteMediaVersion" ? 1 : 0
        }
        #expect(!mistyped.mediaTunnel && !mistyped.mediaRelayRouting)
    }

    struct Rig {
        let recorder = MediaControlRecorder()
        /// The connect deadline and the restart waits run on this.
        let timers = ManualLinkClock()
        let playback: AudioPlaybackCore
        let client: MediaControlClient

        init(connectDeadline: Duration = MediaControlClient.connectDeadline) throws {
            playback = try AudioPlaybackCore()
            client = MediaControlClient(send: recorder.sender, peerFactory: recorder.peerFactory,
                                        playback: playback, clock: recorder.clock, timers: timers,
                                        connectDeadline: connectDeadline,
                                        onMediaConnected: recorder.onMediaConnected)
            recorder.listen(to: client)
        }

        /// The Core's hello at `minor`, its capabilities and, unless told
        /// otherwise, snapshot.complete.
        func open(minor: UInt16, capabilities: [String: Int64], complete: Bool = true) async {
            await client.handle(.stateChanged(.authenticating))
            await client.handle(.message(.hello(LinkMessage.Hello(major: 1, minor: minor, settingsSchema: 0,
                                                                 peer: "nereusd", majors: [1], features: [:]))))
            await client.handle(.stateChanged(.receivingSnapshot))
            await client.handle(.message(.capabilities(MediaControlClientTests.capabilities(capabilities))))
            if complete {
                await client.handle(.message(.snapshotComplete))
                await client.handle(.stateChanged(.ready))
            }
        }

        var peer: ScriptedMediaPeer {
            get throws {
                try #require(recorder.peers.last)
            }
        }

        var connectionId: String {
            get async throws {
                try #require(await client.connectionId)
            }
        }

        /// The peer connects; waits until the client has seen it, has told
        /// the session (the event goes out before that call) and has
        /// finished with it: the timers it arms after telling the session
        /// (the direct ladder's first step) are booked before a test moves
        /// the clock.
        func connect() async throws {
            let calls = recorder.mediaConnectedCalls
            let handled = await client.mediaUpHandled
            try peer.become(.connected)
            await recorder.settle {
                recorder.events.contains(.mediaState(.connected)) && recorder.mediaConnectedCalls > calls
            }
            #expect(recorder.events.contains(.mediaState(.connected)))
            await mediaUpHandled(after: handled)
        }

        /// Waits until the client has finished bringing a media connection
        /// up since it had counted `handled`.
        func mediaUpHandled(after handled: Int) async {
            var done = await client.mediaUpHandled
            let wait = Date().addingTimeInterval(5)
            while done == handled && Date() < wait {
                await Task.yield()
                done = await client.mediaUpHandled
            }
            #expect(done > handled, "the client did not finish bringing media up")
        }

        /// Delivers one of the Core's operations.
        func deliver(_ payload: [String: LinkJSON]) async {
            await client.handle(.message(.mediaControl(LinkMessage.MediaControl(payload: payload))))
        }
    }

    static func capabilities(_ versions: [String: Int64]) -> LinkMessage.Capabilities {
        LinkMessage.Capabilities(properties: versions.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: .i64(versions[$0] ?? 0))
        })
    }

    static func subscription(endpointId: UInt32 = 1, revision: UInt32 = 1,
                             extendedView: Bool? = nil) -> DisplaySubscription {
        DisplaySubscription(endpointId: endpointId, revision: revision, sliceId: 0, tier: .wide, fftSize: 4096,
                            windowType: 1, centreHz: 14_100_000, spanHz: 96_000, pixels: 1170, fps: 30,
                            framesPerLine: 1,
                            trace: .init(detector: 0, averageMode: 1, averageAlpha: 0.5),
                            waterfall: .init(detector: 0, averageMode: 0, averageAlpha: 1),
                            minDbm: -140, maxDbm: -40, wideSpanFactor: 0, extendedView: extendedView)
    }

    /// A context as the Core writes it: the 19 fields, plus `wideband` and
    /// the grant fields when asked.
    static func context(_ id: String, endpointId: UInt32 = 1, revision: UInt32 = 1, generation: UInt32 = 1,
                        wideband: Bool = false, grant: Bool = false) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = [
            "op": "context", "connectionId": .string(id), "endpointId": .number(Double(endpointId)),
            "revision": .number(Double(revision)), "contextGeneration": .number(Double(generation)),
            "sourceStream": 0, "sourceCentreHz": 14_100_000, "sampleRateHz": 192_000,
            "centreHz": 14_100_000, "spanHz": 96_000, "wideCentreHz": 0, "wideSpanHz": 0,
            "traceSamples": 32, "waterfallSamples": 32, "wideSamples": 0, "minDbm": -140, "maxDbm": -40,
            "fps": 30, "framesPerLine": 1,
        ]
        if wideband {
            payload["wideband"] = .object(["version": 1, "available": false, "active": false])
        }
        if grant {
            payload["grantedFftSize"] = 4096
            payload["grantedTier"] = "wide"
            payload["requestedPixels"] = 32
            payload["grantedPixels"] = 32
            payload["limit"] = "none"
        }
        return payload
    }

    static func audioContext(_ id: String, revision: UInt32, generation: UInt32, ssrc: UInt32,
                             enabled: Bool = true, detail: Bool = false, profile: Bool = false,
                             firstSequence: UInt16 = 100) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = [
            "op": "audio-context", "connectionId": .string(id), "revision": .number(Double(revision)),
            "generation": .number(Double(generation)), "enabled": .bool(enabled), "ssrc": .number(Double(ssrc)),
            "firstSequence": .number(Double(firstSequence)), "firstTimestamp": 0,
        ]
        if detail {
            if enabled {
                payload["encoder"] = .object(["codec": "opus", "sampleRate": 48000, "channels": 2,
                                              "frameSamples": 1920, "targetBitrate": 24000,
                                              "audioBandwidthHz": 8000])
            } else {
                payload["reason"] = "client-disabled"
            }
            if profile {
                payload["profile"] = "opus"
            }
        }
        return payload
    }

    static func allocationResult(_ id: String, endpointId: UInt32, revision: UInt32,
                                 accepted: Bool) -> [String: LinkJSON] {
        [
            "op": "allocation-result", "connectionId": .string(id), "endpointId": .number(Double(endpointId)),
            "revision": .number(Double(revision)), "accepted": .bool(accepted),
            "reason": .string(accepted ? "" : "The Core's display limit has no room left."),
            "budgetGeneration": 1, "acceptedRevision": .number(accepted ? Double(revision) : 0),
            "applicationBytesPerSecond": .number(accepted ? 40_000 : 0),
            "spectrumSampleUnitsPerSecond": .number(accepted ? 35_100 : 0),
            "messagesPerSecond": .number(accepted ? 30 : 0),
        ]
    }

    /// The media control table of `surface.json`: operation name to its
    /// always-present fields and its conditional ones with their conditions.
    static func surface(_ direction: String) throws -> [String: (fields: Set<String>, conditional: [String: [String]])] {
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let table = try #require((object["mediaControl"] as? [String: Any])?[direction] as? [String: Any])
        var out: [String: (fields: Set<String>, conditional: [String: [String]])] = [:]
        for (op, entry) in table {
            let row = try #require(entry as? [String: Any])
            let fields = Set(try #require(row["fields"] as? [String]))
            let conditional = (row["conditional"] as? [String: [String]]) ?? [:]
            out[op] = (fields, conditional)
        }
        return out
    }

    // MARK: Starting

    @Test func mediaStartsOnlyAfterSnapshotCompleteWhenTheCoreAdvertisesIt() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.mediaOnly, complete: false)
        #expect(rig.recorder.sent.isEmpty)
        await rig.client.handle(.message(.snapshotComplete))
        await rig.client.handle(.stateChanged(.ready))
        let start = try #require(rig.recorder.sent("start").first)
        #expect(start == ["op": "start", "connectionId": .string(try await rig.connectionId)])

        let without = try Rig()
        await without.open(minor: 11, capabilities: ["remoteMediaVersion": 0])
        #expect(without.recorder.sent.isEmpty)
        #expect(await without.client.connectionId == nil)

        // Minor 0 predates media: the capability alone is not enough.
        let older = try Rig()
        await older.open(minor: 0, capabilities: Self.mediaOnly)
        #expect(older.recorder.sent.isEmpty)
    }

    @Test func theConnectionIdIsACanonicalLowercaseUuid() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        #expect(id.count == 36)
        #expect(id == id.lowercased())
        #expect(!id.contains("{") && !id.contains("}"))
        #expect(id.split(separator: "-").map(\.count) == [8, 4, 4, 4, 12])
        #expect(UUID(uuidString: id)?.uuidString.lowercased() == id)
    }

    @Test func theAudioSsrcIsDerivedFromTheConnectionId() {
        // SHA-256 of "NereusSDR/media-audio-ssrc/v1:" + the ID, first four
        // bytes big-endian; worked out with Python's hashlib.
        #expect(MediaControlClient.audioSsrc(forConnection: "3f2504e0-4f89-41d3-9a0c-0305e82c3301") == 572_081_121)
    }

    // MARK: Field sets against the documents' tables

    /// Drives every operation the app sends and returns them by op.
    private func everyOperation(minor: UInt16, capabilities: [String: Int64])
        async throws -> (sent: [String: [String: LinkJSON]], id: String) {
        let rig = try Rig()
        await rig.open(minor: minor, capabilities: capabilities)
        let id = try await rig.connectionId
        // The answer and the candidate reach the client on separate streams,
        // so each is waited for on its own before anything is read.
        try rig.peer.answer("v=0\r\n")
        await rig.recorder.waitUntilSent("description")
        try rig.peer.trickle("candidate:1 1 UDP 2122252543 192.0.2.30 50002 typ host", mid: "0")
        await rig.recorder.waitUntilSent("candidate")
        try await rig.connect()
        await rig.client.setAudioEnabled(true)
        let gates = await rig.client.gates
        try await rig.client.subscribe(Self.subscription(extendedView: gates.wideband ? false : nil))
        await rig.deliver(Self.context(id, wideband: gates.wideband, grant: gates.spectrumGrant))
        try await rig.client.unsubscribe(endpointId: 1)
        var byOp: [String: [String: LinkJSON]] = [:]
        for payload in rig.recorder.sent {
            let op = try #require(MediaControlDecoder.string(payload["op"]))
            #expect(byOp[op] == nil, "one \(op) expected")
            byOp[op] = payload
        }
        return (byOp, id)
    }

    @Test(.timeLimit(.minutes(1)), arguments: [(UInt16(11), everyFeature), (UInt16(1), mediaOnly)])
    func everyOperationCarriesExactlyTheDocumentsFields(minor: UInt16, capabilities: [String: Int64]) async throws {
        let table = try Self.surface("guiToCore")
        let (sent, id) = try await everyOperation(minor: minor, capabilities: capabilities)
        #expect(Set(sent.keys) == ["start", "description", "candidate", "audio", "subscribe", "keyframe",
                                   "unsubscribe"])
        // The app declares no receiver streams and no headphones mix yet, so
        // start carries neither even from a Core that offers them.
        let declined: Set<String> = ["receiverAudioVersion", "headphonesMixVersion"]
        // Nor does it send Rendering > Decimation (spectrumGrantVersion 2),
        // which is optional: absent, the Core's engine runs undecimated.
        let declinedFields: Set<String> = ["decimation"]
        for (op, payload) in sent {
            let row = try #require(table[op], "\(op) is in the link document's table")
            var expected = row.fields
            for (field, conditions) in row.conditional
            where !declined.contains(field) && !declinedFields.contains(field) {
                if conditions.allSatisfy({ (capabilities[$0] ?? 0) >= 1 }) {
                    expected.insert(field)
                }
            }
            #expect(Set(payload.keys) == expected, "\(op)")
            #expect(payload["connectionId"] == .string(id), "\(op)")
        }
        #expect(sent["description"]?["type"] == "answer")
        #expect(sent["audio"]?["enabled"] == .bool(true))
        if minor == 11 {
            #expect(sent["audio"]?["profile"] == "opus")
            #expect(sent["start"]?["audioProfileVersion"] == 1)
        }
        let subscribe = try #require(sent["subscribe"])
        for plane in ["trace", "waterfall"] {
            guard case .object(let fields)? = subscribe[plane] else {
                Issue.record("\(plane) is an object")
                continue
            }
            #expect(Set(fields.keys) == ["detector", "averageMode", "averageAlpha"])
        }
    }

    @Test func theCoresOperationsDecodeInExactlyTheDocumentsFields() throws {
        let table = try Self.surface("coreToGui")
        let id = "3f2504e0-4f89-41d3-9a0c-0305e82c3301"
        // The builders write the table's sets; a key more or less is refused.
        let cases: [(String, [String: LinkJSON], Set<String>, ([String: LinkJSON]) -> Bool)] = [
            ("context", Self.context(id), [], { MediaControlDecoder.context($0, wideband: false, grant: false) != nil }),
            ("context", Self.context(id, wideband: true, grant: true),
             ["wideband", "grantedFftSize", "grantedTier", "requestedPixels", "grantedPixels", "limit"],
             { MediaControlDecoder.context($0, wideband: true, grant: true) != nil }),
            ("audio-context", Self.audioContext(id, revision: 1, generation: 1, ssrc: 7), [],
             { MediaControlDecoder.audioContext($0, detail: false, profile: false) != nil }),
            ("audio-context", Self.audioContext(id, revision: 1, generation: 1, ssrc: 7, detail: true, profile: true),
             ["encoder", "profile"], { MediaControlDecoder.audioContext($0, detail: true, profile: true) != nil }),
            ("allocation-result", Self.allocationResult(id, endpointId: 1, revision: 1, accepted: true), [],
             { MediaControlDecoder.allocationResult($0) != nil }),
            ("rejected", ["op": "rejected", "connectionId": .string(id), "endpointId": 1, "revision": 1,
                          "reason": "This display's receiver is not on the Core."], [],
             { MediaControlDecoder.rejected($0) != nil }),
            ("noise-floor", ["op": "noise-floor", "connectionId": .string(id), "endpointId": 1, "revision": 1,
                             "contextGeneration": 1, "floorDbm": -121.5], [],
             { MediaControlDecoder.noiseFloor($0) != nil }),
            ("description", ["op": "description", "connectionId": .string(id), "sdp": "v=0\r\n", "type": "offer"], [],
             { MediaControlDecoder.description($0) != nil }),
            ("candidate", ["op": "candidate", "connectionId": .string(id), "candidate": "candidate:1", "mid": "0"], [],
             { MediaControlDecoder.candidate($0) != nil }),
        ]
        for (op, payload, conditional, decodes) in cases {
            let row = try #require(table[op])
            #expect(Set(payload.keys) == row.fields.union(conditional), "\(op)")
            #expect(conditional.isSubset(of: Set(row.conditional.keys)), "\(op)")
            #expect(decodes(payload), "\(op) decodes")
            var extra = payload
            extra["unexpected"] = 1
            #expect(!decodes(extra), "\(op) with a key more")
            for key in payload.keys where key != "op" {
                var missing = payload
                missing.removeValue(forKey: key)
                #expect(!decodes(missing), "\(op) without \(key)")
            }
        }
        // The shape follows the gates: a minor-9 context is refused where the
        // grant was not negotiated, and the other way round.
        #expect(MediaControlDecoder.context(Self.context(id, grant: true), wideband: false, grant: false) == nil)
        #expect(MediaControlDecoder.context(Self.context(id), wideband: false, grant: true) == nil)
        #expect(MediaControlDecoder.audioContext(Self.audioContext(id, revision: 1, generation: 1, ssrc: 7, detail: true),
                                                 detail: false, profile: false) == nil)
    }

    // MARK: Endpoint checks

    static let invalidSubscriptions: [(String, DisplaySubscription, DisplayEndpointRequest.Invalid)] = {
        var list: [(String, DisplaySubscription, DisplayEndpointRequest.Invalid)] = []
        func with(_ name: String, _ error: DisplayEndpointRequest.Invalid,
                  _ change: (inout DisplaySubscription) -> Void) {
            var subscription = subscription(endpointId: 2)
            change(&subscription)
            list.append((name, subscription, error))
        }
        with("0 pixels", .pixels) { $0.pixels = 0 }
        with("4097 pixels", .pixels) { $0.pixels = 4097 }
        with("61 frames a second", .fps) { $0.fps = 61 }
        with("0 frames a second", .fps) { $0.fps = 0 }
        with("FFT size 1000", .fftSize) { $0.fftSize = 1000 }
        with("FFT size 512", .fftSize) { $0.fftSize = 512 }
        with("minDbm below -400", .dbmWindow) { $0.minDbm = -400.5 }
        with("minDbm equal to maxDbm", .dbmWindow) { $0.minDbm = -40 }
        with("minDbm above maxDbm", .dbmWindow) { $0.minDbm = -30 }
        with("maxDbm not finite", .dbmWindow) { $0.maxDbm = .infinity }
        with("endpoint 0", .endpointIdZero) { $0.endpointId = 0 }
        with("revision 0", .revisionZero) { $0.revision = 0 }
        with("alpha above 1", .plane) { $0.trace.averageAlpha = 1.5 }
        with("span 0", .frequency) { $0.spanHz = 0 }
        with("wide span factor 1", .wideSpanFactor) { $0.wideSpanFactor = 1 }
        with("extended view from a Core without it", .extendedViewUnavailable) { $0.extendedView = true }
        return list
    }()

    @Test(arguments: invalidSubscriptions.map(\.0))
    func anInvalidSubscriptionThrowsBeforeSending(name: String) async throws {
        let (_, subscription, error) = try #require(Self.invalidSubscriptions.first { $0.0 == name })
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let before = rig.recorder.sent.count
        await #expect(throws: error, "\(name)") {
            try await rig.client.subscribe(subscription)
        }
        #expect(rig.recorder.sent.count == before, "\(name)")
    }

    @Test func aNinthEndpointThrowsBeforeSending() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        for endpoint in UInt32(1)...8 {
            try await rig.client.subscribe(Self.subscription(endpointId: endpoint))
        }
        #expect(rig.recorder.sent("subscribe").count == 8)
        await #expect(throws: DisplayEndpointRequest.Invalid.tooManyEndpoints) {
            try await rig.client.subscribe(Self.subscription(endpointId: 9))
        }
        #expect(rig.recorder.sent("subscribe").count == 8)
        // Renewing a held endpoint is not a ninth.
        try await rig.client.subscribe(Self.subscription(endpointId: 8, revision: 2))
        #expect(rig.recorder.sent("subscribe").count == 9)
        await #expect(throws: DisplayEndpointRequest.Invalid.staleRevision) {
            try await rig.client.subscribe(Self.subscription(endpointId: 8, revision: 2))
        }
    }

    @Test func subscribingWithoutAMediaConnectionThrows() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 0])
        await #expect(throws: MediaControlError.noMediaConnection) {
            try await rig.client.subscribe(Self.subscription())
        }
        #expect(rig.recorder.sent.isEmpty)
    }

    // MARK: The display budget wire

    @Test(arguments: [
        (UInt16(7), Int64(1), true),
        (UInt16(11), Int64(1), true),
        (UInt16(6), Int64(1), false),
        (UInt16(11), Int64(0), false),
    ])
    func unsubscribeCarriesARevisionExactlyOnTheBudgetWire(minor: UInt16, budget: Int64, carries: Bool) async throws {
        let rig = try Rig()
        await rig.open(minor: minor, capabilities: ["remoteMediaVersion": 1, "remoteDisplayBudgetVersion": budget])
        try await rig.client.subscribe(Self.subscription(endpointId: 3, revision: 41))
        try await rig.client.unsubscribe(endpointId: 3)
        let unsubscribe = try #require(rig.recorder.sent("unsubscribe").first)
        var expected: Set<String> = ["op", "connectionId", "endpointId"]
        if carries {
            expected.insert("revision")
            #expect(unsubscribe["revision"] == 42)
        }
        #expect(Set(unsubscribe.keys) == expected)
        #expect(unsubscribe["endpointId"] == 3)
    }

    @Test(arguments: [(UInt16(7), Int64(1), true), (UInt16(6), Int64(1), false), (UInt16(11), Int64(0), false)])
    func allocationResultIsDecodedOnlyOnTheBudgetWire(minor: UInt16, budget: Int64, decoded: Bool) async throws {
        let rig = try Rig()
        await rig.open(minor: minor, capabilities: ["remoteMediaVersion": 1, "remoteDisplayBudgetVersion": budget])
        let id = try await rig.connectionId
        try await rig.client.subscribe(Self.subscription(endpointId: 4, revision: 1))
        await rig.deliver(Self.allocationResult(id, endpointId: 4, revision: 1, accepted: false))
        await rig.recorder.settle { false }
        let results = rig.recorder.events.filter {
            if case .allocationResult = $0 { return true } else { return false }
        }
        #expect(results.count == (decoded ? 1 : 0))
        // A refusal the Core holds nothing for frees the endpoint.
        #expect(await rig.client.endpointIds == (decoded ? [] : [4]))

        // A rejected for an endpoint is the legacy wire's; on the budget
        // wire allocation-result replaces it.
        try await rig.client.subscribe(Self.subscription(endpointId: 5, revision: 1))
        await rig.deliver(["op": "rejected", "connectionId": .string(id), "endpointId": 5, "revision": 1,
                           "reason": "This display's receiver is not on the Core."])
        await rig.recorder.settle { false }
        let rejections = rig.recorder.events.filter { if case .rejected = $0 { return true } else { return false } }
        #expect(rejections.count == (decoded ? 0 : 1))
    }

    /// A whole-peer refusal: `rejected` with endpoint and revision 0.
    static func wholePeerRejected(_ id: String, reason: String) -> [String: LinkJSON] {
        ["op": "rejected", "connectionId": .string(id), "endpointId": 0, "revision": 0, "reason": .string(reason)]
    }

    @Test(arguments: [
        "The Core is already sending audio and display on another connection.",
        "The Core could not start audio and display.",
        "Something this app has never heard of.",
        "The Core lost the audio and display connection",
    ])
    func aWholePeerRefusalIsFinal(reason: String) async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let id = try await rig.connectionId
        let peer = try rig.peer
        await rig.deliver(Self.wholePeerRejected(id, reason: reason))
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.closed)) }
        #expect(rig.recorder.events.contains {
            if case .rejected(let rejection) = $0 { return rejection.refusesWholePeer } else { return false }
        })
        #expect(rig.recorder.events.contains(.mediaState(.closed)))
        #expect(peer.isClosed)
        #expect(await rig.client.connectionId == nil)
        // Not started again in this session: not at once, not later, not
        // by the next capabilities.
        await rig.timers.advance(by: 600_000)
        await rig.client.handle(.message(.capabilities(Self.capabilities(Self.everyFeature))))
        #expect(rig.recorder.sent("start").count == 1)
        #expect(rig.timers.pendingDueTimes.isEmpty)
    }

    @Test(arguments: MediaControlClient.restartingRefusalReasons.sorted())
    func aCoreThatDroppedTheConnectionGetsANewOne(reason: String) async throws {
        #expect(MediaControlClient.restartingRefusalReasons == [
            "The Core lost the audio and display connection.",
            "The audio and display connection to the Core closed.",
        ])
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let id = try await rig.connectionId
        let peer = try rig.peer
        try await rig.connect()
        await rig.deliver(Self.wholePeerRejected(id, reason: reason))
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.closed)) }
        #expect(rig.recorder.events.contains(.mediaState(.closed)))
        #expect(peer.isClosed)
        // The first restart goes at once, under a new connection ID.
        let second = try await rig.connectionId
        #expect(second != id)
        #expect(rig.recorder.sent("start").map { $0["connectionId"] } == [.string(id), .string(second)])
        // Dropped again before connecting: the next waits out 1 s.
        await rig.deliver(Self.wholePeerRejected(second, reason: reason))
        #expect(await rig.client.connectionId == nil)
        await rig.timers.advance(by: 999)
        #expect(rig.recorder.sent("start").count == 2)
        await rig.timers.advance(by: 1)
        #expect(rig.recorder.sent("start").count == 3)
    }

    // MARK: Contexts, keyframes and frames

    @Test func keyframesWaitForAContextAndStayWithinFiveASecond() async throws {
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        #expect(await rig.client.requestKeyframe(endpointId: 1) == false)
        #expect(rig.recorder.sent("keyframe").isEmpty)

        // A context for another revision is not this endpoint's.
        await rig.deliver(Self.context(id, revision: 2))
        #expect(rig.recorder.sent("keyframe").isEmpty)

        await rig.deliver(Self.context(id, generation: 7))
        // Accepting the context asks for one keyframe of it.
        #expect(rig.recorder.sent("keyframe") == [["op": "keyframe", "connectionId": .string(id), "endpointId": 1,
                                                   "contextGeneration": 7]])
        var sentNow = 1
        for _ in 0..<10 {
            if await rig.client.requestKeyframe(endpointId: 1) {
                sentNow += 1
            }
        }
        #expect(sentNow == 5)
        rig.recorder.advanceClock(by: 999)
        #expect(await rig.client.requestKeyframe(endpointId: 1) == false)
        rig.recorder.advanceClock(by: 1)
        #expect(await rig.client.requestKeyframe(endpointId: 1) == true)
        #expect(rig.recorder.sent("keyframe").count == 6)
    }

    @Test func displayFramesBeforeTheirContextAreDiscarded() async throws {
        let vectors = try LinkFixtureLoader.mediaVectors()
        let full = try #require(vectors["media-nsdc1-full"]).bytes
        let delta = try #require(vectors["media-nsdc1-delta"]).bytes
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        let peer = try rig.peer
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        func frames() -> [DisplayFrame] {
            rig.recorder.events.compactMap { event -> DisplayFrame? in
                if case .displayFrame(let frame) = event { return frame } else { return nil }
            }
        }

        // The fixtures are endpoint 1, context generation 1. The keyframe
        // arrives before the context and is discarded, so the delta after
        // the context has nothing to build on and asks for a keyframe.
        peer.receiveDisplay(full)
        await rig.recorder.settle { false }
        #expect(frames().isEmpty)
        await rig.deliver(Self.context(id, generation: 1))
        #expect(rig.recorder.sent("keyframe").count == 1)
        peer.receiveDisplay(delta)
        await rig.recorder.settle { rig.recorder.sent("keyframe").count == 2 }
        #expect(frames().isEmpty)
        #expect(rig.recorder.sent("keyframe").count == 2)

        // A frame of a newer generation than the accepted context is
        // discarded too; the current generation's keyframe and delta play.
        var newer = full
        newer[15] = 2
        peer.receiveDisplay(newer)
        peer.receiveDisplay(full)
        peer.receiveDisplay(delta)
        await rig.recorder.settle { frames().count == 2 }
        #expect(frames().map(\.encoderSequence) == [1, 2])
        #expect(frames().allSatisfy { $0.contextGeneration == 1 })
    }

    /// A drag sends revision after revision, and on the budget wire the
    /// Core answers each with its result at once and its context with the
    /// next frame, so a context comes after the next revision has gone. A
    /// context for any revision this app sent, newer than the one held, is
    /// taken and its frames play, so the band keeps scrolling through the
    /// drag; one for a revision never sent, or older than the one held, is not.
    @Test func aContextForARevisionSentBeforeTheNewestStillPlays() async throws {
        let vectors = try LinkFixtureLoader.mediaVectors()
        let full = try #require(vectors["media-nsdc1-full"]).bytes
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        let peer = try rig.peer
        func frames() -> [DisplayFrame] {
            rig.recorder.events.compactMap { event -> DisplayFrame? in
                if case .displayFrame(let frame) = event { return frame } else { return nil }
            }
        }
        func contexts() -> [MediaControlEvent.DisplayContext] {
            rig.recorder.events.compactMap { event -> MediaControlEvent.DisplayContext? in
                if case .context(let context) = event { return context } else { return nil }
            }
        }
        for revision: UInt32 in 1...3 {
            try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: revision))
        }
        // A revision never sent is not this endpoint's.
        await rig.deliver(Self.context(id, revision: 4, generation: 1))
        await rig.recorder.settle { false }
        #expect(contexts().isEmpty)

        // Revision 2's context, after revision 3 went: its frames play.
        await rig.deliver(Self.context(id, revision: 2, generation: 1))
        await rig.recorder.settle { contexts().count == 1 }
        #expect(contexts().map(\.revision) == [2])
        peer.receiveDisplay(full)
        await rig.recorder.settle { frames().count == 1 }
        #expect(frames().map(\.contextGeneration) == [1])

        // Revision 1's, late, is older than the one held.
        await rig.deliver(Self.context(id, revision: 1, generation: 2))
        await rig.recorder.settle { contexts().count > 1 }
        #expect(contexts().map(\.revision) == [2])
        // The newest revision's takes over.
        await rig.deliver(Self.context(id, revision: 3, generation: 2))
        await rig.recorder.settle { contexts().count == 2 }
        #expect(contexts().map(\.revision) == [2, 3])
    }

    @Test func aContextIsAcceptedOnlyInItsNegotiatedShape() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let id = try await rig.connectionId
        // No extendedView asked: its context has no wideband.
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        await rig.deliver(Self.context(id, endpointId: 1, wideband: true, grant: true))
        await rig.deliver(Self.context(id, endpointId: 1, wideband: false, grant: false))
        #expect(!rig.recorder.events.contains { if case .context = $0 { return true } else { return false } })
        await rig.deliver(Self.context(id, endpointId: 1, wideband: false, grant: true))
        // The extended view asked: its context carries wideband.
        try await rig.client.subscribe(Self.subscription(endpointId: 2, revision: 1, extendedView: true))
        await rig.deliver(Self.context(id, endpointId: 2, wideband: false, grant: true))
        await rig.deliver(Self.context(id, endpointId: 2, wideband: true, grant: true))
        await rig.recorder.settle { false }
        let contexts = rig.recorder.events.compactMap { event -> MediaControlEvent.DisplayContext? in
            if case .context(let context) = event { return context } else { return nil }
        }
        try #require(contexts.map(\.endpointId) == [1, 2])
        #expect(contexts[0].grant?.grantedFftSize == 4096)
        #expect(contexts[1].wideband == MediaControlEvent.Wideband(available: false, active: false))
    }

    @Test func aNoiseFloorIsReportedOnlyForTheCurrentContext() async throws {
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        let floor: [String: LinkJSON] = ["op": "noise-floor", "connectionId": .string(id), "endpointId": 1,
                                         "revision": 1, "contextGeneration": 3, "floorDbm": -121.5]
        await rig.deliver(floor)
        await rig.deliver(Self.context(id, generation: 3))
        await rig.deliver(floor)
        var stale = floor
        stale["contextGeneration"] = 2
        await rig.deliver(stale)
        await rig.recorder.settle { false }
        let floors = rig.recorder.events.compactMap { event -> MediaControlEvent.NoiseFloor? in
            if case .noiseFloor(let floor) = event { return floor } else { return nil }
        }
        #expect(floors == [MediaControlEvent.NoiseFloor(endpointId: 1, revision: 1, contextGeneration: 3,
                                                        floorDbm: -121.5)])
    }

    // MARK: Signalling

    @Test func theCoresOfferAndCandidatesReachThePeerAndTheAnswerGoesBack() async throws {
        let rig = try Rig()
        await rig.open(minor: 1, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        let peer = try rig.peer
        await rig.deliver(["op": "candidate", "connectionId": .string(id),
                           "candidate": "candidate:1 1 UDP 2122252543 192.0.2.20 50001 typ host", "mid": "0"])
        await rig.deliver(["op": "description", "connectionId": .string(id), "sdp": "v=0\r\n", "type": "offer"])
        // Another connection's operation, and an answer where an offer belongs, are not taken.
        await rig.deliver(["op": "description", "connectionId": "00000000-0000-4000-8000-000000000000",
                           "sdp": "v=0\r\n", "type": "offer"])
        await rig.deliver(["op": "description", "connectionId": .string(id), "sdp": "v=0\r\n", "type": "answer"])
        await rig.recorder.settle { false }
        #expect(peer.remoteDescriptions == ["v=0\r\n"])
        #expect(peer.remoteCandidates.map(\.candidate) == ["candidate:1 1 UDP 2122252543 192.0.2.20 50001 typ host"])
        #expect(rig.recorder.events.contains(.description(.init(sdp: "v=0\r\n", type: "offer"))))
        peer.answer("v=0\r\nanswer\r\n")
        await rig.recorder.settle { !rig.recorder.sent("description").isEmpty }
        #expect(rig.recorder.sent("description") == [["op": "description", "connectionId": .string(id),
                                                      "sdp": "v=0\r\nanswer\r\n", "type": "answer"]])
    }

    // MARK: Audio

    @Test func relayAudioStallWaitsThroughAuthenticatedTransmitSilenceThenGetsFreshRXWindow() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        try await rig.connect()
        let peer = try rig.peer
        peer.selectCarrier(true)
        await rig.client.setAudioEnabled(true)
        let ssrc = MediaControlClient.audioSsrc(forConnection: id)
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc))
        peer.receiveAudio(RtpPacket(payloadType: 111, sequence: 100, timestamp: 0,
                                    ssrc: ssrc, payload: Data([0xf8, 0xff, 0xfe])))
        await rig.recorder.settle { rig.timers.pendingDueTimes.contains(3_000) }
        await rig.client.setExpectedAudioSilenceDuringTransmit(true)
        rig.recorder.advanceClock(by: 5_000)
        await rig.timers.advance(by: 5_000)
        #expect(rig.recorder.sent("start").count == 1)
        await rig.client.setExpectedAudioSilenceDuringTransmit(false)
        rig.recorder.advanceClock(by: 2_999)
        await rig.timers.advance(by: 2_999)
        #expect(rig.recorder.sent("start").count == 1)
        rig.recorder.advanceClock(by: 1)
        await rig.timers.advance(by: 1)
        #expect(rig.recorder.sent("start").count == 2)
    }

    @Test func replacementWithoutFirstNewAudioGetsFreshThreeSecondStallWindow() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.mediaOnly.merging(["mediaReplaceVersion": 1]) { _, new in new })
        let oldId = try await rig.connectionId
        try await rig.connect()
        await rig.client.setAudioEnabled(true)
        let oldPeer = try rig.peer
        oldPeer.selectCarrier(true)
        let oldSsrc = MediaControlClient.audioSsrc(forConnection: oldId)
        await rig.deliver(Self.audioContext(oldId, revision: 1, generation: 1, ssrc: oldSsrc))
        oldPeer.receiveAudio(RtpPacket(payloadType: 111, sequence: 100, timestamp: 0,
                                       ssrc: oldSsrc, payload: Data([0xf8, 0xff, 0xfe])))
        await rig.recorder.settle { rig.timers.pendingDueTimes.contains(3_000) }
        let next = ScriptedMediaPeer()
        next.selectCarrier(true)
        await rig.client.controlRouteDidMove(peerFactory: { next }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        let newId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        rig.recorder.advanceClock(by: 2_999)
        await rig.timers.advance(by: 2_999)
        #expect(rig.recorder.sent("start").count == 1)
        rig.recorder.advanceClock(by: 1)
        await rig.timers.advance(by: 1)
        #expect(rig.recorder.sent("start").count == 2, "selected NEW without RTP must restart promptly")
    }

    @Test func audioStaysOffUntilAskedForAndEachContextReanchorsPlayback() async throws {
        let rig = try Rig()
        await rig.open(minor: 5, capabilities: Self.mediaOnly)
        let id = try await rig.connectionId
        let peer = try rig.peer
        try await rig.connect()
        #expect(rig.recorder.sent("audio").isEmpty)
        #expect(rig.playback.anchor == nil)
        #expect(rig.recorder.mediaConnectedCalls == 1)

        await rig.client.setAudioEnabled(true)
        let audio = try #require(rig.recorder.sent("audio").first)
        #expect(audio == ["op": "audio", "connectionId": .string(id), "revision": 1, "enabled": true])
        let ssrc = MediaControlClient.audioSsrc(forConnection: id)

        // A context for another SSRC or an older request is not taken.
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc &+ 1))
        await rig.deliver(Self.audioContext(id, revision: 9, generation: 1, ssrc: ssrc))
        #expect(rig.playback.anchor == nil)

        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc, firstSequence: 100))
        #expect(peer.expectedSsrc == .some(ssrc))
        #expect(rig.playback.anchor == AudioStreamAnchor(generation: 1, ssrc: ssrc, firstSequence: 100,
                                                         firstTimestamp: 0))
        // A newer context re-anchors; an older generation does not.
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 2, ssrc: ssrc, firstSequence: 900))
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc, firstSequence: 5))
        #expect(rig.playback.anchor?.firstSequence == 900)
        #expect(rig.playback.anchor?.generation == 2)

        // Its revision always rises; turning audio off anchors nothing.
        await rig.client.setAudioEnabled(false)
        #expect(rig.recorder.sent("audio").last?["revision"] == 2)
        #expect(rig.recorder.sent("audio").last?["enabled"] == .bool(false))
        await rig.deliver(Self.audioContext(id, revision: 2, generation: 3, ssrc: ssrc, enabled: false))
        #expect(rig.playback.anchor == nil)
    }

    @Test func theProfileShapeFollowsTheAudioProfileGate() async throws {
        let rig = try Rig()
        await rig.open(minor: 8, capabilities: ["remoteMediaVersion": 1, "remoteAudioStatusVersion": 1,
                                                "audioProfileVersion": 1])
        let id = try await rig.connectionId
        #expect(rig.recorder.sent("start").first?["audioProfileVersion"] == 1)
        try await rig.connect()
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").first?["profile"] == "opus")
        let ssrc = MediaControlClient.audioSsrc(forConnection: id)
        // The detail shape without the profile is refused; with it, taken.
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc, detail: true))
        #expect(rig.playback.anchor == nil)
        await rig.deliver(Self.audioContext(id, revision: 1, generation: 1, ssrc: ssrc, detail: true, profile: true))
        #expect(rig.playback.anchor?.ssrc == ssrc)

        // At minor 7 the profile is not sent, whatever the Core advertises.
        let older = try Rig()
        await older.open(minor: 7, capabilities: ["remoteMediaVersion": 1, "remoteAudioStatusVersion": 1,
                                                  "audioProfileVersion": 1])
        try await older.connect()
        await older.client.setAudioEnabled(true)
        #expect(older.recorder.sent("start").first?["audioProfileVersion"] == nil)
        #expect(older.recorder.sent("audio").first.map { Set($0.keys) } == ["op", "connectionId", "revision", "enabled"])
    }

    // MARK: The microphone line (Task 55)

    /// At minor 11 with remoteTxVersion the start carries it and the peer is
    /// asked for the line on the Core's derivation of its SSRC; once media
    /// is up with the line open, the uplink sends the microphone as RTP on
    /// it and the keepalive on "tx", and when the connection ends both stop.
    @Test func withRemoteTxTheStartAsksForTheMicrophoneLineAndTheUplinkUsesIt() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1, "remoteTxVersion": 1])
        let id = try await rig.connectionId
        #expect(rig.recorder.sent("start").first?["remoteTxVersion"] == 1)
        let peer = try rig.peer
        let microphone = MediaControlClient.microphoneSsrc(forConnection: id)
        #expect(peer.requestedMicrophoneSsrc == microphone)
        #expect(microphone != MediaControlClient.audioSsrc(forConnection: id))
        #expect(!MediaControlClient.receiverSsrcs(forConnection: id).contains(microphone))
        #expect(microphone != MediaControlClient.headphonesSsrc(forConnection: id))
        #expect(!rig.client.uplink.sendMicrophone(Data([1])), "nothing goes before media is up")

        peer.openMicrophoneLine()
        try await rig.connect()
        // The line's event is posted after connect returns; wait for it, as
        // the closing half of this test does for the line going down.
        await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(true)) }
        #expect(rig.recorder.events.contains(.microphoneLine(true)))
        #expect(rig.client.uplink.microphoneLineReady)
        #expect(rig.client.uplink.sendMicrophone(Data([1, 2])))
        #expect(rig.client.uplink.sendMicrophone(Data([3])))
        let packets = peer.microphonePackets
        #expect(packets.count == 2)
        #expect(packets.allSatisfy { $0.ssrc == microphone && $0.payloadType == 111 })
        #expect(packets[1].sequence == packets[0].sequence &+ 1)
        #expect(packets[1].timestamp == packets[0].timestamp &+ 960)
        #expect(rig.client.uplink.sendKeepalive(sequence: 3, epoch: 12))
        #expect(peer.txMessages == [Data([1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 12])])

        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(false)) }
        #expect(rig.recorder.events.contains(.microphoneLine(false)))
        #expect(!rig.client.uplink.microphoneLineReady)
        #expect(!rig.client.uplink.sendMicrophone(Data([4])))
        #expect(!rig.client.uplink.sendKeepalive(sequence: 4, epoch: 12))
    }

    /// M6: the Core closing the microphone line's track while the media
    /// connection stays up says the line is gone, so the phone stops
    /// counting on a microphone the Core no longer hears.
    @Test func theMicrophoneTrackClosingSaysTheLineIsGone() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1, "remoteTxVersion": 1])
        _ = try await rig.connectionId
        let peer = try rig.peer
        peer.openMicrophoneLine()
        try await rig.connect()
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(true)) })
        peer.closeMicrophoneLine()
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(false)) })
        #expect(!rig.client.uplink.sendMicrophone(Data([1])))
        #expect(rig.recorder.events.filter { $0 == .microphoneLine(false) }.count == 1)
        // The connection ending afterwards does not say it twice.
        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(rig.recorder.events.filter { $0 == .microphoneLine(false) }.count == 1)
    }

    /// Fix wave (fix-tx2 item 5): the Core closing the microphone line's
    /// track on a connection that stays up is told apart, once, after the
    /// line is said to be gone, so a key held here can be released. The
    /// line going with the connection says only that the line is gone.
    @Test func theMicrophoneTrackClosingIsToldApartFromTheConnectionEnding() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1, "remoteTxVersion": 1])
        _ = try await rig.connectionId
        let peer = try rig.peer
        peer.openMicrophoneLine()
        try await rig.connect()
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(true)) })
        peer.closeMicrophoneLine()
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneTrackClosed) })
        let events = rig.recorder.events
        let gone = try #require(events.firstIndex(of: .microphoneLine(false)))
        let closed = try #require(events.firstIndex(of: .microphoneTrackClosed))
        #expect(gone < closed)
        #expect(events.filter { $0 == .microphoneTrackClosed }.count == 1)
        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.closed)) })
        #expect(rig.recorder.events.filter { $0 == .microphoneTrackClosed }.count == 1)
    }

    /// The line going with the connection is not the Core closing its track.
    @Test func theConnectionEndingIsNotTheMicrophoneTrackClosing() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1, "remoteTxVersion": 1])
        _ = try await rig.connectionId
        let peer = try rig.peer
        peer.openMicrophoneLine()
        try await rig.connect()
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(true)) })
        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.microphoneLine(false)) })
        #expect(await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.closed)) })
        #expect(!rig.recorder.events.contains(.microphoneTrackClosed))
    }

    /// Without the Core's remoteTxVersion, or below minor 11, the start and
    /// the peer are exactly as before.
    @Test func withoutRemoteTxNothingAsksForTheLine() async throws {
        for (minor, capabilities) in [(UInt16(11), ["remoteMediaVersion": Int64(1)]),
                                      (UInt16(10), ["remoteMediaVersion": 1, "remoteTxVersion": 1])] {
            let rig = try Rig()
            await rig.open(minor: minor, capabilities: capabilities)
            _ = try await rig.connectionId
            #expect(rig.recorder.sent("start").first?["remoteTxVersion"] == nil)
            #expect(try rig.peer.requestedMicrophoneSsrc == nil)
            try await rig.connect()
            #expect(!rig.recorder.events.contains(.microphoneLine(true)))
        }
    }

    /// The keepalive's 13 bytes: kind 1, the sequence and the epoch big-endian.
    @Test func theKeepaliveMessageIsThirteenBytes() {
        let message = MediaUplink.keepaliveMessage(sequence: 0x0102_0304_0506_0708, epoch: 4_294_967_295)
        #expect(message == Data([1, 1, 2, 3, 4, 5, 6, 7, 8, 0xff, 0xff, 0xff, 0xff]))
        #expect(message.count == MediaUplink.keepaliveBytes)
    }

    // MARK: Display extras

    /// Every feature, display extras included.
    static let withExtras: [String: Int64] = everyFeature.merging(["displayExtrasVersion": 1]) { $1 }

    @Test func theDisplayExtrasGateNeedsMinor11AndTheCoresVersion() {
        let cases: [(UInt16, [String: Int64], Bool)] = [
            (11, Self.withExtras, true),
            (10, Self.withExtras, false),
            (11, Self.everyFeature, false),
            (11, ["remoteMediaVersion": 0, "displayExtrasVersion": 1], false),
        ]
        for (minor, capabilities, open) in cases {
            let gates = MediaFeatureGates(agreedMinor: minor) { capabilities[$0] ?? 0 }
            #expect(gates.displayExtras == open, "minor \(minor), \(capabilities)")
        }
    }

    @Test func withTheGateClosedExtrasAreRefusedLocallyAndNothingIsSent() async throws {
        for (minor, capabilities) in [(UInt16(10), Self.withExtras), (UInt16(11), Self.everyFeature)] {
            let rig = try Rig()
            await rig.open(minor: minor, capabilities: capabilities)
            var subscription = Self.subscription(endpointId: 1)
            subscription.extras = DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 0))
            await #expect(throws: DisplayEndpointRequest.Invalid.displayExtrasUnavailable) {
                try await rig.client.subscribe(subscription)
            }
            #expect(rig.recorder.sent("subscribe").isEmpty)
            #expect(await rig.client.endpointIds.isEmpty)
            // A request with no field asks for nothing and goes as today's.
            subscription.extras = DisplayExtrasRequest()
            try await rig.client.subscribe(subscription)
            let sent = try #require(rig.recorder.sent("subscribe").first)
            #expect(Set(sent.keys) == Set(DisplayEndpointRequest.subscribe(Self.subscription(endpointId: 1),
                                                                          connectionId: "x", gates: .none).keys))
        }
    }

    @Test func withTheGateOpenSubscribeCarriesExactlyTheFieldsAskedFor() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.withExtras)
        #expect(await rig.client.gates.displayExtras)
        var subscription = Self.subscription(endpointId: 1)
        subscription.extras = DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: -3), averageTimeMs: 30)
        try await rig.client.subscribe(subscription)
        let sent = try #require(rig.recorder.sent("subscribe").first)
        let plain = DisplayEndpointRequest.subscribe(Self.subscription(endpointId: 1), connectionId: "x", gates: .none)
        #expect(Set(sent.keys) == Set(plain.keys).union(["noiseFloor", "averageTimeMs"]))
        #expect(sent["noiseFloor"] == .object(["enabled": .bool(true), "shiftDb": .number(-3)]))
        #expect(sent["averageTimeMs"] == .number(30))

        // A value out of range is refused before anything is sent.
        var outside = Self.subscription(endpointId: 2)
        outside.extras = DisplayExtrasRequest(waterfallAverageTimeMs: 10_000)
        await #expect(throws: DisplayEndpointRequest.Invalid.displayExtras(.waterfallAverageTimeMs)) {
            try await rig.client.subscribe(outside)
        }
        #expect(rig.recorder.sent("subscribe").count == 1)
    }

    @Test func extrasReachTheDecoderAgainstTheirEndpointsContextBesideTheirFrame() async throws {
        let vectors = try LinkFixtureLoader.mediaVectors()
        func bytes(_ name: String) throws -> Data {
            try #require(vectors["media-\(name)"]).bytes
        }
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.withExtras)
        let id = try await rig.connectionId
        let peer = try rig.peer
        var subscription = Self.subscription(endpointId: 1)
        subscription.extras = Self.extrasForEverySection
        try await rig.client.subscribe(subscription)
        func frames() -> [UInt32] {
            rig.recorder.events.compactMap { event -> UInt32? in
                if case .displayFrame(let frame) = event { return frame.encoderSequence } else { return nil }
            }
        }
        func extras() -> [DisplayExtras] {
            rig.recorder.events.compactMap { event -> DisplayExtras? in
                if case .displayExtras(let extras) = event { return extras } else { return nil }
            }
        }

        // The fixtures are endpoint 1, context generation 1, -140 to -40 dBm
        // over 32 trace samples, as this rig's context. Before the context
        // there is nothing to decode against.
        // The datagram reaches the client on the peer's stream, so the
        // context waits until the client has refused it.
        peer.receiveDisplay(try bytes("nsdx1-full"))
        var refused = false
        for _ in 0..<20_000 where !refused {
            refused = await rig.client.displayExtrasRefusals[.contextMismatch] == 1
            await Task.yield()
        }
        #expect(refused)
        await rig.deliver(Self.context(id, generation: 1, grant: true))
        peer.receiveDisplay(try bytes("nsdc1-full"))
        peer.receiveDisplay(try bytes("nsdx1-full"))
        await rig.recorder.settle { extras().count == 1 }
        let context = try #require(Self.decoderContext(vectors["media-nsdx1-full"]))
        let expected = try #require(DisplayExtrasDecoder.decode(try bytes("nsdx1-full"), context: context).extras)
        #expect(extras() == [expected])
        #expect(expected.encoderSequence == 1)
        #expect(frames() == [1])
        // Delivered beside its frame: the frame's event comes first.
        let order = rig.recorder.events.compactMap { event -> String? in
            switch event {
            case .displayFrame: return "frame"
            case .displayExtras: return "extras"
            default: return nil
            }
        }
        #expect(order == ["frame", "extras"])
        #expect(await rig.client.displayExtras(endpointId: 1) == expected)
        #expect(await rig.client.displayExtrasRefusals == [.contextMismatch: 1])

        // A frame without its datagram leaves the last extras in place.
        peer.receiveDisplay(try bytes("nsdc1-delta"))
        await rig.recorder.settle { frames().count == 2 }
        #expect(await rig.client.displayExtras(endpointId: 1) == expected)

        // Another endpoint, an unknown section and a cut datagram are
        // refused and change nothing; the next good one is taken.
        var otherEndpoint = try bytes("nsdx1-full")
        otherEndpoint[11] = 2
        peer.receiveDisplay(otherEndpoint)
        peer.receiveDisplay(try bytes("nsdx1-unknown-section"))
        peer.receiveDisplay(try bytes("nsdx1-truncated"))
        peer.receiveDisplay(try bytes("nsdx1-noise-floor"))
        await rig.recorder.settle { extras().count == 2 }
        #expect(extras().last?.encoderSequence == 2)
        #expect(extras().last?.noiseFloorDbm != nil)
        #expect(extras().last?.peakBlobs == nil)
        #expect(await rig.client.displayExtrasRefusals
                == [.contextMismatch: 2, .unknownSections: 1, .truncated: 1])
        #expect(frames() == [1, 2])

        // A new context: the extras of the old one are gone.
        await rig.deliver(Self.context(id, generation: 2, grant: true))
        #expect(await rig.client.displayExtras(endpointId: 1) == nil)
    }

    static let extrasForEverySection = DisplayExtrasRequest(
        peakBlobs: .init(count: 3, holdMs: 0, fallDbPerSec: 0, insideOnly: false),
        activePeakHold: .init(enabled: true, holdMs: 100, fallDbPerSec: 1),
        noiseFloor: .init(enabled: true, shiftDb: 0),
        waterfallLevels: .init(mode: .manual, lowDbm: -130, highDbm: -70, offsetDb: 0))

    /// An nsdx1 vector's context.
    static func decoderContext(_ vector: LinkFixtureLoader.MediaVector?) -> DisplayExtrasDecoder.Context? {
        guard let object = vector?.expect["context"] as? [String: Any],
              let endpointId = (object["endpointId"] as? NSNumber)?.uint32Value,
              let generation = (object["contextGeneration"] as? NSNumber)?.uint32Value,
              let minDbm = (object["minDbm"] as? NSNumber)?.floatValue,
              let maxDbm = (object["maxDbm"] as? NSNumber)?.floatValue,
              let samples = (object["traceSamples"] as? NSNumber)?.intValue else {
            return nil
        }
        return DisplayExtrasDecoder.Context(endpointId: endpointId, contextGeneration: generation,
                                            minDbm: minDbm, maxDbm: maxDbm, traceSamples: samples)
    }

    // MARK: Reconnecting

    @Test func aReconnectRetiresThePeerAndItsSubscriptionsAndStartsAfresh() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let first = try await rig.connectionId
        let firstPeer = try rig.peer
        try await rig.connect()
        await rig.client.setAudioEnabled(true)
        try await rig.client.subscribe(Self.subscription(endpointId: 1))

        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(firstPeer.isClosed)
        #expect(await rig.client.connectionId == nil)
        #expect(await rig.client.endpointIds.isEmpty)
        #expect(rig.playback.anchor == nil)

        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let second = try await rig.connectionId
        #expect(second != first)
        #expect(rig.recorder.sent("start").map { $0["connectionId"] } == [.string(first), .string(second)])
        // The wish for audio carries over, sent once the new connection is up,
        // with a revision above the last.
        #expect(rig.recorder.sent("audio").count == 1)
        // Connected: the audio goes out, then the session is told, so wait
        // for the telling, not the audio.
        try await rig.connect()
        await rig.recorder.waitUntilSent("audio", count: 2)
        let audio = try #require(rig.recorder.sent("audio").last)
        #expect(audio["connectionId"] == .string(second))
        #expect(audio["revision"] == 2)
        #expect(rig.recorder.mediaConnectedCalls == 2)
    }

    @Test func onASessionTheRedialScheduleWaitsForTheMediaConnection() async throws {
        let station = ScriptedStation()
        let clock = ManualLinkClock()
        let session = StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: clock, transportFactory: station.factory)
        let recorder = MediaControlRecorder()
        let client = await MediaControlClient.attached(to: session, peerFactory: recorder.peerFactory)
        let events = EventRecorder(session) { await client.handle($0) }

        // Two failed dials move the schedule on to 5 s.
        station.failOpens(with: .failed("unreachable"))
        await session.connect()
        await clock.advance(by: 1_000)
        station.failOpens(with: nil)
        await clock.advance(by: 2_000)
        let transport = try #require(station.latest)
        await transport.deliver(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd",
                                                        majors: [1], features: [:])))
        await transport.deliver(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        await transport.deliver(.capabilities(Self.capabilities(Self.mediaOnly)))
        await transport.deliver(.snapshotComplete)
        await events.settle { $0.contains(.stateChanged(.ready)) }
        await recorder.settle { recorder.peers.count == 1 }
        // The start went out on the session, as its media.control.
        await recorder.settle { transport.pending.contains { $0.contains("\"op\":\"start\"") } }
        #expect(transport.pending.contains { $0.contains("\"op\":\"start\"") })

        // The media connection comes up: now the schedule starts over.
        try #require(recorder.peers.last).become(.connected)
        await recorder.settle { false }
        await transport.dropLink()
        #expect(await session.state == .waitingToRetry(seconds: 1))
        let peer = try #require(recorder.peers.last)
        await recorder.settle { peer.isClosed }
        #expect(peer.isClosed, "the lost session retired the peer")
    }

    // MARK: The media watchdog

    @Test func aPeerThatNeverConnectsIsRestartedAtTheDeadline() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let first = try await rig.connectionId
        let firstPeer = try rig.peer
        firstPeer.become(.connecting)
        await rig.timers.advance(by: 4_999)
        #expect(rig.recorder.sent("start").count == 1)
        #expect(!firstPeer.isClosed)
        await rig.timers.advance(by: 1)
        #expect(firstPeer.isClosed)
        await rig.recorder.settle { rig.recorder.events.contains(.mediaState(.closed)) }
        #expect(rig.recorder.events.contains(.mediaState(.closed)))
        let second = try await rig.connectionId
        #expect(second != first)
        #expect(rig.recorder.sent("start").map { $0["connectionId"] } == [.string(first), .string(second)])
        // A connection that comes up in time is left alone.
        try await rig.connect()
        await rig.timers.advance(by: 600_000)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(await rig.client.connectionId == second)
    }

    /// A peer through the remote access service (iPhone app plan Task 27a):
    /// its deadline covers gathering (23.5 s) and ICE's 39.5 s checks.
    @Test func aPeerThroughTheServiceWaitsForGatheringAndTheChecks() async throws {
        #expect(IceSettings.connectDeadline == .milliseconds(63_000))
        let rig = try Rig(connectDeadline: IceSettings.connectDeadline)
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let peer = try rig.peer
        peer.become(.connecting)
        await rig.timers.advance(by: 62_999)
        #expect(!peer.isClosed)
        #expect(rig.recorder.sent("start").count == 1)
        await rig.timers.advance(by: 1)
        #expect(peer.isClosed)
    }

    @Test(arguments: [MediaPeer.State.failed, .closed, .disconnected])
    func aPeerThatEndsIsRestartedAtOnce(ending: MediaPeer.State) async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        try await rig.connect()
        await rig.client.setAudioEnabled(true)
        let first = try await rig.connectionId
        let firstPeer = try rig.peer
        firstPeer.become(ending)
        await rig.recorder.waitUntilSent("start", count: 2)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(firstPeer.isClosed)
        // The band and the RX panel hear that the old connection is gone:
        // the events come on their own stream, so wait for the end, then
        // for anything more.
        let isEnd: (MediaControlEvent) -> Bool = { $0 == .mediaState(.closed) || $0 == .mediaState(.failed) }
        await rig.recorder.settle { rig.recorder.events.contains(where: isEnd) }
        await rig.recorder.settle { false }
        #expect(rig.recorder.events.filter(isEnd).count == 1)
        let second = try await rig.connectionId
        #expect(second != first)
        // The new one connects and the sound is asked for again on it. The
        // audio goes out before the session is told it connected, so wait
        // for the telling.
        try await rig.connect()
        await rig.recorder.waitUntilSent("audio", count: 2)
        #expect(rig.recorder.sent("audio").last?["connectionId"] == .string(second))
        #expect(rig.recorder.mediaConnectedCalls == 2)
    }

    @Test func theRestartWaitsDoubleFromOneSecondAndCapAtTen() async throws {
        #expect(MediaControlClient.restartDelay(afterFailures: 1) == .zero)
        #expect((2...8).map { MediaControlClient.restartDelay(afterFailures: $0) }
            == [.seconds(1), .seconds(2), .seconds(4), .seconds(8), .seconds(10), .seconds(10), .seconds(10)])

        // Never connecting: each try gets its 5 s, then the wait.
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        var startsAt: [Int64] = []
        for second in 0...100 {
            if second > 0 {
                await rig.timers.advance(by: 1_000)
            }
            while startsAt.count < rig.recorder.sent("start").count {
                startsAt.append(Int64(second))
            }
        }
        #expect(startsAt == [0, 5, 11, 18, 27, 40, 55, 70, 85, 100])
    }

    @Test func aConnectionThatComesUpStartsTheWaitsOver() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        // Two tries that never connect: the third waits 1 s.
        await rig.timers.advance(by: 10_000)
        #expect(rig.recorder.sent("start").count == 2)
        await rig.timers.advance(by: 1_000)
        #expect(rig.recorder.sent("start").count == 3)
        try await rig.connect()
        try rig.peer.become(.failed)
        await rig.recorder.settle { rig.recorder.sent("start").count == 4 }
        #expect(rig.recorder.sent("start").count == 4, "restarted at once, not after 2 s")
    }

    @Test func aRestartWaitingIsCalledOffWhenTheSessionEnds() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        await rig.timers.advance(by: 10_000)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(await rig.client.connectionId == nil)
        await rig.client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await rig.timers.advance(by: 600_000)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(rig.timers.pendingDueTimes.isEmpty)
    }

    @Test func restartMediaStartsANewConnectionAtOnce() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        try await rig.connect()
        let first = try await rig.connectionId
        let firstPeer = try rig.peer
        await rig.client.restartMedia(because: .networkChanged)
        #expect(firstPeer.isClosed)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(try await rig.connectionId != first)
    }

    @Test func mediaWithdrawnMidSessionRetiresTheConnection() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.everyFeature)
        let peer = try rig.peer
        await rig.client.handle(.message(.capabilities(Self.capabilities(["remoteMediaVersion": 0]))))
        #expect(peer.isClosed)
        #expect(await rig.client.connectionId == nil)
    }

    // MARK: Clarity's Re-tune

    static let withRetune: [String: Int64] = everyFeature.merging(["displayExtrasVersion": 2]) { $1 }

    @Test func theRetuneGateNeedsDisplayExtrasVersion2() {
        let cases: [(UInt16, [String: Int64], Bool)] = [
            (11, Self.withRetune, true),
            (11, Self.withExtras, false),
            (10, Self.withRetune, false),
            (11, ["remoteMediaVersion": 0, "displayExtrasVersion": 2], false),
        ]
        for (minor, capabilities, open) in cases {
            let gates = MediaFeatureGates(agreedMinor: minor) { capabilities[$0] ?? 0 }
            #expect(gates.clarityRetune == open, "minor \(minor), \(capabilities)")
        }
    }

    @Test func aRetuneGoesOnlyToACoreThatOffersItForAnEndpointWithAContext() async throws {
        let older = try Rig()
        await older.open(minor: 11, capabilities: Self.withExtras)
        let olderId = try await older.connectionId
        try await older.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        await older.deliver(Self.context(olderId, grant: true))
        await #expect(throws: MediaControlError.notOffered) { try await older.client.retuneClarity(endpointId: 1) }
        #expect(older.recorder.sent("clarity-retune").isEmpty)

        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.withRetune)
        let id = try await rig.connectionId
        await #expect(throws: MediaControlError.unknownEndpoint) { try await rig.client.retuneClarity(endpointId: 1) }
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        // No context accepted yet.
        await #expect(throws: MediaControlError.notOffered) { try await rig.client.retuneClarity(endpointId: 1) }
        await rig.deliver(Self.context(id, grant: true))
        try await rig.client.retuneClarity(endpointId: 1)
        #expect(rig.recorder.sent("clarity-retune") == [["op": "clarity-retune", "connectionId": .string(id),
                                                         "endpointId": 1]])
    }

    @Test func aRefusedRetuneIsReportedAndTheEndpointStands() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.withRetune)
        let id = try await rig.connectionId
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        await rig.deliver(Self.context(id, grant: true))
        try await rig.client.retuneClarity(endpointId: 1)
        // The Core's refusal names the endpoint with revision 0, in its words.
        await rig.deliver(["op": "rejected", "connectionId": .string(id), "endpointId": 1, "revision": 0,
                           "reason": "Clarity is not setting this display's waterfall levels."])
        await rig.recorder.settle { rig.recorder.events.contains { if case .clarityRetuneRefused = $0 { true } else { false } } }
        #expect(rig.recorder.events.contains(.clarityRetuneRefused(
            MediaControlEvent.Rejection(endpointId: 1, revision: 0,
                                        reason: "Clarity is not setting this display's waterfall levels."))))
        #expect(!rig.recorder.events.contains { if case .rejected = $0 { true } else { false } })
        #expect(await rig.client.endpointIds == [1])
        // With no retune waiting, a revision-0 rejected is neither a refusal
        // reported nor the subscription's own.
        await rig.deliver(["op": "rejected", "connectionId": .string(id), "endpointId": 1, "revision": 0,
                           "reason": "Another refusal."])
        #expect(rig.recorder.events.filter { if case .clarityRetuneRefused = $0 { true } else { false } }.count == 1)
        #expect(await rig.client.endpointIds == [1])
    }

    @Test func aSubscriptionsOwnRejectedRetiresItWhileARetuneWaits() async throws {
        let rig = try Rig()
        // Without the display budget, where a subscription is still refused
        // by rejected (a Core with the budget answers allocation-result).
        await rig.open(minor: 11, capabilities: Self.withRetune.filter { $0.key != "remoteDisplayBudgetVersion" })
        let id = try await rig.connectionId
        try await rig.client.subscribe(Self.subscription(endpointId: 1, revision: 1))
        await rig.deliver(Self.context(id, grant: true))
        try await rig.client.retuneClarity(endpointId: 1)
        // A rejected carrying the subscription's revision is not the retune's.
        await rig.deliver(["op": "rejected", "connectionId": .string(id), "endpointId": 1, "revision": 1,
                           "reason": "That display is no longer open on the Core."])
        await rig.recorder.settle { rig.recorder.events.contains { if case .rejected = $0 { true } else { false } } }
        #expect(rig.recorder.events.contains(.rejected(
            MediaControlEvent.Rejection(endpointId: 1, revision: 1, reason: "That display is no longer open on the Core."))))
        #expect(!rig.recorder.events.contains { if case .clarityRetuneRefused = $0 { true } else { false } })
        #expect(await rig.client.endpointIds == [])
    }

    // MARK: The extended view's ceiling

    @Test func aContextsSpanCeilingIsTheAdcHalfRateOnlyWhileTheExtendedViewIsAvailable() {
        let available = MediaControlEvent.Wideband(available: true, active: true, physicalAdcIndex: 0,
                                                    filterChainIndex: 0, sourceGeneration: 1, adcRateHz: 122_880_000)
        #expect(MediaControlEvent.DisplayContext.spanCeilingHz(sampleRateHz: 192_000, wideband: available) == 61_440_000)
        #expect(MediaControlEvent.DisplayContext.spanCeilingHz(
            sampleRateHz: 192_000, wideband: MediaControlEvent.Wideband(available: false, active: false)) == 192_000)
        #expect(MediaControlEvent.DisplayContext.spanCeilingHz(sampleRateHz: 192_000, wideband: nil) == 192_000)
        // A context wider than its ceiling is refused.
        var payload = Self.context("3f2504e0-4f89-41d3-9a0c-0305e82c3301", wideband: true)
        payload["spanHz"] = 1_000_000
        #expect(MediaControlDecoder.context(payload, wideband: true, grant: false) == nil)
        payload["spanHz"] = 192_000
        #expect(MediaControlDecoder.context(payload, wideband: true, grant: false)?.maxSpanHz == 192_000)
    }
}
