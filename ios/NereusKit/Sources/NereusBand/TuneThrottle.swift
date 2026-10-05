// NereusSDR for iOS: paces a drag's frequency writes: at most one every 50 ms, and the final value at the end
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Paces the frequency writes of one drag on the band (R-IOS-12): the first
/// value goes at once, then at most one every ``interval`` while the finger
/// moves, the latest value winning; when the finger lifts, the final value
/// goes if it has not already. Times are seconds on any steady clock.
public struct TuneThrottle: Equatable, Sendable {
    /// What to do with an offered value.
    public enum Decision: Equatable, Sendable {
        /// Send this value now.
        case send(Double)
        /// Hold it: the next write may go at this time.
        case hold(until: TimeInterval)
        /// Nothing new to send.
        case nothing
    }

    /// The least time between two writes during a drag.
    public static let interval: TimeInterval = 0.05
    /// Clock readings a microsecond short of the interval count as on time.
    static let tolerance: TimeInterval = 1e-6

    public private(set) var lastSent: Double?
    private var lastSentAt: TimeInterval?
    /// The latest value not yet sent.
    public private(set) var pending: Double?

    public init() {}

    /// A new value from the moving finger at `now`.
    public mutating func offer(_ hz: Double, at now: TimeInterval) -> Decision {
        if hz == lastSent {
            pending = nil
            return .nothing
        }
        if let at = lastSentAt, now - at < Self.interval - Self.tolerance {
            pending = hz
            return .hold(until: at + Self.interval)
        }
        return send(hz, at: now)
    }

    /// Called when a hold ends: the held value, if its time has come.
    public mutating func due(at now: TimeInterval) -> Double? {
        guard let hz = pending, let at = lastSentAt, now - at >= Self.interval - Self.tolerance else {
            return nil
        }
        guard case .send(let value) = send(hz, at: now) else {
            return nil
        }
        return value
    }

    /// The finger lifted: the final value, when it has not been
    /// sent yet. The throttle is then ready for the next drag.
    public mutating func finish() -> Double? {
        let final = pending
        self = TuneThrottle()
        return final
    }

    private mutating func send(_ hz: Double, at now: TimeInterval) -> Decision {
        lastSent = hz
        lastSentAt = now
        pending = nil
        return .send(hz)
    }
}
