// NereusSDR for iOS: plays the Core's audio, from the jitter buffer through the rate matcher to the audio render callback
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import CAudioRing
import Darwin
import Foundation
import os

/// The playback path for the Core's audio (spec section 4.1: playback runs
/// on the phone). Packets go into an ``AudioJitterBuffer`` on a feed queue,
/// which decodes each 40 ms block when it falls due into a lock-free ring.
/// The audio render callback reads the ring through a ``DriftResampler``
/// into the output. The render path never allocates and never locks: it
/// reads only the ring and two shared numbers (the buffer's depth and
/// target), which the feed queue publishes.
///
/// A block falls due when the ring holds less than its low water mark, so
/// the buffer's pulls follow the output device's clock. While audio is off
/// (no stream anchored) nothing is written and the output is silence.
///
/// The low water mark follows the output's piece size, so the ring holds
/// only what the output needs and no more latency than that: one piece
/// (``callbackFrames``) plus ``feedCoverFrames`` (20 ms, the feed's 10 ms
/// interval, its leeway and a margin), whatever the I/O buffer is, 5 ms,
/// 10 ms or iOS's default. A render that finds the ring dry while the
/// buffer plays counts as a render underrun (``renderUnderruns``) and adds
/// ``coverStepFrames`` to the mark, up to ``maximumExtraCoverFrames``. The
/// counts, with the piece size and the mark, go to the log once a minute
/// while a stream plays and when it ends
/// (``summaryLine(_:counters:renderUnderruns:callbackFrames:lowWaterFrames:)``).
public final class AudioPlaybackCore: @unchecked Sendable {
    public static let sampleRate = 48000.0
    public static let channels = 2
    /// The ring's size, in frames: room for several blocks.
    static let ringFrames = 8192
    /// The render path works in pieces of at most this many frames.
    static let renderChunkFrames = 1024
    /// The piece size assumed until the output has asked for one.
    static let unknownCallbackFrames = renderChunkFrames
    /// What the low water mark holds beyond one piece: 20 ms, which covers
    /// the feed's 10 ms interval, its 2 ms leeway and a margin.
    static let feedCoverFrames = 960
    /// How much each dry render adds to the mark: 5 ms.
    static let coverStepFrames = 240
    /// The most dry renders add: 80 ms.
    static let maximumExtraCoverFrames = 2 * AudioJitterBuffer.packetFrames
    /// How often the feed queue checks whether a block is due.
    static let feedInterval: DispatchTimeInterval = .milliseconds(10)
    /// How late a feed check may run: inside ``feedCoverFrames``.
    static let feedLeeway: DispatchTimeInterval = .milliseconds(2)
    /// How often the counts go to the log while a stream plays, in ms.
    static let summaryIntervalMs: UInt64 = 60_000

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.playback")

    /// Above the display and the session's work, so the thin ring is fed on time.
    private let feedQueue = DispatchQueue(label: "NereusSDR.audio.playback.feed", qos: .userInteractive)

    // The feed queue's own.
    private let jitter: AudioJitterBuffer
    private var feedTimer: DispatchSourceTimer?
    /// When the counts last went to the log, in ms of uptime.
    private var lastSummaryMs: UInt64 = 0
    /// Added to the low water mark by dry renders, and the dry renders
    /// already answered.
    private var extraCoverFrames = 0
    private var answeredRenderUnderruns = 0
    private let lifetimeID = DispatchTime.now().uptimeNanoseconds
    private var streamEpoch: UInt64 = 0
    private var retiredCounters = AudioJitterBuffer.Observation()
    private var retiredStream: AudioStreamAnchor?
    private var lifetimeBufferUnderflows: UInt64 = 0
    private var lifetimeOverflows: UInt64 = 0
    private var retiredStreamRenderUnderruns: UInt64?
    private var knownStreamRenderUnderruns: UInt64? = 0
    private var renderCrossingsSeen: UInt64 = 0
    /// Old queued records cannot close a retirement/publication evidence gap.
    private var streamCounterNeedsCurrentEvidence = false
    private var latestRender: nereus_render_observation?
    private var latestSchedule: nereus_render_observation?
    private var droppedRenderSeen: UInt64 = 0
    private var renderEvidenceRecoveredAfterDrop = false
    private struct RingSegment {
        let first: UInt64
        let end: UInt64
        let epoch: UInt64
        let rtpEnd: UInt32?
    }
    private var ringSegments: [RingSegment] = []

