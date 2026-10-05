// NereusSDR for iOS: bounded clock exchange and measured playback delay arithmetic
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

private enum ClockMath {
    static let nsPerSecond: Int64 = 1_000_000_000
    static let nsPerMillisecond = 1_000_000.0
    static let streamRate: Int64 = 48_000

    static func add(_ lhs: Int64, _ rhs: Int64) -> Int64? {
        let (value, overflow) = lhs.addingReportingOverflow(rhs)
        return overflow ? nil : value
    }

    static func subtract(_ lhs: Int64, _ rhs: Int64) -> Int64? {
        let (value, overflow) = lhs.subtractingReportingOverflow(rhs)
        return overflow ? nil : value
    }

    static func multiply(_ lhs: Int64, _ rhs: Int64) -> Int64? {
        let (value, overflow) = lhs.multipliedReportingOverflow(by: rhs)
        return overflow ? nil : value
    }

    static func roundedInt(_ value: Double) -> Int64? {
        guard value.isFinite, value >= Double(Int64.min), value < Double(Int64.max) else { return nil }
        return Int64(value.rounded())
    }

    static func gcd(_ lhs: Int64, _ rhs: Int64) -> Int64 {
        var a = lhs
        var b = rhs
        while b != 0 { (a, b) = (b, a % b) }
        return a
    }
}

/// Four nonnegative steady-clock readings in ns. t0/t3 are local; t1/t2 are Core.
public struct AudioClockSample: Sendable, Equatable {
    public let t0Ns: Int64
    public let t1Ns: Int64
    public let t2Ns: Int64
    public let t3Ns: Int64

    public init(t0Ns: Int64, t1Ns: Int64, t2Ns: Int64, t3Ns: Int64) {
        self.t0Ns = t0Ns; self.t1Ns = t1Ns; self.t2Ns = t2Ns; self.t3Ns = t3Ns
    }
}

/// Core minus local offset in ns, with its effective network RTT and exchange age.
public struct AudioClockOffset: Sendable, Equatable {
    public let offsetNs: Int64
    public let roundTripNs: Int64
    public let sampleNs: Int64
    public let exchangeNs: Int64

    public init(offsetNs: Int64, roundTripNs: Int64, sampleNs: Int64, exchangeNs: Int64) {
        self.offsetNs = offsetNs; self.roundTripNs = roundTripNs
        self.sampleNs = sampleNs; self.exchangeNs = exchangeNs
    }

    /// Half the network RTT plus 100 ppm over age and the entire exchange.
    public func boundNs(atLocalNs localNs: Int64) -> Double? {
        guard roundTripNs >= 0, sampleNs >= 0, exchangeNs >= roundTripNs,
              exchangeNs <= sampleNs, localNs >= 0 else { return nil }
        let age = localNs >= sampleNs ? localNs - sampleNs : sampleNs - localNs
        let bound = Double(roundTripNs) / 2 + (Double(age) + Double(exchangeNs)) * 0.0001
        return bound.isFinite ? bound : nil
    }
}

/// Pure value state; callers supply all clock readings and query times.
public struct AudioClockEstimator: Sendable {
    public static let windowNs: Int64 = 16_000_000_000
    public static let echoStaleNs: Int64 = 3_000_000_000
    /// Defensive cap against untrusted probe frequency. All 17 samples of normal 1 Hz
    /// traffic in the inclusive 16 s window fit. When full, the worst RTT is discarded,
    /// oldest first on ties; the best eligible RTT and newest echo remain.
    public static let maximumSamples = 64

    private var samples: [AudioClockOffset] = []
    public init() {}
    public var sampleCount: Int { samples.count }

