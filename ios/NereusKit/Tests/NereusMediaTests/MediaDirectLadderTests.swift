// NereusSDR for iOS: the direct media ladder: the direct-only schedule, the no-packets fallback and its recovery
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-16: the phone's half of direct-only media (the link document,
/// sections 6.1 and 6.3). Every timer runs on the rig's manual clock and
/// every age on the recorder's clock, so nothing here sleeps.
@Suite struct MediaDirectLadderTests {
    typealias Rig = MediaControlClientTests.Rig

    /// The ladder's peers and the transmit state it reads.
    final class Ladder: @unchecked Sendable {
        private let lock = NSLock()
        private var current = MediaLadderState(keyed: false, voxArmed: false, coreOnAir: false,
                                               controlRelayed: false)
        private var directMade: [ScriptedMediaPeer] = []
        private var fallbackMade: [ScriptedMediaPeer] = []

        var peers: DirectLadderPeers {
            DirectLadderPeers(
                direct: { [self] in
                    let peer = ScriptedMediaPeer()
                    lock.withLock { directMade.append(peer) }
                    return peer
                },
                tunnelAlone: { [self] in
                    let peer = ScriptedMediaPeer()
                    lock.withLock { fallbackMade.append(peer) }
                    return peer
                },
                state: { [self] in lock.withLock { current } })
        }

        func update(_ change: (inout MediaLadderState) -> Void) { lock.withLock { change(&current) } }
        var directs: [ScriptedMediaPeer] { lock.withLock { directMade } }
        var fallbacks: [ScriptedMediaPeer] { lock.withLock { fallbackMade } }
    }

    static let offered: [String: Int64] = ["remoteMediaVersion": 1, "mediaReplaceVersion": 1,
                                           "mediaDirectVersion": 1]
    static let onAirRefusal = "The Core did not move audio and display: the radio is transmitting."

    /// Media up on the given path with the ladder set.
    private func open(onTunnel: Bool, offered: [String: Int64] = Self.offered)
        async throws -> (Rig, Ladder) {
        let rig = try Rig()
        let ladder = Ladder()
        await rig.open(minor: 11, capabilities: offered)
        await rig.client.useDirectLadder(ladder.peers)
        try rig.peer.selectTunnel(onTunnel)
        try await rig.connect()
        return (rig, ladder)
    }

    private func advance(_ rig: Rig, by ms: UInt64) async {
        rig.recorder.advanceClock(by: ms)
        await rig.timers.advance(by: Int64(ms))
    }

    private func refuse(_ rig: Rig, _ id: String?, reason: String) async {
        await rig.deliver(["op": "rejected", "connectionId": .string(id ?? ""), "endpointId": 0, "revision": 0,
                           "reason": .string(reason)])
    }

    private func lastReplaceId(_ rig: Rig) -> String? {
        MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"])
    }

    /// Audio wanted, a context, and one packet heard on the current peer.
    private func hearAudio(_ rig: Rig) async throws {
        let id = try await rig.connectionId
        await rig.client.setAudioEnabled(true)
        let ssrc = MediaControlClient.audioSsrc(forConnection: id)
        await rig.deliver(MediaControlClientTests.audioContext(id, revision: 1, generation: 1, ssrc: ssrc))
        try rig.peer.receiveAudio(RtpPacket(payloadType: 111, sequence: 100, timestamp: 0, ssrc: ssrc,
                                            payload: Data([0xf8, 0xff, 0xfe])))
        await heard(rig)
    }

    /// Waits until the client has stamped a packet at the recorder's now.
    private func heard(_ rig: Rig) async {
        let now = rig.recorder.clock()
        var stamp: UInt64?
        for _ in 0..<20_000 {
            stamp = await rig.client.ladderMediaStamp
            if stamp == now { break }
            await Task.yield()
        }
        #expect(stamp == now)
    }

    // MARK: The direct-only schedule

