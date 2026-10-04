// NereusSDR for iOS: the phone's own check that the network carries lossless audio, and the fall back to Opus
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-09: the desktop's lossless link trial (`RemoteAudioLinkTrial`,
/// R-R3-23): windows of 5 s; the first fails above 2 % loss or on any
/// interruption, later ones only on three bad windows in a row above 1 %
/// or a second interruption within 60 s. Loss is the larger of missing
/// over expected and filled over played.
@Suite struct LosslessLinkTrialTests {
    typealias Sample = LosslessLinkTrial.Sample

    /// A running receiver's counters after `packets` expected, with
    /// `missing` of them missing and `filled` filled in.
    static func sample(generation: UInt64 = 1, packets: UInt64, missing: UInt64 = 0, filled: UInt64 = 0,
                       interruptions: UInt64 = 0) -> Sample {
        Sample(running: true, generation: generation, expectedPackets: packets, missingPackets: missing,
               decodedPackets: packets - filled, concealedPackets: filled, linkInterruptions: interruptions)
    }

    @Test func theDesktopsNumbers() {
        #expect(LosslessLinkTrial.windowMs == 5_000)
        #expect(LosslessLinkTrial.trialLossLimit == 0.02)
        #expect(LosslessLinkTrial.sustainedLossLimit == 0.01)
        #expect(LosslessLinkTrial.sustainedWindows == 3)
        #expect(LosslessLinkTrial.trialRestartLimit == 1)
        #expect(LosslessLinkTrial.sustainedRestartLimit == 2)
        #expect(LosslessLinkTrial.restartSpanMs == 60_000)
        #expect(LosslessLinkTrial.sampleMs == 1_000)
    }

