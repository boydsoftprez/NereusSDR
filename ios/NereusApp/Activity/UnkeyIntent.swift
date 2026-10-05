// NereusSDR for iOS: UNKEY on the lock-screen card and the opened Dynamic Island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents

/// UNKEY (D24, D25; spec section 5.5 items 3 and 5): ends this phone's
/// transmission in one tap. iOS runs it in the app's process, where it
/// goes through the PTT's one ordered queue as `tx.unkey`, as Stop on the
/// TX pill does. It never keys. Built into the app and the widget, as
/// iOS asks of a Live Activity intent.
struct UnkeyIntent: LiveActivityIntent {
    static let title: LocalizedStringResource = "Unkey"
    static let description = IntentDescription("Stops this phone's transmission.")
    static let isDiscoverable = false
    /// One tap, on the lock screen too, with no Face ID first: a stop
    /// never waits (JJ, 2026-09-26; spec sections 4.7 and 5.5 item 8).
    static let authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    init() {}

    @MainActor
    func perform() async throws -> some IntentResult {
        await ActivityAction.perform(.unkey)
        return .result()
    }
}