    // Shared with the render thread, lock-free.
    private let ring: OpaquePointer
    private let publishedDepthMs: OpaquePointer
    private let publishedTargetMs: OpaquePointer
    /// Written by the render thread only: its dry renders while playing,
    /// and the frames it was last asked for.
    private let renderUnderrunCount: OpaquePointer
    private let lastCallbackFrames: OpaquePointer
    let renderObservations: OpaquePointer
    private let publishedStreamEpoch: OpaquePointer
    private let publishedOutputEpoch: OpaquePointer
    private let outputSequence: OpaquePointer
    private let snapshotClaim: OpaquePointer
    private let requestedSourceFrames: OpaquePointer
    private let renderedSourceFrames: OpaquePointer
    private let renderFence: OpaquePointer
    private let renderEpochCrossings: OpaquePointer

    // The render thread's own.
    private let resampler: DriftResampler
    private let scratch: UnsafeMutablePointer<Float>
    private let timebaseNumer: UInt32
    private let timebaseDenom: UInt32
    private var renderObservedStreamEpoch: UInt64 = 0
    private var renderStreamUnderrunCount: UInt64 = 0

    public struct Counters: Sendable, Equatable {
        public var underruns: Int
        public var lateDrops: Int
        public var concealed: Int
        /// Packets with a payload type other than Opus's, dropped undecoded.
        public var otherPayloadDrops: Int

        public init(underruns: Int, lateDrops: Int, concealed: Int, otherPayloadDrops: Int = 0) {
            self.underruns = underruns
            self.lateDrops = lateDrops
            self.concealed = concealed
            self.otherPayloadDrops = otherPayloadDrops
        }
    }

    public init() throws {
        jitter = try AudioJitterBuffer()
        var timebase = mach_timebase_info_data_t()
        let timebaseResult = mach_timebase_info(&timebase)
        timebaseNumer = timebaseResult == KERN_SUCCESS ? timebase.numer : 0
        timebaseDenom = timebaseResult == KERN_SUCCESS ? timebase.denom : 0
        guard let ring = nereus_audio_ring_create(Self.ringFrames, Self.channels),
              let depth = nereus_shared_value_create(Self.notSteering),
              let target = nereus_shared_value_create(0),
              let dry = nereus_shared_value_create(0),
              let callback = nereus_shared_value_create(0),
              let observations = nereus_render_observation_queue_create(),
              let streamEpoch = nereus_shared_value_create(0),
              let outputEpoch = nereus_shared_value_create(0),
              let outputSequence = nereus_shared_value_create(0),
              let claim = nereus_shared_value_create(0),
              let requested = nereus_shared_value_create(0),
              let rendered = nereus_shared_value_create(0),
              let fence = nereus_render_fence_create(),
              let crossings = nereus_shared_value_create(0) else {
            throw AudioPlaybackError.outOfMemory
        }
        self.ring = ring
        publishedDepthMs = depth
        publishedTargetMs = target
        renderUnderrunCount = dry
        lastCallbackFrames = callback
        renderObservations = observations
        publishedStreamEpoch = streamEpoch
        publishedOutputEpoch = outputEpoch
        self.outputSequence = outputSequence
        snapshotClaim = claim
        requestedSourceFrames = requested
        renderedSourceFrames = rendered
        renderFence = fence
        renderEpochCrossings = crossings
        resampler = DriftResampler(channels: Self.channels)
        scratch = .allocate(capacity: Self.renderChunkFrames * Self.channels)
        scratch.initialize(repeating: 0, count: Self.renderChunkFrames * Self.channels)
    }

    deinit {
        feedTimer?.cancel()
        nereus_audio_ring_destroy(ring)
        nereus_shared_value_destroy(publishedDepthMs)
        nereus_shared_value_destroy(publishedTargetMs)
        nereus_shared_value_destroy(renderUnderrunCount)
        nereus_shared_value_destroy(lastCallbackFrames)
        nereus_render_observation_queue_destroy(renderObservations)
        nereus_shared_value_destroy(publishedStreamEpoch)
        nereus_shared_value_destroy(publishedOutputEpoch)
        nereus_shared_value_destroy(outputSequence)
        nereus_shared_value_destroy(snapshotClaim)
        nereus_shared_value_destroy(requestedSourceFrames)
        nereus_shared_value_destroy(renderedSourceFrames)
        nereus_render_fence_destroy(renderFence)
        nereus_shared_value_destroy(renderEpochCrossings)
        scratch.deallocate()
    }

    // MARK: The feed side

    /// Starts over on a new audio context's stream, or turns audio off with nil.
    public func reanchor(_ anchor: AudioStreamAnchor?) {
        feedQueue.async { [self] in
            reanchorOnFeed(anchor)
        }
    }

