// NereusSDR for iOS: the S-meter's two peaks: Signal Peak's held needle and the peak hold line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The S-meter's peaks (D86), kept as the desktop's meter keeps them, on a
/// clock the caller supplies in seconds:
///
/// - **The held peak** Signal Peak's needle shows: a new high takes it at
///   once; from there it falls 0.5 dB every 50 ms until it meets the
///   reading, and every 10 seconds it drops to the reading.
/// - **The peak hold line**, when Peak Hold is on: a new high takes it,
///   it holds for a second, then falls at the chosen decay until it meets
///   the reading. Reset puts it at the reading.
///
/// With no reading both clear, and the next reading starts afresh.
public struct SMeterPeaks: Equatable, Sendable {
    /// The held peak's fall: 0.5 dB each 50 ms tick.
    public static let peakTickSeconds = 0.05
    public static let peakTickDb = 0.5
    /// The held peak drops to the reading this often.
    public static let peakResetSeconds = 10.0
    /// How long the peak hold line holds before it falls.
    public static let holdSeconds = 1.0

    /// The reading; nil with none.
    public private(set) var level: Double?
    /// The held peak (Signal Peak's needle).
    public private(set) var peak: Double?
    /// The peak hold line's decay, and whether it is on.
    public private(set) var holdOn: Bool
    public private(set) var decayDbPerSecond: Double

    private var peakFalling = false
    private var nextPeakTick = 0.0
    private var nextPeakReset: Double?
    private var holdTop: Double?
    private var holdSince: Double?

    public init(holdOn: Bool = true, decay: SMeterPeakDecay = .medium) {
        self.holdOn = holdOn
        decayDbPerSecond = decay.dbPerSecond
    }

    /// A new reading at time `now`; nil for no reading.
    public mutating func update(level newLevel: Double?, at now: Double) {
        advance(to: now)
        guard let newLevel else {
            level = nil
            peak = nil
            peakFalling = false
            holdTop = nil
            holdSince = nil
            return
        }
        level = newLevel
        if nextPeakReset == nil {
            nextPeakReset = now + Self.peakResetSeconds
        }
        if newLevel > (peak ?? -.infinity) {
            peak = newLevel
            peakFalling = true
            nextPeakTick = now + Self.peakTickSeconds
        }
        if holdOn {
            if holdTop == nil || newLevel > hold(at: now) ?? -.infinity {
                holdTop = newLevel
                holdSince = now
            }
        }
    }

    /// Runs the held peak's clock to `now`: its 50 ms falls and its 10 s drops.
    public mutating func advance(to now: Double) {
        guard let level else {
            return
        }
        while true {
            let tick = peakFalling ? nextPeakTick : .infinity
            let reset = nextPeakReset ?? .infinity
            let next = min(tick, reset)
            // A hair of slack so a tick due at `now` counts despite the sums' rounding.
            guard next <= now + 1e-9 else {
                break
            }
            if reset <= tick {
                peak = level
                nextPeakReset = reset + Self.peakResetSeconds
            } else {
                let fallen = (peak ?? level) - Self.peakTickDb
                if fallen < level {
                    peak = level
                    peakFalling = false
                } else {
                    peak = fallen
                    nextPeakTick = tick + Self.peakTickSeconds
                }
            }
        }
    }

    /// The peak hold line at `now`: nil while it is off or there is no reading.
    public func hold(at now: Double) -> Double? {
        guard holdOn, let level, let top = holdTop else {
            return nil
        }
        // Put at the reading by Reset or Enabled, the line stays there
        // until a reading passes it.
        guard let since = holdSince else {
            return top
        }
        let elapsed = now - since
        guard elapsed > Self.holdSeconds else {
            return max(top, level)
        }
        let fallen = top - decayDbPerSecond * (elapsed - Self.holdSeconds)
        return fallen <= level ? level : fallen
    }

    /// Peak Hold's Enabled: on or off, the line starting afresh at the reading.
    public mutating func setHold(_ on: Bool) {
        holdOn = on
        resetHold()
    }

    /// Decay's rate, from the next fall on.
    public mutating func setDecay(_ decay: SMeterPeakDecay) {
        decayDbPerSecond = decay.dbPerSecond
    }

    /// Peak Hold's Reset: the line drops to the reading.
    public mutating func resetHold() {
        holdTop = holdOn ? level : nil
        holdSince = nil
    }
}
