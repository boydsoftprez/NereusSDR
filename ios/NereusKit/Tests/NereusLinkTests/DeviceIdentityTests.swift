// NereusSDR for iOS: tests for the device key, made once and kept, in each of its stores
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import Testing
@testable import NereusLink

/// True when this process may use the data protection Keychain. A package
/// test run has no Keychain entitlement, on a Mac or on the simulator, so
/// the tests gated on this are off there; the app's own test bundle runs
/// the same checks, ungated, inside the app (RealKeychainTests). Asked of
/// Security itself, never through ``KeychainItem``, the code these tests
/// check: a fault there can never turn them off.
let keychainIsUsable: Bool = {
    let item: [String: Any] = [
        kSecClass as String: kSecClassGenericPassword,
        kSecAttrService as String: "NereusSDR tests",
        kSecAttrAccount as String: "probe-\(UUID().uuidString)",
        kSecUseDataProtectionKeychain as String: true,
    ]
    var added = item
    added[kSecValueData as String] = Data([1])
    added[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
    guard SecItemAdd(added as CFDictionary, nil) == errSecSuccess else {
        return false
    }
    return SecItemDelete(item as CFDictionary) == errSecSuccess
}()

/// True when this machine has a Secure Enclave this process can make a key in.
let secureEnclaveIsUsable: Bool = {
    guard SecureEnclave.isAvailable else {
        return false
    }
    return (try? SecureEnclave.P256.Signing.PrivateKey()) != nil
}()

@Suite struct DeviceIdentityTests {
    private static let message = Data("a sign-in transcript".utf8)

    /// The key `store` gives verifies its own signature, and is a canonical link key.
    private static func check(_ identity: DeviceIdentity) throws {
        #expect(identity.publicKey.count == 91)
        #expect(P256Wire.isCanonicalKey(identity.publicKey))
        #expect(identity.id == P256Wire.deviceId(spki: identity.publicKey))
        #expect(identity.id.count == 43)
        let signature = try identity.signature(for: message)
        #expect(signature.count == 64)
        #expect(P256Wire.verify(signature: signature, over: message, spki: identity.publicKey))
    }

    @Test func theInMemoryStoreMakesItsKeyOnce() throws {
        let store = InMemoryKeyStore()
        let first = try DeviceIdentity.load(store: store)
        let second = try DeviceIdentity.load(store: store)
        try Self.check(first)
        #expect(first.publicKey == second.publicKey)
        #expect(first.id == second.id)
        #expect(try DeviceIdentity.load(store: InMemoryKeyStore()).publicKey != first.publicKey)
    }

    @Test func theKeychainStoreKeepsItsKeyForANewStoreObject() throws {
        let item = InMemorySecretItem()
        let first = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        try Self.check(first)
        let again = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        #expect(again.publicKey == first.publicKey)
        // The key found again signs as the first did.
        let signature = try again.signature(for: Self.message)
        #expect(P256Wire.verify(signature: signature, over: Self.message, spki: first.publicKey))
    }

    @Test func aKeyStoredFirstByAnotherCallerIsTheOneUsed() throws {
        let item = InMemorySecretItem()
        let winner = P256.Signing.PrivateKey()
        #expect(try item.add(winner.rawRepresentation))
        let identity = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        #expect(identity.publicKey == winner.publicKey.derRepresentation)
    }

    @Test func anUnreadableStoredKeyIsNeverReplaced() throws {
        let garbage = Data([1, 2, 3])
        let item = InMemorySecretItem(garbage)
        #expect(throws: DeviceKeyError.unreadableKey) {
            try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        }
        #expect(try item.read() == garbage)
        #expect(throws: DeviceKeyError.unreadableKey) {
            try DeviceIdentity.load(store: SecureEnclaveKeyStore(item: item))
        }
        #expect(try item.read() == garbage)
    }

    @Test func makingANewKeyReplacesAnUnreadableOneAndOnlyThen() throws {
        let item = InMemorySecretItem(Data([1, 2, 3]))
        let store = KeychainKeyStore(item: item)
        #expect(throws: DeviceKeyError.unreadableKey) { try DeviceIdentity.load(store: store) }
        let made = try DeviceIdentity.makeNewKey(store: store)
        try Self.check(made)
        // The new key is the one kept: every later load finds it.
        #expect(try DeviceIdentity.load(store: store).publicKey == made.publicKey)
        #expect(try DeviceIdentity.load(store: KeychainKeyStore(item: item)).publicKey == made.publicKey)
    }

    @Test func makingANewKeyGivesAnotherDevice() throws {
        let store = InMemoryKeyStore()
        let first = try DeviceIdentity.load(store: store)
        let made = try DeviceIdentity.makeNewKey(store: store)
        #expect(made.publicKey != first.publicKey)
        #expect(made.id != first.id)
        #expect(try DeviceIdentity.load(store: store).publicKey == made.publicKey)
    }

    @Test(.enabled(if: secureEnclaveIsUsable, "this machine has no Secure Enclave this process can use"))
    func theSecureEnclaveStoreMakesANewKeyInPlaceOfAnUnreadableOne() throws {
        let item = InMemorySecretItem(Data([1, 2, 3]))
        let store = SecureEnclaveKeyStore(item: item)
        #expect(throws: DeviceKeyError.unreadableKey) { try DeviceIdentity.load(store: store) }
        let made = try DeviceIdentity.makeNewKey(store: store)
        try Self.check(made)
        #expect(try DeviceIdentity.load(store: store).publicKey == made.publicKey)
    }

    /// A secret item whose Keychain refuses with `status`.
    private final class RefusingItem: SecretItem, @unchecked Sendable {
        let readStatus: OSStatus?
        let writeStatus: OSStatus?

        init(read: OSStatus? = nil, write: OSStatus? = nil) {
            readStatus = read
            writeStatus = write
        }

        func read() throws -> Data? {
            if let readStatus {
                throw KeychainError(status: readStatus)
            }
            return nil
        }

        func add(_ data: Data) throws -> Bool {
            if let writeStatus {
                throw KeychainError(status: writeStatus)
            }
            return true
        }

        func write(_ data: Data) throws {
            if let writeStatus {
                throw KeychainError(status: writeStatus)
            }
        }

        func delete() throws {}
    }

    @Test func theKeychainsRefusalsArriveAsTheKeysOwnErrors() {
        // Not unlocked since the phone started: a wait, never a new key.
        #expect(throws: DeviceKeyError.couldNotCreate) {
            try DeviceIdentity.load(store: KeychainKeyStore(item: RefusingItem(read: errSecInteractionNotAllowed)))
        }
        #expect(throws: DeviceKeyError.couldNotCreate) {
            try DeviceIdentity.load(store: SecureEnclaveKeyStore(item: RefusingItem(read: errSecInteractionNotAllowed)))
        }
        // Any other refused read: the stored key can't be read.
        #expect(throws: DeviceKeyError.unreadableKey) {
            try DeviceIdentity.load(store: KeychainKeyStore(item: RefusingItem(read: errSecDecode)))
        }
        // A refused add or replace: the key could not be kept.
        #expect(throws: DeviceKeyError.couldNotCreate) {
            try DeviceIdentity.load(store: KeychainKeyStore(item: RefusingItem(write: errSecNotAvailable)))
        }
        #expect(throws: DeviceKeyError.couldNotCreate) {
            try DeviceIdentity.makeNewKey(store: KeychainKeyStore(item: RefusingItem(write: errSecNotAvailable)))
        }
    }

    @Test(.enabled(if: keychainIsUsable, "the data protection Keychain is not available to this process"))
    func theRealKeychainKeepsTheKey() throws {
        let item = KeychainItem(service: "NereusSDR tests", account: "device-key-\(UUID().uuidString)")
        defer { try? item.delete() }
        let first = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        let again = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        try Self.check(again)
        #expect(again.publicKey == first.publicKey)
    }

    @Test(.enabled(if: secureEnclaveIsUsable, "this machine has no Secure Enclave this process can use"))
    func theSecureEnclaveStoreKeepsItsKeyForANewStoreObject() throws {
        let item = InMemorySecretItem()
        let first = try DeviceIdentity.load(store: SecureEnclaveKeyStore(item: item))
        try Self.check(first)
        let again = try DeviceIdentity.load(store: SecureEnclaveKeyStore(item: item))
        #expect(again.publicKey == first.publicKey)
        let signature = try again.signature(for: Self.message)
        #expect(P256Wire.verify(signature: signature, over: Self.message, spki: first.publicKey))
    }

    @Test func theStandardStoreFollowsTheSecureEnclave() {
        let store = DeviceIdentity.standardStore()
        if SecureEnclave.isAvailable {
            #expect(store is SecureEnclaveKeyStore)
        } else {
            #expect(store is KeychainKeyStore)
        }
    }

    @Test func keysAreHeldToTheCanonicalForm() {
        let key = P256.Signing.PrivateKey().publicKey
        #expect(P256Wire.isCanonicalKey(key.derRepresentation))
        #expect(!P256Wire.isCanonicalKey(key.derRepresentation.dropLast()))
        #expect(!P256Wire.isCanonicalKey(key.derRepresentation + Data([0])))
        #expect(!P256Wire.isCanonicalKey(key.x963Representation))
        // A point that is not on the curve.
        var offCurve = key.derRepresentation
        offCurve[offCurve.count - 1] ^= 0x01
        #expect(!P256Wire.isCanonicalKey(offCurve))
        // Another curve's key.
        #expect(!P256Wire.isCanonicalKey(P384.Signing.PrivateKey().publicKey.derRepresentation))
    }
}
