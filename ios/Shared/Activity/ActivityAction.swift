// NereusSDR for iOS: what a button on the Live Activity asks the app to do, and where the app hears it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import os

/// The Live Activity's buttons (spec section 5.5 items 1 to 5; the board's
/// lock screen and island): UNKEY, the speaker, and Cancel or Reconnect
/// while the link is lost. Each button is a Live Activity intent, which iOS
/// runs in the app's own process; the intent hands its action to
/// ``handler``, which the app sets as it starts. In the widget's process
/// no handler is set: the tap is logged there and goes no further.
enum ActivityAction: Equatable, Sendable {
    /// End this phone's transmission, through the PTT's one ordered queue.
    case unkey
    /// Mute or unmute the band's sound on this phone.
    case setMuted(Bool)
    /// Stop retrying the lost link.
    case cancelReconnecting
    /// Start retrying again after Cancel.
    case reconnect

    /// Where the app hears the Live Activity's buttons.
    @MainActor static var handler: (@MainActor (ActivityAction) async -> Void)?

    /// Where the current task's actions go, ahead of ``handler``: a test
    /// hands the buttons to its own app this way, leaving the one the app
    /// set alone. Nil everywhere else.
    @TaskLocal static var taskHandler: (@MainActor @Sendable (ActivityAction) async -> Void)?

    /// Hands an action to the app, when the app is listening.
    @MainActor static func perform(_ action: ActivityAction) async {
        if let taskHandler {
            await taskHandler(action)
            return
        }
        guard let handler else {
            logger.info("A Live Activity button was tapped where the app isn't listening: \(String(describing: action), privacy: .public)")
            return
        }
        await handler(action)
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "activity")
}