    private func reanchorOnFeed(_ anchor: AudioStreamAnchor?) {
        dispatchPrecondition(condition: .onQueue(feedQueue))
        drainRenderObservations()
        let oldAnchor = jitter.anchor
        guard oldAnchor != nil || anchor != nil else { return }
        if oldAnchor != nil {
            logSummary(anchor == nil ? "session end" : "stream change")
            retiredStream = oldAnchor
            lifetimeBufferUnderflows += jitter.observation.underflowEvents
            lifetimeOverflows += jitter.observation.overflowEvents
        }
        nereus_shared_value_store(publishedDepthMs, Self.notSteering)
        let oldEpoch = streamEpoch
        streamEpoch &+= 1
        nereus_shared_value_store(publishedStreamEpoch, Int64(bitPattern: streamEpoch))
        let overlapping = nereus_render_fence_retire(renderFence) != 0
        if overlapping { _ = nereus_shared_value_add(renderEpochCrossings, 1) }
        // A callback that read the old epoch has entered the fence already.
        // Finished records are drained below; a crossing or live callback
        // leaves the final per-stream total explicitly unavailable.
        drainRenderObservations(counterEpoch: oldEpoch)
        if oldAnchor != nil {
            retiredStreamRenderUnderruns = overlapping ? nil : knownStreamRenderUnderruns
        }
        let previous = jitter.reanchor(anchor)
        if oldAnchor != nil { retiredCounters = previous }
        knownStreamRenderUnderruns = anchor == nil ? nil : 0
        streamCounterNeedsCurrentEvidence = false
        latestRender = nil
        latestSchedule = nil
        renderEvidenceRecoveredAfterDrop = false
        lastSummaryMs = Self.uptimeMs()
        publish()
    }

    /// Makes replacement packets eligible for the existing playout queue.
    public func acceptEquivalentSsrc(_ ssrc: UInt32) {
        feedQueue.async { [self] in jitter.acceptEquivalentSsrc(ssrc) }
    }

    /// A later context on the replacement peer keeps the same playout clock.
    public func adoptEquivalentAnchor(_ anchor: AudioStreamAnchor) {
        feedQueue.async { [self] in
            if jitter.acceptsEquivalentSsrc(anchor.ssrc) { jitter.adoptEquivalentAnchor(anchor) }
            else { reanchorOnFeed(anchor) }
        }
    }

    /// The Core's old peer has drained; no old-path packets enter again.
    public func keepOnlySsrc(_ ssrc: UInt32) {
        feedQueue.async { [self] in jitter.keepOnlySsrc(ssrc) }
    }

    public func beginReplacement() {
        feedQueue.async { [self] in jitter.beginReplacement() }
    }

    public func replacementLeadEased() {
        feedQueue.async { [self] in jitter.replacementLeadEased() }
    }

    public func endReplacement() {
        feedQueue.async { [self] in jitter.endReplacement() }
    }

    /// One received packet.
    public func receive(_ packet: RtpPacket) {
        let arrivedNs = Int64(DispatchTime.now().uptimeNanoseconds)
        feedQueue.async { [self] in
            jitter.push(packet, arrivalNs: arrivedNs)
            pump()
        }
    }

    /// Checks for due blocks every ``feedInterval`` until ``stop()``.
    public func start() {
        feedQueue.async { [self] in
            guard feedTimer == nil else {
                return
            }
            let timer = DispatchSource.makeTimerSource(queue: feedQueue)
            timer.schedule(deadline: .now(), repeating: Self.feedInterval, leeway: Self.feedLeeway)
            timer.setEventHandler { [weak self] in
                self?.pump()
                self?.summarizeIfDue()
            }
            feedTimer = timer
            timer.resume()
        }
    }

    public func stop() {
        outputDidRetire()
        feedQueue.async { [self] in
            feedTimer?.cancel()
            feedTimer = nil
        }
    }

    /// A new AVAudioEngine output lifetime. Old callback records cannot
    /// become current after a pause, reset or route configuration change.
    @discardableResult public func outputDidStart() -> UInt64 {
        let next = nereus_shared_value_add(outputSequence, 1)
        nereus_shared_value_store(publishedOutputEpoch, next)
        return UInt64(next)
    }

    public func outputDidRetire() {
        nereus_shared_value_store(publishedOutputEpoch, 0)
        if nereus_render_fence_retire(renderFence) != 0 {
            _ = nereus_shared_value_add(renderEpochCrossings, 1)
        }
    }

    public var currentOutputEpoch: UInt64? {
        let epoch = nereus_shared_value_load(publishedOutputEpoch)
        return epoch > 0 ? UInt64(epoch) : nil
    }

