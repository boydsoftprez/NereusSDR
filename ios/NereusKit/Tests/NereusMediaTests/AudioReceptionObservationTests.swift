// NereusSDR for iOS: packet reception accounting at the playback admission boundary
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusMedia

@Suite struct AudioReceptionObservationTests {
    @Test func receiveProfileLookaheadComesFromTheVendoredOpusEncoder() {
        #expect(AudioPlaybackCodecDelay.frames == 312)
    }

    @Test func firstPacketHasNoArrivalJitterAndSequenceWrapExtendsExpectedSpan() {
        var stats = AudioReceptionStats()
        stats.observe(sequence: 65_534, timestamp: UInt32.max - 1_919, arrivalNs: 1_000_000_000)
        #expect(stats.receivedPackets == 1)
        #expect(stats.expectedPackets == 1)
        #expect(stats.missingPackets == 0)
        #expect(stats.arrivalJitterMs == nil)

        // The RTP timestamp and sequence cross their unsigned boundaries.
        stats.observe(sequence: 0, timestamp: 1_920, arrivalNs: 1_080_000_000)
        #expect(stats.receivedPackets == 2)
        #expect(stats.expectedPackets == 3)
        #expect(stats.missingPackets == 1)
        #expect(stats.arrivalJitterMs == 0)

        // A reordered arrival is received, but cannot move the highest sequence.
        stats.observe(sequence: 65_535, timestamp: 0, arrivalNs: 1_100_000_000)
        #expect(stats.receivedPackets == 3)
        #expect(stats.expectedPackets == 3)
        #expect(stats.missingPackets == 0)
        #expect(stats.arrivalJitterMs == 3.75)
    }

    @Test func resetStartsASeparateSemanticStream() {
        var stats = AudioReceptionStats()
        stats.observe(sequence: 4, timestamp: 7_680, arrivalNs: 200_000_000)
        stats.observe(sequence: 5, timestamp: 9_600, arrivalNs: 240_000_000)
        #expect(stats.arrivalJitterMs == 0)
        stats.reset()
        #expect(stats.receivedPackets == 0)
        #expect(stats.expectedPackets == 0)
        #expect(stats.missingPackets == 0)
        #expect(stats.arrivalJitterMs == nil)
    }
}
