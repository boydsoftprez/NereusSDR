// NereusSDR for iOS: the grid's minimum following the noise floor, by the Core's V12 rule
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Adjust grid min to track noise floor (`display-v12-for-phone.md`, the
/// 2026-09-28 update, item 2, and "Grid noise-floor tracking"): every
/// ``intervalSeconds``, not while the pan transmits, the grid minimum
/// follows a floor plus the NF offset, moving only when that is
/// ``stepDb`` or more from the minimum now, the maximum moving with it
/// under Maintain grid range. The floor is the pan's display noise floor
/// (the extras' value, which carries the NF shift) while not in fast
/// attack, each reading used once; with Clarity on it is instead the
/// Core's `noise-floor` operation smoothed as the desktop's Clarity does,
/// alpha = 1 - exp(-dt / ``claritySmoothingSeconds``). What it sets is
/// not kept: it lives here, beside the pan's stored scale.
public struct GridFloorTracker: Equatable, Sendable {
    /// Every 500 ms (the note's rule).
    public static let intervalSeconds = 0.5
    /// A move of 2 dB or more.
    public static let stepDb = 2.0
    /// Clarity's smoothing time, 3 s.
    public static let claritySmoothingSeconds = 3.0

    /// The grid's bottom and top as the tracking set them; nil while it
    /// has set none, so the pan's own scale shows.
    public private(set) var range: ClosedRange<Double>?
    /// The latest display floor not yet used, and whether it was in fast attack.
    private var reading: (dbm: Double, fastAttack: Bool)?
    /// Clarity's smoothed estimate and when its last value came.
    private var smoothed: Double?
    private var smoothedAt: Double?
    /// When the rule last ran.
    private var ranAt: Double?

    public init() {}

    public static func == (lhs: GridFloorTracker, rhs: GridFloorTracker) -> Bool {
        lhs.range == rhs.range && lhs.reading?.dbm == rhs.reading?.dbm
            && lhs.reading?.fastAttack == rhs.reading?.fastAttack && lhs.smoothed == rhs.smoothed
            && lhs.smoothedAt == rhs.smoothedAt && lhs.ranAt == rhs.ranAt
    }

    /// The display noise floor from the Core's extras; `fastAttack` nil
    /// (a Core that does not say) reads as not in fast attack.
    public mutating func noteDisplayFloor(_ dbm: Float, fastAttack: Bool?) {
        guard dbm.isFinite else { return }
        reading = (Double(dbm), fastAttack == true)
    }

    /// One value of the Core's `noise-floor` operation at `time` seconds.
    public mutating func noteClarityFloor(_ dbm: Double, at time: Double) {
        guard dbm.isFinite else { return }
        if let previous = smoothed, let then = smoothedAt, time > then {
            let alpha = 1 - exp(-(time - then) / Self.claritySmoothingSeconds)
            smoothed = previous + alpha * (dbm - previous)
        } else if smoothed == nil {
            smoothed = dbm
        }
        smoothedAt = time
    }

    /// Forgets what the tracking set (it was turned off, or the band changed).
    public mutating func reset() {
        range = nil
        reading = nil
        ranAt = nil
    }

    /// Runs the rule at `time` seconds for `settings`; true when the range changed.
    @discardableResult
    public mutating func run(_ settings: BandDisplaySettings, transmitting: Bool, at time: Double) -> Bool {
        guard settings.gridFollowsNoiseFloor else {
            let had = range != nil
            reset()
            return had
        }
        if let ranAt, time - ranAt < Self.intervalSeconds {
            return false
        }
        ranAt = time
        guard !transmitting else {
            return false
        }
        let floor: Double
        if settings.waterfallLevelMode == .clarity {
            guard let smoothed else { return false }
            floor = smoothed
        } else {
            guard let taken = reading else { return false }
            reading = nil
            guard !taken.fastAttack else { return false }
            floor = taken.dbm
        }
        let current = range ?? settings.scaleRange
        let proposed = floor + Double(settings.gridNoiseFloorOffsetDb)
        guard abs(current.lowerBound - proposed) >= Self.stepDb else {
            return false
        }
        let top = settings.gridKeepsRange ? proposed + (current.upperBound - current.lowerBound) : current.upperBound
        range = min(proposed, top - 1)...top
        return true
    }

    /// `settings` with the tracked range as its scale, and no further
    /// following by the renderer; `settings` itself when none is set.
    public func applied(to settings: BandDisplaySettings) -> BandDisplaySettings {
        guard let range, settings.gridFollowsNoiseFloor else {
            return settings
        }
        var shown = settings
        shown.scaleBottomDbm = range.lowerBound
        shown.scaleTopDbm = range.upperBound
        return shown
    }
}