    /// Atomic stream identity for validating a later actor-owned snapshot.
    public var currentStreamEpoch: UInt64 {
        UInt64(bitPattern: nereus_shared_value_load(publishedStreamEpoch))
    }

    /// At most one pending feed read. A caller must recheck its current
    /// session owner after suspension before using the returned value.
    public func observation() async -> AudioPlaybackObservation? {
        guard nereus_shared_value_claim(snapshotClaim) != 0 else { return nil }
        return await withCheckedContinuation { continuation in
            feedQueue.async { [self] in
                let value = makeObservation()
                nereus_shared_value_store(snapshotClaim, 0)
                continuation.resume(returning: value)
            }
        }
    }

    /// The jitter buffer's counters.
    public var counters: Counters {
        feedQueue.sync {
            Counters(underruns: jitter.underruns, lateDrops: jitter.lateDrops, concealed: jitter.concealed,
                     otherPayloadDrops: jitter.otherPayloadDrops)
        }
    }

    /// Renders that found the ring dry while the buffer played.
    public var renderUnderruns: Int {
        Int(nereus_shared_value_load(renderUnderrunCount))
    }

    /// The frames the output last asked for in one render.
    public var callbackFrames: Int {
        Int(nereus_shared_value_load(lastCallbackFrames))
    }

    /// The stream being played, or nil while audio is off.
    public var anchor: AudioStreamAnchor? {
        feedQueue.sync { jitter.anchor }
    }

    /// Waits for everything handed to the feed queue so far (tests).
    func flush() {
        feedQueue.sync {}
    }

    /// The ring's low water mark now (tests).
    var currentLowWaterFrames: Int {
        feedQueue.sync { lowWaterFrames }
    }

    /// The frames the ring holds now (tests).
    var heldRingFrames: Int {
        nereus_audio_ring_readable(ring)
    }

    var ringSegmentCount: Int { feedQueue.sync { ringSegments.count } }

    struct RenderBoundary {
        let stream: UInt64
        let output: UInt64
        let ticket: UInt64
    }

    /// Same bounded epoch fence used by the actual render path. Tests can
    /// hold a boundary across retirement without blocking an audio callback.
    func beginRenderBoundary() -> RenderBoundary {
        let ticket = nereus_render_fence_begin(renderFence)
        return RenderBoundary(stream: UInt64(nereus_shared_value_load(publishedStreamEpoch)),
                              output: UInt64(nereus_shared_value_load(publishedOutputEpoch)), ticket: ticket)
    }

    func endRenderBoundary(_ boundary: RenderBoundary) -> Bool {
        let stable = boundary.stream == UInt64(nereus_shared_value_load(publishedStreamEpoch))
            && boundary.output == UInt64(nereus_shared_value_load(publishedOutputEpoch))
        let sameLifetime = nereus_render_fence_end(renderFence, boundary.ticket) != 0
        return stable && sameLifetime
    }

    /// Runs one feed check now, as the feed timer would (tests).
    func pumpNow() {
        feedQueue.sync { pump() }
    }

    /// Runs `body` on the feed queue with the jitter buffer (tests).
    func withJitterBuffer<Result>(_ body: (AudioJitterBuffer) -> Result) -> Result {
        feedQueue.sync { body(jitter) }
    }

    /// The rate matcher's ratio (tests, while nothing renders).
    var resamplerRatio: Double {
        resampler.ratio
    }

    /// The ring's low water mark now: one piece of the output, the feed's
    /// cover and what dry renders added, never more than leaves room for a
    /// block. Feed queue only.
    private var lowWaterFrames: Int {
        let piece = callbackFrames > 0 ? callbackFrames : Self.unknownCallbackFrames
        return min(piece + Self.feedCoverFrames + extraCoverFrames, Self.ringFrames - AudioJitterBuffer.packetFrames)
    }

    /// What the ring holds on average while playing, in ms: the low water
    /// mark and half a block. The rate matcher steers the jitter buffer's
    /// depth plus the ring's to the buffer's target plus this.
    static func nominalRingMs(lowWaterFrames: Int) -> Int {
        Int((Double(lowWaterFrames + AudioJitterBuffer.packetFrames / 2) * 1000 / sampleRate).rounded())
    }