    @discardableResult public mutating func add(_ sample: AudioClockSample) -> Bool {
        guard sample.t0Ns >= 0, sample.t1Ns >= 0, sample.t2Ns >= 0, sample.t3Ns >= 0,
              let exchange = ClockMath.subtract(sample.t3Ns, sample.t0Ns), exchange >= 0,
              let held = ClockMath.subtract(sample.t2Ns, sample.t1Ns), held >= 0,
              let rtt = ClockMath.subtract(exchange, held), rtt >= 0,
              let forward = ClockMath.subtract(sample.t1Ns, sample.t0Ns),
              let returnSide = ClockMath.subtract(sample.t2Ns, sample.t3Ns),
              let offsetSum = ClockMath.add(forward, returnSide),
              sample.t3Ns >= (samples.last?.sampleNs ?? 0) else { return false }
        samples.append(AudioClockOffset(offsetNs: offsetSum / 2, roundTripNs: rtt,
                                        sampleNs: sample.t3Ns, exchangeNs: exchange))
        let oldest = sample.t3Ns >= Self.windowNs ? sample.t3Ns - Self.windowNs : 0
        samples.removeAll { $0.sampleNs < oldest }
        if samples.count > Self.maximumSamples {
            var discard = 0
            for index in 1..<(samples.count - 1) {
                if samples[index].roundTripNs > samples[discard].roundTripNs { discard = index }
            }
            samples.remove(at: discard)
        }
        return true
    }

    public mutating func reset() { samples.removeAll() }

    public func offset(atLocalNs nowNs: Int64) -> AudioClockOffset? {
        guard nowNs >= 0, let latest = samples.last?.sampleNs, nowNs >= latest,
              nowNs - latest <= Self.echoStaleNs else { return nil }
        let oldest = nowNs >= Self.windowNs ? nowNs - Self.windowNs : 0
        var best: AudioClockOffset?
        for sample in samples where sample.sampleNs >= oldest {
            if best == nil || sample.roundTripNs <= best!.roundTripNs { best = sample }
        }
        return best
    }
}

/// Core's captured end-of-block RTP timestamp and steady-clock reading.
public struct AudioCaptureAnchor: Sendable, Equatable {
    public let generation: UInt32
    public let rtpTimestamp: UInt32
    public let capturedNs: Int64
    public init(generation: UInt32, rtpTimestamp: UInt32, capturedNs: Int64) {
        self.generation = generation; self.rtpTimestamp = rtpTimestamp; self.capturedNs = capturedNs
    }
}

/// A real phone playout reading. Frame fields use the device rate except the
/// pipeline and codec delays, which use the 48 kHz stream rate. The callback
/// contributes half a quantum to time and half to uncertainty. Device latency
/// is counted only when supplied by the future playback adapter.
public struct AudioPlayoutPoint: Sendable, Equatable {
    public let rtpTimestamp: UInt32
    public let measuredNs: Int64
    public let matcherFillFrames: Int
    public let speakerQueuedFrames: Int
    public let deviceRateHz: Int
    public let pipelineDelayFrames: Int
    public let codecDelayFrames: Int
    public let callbackFrames: Int
    public let readWindowNs: Int64
    public let matcherRatio: Double
    public let deviceLatencyNs: Int64?

    public init(rtpTimestamp: UInt32, measuredNs: Int64, matcherFillFrames: Int,
                speakerQueuedFrames: Int, deviceRateHz: Int, pipelineDelayFrames: Int,
                codecDelayFrames: Int, callbackFrames: Int, readWindowNs: Int64,
                matcherRatio: Double, deviceLatencyNs: Int64?) {
        self.rtpTimestamp = rtpTimestamp; self.measuredNs = measuredNs
        self.matcherFillFrames = matcherFillFrames; self.speakerQueuedFrames = speakerQueuedFrames
        self.deviceRateHz = deviceRateHz; self.pipelineDelayFrames = pipelineDelayFrames
        self.codecDelayFrames = codecDelayFrames; self.callbackFrames = callbackFrames
        self.readWindowNs = readWindowNs; self.matcherRatio = matcherRatio
        self.deviceLatencyNs = deviceLatencyNs
    }

