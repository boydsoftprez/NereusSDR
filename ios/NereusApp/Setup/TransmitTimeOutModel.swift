// NereusSDR for iOS: the transmit time-out for phones and iPads, a Core setting read and written through the settings proxy
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// The Transmit time-out group on the PTT buttons page (D29, R-IOS-21,
/// spec section 5.5 item 14): "Stop transmitting after" a time from 30
/// seconds to 30 minutes, or Off, 3 minutes until someone changes it. It is
/// the Core's setting, shared with every device and with the desktop's
/// Setup, General, Options "Phone and iPad" row: `RemoteMoxTimeOutEnabled`
/// ("True" or "False") and `RemoteMoxTimeOutSeconds`. The Core clamps the
/// seconds to its own limits when it reads them; this page offers only
/// values inside them. A refusal shows the Core's words under the group.
@MainActor
final class TransmitTimeOutModel: ObservableObject {
    static let enabledKey = "RemoteMoxTimeOutEnabled"
    static let secondsKey = "RemoteMoxTimeOutSeconds"
    /// The Core's default and limits (its `TxTimeOutTimer`: 180, 30 and
    /// 1800 seconds; on by default for phones and iPads, D29).
    static let defaultSeconds = 180
    static let minimumSeconds = 30
    static let maximumSeconds = 1800
    static let defaultEnabled = true
    /// The times the menu steps through, in seconds.
    static let choices = [30, 60, 120, 180, 300, 600, 900, 1200, 1800]
    /// What the group says when the Core refuses a change without a reason.
    static let refusedText = "The Core did not change the time-out."

    /// The Core's words for the last change it refused.
    @Published private(set) var problem: String?

    let settings: SettingsProxyClient

    init(settings: SettingsProxyClient) {
        self.settings = settings
    }

    /// The time-out is on.
    var enabled: Bool {
        guard let text = settings.value(Self.enabledKey) else {
            return Self.defaultEnabled
        }
        return text == "True"
    }

    /// The time-out in seconds, within the Core's limits.
    var seconds: Int {
        guard let text = settings.value(Self.secondsKey), let value = Int(text) else {
            return Self.defaultSeconds
        }
        return min(max(value, Self.minimumSeconds), Self.maximumSeconds)
    }

    /// The menu's current choice: nil for Off.
    var choice: Int? { enabled ? seconds : nil }

    /// The menu's entries, with the current time among them when the
    /// desktop set one the menu does not list.
    var menu: [Int] {
        guard enabled, !Self.choices.contains(seconds) else {
            return Self.choices
        }
        return (Self.choices + [seconds]).sorted()
    }

    /// A time in the group's words: "30 seconds", "1 minute", "3 minutes",
    /// "2 minutes 30 seconds", or "Off".
    static func text(_ seconds: Int?) -> String {
        guard let seconds else {
            return "Off"
        }
        let minutes = seconds / 60
        let rest = seconds % 60
        func unit(_ count: Int, _ one: String) -> String {
            count == 1 ? "1 \(one)" : "\(count) \(one)s"
        }
        if minutes == 0 {
            return unit(rest, "second")
        }
        if rest == 0 {
            return unit(minutes, "minute")
        }
        return "\(unit(minutes, "minute")) \(unit(rest, "second"))"
    }

    /// Picks a time, or Off (nil): the seconds first, then the switch, so
    /// the Core never runs the new switch with the old time.
    func choose(_ seconds: Int?) async {
        problem = nil
        if let seconds {
            let clamped = min(max(seconds, Self.minimumSeconds), Self.maximumSeconds)
            if !(await accepted(settings.write(Self.secondsKey, String(clamped)))) {
                return
            }
            _ = await accepted(settings.write(Self.enabledKey, "True"))
        } else {
            _ = await accepted(settings.write(Self.enabledKey, "False"))
        }
    }

    private func accepted(_ outcome: SettingsWriteOutcome) -> Bool {
        switch outcome {
        case .accepted, .keptOnThisDevice:
            return true
        case .rejected(let reason):
            problem = reason.isEmpty ? Self.refusedText : reason
            return false
        case .notSent, .linkLost:
            problem = Self.notSentText
            return false
        case .notConfirmed:
            // The value stays, marked; the Core's answer may still come.
            problem = PropertyWriteOutcome.notConfirmed.reason
            return false
        }
    }

    static let notSentText = "The change didn't reach the Core. Try again when the link is back."
}
