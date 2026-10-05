// NereusSDR for iOS: the jitter buffer against written arrival patterns, its bounds, concealment and re-anchoring
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusMedia

/// Spec section 4.1 (playback on the phone). Each pattern below is a
/// written schedule of arrival times for 1500 packets (60 s), played by a
/// pull every 40 ms from t = 0, arrivals at a pull's time first. The
/// expected counts are worked out from the buffer's rules (its type
/// comment), with the working beside each pattern.
@Suite struct AudioJitterBufferTests {
    static let ssrc: UInt32 = 0x4E53_0001
    static let packets = 1500

    /// A real 40 ms Opus packet's payload, from the link's media vectors.
    static func opusPayload() throws -> Data {
        let vectors = try LinkFixtureLoader.mediaVectors()
        let vector = try #require(vectors["media-opus-1"])
        return try #require(RtpPacket(parsing: vector.bytes)).payload
    }

    static func packet(_ sequence: UInt16, ssrc: UInt32 = ssrc, payload: Data) -> RtpPacket {
        RtpPacket(payloadType: 111, sequence: sequence, timestamp: UInt32(sequence) &* 1920, ssrc: ssrc,
                  payload: payload)
    }

    struct Arrival {
        let atMs: Int
        let sequence: UInt16
    }

    struct Outcome {
        var played = 0
        var concealedBlocks = 0
        var silentPulls = 0
        var underruns = 0
        var lateDrops = 0
        var concealed = 0
        var finalTargetMs = 0
        var boundsHeld = true
    }

