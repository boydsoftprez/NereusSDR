// NereusSDR for iOS: the part of the audio session the controller drives, so tests can stand in for it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation

/// The audio session as ``AudioSessionController`` uses it. The controller
/// calls it only from its own serial queue, never from the main thread,
/// because activating a session can block. The app's conformance is
/// ``SystemAudioSession``; tests use a stand-in that routes the way the
/// phone does.
protocol AudioSessionPort: AnyObject, Sendable {
    func setCategory(_ category: AVAudioSession.Category, mode: AVAudioSession.Mode,
                     options: AVAudioSession.CategoryOptions) throws
    func overrideOutputAudioPort(_ port: AVAudioSession.PortOverride) throws
    func setActive(_ active: Bool, options: AVAudioSession.SetActiveOptions) throws
    /// Lets the dial's haptic ticks and the system's sounds play while the
    /// microphone is open. iOS silences them while an app records unless
    /// this is on, and on the phone they stayed silent after the mic level
    /// meter or a key had opened the microphone, until the app was closed
    /// (TestFlight build 5, 2026-09-27). A failure is logged, never thrown:
    /// the band plays either way.
    func allowHapticsWhileRecording()
    /// The outputs the session plays through now.
    var currentOutputs: [AudioOutputPort] { get }
    /// The session as the log shows it: category, mode, options, the
    /// output override last set, and the route's output and input port
    /// types. Never a port's name or address.
    var diagnostics: String { get }
    /// True on an iPhone, which has an earpiece; false on an iPad.
    var isPhone: Bool { get }
}
