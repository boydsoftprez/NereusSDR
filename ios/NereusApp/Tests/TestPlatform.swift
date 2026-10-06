// NereusSDR for iOS: a test's own lock notification, background time and idle timer for the app it builds
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import UIKit

/// One test's stand-in for what transmit reaches in iOS itself
/// (``TransmitModel/Platform``): its own notification center for the lock,
/// its own record of background time, and its own idle timer. Suites run in
/// parallel in one process; with this, a lock one test sends reaches only
/// its own app, and what a test reads was set by its own app alone.
@MainActor
final class TestPlatform {
    /// Where this test's app hears the lock.
    let center = NotificationCenter()
    /// Whether this test's app keeps the screen awake.
    private(set) var screenAwake = false
    /// The background time this test's app asked for, by name, in order.
    private(set) var backgroundWorkBegun: [String] = []
    /// Runs as each stretch of background time begins.
    var onBackgroundWorkBegin: (() -> Void)?
    /// Runs as each stretch of background time ends.
    var onBackgroundWorkEnd: (() -> Void)?

    var platform: TransmitModel.Platform {
        TransmitModel.Platform(
            notificationCenter: center,
            backgroundWork: { [weak self] name in
                self?.backgroundWorkBegun.append(name)
                self?.onBackgroundWorkBegin?()
                return { [weak self] in self?.onBackgroundWorkEnd?() }
            },
            isScreenAwake: { [weak self] in self?.screenAwake ?? false },
            setScreenAwake: { [weak self] awake in self?.screenAwake = awake })
    }

    /// Locks this test's phone: iOS's notice that protected data is going.
    func lock() {
        center.post(name: UIApplication.protectedDataWillBecomeUnavailableNotification, object: nil)
    }
}