    @Test func theStepsAre5_30_120And300SecondsAndTheLastRepeats() async throws {
        #expect(MediaDirectLadder.directSteps == [.seconds(5), .seconds(30), .seconds(120), .seconds(300)])
        let (rig, ladder) = try await open(onTunnel: true)
        let oldId = try await rig.connectionId
        var sent = 0
        var waited: UInt64 = 0
        for (index, seconds) in [5, 30, 120, 300, 300].enumerated() {
            await advance(rig, by: UInt64(seconds) * 1_000 - 1 - waited)
            #expect(rig.recorder.sent("replace").count == sent, "step \(index) went early")
            await advance(rig, by: 1)
            sent += 1
            #expect(rig.recorder.sent("replace").count == sent, "step \(index) did not go")
            let replace = try #require(rig.recorder.sent("replace").last)
            #expect(Set(replace.keys) == ["op", "connectionId", "replaces", "mediaDirectVersion"])
            #expect(replace["mediaDirectVersion"] == .number(1))
            #expect(replace["replaces"] == .string(oldId))
            #expect(ladder.directs.count == sent)
            #expect(await rig.client.directLadderStep == min(index + 1, 3))
            // Even the on-air refusal leaves nothing pending: the next step waits.
            await refuse(rig, lastReplaceId(rig), reason: Self.onAirRefusal)
            #expect(ladder.directs.last?.isClosed == true)
            waited = 499
            await advance(rig, by: waited)
            #expect(rig.recorder.sent("replace").count == sent, "a refused direct replace was re-pended")
        }
        #expect(await rig.client.connectionId == oldId)
    }

    @Test func aStepIsSkippedWhileKeyedVoxArmedOrOnTheAirAndTheScheduleStillMoves() async throws {
        let (rig, ladder) = try await open(onTunnel: true)
        ladder.update { $0.keyed = true }
        await advance(rig, by: 5_000)
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(await rig.client.directLadderStep == 1)
        ladder.update { $0.keyed = false; $0.voxArmed = true }
        await advance(rig, by: 30_000)
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(await rig.client.directLadderStep == 2)
        ladder.update { $0.voxArmed = false; $0.coreOnAir = true }
        await advance(rig, by: 120_000)
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(await rig.client.directLadderStep == 3)
        ladder.update { $0.coreOnAir = false }
        await advance(rig, by: 299_999)
        #expect(rig.recorder.sent("replace").isEmpty)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("replace").count == 1)
        #expect(ladder.directs.count == 1)
    }

    @Test func aNewMediaStartBeginsTheScheduleAgain() async throws {
        let (rig, _) = try await open(onTunnel: true)
        await advance(rig, by: 5_000)
        await refuse(rig, lastReplaceId(rig), reason: "The Core could not open that connection.")
        await advance(rig, by: 30_000)
        await refuse(rig, lastReplaceId(rig), reason: "The Core could not open that connection.")
        #expect(await rig.client.directLadderStep == 2)
        await rig.client.restartMedia()
        #expect(await rig.client.directLadderStep == 0)
        try rig.peer.selectTunnel(true)
        try await rig.connect()
        await advance(rig, by: 4_999)
        #expect(rig.recorder.sent("replace").count == 2)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("replace").count == 3)
    }

    @Test func theScheduleRunsOnlyOnTheTunnelAndOnlyWithACoreThatOffersIt() async throws {
        let (direct, directLadder) = try await open(onTunnel: false)
        await advance(direct, by: 600_000)
        #expect(direct.recorder.sent("replace").isEmpty)
        #expect(directLadder.directs.isEmpty)
        var older = Self.offered
        older["mediaDirectVersion"] = nil
        let (plain, plainLadder) = try await open(onTunnel: true, offered: older)
        #expect(!(await plain.client.gates.mediaDirect))
        await advance(plain, by: 600_000)
        #expect(plain.recorder.sent("replace").isEmpty)
        #expect(plainLadder.directs.isEmpty)
    }

    @Test func anAcceptedDirectReplaceMovesMediaAndEndsTheSchedule() async throws {
        let (rig, ladder) = try await open(onTunnel: true)
        let oldId = try await rig.connectionId
        await advance(rig, by: 5_000)
        let newId = try #require(lastReplaceId(rig))
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        #expect(ladder.directs.count == 1)
        await advance(rig, by: 600_000)
        #expect(rig.recorder.sent("replace").count == 1)
    }

    @Test func theCoresOwnReplaceIsStillDropped() async throws {
        let (rig, _) = try await open(onTunnel: true)
        let oldId = try await rig.connectionId
        await rig.deliver(["op": "replace", "connectionId": .string(UUID().uuidString.lowercased()),
                           "replaces": .string(oldId), "mediaDirectVersion": 1])
        #expect(await rig.client.connectionId == oldId)
    }

