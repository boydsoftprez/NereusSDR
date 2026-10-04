// NereusSDR for iOS: why the device's key could not be had
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Why the device's key could not be had.
public enum DeviceKeyError: Error, Equatable {
    /// What is stored is not a key this app can use, or the Keychain
    /// refused to read it for a reason other than the phone not being
    /// unlocked yet. It is never replaced silently, because a new key is a
    /// new device to every paired Core: only
    /// ``DeviceIdentity/makeNewKey(store:)`` replaces it.
    case unreadableKey
    /// The key could not be made or kept, or could not be reached for now:
    /// the Secure Enclave refused, the Keychain refused to add or replace
    /// it, or the Keychain is not readable yet because the phone has not
    /// been unlocked since it started. Trying again later may work; nothing
    /// stored was changed.
    case couldNotCreate
}
