// NereusSDR for iOS: Setup's Battery and sessions page: sound only, the screen, a sleep timer and saving power
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Setup, General, Battery and sessions (R-IOS-22, spec section 5.5 items
/// 9 to 11, D27, picture 16), in the board's words and order, kept on this
/// phone: Sound only while locked or in another app, on by default; Keep
/// the screen on, Always by default; the sleep timer, off by default; and
/// the two ways of saving power, both on. Each applies at once.
struct BatteryAndSessionsPage: View {
    @ObservedObject var settings: PhoneSettings
    @ObservedObject var sleepTimer: SleepTimer

    /// Sound only's cost: Data use's Audio only, at High.
    static let soundOnlyDetail = "The Core stops sending the band \u{00B7} about 24 MB an hour"

    var body: some View {
        List {
            Section {
                SetupSwitchRow(title: "Sound only", detail: Self.soundOnlyDetail,
                               isOn: Binding(get: { settings.soundOnlyWhenAway },
                                             set: { settings.soundOnlyWhenAway = $0 }), id: "soundOnly")
            } header: {
                Text("Locked or in another app")
            }
            Section {
                screen(.never, "Never", "It still stays on while you're keyed", id: "screen.never")
                screen(.whileCharging, "While charging", "For a stand or a car mount", id: "screen.whileCharging")
                screen(.always, "Always", "While the band is showing", id: "screen.always")
            } header: {
                Text("Keep the screen on")
            }
            Section {
                sleep(.off, "Off", "Listen until you stop", id: "sleep.off")
                sleep(.thirtyMinutes, "30 minutes", "Then disconnect", id: "sleep.30")
                sleep(.oneHour, "1 hour", "Then disconnect", id: "sleep.60")
                sleep(.twoHours, "2 hours", "Then disconnect", id: "sleep.120")
            } header: {
                Text("Sleep timer")
            } footer: {
                if let endsAt = sleepTimer.endsAt {
                    Text(Self.sleepFooter(endsAt))
                        .accessibilityIdentifier("sleepEndsAt")
                }
            }
            Section {
                SetupSwitchRow(title: "In Low Power Mode, drop to Saver", detail: "5 frames a second, half the detail",
                               isOn: Binding(get: { settings.lowPowerDropsToSaver },
                                             set: { settings.lowPowerDropsToSaver = $0 }), id: "lowPowerSaver")
                SetupSwitchRow(title: "Slow the band when the phone is hot", detail: "Until it cools",
                               isOn: Binding(get: { settings.hotPhoneSlows }, set: { settings.hotPhoneSlows = $0 }),
                               id: "hotPhoneSlows")
            } header: {
                Text("Saving power")
            }
        }
        .navigationTitle("Battery and sessions")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }

    private func screen(_ choice: KeepScreenOn, _ title: String, _ detail: String, id: String) -> some View {
        SetupChoiceRow(title: title, detail: detail, chosen: settings.keepScreenOn == choice, id: id) {
            settings.keepScreenOn = choice
        }
    }

    private func sleep(_ choice: SleepTimer.Choice, _ title: String, _ detail: String, id: String) -> some View {
        SetupChoiceRow(title: title, detail: detail, chosen: settings.sleepTimer == choice, id: id) {
            settings.sleepTimer = choice
        }
    }

    /// While the timer runs: "Disconnects at 21:15."
    static func sleepFooter(_ endsAt: Date, timeZone: TimeZone = .current) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = timeZone
        return "Disconnects at \(formatter.string(from: endsAt))."
    }
}
