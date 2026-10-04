// NereusSDR for iOS: the phone's own check that the network carries lossless audio, as the desktop checks it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// R-IOS-09: this phone's check that the network carries the Core's
/// lossless audio, the desktop's `RemoteAudioLinkTrial` (R-R3-23) for the
/// one stream the phone plays. Only the phone can see what arrives here.
/// Lossless is about 1.6 Mbit/s at 250 packets a second where Opus is 24
/// to 48 kbit/s at 25; a link that cannot carry it loses packets (each one
/// 4 ms of silence) and stalls. Opus, whose concealment hides isolated
/// loss and whose rate fits almost any link, is the better sound there.
///
/// The trial begins when lossless playback begins. Its first window closes
/// after ``windowMs``; later windows follow back to back while lossless
/// plays. Loss in a window is the larger of missing packets over expected
/// packets and filled gaps over played packets.
public struct LosslessLinkTrial: Sendable {
    /// About 5 s: some 1250 lossless packets, enough that 2 % (25 packets)
    /// is a real pattern and not one unlucky burst, yet short enough that
    /// an unusable link falls back before the operator gives up on it.
    public static let windowMs: Int64 = 5_000
    /// The first window fails above 2 % loss: 25 gaps of 4 ms in 5 s.
    public static let trialLossLimit = 0.02
    /// After that, only sustained loss fails: every one of three windows in
    /// a row (15 s) above 1 %.
    public static let sustainedLossLimit = 0.01
    public static let sustainedWindows = 3
    /// Interruptions of the stream (an underrun, a stream gap): any one
    /// during the first window fails it; later, the second within
    /// ``restartSpanMs``.
    public static let trialRestartLimit = 1
    public static let sustainedRestartLimit = 2
    public static let restartSpanMs: Int64 = 60_000
    /// How often the media client samples playback: each window closes
    /// within a second of its end.
    public static let sampleMs: Int64 = 1_000

    public enum Verdict: Sendable, Equatable {
        case `continue`
        case failed
    }

    /// One reading of the playing receiver. Counters are per receiver
    /// generation; a new generation starts from zero.
    public struct Sample: Sendable, Equatable {
        public var running: Bool
        public var generation: UInt64
        public var expectedPackets: UInt64
        public var missingPackets: UInt64
        public var decodedPackets: UInt64
        public var concealedPackets: UInt64
        public var linkInterruptions: UInt64

        public init(running: Bool, generation: UInt64, expectedPackets: UInt64, missingPackets: UInt64,
                    decodedPackets: UInt64, concealedPackets: UInt64, linkInterruptions: UInt64) {
            self.running = running
            self.generation = generation
            self.expectedPackets = expectedPackets
            self.missingPackets = missingPackets
            self.decodedPackets = decodedPackets
            self.concealedPackets = concealedPackets
            self.linkInterruptions = linkInterruptions
        }
    }

    private struct Counters {
        var expected: UInt64 = 0
        var missing: UInt64 = 0
        var concealed: UInt64 = 0
        var played: UInt64 = 0
        var interruptions: UInt64 = 0
    }

    public private(set) var active = false
    /// The loss of the last closed window; nil before one.
    public private(set) var lastWindowLoss: Double?
    /// The last failed verdict came from interruptions, not loss.
    public private(set) var failedOnInterruptions = false
    private var windowStartMs: Int64 = 0
    private var window = Counters()
    /// Windows closed with a verdict since ``begin(nowMs:)``.
    public private(set) var closedWindows = 0
    private var badWindows = 0
    private var interruptions: [Int64] = []
    /// The sampled stream's generation and the counters last read from it;
    /// nil while no stream is sampled.
    private var stream: (generation: UInt64, base: Counters)?

    public init() {}

    public mutating func begin(nowMs: Int64) {
        self = LosslessLinkTrial()
        active = true
        windowStartMs = nowMs
    }

    public mutating func end() {
        self = LosslessLinkTrial()
    }

    /// A playback sample; nil when no lossless stream plays, which forgets
    /// the stream, so a new one counts from zero.
    public mutating func observe(nowMs: Int64, _ sample: Sample?) -> Verdict {
        guard active else {
            return .continue
        }
        var interrupted = false
        if let sample {
            if sample.running {
                var base = stream?.base ?? Counters()
                if stream?.generation != sample.generation {
                    // A fresh receiver counts from zero.
                    base = Counters()
                }
                let now = Counters(expected: sample.expectedPackets, missing: sample.missingPackets,
                                   concealed: sample.concealedPackets,
                                   played: sample.decodedPackets + sample.concealedPackets,
                                   interruptions: sample.linkInterruptions)
                func grow(_ value: UInt64, _ base: UInt64) -> UInt64 {
                    value > base ? value - base : 0
                }
                window.expected += grow(now.expected, base.expected)
                window.missing += grow(now.missing, base.missing)
                window.concealed += grow(now.concealed, base.concealed)
                window.played += grow(now.played, base.played)
                // A burst, gap or stall ridden through since the last
                // sample counts as the restart it used to cause.
                interrupted = grow(now.interruptions, base.interruptions) > 0
                stream = (sample.generation, now)
            }
        } else {
            stream = nil
        }
        if interrupted && noteInterruption(nowMs: nowMs) == .failed {
            return .failed
        }
        guard nowMs - windowStartMs >= Self.windowMs else {
            return .continue
        }
        let closed = window
        window = Counters()
        windowStartMs = nowMs
        guard closed.expected > 0 || closed.played > 0 else {
            // Nothing played this window (between contexts): no verdict on it.
            return .continue
        }
        let missing = closed.expected > 0 ? Double(closed.missing) / Double(closed.expected) : 0
        let concealed = closed.played > 0 ? Double(closed.concealed) / Double(closed.played) : 0
        let loss = max(missing, concealed)
        lastWindowLoss = loss
        let first = closedWindows == 0
        closedWindows += 1
        if first {
            return loss > Self.trialLossLimit ? .failed : .continue
        }
        badWindows = loss > Self.sustainedLossLimit ? badWindows + 1 : 0
        return badWindows >= Self.sustainedWindows ? .failed : .continue
    }

    /// One interruption of the stream.
    public mutating func noteInterruption(nowMs: Int64) -> Verdict {
        guard active else {
            return .continue
        }
        while let oldest = interruptions.first, nowMs - oldest >= Self.restartSpanMs {
            interruptions.removeFirst()
        }
        interruptions.append(nowMs)
        let limit = closedWindows == 0 ? Self.trialRestartLimit : Self.sustainedRestartLimit
        failedOnInterruptions = interruptions.count >= limit
        return failedOnInterruptions ? .failed : .continue
    }
}
