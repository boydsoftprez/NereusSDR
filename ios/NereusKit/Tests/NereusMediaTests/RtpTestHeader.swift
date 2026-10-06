// NereusSDR for iOS: reads the RTP header of a conformance media vector for the Opus tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The RTP header fields the Opus vectors are checked against, and the
/// payload after the header (RFC 3550 section 5.1). Test support only; the
/// app's own RTP type belongs to the media library.
struct RtpTestHeader {
    let payloadType: Int
    let sequence: Int
    let timestamp: Int
    let ssrc: Int
    let payload: Data

    struct Malformed: Error, CustomStringConvertible {
        let description: String
    }

    init(_ packet: Data) throws {
        let bytes = [UInt8](packet)
        guard bytes.count >= 12 else {
            throw Malformed(description: "an RTP packet has at least 12 bytes, this has \(bytes.count)")
        }
        guard bytes[0] >> 6 == 2 else {
            throw Malformed(description: "RTP version \(bytes[0] >> 6), expected 2")
        }
        func u16(_ at: Int) -> Int {
            Int(bytes[at]) << 8 | Int(bytes[at + 1])
        }
        func u32(_ at: Int) -> Int {
            u16(at) << 16 | u16(at + 2)
        }
        let padding = bytes[0] & 0x20 != 0
        let hasExtension = bytes[0] & 0x10 != 0
        let csrcCount = Int(bytes[0] & 0x0f)
        payloadType = Int(bytes[1] & 0x7f)
        sequence = u16(2)
        timestamp = u32(4)
        ssrc = u32(8)

        var offset = 12 + 4 * csrcCount
        if hasExtension {
            guard bytes.count >= offset + 4 else {
                throw Malformed(description: "the RTP header extension runs past the packet")
            }
            offset += 4 + 4 * u16(offset + 2)
        }
        var end = bytes.count
        if padding, let last = bytes.last {
            end -= Int(last)
        }
        guard offset < end else {
            throw Malformed(description: "the RTP packet has no payload")
        }
        payload = Data(bytes[offset..<end])
    }
}
