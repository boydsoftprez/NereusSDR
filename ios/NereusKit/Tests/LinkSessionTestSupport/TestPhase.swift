// NereusSDR for iOS: finite acknowledgements for asynchronous test phases
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One acknowledgement, with a failing backstop rather than an unbounded
/// continuation or a task group that must join an uncooperative child.
/// Closure and cancellation settle all waiters; a late entry cannot pass.
public final class TestPhase<Value: Sendable>: @unchecked Sendable {
    public enum Failure: Error, Sendable { case noEntry, closed, cancelled }
    private let lock = NSLock()
    private var result: (Result<Value, Failure>, ContinuousClock.Instant)?
    private var waiters: [(ContinuousClock.Instant, CheckedContinuation<Value, Error>)] = []

    public init() {}
    /// Establishes that another observer has actually registered in phase controls.
    public var pendingWaiterCount: Int { lock.withLock { waiters.count } }

    public func finish(_ outcome: Result<Value, Failure>) {
        let completed = ContinuousClock.now
        let settlement = lock.withLock { () -> (Result<Value, Failure>, [CheckedContinuation<Value, Error>])? in
            guard result == nil else { return nil }
            // The first expired registered bound ends this one phase for
            // every observer, even if its backstop has not run yet.
            let settled: Result<Value, Failure> = waiters.contains { completed > $0.0 }
                ? .failure(.noEntry) : outcome
            result = (settled, completed)
            defer { waiters.removeAll() }
            return (settled, waiters.map { $0.1 })
        }
        guard let (settled, waiting) = settlement else { return }
        for waiter in waiting { waiter.resume(with: settled.mapError { $0 as Error }) }
    }

    public func wait(until deadline: ContinuousClock.Instant) async throws -> Value {
        let backstop = Task {
            do { try await Task.sleep(until: deadline, clock: .continuous) }
            catch { return }
            finish(.failure(.noEntry))
        }
        defer { backstop.cancel() }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                let registration = lock.withLock { () -> (Result<Value, Error>?, [CheckedContinuation<Value, Error>]) in
                    if let (outcome, completed) = result {
                        let ready: Result<Value, Error> = completed <= deadline ? outcome.mapError { $0 as Error }
                            : .failure(Failure.noEntry)
                        return (ready, [])
                    }
                    let now = ContinuousClock.now
                    if now >= deadline {
                        // A failed registration is terminal for this one phase,
                        // including observers that registered before it expired.
                        result = (.failure(.noEntry), now)
                        let waiting = waiters.map { $0.1 }
                        waiters.removeAll()
                        return (.failure(Failure.noEntry), waiting)
                    }
                    waiters.append((deadline, continuation))
                    return (nil, [])
                }
                for waiter in registration.1 { waiter.resume(throwing: Failure.noEntry) }
                if let ready = registration.0 { continuation.resume(with: ready) }
            }
        } onCancel: {
            self.finish(.failure(.cancelled))
        }
    }
}

/// Test-local monotonic receipts. No radio or credential payloads are recorded.
public final class TestReceipts: @unchecked Sendable {
    private let started = ContinuousClock.now
    private let lock = NSLock()
    private var entries: [String] = []
    public init() {}
    public func mark(_ phase: String) {
        let elapsed = ContinuousClock.now - started
        lock.withLock { entries.append("\(elapsed): \(phase)") }
    }
    public var summary: String { lock.withLock { entries.joined(separator: "\n") } }
}
