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
        observe: (@Sendable (String) -> Void)? = nil,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        try await run(target: target, timeout: timeout, startedAt: startedAt,
                      resolve: { target, timeout in try await resolver.resolve(for: target, timeout: timeout) },
                      observe: observe, attempt: attempt)
    }

    static func run<Value: Sendable>(
        target: URL,
        timeout: Duration,
        startedAt: ContinuousClock.Instant = ContinuousClock().now,
        now: @Sendable () -> ContinuousClock.Instant = { ContinuousClock().now },
        resolve: @Sendable (URL, Duration) async throws -> SystemProxyPlan,
        observe: (@Sendable (String) -> Void)? = nil,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        observe?("outer opening entry")
        let deadline = startedAt + timeout
        let resolutionBudget = deadline - now()
        observe?("outer resolution budget \(resolutionBudget)")
        guard resolutionBudget > .zero else {
            observe?("outer before-resolve deadline expired")
            throw SystemProxyError.timedOut
        }
        observe?("outer resolve entry")
        let plan: SystemProxyPlan
        do {
            plan = try await resolve(target, resolutionBudget)
            observe?("outer resolve returned; routes \(plan.routes.count)")
        } catch {
            observe?("outer resolve threw")
            throw error
        }
        try Task.checkCancellation()
        let afterResolution = now()
        observe?("outer after-resolve expired \(afterResolution >= deadline)")
        if afterResolution >= deadline { throw SystemProxyError.timedOut }
        var lastFailure: Error?
        for route in plan.routes {
            try Task.checkCancellation()
            let remaining = deadline - now()
            observe?("outer before-attempt budget \(remaining); direct \(route == .direct)")
            guard remaining > .zero else {
                observe?("outer before-attempt deadline expired")
                throw SystemProxyError.timedOut
            }
            do {
                observe?("outer attempt entry")
                let value = try await attempt(route, remaining)
                observe?("outer attempt returned")
                try Task.checkCancellation()
                let afterAttempt = now()
                observe?("outer after-attempt expired \(afterAttempt >= deadline)")
                if afterAttempt >= deadline { throw SystemProxyError.timedOut }
                return value
            } catch {
                observe?("outer attempt threw")
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
