// NereusSDR for iOS: one RTP packet on the media peer's audio line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One RTP packet (RFC 3550 section 5.1): the header fields the app uses and
/// the payload after the header, CSRC list, extension and padding.
public struct RtpPacket: Equatable, Sendable {
    public var payloadType: UInt8
    public var sequence: UInt16
    public var timestamp: UInt32
    public var ssrc: UInt32
    public var payload: Data

    /// The fixed header's size; a packet is never shorter.
    public static let headerBytes = 12

    public init(payloadType: UInt8, sequence: UInt16, timestamp: UInt32, ssrc: UInt32, payload: Data) {
        self.payloadType = payloadType
        self.sequence = sequence
        self.timestamp = timestamp
        self.ssrc = ssrc
        self.payload = payload
    }

    /// Reads an RTP packet, or returns nil when `bytes` is not one: shorter
    /// than the header, a version other than 2, an RTCP packet (payload
    /// types 72 to 79 with the marker bit, RFC 5761 section 4), or a CSRC
    /// list, extension or padding that runs past the end.
    public init?(parsing bytes: Data) {
        let raw = [UInt8](bytes)
        guard raw.count >= Self.headerBytes, raw[0] >> 6 == 2 else {
            return nil
        }
        let payloadType = raw[1] & 0x7f
        guard !(72...79).contains(payloadType) else {
            return nil
        }
        func u16(_ at: Int) -> UInt16 {
            UInt16(raw[at]) << 8 | UInt16(raw[at + 1])
        }
        func u32(_ at: Int) -> UInt32 {
            UInt32(u16(at)) << 16 | UInt32(u16(at + 2))
        }
        var offset = Self.headerBytes + 4 * Int(raw[0] & 0x0f)
        if raw[0] & 0x10 != 0 {
            guard raw.count >= offset + 4 else {
                return nil
            }
            offset += 4 + 4 * Int(u16(offset + 2))
        }
        var end = raw.count
        if raw[0] & 0x20 != 0 {
            guard let padding = raw.last, padding > 0 else {
                return nil
            }
            end -= Int(padding)
        }
        guard offset <= end else {
            return nil
        }
        self.init(payloadType: payloadType,
                  sequence: u16(2),
                  timestamp: u32(4),
                  ssrc: u32(8),
                  payload: Data(raw[offset..<end]))
    }

    /// The packet as it goes on the wire: version 2, no padding, extension,
    /// CSRCs or marker, then the payload.
    public var bytes: Data {
        var out = Data(capacity: Self.headerBytes + payload.count)
        out.append(0x80)
        out.append(payloadType & 0x7f)
        out.append(UInt8(sequence >> 8))
        out.append(UInt8(sequence & 0xff))
        for shift in stride(from: 24, through: 0, by: -8) {
            out.append(UInt8((timestamp >> UInt32(shift)) & 0xff))
        }
        for shift in stride(from: 24, through: 0, by: -8) {
            out.append(UInt8((ssrc >> UInt32(shift)) & 0xff))
        }
        out.append(payload)
        return out
    }
}
