// NereusSDR for iOS: one continuation resumed by whichever of several answers comes first
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Resumes one continuation with the first answer given; later ones are dropped.
final class FirstAnswer<Value: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Value, Never>?
    private var early: Value?
    private var done = false

    func wait(_ continuation: CheckedContinuation<Value, Never>) {
        let ready: Value? = lock.withLock {
            if let early {
                done = true
                return early
            }
            self.continuation = continuation
            return nil
        }
        if let ready {
            continuation.resume(returning: ready)
        }
    }

    func answer(_ value: Value) {
        let waiting: CheckedContinuation<Value, Never>? = lock.withLock {
            guard !done else {
                return nil
            }
            guard let continuation else {
                if early == nil {
                    early = value
                }
                return nil
            }
            done = true
            self.continuation = nil
            return continuation
        }
        waiting?.resume(returning: value)
    }
}