    // MARK: The no-packets fallback

    @Test func fiveSecondsWithoutMediaOnADirectPathMovesBackToTheTunnelThenRecovers() async throws {
        #expect(MediaDirectLadder.silenceFallbackMs == 5_000)
        let (rig, ladder) = try await open(onTunnel: false)
        let oldId = try await rig.connectionId
        try await hearAudio(rig)
        await advance(rig, by: 4_999)
        #expect(rig.recorder.sent("replace").isEmpty)
        await advance(rig, by: 1)
        let fallback = try #require(rig.recorder.sent("replace").last)
        #expect(Set(fallback.keys) == ["op", "connectionId", "replaces"])
        #expect(fallback["replaces"] == .string(oldId))
        #expect(ladder.fallbacks.count == 1)
        #expect(ladder.directs.isEmpty)
        // Once per silence: a refused fallback is not tried again; a window
        // later, a new media start.
        await refuse(rig, lastReplaceId(rig), reason: "The Core could not open that connection.")
        #expect(rig.recorder.sent("start").count == 1)
        await advance(rig, by: 4_999)
        #expect(rig.recorder.sent("replace").count == 1)
        #expect(rig.recorder.sent("start").count == 1)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(rig.recorder.sent("replace").count == 1)
    }

    @Test func mediaAfterAnAcceptedFallbackStopsTheRecoveryAndTheScheduleStartsAtFiveSeconds() async throws {
        let (rig, ladder) = try await open(onTunnel: false)
        let oldId = try await rig.connectionId
        try await hearAudio(rig)
        await advance(rig, by: 5_000)
        let newId = try #require(lastReplaceId(rig))
        let tunnel = try #require(ladder.fallbacks.first)
        tunnel.selectTunnel(true)
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        let ssrc = MediaControlClient.audioSsrc(forConnection: newId)
        tunnel.receiveAudio(RtpPacket(payloadType: 111, sequence: 1, timestamp: 0, ssrc: ssrc,
                                      payload: Data([0xf8, 0xff, 0xfe])))
        await heard(rig)
        // Audio every 2 s keeps the tunnel's 3 s stall rule quiet too.
        for sequence in UInt16(2)...3 {
            await advance(rig, by: 2_000)
            tunnel.receiveAudio(RtpPacket(payloadType: 111, sequence: sequence, timestamp: 960,
                                          ssrc: ssrc, payload: Data([0xf8, 0xff, 0xfe])))
            await heard(rig)
        }
        await advance(rig, by: 999)
        #expect(rig.recorder.sent("replace").count == 1)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("start").count == 1, "media arrived, so no recovery")
        let direct = try #require(rig.recorder.sent("replace").last)
        #expect(direct["mediaDirectVersion"] == .number(1), "the schedule starts again at its first step")
    }

    @Test func aFallbackNotReadyByItsDeadlineAsksForRecoveryAWindowAfterItFinished() async throws {
        let (rig, ladder) = try await open(onTunnel: false)
        try await hearAudio(rig)
        // VOX armed holds the direct-only steps, not the fallback or recovery.
        ladder.update { $0.voxArmed = true }
        await advance(rig, by: 5_000)
        #expect(rig.recorder.sent("replace").count == 1)
        let fallback = try #require(ladder.fallbacks.first)
        await advance(rig, by: 5_000)
        #expect(fallback.isClosed, "the replacement's own deadline ends it")
        #expect(rig.recorder.sent("start").count == 1)
        await advance(rig, by: 4_999)
        #expect(rig.recorder.sent("start").count == 1)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("start").count == 2)
        #expect(rig.recorder.sent("replace").count == 1, "once per silence")
    }

    @Test func aSilentAcceptedFallbackOnTheTunnelKeepsTheStallRule() async throws {
        let (rig, ladder) = try await open(onTunnel: false)
        let oldId = try await rig.connectionId
        try await hearAudio(rig)
        await advance(rig, by: 5_000)
        let newId = try #require(lastReplaceId(rig))
        try #require(ladder.fallbacks.first).selectTunnel(true)
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        await advance(rig, by: 2_999)
        #expect(rig.recorder.sent("start").count == 1)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("start").count == 2, "the tunnel's 3 s stall rule comes first")
    }

