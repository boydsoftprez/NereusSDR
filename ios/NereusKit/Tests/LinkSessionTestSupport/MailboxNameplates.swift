// NereusSDR for iOS: mailbox numbers for tests that no other pairing in the process can hold
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The numbers the tests pair through the service's mailbox on.
///
/// ``PairingClient`` runs one pairing per mailbox number per process, and
/// every test in the package runs in one process, many at once. A test that
/// picked its number at random, or derived it from another code, could land
/// on a number another test's pairing holds and be refused with
/// `alreadyPairing`. So a Core's shown number comes from ``coreRange`` (only
/// the fake Core shows one), and every other test's number comes from
/// ``unshown()``: above that range, and never the same twice in the process.
public enum MailboxNameplates {
    /// The numbers a Core shows: 1 to 99.
    public static let coreRange: ClosedRange<Int> = 1...99

    private static let lock = NSLock()
    nonisolated(unsafe) private static var next = coreRange.upperBound + 1

    /// A number no Core in the process shows and no earlier call gave.
    public static func unshown() -> Int {
        lock.withLock {
            precondition(next <= PairingCodeText.maxNameplate, "the tests ran out of mailbox numbers")
            defer { next += 1 }
            return next
        }
    }
}
