// NereusSDR for iOS: when the 3D view is drawn, how often, and why a pan shows 2D while 3D stays chosen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Keeps the 3D view within what this phone can draw (JJ's board,
/// recommendation 3): it measures how many 3D frames a second the phone
/// could draw, from how long each drawn frame took. Under
/// ``smoothFloorFps`` for a whole ``windowSeconds`` it first draws the
/// stack at half the rate; still under it for ``sustainedSeconds`` at half
/// rate, the pan shows 2D (``Stage/fellBack``) until Try 3D again. 3D stays
/// the pan's choice throughout; nothing here is kept.
///
/// The clock and the frames are the caller's (``noteFrame(drawSeconds:at:)``),
/// so the tests drive it with their own.
public struct StackedTracePacer: Equatable, Sendable {
    public enum Stage: Equatable, Sendable {
        /// Every frame the band gets is drawn.
        case full
        /// Every second frame is drawn.
        case halfRate
        /// The pan shows 2D until Try 3D again.
        case fellBack
    }

    /// BENCH-PENDING: the frames a second under which the stack counts as
    /// not keeping up ("about 20", JJ, 2026-09-29), and for how long at
    /// half rate before the pan shows 2D ("about 5 seconds"). Neither has
    /// been measured on a phone.
    public static let smoothFloorFps = 20.0
    public static let sustainedSeconds = 5.0
    /// BENCH-PENDING: how long each measurement runs.
    public static let windowSeconds = 1.0
    /// Never faster than this, whatever the screen can do (recommendation 4:
    /// never at 120 Hz).
    public static let fastestFps = 60.0

    public private(set) var stage: Stage = .full
    private var windowStart: Double?
    private var framesInWindow = 0
    private var busySeconds = 0.0
    /// When the half-rate stack was first measured slow without a let-up.
    private var slowSince: Double?

    public init() {}

    /// One 3D frame drawn, taking `drawSeconds` from the start of its
    /// drawing to the graphics finishing it, noted at `now` (seconds).
    /// Returns whether the stage changed.
    @discardableResult
    public mutating func noteFrame(drawSeconds: Double, at now: Double) -> Bool {
        guard stage != .fellBack, drawSeconds.isFinite, drawSeconds >= 0, now.isFinite else {
            return false
        }
        guard let start = windowStart, now >= start else {
            // The first frame, or a clock that went back: a new window.
            windowStart = now
            framesInWindow = 1
            busySeconds = drawSeconds
            return false
        }
        framesInWindow += 1
        busySeconds += drawSeconds
        guard now - start >= Self.windowSeconds else {
            return false
        }
        let rate = busySeconds > 0 ? Double(framesInWindow) / busySeconds : .infinity
        let slow = rate < Self.smoothFloorFps
        windowStart = now
        framesInWindow = 0
        busySeconds = 0
        switch stage {
        case .full:
            guard slow else {
                return false
            }
            stage = .halfRate
            slowSince = now
            return true
        case .halfRate:
            guard slow else {
                slowSince = nil
                return false
            }
            let since = slowSince ?? start
            slowSince = since
            guard now - since >= Self.sustainedSeconds else {
                return false
            }
            stage = .fellBack
            return true
        case .fellBack:
            return false
        }
    }

    /// Try 3D again: back to every frame, measured afresh.
    public mutating func tryAgain() {
        self = StackedTracePacer()
    }

    /// Whether a frame at this position in the band's frame count is drawn.
    public func draws(frame number: UInt64) -> Bool {
        switch stage {
        case .full:
            return true
        case .halfRate:
            return number % 2 == 0
        case .fellBack:
            return false
        }
    }

    /// The least time between two 3D draws for frames asked at `askedFps`:
    /// never faster than the frames asked for, nor than ``fastestFps``, and
    /// half as often at half rate.
    public func leastInterval(askedFps: Double) -> Double {
        let fps = askedFps.isFinite && askedFps > 0 ? min(askedFps, Self.fastestFps) : Self.fastestFps
        return (stage == .halfRate ? 2 : 1) / fps
    }
}

/// Why a pan that has 3D chosen shows 2D, and the words it says.
public enum StackedTraceHold: Equatable, Sendable {
    /// The Core offers no 3D view (below description 12, or its 3D gate).
    case coreOlder
    /// The phone could not keep the stack moving smoothly.
    case cannotKeepUp
    /// Low Power Mode is on.
    case lowPower
    /// The phone is hot.
    case hot

    /// The reason in the sheet and Setup for a Core without the 3D view.
    public static let coreOlderText = "This Core does not offer the 3D view. Updating the Core may help."

    /// What the pan says while it shows 2D for this reason, for `pan`.
    public func notice(pan: Int) -> String {
        switch self {
        case .coreOlder:
            return Self.coreOlderText
        case .cannotKeepUp:
            return "It could not keep the 3D view moving smoothly. 3D stays your choice for Pan \(pan)."
        case .lowPower:
            return "Low Power Mode is on. 3D comes back when it is off, and stays your choice for Pan \(pan)."
        case .hot:
            return "The phone is hot. 3D comes back when it cools, and stays your choice for Pan \(pan)."
        }
    }

    /// The notice's first words, in bold on the band.
    public static let noticeTitle = "Showing 2D on this phone."

    /// Why a pan with 3D chosen shows 2D now, or nil while it draws 3D
    /// (and for a pan without 3D chosen). The Core's lack comes first,
    /// then Low Power Mode, then heat, then a phone that could not keep up.
    public static func reason(chosen: Bool, offered: Bool, lowPower: Bool, hot: Bool,
                              stage: StackedTracePacer.Stage) -> StackedTraceHold? {
        guard chosen else {
            return nil
        }
        if !offered {
            return .coreOlder
        }
        if lowPower {
            return .lowPower
        }
        if hot {
            return .hot
        }
        return stage == .fellBack ? .cannotKeepUp : nil
    }
}
