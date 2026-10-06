// NereusSDR for iOS: the speaker on the lock-screen card and the opened Dynamic Island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents

/// The card's speaker (spec section 5.5 item 1; the board's "Mute this
/// phone"): mutes or unmutes the band's sound on this phone, as the
/// toolbar's speaker does. It carries the state it asks for, so a second
/// tap that arrives late can't undo the first.
struct MuteIntent: LiveActivityIntent {
    static let title: LocalizedStringResource = "Mute this phone"
    static let description = IntentDescription("Mutes or unmutes the band's sound on this phone.")
    static let isDiscoverable = false
    /// Runs on the lock screen without Face ID (JJ, 2026-09-26).
    static let authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    @Parameter(title: "Muted")
    var muted: Bool

    init() {}

    init(muted: Bool) {
        self.muted = muted
    }

    @MainActor
    func perform() async throws -> some IntentResult {
        await ActivityAction.perform(.setMuted(muted))
        return .result()
    }
}
