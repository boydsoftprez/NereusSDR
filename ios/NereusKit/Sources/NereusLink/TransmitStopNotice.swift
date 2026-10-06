// NereusSDR for iOS: why the Core stopped this device's transmission on its own
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A stop of this device's key by the Core (link document section 18.8,
/// Stops): the time-out, the link going quiet, microphone starvation, a
/// take, a removal or any other stop of the Core's own. The band shows
/// `text` as sent, and PTT reads Tap.
public struct TransmitStopNotice: Equatable, Sendable {
    /// `timeOut`, `linkLost`, `micStarved`, `takenOver`, `revoked` or `station`.
    public let reason: String
    /// The Core's words.
    public let text: String
    /// The stop's `stopSerial`, which tells one stop from the next.
    public let serial: Int64

    public init(reason: String, text: String, serial: Int64) {
        self.reason = reason
        self.text = text
        self.serial = serial
    }
}
