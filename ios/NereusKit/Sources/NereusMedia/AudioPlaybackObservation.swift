// NereusSDR for iOS: immutable playback measurements for diagnostics collection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One feed-owned reading. Counters are retained when a stream retires;
/// live gauges are absent immediately when the stream or output retires.
public struct AudioPlaybackObservation: Sendable, Equatable {
    public let lifetimeID: UInt64
    public let streamEpoch: UInt64
    public let stream: AudioStreamAnchor?
    /// Identity of the last stream after retirement, retained for its final counters.
    public let retiredStream: AudioStreamAnchor?
    public let outputEpoch: UInt64?
    public let counters: AudioJitterBuffer.Observation
    public let lifetimeUnderflows: UInt64
    public let lifetimeOverflows: UInt64
    public let renderUnderruns: UInt64
    public let streamRenderUnderruns: UInt64?
    /// The complete source-node callback quantum, in 48 kHz source frames.
    public let lastCallbackSourceFrames: Int?
    public let requestedSourceFrames: UInt64
    public let renderedSourceFrames: UInt64
    public let droppedRenderObservations: UInt64
    public let renderEvidenceRecoveredAfterDrop: Bool
    public let lastAdmittedPacketAgeMs: Double?
    public let reorderQueuedMs: Int?
    public let timelineDepthMs: Int?
    public let adaptiveTargetMs: Int?
    /// Ring plus resampler prefetch, both in 48 kHz source frames. This does
    /// not include the AVAudioEngine/hardware queue or device latency.
    public let speakerQueuedSourceFrames: Double?
    public let measuredDriftPpm: Double?
    public let release: AudioReleasePoint?
    /// A valid RTP position in the source render timeline. Hardware progress
    /// and rate must come from EnginePlaybackOutput before measuring delay.
    public let sourcePlayout: AudioSourcePlayoutObservation?
    /// Raw HAL schedule for the same current source callback record. This is
    /// neither consumed audio nor an audible playback or queue measurement.
    public let sourceRenderSchedule: AudioSourceRenderScheduleObservation?

    init(lifetimeID: UInt64, streamEpoch: UInt64, stream: AudioStreamAnchor?,
         retiredStream: AudioStreamAnchor?, outputEpoch: UInt64?,
         counters: AudioJitterBuffer.Observation, lifetimeUnderflows: UInt64,
         lifetimeOverflows: UInt64, renderUnderruns: UInt64, streamRenderUnderruns: UInt64?,
         lastCallbackSourceFrames: Int?, requestedSourceFrames: UInt64,
         renderedSourceFrames: UInt64, droppedRenderObservations: UInt64,
         renderEvidenceRecoveredAfterDrop: Bool, lastAdmittedPacketAgeMs: Double?,
         reorderQueuedMs: Int?, timelineDepthMs: Int?, adaptiveTargetMs: Int?,
         speakerQueuedSourceFrames: Double?, measuredDriftPpm: Double?,
         release: AudioReleasePoint?, sourcePlayout: AudioSourcePlayoutObservation?,
         sourceRenderSchedule: AudioSourceRenderScheduleObservation? = nil) {
        self.lifetimeID = lifetimeID
        self.streamEpoch = streamEpoch
        self.stream = stream
        self.retiredStream = retiredStream
        self.outputEpoch = outputEpoch
        self.counters = counters
        self.lifetimeUnderflows = lifetimeUnderflows
        self.lifetimeOverflows = lifetimeOverflows
        self.renderUnderruns = renderUnderruns
        self.streamRenderUnderruns = streamRenderUnderruns
        self.lastCallbackSourceFrames = lastCallbackSourceFrames
        self.requestedSourceFrames = requestedSourceFrames
        self.renderedSourceFrames = renderedSourceFrames
        self.droppedRenderObservations = droppedRenderObservations
        self.renderEvidenceRecoveredAfterDrop = renderEvidenceRecoveredAfterDrop
        self.lastAdmittedPacketAgeMs = lastAdmittedPacketAgeMs
        self.reorderQueuedMs = reorderQueuedMs
        self.timelineDepthMs = timelineDepthMs
        self.adaptiveTargetMs = adaptiveTargetMs
        self.speakerQueuedSourceFrames = speakerQueuedSourceFrames
        self.measuredDriftPpm = measuredDriftPpm
        self.release = release
        self.sourcePlayout = sourcePlayout
        self.sourceRenderSchedule = sourceRenderSchedule
    }
}

public struct AudioSourceRenderScheduleObservation: Sendable, Equatable {
    public let streamEpoch: UInt64
    public let outputEpoch: UInt64
    public let measuredNs: Int64
    public let readWindowNs: Int64
    /// These are frames of the fixed 48 kHz source-node format, not device frames.
    public let sourceRateHz: Double
    public let callbackSourceFrames: Int
    public let chunkStartSourceFrame: Int
    public let chunkEndSourceFrame: Int
    public let rawTimestampFlags: UInt32
    /// Each representation is independently optional according to its own flag.
    public let sourceHostTicks: UInt64?
    /// Checked mach timebase conversion, absent on overflow or bad timebase.
    public let sourceHostTimeNs: UInt64?
    public let sourceSampleTime: Double?
}

public struct AudioSourcePlayoutObservation: Sendable, Equatable {
    /// RTP time at the end of the newest valid sample handed to the matcher.
    public let rtpTimestamp: UInt32
    public let measuredNs: Int64
    public let readWindowNs: Int64
    public let callbackSourceFrames: Int
    /// Source samples already pulled from the ring but still in the matcher.
    /// Unread ring frames are later samples and excluded from this point.
    public let matcherPrefetchedSourceFrames: Double
    public let unreadRingSourceFrames: Int
    public let ratio: Double
    public let outputEpoch: UInt64
}
