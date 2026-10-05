// NereusSDR for iOS: Reconnect on the Live Activity after Cancel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents

/// Reconnect (the board's lost card after Cancel): the retries start again,
/// as Reconnect on the band's LINK LOST cover does. Transmit stays off
/// after it; the next key is the operator's.
struct ReconnectIntent: LiveActivityIntent {
    static let title: LocalizedStringResource = "Reconnect"
    static let description = IntentDescription("Tries again to reconnect to the Core.")
    static let isDiscoverable = false
    /// Starting the link again waits for the operator to authenticate on
    /// the lock screen (JJ, 2026-09-26).
    static let authenticationPolicy: IntentAuthenticationPolicy = .requiresAuthentication

    init() {}

    @MainActor
    func perform() async throws -> some IntentResult {
        await ActivityAction.perform(.reconnect)
        return .result()
    }
}
