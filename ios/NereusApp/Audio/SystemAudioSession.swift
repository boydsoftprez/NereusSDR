// NereusSDR for iOS: the phone's shared audio session behind the controller's port
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
import os
import UIKit

/// ``AudioSessionPort`` over the app's shared `AVAudioSession`, which may be
/// called from any thread.
final class SystemAudioSession: AudioSessionPort, @unchecked Sendable {
    private let session: AVAudioSession
    let isPhone: Bool
    private let lock = NSLock()
    private var lastOverride: AVAudioSession.PortOverride = .none

    @MainActor
    init(session: AVAudioSession = .sharedInstance()) {
        self.session = session
        isPhone = UIDevice.current.userInterfaceIdiom == .phone
    }

    func setCategory(_ category: AVAudioSession.Category, mode: AVAudioSession.Mode,
                     options: AVAudioSession.CategoryOptions) throws {
        try session.setCategory(category, mode: mode, options: options)
        // A category change drops the override.
        lock.withLock { lastOverride = .none }
    }

    func overrideOutputAudioPort(_ port: AVAudioSession.PortOverride) throws {
        try session.overrideOutputAudioPort(port)
        lock.withLock { lastOverride = port }
    }

    var diagnostics: String {
        let route = session.currentRoute
        let override = lock.withLock { lastOverride } == .speaker ? "speaker" : "none"
        return "category \(session.category.rawValue), mode \(session.mode.rawValue), "
            + "options \(session.categoryOptions.rawValue), override \(override), "
            + "haptics while recording \(session.allowHapticsAndSystemSoundsDuringRecording ? "on" : "off"), "
            + "outputs [\(route.outputs.map(\.portType.rawValue).joined(separator: ", "))], "
            + "inputs [\(route.inputs.map(\.portType.rawValue).joined(separator: ", "))]"
    }

    func setActive(_ active: Bool, options: AVAudioSession.SetActiveOptions) throws {
        try session.setActive(active, options: options)
    }

    func allowHapticsWhileRecording() {
        Self.allowHapticsWhileRecording(on: session)
    }

    /// Turns on haptics and system sounds while recording for `session`,
    /// logging a failure. ``MicCapture`` calls it too, before it opens the
    /// microphone.
    static func allowHapticsWhileRecording(on session: AVAudioSession = .sharedInstance()) {
        do {
            try session.setAllowHapticsAndSystemSoundsDuringRecording(true)
        } catch {
            logger.warning("haptics while recording could not be allowed: \(error.localizedDescription)")
        }
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.session")

    var currentOutputs: [AudioOutputPort] {
        session.currentRoute.outputs.map(Self.port(for:))
    }

    /// How the controller sees one output of a route.
    static func port(for description: AVAudioSessionPortDescription) -> AudioOutputPort {
        switch description.portType {
        case .builtInSpeaker:
            return AudioOutputPort(kind: .speaker, name: description.portName)
        case .builtInReceiver:
            return AudioOutputPort(kind: .earpiece, name: description.portName)
        case .headphones, .bluetoothA2DP, .bluetoothHFP, .bluetoothLE:
            return AudioOutputPort(kind: .external, name: description.portName, pausesOnRemoval: true,
                                   isHeadphones: true)
        case .usbAudio, .lineOut, .carAudio:
            return AudioOutputPort(kind: .external, name: description.portName, pausesOnRemoval: true)
        default:
            return AudioOutputPort(kind: .external, name: description.portName)
        }
    }
}
