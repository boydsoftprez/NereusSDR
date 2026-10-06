// NereusSDR for iOS: one decoded display frame, the Core's trace and waterfall rows for an endpoint
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One accepted NSDC v1 frame (docs/architecture/2026-09-20-display-codec-v1.md):
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
    public let isKeyframe: Bool
    public let waterfallAdvance: Bool
    public let minDbm: Float
    public let maxDbm: Float
    public let traceDbm: [Float]
    public let waterfallDbm: [Float]
    /// Empty when the frame has no wide row.
    public let wideDbm: [Float]

    public init(endpointId: UInt32, contextGeneration: UInt32, encoderSequence: UInt32,
                producerTimestamp: UInt64, isKeyframe: Bool, waterfallAdvance: Bool,
                minDbm: Float, maxDbm: Float, traceDbm: [Float], waterfallDbm: [Float],
                wideDbm: [Float]) {
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
    }
}