    /// Decodes due blocks into the ring. Feed queue only.
    func pump() {
        dispatchPrecondition(condition: .onQueue(feedQueue))
        drainRenderObservations()
        let dry = renderUnderruns
        if dry > answeredRenderUnderruns {
            answeredRenderUnderruns = dry
            extraCoverFrames = min(extraCoverFrames + Self.coverStepFrames, Self.maximumExtraCoverFrames)
        }
        let lowWater = lowWaterFrames
        while jitter.anchor != nil,
              nereus_audio_ring_readable(ring) < lowWater,
              nereus_audio_ring_writable(ring) >= AudioJitterBuffer.packetFrames {
            let samples: [Float]
            let rtpEnd: UInt32?
            switch jitter.pull() {
            case .audio(let pcm):
                samples = pcm
                rtpEnd = jitter.lastPullRelease?.rtpTimestamp
            case .concealed(let pcm):
                samples = pcm
                rtpEnd = nil
            case .silence:
                samples = [Float](repeating: 0, count: AudioJitterBuffer.packetFrames * Self.channels)
                rtpEnd = nil
            }
            let first = nereus_audio_ring_written_frames(ring)
            samples.withUnsafeBufferPointer { buffer in
                _ = nereus_audio_ring_write(ring, buffer.baseAddress, AudioJitterBuffer.packetFrames)
            }
            ringSegments.append(RingSegment(first: first, end: first + UInt64(AudioJitterBuffer.packetFrames),
                                            epoch: streamEpoch, rtpEnd: rtpEnd))
            pruneRingSegments()
        }
        publish()
    }

    /// Publishes the depth and target the render path steers by. While the
    /// buffer is not playing there is nothing to steer by, so the depth is
    /// published as ``notSteering`` and the ratio holds where it was.
    private func publish() {
        let target = jitter.targetMs + Self.nominalRingMs(lowWaterFrames: lowWaterFrames)
        nereus_shared_value_store(publishedTargetMs, Int64(target))
        nereus_shared_value_store(publishedDepthMs, jitter.isPlaying ? Int64(jitter.depthMs) : Self.notSteering)
    }

    private func drainRenderObservations(counterEpoch: UInt64? = nil) {
        let counterEpoch = counterEpoch ?? streamEpoch
        let dropped = nereus_render_observation_queue_dropped(renderObservations)
        let hadDrop = dropped != droppedRenderSeen
        if hadDrop {
            droppedRenderSeen = dropped
            latestRender = nil
            latestSchedule = nil
            renderEvidenceRecoveredAfterDrop = false
            knownStreamRenderUnderruns = nil
            streamCounterNeedsCurrentEvidence = true
        }
        let crossed = UInt64(nereus_shared_value_load(renderEpochCrossings))
        if crossed != renderCrossingsSeen {
            knownStreamRenderUnderruns = nil
            streamCounterNeedsCurrentEvidence = true
            latestSchedule = nil
            renderCrossingsSeen = crossed
        }
        var record = nereus_render_observation()
        while nereus_render_observation_queue_pop(renderObservations, &record) != 0 {
            if hadDrop { continue }
            guard record.stream_epoch == counterEpoch, record.output_epoch != 0 else {
                latestSchedule = nil
                continue
            }
            let currentOutput = UInt64(nereus_shared_value_load(publishedOutputEpoch))
            let isCurrent = currentOutput != 0 && record.stream_epoch == streamEpoch
                && record.output_epoch == currentOutput
            latestSchedule = isCurrent && record.has_timestamp != 0 ? record : nil
            if !streamCounterNeedsCurrentEvidence || isCurrent {
                knownStreamRenderUnderruns = record.stream_render_underruns
                if isCurrent { streamCounterNeedsCurrentEvidence = false }
            }
            if isCurrent {
                latestRender = record
                renderEvidenceRecoveredAfterDrop = true
            }
        }
        // A callback can finish during the drain and publish an invalidated
        // old-epoch record. Its crossing always gaps a final count.
        let crossedAfter = UInt64(nereus_shared_value_load(renderEpochCrossings))
        if crossedAfter != renderCrossingsSeen {
            knownStreamRenderUnderruns = nil
            streamCounterNeedsCurrentEvidence = true
            latestSchedule = nil
            renderCrossingsSeen = crossedAfter
        }
    }

    private func pruneRingSegments() {
        // The matcher can keep a full 256-frame read plus its interpolation
        // pair after those samples have left the ring. Retain that span.
        if let latestRender {
            let earliest = Double(latestRender.ring_read_frames) - latestRender.matcher_prefetched_frames - 258
            while ringSegments.count > 1, Double(ringSegments[0].end) < earliest {
                ringSegments.removeFirst()
            }
        }
        if ringSegments.count > 16 { ringSegments.removeFirst(ringSegments.count - 16) }
    }

