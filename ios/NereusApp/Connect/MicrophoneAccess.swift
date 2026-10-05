// NereusSDR for iOS: whether the app may use the microphone, and asking iOS once
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation

/// The microphone question (spec section 5.3 item 4): asked right after
/// the first pairing, so iOS never interrupts the first transmission.
/// Listening works without it.
@MainActor
protocol MicrophoneAccess: AnyObject {
    /// True while iOS has not asked yet.
    var needsAsking: Bool { get }
    /// Asks iOS once; true when the operator allowed it.
    func ask() async -> Bool
}

/// The phone's own microphone permission.
@MainActor
final class SystemMicrophoneAccess: MicrophoneAccess {
    var needsAsking: Bool {
        AVAudioApplication.shared.recordPermission == .undetermined
    }

    func ask() async -> Bool {
        await AVAudioApplication.requestRecordPermission()
    }
}
