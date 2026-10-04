// NereusSDR: isolated diagnostic control for synchronous fixture waits
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Only assertion-free blocking operations run on this owned thread. The
/// original operation decides when it ends; cancellation neither shortens its
/// wait nor fabricates a result. A hang remains a failed owned-run watchdog.
public enum BlockingFixtureWait {
    public static func run<Value: Sendable>(
        _ label: String, diagnostic: HostedDiagnosticReceipts,
        operation: @escaping @Sendable () -> Value
    ) async -> Value {
        diagnostic.mark(label + " caller entry")
        let value = await withCheckedContinuation { continuation in
            let thread = Thread {
                diagnostic.mark(label + " dedicated thread entry")
                let result = operation()
                diagnostic.mark(label + " dedicated operation returned")
                continuation.resume(returning: result)
            }
            thread.name = "NereusSDR.test.blocking-fixture-wait"
            thread.start()
        }
        diagnostic.mark(label + " caller resumed")
        return value
    }
}
