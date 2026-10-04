// NereusSDR for iOS: replacement media ownership and dual receive regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMedia

@Suite struct DualReceiveTests {
    static let replaceFeatures: [String: Int64] = [
        "remoteMediaVersion": 1, "mediaReplaceVersion": 1,
    ]

    private final class Safety: @unchecked Sendable {
        let lock = NSLock()
        private var allowed = false
        func set(_ next: Bool) { lock.withLock { allowed = next } }
        func read() -> Bool { lock.withLock { allowed } }
    }

    private actor PausedSafety {
        private var waiting: CheckedContinuation<Bool, Never>?
        private var entered: CheckedContinuation<Void, Never>?
        private var didEnter = false

        func check() async -> Bool {
            didEnter = true
            entered?.resume()
            entered = nil
            return await withCheckedContinuation { waiting = $0 }
        }

        func waitUntilEntered() async {
            if didEnter { return }
            await withCheckedContinuation { entered = $0 }
        }

        func release() {
            waiting?.resume(returning: true)
            waiting = nil
        }
    }

    private static func packet(_ sequence: UInt16, _ timestamp: UInt32, _ ssrc: UInt32) -> RtpPacket {
        RtpPacket(payloadType: 111, sequence: sequence, timestamp: timestamp,
                  ssrc: ssrc, payload: Data([1]))
    }

    @Test func fasterNewPathHoldsDuplicatesAndFillsOneLostOldPacket() {
        let merge = DualReceiveAudio()
        let old: UInt32 = 17, new: UInt32 = 19
        #expect(merge.submit(Self.packet(101, 2_920, new), stream: 0, fromNew: true, nowMs: 1_020).isEmpty)
        #expect(merge.submit(Self.packet(102, 4_840, new), stream: 0, fromNew: true, nowMs: 1_060).isEmpty)
        #expect(merge.submit(Self.packet(100, 1_000, old), stream: 0, fromNew: false, nowMs: 1_180).count == 1)
        let second = merge.submit(Self.packet(101, 2_920, old), stream: 0, fromNew: false, nowMs: 1_220)
        #expect(second.map(\.sequence) == [101])
        #expect(merge.leadMs == 200)
        #expect(merge.tick(nowMs: 1_259).isEmpty)
        #expect(merge.tick(nowMs: 1_260).map(\.sequence) == [102])
        #expect(merge.submit(Self.packet(102, 4_840, old), stream: 0, fromNew: false, nowMs: 1_300).isEmpty)
    }

