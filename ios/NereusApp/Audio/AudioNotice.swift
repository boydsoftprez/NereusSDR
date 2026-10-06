// NereusSDR for iOS: a notice about the band's sound for the band screen to show
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Something about the band's sound the operator needs to see and act on.
/// The band screen shows ``text``; tapping it acts on the notice.
enum AudioNotice: Equatable, Sendable {
    /// Headphones or AirPods went away and the sound paused, waiting for the
    /// operator to play it on the speaker.
    case headphonesDisconnected
    /// A call, Siri or another app's audio ended without iOS saying the
    /// band may play again, so the sound waits for the operator's tap.
    case interruptionEnded

    var text: String {
        switch self {
        case .headphonesDisconnected:
            return "Sound paused: your headphones disconnected. Tap to play on the speaker."
        case .interruptionEnded:
            return "Sound paused after the interruption. Tap to play it again."
        }
    }

    /// What the band screen says for a moment after the tap.
    var resumedText: String {
        switch self {
        case .headphonesDisconnected:
            return "Playing on the speaker."
        case .interruptionEnded:
            return "Playing again."
        }
    }
}