    private func makeObservation() -> AudioPlaybackObservation {
        drainRenderObservations()
        let active = jitter.anchor != nil
        let output = UInt64(nereus_shared_value_load(publishedOutputEpoch))
        let record = active && output != 0 && latestRender?.output_epoch == output
            && latestRender?.stream_epoch == streamEpoch ? latestRender : nil
        let scheduleRecord = active && output != 0 && latestSchedule?.output_epoch == output
            && latestSchedule?.stream_epoch == streamEpoch ? latestSchedule : nil
        var counters = active ? jitter.observation : retiredCounters
        if !active || output == 0 {
            counters.reorderQueuedMs = nil
            counters.timelineDepthMs = nil
            counters.adaptiveTargetMs = nil
            counters.release = nil
        }
        let now = Int64(DispatchTime.now().uptimeNanoseconds)
        let age = active ? counters.lastAdmittedPacketNs.map { Double(max(0, now - $0)) / 1_000_000 } : nil
        let queued = record.map { Double($0.ring_queued_frames) + $0.matcher_prefetched_frames }
        var sourceSchedule: AudioSourceRenderScheduleObservation?
        if let record = scheduleRecord {
            let hostTicks = record.has_source_host_time != 0 ? record.source_host_ticks : nil
            sourceSchedule = AudioSourceRenderScheduleObservation(
                streamEpoch: streamEpoch, outputEpoch: output,
                measuredNs: Int64(record.measured_ns), readWindowNs: Int64(record.read_window_ns),
                sourceRateHz: Double(record.source_rate_hz), callbackSourceFrames: Int(record.callback_frames),
                chunkStartSourceFrame: Int(record.chunk_start_frame),
                chunkEndSourceFrame: Int(record.chunk_end_frame),
                rawTimestampFlags: record.timestamp_flags,
                sourceHostTicks: hostTicks,
                sourceHostTimeNs: hostTicks.flatMap {
                    Self.hostTicksToNanoseconds($0, numer: timebaseNumer, denom: timebaseDenom)
                },
                sourceSampleTime: record.has_source_sample_time != 0 ? record.source_sample_time : nil)
        }
        var sourcePoint: AudioSourcePlayoutObservation?
        if let record, record.source_frames_rendered > 0, record.starved == 0,
           record.ratio_measured != 0, record.ring_read_frames > 0 {
            let oldestUnrendered = Double(record.ring_read_frames) - record.matcher_prefetched_frames
            let newestHanded = record.ring_read_frames - 1
            let oldIsCurrent = ringSegments.contains {
                Double($0.first) <= oldestUnrendered && oldestUnrendered < Double($0.end)
                    && $0.epoch == streamEpoch && $0.rtpEnd != nil
            }
            if oldIsCurrent,
               let segment = ringSegments.first(where: { $0.first <= newestHanded && newestHanded < $0.end }),
               segment.epoch == streamEpoch, let end = segment.rtpEnd {
                let remaining = UInt32(segment.end - record.ring_read_frames)
                sourcePoint = AudioSourcePlayoutObservation(
                    rtpTimestamp: end &- remaining, measuredNs: Int64(record.measured_ns),
                    readWindowNs: Int64(record.read_window_ns),
                    callbackSourceFrames: Int(record.callback_frames),
                    matcherPrefetchedSourceFrames: record.matcher_prefetched_frames,
                    unreadRingSourceFrames: Int(record.ring_queued_frames),
                    ratio: record.matcher_ratio, outputEpoch: output)
            }
        }
        return AudioPlaybackObservation(
            lifetimeID: lifetimeID, streamEpoch: streamEpoch, stream: active ? jitter.anchor : nil,
            retiredStream: retiredStream,
            outputEpoch: active && output != 0 ? output : nil, counters: counters,
            lifetimeUnderflows: lifetimeBufferUnderflows + jitter.observation.underflowEvents
                + UInt64(nereus_shared_value_load(renderUnderrunCount)),
            lifetimeOverflows: lifetimeOverflows + jitter.observation.overflowEvents,
            renderUnderruns: UInt64(nereus_shared_value_load(renderUnderrunCount)),
            streamRenderUnderruns: active
                ? knownStreamRenderUnderruns
                : (UInt64(nereus_shared_value_load(renderEpochCrossings)) != renderCrossingsSeen
                    ? nil : retiredStreamRenderUnderruns),
            lastCallbackSourceFrames: record.map { Int($0.callback_frames) },
            requestedSourceFrames: UInt64(nereus_shared_value_load(requestedSourceFrames)),
            renderedSourceFrames: UInt64(nereus_shared_value_load(renderedSourceFrames)),
            droppedRenderObservations: droppedRenderSeen,
            renderEvidenceRecoveredAfterDrop: renderEvidenceRecoveredAfterDrop,
            lastAdmittedPacketAgeMs: output != 0 ? age : nil,
            reorderQueuedMs: active && output != 0 ? counters.reorderQueuedMs : nil,
            timelineDepthMs: active && output != 0 ? counters.timelineDepthMs : nil,
            adaptiveTargetMs: active && output != 0 ? counters.adaptiveTargetMs : nil,
            speakerQueuedSourceFrames: queued,
            measuredDriftPpm: record.flatMap { $0.ratio_measured != 0 ? ($0.matcher_ratio - 1) * 1_000_000 : nil },
            release: active && output != 0 ? counters.release : nil,
            sourcePlayout: sourcePoint,
            sourceRenderSchedule: sourceSchedule)
    }

