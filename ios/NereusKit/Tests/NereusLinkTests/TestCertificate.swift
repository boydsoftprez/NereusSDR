// NereusSDR for iOS: a self-signed certificate made at run time for the loopback Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Security

/// A P-256 key made in memory and a self-signed X.509 certificate for it,
/// written out in DER here, so no test keeps a key and nothing touches the
/// keychain.
struct TestCertificate {
    struct Failure: Error, CustomStringConvertible {
        let description: String
    }

    let identity: SecIdentity
    let der: Data

    static func make(commonName: String = "nereusd") throws -> TestCertificate {
        var error: Unmanaged<CFError>?
        let attributes: [CFString: Any] = [
            kSecAttrKeyType: kSecAttrKeyTypeECSECPrimeRandom,
            kSecAttrKeySizeInBits: 256,
        ]
        guard let privateKey = SecKeyCreateRandomKey(attributes as CFDictionary, &error),
              let publicKey = SecKeyCopyPublicKey(privateKey),
              let point = SecKeyCopyExternalRepresentation(publicKey, &error) as Data? else {
            throw Failure(description: "could not make a key: \(String(describing: error))")
        }

        let signatureAlgorithm = sequence(oid([0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02]))
        let name = sequence(set(sequence(oid([0x55, 0x04, 0x03]), tlv(0x0C, Array(commonName.utf8)))))
        let now = Date()
        let validity = sequence(utcTime(now.addingTimeInterval(-86_400)), utcTime(now.addingTimeInterval(86_400)))
        let publicKeyInfo = sequence(
            sequence(oid([0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01]),
                     oid([0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07])),
            tlv(0x03, [0x00] + Array(point)))
        var serial = (0..<8).map { _ in UInt8.random(in: 0...255) }
        serial[0] &= 0x7F
        serial[0] |= 0x01
        let tbs = sequence(
            tlv(0xA0, tlv(0x02, [0x02])),
            tlv(0x02, serial),
            signatureAlgorithm,
            name,
            validity,
            name,
            publicKeyInfo)

        guard let signature = SecKeyCreateSignature(privateKey, .ecdsaSignatureMessageX962SHA256,
                                                    Data(tbs) as CFData, &error) as Data? else {
            throw Failure(description: "could not sign: \(String(describing: error))")
        }
        let der = Data(sequence(tbs, signatureAlgorithm, tlv(0x03, [0x00] + Array(signature))))
        guard let certificate = SecCertificateCreateWithData(nil, der as CFData),
              let identity = SecIdentityCreate(nil, certificate, privateKey) else {
            throw Failure(description: "could not make the identity")
        }
        return TestCertificate(identity: identity, der: der)
    }

    // MARK: DER

    private static func tlv(_ tag: UInt8, _ content: [UInt8]) -> [UInt8] {
        [tag] + length(content.count) + content
    }

    private static func length(_ count: Int) -> [UInt8] {
        if count < 0x80 {
            return [UInt8(count)]
        }
        var bytes: [UInt8] = []
        var remaining = count
        while remaining > 0 {
            bytes.insert(UInt8(remaining & 0xFF), at: 0)
            remaining >>= 8
        }
        return [0x80 | UInt8(bytes.count)] + bytes
    }

    private static func sequence(_ parts: [UInt8]...) -> [UInt8] {
        tlv(0x30, parts.flatMap { $0 })
    }

    private static func set(_ parts: [UInt8]...) -> [UInt8] {
        tlv(0x31, parts.flatMap { $0 })
    }

    private static func oid(_ encoded: [UInt8]) -> [UInt8] {
        tlv(0x06, encoded)
    }

    private static func utcTime(_ date: Date) -> [UInt8] {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(identifier: "UTC")
        formatter.dateFormat = "yyMMddHHmmss'Z'"
        return tlv(0x17, Array(formatter.string(from: date).utf8))
    }
}
