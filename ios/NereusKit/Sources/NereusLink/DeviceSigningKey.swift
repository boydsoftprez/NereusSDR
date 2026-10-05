// NereusSDR for iOS: the device's P-256 private key, in the Secure Enclave or in software
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// The device's own P-256 key (link document section 3.5). The private
/// half never leaves it: it signs, and gives out only its public key.
public protocol DeviceSigningKey: Sendable {
    /// The public key as its 91-byte SubjectPublicKeyInfo DER.
    var publicKey: Data { get }
    /// ECDSA P-256 over SHA-256 of `message`, raw `r || s`, 64 bytes.
    func signature(for message: Data) throws -> Data
}

/// A key held in software, where the device has no Secure Enclave, and in tests.
struct SoftwareSigningKey: DeviceSigningKey {
    let key: P256.Signing.PrivateKey

    var publicKey: Data { key.publicKey.derRepresentation }

    func signature(for message: Data) throws -> Data {
        try key.signature(for: message).rawRepresentation
    }
}

/// A key held in the Secure Enclave; the app keeps only its opaque handle.
struct SecureEnclaveSigningKey: DeviceSigningKey, @unchecked Sendable {
    let key: SecureEnclave.P256.Signing.PrivateKey

    var publicKey: Data { key.publicKey.derRepresentation }

    func signature(for message: Data) throws -> Data {
        try key.signature(for: message).rawRepresentation
    }
}
