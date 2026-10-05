// NereusSDR for iOS: the audio session options and output override for one route choice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation

/// What ``AudioSessionController`` sets on the session for a route choice.
/// The category and mode never change; only these do.
struct AudioSessionConfiguration: Equatable, Sendable {
    let options: AVAudioSession.CategoryOptions
    let outputOverride: AVAudioSession.PortOverride
}
