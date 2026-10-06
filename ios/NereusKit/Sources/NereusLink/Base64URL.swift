// NereusSDR for iOS: base64url without padding, strict, as the link writes keys and signatures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Base64url without padding (link document section 3.4). Decoding is
/// strict, as the Core's is: only `A-Z a-z 0-9 - _`, no padding, no
/// whitespace, a length no whole number of bytes could not give, and the
/// unused bits of the last character zero. So each value has exactly one
/// written form.
public enum Base64URL {
    private static let alphabet = Array("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_".utf8)

    /// The value of each alphabet character, by its byte.
    private static let values: [UInt8: UInt8] = {
        var table: [UInt8: UInt8] = [:]
        for (index, character) in alphabet.enumerated() {
            table[character] = UInt8(index)
        }
        return table
    }()

    /// `data` written as base64url without padding.
    public static func encode(_ data: Data) -> String {
        var out: [UInt8] = []
        out.reserveCapacity((data.count * 4 + 2) / 3)
        let bytes = [UInt8](data)
        var index = 0
        while index + 3 <= bytes.count {
            let chunk = UInt32(bytes[index]) << 16 | UInt32(bytes[index + 1]) << 8 | UInt32(bytes[index + 2])
            out.append(alphabet[Int(chunk >> 18 & 63)])
            out.append(alphabet[Int(chunk >> 12 & 63)])
            out.append(alphabet[Int(chunk >> 6 & 63)])
            out.append(alphabet[Int(chunk & 63)])
            index += 3
        }
        let left = bytes.count - index
        if left == 1 {
            let chunk = UInt32(bytes[index]) << 16
            out.append(alphabet[Int(chunk >> 18 & 63)])
            out.append(alphabet[Int(chunk >> 12 & 63)])
        } else if left == 2 {
            let chunk = UInt32(bytes[index]) << 16 | UInt32(bytes[index + 1]) << 8
            out.append(alphabet[Int(chunk >> 18 & 63)])
            out.append(alphabet[Int(chunk >> 12 & 63)])
            out.append(alphabet[Int(chunk >> 6 & 63)])
        }
        return String(decoding: out, as: UTF8.self)
    }

    /// The bytes `text` writes, or nil when it is not strict base64url.
    public static func decode(_ text: String) -> Data? {
        let characters = Array(text.utf8)
        guard characters.count % 4 != 1 else {
            return nil
        }
        var sextets: [UInt8] = []
        sextets.reserveCapacity(characters.count)
        for character in characters {
            guard let value = values[character] else {
                return nil
            }
            sextets.append(value)
        }
        var out = Data(capacity: characters.count * 3 / 4)
        var index = 0
        while index + 4 <= sextets.count {
            let chunk = UInt32(sextets[index]) << 18 | UInt32(sextets[index + 1]) << 12
                | UInt32(sextets[index + 2]) << 6 | UInt32(sextets[index + 3])
            out.append(UInt8(chunk >> 16 & 0xFF))
            out.append(UInt8(chunk >> 8 & 0xFF))
            out.append(UInt8(chunk & 0xFF))
            index += 4
        }
        let left = sextets.count - index
        if left == 2 {
            // Twelve bits carry one byte; the last four must be zero.
            guard sextets[index + 1] & 0x0F == 0 else {
                return nil
            }
            out.append(sextets[index] << 2 | sextets[index + 1] >> 4)
        } else if left == 3 {
            // Eighteen bits carry two bytes; the last two must be zero.
            guard sextets[index + 2] & 0x03 == 0 else {
                return nil
            }
            out.append(sextets[index] << 2 | sextets[index + 1] >> 4)
            out.append((sextets[index + 1] & 0x0F) << 4 | sextets[index + 2] >> 2)
        }
        return out
    }
}