    /// Convert mach ticks with checked arithmetic on the feed side. Splitting
    /// quotient and remainder avoids rejecting a result that fits after division.
    static func hostTicksToNanoseconds(_ ticks: UInt64, numer: UInt32, denom: UInt32) -> UInt64? {
        guard numer > 0, denom > 0 else { return nil }
        let divisor = UInt64(denom)
        let (whole, wholeOverflow) = (ticks / divisor).multipliedReportingOverflow(by: UInt64(numer))
        guard !wholeOverflow else { return nil }
        let fraction = ((ticks % divisor) * UInt64(numer)) / divisor
        let (result, sumOverflow) = whole.addingReportingOverflow(fraction)
        return sumOverflow ? nil : result
    }

    /// The published depth while the buffer is not playing.
    static let notSteering: Int64 = -1

    // MARK: The log

    /// One line of the counts: why it is written, the jitter buffer's
    /// counters, the dry renders, the output's piece size and the ring's
    /// low water mark.
    public static func summaryLine(_ reason: String, counters: Counters, renderUnderruns: Int,
                                   callbackFrames: Int, lowWaterFrames: Int) -> String {
        func milliseconds(_ frames: Int) -> String {
            String(format: "%.1f", Double(frames) * 1000 / sampleRate)
        }
        return "band playback (\(reason)): renderUnderruns=\(renderUnderruns) "
            + "bufferUnderruns=\(counters.underruns) concealed=\(counters.concealed) "
            + "lateDrops=\(counters.lateDrops) callbackFrames=\(callbackFrames) "
            + "callbackMs=\(milliseconds(callbackFrames)) lowWaterMs=\(milliseconds(lowWaterFrames))"
    }

    /// Logs the counts once a minute while a stream plays. Feed queue only.
    private func summarizeIfDue() {
        guard jitter.anchor != nil else {
            return
        }
        let now = Self.uptimeMs()
        guard now &- lastSummaryMs >= Self.summaryIntervalMs else {
            return
        }
        logSummary("each minute")
    }

    /// Feed queue only.
    private func logSummary(_ reason: String) {
        lastSummaryMs = Self.uptimeMs()
        let counters = Counters(underruns: jitter.underruns, lateDrops: jitter.lateDrops, concealed: jitter.concealed,
                                otherPayloadDrops: jitter.otherPayloadDrops)
        let line = Self.summaryLine(reason, counters: counters, renderUnderruns: renderUnderruns,
                                    callbackFrames: callbackFrames, lowWaterFrames: lowWaterFrames)
        Self.logger.notice("\(line, privacy: .public)")
    }

    private static func uptimeMs() -> UInt64 {
        DispatchTime.now().uptimeNanoseconds / 1_000_000
    }

    // MARK: The render side

    /// Writes `frames` interleaved stereo frames of playback. Never
    /// allocates, never locks; called on the audio render thread.
    func render(into output: UnsafeMutablePointer<Float>, frames: Int) {
        render(into: output, frames: frames, callbackFrames: frames,
               chunkStartFrame: 0, timestamp: nil)
    }