    @Test func aCleanFirstWindowPassesAndAFouledOneFails() {
        var clean = LosslessLinkTrial()
        clean.begin(nowMs: 0)
        #expect(clean.observe(nowMs: 1_000, Self.sample(packets: 250)) == .continue)
        // 2 % exactly is not above the limit.
        #expect(clean.observe(nowMs: 5_000, Self.sample(packets: 1_250, missing: 25)) == .continue)
        #expect(clean.lastWindowLoss == 0.02)

        var fouled = LosslessLinkTrial()
        fouled.begin(nowMs: 0)
        #expect(fouled.observe(nowMs: 4_999, Self.sample(packets: 1_250, filled: 26)) == .continue,
                "a window decides only when it closes")
        #expect(fouled.observe(nowMs: 5_000, Self.sample(packets: 1_250, filled: 26)) == .failed)
        #expect(!fouled.failedOnInterruptions)
    }

    @Test func laterOnlyThreeBadWindowsInARowFail() {
        var trial = LosslessLinkTrial()
        trial.begin(nowMs: 0)
        var packets: UInt64 = 1_250
        var missing: UInt64 = 0
        #expect(trial.observe(nowMs: 5_000, Self.sample(packets: packets)) == .continue)
        func window(_ endMs: Int64, lost: UInt64) -> LosslessLinkTrial.Verdict {
            packets += 1_250
            missing += lost
            return trial.observe(nowMs: endMs, Self.sample(packets: packets, missing: missing))
        }
        // 1.5 % twice, then a good one: the run starts over.
        #expect(window(10_000, lost: 19) == .continue)
        #expect(window(15_000, lost: 19) == .continue)
        #expect(window(20_000, lost: 0) == .continue)
        #expect(window(25_000, lost: 19) == .continue)
        #expect(window(30_000, lost: 19) == .continue)
        #expect(window(35_000, lost: 19) == .failed)
    }

    @Test func anyInterruptionFailsTheFirstWindowAndASecondWithinAMinuteLater() {
        var first = LosslessLinkTrial()
        first.begin(nowMs: 0)
        #expect(first.observe(nowMs: 1_000, Self.sample(packets: 250)) == .continue)
        #expect(first.observe(nowMs: 2_000, Self.sample(packets: 500, interruptions: 1)) == .failed)
        #expect(first.failedOnInterruptions)

        var later = LosslessLinkTrial()
        later.begin(nowMs: 0)
        #expect(later.observe(nowMs: 5_000, Self.sample(packets: 1_250)) == .continue)
        #expect(later.observe(nowMs: 6_000, Self.sample(packets: 1_500, interruptions: 1)) == .continue)
        #expect(later.observe(nowMs: 67_000, Self.sample(packets: 1_750, interruptions: 2)) == .continue,
                "the first is more than a minute old")
        #expect(later.observe(nowMs: 68_000, Self.sample(packets: 2_000, interruptions: 3)) == .failed)
    }

    @Test func aNewReceiverCountsFromZeroAndAStoppedOneIsIgnored() {
        var trial = LosslessLinkTrial()
        trial.begin(nowMs: 0)
        #expect(trial.observe(nowMs: 1_000, Self.sample(packets: 900, missing: 400)) == .continue)
        // Its counters reset with the new generation; the old ones are not
        // taken away from it.
        #expect(trial.observe(nowMs: 2_000, Self.sample(generation: 2, packets: 250)) == .continue)
        #expect(trial.observe(nowMs: 3_000, Sample(running: false, generation: 2, expectedPackets: 0,
                                                    missingPackets: 0, decodedPackets: 0, concealedPackets: 0,
                                                    linkInterruptions: 5)) == .continue)
        #expect(trial.observe(nowMs: 5_000, Self.sample(generation: 2, packets: 1_250)) == .failed,
                "the first receiver's 400 of 900 missing count in the window")
        #expect(trial.lastWindowLoss.map { $0 > 0.15 } == true)
    }

    @Test func aWindowWithNothingPlayedHasNoVerdict() {
        var trial = LosslessLinkTrial()
        trial.begin(nowMs: 0)
        #expect(trial.observe(nowMs: 5_000, nil) == .continue)
        #expect(trial.lastWindowLoss == nil)
        var ended = LosslessLinkTrial()
        #expect(ended.observe(nowMs: 5_000, Self.sample(packets: 1_250, missing: 1_000)) == .continue,
                "a trial not begun never fails")
    }

    // MARK: In the media client

    typealias Rig = MediaControlClientTests.Rig

    /// L16 packet `sequence` on the connection's audio stream.
    static func l16(_ sequence: UInt16, ssrc: UInt32) -> RtpPacket {
        RtpPacket(payloadType: 96, sequence: sequence, timestamp: UInt32(sequence) &* 192, ssrc: ssrc,
                  payload: Data(repeating: 0, count: L16Audio.payloadBytes))
    }

    /// One second of lossless playback through the buffer, as it arrives:
    /// 25 times, ten packets (but for every `dropEvery`th) then one pull.
    static func playOneSecond(_ rig: Rig, from first: UInt16, ssrc: UInt32, dropEvery: Int?) {
        rig.playback.withJitterBuffer { buffer in
            for pull in 0..<25 {
                for slot in 0..<10 {
                    let index = pull * 10 + slot
                    if let dropEvery, index % dropEvery == dropEvery - 1 { continue }
                    let sequence = first &+ UInt16(index)
                    buffer.push(Self.l16(sequence, ssrc: ssrc),
                                arrivalNs: Int64(Int(first) + index) * 4_000_000)
                }
                _ = buffer.pull(releasedNs: Int64(Int(first) / 10 + pull) * 40_000_000)
            }
        }
    }

    static func losslessUp(_ rig: Rig) async throws -> (id: String, ssrc: UInt32) {
        await rig.open(minor: 11, capabilities: AudioQualityMediaTests.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(AudioQualityMediaTests.context(id, revision: try AudioQualityMediaTests.lastRevision(rig),
                                                         generation: 1, profile: "lossless", firstSequence: 0))
        #expect(rig.playback.anchor?.format == .l16)
        return (id, MediaControlClient.audioSsrc(forConnection: id))
    }

    static func fellBack(_ rig: Rig) -> Bool {
        rig.recorder.events.contains(.losslessFallback)
    }

    @Test func aLinkThatLosesLosslessPacketsFallsBackToOpus() async throws {
        let rig = try Rig()
        let (_, ssrc) = try await Self.losslessUp(rig)
        let sentBefore = rig.recorder.sent("audio").count
        // 10 % lost each second; the first window closes after 5 s.
        for second in 0..<6 {
            Self.playOneSecond(rig, from: UInt16(second * 250), ssrc: ssrc, dropEvery: 10)
            rig.recorder.advanceClock(by: 1_000)
            await rig.timers.advance(by: 1_000)
            await rig.recorder.settle { Self.fellBack(rig) }
            if Self.fellBack(rig) { break }
        }
        #expect(Self.fellBack(rig))
        let asked = rig.recorder.sent("audio")
        #expect(asked.count == sentBefore + 1)
        #expect(asked.last?["profile"] == "opus", "the fall back asks the Core for Opus")
        #expect(asked.last?["opusBitrate"] == 48000)
        #expect(await rig.client.losslessFallback)
        // The same request again keeps the fall back; choosing it again
        // gives lossless a fresh chance.
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == sentBefore + 1)
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000),
                                         choosing: true)
        #expect(rig.recorder.sent("audio").last?["profile"] == "lossless")
        #expect(!(await rig.client.losslessFallback))
    }

    @Test func aCleanLinkKeepsLossless() async throws {
        let rig = try Rig()
        let (_, ssrc) = try await Self.losslessUp(rig)
        let sentBefore = rig.recorder.sent("audio").count
        for second in 0..<11 {
            Self.playOneSecond(rig, from: UInt16(second * 250), ssrc: ssrc, dropEvery: nil)
            rig.recorder.advanceClock(by: 1_000)
            await rig.timers.advance(by: 1_000)
        }
        await rig.recorder.settle { Self.fellBack(rig) }
        #expect(!Self.fellBack(rig))
        #expect(rig.recorder.sent("audio").count == sentBefore)
        // Eleven seconds closed two windows, both clean, and the trial runs on.
        #expect(await rig.client.losslessTrialActive)
        #expect(await rig.client.losslessTrialClosedWindows == 2)
        #expect(await rig.client.losslessTrialLastWindowLoss == 0)
    }

    /// A stall while lossless plays is an interruption of the stream; in
    /// the first window any interruption fails the trial (the desktop's
    /// trialRestartLimit of 1), so the phone falls back to Opus.
    @Test func aLosslessStallFallsBack() async throws {
        let rig = try Rig()
        let (_, ssrc) = try await Self.losslessUp(rig)
        let sentBefore = rig.recorder.sent("audio").count
        Self.playOneSecond(rig, from: 0, ssrc: ssrc, dropEvery: nil)
        rig.recorder.advanceClock(by: 1_000)
        await rig.timers.advance(by: 1_000)
        #expect(!Self.fellBack(rig))
        // Nothing arrives: the next pulls find the queue empty.
        let underruns = rig.playback.withJitterBuffer { buffer in
            for pull in 0..<25 { _ = buffer.pull(releasedNs: Int64(25 + pull) * 40_000_000) }
            return buffer.underruns
        }
        #expect(underruns > 0)
        rig.recorder.advanceClock(by: 1_000)
        await rig.timers.advance(by: 1_000)
        await rig.recorder.settle { Self.fellBack(rig) }
        #expect(Self.fellBack(rig))
        #expect(rig.recorder.sent("audio").count == sentBefore + 1)
        #expect(rig.recorder.sent("audio").last?["profile"] == "opus")
        #expect(await rig.client.losslessFallback)
        #expect(await !rig.client.losslessTrialActive)
    }

    @Test func anOpusContextEndsTheTrial() async throws {
        let rig = try Rig()
        let (id, ssrc) = try await Self.losslessUp(rig)
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.deliver(AudioQualityMediaTests.context(id, revision: try AudioQualityMediaTests.lastRevision(rig),
                                                         generation: 2))
        #expect(await !rig.client.losslessTrialActive)
        _ = ssrc
    }
}