    @Test func nothingFallsBackWhileKeyedOrOnTheAirAndReceiveRestartsTheWindow() async throws {
        let (rig, ladder) = try await open(onTunnel: false)
        try await hearAudio(rig)
        ladder.update { $0.keyed = true; $0.coreOnAir = true }
        await advance(rig, by: 5_000)
        await advance(rig, by: 30_000)
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(rig.recorder.sent("start").count == 1)
        // The client's own transmit silence: the check waits, and the return
        // to receive starts a fresh window.
        await rig.client.setExpectedAudioSilenceDuringTransmit(true)
        ladder.update { $0.keyed = false; $0.coreOnAir = false }
        await advance(rig, by: 30_000)
        #expect(rig.recorder.sent("replace").isEmpty)
        await rig.client.setExpectedAudioSilenceDuringTransmit(false)
        await advance(rig, by: 4_999)
        #expect(rig.recorder.sent("replace").isEmpty)
        await advance(rig, by: 1)
        #expect(rig.recorder.sent("replace").count == 1)
    }

    @Test func vOXArmedAloneDoesNotHoldTheFallback() async throws {
        let (rig, ladder) = try await open(onTunnel: false)
        try await hearAudio(rig)
        ladder.update { $0.voxArmed = true }
        await advance(rig, by: 5_000)
        #expect(rig.recorder.sent("replace").count == 1)
    }

    @Test func noFallbackWithoutReceiveAudioOrBeforeAnyWasHeardOrOnARelayedControl() async throws {
        let (quiet, _) = try await open(onTunnel: false)
        await quiet.client.setAudioEnabled(true)
        await advance(quiet, by: 60_000)
        #expect(quiet.recorder.sent("replace").isEmpty, "no audio heard since the start")

        let (off, _) = try await open(onTunnel: false)
        try await hearAudio(off)
        await off.client.setAudioEnabled(false)
        await advance(off, by: 60_000)
        #expect(off.recorder.sent("replace").isEmpty, "receive audio not wanted")

        let (relayed, relayedLadder) = try await open(onTunnel: false)
        try await hearAudio(relayed)
        relayedLadder.update { $0.controlRelayed = true }
        await advance(relayed, by: 60_000)
        #expect(relayed.recorder.sent("replace").isEmpty, "a relayed control keeps the stall rule")
    }

    // MARK: Pieces

    @Test func theCoresStunListKeepsOnlyStunUrlsInItsOrder() {
        #expect(MediaDirectLadder.stunUrls(fromCapability:
            #"["stun:stun.example.net:3478","turn:relay.example.net","stuns:b.example.org:5349","stun:u@h.example","stun:h.example?x=1"]"#)
            == ["stun:stun.example.net:3478", "stuns:b.example.org:5349"])
        #expect(MediaDirectLadder.stunUrls(fromCapability: "[]") == [])
        #expect(MediaDirectLadder.stunUrls(fromCapability: "stun:stun.example.net") == nil)
        #expect(MediaDirectLadder.stunUrls(fromCapability: #"[1]"#) == nil)
    }

    @Test func theDirectGateNeedsReplaceMinor11AndVersion1() {
        let all: [String: Int64] = Self.offered
        #expect(MediaFeatureGates(agreedMinor: 11) { all[$0] ?? 0 }.mediaDirect)
        #expect(!MediaFeatureGates(agreedMinor: 10) { all[$0] ?? 0 }.mediaDirect)
        #expect(!MediaFeatureGates(agreedMinor: 11) { $0 == "mediaDirectVersion" ? 2 : all[$0] ?? 0 }.mediaDirect)
        #expect(!MediaFeatureGates(agreedMinor: 11) { $0 == "mediaReplaceVersion" ? 0 : all[$0] ?? 0 }.mediaDirect)
    }

    @Test func theTunnelAloneConnectionTakesNoneOfTheCoresCandidates() {
        let peer = MediaPeer(configuration: .tunnelAlone)
        defer { peer.close() }
        #expect(MediaPeer.Configuration.tunnelAlone.onlyRouteCandidate)
        #expect(MediaPeer.Configuration.tunnelAlone.iceServers.isEmpty)
        #expect(throws: MediaPeerError.notRouteCandidate) {
            try peer.addRemoteCandidate("candidate:1 1 UDP 2122260223 192.0.2.10 50000 typ host", mid: "audio")
        }
    }
}