    private func render(into output: UnsafeMutablePointer<Float>, frames: Int, callbackFrames: Int,
                        chunkStartFrame: Int, timestamp: AudioTimeStamp?) {
        let boundary = beginRenderBoundary()
        let streamAtStart = boundary.stream
        if streamAtStart != renderObservedStreamEpoch {
            renderObservedStreamEpoch = streamAtStart
            renderStreamUnderrunCount = 0
        }
        let outputAtStart = boundary.output
        let bufferedMs = nereus_shared_value_load(publishedDepthMs)
        if bufferedMs != Self.notSteering {
            let ringMs = Double(nereus_audio_ring_readable(ring)) * 1000 / Self.sampleRate
            let targetMs = Double(nereus_shared_value_load(publishedTargetMs))
            resampler.steer(depthMs: Double(bufferedMs) + ringMs, targetMs: targetMs, frames: frames)
        }
        let ring = self.ring
        let made = resampler.render(into: output, frames: frames) { buffer, capacity in
            nereus_audio_ring_read(ring, buffer, capacity)
        }
        let totalRequested = nereus_shared_value_add(requestedSourceFrames, Int64(frames))
        let totalRendered = nereus_shared_value_add(renderedSourceFrames, Int64(made))
        if made < frames, bufferedMs != Self.notSteering {
            // Only this thread writes the count, so a load and a store do.
            nereus_shared_value_store(renderUnderrunCount, nereus_shared_value_load(renderUnderrunCount) + 1)
            if outputAtStart != 0 { renderStreamUnderrunCount += 1 }
        }
        let stable = streamAtStart == UInt64(nereus_shared_value_load(publishedStreamEpoch))
            && outputAtStart == UInt64(nereus_shared_value_load(publishedOutputEpoch))
        var observed = nereus_render_observation()
        observed.stream_epoch = stable ? streamAtStart : 0
        observed.output_epoch = stable ? outputAtStart : 0
        let readStartNs = DispatchTime.now().uptimeNanoseconds
        observed.ring_read_frames = nereus_audio_ring_read_frames(ring)
        observed.ring_queued_frames = Int64(nereus_audio_ring_readable(ring))
        observed.matcher_prefetched_frames = resampler.prefetchedInputFrames
        observed.matcher_ratio = resampler.ratio
        let readEndNs = DispatchTime.now().uptimeNanoseconds
        observed.measured_ns = readStartNs + (readEndNs - readStartNs) / 2
        observed.read_window_ns = readEndNs - readStartNs
        observed.callback_frames = UInt32(callbackFrames)
        observed.chunk_start_frame = UInt32(chunkStartFrame)
        observed.chunk_end_frame = UInt32(chunkStartFrame + frames)
        observed.source_rate_hz = UInt32(Self.sampleRate)
        if let timestamp {
            observed.timestamp_flags = timestamp.mFlags.rawValue
            if timestamp.mFlags.contains(.hostTimeValid) {
                observed.source_host_ticks = timestamp.mHostTime
                observed.has_source_host_time = 1
            }
            if timestamp.mFlags.contains(.sampleTimeValid), timestamp.mSampleTime.isFinite {
                observed.source_sample_time = timestamp.mSampleTime
                observed.has_source_sample_time = 1
            }
            observed.has_timestamp = observed.has_source_host_time | observed.has_source_sample_time
        }
        observed.source_frames_requested = UInt64(totalRequested)
        observed.source_frames_rendered = UInt64(totalRendered)
        observed.stream_render_underruns = renderStreamUnderrunCount
        observed.ratio_measured = bufferedMs != Self.notSteering && stable ? 1 : 0
        observed.starved = made < frames ? 1 : 0
        _ = nereus_render_observation_queue_push(renderObservations, &observed)
        _ = endRenderBoundary(boundary)
    }

    /// Renders into an audio buffer list of one interleaved buffer or one
    /// buffer per channel, in pieces through the preallocated scratch.
    func render(frames: Int, into bufferList: UnsafeMutablePointer<AudioBufferList>,
                timestamp: UnsafePointer<AudioTimeStamp>? = nil) {
        let buffers = UnsafeMutableAudioBufferListPointer(bufferList)
        let callbackTimestamp = timestamp?.pointee
        nereus_shared_value_store(lastCallbackFrames, Int64(frames))
        var done = 0
        while done < frames {
            let count = min(frames - done, Self.renderChunkFrames)
            render(into: scratch, frames: count, callbackFrames: frames,
                   chunkStartFrame: done, timestamp: callbackTimestamp)
            if buffers.count == 1 {
                if let data = buffers[0].mData?.assumingMemoryBound(to: Float.self) {
                    (data + done * Self.channels).update(from: scratch, count: count * Self.channels)
                }
            } else {
                for channel in 0..<min(buffers.count, Self.channels) {
                    guard let data = buffers[channel].mData?.assumingMemoryBound(to: Float.self) else {
                        continue
                    }
                    for frame in 0..<count {
                        data[done + frame] = scratch[frame * Self.channels + channel]
                    }
                }
            }
            done += count
        }
    }

    /// A source node that plays this core, 48 kHz stereo. The node holds
    /// the core; attach it to an engine to hear it.
    public func makeSourceNode() -> AVAudioSourceNode? {
        guard let format = AVAudioFormat(standardFormatWithSampleRate: Self.sampleRate,
                                         channels: AVAudioChannelCount(Self.channels)) else {
            return nil
        }
        return AVAudioSourceNode(format: format) { [self] _, timestamp, frameCount, bufferList in
            render(frames: Int(frameCount), into: bufferList, timestamp: timestamp)
            return noErr
        }
    }
}
