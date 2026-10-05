// NereusSDR for iOS: the Core's certificate pin, the SHA-256 of its certificate
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// The pin is the SHA-256 digest of the Core's whole certificate in DER
/// form (link document section 3.2). It is written as 32 uppercase
/// hexadecimal pairs joined by colons, 95 characters, and compared without
/// regard to case.
public enum CertificatePin {
    /// The digest of a certificate in DER form.
    public static func sha256(ofCertificate der: Data) -> Data {
        Data(SHA256.hash(data: der))
    }

    /// True when `certificateSHA256` is the pinned digest.
    public static func matches(_ certificateSHA256: Data, pin: Data) -> Bool {
        pin.count == 32 && certificateSHA256 == pin
    }

    /// The digest a written fingerprint names (`AB:12:...:FF`, any case),
    /// or nil when it is not one.
    public static func parse(_ fingerprint: String) -> Data? {
        let pairs = fingerprint.split(separator: ":", omittingEmptySubsequences: false)
        guard pairs.count == 32 else {
            return nil
        }
        var digest = Data(capacity: 32)
        for pair in pairs {
            guard pair.count == 2, pair.allSatisfy(\.isHexDigit), let byte = UInt8(pair, radix: 16) else {
                return nil
            }
            digest.append(byte)
        }
        return digest
    }

    /// The written form of a digest, as the Core prints it.
    public static func format(_ digest: Data) -> String {
        digest.map { String(format: "%02X", $0) }.joined(separator: ":")
    }
}
