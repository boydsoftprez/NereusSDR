// NereusSDR for iOS: joins the chunks the control data channel receives, and reads its heartbeat
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The receiving side of ``ControlChannelFraming`` (link document section
/// 20): joins one connection's chunks in order into session messages and
/// reads pings and pongs, which may arrive between two chunks without
/// disturbing the message being joined.
///
/// The connection ends (``Result/refused(_:)``) the moment the bytes joined
/// for one message pass the inbound cap, without waiting for the last chunk,
/// as the WebSocket's cap ends it; and on an empty data-channel message, a
/// chunk with no piece of a message, a first byte it does not know, a
/// message longer than ``ControlChannelFraming/chunkBytes``, and a ping or
/// pong that is not ``ControlChannelFraming/heartbeatBytes`` long. Once it
/// has refused, it refuses everything.
public struct ControlChannelReassembler: Sendable {
    public enum Result: Sendable, Equatable {
        /// A chunk that does not end its message.
        case pending
        /// One whole session message, its UTF-8 bytes.
        case message(Data)
        /// A ping with this id.
        case ping(UInt32)
        /// A pong answering the ping with this id.
        case pong(UInt32)
        /// The connection ends. The words are for the log only.
        case refused(String)
    }

    /// The inbound cap: 8 MiB on the app (link document section 12.3).
    public let maxMessageBytes: Int
    private var joined = Data()
    private var refusedWhy: String?

    public init(maxMessageBytes: Int) {
        self.maxMessageBytes = maxMessageBytes
    }

    /// Bytes of the message being joined.
    public var pendingBytes: Int { joined.count }

    /// One binary data-channel message.
    public mutating func feed(_ frame: Data) -> Result {
        if let refusedWhy {
            return .refused(refusedWhy)
        }
        guard let kind = frame.first else {
            return refuse("an empty message on the control channel")
        }
        guard frame.count <= ControlChannelFraming.chunkBytes else {
            return refuse("a control channel message longer than \(ControlChannelFraming.chunkBytes) bytes")
        }
        switch kind {
        case ControlChannelFraming.ping, ControlChannelFraming.pong:
            guard frame.count == ControlChannelFraming.heartbeatBytes else {
                return refuse("a ping or pong that is not \(ControlChannelFraming.heartbeatBytes) bytes")
            }
            let id = frame.dropFirst().reduce(UInt32(0)) { ($0 << 8) | UInt32($1) }
            return kind == ControlChannelFraming.ping ? .ping(id) : .pong(id)
        case ControlChannelFraming.more, ControlChannelFraming.last:
            guard frame.count > 1 else {
                return refuse("a chunk with nothing in it")
            }
            guard joined.count + frame.count - 1 <= maxMessageBytes else {
                return refuse("a message larger than \(maxMessageBytes) bytes")
            }
            joined.append(frame.dropFirst())
            guard kind == ControlChannelFraming.last else {
                return .pending
            }
            let message = joined
            joined = Data()
            return .message(message)
        default:
            return refuse("a control channel message of an unknown kind")
        }
    }

    /// A text data-channel message, which the control channel never
    /// carries: the connection ends.
    public mutating func textMessage() -> Result {
        refuse("a text message on the control channel")
    }

    private mutating func refuse(_ why: String) -> Result {
        refusedWhy = refusedWhy ?? why
        joined = Data()
        return .refused(refusedWhy ?? why)
    }
}
