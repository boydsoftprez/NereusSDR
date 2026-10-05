// NereusSDR for iOS: the pairing code's key exchange, SPAKE2+EE on libsodium, the device's side and the Core's
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CSodium
import CSpake2EEShim
import Foundation

/// SPAKE2+EE (link document section 3.6; D37): a device and a Core that
/// share the short code agree two keys, and each learns whether the other
/// held the same code. Whoever carries the messages learns nothing it
/// could test guesses against offline.
///
/// The steps, as spake2-ee numbers them and `pair.spake` carries them:
///
///     0  Core -> device   the password hash settings and salt (36 bytes)
///     1  device -> Core   the device's share (32 bytes), once the device has
///                         checked step 0 names exactly the fixed settings
///     2  Core -> device   the Core's share and the device's validator (64)
///     3  device -> Core   the Core's validator (32); the device fails here
///                         when the codes differ
///     4  (the Core)       checks step 3
///
/// Fixed on both ends: client identity `"nereussdr-device-v1"`, server
/// identity `"nereussdr-station-v1"` (no trailing NUL), libsodium's default
/// algorithm (Argon2id) with its interactive limits; the password is the
/// normalised code as UTF-8.
///
/// The app plays the device. The Core's role is here for the fake Core and
/// the tests: it checks that step 1 is a valid point before step 2, as the
/// Core does. Each step hashes or multiplies on the calling thread; the
/// Argon2id hash (64 MiB) belongs off any actor that answers the operator.
///
/// Secret material (the states, the stored data, the shared keys) is wiped
/// when this object goes and is never logged.
public final class SpakeExchange: @unchecked Sendable {
    public enum Role: Sendable {
        case device
        case station
    }

    public static let clientId = "nereussdr-device-v1"
    public static let serverId = "nereussdr-station-v1"
    public static let publicDataBytes = 36
    public static let response1Bytes = 32
    public static let response2Bytes = 64
    public static let response3Bytes = 32
    public static let storedBytes = 164
    public static let sharedKeyBytes = 32

    /// libsodium is initialised and usable; every step fails closed when it is not.
    public static let isAvailable: Bool = sodium_init() >= 0

    public let role: Role

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var client = crypto_spake_client_state()
    private var server = crypto_spake_server_state()
    private var keys = crypto_spake_shared_keys()
    private var step0Taken = false
    private var step1Taken = false
    private var step2Taken = false
    private var complete = false

    public init(role: Role) {
        self.role = role
    }

    deinit {
        sodium_memzero(&client, MemoryLayout<crypto_spake_client_state>.size)
        sodium_memzero(&server, MemoryLayout<crypto_spake_server_state>.size)
        sodium_memzero(&keys, MemoryLayout<crypto_spake_shared_keys>.size)
    }

    // MARK: The device

    /// True when step 0 names exactly the fixed hash settings
    /// (`crypto_spake_validate_public_data`). A Core naming weaker ones is
    /// refused before the code is hashed.
    public static func validatesPublicData(_ publicData: Data) -> Bool {
        guard isAvailable, publicData.count == publicDataBytes else {
            return false
        }
        return publicData.withUnsafeBytes { data in
            crypto_spake_validate_public_data(data.bindMemory(to: UInt8.self).baseAddress,
                                              crypto_pwhash_alg_default(),
                                              UInt64(crypto_pwhash_opslimit_interactive()),
                                              UInt64(crypto_pwhash_memlimit_interactive())) == 0
        }
    }

    /// Step 1 from the Core's step 0 and the normalised code: nil when step
    /// 0 is malformed or names other settings, or on failure. Costs one
    /// Argon2id hash.
    public func deviceStep1(publicData: Data, code: String) -> Data? {
        guard role == .device, Self.validatesPublicData(publicData), !code.isEmpty else {
            return nil
        }
        return lock.withLock {
            guard !step1Taken else {
                return nil
            }
            step1Taken = true
            var password = Array(code.utf8)
            defer { sodium_memzero(&password, password.count) }
            var response = Data(count: Self.response1Bytes)
            let result = response.withUnsafeMutableBytes { out in
                publicData.withUnsafeBytes { data in
                    password.withUnsafeBufferPointer { pw in
                        pw.withMemoryRebound(to: CChar.self) { chars in
                            crypto_spake_step1(&client, out.bindMemory(to: UInt8.self).baseAddress,
                                               data.bindMemory(to: UInt8.self).baseAddress,
                                               chars.baseAddress, UInt64(chars.count))
                        }
                    }
                }
            }
            return result == 0 ? response : nil
        }
    }

    /// Step 3 from the Core's step 2: nil when the Core did not hold the same
    /// code (or step 2 is malformed); the shared keys are agreed otherwise.
    public func deviceStep3(response2: Data) -> Data? {
        guard role == .device, response2.count == Self.response2Bytes else {
            return nil
        }
        return lock.withLock {
            guard step1Taken, !complete else {
                return nil
            }
            var agreed = crypto_spake_shared_keys()
            defer { sodium_memzero(&agreed, MemoryLayout<crypto_spake_shared_keys>.size) }
            var response = Data(count: Self.response3Bytes)
            let result = response.withUnsafeMutableBytes { out in
                response2.withUnsafeBytes { data in
                    Self.clientId.withCString { cid in
                        Self.serverId.withCString { sid in
                            crypto_spake_step3(&client, out.bindMemory(to: UInt8.self).baseAddress, &agreed,
                                               cid, Self.clientId.utf8.count, sid, Self.serverId.utf8.count,
                                               data.bindMemory(to: UInt8.self).baseAddress)
                        }
                    }
                }
            }
            guard result == 0 else {
                return nil
            }
            keys = agreed
            complete = true
            return response
        }
    }

