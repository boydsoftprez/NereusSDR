// NereusSDR for iOS: the Core Opus receive profile's measured delay and packet shape
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
internal import COpusShim

/// Matches the Core's 48 kHz stereo OPUS_APPLICATION_AUDIO encoder query.
/// The microphone encoder's independently configured application is not used.
public enum AudioPlaybackCodecDelay {
    public static let frames: Int? = {
        let measured = nereus_opus_receive_profile_lookahead_frames()
        return measured >= 0 ? Int(measured) : nil
    }()

    static func hasValidPacketShape(_ payload: Data) -> Bool {
        payload.withUnsafeBytes { raw in
            nereus_opus_valid_receive_packet(raw.bindMemory(to: UInt8.self).baseAddress,
                                             Int32(raw.count)) != 0
        }
    }
}