    private var timing: (playout: Int64, accuracy: Int64, stretch: Int64)? {
        guard measuredNs >= 0, matcherFillFrames >= 0, speakerQueuedFrames >= 0,
              deviceRateHz > 0, pipelineDelayFrames >= 0, codecDelayFrames >= 0,
              codecDelayFrames <= pipelineDelayFrames, callbackFrames >= 0,
              readWindowNs >= 0, deviceLatencyNs.map({ $0 >= 0 }) ?? true,
              matcherRatio.isFinite, matcherRatio > 0 else { return nil }
        let rate = Int64(deviceRateHz)
        let common = ClockMath.gcd(rate, ClockMath.streamRate)
        let deviceWeight = ClockMath.streamRate / common
        let streamWeight = rate / common
        guard let queued = ClockMath.add(Int64(matcherFillFrames), Int64(speakerQueuedFrames)),
              let doubled = ClockMath.multiply(queued, 2),
              let deviceHalfFrames = ClockMath.add(doubled, Int64(callbackFrames)),
              let deviceTerm = ClockMath.multiply(deviceHalfFrames, deviceWeight),
              let pipelineDouble = ClockMath.multiply(Int64(pipelineDelayFrames), 2),
              let streamTerm = ClockMath.multiply(pipelineDouble, streamWeight),
              let weighted = ClockMath.add(deviceTerm, streamTerm),
              let numerator = ClockMath.multiply(weighted, ClockMath.nsPerSecond),
              let twiceRate = ClockMath.multiply(rate, 2),
              let denominator = ClockMath.multiply(twiceRate, deviceWeight),
              let play = ClockMath.add(measuredNs, numerator / denominator),
              let playout = ClockMath.add(play, deviceLatencyNs ?? 0),
              let callbackNumerator = ClockMath.multiply(Int64(callbackFrames), ClockMath.nsPerSecond),
              let halfRate = ClockMath.multiply(rate, 2),
              let callbackCeil = ClockMath.add(callbackNumerator, halfRate - 1),
              let windowCeil = ClockMath.add(readWindowNs, 1),
              let accuracy = ClockMath.add(callbackCeil / halfRate, windowCeil / 2),
              let codecNumerator = ClockMath.multiply(Int64(codecDelayFrames), ClockMath.nsPerSecond),
              let ahead = ClockMath.subtract(playout, measuredNs),
              let made = ClockMath.subtract(ahead, codecNumerator / ClockMath.streamRate) else { return nil }
        let factor = 1 - 1 / matcherRatio
        guard factor.isFinite,
              let stretch = ClockMath.roundedInt(Double(max(0, made)) * factor) else { return nil }
        return (playout, accuracy, stretch)
    }

    public var playoutNs: Int64? { timing?.playout }
    public var accuracyNs: Int64? { timing?.accuracy }
    public var matcherStretchNs: Int64? { timing?.stretch }
}

/// Reorder release point before speaker buffering, in local steady-clock ns.
public struct AudioReleasePoint: Sendable, Equatable {
    public let rtpTimestamp: UInt32
    public let releasedNs: Int64
    public init(rtpTimestamp: UInt32, releasedNs: Int64) {
        self.rtpTimestamp = rtpTimestamp; self.releasedNs = releasedNs
    }
}

public struct AudioDelayInputs: Sendable, Equatable {
    public let offset: AudioClockOffset?
    public let capture: AudioCaptureAnchor?
    public let playingGeneration: UInt32
    public let playout: AudioPlayoutPoint?
    public let release: AudioReleasePoint?
    public init(offset: AudioClockOffset?, capture: AudioCaptureAnchor?, playingGeneration: UInt32,
                playout: AudioPlayoutPoint?, release: AudioReleasePoint?) {
        self.offset = offset; self.capture = capture; self.playingGeneration = playingGeneration
        self.playout = playout; self.release = release
    }
}

/// True playout and delivery lie within each value plus or minus its own bound.
public struct AudioDelayEstimate: Sendable, Equatable {
    public let delayMs: Double
    public let boundMs: Double
    public let includesDevice: Bool
    public let deliveryMs: Double?
    public let deliveryBoundMs: Double?
}

/// Capture to reorder-release delay, independent of output playout timing.
public struct AudioDeliveryEstimate: Sendable, Equatable {
    public let delayMs: Double
    public let boundMs: Double
}

private func capturedAudioTimestamp(_ timestamp: UInt32, at localNs: Int64,
                                    offset: AudioClockOffset, capture: AudioCaptureAnchor,
                                    anchorLocal: Int64) -> (since: Double, bound: Double)? {
    guard localNs >= 0 else { return nil }
    let frames = Int64(Int32(bitPattern: timestamp &- capture.rtpTimestamp))
    guard (-2_880_000...2_880_000).contains(frames) else { return nil }
    let span = Double(frames) * Double(ClockMath.nsPerSecond) / Double(ClockMath.streamRate)
    guard let rounded = ClockMath.roundedInt(span),
          let spanLocal = ClockMath.add(anchorLocal, rounded), spanLocal >= 0,
          let elapsed = ClockMath.subtract(localNs, anchorLocal),
          let clockBound = offset.boundNs(atLocalNs: spanLocal) else { return nil }
    let bound = clockBound + abs(span) * 0.0001
    let since = Double(elapsed) - span
    guard bound.isFinite, since.isFinite else { return nil }
    return (since, bound)
}