    /// Lossless packets are 4 ms, ten to a 40 ms Opus packet, so the
    /// duplicate history covers the same span of audio with ten times as
    /// many timestamps.
    @Test func losslessDuplicateHistoryCoversTheSameSpanAsOpus() {
        for (format, history) in [(AudioStreamFormat.opus, 64), (.l16, 640)] {
            let merge = DualReceiveAudio(audioFormat: format)
            #expect(merge.duplicateHistory == history)
            _ = merge.oldPathDone(nowMs: 0)
            for index in 0..<history {
                let timestamp = UInt32(index * 192)
                #expect(merge.submit(Self.packet(UInt16(index), timestamp, 17), stream: 0,
                                     fromNew: false, nowMs: 0).count == 1)
            }
            #expect(merge.submit(Self.packet(0, 0, 19), stream: 0, fromNew: true, nowMs: 0).isEmpty,
                    "the oldest timestamp is still remembered")
            _ = merge.submit(Self.packet(UInt16(history), UInt32(history * 192), 17), stream: 0,
                             fromNew: false, nowMs: 0)
            #expect(merge.submit(Self.packet(0, 0, 19), stream: 0, fromNew: true, nowMs: 0).count == 1,
                    "one more forgets it")
        }
    }

    @Test func timestampHistoryIsPerStreamAndHeldOrderSurvivesWrap() {
        let merge = DualReceiveAudio()
        let old: UInt32 = 17, new: UInt32 = 19
        #expect(merge.submit(Self.packet(5, UInt32.max, old), stream: 0, fromNew: false, nowMs: 0).count == 1)
        #expect(merge.submit(Self.packet(5, UInt32.max, new), stream: 1, fromNew: true, nowMs: 0).isEmpty)
        #expect(merge.submit(Self.packet(6, 1_919, new), stream: 1, fromNew: true, nowMs: 1).isEmpty)
        let ready = merge.oldPathDone(nowMs: 2)
        #expect(ready.isEmpty)
        let due = merge.tick(nowMs: 601)
        #expect(due.map(\.timestamp) == [UInt32.max, 1_919])
        #expect(merge.submit(Self.packet(7, UInt32.max, old), stream: 0, fromNew: false, nowMs: 602).isEmpty)
    }

    @Test func slowerNewPathNeverCreatesAnArtificialLead() {
        let merge = DualReceiveAudio()
        let old = Self.packet(10, 19_200, 17)
        let copy = Self.packet(10, 19_200, 19)
        #expect(merge.submit(old, stream: 0, fromNew: false, nowMs: 100).map(\.sequence) == [10])
        #expect(merge.submit(copy, stream: 0, fromNew: true, nowMs: 200).isEmpty)
        #expect(merge.leadMs == 0)
        #expect(merge.oldPathDone(nowMs: 300).isEmpty)
        #expect(merge.submit(Self.packet(11, 21_120, 19), stream: 0,
                             fromNew: true, nowMs: 320).map(\.sequence) == [11])
    }

    @Test func equivalentSsrcKeepsTheDecodedQueueAndDeduplicatesByTimestamp() throws {
        let buffer = try AudioJitterBuffer()
        let payload = try AudioJitterBufferTests.opusPayload()
        let old: UInt32 = 17, new: UInt32 = 19
        buffer.reanchor(.init(generation: 1, ssrc: old, firstSequence: 100, firstTimestamp: 192_000))
        for sequence in UInt16(100)...UInt16(104) {
            buffer.push(AudioJitterBufferTests.packet(sequence, ssrc: old, payload: payload))
        }
        #expect(buffer.depthMs == 200)
        buffer.acceptEquivalentSsrc(new)
        buffer.push(AudioJitterBufferTests.packet(105, ssrc: new, payload: payload))
        // A wrong sequence with the same timestamp is still a duplicate.
        let duplicate = RtpPacket(payloadType: 111, sequence: 106, timestamp: UInt32(105) * 1920,
                                  ssrc: old, payload: payload)
        buffer.push(duplicate)
        #expect(buffer.depthMs == 240)
        buffer.adoptEquivalentAnchor(.init(generation: 1, ssrc: new,
                                          firstSequence: 100, firstTimestamp: 192_000))
        #expect(buffer.depthMs == 240)
        if case .audio(let pcm) = buffer.pull() {
            #expect(pcm.count == 1920 * 2)
        } else {
            Issue.record("the first queued block should decode across the SSRC handoff")
        }
        #expect(buffer.depthMs == 200)
        buffer.keepOnlySsrc(new)
        buffer.push(AudioJitterBufferTests.packet(106, ssrc: old, payload: payload))
        #expect(buffer.depthMs == 200)
        buffer.push(AudioJitterBufferTests.packet(106, ssrc: new, payload: payload))
        #expect(buffer.depthMs == 240)
    }

    @Test func skewedOverlapDecodesConsecutiveBlocksWithAtMostOnePacketGap() throws {
        let payload = try AudioJitterBufferTests.opusPayload()
        let buffer = try AudioJitterBuffer()
        let old: UInt32 = 17, new: UInt32 = 19
        buffer.reanchor(.init(generation: 1, ssrc: old, firstSequence: 100, firstTimestamp: 192_000))
        buffer.acceptEquivalentSsrc(new)
        for sequence in UInt16(100)...UInt16(104) {
            buffer.push(AudioJitterBufferTests.packet(sequence, ssrc: old, payload: payload))
        }
        let merge = DualReceiveAudio()
        let new105 = AudioJitterBufferTests.packet(105, ssrc: new, payload: payload)
        let new106 = AudioJitterBufferTests.packet(106, ssrc: new, payload: payload)
        let old105 = AudioJitterBufferTests.packet(105, ssrc: old, payload: payload)
        #expect(merge.submit(new105, stream: 0, fromNew: true, nowMs: 200).isEmpty)
        #expect(merge.submit(new106, stream: 0, fromNew: true, nowMs: 240).isEmpty)
        for packet in merge.submit(old105, stream: 0, fromNew: false, nowMs: 400) { buffer.push(packet) }
        #expect(merge.leadMs == 200)
        for packet in merge.tick(nowMs: 440) { buffer.push(packet) }
        var decoded = 0
        for _ in 0..<7 {
            if case .audio = buffer.pull() { decoded += 1 }
        }
        #expect(decoded == 7)
        #expect(buffer.concealed == 0)
        #expect(buffer.underruns == 0)
    }

    @Test func sixHundredMillisecondSkewEasesWithoutClusteredAudioSkips() throws {
        let payload = try AudioJitterBufferTests.opusPayload()
        let buffer = try AudioJitterBuffer()
        let merge = DualReceiveAudio()
        let old: UInt32 = 17, new: UInt32 = 19
        buffer.reanchor(.init(generation: 1, ssrc: old, firstSequence: 0, firstTimestamp: 0))
        var startedPlaying = false
        var previousSequence: UInt16?
        var lastSkipMs: Int?
        var decoded = 0
        var skips = 0
        var mergeEnded = false

        func makePacket(_ sourceMs: Int, ssrc: UInt32) -> RtpPacket {
            let sequence = UInt16(sourceMs / 40)
            return AudioJitterBufferTests.packet(sequence, ssrc: ssrc, payload: payload)
        }
        for timeMs in stride(from: 0, through: 44_000, by: 40) {
            if timeMs == 2_000 {
                buffer.beginReplacement()
                buffer.acceptEquivalentSsrc(new)
            }
            let steps = merge.easeSteps
            var delivered: [RtpPacket] = []
            if timeMs >= 600, timeMs - 600 < 3_000 {
                let oldPacket = makePacket(timeMs - 600, ssrc: old)
                if timeMs < 2_000 {
                    delivered.append(oldPacket)
                } else {
                    delivered += merge.submit(oldPacket, stream: 0, fromNew: false,
                                              nowMs: UInt64(timeMs))
                }
            }
            if timeMs >= 2_000 {
                delivered += merge.submit(makePacket(timeMs, ssrc: new), stream: 0,
                                          fromNew: true, nowMs: UInt64(timeMs))
            }
            if timeMs == 5_000 {
                delivered += merge.oldPathDone(nowMs: UInt64(timeMs))
            }
            if timeMs >= 2_000 {
                delivered += merge.tick(nowMs: UInt64(timeMs))
            }
            if merge.easeSteps > steps { buffer.replacementLeadEased() }
            for packet in delivered { buffer.push(packet) }
            if !mergeEnded && !merge.active && timeMs > 5_000 {
                buffer.endReplacement()
                buffer.keepOnlySsrc(new)
                mergeEnded = true
            }
            let beforeDrops = buffer.lateDrops
            switch buffer.pull() {
            case .audio:
                startedPlaying = true
                decoded += 1
                let sequence = try #require(buffer.lastPulledSequence)
                if let previousSequence {
                    let distance = Int(sequence &- previousSequence)
                    #expect(distance >= 1 && distance <= 2)
                    if distance == 2 {
                        skips += 1
                        if let lastSkipMs { #expect(timeMs - lastSkipMs >= 2_000) }
                        lastSkipMs = timeMs
                    }
                }
                previousSequence = sequence
            case .silence:
                #expect(!startedPlaying)
            case .concealed:
                Issue.record("overlap must decode a real packet at every playing pull")
            }
            #expect(buffer.lateDrops - beforeDrops <= 1)
        }
        #expect(decoded > 900)
        #expect(merge.leadMs == 0)
        #expect(merge.easeSteps == 15)
        #expect(mergeEnded)
        #expect((14...15).contains(skips)) // one step had no excess buffered packet to shed
        #expect(buffer.underruns == 0)
        #expect(buffer.depthMs <= buffer.targetMs + 120)
    }

    @Test func legacyCoreKeepsOldMediaButRemembersNewRouteFactory() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1])
        try await rig.connect()
        let oldId = try await rig.connectionId
        let newPeer = ScriptedMediaPeer()
        await rig.client.controlRouteDidMove(peerFactory: { newPeer }, connectDeadline: .seconds(8),
                                             safeToReplace: { true })
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(await rig.client.connectionId == oldId)
        await rig.client.restartMedia()
        #expect(await rig.client.connectionId != oldId)
    }

    @Test func wrongTypedReplacementCapabilityDoesNotEnableTheGate() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: ["remoteMediaVersion": 1])
        try await rig.connect()
        await rig.client.handle(.message(.capabilities(.init(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "mediaReplaceVersion", value: .enumeration(1)),
        ]))))
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(!(await rig.client.gates.mediaReplace))
    }

    @Test func staleAsyncSafetyResultCannotReviveMediaAfterSessionEnd() async throws {
        let rig = try MediaControlClientTests.Rig()
        let gate = PausedSafety()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let move = Task {
            await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                                 connectDeadline: .seconds(5),
                                                 safeToReplace: { await gate.check() })
        }
        await gate.waitUntilEntered()
        await rig.client.handle(.stateChanged(.stopped))
        await gate.release()
        await move.value
        #expect(rig.recorder.sent("replace").isEmpty)
        #expect(await rig.client.connectionId == nil)
    }

    @Test func pendingMovePollsSafetyWithoutSpendingARefusal() async throws {
        let rig = try MediaControlClientTests.Rig()
        let safety = Safety()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        let oldId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5),
                                             safeToReplace: { safety.read() })
        await rig.timers.advance(by: 500)
        #expect(rig.recorder.sent("replace").isEmpty)
        try await rig.connect()
        await rig.timers.advance(by: 500)
        #expect(rig.recorder.sent("replace").isEmpty)
        safety.set(true)
        await rig.timers.advance(by: 500)
        let sent = try #require(rig.recorder.sent("replace").first)
        #expect(Set(sent.keys) == ["op", "connectionId", "replaces"])
        #expect(sent["replaces"] == .string(oldId))
        #expect(await rig.client.connectionId == oldId)
    }

    @Test func enablingAudioDuringOverlapKeepsNewEligibleAndNewGenerationResetsOldQueue() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let newId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(MediaControlClientTests.audioContext(oldId, revision: 1, generation: 1,
                                                               ssrc: MediaControlClient.audioSsrc(forConnection: oldId)))
        rig.playback.flush()
        let newSsrc = MediaControlClient.audioSsrc(forConnection: newId)
        let first = Self.packet(100, 0, newSsrc)
        #expect(rig.playback.withJitterBuffer { jitter in
            jitter.push(first)
            return jitter.depthMs
        } == 40)

        var changed = MediaControlClientTests.audioContext(oldId, revision: 1, generation: 2,
                                                            ssrc: MediaControlClient.audioSsrc(forConnection: oldId),
                                                            firstSequence: 200)
        changed["firstTimestamp"] = .number(384_000)
        await rig.deliver(changed)
        rig.playback.flush()
        #expect(rig.playback.withJitterBuffer { $0.depthMs } == 0)
        let next = Self.packet(200, 384_000, newSsrc)
        #expect(rig.playback.withJitterBuffer { jitter in
            jitter.push(next)
            return jitter.depthMs
        } == 40)
    }

    @Test func newestRouteSupersedesPendingAndQueuesBehindAnInflightReplacement() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.setAudioEnabled(true)
        await rig.deliver(MediaControlClientTests.audioContext(oldId, revision: 1, generation: 1,
                                                               ssrc: MediaControlClient.audioSsrc(forConnection: oldId)))
        let unused = ScriptedMediaPeer()
        let first = ScriptedMediaPeer()
        let latest = ScriptedMediaPeer()
        await rig.client.controlRouteDidMove(peerFactory: { unused }, connectDeadline: .seconds(5),
                                             safeToReplace: { false })
        await rig.client.controlRouteDidMove(peerFactory: { first }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        let firstId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        #expect(firstId != oldId)
        await rig.client.controlRouteDidMove(peerFactory: { latest }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        #expect(rig.recorder.sent("replace").count == 1)
        await rig.deliver(["op": "replace", "connectionId": .string(firstId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == firstId)
        #expect(rig.recorder.sent("replace").count == 1, "the prior peer still has its drain")
        await rig.timers.advance(by: 2_000)
        #expect(rig.recorder.sent("replace").count == 2)
        let latestId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        #expect(latestId != firstId)
        await rig.deliver(["op": "rejected", "connectionId": .string(latestId),
                           "endpointId": 0, "revision": 0,
                           "reason": "The Core did not move audio and display: that connection is not the current one."])
        #expect(await rig.client.connectionId == firstId)
        #expect(first.expectedSsrc == .some(MediaControlClient.audioSsrc(forConnection: firstId)))
        let bPacket = Self.packet(100, 0, MediaControlClient.audioSsrc(forConnection: firstId))
        let acceptedDepth = rig.playback.withJitterBuffer { jitter in
            jitter.push(bPacket)
            return jitter.depthMs
        }
        #expect(acceptedDepth == 40)
        #expect(!unused.isClosed) // no peer was allocated from the superseded factory
        #expect(!first.isClosed) // now current; Core has not acknowledged the next move
        #expect(latest.isClosed)
    }

    @Test func nextMoveWaitsForOldDrainAndMeasuredLeadEasing() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        let old = try rig.peer
        await rig.client.setAudioEnabled(true)
        await rig.deliver(MediaControlClientTests.audioContext(oldId, revision: 1, generation: 1,
                                                               ssrc: MediaControlClient.audioSsrc(forConnection: oldId)))
        let b = ScriptedMediaPeer()
        let c = ScriptedMediaPeer()
        await rig.client.controlRouteDidMove(peerFactory: { b }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        let bId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        rig.recorder.advanceClock(by: 100)
        b.receiveAudio(Self.packet(100, 0, MediaControlClient.audioSsrc(forConnection: bId)))
        for _ in 0..<2_000 {
            if await rig.client.replacementAudioHeld == 1 { break }
            await Task.yield()
        }
        #expect(await rig.client.replacementAudioHeld == 1)
        rig.recorder.advanceClock(by: 200)
        old.receiveAudio(Self.packet(100, 0, MediaControlClient.audioSsrc(forConnection: oldId)))
        for _ in 0..<2_000 {
            if await rig.client.replacementAudioLeadMs == 200 { break }
            await Task.yield()
        }
        #expect(await rig.client.replacementAudioLeadMs == 200)
        await rig.deliver(["op": "replace", "connectionId": .string(bId), "replaces": .string(oldId)])
        await rig.client.controlRouteDidMove(peerFactory: { c }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        rig.recorder.advanceClock(by: 2_000)
        await rig.timers.advance(by: 2_000)
        #expect(rig.recorder.sent("replace").count == 1)
        for step in 1...5 {
            rig.recorder.advanceClock(by: 2_000)
            await rig.timers.advance(by: 2_000)
            #expect(rig.recorder.sent("replace").count == (step == 5 ? 2 : 1))
        }
        #expect(await rig.client.connectionId == bId)
        #expect(old.isClosed)
        #expect(!b.isClosed)
        #expect(!c.isClosed)
    }

    @Test func newAudioContextClearingOldMergeRearmsQueuedMove() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let aId = try await rig.connectionId
        let a = try rig.peer
        await rig.client.setAudioEnabled(true)
        await rig.deliver(MediaControlClientTests.audioContext(aId, revision: 1, generation: 1,
                                                               ssrc: MediaControlClient.audioSsrc(forConnection: aId)))
        let b = ScriptedMediaPeer()
        let c = ScriptedMediaPeer()
        await rig.client.controlRouteDidMove(peerFactory: { b }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        let bId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        rig.recorder.advanceClock(by: 100)
        b.receiveAudio(Self.packet(100, 0, MediaControlClient.audioSsrc(forConnection: bId)))
        for _ in 0..<2_000 {
            if await rig.client.replacementAudioHeld == 1 { break }
            await Task.yield()
        }
        #expect(await rig.client.replacementAudioHeld == 1)
        rig.recorder.advanceClock(by: 200)
        a.receiveAudio(Self.packet(100, 0, MediaControlClient.audioSsrc(forConnection: aId)))
        for _ in 0..<2_000 {
            if await rig.client.replacementAudioLeadMs == 200 { break }
            await Task.yield()
        }
        #expect(await rig.client.replacementAudioLeadMs == 200)
        await rig.deliver(["op": "replace", "connectionId": .string(bId), "replaces": .string(aId)])
        await rig.client.controlRouteDidMove(peerFactory: { c }, connectDeadline: .seconds(5),
                                             safeToReplace: { true })
        rig.recorder.advanceClock(by: 2_000)
        await rig.timers.advance(by: 2_000)
        #expect(rig.recorder.sent("replace").count == 1)
        #expect(await rig.client.replacementAudioLeadMs == 200)
        #expect(a.isClosed)

        // B's new context ends the obsolete A/B merge before its 10-second
        // measured-lead easing would finish. C must regain a polling path.
        await rig.deliver(MediaControlClientTests.audioContext(bId, revision: 1, generation: 2,
                                                               ssrc: MediaControlClient.audioSsrc(forConnection: bId),
                                                               enabled: false))
        #expect(await rig.client.connectionId == bId)
        #expect(rig.recorder.sent("replace").count == 1)
        rig.recorder.advanceClock(by: 500)
        await rig.timers.advance(by: 500)
        let next = try #require(rig.recorder.sent("replace").last)
        #expect(rig.recorder.sent("replace").count == 2)
        #expect(next["replaces"] == .string(bId))
        #expect(await rig.client.connectionId == bId)
        #expect(!b.isClosed)
        #expect(!c.isClosed)
    }

    @Test func coreOnAirRefusalRearmsAtMostThreeTimes() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        for attempt in 1...4 {
            let request = try #require(rig.recorder.sent("replace").last)
            let newId = try #require(MediaControlDecoder.string(request["connectionId"]))
            await rig.deliver(["op": "rejected", "connectionId": .string(newId),
                               "endpointId": 0, "revision": 0,
                               "reason": "The Core did not move audio and display: the radio is transmitting."])
            await rig.timers.advance(by: 500)
            #expect(rig.recorder.sent("replace").count == min(attempt + 1, 4))
            #expect(await rig.client.connectionId == oldId)
        }
    }

    @Test func rejectedOrFailedNewLeavesOldAndLateCallbacksCannotPromote() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let first = try #require(rig.recorder.peers.last)
        let firstId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        first.become(.failed)
        await rig.recorder.settle { first.isClosed }
        #expect(await rig.client.connectionId == oldId)
        await rig.deliver(["op": "replace", "connectionId": .string(firstId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == oldId)
    }

    @Test func readyNewWaitsForCoreAckBeyondConnectDeadline() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let next = try #require(rig.recorder.peers.last)
        let newId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        next.become(.connected)
        await rig.recorder.settle { !rig.timers.pendingDueTimes.contains(5_000) }
        await rig.timers.advance(by: 8_000) // Core may hold overlap while its MOX state is not Rx.
        #expect(!next.isClosed)
        #expect(await rig.client.connectionId == oldId)
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
    }

    @Test func oldFailureBeforeAckRestartsBothWhileDrainingOldFailureDoesNot() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures)
        try await rig.connect()
        let oldId = try await rig.connectionId
        let old = try rig.peer
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let failedCandidate = try #require(rig.recorder.peers.last)
        old.become(.failed)
        await rig.recorder.settle { old.isClosed && failedCandidate.isClosed && rig.recorder.peers.count == 3 }
        #expect(await rig.client.connectionId != oldId)

        let fresh = try #require(rig.recorder.peers.last)
        fresh.become(.connected)
        await rig.recorder.settle { rig.recorder.mediaConnectedCalls >= 2 }
        let freshId = try await rig.connectionId
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let nextId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        await rig.deliver(["op": "replace", "connectionId": .string(nextId), "replaces": .string(freshId)])
        fresh.become(.failed) // the draining ID is no longer the owner
        await rig.timers.advance(by: 2_000)
        #expect(await rig.client.connectionId == nextId)
    }

    @Test func coreAckMovesSingleUplinkWithoutResetAndDrainsOld() async throws {
        let rig = try MediaControlClientTests.Rig()
        await rig.open(minor: 11, capabilities: Self.replaceFeatures.merging(["remoteTxVersion": 1]) { _, new in new })
        let oldId = try await rig.connectionId
        let old = try rig.peer
        old.openMicrophoneLine()
        try await rig.connect()
        #expect(rig.client.uplink.sendMicrophone(Data([1])))
        let first = try #require(old.microphonePackets.last)
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let next = try #require(rig.recorder.peers.last)
        let newId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        next.openMicrophoneLine()
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        #expect(rig.client.uplink.sendMicrophone(Data([2])))
        let second = try #require(next.microphonePackets.last)
        #expect(second.sequence == first.sequence &+ 1)
        #expect(second.timestamp == first.timestamp &+ MediaUplink.frameSamples)
        #expect(old.microphonePackets.count == 1)
        #expect(next.microphonePackets.count == 1)
        #expect(!old.isClosed)
        next.become(.connected) // late state callback cannot reattach/reset RTP
        await rig.recorder.settle { next.isClosed == false }
        #expect(rig.client.uplink.sendMicrophone(Data([3])))
        #expect(next.microphonePackets.last?.sequence == second.sequence &+ 1)
        await rig.timers.advance(by: 2_000)
        #expect(old.isClosed)
        #expect(!next.isClosed)
    }

    /// R-IOS-09: a media replacement while lossless plays keeps the playout
    /// clock, as an Opus one does. The new peer's context carries the same
    /// first sequence and timestamp, so it is the same stream and nothing
    /// starts over.
    @Test func aLosslessReplacementKeepsPlayingWithoutStartingOver() async throws {
        let rig = try MediaControlClientTests.Rig()
        let features = Self.replaceFeatures.merging(AudioQualityMediaTests.capabilities()) { _, new in new }
        await rig.open(minor: 11, capabilities: features)
        try await rig.connect()
        let oldId = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(AudioQualityMediaTests.context(oldId, revision: try AudioQualityMediaTests.lastRevision(rig),
                                                         generation: 1, profile: "lossless"))
        #expect(rig.playback.anchor?.format == .l16)
        await rig.client.controlRouteDidMove(peerFactory: rig.recorder.peerFactory,
                                             connectDeadline: .seconds(5), safeToReplace: { true })
        let newId = try #require(MediaControlDecoder.string(rig.recorder.sent("replace").last?["connectionId"]))
        rig.playback.flush()
        let newSsrc = MediaControlClient.audioSsrc(forConnection: newId)
        for sequence in UInt16(100)..<110 {
            let packet = RtpPacket(payloadType: 96, sequence: sequence, timestamp: UInt32(sequence - 100) * 192,
                                   ssrc: newSsrc, payload: Data(count: 768))
            rig.playback.withJitterBuffer { $0.push(packet) }
        }
        #expect(rig.playback.withJitterBuffer { $0.depthMs } == 40)
        await rig.deliver(["op": "replace", "connectionId": .string(newId), "replaces": .string(oldId)])
        #expect(await rig.client.connectionId == newId)
        await rig.deliver(AudioQualityMediaTests.context(newId, revision: try AudioQualityMediaTests.lastRevision(rig),
                                                         generation: 2, profile: "lossless"))
        rig.playback.flush()
        #expect(rig.playback.anchor?.ssrc == newSsrc)
        #expect(rig.playback.anchor?.format == .l16)
        #expect(rig.playback.withJitterBuffer { $0.depthMs } == 40, "the queue was kept, not started over")
    }
}
