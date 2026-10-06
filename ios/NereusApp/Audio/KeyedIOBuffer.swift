// NereusSDR for iOS: the small I/O buffer the microphone asks of the audio session while this phone transmits, and putting the old one back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import os

/// The part of the audio session that holds the I/O buffer duration, so
/// tests can stand in for it. The duration is one setting for the whole
/// app: the band's playback runs on it as much as the microphone does.
protocol IOBufferDurationPort: AnyObject, Sendable {
    /// The duration last asked for; 0 when nothing has been asked.
    var preferredIOBufferDuration: TimeInterval { get }
    /// The duration iOS runs now.
    var ioBufferDuration: TimeInterval { get }
    func setPreferredIOBufferDuration(_ duration: TimeInterval) throws
}

/// ``IOBufferDurationPort`` over the app's shared `AVAudioSession`.
final class SessionIOBufferDuration: IOBufferDurationPort, @unchecked Sendable {
    private let session: AVAudioSession

    init(session: AVAudioSession = .sharedInstance()) {
        self.session = session
    }

    var preferredIOBufferDuration: TimeInterval {
        session.preferredIOBufferDuration
    }

    var ioBufferDuration: TimeInterval {
        session.ioBufferDuration
    }

    func setPreferredIOBufferDuration(_ duration: TimeInterval) throws {
        try session.setPreferredIOBufferDuration(duration)
    }
}

/// The 10 ms I/O buffer the microphone's sends want (``MicCapture``), held
/// only while this phone transmits from its microphone.
///
/// The duration is shared by everything on the session, the band's
/// playback included, so it is asked for when a key starts and the
/// duration from before is put back as soon as the key ends (TestFlight
/// build 5, 2026-09-27: the 10 ms buffer was set when the mic level meter
/// opened the microphone and never put back, and the band sounded robotic
/// from then on). The mic level meter alone never changes it.
///
/// Not thread-safe: ``MicCapture`` calls it on its own queue only.
final class KeyedIOBuffer {
    /// The I/O buffer asked for while keyed, so the input comes in small pieces.
    static let keyedDuration: TimeInterval = 0.010

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.microphone")

    private let port: any IOBufferDurationPort
    /// The duration to put back, while the small one is held.
    private var saved: TimeInterval?

    init(port: any IOBufferDurationPort) {
        self.port = port
    }

    /// True while the small buffer is held.
    var isHeld: Bool {
        saved != nil
    }

    /// Asks for the small buffer, remembering what to put back: the
    /// duration asked for before, or, when nothing was asked, the one iOS
    /// ran. With neither known the small buffer is not asked for, since it
    /// could not be put back; the sends are paced anyway.
    func hold() {
        guard saved == nil else {
            return
        }
        let preferred = port.preferredIOBufferDuration
        let previous = preferred > 0 ? preferred : port.ioBufferDuration
        guard previous > 0 else {
            Self.logger.warning("the I/O buffer in use is unknown; keeping it while transmitting")
            return
        }
        do {
            try port.setPreferredIOBufferDuration(Self.keyedDuration)
            saved = previous
        } catch {
            Self.logger.warning("the 10 ms I/O buffer was refused: \(error.localizedDescription)")
        }
    }

    /// Puts back the duration from before ``hold()``; nothing when not held.
    func release() {
        guard let saved else {
            return
        }
        self.saved = nil
        do {
            try port.setPreferredIOBufferDuration(saved)
        } catch {
            Self.logger.warning("the I/O buffer from before transmitting was refused: \(error.localizedDescription)")
        }
        let restored = String(format: "%.1f", saved * 1000)
        Self.logger.notice("I/O buffer put back to \(restored, privacy: .public) ms after transmitting")
    }
}
