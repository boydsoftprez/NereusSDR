// NereusSDR for iOS: when the screen stays on: never, while charging, or always while the band shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Setup, Battery and sessions, Keep the screen on (spec section 5.5 item
/// 10, D27): Always by default. Whatever the choice, the screen stays on
/// while this phone is keyed.
enum KeepScreenOn: String, CaseIterable, Sendable {
    case never
    case whileCharging
    case always

    static let standard = KeepScreenOn.always

    /// Whether the screen stays on: always while keyed; otherwise only while
    /// the band is showing, and then as chosen.
    func screenStaysOn(bandShowing: Bool, charging: Bool, keyed: Bool) -> Bool {
        if keyed {
            return true
        }
        guard bandShowing else {
            return false
        }
        switch self {
        case .never:
            return false
        case .whileCharging:
            return charging
        case .always:
            return true
        }
    }
}
