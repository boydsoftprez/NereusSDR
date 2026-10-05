// NereusSDR for iOS: wrap-aware RTP reception statistics for one semantic audio stream
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// RFC 3550 sequence-span and interarrival-jitter arithmetic for admitted
/// playback packets. The feed queue owns this value; callers supply a monotonic
/// arrival clock. Reordered packets count as received without moving the high
/// sequence. This follows NereusSDR's original RtpReceptionStats contract.
public struct AudioReceptionStats: Sendable, Equatable {
    public private(set) var receivedPackets: UInt64 = 0
    public private(set) var expectedPackets: UInt64 = 0
    public var missingPackets: UInt64 {
        expectedPackets > receivedPackets ? expectedPackets - receivedPackets : 0
    }
    public private(set) var arrivalJitterMs: Double?

    private var firstSequence: UInt16?
    private var maximumSequence: UInt16 = 0
    private var cycles: UInt64 = 0
    private var previousArrivalNs: Int64?
    private var previousTimestamp: UInt32 = 0
    private var jitterNs = 0.0

    public init() {}

    public mutating func reset() { self = Self() }

    public mutating func observe(sequence: UInt16, timestamp: UInt32, arrivalNs: Int64) {
        guard arrivalNs >= 0 else { return }
        if let firstSequence {
            let forward = sequence &- maximumSequence
            if forward < 0x8000 {
                if sequence < maximumSequence { cycles += 65_536 }
                maximumSequence = sequence
            }
            expectedPackets = cycles + UInt64(maximumSequence) - UInt64(firstSequence) + 1
        } else {
            firstSequence = sequence
            maximumSequence = sequence
            expectedPackets = 1
        }
        receivedPackets += 1
        if let previousArrivalNs {
            let tickDelta = Int32(bitPattern: timestamp &- previousTimestamp)
            let expectedNs = Double(tickDelta) * 1_000_000_000 / 48_000
            let deviationNs = Double(arrivalNs) - Double(previousArrivalNs) - expectedNs
            jitterNs += (abs(deviationNs) - jitterNs) / 16
            arrivalJitterMs = jitterNs / 1_000_000
        }
        previousArrivalNs = arrivalNs
        previousTimestamp = timestamp
    }
}
