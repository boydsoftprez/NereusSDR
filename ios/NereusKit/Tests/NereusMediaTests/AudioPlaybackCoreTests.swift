// NereusSDR for iOS: the band player with the small output pieces a 5 ms or 10 ms I/O buffer gives, its dry-render count and its log line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import CAudioRing
import Foundation
import Testing
@testable import NereusMedia

/// TestFlight build 5 (2026-09-27): the band sounded robotic after the
/// microphone asked the shared audio session for a 10 ms I/O buffer. These
/// drive the playback core by hand on a simulated clock, as the output and
/// the feed timer would, with the output asking for 5 ms and 10 ms pieces.
@Suite struct AudioPlaybackCoreTests {
    static let ssrc: UInt32 = 0x0A0B_0C0D
    /// The feed timer's interval, in frames: 10 ms.
    static let feedFrames = 480

    /// Plays `seconds` of a steady stream (one 40 ms packet per 40 ms,
    /// each up to 30 ms late) into `core`, rendering `callbackFrames` at a
    /// time through a buffer list and running a feed check every 10 ms of
    /// output. From `stallAfterFrames` on, when given, nothing reaches the
    /// feed queue at all: no packets (each packet's arrival also runs a
    /// feed check) and no feed checks, as when the queue is held up. Returns
    /// whether any sound came out after the first second, and the most the
    /// ring held after a feed check from then on.
    @discardableResult
    static func play(_ core: AudioPlaybackCore, callbackFrames: Int, seconds: Int,
                     stallAfterFrames: Int? = nil) async throws -> (heard: Bool, mostHeldFrames: Int) {
        let payload = try AudioJitterBufferTests.opusPayload()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: ssrc, firstSequence: 0, firstTimestamp: 0))
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(callbackFrames)))
        buffer.frameLength = AVAudioFrameCount(callbackFrames)
        let total = seconds * 48_000
        var now = 0
        var nextFeed = 0
        var nextPacket = 0
        var heard = false
        var mostHeld = 0
        while now < total {
            // Packets that have arrived by now: packet k is sent at k * 40 ms
            // and arrives 0 to 30 ms after, by a fixed pattern.
            let stalled = stallAfterFrames.map { now >= $0 } ?? false
            while !stalled {
                let lateFrames = (nextPacket * 7 % 31) * 48
                let arrival = nextPacket * AudioJitterBuffer.packetFrames + lateFrames
                guard arrival <= now else {
                    break
                }
                core.receive(AudioJitterBufferTests.packet(UInt16(truncatingIfNeeded: nextPacket), ssrc: ssrc,
                                                           payload: payload))
                nextPacket += 1
            }
            while nextFeed <= now {
                if !stalled {
                    core.pumpNow()
                    if now >= 48_000 {
                        mostHeld = max(mostHeld, core.heldRingFrames)
                    }
                }
                nextFeed += feedFrames
            }
            core.render(frames: callbackFrames, into: buffer.mutableAudioBufferList)
            if now >= 48_000, let left = buffer.floatChannelData?[0] {
                heard = heard || (0..<callbackFrames).contains { left[$0] != 0 }
            }
            now += callbackFrames
            // The simulated callback is complete; let other test tasks run.
            await Task.yield()
        }
        return (heard, mostHeld)
    }

    /// 5 ms and 10 ms pieces (the microphone's I/O buffer), and 1024
    /// frames, about iOS's default.
    @Test(arguments: [240, 480, 1_024])
    func smallOutputPiecesNeverFindTheRingDry(callbackFrames: Int) async throws {
        let core = try AudioPlaybackCore()
        let played = try await Self.play(core, callbackFrames: callbackFrames, seconds: 20)
        #expect(played.heard, "the band came through")
        #expect(core.renderUnderruns == 0)
        #expect(core.callbackFrames == callbackFrames)
        // The ring holds one piece and the feed's cover, and a block more at most.
        let lowWater = callbackFrames + AudioPlaybackCore.feedCoverFrames
        #expect(core.currentLowWaterFrames == lowWater)
        #expect(played.mostHeldFrames <= lowWater + AudioJitterBuffer.packetFrames)
        let counters = core.counters
        #expect(counters.underruns == 0)
        #expect(counters.concealed == 0)
        let ratio = core.resamplerRatio
        #expect(ratio >= 0.999 && ratio <= 1.001)
    }

    @Test func aFeedThatStallsPastTheRingCountsDryRenders() async throws {
        let core = try AudioPlaybackCore()
        // Playing starts after 180 ms of packets; the feed queue hears
        // nothing from 2 s on, so the ring's 30 to 70 ms run out and every
        // later render is dry.
        try await Self.play(core, callbackFrames: 480, seconds: 3, stallAfterFrames: 96_000)
        #expect(core.renderUnderruns > 0)
        // The next feed check answers them: the mark rises by 5 ms.
        #expect(core.currentLowWaterFrames == 480 + AudioPlaybackCore.feedCoverFrames)
        core.pumpNow()
        #expect(core.currentLowWaterFrames
            == 480 + AudioPlaybackCore.feedCoverFrames + AudioPlaybackCore.coverStepFrames)
    }

    @Test func dryRendersNeverRaiseTheMarkPastEightyMilliseconds() throws {
        let core = try AudioPlaybackCore()
        let payload = try AudioJitterBufferTests.opusPayload()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 480))
        buffer.frameLength = 480
        var sequence: UInt16 = 0
        // Each round: 240 ms of packets arrive (each arrival a feed check,
        // which also answers the last round's dry renders), then the output
        // renders 400 ms with no feed check at all, running the ring dry
        // while the buffer still plays.
        for round in 0..<40 {
            for _ in 0..<6 {
                core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
                sequence &+= 1
            }
            core.flush()
            let before = core.renderUnderruns
            for _ in 0..<40 {
                core.render(frames: 480, into: buffer.mutableAudioBufferList)
            }
            // The first round's packets only fill the buffer: it plays from the second.
            if round > 0 {
                #expect(core.renderUnderruns > before)
            }
        }
        core.pumpNow()
        #expect(core.currentLowWaterFrames
            == 480 + AudioPlaybackCore.feedCoverFrames + AudioPlaybackCore.maximumExtraCoverFrames)
    }

    @Test func theSteeringTargetCountsTheRingsRealShare() {
        // 10 ms pieces: a 30 ms mark and half a 40 ms block, 50 ms.
        #expect(AudioPlaybackCore.nominalRingMs(lowWaterFrames: 480 + AudioPlaybackCore.feedCoverFrames) == 50)
        // 5 ms pieces: 45 ms.
        #expect(AudioPlaybackCore.nominalRingMs(lowWaterFrames: 240 + AudioPlaybackCore.feedCoverFrames) == 45)
    }

    @Test func withoutAStreamNothingCountsAsDry() throws {
        let core = try AudioPlaybackCore()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 240))
        buffer.frameLength = 240
        for _ in 0..<100 {
            core.pumpNow()
            core.render(frames: 240, into: buffer.mutableAudioBufferList)
        }
        #expect(core.renderUnderruns == 0)
        #expect(core.callbackFrames == 240)
    }

    @Test func theLogLineCarriesEveryCount() {
        let line = AudioPlaybackCore.summaryLine(
            "each minute", counters: AudioPlaybackCore.Counters(underruns: 2, lateDrops: 3, concealed: 4),
            renderUnderruns: 5, callbackFrames: 480, lowWaterFrames: 1_440)
        #expect(line == "band playback (each minute): renderUnderruns=5 bufferUnderruns=2 concealed=4 lateDrops=3 "
            + "callbackFrames=480 callbackMs=10.0 lowWaterMs=30.0")
    }

    @Test func observationSeparatesLiveQueuesFromPolicyAndRetiresAtOutputStop() async throws {
        let core = try AudioPlaybackCore()
        let payload = try AudioJitterBufferTests.opusPayload()
        let anchor = AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0)
        let outputEpoch = core.outputDidStart()
        core.reanchor(anchor)
        for sequence in UInt16(0)..<5 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
        }
        core.flush()
        let before = try #require(await core.observation())
        #expect(before.stream == anchor && before.outputEpoch == outputEpoch)
        #expect(before.counters.acceptedPackets == 0)
        #expect(before.counters.receivedContentBytes == UInt64(5 * payload.count))
        #expect(before.reorderQueuedMs == 200)
        #expect(before.adaptiveTargetMs == 180)
        #expect(before.lastAdmittedPacketAgeMs == nil, "startup packets are pending admission")
        #expect(before.speakerQueuedSourceFrames == nil, "no render observation yet")

        let output = UnsafeMutablePointer<Float>.allocate(capacity: 2_500 * 2)
        defer { output.deallocate() }
        core.render(into: output, frames: 2_500)
        core.pumpNow()
        core.render(into: output, frames: 480)
        let after = try #require(await core.observation())
        #expect(after.requestedSourceFrames == 2_980)
        #expect(after.renderedSourceFrames > 0)
        #expect(after.speakerQueuedSourceFrames != nil)
        #expect(after.measuredDriftPpm != nil)
        #expect(after.counters.acceptedPackets == 5)
        #expect(after.lastAdmittedPacketAgeMs != nil)

        core.outputDidRetire()
        let retired = try #require(await core.observation())
        #expect(retired.outputEpoch == nil)
        #expect(retired.speakerQueuedSourceFrames == nil)
        #expect(retired.measuredDriftPpm == nil)
        #expect(retired.reorderQueuedMs == nil)
        #expect(retired.lastAdmittedPacketAgeMs == nil)
        #expect(retired.counters.acceptedPackets == 5)
    }

    @Test func overflowingRenderPublicationKeepsCumulativeFramesAndRecovers() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 64 * 2)
        defer { output.deallocate() }
        for _ in 0..<100 { core.render(into: output, frames: 64) }
        let dropped = try #require(await core.observation())
        #expect(dropped.requestedSourceFrames == 6_400)
        #expect(dropped.droppedRenderObservations > 0)
        #expect(!dropped.renderEvidenceRecoveredAfterDrop)
        core.render(into: output, frames: 64)
        let recovered = try #require(await core.observation())
        #expect(recovered.requestedSourceFrames == 6_464)
        #expect(recovered.renderEvidenceRecoveredAfterDrop)
    }

    @Test func equivalentRouteKeepsTimelineButNewStreamCannotClaimOldAudio() async throws {
        let core = try AudioPlaybackCore()
        let payload = try AudioJitterBufferTests.opusPayload()
        core.outputDidStart()
        let first = AudioStreamAnchor(generation: 7, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0)
        core.reanchor(first)
        for sequence in UInt16(0)..<8 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
        }
        core.flush()
        let initial = try #require(await core.observation())
        let replacementSsrc: UInt32 = 0xAABB_CCDD
        core.acceptEquivalentSsrc(replacementSsrc)
        core.adoptEquivalentAnchor(AudioStreamAnchor(generation: 8, ssrc: replacementSsrc,
                                                     firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let equivalent = try #require(await core.observation())
        #expect(equivalent.streamEpoch == initial.streamEpoch)
        #expect(equivalent.counters.receivedContentBytes == initial.counters.receivedContentBytes)

        // The ring still contains old samples when the semantic stream changes.
        core.reanchor(AudioStreamAnchor(generation: 9, ssrc: 0x1234_5678,
                                        firstSequence: 100, firstTimestamp: 200_000))
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 480 * 2)
        defer { output.deallocate() }
        core.render(into: output, frames: 480)
        let changed = try #require(await core.observation())
        #expect(changed.streamEpoch != equivalent.streamEpoch)
        #expect(changed.counters.acceptedPackets == 0)
        #expect(changed.counters.receivedContentBytes == 0)
        #expect(changed.sourcePlayout == nil)
    }

    @Test func outputRetirementDoesNotResetKnownStreamDryRenders() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        let payload = try AudioJitterBufferTests.opusPayload()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<5 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
        }
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 8_192 * 2)
        defer { output.deallocate() }
        core.render(into: output, frames: 2_500)
        core.pumpNow()
        core.render(into: output, frames: 8_192)
        let playing = try #require(await core.observation())
        let known = try #require(playing.streamRenderUnderruns)
        #expect(known > 0)
        core.outputDidRetire()
        let paused = try #require(await core.observation())
        #expect(paused.streamRenderUnderruns == known)
        core.outputDidStart()
        let resumed = try #require(await core.observation())
        #expect(resumed.streamRenderUnderruns == known)
    }

    @Test func repeatedNilReanchorPreservesFinalStreamRecord() async throws {
        let core = try AudioPlaybackCore()
        let anchor = AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0)
        let payload = try AudioJitterBufferTests.opusPayload()
        core.outputDidStart()
        core.reanchor(anchor)
        for sequence in UInt16(0)..<5 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
        }
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 2_500 * 2)
        defer { output.deallocate() }
        core.render(into: output, frames: 2_500)
        core.pumpNow()
        core.reanchor(nil)
        let first = try #require(await core.observation())
        #expect(first.counters.acceptedPackets == 5)
        core.reanchor(nil)
        let second = try #require(await core.observation())
        #expect(second.streamEpoch == first.streamEpoch)
        #expect(second.counters == first.counters)
        #expect(second.streamRenderUnderruns == first.streamRenderUnderruns)
        #expect(second.retiredStream == anchor)
        #expect(second.outputEpoch == nil || second.sourcePlayout == nil)
    }

    @Test(arguments: [4_096, 2_500])
    func sourceNodeObservationUsesWholeCallbackQuantum(frames: Int) async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(frames)))
        buffer.frameLength = AVAudioFrameCount(frames)
        core.render(frames: frames, into: buffer.mutableAudioBufferList)
        let reading = try #require(await core.observation())
        #expect(reading.lastCallbackSourceFrames == frames)
    }

    @Test func ringMetadataRemainsBoundedWithoutCurrentRenderEvidence() throws {
        let core = try AudioPlaybackCore()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 1_920 * 2)
        defer { output.deallocate() }
        for _ in 0..<50 {
            core.pumpNow()
            core.render(into: output, frames: 1_920)
        }
        #expect(core.ringSegmentCount <= 16)
    }

    @Test func callbackAlreadyInFlightMakesRetiredStreamTotalUnavailable() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        let anchor = AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0)
        core.reanchor(anchor)
        core.flush()
        let held = core.beginRenderBoundary()
        core.reanchor(nil)
        let during = try #require(await core.observation())
        #expect(during.retiredStream == anchor)
        #expect(during.streamRenderUnderruns == nil)
        #expect(!core.endRenderBoundary(held))
        let after = try #require(await core.observation())
        #expect(after.streamRenderUnderruns == nil)
        #expect(after.retiredStream == anchor)
    }

    @Test func callbackStartingAfterRetirementCannotClaimOldStream() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        let anchor = AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0)
        core.reanchor(anchor)
        core.flush()
        core.reanchor(nil)
        let retired = try #require(await core.observation())
        #expect(retired.streamRenderUnderruns == 0)
        let afterRetirement = core.beginRenderBoundary()
        #expect(afterRetirement.stream == retired.streamEpoch)
        #expect(core.endRenderBoundary(afterRetirement))
        let later = try #require(await core.observation())
        #expect(later.streamRenderUnderruns == 0)
        #expect(later.retiredStream == anchor)
    }

    @Test func outputRetirementHeldAcrossCallbackGapsCurrentStreamTotal() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let held = core.beginRenderBoundary()
        core.outputDidRetire()
        let paused = try #require(await core.observation())
        #expect(paused.streamRenderUnderruns == nil)
        #expect(!core.endRenderBoundary(held))
        let completed = try #require(await core.observation())
        #expect(completed.streamRenderUnderruns == nil)
    }

    @Test func queuedOldRecordCannotResolveOverlappingOutputRetirement() async throws {
        let core = try AudioPlaybackCore()
        let payload = try AudioJitterBufferTests.opusPayload()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<5 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: Self.ssrc, payload: payload))
        }
        core.flush()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 8_192 * 2)
        defer { output.deallocate() }
        core.render(into: output, frames: 2_500)
        core.pumpNow()
        // A queues a real record, and no feed read consumes it yet.
        core.render(into: output, frames: 8_192)
        #expect(core.renderUnderruns > 0)
        // B has entered the same fence used by render, then retirement
        // crosses it. A's queued record predates that unresolved callback.
        let heldB = core.beginRenderBoundary()
        core.outputDidRetire()
        let beforeBCompletes = try #require(await core.observation())
        #expect(beforeBCompletes.streamRenderUnderruns == nil)
        #expect(!core.endRenderBoundary(heldB))
        let afterBCompletes = try #require(await core.observation())
        #expect(afterBCompletes.streamRenderUnderruns == nil)

        core.outputDidStart()
        core.render(into: output, frames: 480)
        let resumed = try #require(await core.observation())
        #expect(resumed.streamRenderUnderruns == UInt64(core.renderUnderruns),
                "a current output record acknowledges the complete stream total")
    }

    @Test func concurrentRenderPublicationAndSnapshotsKeepLifetimeTotalsMonotonic() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let iterations = 4_096
        let frames = 64
        let producer = Task.detached {
            let output = UnsafeMutablePointer<Float>.allocate(capacity: frames * 2)
            defer { output.deallocate() }
            for index in 0..<iterations {
                core.render(into: output, frames: frames)
                if index.isMultiple(of: 16) { await Task.yield() }
            }
        }
        var previousRequested: UInt64 = 0
        var previousRendered: UInt64 = 0
        var previousDry: UInt64 = 0
        for _ in 0..<256 {
            if let reading = await core.observation() {
                #expect(reading.requestedSourceFrames >= previousRequested)
                #expect(reading.renderedSourceFrames >= previousRendered)
                #expect(reading.renderUnderruns >= previousDry)
                previousRequested = reading.requestedSourceFrames
                previousRendered = reading.renderedSourceFrames
                previousDry = reading.renderUnderruns
            }
        }
        await producer.value
        let final = try #require(await core.observation())
        #expect(final.requestedSourceFrames == UInt64(iterations * frames))
        #expect(final.renderedSourceFrames <= final.requestedSourceFrames)
        #expect(final.renderUnderruns >= previousDry)
    }

    @Test(arguments: [2_048, 2_500])
    func sourceCallbackRecordsRetainFullQuantumAndEveryChunkOffset(frames: Int) throws {
        let core = try AudioPlaybackCore()
        let outputEpoch = core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(frames)))
        buffer.frameLength = AVAudioFrameCount(frames)
        var timestamp = AudioTimeStamp()
        timestamp.mFlags = [.hostTimeValid, .sampleTimeValid]
        timestamp.mHostTime = 12_345
        timestamp.mSampleTime = 900.5
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: frames, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        var records: [nereus_render_observation] = []
        var record = nereus_render_observation()
        while nereus_render_observation_queue_pop(core.renderObservations, &record) != 0 {
            records.append(record)
        }
        #expect(records.count == (frames == 2_500 ? 3 : 2))
        #expect(records.map(\.callback_frames) == Array(repeating: UInt32(frames), count: records.count))
        #expect(records.map(\.chunk_start_frame) == (frames == 2_500 ? [0, 1_024, 2_048] : [0, 1_024]))
        #expect(records.map(\.chunk_end_frame) == (frames == 2_500 ? [1_024, 2_048, 2_500] : [1_024, 2_048]))
        #expect(records.allSatisfy { $0.output_epoch == outputEpoch && $0.source_rate_hz == 48_000 })
        #expect(records.allSatisfy { $0.source_host_ticks == 12_345 && $0.source_sample_time == 900.5 })
    }

    @Test func sourceScheduleKeepsFlagValidityIndependentAndNilMetadataUnavailable() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 240))
        buffer.frameLength = 240
        core.render(frames: 240, into: buffer.mutableAudioBufferList)
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)

        var sampleOnly = AudioTimeStamp()
        sampleOnly.mFlags = .sampleTimeValid
        sampleOnly.mSampleTime = 42.25
        sampleOnly.mHostTime = 777
        withUnsafePointer(to: &sampleOnly) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        let sampleReading = try #require(await core.observation())
        let sample = try #require(sampleReading.sourceRenderSchedule)
        #expect(sample.sourceSampleTime == 42.25)
        #expect(sample.sourceHostTicks == nil && sample.sourceHostTimeNs == nil)
        #expect(sample.callbackSourceFrames == 240 && sample.chunkStartSourceFrame == 0
                && sample.chunkEndSourceFrame == 240)
        #expect(sample.sourceRateHz == 48_000)
        #expect(sample.readWindowNs >= 0 && sample.measuredNs > 0)

        var hostOnly = AudioTimeStamp()
        hostOnly.mFlags = .hostTimeValid
        hostOnly.mHostTime = 1_234
        hostOnly.mSampleTime = 99.5
        withUnsafePointer(to: &hostOnly) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        let hostReading = try #require(await core.observation())
        let host = try #require(hostReading.sourceRenderSchedule)
        #expect(host.sourceHostTicks == 1_234)
        #expect(host.sourceHostTimeNs != nil)
        #expect(host.sourceSampleTime == nil)

        hostOnly.mFlags = .sampleTimeValid
        hostOnly.mSampleTime = .infinity
        withUnsafePointer(to: &hostOnly) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
        hostOnly.mFlags = []
        hostOnly.mSampleTime = 12
        withUnsafePointer(to: &hostOnly) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
    }

    @Test func checkedTimebaseConversionHandlesRatiosAndOverflow() {
        #expect(AudioPlaybackCore.hostTicksToNanoseconds(9, numer: 125, denom: 3) == 375)
        #expect(AudioPlaybackCore.hostTicksToNanoseconds(UInt64.max, numer: 2, denom: 2) == UInt64.max)
        #expect(AudioPlaybackCore.hostTicksToNanoseconds(UInt64.max, numer: 2, denom: 1) == nil)
        #expect(AudioPlaybackCore.hostTicksToNanoseconds(9, numer: 1, denom: 0) == nil)
    }

    @Test func scheduleRetiresWithOutputAndDoesNotRecoverAcrossPublicationDrop() async throws {
        let core = try AudioPlaybackCore()
        core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 240))
        buffer.frameLength = 240
        var timestamp = AudioTimeStamp()
        timestamp.mFlags = .hostTimeValid
        timestamp.mHostTime = 123
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule?.sourceHostTicks == 123)
        for _ in 0..<100 {
            withUnsafePointer(to: &timestamp) { pointer in
                core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
            }
        }
        let dropped = try #require(await core.observation())
        #expect(dropped.droppedRenderObservations > 0)
        #expect(dropped.sourceRenderSchedule == nil)
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule?.sourceHostTicks == 123)
        core.outputDidRetire()
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
        core.outputDidStart()
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule?.sourceHostTicks == 123)
        core.reanchor(AudioStreamAnchor(generation: 2, ssrc: Self.ssrc &+ 1,
                                        firstSequence: 0, firstTimestamp: 0))
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
    }

    @Test func mismatchedRenderRecordInvalidatesPreviousCurrentSchedule() async throws {
        let core = try AudioPlaybackCore()
        let epoch = core.outputDidStart()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        core.flush()
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 240))
        buffer.frameLength = 240
        var timestamp = AudioTimeStamp()
        timestamp.mFlags = .hostTimeValid
        timestamp.mHostTime = 91
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule?.sourceHostTicks == 91)
        var oldOutput = nereus_render_observation()
        oldOutput.stream_epoch = try #require(await core.observation()).streamEpoch
        oldOutput.output_epoch = epoch - 1
        oldOutput.has_timestamp = 1
        oldOutput.has_source_host_time = 1
        oldOutput.source_host_ticks = 92
        #expect(nereus_render_observation_queue_push(core.renderObservations, &oldOutput) != 0)
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
        withUnsafePointer(to: &timestamp) { pointer in
            core.render(frames: 240, into: buffer.mutableAudioBufferList, timestamp: pointer)
        }
        #expect(try #require(await core.observation()).sourceRenderSchedule?.sourceHostTicks == 91)
        var oldStream = oldOutput
        oldStream.stream_epoch = 0
        oldStream.output_epoch = epoch
        #expect(nereus_render_observation_queue_push(core.renderObservations, &oldStream) != 0)
        #expect(try #require(await core.observation()).sourceRenderSchedule == nil)
    }

    @Test func currentStreamEpochFollowsReanchorsAndRetirement() async throws {
        let core = try AudioPlaybackCore()
        #expect(core.currentStreamEpoch == 0)
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        let first = try #require(await core.observation())
        #expect(core.currentStreamEpoch == first.streamEpoch)
        core.reanchor(AudioStreamAnchor(generation: 2, ssrc: Self.ssrc &+ 1,
                                        firstSequence: 0, firstTimestamp: 0))
        let second = try #require(await core.observation())
        #expect(second.streamEpoch == first.streamEpoch + 1)
        #expect(core.currentStreamEpoch == second.streamEpoch)
        core.reanchor(nil)
        let retired = try #require(await core.observation())
        #expect(retired.streamEpoch == second.streamEpoch + 1)
        #expect(core.currentStreamEpoch == retired.streamEpoch)
    }
}
