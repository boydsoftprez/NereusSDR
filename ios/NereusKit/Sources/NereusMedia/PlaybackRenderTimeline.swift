// NereusSDR for iOS: one output lifetime's validated device render progress
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Checks consecutive AVAudioTime readings without assuming its sample
/// clock is the 48 kHz source clock. This counts rendering, not audibility.
public struct PlaybackRenderTimeline: Sendable {
    private var firstSample: Int64?
    private var previousSample: Int64?
    private var previousHostTime: UInt64?
    private var rateHz: Double?
    private var discontinuous = false

    public init() {}

    public mutating func reset() { self = Self() }

    /// Nil remains sticky until the owner creates a new output epoch.
    public mutating func progress(sampleTime: Int64, hostTime: UInt64, rateHz: Double) -> Int64? {
        guard !discontinuous else { return nil }
        guard sampleTime >= 0, hostTime > 0, rateHz.isFinite, rateHz > 0 else {
            discontinuous = true
            return nil
        }
        if let previousSample, let previousHostTime, let previousRate = self.rateHz,
           (sampleTime < previousSample || hostTime < previousHostTime || rateHz != previousRate) {
            discontinuous = true
            return nil
        }
        if firstSample == nil { firstSample = sampleTime }
        previousSample = sampleTime
        previousHostTime = hostTime
        self.rateHz = rateHz
        return sampleTime - (firstSample ?? sampleTime)
    }
}
