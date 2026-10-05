// NereusSDR for iOS: which microphone the operator talks into
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The microphone this phone transmits from (spec section 5.4 item 6).
/// The iPhone's own is the default: the AirPods' would drop them to
/// phone-call quality both ways, since iOS then runs them on the hands-free
/// profile (A2DP is output-only).
enum MicrophoneChoice: String, CaseIterable, Sendable {
    case iPhone
    case airPods

    /// The default, and what a value this build does not know reads as.
    static let standard = MicrophoneChoice.iPhone
}
