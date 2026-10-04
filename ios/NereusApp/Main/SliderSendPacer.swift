// NereusSDR for iOS: paces a slider drag's writes: one at most every 50 ms, and the final value on release
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink

/// Paces one slider's values to its owner while the finger moves: the
/// first goes at once, then at most one every 50 ms, the latest winning,
/// and when the finger lifts the final value goes if it has not already.
/// The rate is the desktop's: its remote window sends the operator's
/// changes every 50 ms (`StationClient.h:644`, `kDefaultWriteFlushMs`;
/// `StationClient.cpp`, `onWriteFlushTick`), as ``TuneThrottle`` paces a
/// drag on the band.
@MainActor
final class SliderSendPacer {
    private let clock: any LinkClock
    private var throttle = TuneThrottle()
    private var held: (any LinkTimer)?
    /// Where each paced value goes; the row sets it to its current owner.
    var send: (Double) -> Void

    init(clock: any LinkClock = SystemLinkClock(), send: @escaping (Double) -> Void = { _ in }) {
        self.clock = clock
        self.send = send
    }

    /// The finger moved to `value`.
    func move(to value: Double) {
        switch throttle.offer(value, at: now) {
        case .send(let next):
            send(next)
        case .hold(let until):
            guard held == nil else {
                return
            }
            let delay = Duration.milliseconds(max(0, Int64(((until - now) * 1_000).rounded(.up))))
            held = clock.schedule(after: delay) { [weak self] in
                await self?.heldEnded()
            }
        case .nothing:
            break
        }
    }

    /// The finger lifted: the final value goes, if it has not already.
    func release() {
        held?.cancel()
        held = nil
        if let final = throttle.finish() {
            send(final)
        }
    }

    /// A retired context never flushes its held finger value into another context.
    func retire() {
        held?.cancel()
        held = nil
        _ = throttle.finish()
    }

    private var now: TimeInterval {
        TimeInterval(clock.nowMilliseconds) / 1_000
    }

    private func heldEnded() {
        held = nil
        if let value = throttle.due(at: now) {
            send(value)
        }
    }
}

/// The gesture draft used by both slider rows. The model owns the value
/// outside a drag; a Core answer can arrive while the finger is stationary.
struct SliderGestureDraft {
    private(set) var value: Double?
    private(set) var editing = false

    mutating func begin() { editing = true }
    private var revision: UInt64 = 0
    private var submittedRevision: UInt64?

    mutating func move(to next: Double) { editing = true; revision &+= 1; value = next }
    mutating func submitted(_ next: Double) {
        if value == next { submittedRevision = revision }
    }
    mutating func modelChanged(notConfirmed: Bool = false) {
        if !editing || (notConfirmed && submittedRevision == revision) { value = nil }
    }
    mutating func disable() { value = nil }
    mutating func release(model: Double?) {
        editing = false
        value = nil
    }
}