    // MARK: The Core, for the fake Core and the tests

    /// What a Core keeps for one code: its password hash with a fresh salt
    /// (`crypto_spake_server_store`, the fixed settings). Nil on failure.
    /// Costs one Argon2id hash.
    public static func storedData(code: String) -> Data? {
        guard isAvailable, !code.isEmpty else {
            return nil
        }
        var password = Array(code.utf8)
        defer { sodium_memzero(&password, password.count) }
        var stored = Data(count: storedBytes)
        let result = stored.withUnsafeMutableBytes { out in
            password.withUnsafeBufferPointer { pw in
                pw.withMemoryRebound(to: CChar.self) { chars in
                    crypto_spake_server_store(out.bindMemory(to: UInt8.self).baseAddress, chars.baseAddress,
                                              UInt64(chars.count), UInt64(crypto_pwhash_opslimit_interactive()),
                                              crypto_pwhash_memlimit_interactive())
                }
            }
        }
        return result == 0 ? stored : nil
    }

    /// Step 0 from `stored`; nil on failure.
    public func stationStep0(stored: Data) -> Data? {
        guard role == .station, stored.count == Self.storedBytes else {
            return nil
        }
        return lock.withLock {
            guard !step0Taken else {
                return nil
            }
            var publicData = Data(count: Self.publicDataBytes)
            let result = publicData.withUnsafeMutableBytes { out in
                stored.withUnsafeBytes { data in
                    crypto_spake_step0(&server, out.bindMemory(to: UInt8.self).baseAddress,
                                       data.bindMemory(to: UInt8.self).baseAddress)
                }
            }
            guard result == 0 else {
                return nil
            }
            step0Taken = true
            return publicData
        }
    }

    /// Step 2, answering the device's step 1: nil unless step 1 is a valid
    /// point (`crypto_core_ed25519_is_valid_point`: canonical, on the curve,
    /// in the main subgroup, not of small order), as the Core checks it. One
    /// share per exchange, whatever the outcome.
    public func stationStep2(stored: Data, response1: Data) -> Data? {
        guard role == .station, stored.count == Self.storedBytes, response1.count == Self.response1Bytes else {
            return nil
        }
        return lock.withLock {
            guard step0Taken, !step2Taken else {
                return nil
            }
            step2Taken = true
            let valid = response1.withUnsafeBytes { data in
                crypto_core_ed25519_is_valid_point(data.bindMemory(to: UInt8.self).baseAddress!) == 1
            }
            guard valid else {
                return nil
            }
            var response = Data(count: Self.response2Bytes)
            let result = response.withUnsafeMutableBytes { out in
                stored.withUnsafeBytes { storedData in
                    response1.withUnsafeBytes { data in
                        Self.clientId.withCString { cid in
                            Self.serverId.withCString { sid in
                                crypto_spake_step2(&server, out.bindMemory(to: UInt8.self).baseAddress,
                                                   cid, Self.clientId.utf8.count, sid, Self.serverId.utf8.count,
                                                   storedData.bindMemory(to: UInt8.self).baseAddress,
                                                   data.bindMemory(to: UInt8.self).baseAddress)
                            }
                        }
                    }
                }
            }
            return result == 0 ? response : nil
        }
    }

    /// Step 4: true when the device's step 3 shows it held the same code;
    /// the shared keys are then agreed.
    public func stationStep4(response3: Data) -> Bool {
        guard role == .station, response3.count == Self.response3Bytes else {
            return false
        }
        return lock.withLock {
            guard step2Taken, !complete else {
                return false
            }
            var agreed = crypto_spake_shared_keys()
            defer { sodium_memzero(&agreed, MemoryLayout<crypto_spake_shared_keys>.size) }
            let result = response3.withUnsafeBytes { data in
                crypto_spake_step4(&server, &agreed, data.bindMemory(to: UInt8.self).baseAddress)
            }
            guard result == 0 else {
                return false
            }
            keys = agreed
            complete = true
            return true
        }
    }

    // MARK: Both

    /// The shared keys are agreed (device step 3, Core step 4).
    public var isComplete: Bool { lock.withLock { complete } }

    /// This side's confirmation box around `plaintext`: the device's under
    /// `client_sk`, the Core's under `server_sk`. Nil before the keys are agreed.
    public func seal(_ plaintext: Data) -> Data? {
        withKey(own: true) { PairingBox.seal(plaintext, key: $0) }
    }

    /// The other side's box opened: the device opens the Core's under
    /// `server_sk`, the Core the device's under `client_sk`. Nil when it
    /// does not open.
    public func open(_ box: Data) -> Data? {
        withKey(own: false) { PairingBox.open(box, key: $0) }
    }

    /// Runs `body` over the shared key in place, under the lock, so the
    /// key is never copied out of this object; nil before the keys are agreed.
    private func withKey(own: Bool, _ body: (UnsafePointer<UInt8>) -> Data?) -> Data? {
        lock.withLock {
            guard complete else {
                return nil
            }
            let clientSide = (role == .device) == own
            if clientSide {
                return withUnsafeBytes(of: &keys.client_sk) { body($0.bindMemory(to: UInt8.self).baseAddress!) }
            }
            return withUnsafeBytes(of: &keys.server_sk) { body($0.bindMemory(to: UInt8.self).baseAddress!) }
        }
    }
}
