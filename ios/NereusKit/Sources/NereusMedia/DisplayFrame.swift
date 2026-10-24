// NereusSDR for iOS: one decoded display frame, the Core's trace and waterfall rows for an endpoint
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-22: NSDC v2 frames and traceCarried.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation

/// One accepted NSDC v1 frame (docs/architecture/2026-09-20-display-codec-v1.md)
/// or released NSDC v2 frame (docs/architecture/2026-10-22-display-codec-v2-intra-rows.md):
/// its header and the reconstructed trace, waterfall and optional wide rows,
/// in antenna-referenced dBm. The frequency axis is not part of the frame; it
/// comes with the endpoint's context.
public struct DisplayFrame: Equatable, Sendable {
    public let endpointId: UInt32
    public let contextGeneration: UInt32
    public let encoderSequence: UInt32
    /// Sender-monotonic nanoseconds; ordered only against the same sender's
    /// frames, never against a clock on the phone.
    public let producerTimestamp: UInt64
    /// Always false for NSDC v2, which has no keyframes.
    public let isKeyframe: Bool
    public let waterfallAdvance: Bool
    public let minDbm: Float
    public let maxDbm: Float
    public let traceDbm: [Float]
    public let waterfallDbm: [Float]
    /// Empty when the frame has no wide row.
    public let wideDbm: [Float]
    /// NSDC v2: the trace is the last decoded one, repeated because this
    /// frame's trace part was lost. Never current for measurement.
    public let traceCarried: Bool

    public init(endpointId: UInt32, contextGeneration: UInt32, encoderSequence: UInt32,
                producerTimestamp: UInt64, isKeyframe: Bool, waterfallAdvance: Bool,
                minDbm: Float, maxDbm: Float, traceDbm: [Float], waterfallDbm: [Float],
                wideDbm: [Float], traceCarried: Bool = false) {
        self.endpointId = endpointId
        self.contextGeneration = contextGeneration
        self.encoderSequence = encoderSequence
        self.producerTimestamp = producerTimestamp
        self.isKeyframe = isKeyframe
        self.waterfallAdvance = waterfallAdvance
        self.minDbm = minDbm
        self.maxDbm = maxDbm
        self.traceDbm = traceDbm
        self.waterfallDbm = waterfallDbm
        self.wideDbm = wideDbm
        self.traceCarried = traceCarried
    }
}