    /// Plays `arrivals` until `expectedBlocks` packets have been played or
    /// concealed, checking the bounds after every pull.
    static func run(_ arrivals: [Arrival], expectedBlocks: Int) throws -> Outcome {
        let payload = try opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: ssrc, firstSequence: 0, firstTimestamp: 0))
        let ordered = arrivals.enumerated().sorted { ($0.element.atMs, $0.offset) < ($1.element.atMs, $1.offset) }
            .map(\.element)
        var next = 0
        var outcome = Outcome()
        let lastArrival = ordered.last?.atMs ?? 0
        var time = 0
        while outcome.played + outcome.concealedBlocks < expectedBlocks && time <= lastArrival + 2000 {
            while next < ordered.count && ordered[next].atMs <= time {
                buffer.push(packet(ordered[next].sequence, payload: payload))
                next += 1
            }
            switch buffer.pull() {
            case .audio(let pcm):
                outcome.played += 1
                outcome.boundsHeld = outcome.boundsHeld && pcm.count == 1920 * 2
            case .concealed(let pcm):
                outcome.concealedBlocks += 1
                outcome.boundsHeld = outcome.boundsHeld && pcm.count == 1920 * 2
            case .silence:
                outcome.silentPulls += 1
            }
            outcome.boundsHeld = outcome.boundsHeld
                && (AudioJitterBuffer.minimumTargetMs...AudioJitterBuffer.maximumTargetMs).contains(buffer.targetMs)
                && buffer.depthMs <= AudioJitterBuffer.maximumTargetMs
            time += 40
        }
        outcome.underruns = buffer.underruns
        outcome.lateDrops = buffer.lateDrops
        outcome.concealed = buffer.concealed
        outcome.finalTargetMs = buffer.targetMs
        return outcome
    }

    /// SplitMix64, so the loss pattern is the same on every run.
    struct SplitMix64 {
        var state: UInt64

        mutating func next() -> UInt64 {
            state &+= 0x9E37_79B9_7F4A_7C15
            var z = state
            z = (z ^ (z >> 30)) &* 0xBF58_476D_1CE4_E5B9
            z = (z ^ (z >> 27)) &* 0x94D0_49BB_1331_11EB
            return z ^ (z >> 31)
        }
    }

    // MARK: Patterns

    /// Steady: packet n arrives at 40n + 20 ms.
    ///
    /// Packet 4 arrives at 180 ms, making the depth 5 packets (200 ms), the
    /// first depth at or above the 180 ms target, so playing begins at the
    /// 200 ms pull. The pull at 200 + 40p plays packet p, which arrived
    /// 180 ms earlier, so no pull finds the buffer empty: no underrun, no
    /// concealment, nothing late. The 1500 playing pulls are six spells of
    /// 250 without an underrun, so the target falls 180, 160, 140, 120,
    /// 100, 80 and then stays at its floor of 80.
    @Test func aSteadyStreamPlaysWithoutAnUnderrun() throws {
        let arrivals = (0..<Self.packets).map { Arrival(atMs: 40 * $0 + 20, sequence: UInt16($0)) }
        let outcome = try Self.run(arrivals, expectedBlocks: Self.packets)
        #expect(outcome.played == Self.packets)
        #expect(outcome.underruns == 0)
        #expect(outcome.concealed == 0)
        #expect(outcome.lateDrops == 0)
        #expect(outcome.silentPulls == 5)
        #expect(outcome.finalTargetMs == 80)
        #expect(outcome.boundsHeld)
    }

    /// Bursty: packets 5k to 5k + 4 arrive together at 200k + 190 ms, and
    /// every fourth burst (k mod 4 = 3) 100 ms later still.
    ///
    /// Burst 0 at 190 ms is 200 ms deep: playing begins at the 200 ms pull,
    /// and the pulls at 200 to 760 play packets 0 to 14, emptying the
    /// buffer. Burst 3 is late (890 ms), so the 800 ms pull finds nothing:
    /// underrun 1, and the target rises to 220 ms. Burst 3 alone is 200 ms,
    /// short of 220; burst 4 at 990 ms makes it 400 ms (not over the
    /// 400 ms limit, so nothing is dropped) and playing resumes at 1000 ms.
    /// From there the pull at 400 + 40p plays packet p, and packet p
    /// arrived at least 110 ms before (the least margin is a late burst's
    /// first packet: 210 - 100), so no later pull finds the buffer empty.
    /// The depth is at most 10 packets, 400 ms. Underruns: 1.
    @Test func aBurstyStreamUnderrunsOnceThenHolds() throws {
        let arrivals = (0..<Self.packets).map { n -> Arrival in
            let burst = n / 5
            return Arrival(atMs: 200 * burst + 190 + (burst % 4 == 3 ? 100 : 0), sequence: UInt16(n))
        }
        let outcome = try Self.run(arrivals, expectedBlocks: Self.packets)
        #expect(outcome.played == Self.packets)
        #expect(outcome.underruns == 1)
        #expect(outcome.concealed == 0)
        #expect(outcome.lateDrops == 0)
        #expect(outcome.boundsHeld)
    }

    /// Two per cent loss: the steady timing, with each packet from 10 to
    /// 1489 lost when a SplitMix64 seeded with 2026 draws a multiple of 50.
    ///
    /// The pull that plays packet p comes 180 ms after p's arrival time, by
    /// which time packets p + 1 to p + 4 have arrived too. The pattern has
    /// no run of four losses (checked below), so whenever p is lost a later
    /// packet is held and the pull conceals it: every loss is concealed,
    /// none is an underrun, and none is late. The first and last ten
    /// packets are kept so the start and the end play as in the steady case.
    @Test func twoPercentLossIsConcealedPacketForPacket() throws {
        var random = SplitMix64(state: 2026)
        var lost = Set<Int>()
        for n in 10..<(Self.packets - 10) where random.next() % 50 == 0 {
            lost.insert(n)
        }
        // The pattern's own shape, which the working above relies on.
        #expect((20...40).contains(lost.count), "about 2 % of 1480")
        #expect(!lost.contains { lost.contains($0 + 1) && lost.contains($0 + 2) && lost.contains($0 + 3) })
        let arrivals = (0..<Self.packets).filter { !lost.contains($0) }
            .map { Arrival(atMs: 40 * $0 + 20, sequence: UInt16($0)) }
        let outcome = try Self.run(arrivals, expectedBlocks: Self.packets)
        #expect(outcome.concealed == lost.count)
        #expect(outcome.concealedBlocks == lost.count)
        #expect(outcome.played == Self.packets - lost.count)
        #expect(outcome.underruns == 0)
        #expect(outcome.lateDrops == 0)
        #expect(outcome.boundsHeld)
    }

    /// One 300 ms gap: the steady timing, except that nothing arrives from
    /// 20000 ms to 20300 ms and the packets due then (500 to 507) all arrive
    /// at 20300 ms.
    ///
    /// Playing began at 200 ms, so the pull at 200 + 40p plays packet p; the
    /// 500 pulls from 200 to 20160 hold no underrun, so the target has
    /// fallen twice, to 140 ms. The 20000 ms pull plays packet 495 and
    /// leaves 496 to 499; the pulls at 20040 to 20160 play them. The
    /// 20200 ms pull finds nothing: underrun 1, the target rises to 180 ms.
    /// The pulls at 20240 and 20280 wait. At 20300 ms packets 500 to 507
    /// make 320 ms, and playing resumes at the 20320 ms pull, which now
    /// plays packet p at 320 + 40p, 300 ms after its arrival: no further
    /// underrun, and at most 8 packets (320 ms) held. Underruns: 1.
    @Test func aThreeHundredMillisecondGapUnderrunsOnce() throws {
        let arrivals = (0..<Self.packets).map { n -> Arrival in
            let due = 40 * n + 20
            return Arrival(atMs: (20001...20300).contains(due) ? 20300 : due, sequence: UInt16(n))
        }
        #expect(arrivals.filter { $0.atMs == 20300 }.map(\.sequence) == Array(500...507))
        let outcome = try Self.run(arrivals, expectedBlocks: Self.packets)
        #expect(outcome.played == Self.packets)
        #expect(outcome.underruns == 1)
        #expect(outcome.concealed == 0)
        #expect(outcome.lateDrops == 0)
        #expect(outcome.boundsHeld)
    }

    // MARK: Rules one at a time

    @Test func eachNewAnchorStartsOverOnItsOwnStream() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.push(Self.packet(0, payload: payload))
        #expect(buffer.depthMs == 0, "nothing is taken before an anchor")
        #expect(buffer.pull() == .silence)

        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: 1, firstSequence: 100, firstTimestamp: 0))
        for sequence in UInt16(100)...104 {
            buffer.push(Self.packet(sequence, ssrc: 1, payload: payload))
        }
        #expect(buffer.depthMs == 200)
        guard case .audio = buffer.pull() else {
            Issue.record("the first stream plays")
            return
        }

        buffer.reanchor(AudioStreamAnchor(generation: 2, ssrc: 2, firstSequence: 5000, firstTimestamp: 99))
        #expect(buffer.depthMs == 0)
        #expect(!buffer.isPlaying)
        buffer.push(Self.packet(105, ssrc: 1, payload: payload))
        buffer.push(Self.packet(5003, ssrc: 1, payload: payload))
        #expect(buffer.depthMs == 0, "the old stream's SSRC is no longer taken")
        for sequence in UInt16(5000)...5004 {
            buffer.push(Self.packet(sequence, ssrc: 2, payload: payload))
        }
        #expect(buffer.depthMs == 200)
        guard case .audio = buffer.pull() else {
            Issue.record("the new stream plays from its first sequence")
            return
        }
        #expect(buffer.depthMs == 160)
        #expect(buffer.underruns == 0)
        #expect(buffer.lateDrops == 0)
    }

    @Test func lateDuplicateAndFarAheadPacketsFollowTheRules() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 65534, firstTimestamp: 0))
        // Across the sequence number wrap.
        for sequence in [UInt16(65534), 65535, 0, 1, 2] {
            buffer.push(Self.packet(sequence, payload: payload))
        }
        _ = buffer.pull()
        _ = buffer.pull()
        #expect(buffer.depthMs == 120)
        buffer.push(Self.packet(65535, payload: payload))
        #expect(buffer.lateDrops == 1, "behind the playout position")
        buffer.push(Self.packet(1, payload: payload))
        #expect(buffer.lateDrops == 1, "a duplicate is dropped uncounted")
        #expect(buffer.depthMs == 120)
        // 40 packets ahead of the position (0): the position moves to 9,
        // dropping the three packets held at 0 to 2.
        buffer.push(Self.packet(40, payload: payload))
        #expect(buffer.lateDrops == 4)
        #expect(buffer.depthMs == 32 * 40)
    }

    @Test func observationSeparatesReceivedAcceptedDuplicateLateAndStartupTrim() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 3, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<12 {
            buffer.push(Self.packet(sequence, payload: payload), arrivalNs: Int64(sequence) * 40_000_000)
        }
        buffer.push(Self.packet(11, payload: payload), arrivalNs: 480_000_000)
        #expect(buffer.observation.receivedContentBytes == UInt64(payload.count * 13))
        #expect(buffer.observation.acceptedPackets == 0, "startup packets are pending classification")
        #expect(buffer.observation.reception.expectedPackets == 0)
        #expect(buffer.observation.duplicatePackets == 1)
        #expect(buffer.observation.reorderQueuedMs == 480)
        #expect(buffer.observation.timelineDepthMs == 480)
        #expect(buffer.observation.adaptiveTargetMs == 180)
        _ = buffer.pull(releasedNs: 500_000_000)
        #expect(buffer.observation.startDiscardedPackets == 7)
        #expect(buffer.observation.acceptedPackets == 5)
        #expect(buffer.observation.reception.expectedPackets == 5)
        #expect(buffer.observation.decodedPackets == 1)
        #expect(buffer.observation.reorderQueuedMs == 160)
        buffer.push(Self.packet(0, payload: payload), arrivalNs: 520_000_000)
        #expect(buffer.observation.latePackets == 1)
        #expect(buffer.observation.acceptedPackets == 5)
        #expect(buffer.observation.lastAdmittedPacketNs == 440_000_000)
    }

    @Test func malformedOrWrongDurationPayloadCannotRefreshAdmittedAge() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        buffer.push(Self.packet(0, payload: payload), arrivalNs: 10)
        let validAge = buffer.observation.lastAdmittedPacketNs
        #expect(validAge == nil, "startup packets are not finally admitted yet")
        buffer.push(Self.packet(1, payload: Data([0])), arrivalNs: 20)
        #expect(buffer.observation.lastAdmittedPacketNs == validAge)
        #expect(buffer.observation.invalidPackets == 1)
        #expect(buffer.depthMs == 80, "the playback decision is unchanged")
        #expect(buffer.observation.acceptedPackets == 0)
        _ = buffer.reanchor(nil)
        #expect(buffer.observation.acceptedPackets == 0)
    }

    @Test func decodableWrongDurationCannotPublishAReleasePoint() throws {
        let valid = try Self.opusPayload()
        let profile = OpusEncoder.Profile(channels: 2, sampleRate: 48_000, frameSamples: 960,
                                          application: .audio, bitrate: 48_000, variableBitrate: true,
                                          constrainedVariableBitrate: false, inbandFEC: false,
                                          expectedLossPercent: 0, discontinuousTransmission: false,
                                          complexity: 5, signal: .auto)
        let short = try OpusEncoder(profile: profile).encode([Float](repeating: 0, count: 960 * 2))
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<5 { buffer.push(Self.packet(sequence, payload: valid), arrivalNs: Int64(sequence)) }
        _ = buffer.pull(releasedNs: 1_000)
        #expect(buffer.observation.release?.rtpTimestamp == 1_920)
        for _ in 1..<5 { _ = buffer.pull(releasedNs: 2_000) }
        buffer.push(Self.packet(5, payload: short), arrivalNs: 3_000)
        guard case .audio = buffer.pull(releasedNs: 4_000) else {
            Issue.record("the 20 ms Opus packet still decodes under the established playback behavior")
            return
        }
        #expect(buffer.observation.invalidPackets == 1)
        #expect(buffer.observation.release == nil)
        #expect(buffer.lastPullRelease == nil)
    }

    @Test func releaseUsesRealPacketEndTimestampOnly() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 20, firstTimestamp: 70_000))
        for sequence in UInt16(20)...24 {
            buffer.push(RtpPacket(payloadType: 111, sequence: sequence,
                                  timestamp: 70_000 &+ UInt32(sequence - 20) * 1_920,
                                  ssrc: Self.ssrc, payload: payload), arrivalNs: Int64(sequence) * 40_000_000)
        }
        _ = buffer.pull(releasedNs: 1_000_000_000)
        #expect(buffer.observation.release == AudioReleasePoint(rtpTimestamp: 71_920,
                                                                 releasedNs: 1_000_000_000))
        buffer.reanchor(nil)
        _ = buffer.pull(releasedNs: 1_100_000_000)
        #expect(buffer.observation.release == nil)
    }

    @Test func overFourHundredMillisecondsIsCutToTheTarget() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<12 {
            buffer.push(Self.packet(sequence, payload: payload))
        }
        #expect(buffer.depthMs == 480)
        // 480 ms is over the limit: the oldest seven go, leaving five
        // packets (200 ms, the 180 ms target rounded up), and the pull plays.
        guard case .audio = buffer.pull() else {
            Issue.record("playing begins at the target")
            return
        }
        #expect(buffer.lateDrops == 7)
        #expect(buffer.depthMs == 160)
    }

    @Test func theTargetRisesWithUnderrunsToNoMoreThanFourHundred() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        var sequence: UInt16 = 0
        var targets: [Int] = []
        for _ in 0..<8 {
            // Just enough to start playing, then played out to an underrun.
            let packets = (buffer.targetMs + 39) / 40
            for _ in 0..<packets {
                buffer.push(Self.packet(sequence, payload: payload))
                sequence &+= 1
            }
            for _ in 0...packets {
                _ = buffer.pull()
            }
            targets.append(buffer.targetMs)
        }
        #expect(buffer.underruns == 8)
        #expect(targets == [220, 260, 300, 340, 380, 400, 400, 400])
    }

    @Test func aPacketThatIsNotOpusNeverReachesTheDecoder() throws {
        let payload = try Self.opusPayload()
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        // L16 at payload type 96 on the stream's own SSRC, at the playout
        // position and ahead of it: dropped and counted, never held.
        let l16 = Data(repeating: 0x40, count: 768)
        buffer.push(RtpPacket(payloadType: 96, sequence: 0, timestamp: 0, ssrc: Self.ssrc, payload: l16))
        buffer.push(RtpPacket(payloadType: 96, sequence: 3, timestamp: 5760, ssrc: Self.ssrc, payload: l16))
        #expect(buffer.otherPayloadDrops == 2)
        #expect(buffer.depthMs == 0)
        // Another stream's packets are not the anchored stream's to count.
        buffer.push(RtpPacket(payloadType: 96, sequence: 1, timestamp: 0, ssrc: Self.ssrc &+ 1, payload: l16))
        #expect(buffer.otherPayloadDrops == 2)
        // The Opus packets at the same sequence numbers play.
        for sequence in UInt16(0)...4 {
            buffer.push(Self.packet(sequence, payload: payload))
        }
        guard case .audio = buffer.pull() else {
            Issue.record("the Opus packet plays")
            return
        }
        #expect(buffer.concealed == 0)
    }

    @Test func aPacketTheDecoderCannotReadIsConcealed() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        let payload = try Self.opusPayload()
        // An Opus TOC byte promising more frames than the packet holds.
        buffer.push(Self.packet(0, payload: Data([0xFF, 0xFF])))
        for sequence in UInt16(1)...4 {
            buffer.push(Self.packet(sequence, payload: payload))
        }
        guard case .concealed(let pcm) = buffer.pull() else {
            Issue.record("an unreadable packet is concealed")
            return
        }
        #expect(pcm.count == 1920 * 2)
        #expect(buffer.concealed == 1)
    }
}
