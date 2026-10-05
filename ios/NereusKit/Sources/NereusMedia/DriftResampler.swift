// NereusSDR for iOS: stretches or squeezes playback by up to 0.1 % so the Core's clock and the phone's stay in step
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Matches the Core's sample clock to the phone's output clock. The two
/// both run near 48 kHz but never exactly, so without correction the
/// jitter buffer slowly fills or empties. This holds a ratio of output
/// frames made per input frame within 1 ± 0.001 and steers it from how far
/// the buffered audio is from its target: more audio than the target
/// consumes a little faster (ratio below 1), less consumes a little slower.
///
/// Each output frame is a straight-line interpolation between the two
/// input frames around its position, so an output step is never larger
/// than the largest input step it spans. NereusSDR-original.
///
/// ``render(into:frames:source:)`` never allocates and never locks, so the
/// audio render thread calls it directly. Not thread-safe: one thread uses it.
public final class DriftResampler {
    /// The furthest the ratio moves from 1.
    public static let maximumDeviation = 0.001
    /// The ratio's move per millisecond of smoothed depth error: 100 ms
    /// away from the target is the full deviation.
    public static let deviationPerMs = 0.00001
    /// The smoothing of the depth error, in seconds of output.
    public static let steeringTimeConstant = 2.0
    public static let sampleRate = 48000.0

    public let channels: Int
    /// Output frames made per input frame.
    public private(set) var ratio = 1.0
    /// The depth error the ratio follows, in ms, after smoothing.
    public private(set) var smoothedErrorMs = 0.0

    /// Input frames advanced per output frame: 1 / ratio.
    private var step = 1.0
    /// Where the next output frame sits between `left` and `right`, 0 ..< 1.
    private var position = 0.0
    private let left: UnsafeMutablePointer<Float>
    private let right: UnsafeMutablePointer<Float>
    private var hasLeft = false
    private var hasRight = false
    private let input: UnsafeMutablePointer<Float>
    private let inputCapacity: Int
    private var inputCount = 0
    private var inputIndex = 0

    /// Source frames still represented by the prefetch, including the
    /// fractional distance through the interpolation pair. Render thread only.
    public var prefetchedInputFrames: Double {
        let queued = Double(inputCount - inputIndex)
        if hasRight { return queued + max(0, 2 - position) }
        return queued + (hasLeft ? 1 : 0)
    }

    /// `inputChunkFrames` is how many frames one call to the source may give.
    public init(channels: Int = 2, inputChunkFrames: Int = 256) {
        precondition(channels > 0 && inputChunkFrames > 0)
        self.channels = channels
        inputCapacity = inputChunkFrames
        left = .allocate(capacity: channels)
        right = .allocate(capacity: channels)
        input = .allocate(capacity: inputChunkFrames * channels)
        left.initialize(repeating: 0, count: channels)
        right.initialize(repeating: 0, count: channels)
        input.initialize(repeating: 0, count: inputChunkFrames * channels)
    }

    deinit {
        left.deallocate()
        right.deallocate()
        input.deallocate()
    }

    /// Holds the ratio at `ratio`, clamped to 1 ± ``maximumDeviation``. A
    /// value that is not finite is ignored.
    public func setRatio(_ ratio: Double) {
        guard ratio.isFinite else {
            return
        }
        self.ratio = min(max(ratio, 1 - Self.maximumDeviation), 1 + Self.maximumDeviation)
        step = 1 / self.ratio
    }

    /// Steers the ratio from the buffered depth and its target, both in ms,
    /// after `frames` output frames. The error is smoothed over
    /// ``steeringTimeConstant`` so the buffer's 40 ms steps do not reach
    /// the ratio as steps.
    public func steer(depthMs: Double, targetMs: Double, frames: Int) {
        guard depthMs.isFinite, targetMs.isFinite, frames > 0 else {
            return
        }
        let weight = min(1, Double(frames) / (Self.steeringTimeConstant * Self.sampleRate))
        smoothedErrorMs += (depthMs - targetMs - smoothedErrorMs) * weight
        setRatio(1 - smoothedErrorMs * Self.deviationPerMs)
    }

    /// Writes `frames` interleaved output frames to `output`, pulling input
    /// through `source`, which fills the buffer it is given with up to the
    /// count it is given of interleaved frames and returns how many it
    /// wrote. When the source runs dry the rest of the output is silence
    /// and the position is kept for the next call. Returns the frames made
    /// from input.
    @discardableResult
    public func render(into output: UnsafeMutablePointer<Float>, frames: Int,
                       source: (UnsafeMutablePointer<Float>, Int) -> Int) -> Int {
        var produced = 0
        outer: while produced < frames {
            if !hasLeft {
                guard nextFrame(into: left, source: source) else {
                    break
                }
                hasLeft = true
            }
            while !hasRight || position >= 1 {
                if hasRight {
                    left.update(from: right, count: channels)
                    hasRight = false
                    position -= 1
                }
                guard nextFrame(into: right, source: source) else {
                    break outer
                }
                hasRight = true
            }
            let fraction = Float(position)
            let base = produced * channels
            for channel in 0..<channels {
                output[base + channel] = left[channel] + fraction * (right[channel] - left[channel])
            }
            produced += 1
            position += step
        }
        if produced < frames {
            (output + produced * channels).update(repeating: 0, count: (frames - produced) * channels)
        }
        return produced
    }

    /// Copies the next input frame into `frame`, refilling from `source`
    /// when the chunk is used up; false when the source has nothing.
    private func nextFrame(into frame: UnsafeMutablePointer<Float>,
                           source: (UnsafeMutablePointer<Float>, Int) -> Int) -> Bool {
        if inputIndex >= inputCount {
            inputCount = max(0, min(source(input, inputCapacity), inputCapacity))
            inputIndex = 0
            guard inputCount > 0 else {
                return false
            }
        }
        frame.update(from: input + inputIndex * channels, count: channels)
        inputIndex += 1
        return true
    }
}
