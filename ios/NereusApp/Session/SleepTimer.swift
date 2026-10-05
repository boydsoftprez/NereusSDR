// NereusSDR for iOS: the sleep timer: listen for 30 minutes, an hour or two hours, then disconnect
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink

/// Setup, Battery and sessions, Sleep timer (spec section 5.5 item 11): off
/// by default, or 30 minutes, 1 hour or 2 hours from when the Core connects
/// (or from when the choice is made while connected), then disconnect. A
/// link that drops and comes back keeps the time it had; a disconnect ends
/// it. At the time, the disconnect waits while this phone is keyed, and
/// goes as soon as it is not.
@MainActor
final class SleepTimer: ObservableObject {
    /// The choices, by their minutes.
    enum Choice: Int, CaseIterable, Sendable {
        case off = 0
        case thirtyMinutes = 30
        case oneHour = 60
        case twoHours = 120

        var duration: TimeInterval? {
            self == .off ? nil : TimeInterval(rawValue * 60)
        }
    }

    /// When the session ends; nil while no timer runs.
    @Published private(set) var endsAt: Date?

    /// Ends the session; returns false when it could not (keyed), so the
    /// timer asks again at the next tick.
    var onTime: @MainActor () async -> Bool = { false }
    var onExpiry: (@MainActor (Expiry) async -> Bool)?

    /// An expiry belongs to one choice and deadline. Revocation reaches a
    /// queued property write at its synchronous transport handoff.
    struct Expiry {
        let generation: UInt64
        let endsAt: Date
        let choice: Choice
        let permit: CommandSendPermit
    }

    private let now: () -> Date
    private var generation: UInt64 = 0
    private var choice: Choice = .off
    private var activeExpiry: Expiry?

    init(now: @escaping () -> Date = Date.init) {
        self.now = now
    }

    /// The Core connected with `choice`: a timer starts unless one runs.
    func connected(_ choice: Choice) {
        guard endsAt == nil else {
            return
        }
        start(choice)
    }

    /// The choice changed while connected: the timer starts again from now.
    func chosen(_ choice: Choice) {
        start(choice)
    }

    /// The session ended: no timer runs.
    func disconnected() {
        invalidatePendingExpiry()
        endsAt = nil
    }

    /// New connection intent or timer choice cancels an expiry before any await.
    func invalidatePendingExpiry() {
        generation &+= 1
        activeExpiry?.permit.revoke()
        activeExpiry = nil
    }

    func isCurrent(_ expiry: Expiry) -> Bool {
        generation == expiry.generation && endsAt == expiry.endsAt && choice == expiry.choice
            && activeExpiry?.permit === expiry.permit && !expiry.permit.isRevoked
    }

    /// Ends the session once its time has come. The app calls it every few
    /// seconds while connected.
    func tick() async {
        guard let endsAt, now() >= endsAt else {
            return
        }
        if let activeExpiry, isCurrent(activeExpiry) { return }
        let expiry = Expiry(generation: generation, endsAt: endsAt, choice: choice,
                            permit: CommandSendPermit())
        activeExpiry = expiry
        let done: Bool
        if let onExpiry { done = await onExpiry(expiry) } else { done = await onTime() }
        if done && isCurrent(expiry) {
            self.endsAt = nil
        }
        if activeExpiry?.permit === expiry.permit { activeExpiry = nil }
        expiry.permit.revoke()
    }

    private func start(_ choice: Choice) {
        invalidatePendingExpiry()
        self.choice = choice
        endsAt = choice.duration.map { now().addingTimeInterval($0) }
    }
}