public func measureAudioDelivery(offset: AudioClockOffset?, capture: AudioCaptureAnchor?,
                                 playingGeneration: UInt32, release: AudioReleasePoint?) -> AudioDeliveryEstimate? {
    guard let offset, let capture, let release, capture.generation != 0,
          capture.generation == playingGeneration, capture.capturedNs >= 0,
          let anchorLocal = ClockMath.subtract(capture.capturedNs, offset.offsetNs),
          anchorLocal >= 0,
          let measured = capturedAudioTimestamp(release.rtpTimestamp, at: release.releasedNs,
                                                offset: offset, capture: capture,
                                                anchorLocal: anchorLocal) else { return nil }
    return AudioDeliveryEstimate(delayMs: measured.since / ClockMath.nsPerMillisecond,
                                 boundMs: measured.bound / ClockMath.nsPerMillisecond)
}

/// Capture-to-playout delay from actual readings. No clock, anchor or playout
/// reading means no result; an invalid release only suppresses delivery.
public func measureAudioDelay(_ inputs: AudioDelayInputs) -> AudioDelayEstimate? {
    guard let offset = inputs.offset, let capture = inputs.capture, let point = inputs.playout,
          capture.generation != 0, capture.generation == inputs.playingGeneration,
          capture.capturedNs >= 0, let playout = point.playoutNs,
          let accuracy = point.accuracyNs, let stretch = point.matcherStretchNs,
          let anchorLocal = ClockMath.subtract(capture.capturedNs, offset.offsetNs),
          anchorLocal >= 0 else { return nil }

    guard let heard = capturedAudioTimestamp(point.rtpTimestamp, at: playout,
                                             offset: offset, capture: capture,
                                             anchorLocal: anchorLocal) else { return nil }
    let delayMs = (heard.since - Double(stretch)) / ClockMath.nsPerMillisecond
    let boundMs = (heard.bound + Double(accuracy)) / ClockMath.nsPerMillisecond
    guard delayMs.isFinite, boundMs.isFinite else { return nil }
    let delivery = measureAudioDelivery(offset: offset, capture: capture,
                                        playingGeneration: inputs.playingGeneration,
                                        release: inputs.release)
    return AudioDelayEstimate(delayMs: delayMs, boundMs: boundMs,
                              includesDevice: point.deviceLatencyNs != nil,
                              deliveryMs: delivery?.delayMs, deliveryBoundMs: delivery?.boundMs)
}

public struct AudioDelayDisplay: Sendable, Equatable {
    public let valueMs: Int64
    public let accuracyMs: Int64
    public init(valueMs: Int64, accuracyMs: Int64) {
        self.valueMs = valueMs; self.accuracyMs = accuracyMs
    }
}

/// Nearest whole-ms value with a ceiling bound widened by the movement of
/// that value, so the displayed interval always contains the source interval.
public func roundAudioDelay(valueMs: Double, boundMs: Double) -> AudioDelayDisplay? {
    guard valueMs.isFinite, boundMs.isFinite, boundMs >= 0,
          let rounded = ClockMath.roundedInt(valueMs) else { return nil }
    let movement = abs(valueMs - Double(rounded))
    let widened = boundMs + movement
    guard widened.isFinite else { return nil }
    // TwoSum's residual records the part lost by the floating-point addition.
    // If the rounded sum lands exactly on an integer from below, ceil alone
    // would narrow the original interval by that lost positive fraction.
    let virtualMovement = widened - boundMs
    let residual = (boundMs - (widened - virtualMovement)) + (movement - virtualMovement)
    guard let ceiling = ClockMath.roundedInt(ceil(widened)) else { return nil }
    let accuracy: Int64
    if widened == Double(ceiling) && residual > 0 {
        guard let outward = ClockMath.add(ceiling, 1) else { return nil }
        accuracy = outward
    } else {
        accuracy = ceiling
    }
    return AudioDelayDisplay(valueMs: rounded, accuracyMs: max(1, accuracy))
}
