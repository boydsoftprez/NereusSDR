// NereusSDR for iOS: the bytes of the control data channel: chunks, ping and pong
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// How session messages and the heartbeat travel on the control data
/// channel (link document section 20, "Control over a data channel";
/// iPhone app plan Task 28a, R-IOS-16). Every data-channel message is
/// binary:
///
/// - a chunk: ``more`` or ``last``, then the next piece of one session
///   message's UTF-8 bytes, at least one byte. A sender fills every chunk
///   but the last with ``chunkPayloadBytes``, so no chunk is longer than
///   ``chunkBytes``;
/// - a ping: ``ping`` and a 4-byte big-endian id, the first 1 and then one
///   more each time; a pong: ``pong`` and the id of the ping it answers.
///
/// ``ControlChannelReassembler`` is the receiving side.
public enum ControlChannelFraming {
    /// The longest data-channel message the control channel carries
    /// (`controlChannelChunkBytes` in the link's surface).
    public static let chunkBytes = 61_440
    /// The most of a session message one chunk carries.
    public static let chunkPayloadBytes = chunkBytes - 1
    /// First byte of a chunk that more of its message follows.
    public static let more: UInt8 = 0x01
    /// First byte of the chunk that ends its message.
    public static let last: UInt8 = 0x02
    /// First byte of a ping.
    public static let ping: UInt8 = 0x10
    /// First byte of a pong.
    public static let pong: UInt8 = 0x11
    /// A ping or a pong: its first byte and a 4-byte id.
    public static let heartbeatBytes = 5

    /// The chunks one session message travels as, in order: ceil(n / 61439)
    /// of them for n bytes. None for an empty message, which a session
    /// never sends.
    public static func chunks(of message: Data) -> [Data] {
        var chunks: [Data] = []
        chunks.reserveCapacity((message.count + chunkPayloadBytes - 1) / chunkPayloadBytes)
        var offset = message.startIndex
        while offset < message.endIndex {
            let end = message.index(offset, offsetBy: chunkPayloadBytes, limitedBy: message.endIndex)
                ?? message.endIndex
            var chunk = Data(capacity: end - offset + 1)
            chunk.append(end == message.endIndex ? last : more)
            chunk.append(message[offset..<end])
            chunks.append(chunk)
            offset = end
        }
        return chunks
    }

    /// A ping with `id`.
    public static func ping(id: UInt32) -> Data {
        heartbeat(ping, id: id)
    }

    /// The pong answering the ping with `id`.
    public static func pong(id: UInt32) -> Data {
        heartbeat(pong, id: id)
    }

    private static func heartbeat(_ kind: UInt8, id: UInt32) -> Data {
        Data([kind, UInt8(truncatingIfNeeded: id >> 24), UInt8(truncatingIfNeeded: id >> 16),
              UInt8(truncatingIfNeeded: id >> 8), UInt8(truncatingIfNeeded: id)])
    }
}
