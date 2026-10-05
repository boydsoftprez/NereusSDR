// NereusSDR for iOS: run expensive synchronous fixture crypto outside cooperative workers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Only the synchronous crypto operation runs on this owned native thread.
/// Callers check its exact result in their original SwiftTesting Task. The
/// operation keeps its own completion/cancellation behavior; an owned-run
/// watchdog fails a hang rather than inventing a result or changing a deadline.
public enum TestFixtureCrypto {
    public static func run<Value: Sendable>(
        _ operation: @escaping @Sendable () -> Value
    ) async -> Value {
        await withCheckedContinuation { continuation in
            let worker = Thread {
                continuation.resume(returning: operation())
            }
            worker.name = "NereusSDR.tests.fixture-crypto"
            worker.start()
        }
    }
}
