// NereusSDR for iOS: Cancel on the Live Activity while the link is lost
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents

/// Cancel (spec section 5.5 item 2): the lost link's retries stop, as
/// Cancel on the band's LINK LOST cover does.
struct CancelReconnectingIntent: LiveActivityIntent {
    static let title: LocalizedStringResource = "Stop reconnecting"
    static let description = IntentDescription("Stops trying to reconnect to the Core.")
    static let isDiscoverable = false
    /// Runs on the lock screen without Face ID: it only stops (JJ, 2026-09-26).
    static let authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    init() {}

    @MainActor
    func perform() async throws -> some IntentResult {
        await ActivityAction.perform(.cancelReconnecting)
        return .result()
    }
}
