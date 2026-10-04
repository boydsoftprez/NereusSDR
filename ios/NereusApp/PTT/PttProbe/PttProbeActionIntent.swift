// NereusSDR for iOS: the PTT button test's Action button intent (debug copies only, never keys the radio)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import AppIntents

/// The Action button for the button test (plan Task 65, question 2): a
/// Push to Talk transmission intent, so iOS can run it with the phone
/// locked and the app in the background. Each run is one press: the
/// first begins a test transmission on the probe's channel, the next
/// ends it. It never keys the radio.
struct PttProbeActionIntent: PushToTalkTransmissionIntent {
    static let title: LocalizedStringResource = "PTT button test"
    static let description = IntentDescription("Begins or ends a test transmission. It never keys the radio.")
    /// A locked phone runs it without Face ID, which is what the test asks.
    static let authenticationPolicy: IntentAuthenticationPolicy = .alwaysAllowed

    init() {}

    @MainActor
    func perform() async throws -> some IntentResult {
        await PttProbe.shared.actionButtonPressed()
        return .result()
    }
}

/// Offers the intent to the Action button (Settings, Action Button,
/// Shortcut, NereusSDR). Debug copies only.
struct PttProbeShortcuts: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(intent: PttProbeActionIntent(),
                    phrases: ["Test the PTT button in \(.applicationName)"],
                    shortTitle: "PTT button test",
                    systemImageName: "button.programmable")
    }
}
#endif
