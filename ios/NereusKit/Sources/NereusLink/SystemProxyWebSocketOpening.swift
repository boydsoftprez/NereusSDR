// NereusSDR for iOS: one bounded opening through ordered system proxy choices.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

// Diagnostic-only receipts for the isolated refusal investigation. The caller
// supplies a bounded observer; the shipping/default opening has none.
struct WebSocketOpeningReceipt: Sendable {
    let phase: String
    var budget: Duration? = nil
    var startedAt: ContinuousClock.Instant? = nil
    var reason: String? = nil
    var code: Int32? = nil

    static func reason(for error: any Error) -> String {
        if let error = error as? SystemProxyError {
            switch error {
            case .invalidTarget: return "proxy.invalidTarget"
            case .noUsableRoute: return "proxy.noUsableRoute"
            case .pacFailed: return "proxy.pacFailed"
            case .timedOut: return "proxy.timedOut"
            case .cancelled: return "proxy.cancelled"
            }
        }
        if let error = error as? LinkTransportError {
            switch error {
            case .certificateMismatch: return "link.certificateMismatch"
            case .localNetworkDenied: return "link.localNetworkDenied"
            case .refused: return "link.refused"
            case .unreachable: return "link.unreachable"
            case .failed: return "link.failed"
            }
        }
        if error is CancellationError { return "cancelled" }
        return "other"
    }

    static func route(_ route: SystemProxyRoute) -> String {
        switch route {
        case .direct: return "direct"
        case .httpCONNECT: return "httpCONNECT"
        case .socks5: return "socks5"
        }
    }
}

typealias WebSocketOpeningObserver = @Sendable (WebSocketOpeningReceipt) -> Void

/// The resolver reads system settings once per opening. A failed choice is
/// closed by `attempt` before it throws; only choices in the plan are tried.
enum SystemProxyWebSocketOpening {
    static func run<Value: Sendable>(
        target: URL,
        timeout: Duration,
        startedAt: ContinuousClock.Instant = ContinuousClock().now,
        resolver: SystemProxyResolver,
        observer: WebSocketOpeningObserver? = nil,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        try await run(target: target, timeout: timeout, startedAt: startedAt,
                      resolve: { target, timeout in try await resolver.resolve(for: target, timeout: timeout) },
                      observer: observer, attempt: attempt)
    }

    static func run<Value: Sendable>(
        target: URL,
        timeout: Duration,
        startedAt: ContinuousClock.Instant = ContinuousClock().now,
        now: @Sendable () -> ContinuousClock.Instant = { ContinuousClock().now },
        resolve: @Sendable (URL, Duration) async throws -> SystemProxyPlan,
        observer: WebSocketOpeningObserver? = nil,
        attempt: @Sendable (SystemProxyRoute, Duration) async throws -> Value
    ) async throws -> Value {
        let deadline = startedAt + timeout
        let resolutionBudget = deadline - now()
        observer?(.init(phase: "policy.entry", budget: resolutionBudget, startedAt: startedAt))
        guard resolutionBudget > .zero else {
            observer?(.init(phase: "policy.timeout.entry", budget: resolutionBudget))
            throw SystemProxyError.timedOut
        }
        observer?(.init(phase: "policy.resolve.before", budget: resolutionBudget))
        let plan = try await resolve(target, resolutionBudget)
        observer?(.init(phase: "policy.resolve.after"))
        try Task.checkCancellation()
        if now() >= deadline {
            observer?(.init(phase: "policy.timeout.afterResolve"))
            throw SystemProxyError.timedOut
        }
        var lastFailure: Error?
        for route in plan.routes {
            try Task.checkCancellation()
            let remaining = deadline - now()
            guard remaining > .zero else {
                observer?(.init(phase: "policy.timeout.beforeAttempt", budget: remaining))
                throw SystemProxyError.timedOut
            }
            do {
                observer?(.init(phase: "policy.attempt.before", budget: remaining,
                                reason: WebSocketOpeningReceipt.route(route)))
                let value = try await attempt(route, remaining)
                observer?(.init(phase: "policy.attempt.success"))
                try Task.checkCancellation()
                if now() >= deadline {
                    observer?(.init(phase: "policy.timeout.afterSuccess"))
                    throw SystemProxyError.timedOut
                }
                return value
            } catch {
                observer?(.init(phase: "policy.attempt.error", reason: WebSocketOpeningReceipt.reason(for: error)))
                if Task.isCancelled || error is CancellationError || error as? SystemProxyError == .cancelled {
                    throw error
                }
                if error as? LinkTransportError == .certificateMismatch {
                    throw error
                }
                lastFailure = error
            }
        }
        observer?(.init(phase: "policy.exhausted", reason: WebSocketOpeningReceipt.reason(
            for: lastFailure ?? SystemProxyError.noUsableRoute)))
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
