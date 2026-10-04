// NereusSDR for iOS: finite acknowledgements for asynchronous test phases
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport

/// One acknowledgement, with a failing backstop rather than an unbounded
/// continuation or a task group that must join an uncooperative child.
/// Closure and cancellation settle all waiters; a late entry cannot pass.
public final class TestPhase<Value: Sendable>: @unchecked Sendable {
    public enum Failure: Error, Sendable { case noEntry, closed, cancelled }
    private let lock = NSLock()
    private var result: (Result<Value, Failure>, ContinuousClock.Instant)?
    private var waiters: [(ContinuousClock.Instant, CheckedContinuation<Value, Error>)] = []

    private let receipts: HostedDiagnosticReceipts?
    private let label: String
    public init(receipts: HostedDiagnosticReceipts? = nil, label: String = "phase") {
        self.receipts = receipts
        self.label = label
    }
    private func mark(_ event: String) { receipts?.mark(label + " " + event) }
    /// Establishes that another observer has actually registered in phase controls.
    public var pendingWaiterCount: Int { lock.withLock { waiters.count } }

    public func finish(_ outcome: Result<Value, Failure>) {
        let completed = ContinuousClock.now
        mark("finish caller entry")
        let settlement = lock.withLock { () -> (Result<Value, Failure>, [CheckedContinuation<Value, Error>], HostedDiagnosticReceipts.Captured?)? in
            guard result == nil else { return nil }
            // The first expired registered bound ends this one phase for
            // every observer, even if its backstop has not run yet.
            let settled: Result<Value, Failure> = waiters.contains { completed > $0.0 }
                ? .failure(.noEntry) : outcome
            let event: String
            switch settled {
            case .success: event = "finish committed success"
            case .failure(let failure): event = "finish committed " + String(describing: failure)
            }
            result = (settled, completed)
            let captured = receipts == nil ? nil : HostedDiagnosticReceipts.capture(label + " " + event)
            defer { waiters.removeAll() }
            return (settled, waiters.map { $0.1 }, captured)
        }
        guard let (settled, waiting, captured) = settlement else { mark("finish ignored: terminal"); return }
        if let captured { receipts?.append(captured) }
        for waiter in waiting {
            mark("continuation resume call")
            waiter.resume(with: settled.mapError { $0 as Error })
            mark("continuation resume returned")
        }
    }

    public func wait(until deadline: ContinuousClock.Instant) async throws -> Value {
        mark("wait caller entry; remaining " + String(describing: deadline - ContinuousClock.now))
        defer { mark("wait caller exit") }
        let backstop = Task {
            mark("backstop task entry")
            do { try await Task.sleep(until: deadline, clock: .continuous) }
            catch { return }
            mark("backstop woke")
            finish(.failure(.noEntry))
        }
        defer { backstop.cancel() }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                let registration = lock.withLock { () -> (Result<Value, Error>?, [CheckedContinuation<Value, Error>], HostedDiagnosticReceipts.Captured?) in
                    if let (outcome, completed) = result {
                        let ready: Result<Value, Error> = completed <= deadline ? outcome.mapError { $0 as Error }
                            : .failure(Failure.noEntry)
                        return (ready, [], receipts == nil ? nil : HostedDiagnosticReceipts.capture(label + " wait immediate disposition"))
                    }
                    let now = ContinuousClock.now
                    if now >= deadline {
                        // A failed registration is terminal for this one phase,
                        // including observers that registered before it expired.
                        result = (.failure(.noEntry), now)
                        let waiting = waiters.map { $0.1 }
                        waiters.removeAll()
                        return (.failure(Failure.noEntry), waiting, receipts == nil ? nil : HostedDiagnosticReceipts.capture(label + " expired registration committed noEntry"))
                    }
                    waiters.append((deadline, continuation))
                    return (nil, [], receipts == nil ? nil : HostedDiagnosticReceipts.capture(label + " wait registered"))
                }
                if let captured = registration.2 { receipts?.append(captured) }
                for waiter in registration.1 {
                    mark("expired registration resume call")
                    waiter.resume(throwing: Failure.noEntry)
                    mark("expired registration resume returned")
                }
                if let ready = registration.0 {
                    mark("immediate continuation resume call")
                    continuation.resume(with: ready)
                    mark("immediate continuation resume returned")
                }
            }
        } onCancel: {
            self.mark("wait cancellation")
            self.finish(.failure(.cancelled))
        }
    }
}

/// Test-local monotonic receipts. No radio or credential payloads are recorded.
public final class TestReceipts: @unchecked Sendable {
    private let absolute = HostedDiagnosticReceipts("existing-link-receipts")
    private let started = ContinuousClock.now
    private let lock = NSLock()
    private var entries: [String] = []
    public init() {}
    public func mark(_ phase: String) {
        absolute.mark(phase)
        let elapsed = ContinuousClock.now - started
        lock.withLock { entries.append("\(elapsed): \(phase)") }
    }
    public var summary: String {
        absolute.export()
        return lock.withLock { entries.joined(separator: "\n") }
    }
}
