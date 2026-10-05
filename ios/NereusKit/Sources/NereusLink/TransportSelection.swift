// NereusSDR for iOS: cancellation gate for an asynchronous route selection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A route racer may return after the session has ended. Cancellation wakes
/// the session immediately; a transport returned afterward is closed here.
final class TransportSelection: @unchecked Sendable {
    private let lock = NSLock()
    private var result: Result<any SessionTransport, Error>?
    private var waiter: CheckedContinuation<any SessionTransport, Error>?
    private var resolved = false

    func value() async throws -> any SessionTransport {
        try await withCheckedThrowingContinuation { continuation in
            if let result = registerWaiter(continuation) {
                continuation.resume(with: result)
            }
        }
    }

    // Read a resolved result or register the waiter in one critical section.
    // The caller resumes only after this synchronous method has unlocked.
    private func registerWaiter(_ continuation: CheckedContinuation<any SessionTransport, Error>)
        -> Result<any SessionTransport, Error>? {
        lock.lock()
        defer { lock.unlock() }
        if let result { return result }
        waiter = continuation
        return nil
    }

    func complete(_ transport: any SessionTransport) {
        let (accepted, recipient) = lock.withLock { () -> (Bool, CheckedContinuation<any SessionTransport, Error>?) in
            guard !resolved else { return (false, nil) }
            resolved = true
            result = .success(transport)
            let taken = waiter
            waiter = nil
            return (true, taken)
        }
        if accepted {
            recipient?.resume(returning: transport)
        } else {
            transport.close()
        }
    }

    func fail(_ error: Error) {
        let recipient = lock.withLock { () -> CheckedContinuation<any SessionTransport, Error>? in
            guard !resolved else { return nil }
            resolved = true
            result = .failure(error)
            let taken = waiter
            waiter = nil
            return taken
        }
        recipient?.resume(throwing: error)
    }

    func cancel() {
        let (recipient, unclaimed) = lock.withLock { () -> (CheckedContinuation<any SessionTransport, Error>?,
                                                           (any SessionTransport)?) in
            guard !resolved else {
                if case .success(let transport) = result {
                    result = .failure(CancellationError())
                    return (nil, transport)
                }
                return (nil, nil)
            }
            resolved = true
            result = .failure(CancellationError())
            let taken = waiter
            waiter = nil
            return (taken, nil)
        }
        recipient?.resume(throwing: CancellationError())
        unclaimed?.close()
    }
}
