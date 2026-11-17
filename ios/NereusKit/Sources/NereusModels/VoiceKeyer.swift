// NereusSDR for iOS: the Core's voice keyer, M1 to M8 and the message going out, as the recordings document sends it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 15, the phone's copy of the voice keyer.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation

/// The voice keyer in the recordings document (`recordingsVersion` 3):
/// eight messages kept on the Core, whether a message repeats, and the
/// message going out (or waiting to repeat), if any.
public struct VoiceKeyerState: Equatable, Sendable, Decodable {
    /// The voice keyer's message slots are M1 to M8.
    public static let slotRange = 1...8
    /// The longest name a message may have.
    public static let nameMaxCharacters = 32
    /// The wait between repeats, in seconds.
    public static let repeatDelayRange = 1...60

    /// One message slot.
    public struct Slot: Equatable, Sendable, Decodable {
        /// 1 to 8.
        public let n: Int
        public let name: String
        public let lengthMs: Int64
        public let empty: Bool

        public init(n: Int, name: String, lengthMs: Int64, empty: Bool) {
            self.n = n
            self.name = name
            self.lengthMs = lengthMs
            self.empty = empty
        }
    }

    /// The message going out, or (``waitingMs`` above 0) the wait before its next repeat.
    public struct Sending: Equatable, Sendable, Decodable {
        public let n: Int
        public let positionMs: Int64
        public let lengthMs: Int64
        /// Counts down between repeats; 0 while the message goes out.
        public let waitingMs: Int64
        /// The sender in the Core's words: "Desktop" for the Core's own window.
        public let by: String
        /// The sender as the link names a device: `station` for the Core's own window.
        public let deviceId: String

        public init(n: Int, positionMs: Int64, lengthMs: Int64, waitingMs: Int64, by: String, deviceId: String) {
            self.n = n
            self.positionMs = positionMs
            self.lengthMs = lengthMs
            self.waitingMs = waitingMs
            self.by = by
            self.deviceId = deviceId
        }

        /// The wait between repeats, unkeyed.
        public var waiting: Bool { waitingMs > 0 }
    }

    public let slots: [Slot]
    public let `repeat`: Bool
    public let repeatDelaySec: Int
    /// Nil unless a message goes out or waits to repeat.
    public let sending: Sending?

    public init(slots: [Slot], repeat: Bool, repeatDelaySec: Int, sending: Sending?) {
        self.slots = slots
        self.repeat = `repeat`
        self.repeatDelaySec = repeatDelaySec
        self.sending = sending
    }

    /// Mn; nil for a number the Core does not keep.
    public func slot(_ n: Int) -> Slot? {
        slots.first { $0.n == n }
    }

    /// The message the Core sends now is this device's own: `sending.deviceId`
    /// names it, by the id the device knows for itself or the one the Core
    /// gave back for its own `voiceKeyer.send`. While it is, the device keeps
    /// its transmit keepalives going, or the Core's watchdog stops the message.
    public func isSentBy(_ deviceIds: [String?]) -> Bool {
        guard let sender = sending?.deviceId, !sender.isEmpty else {
            return false
        }
        return deviceIds.contains { $0 == sender }
    }

    /// The Core's words for Mn on the air (going out or waiting to repeat):
    /// no device may clear or load into it, its sender included. Nil when Mn
    /// is free for that.
    public func onAirReason(_ n: Int) -> String? {
        guard sending?.n == n else {
            return nil
        }
        return Self.onAirText(n)
    }

    /// "M1 is on the air.", the Core's refusal for a slot on the air.
    public static func onAirText(_ n: Int) -> String {
        "M\(n) is on the air."
    }

    private enum CodingKeys: String, CodingKey {
        case slots, `repeat`, repeatDelaySec, sending
    }

    public init(from decoder: Decoder) throws {
        let keys = try decoder.container(keyedBy: CodingKeys.self)
        slots = try keys.decode([Slot].self, forKey: .slots)
        `repeat` = try keys.decode(Bool.self, forKey: .repeat)
        repeatDelaySec = try keys.decode(Int.self, forKey: .repeatDelaySec)
        // `sending` is null when nothing goes out.
        sending = try keys.decodeIfPresent(Sending.self, forKey: .sending)
    }
}
