// NereusSDR for iOS: one bounded opening through ordered system proxy choices.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The resolver reads system settings once per opening. A failed choice is
/// closed by `attempt` before it throws; only choices in the plan are tried.
enum SystemProxyWebSocketOpening {
    static func run<Value: Sendable>(
        target: URL,
        timeout: Duration,
        startedAt: ContinuousClock.Instant = ContinuousClock().now,
        resolver: SystemProxyResolver,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        try await run(target: target, timeout: timeout, startedAt: startedAt,
                      resolve: { target, timeout in try await resolver.resolve(for: target, timeout: timeout) },
                      attempt: attempt)
    }

    static func run<Value: Sendable>(
        target: URL,
        timeout: Duration,
        startedAt: ContinuousClock.Instant = ContinuousClock().now,
        now: @Sendable () -> ContinuousClock.Instant = { ContinuousClock().now },
        resolve: @Sendable (URL, Duration) async throws -> SystemProxyPlan,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        let deadline = startedAt + timeout
        let resolutionBudget = deadline - now()
        guard resolutionBudget > .zero else { throw SystemProxyError.timedOut }
        let plan = try await resolve(target, resolutionBudget)
        try Task.checkCancellation()
        if now() >= deadline { throw SystemProxyError.timedOut }
        var lastFailure: Error?
        for route in plan.routes {
            try Task.checkCancellation()
            let remaining = deadline - now()
            guard remaining > .zero else { throw SystemProxyError.timedOut }
            do {
                let value = try await attempt(route, remaining)
                try Task.checkCancellation()
                if now() >= deadline { throw SystemProxyError.timedOut }
                return value
            } catch {
                if Task.isCancelled || error is CancellationError || error as? SystemProxyError == .cancelled {
                    throw error
                }
                if error as? LinkTransportError == .certificateMismatch {
                    throw error
                }
                lastFailure = error
            }
        }
        throw lastFailure ?? SystemProxyError.noUsableRoute
    }
}

extension SystemProxyError {
    var openingFailureText: String {
        switch self {
        case .invalidTarget: "not a WebSocket address"
        case .noUsableRoute: "the system proxy settings offered no usable route"
        case .pacFailed: "the system proxy script did not provide a usable route"
        case .timedOut: "no reply before the connection deadline, including proxy lookup"
        case .cancelled: "the proxy lookup was cancelled"
        }
    }
}
