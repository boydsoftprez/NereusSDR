// NereusSDR for iOS: finite acknowledgements for asynchronous test phases
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin

/// One acknowledgement, with a failing backstop rather than an unbounded
/// continuation or a task group that must join an uncooperative child.
/// Closure and cancellation settle all waiters; a late entry cannot pass.
public final class TestPhase<Value: Sendable>: @unchecked Sendable {
    public enum Failure: Error, Sendable { case noEntry, closed, cancelled }
    private let lock = NSLock()
    private var result: (Result<Value, Failure>, ContinuousClock.Instant)?
    private var waiters: [(ContinuousClock.Instant, CheckedContinuation<Value, Error>)] = []
    private let receipts: TestReceipts?
    private let phase: String

    public init(receipts: TestReceipts? = nil, phase: String = "phase") {
        self.receipts = receipts
        self.phase = phase
    }
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
        if case nil = settlement {
            receipts?.mark("\(phase) finish ignored; terminal result retained", at: completed)
        }
        guard let (settled, waiting) = settlement else { return }
        receipts?.mark("\(phase) finished \(Self.label(settled)); observers=\(waiting.count)", at: completed)
        for waiter in waiting { waiter.resume(with: settled.mapError { $0 as Error }) }
    }

    public func wait(until deadline: ContinuousClock.Instant) async throws -> Value {
        if let receipts {
            receipts.mark("\(phase) wait entered; deadline=\(receipts.offset(of: deadline))")
        }
        let backstop = Task {
            do { try await Task.sleep(until: deadline, clock: .continuous) }
            catch { return }
            finish(.failure(.noEntry))
        }
        defer {
            backstop.cancel()
            receipts?.mark("\(phase) observer returned")
        }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                var registrationEvent = "recorded result"
                var registrationInstant = ContinuousClock.now
                var recordedCompletion: ContinuousClock.Instant?
                let registration = lock.withLock { () -> (Result<Value, Error>?, [CheckedContinuation<Value, Error>]) in
                    if let (outcome, completed) = result {
                        recordedCompletion = completed
                        let ready: Result<Value, Error> = completed <= deadline ? outcome.mapError { $0 as Error }
                            : .failure(Failure.noEntry)
                        return (ready, [])
                    }
                    let now = ContinuousClock.now
                    registrationInstant = now
                    if now >= deadline {
                        registrationEvent = "expired registration; terminal noEntry"
                        // A failed registration is terminal for this one phase,
                        // including observers that registered before it expired.
                        result = (.failure(.noEntry), now)
                        let waiting = waiters.map { $0.1 }
                        waiters.removeAll()
                        return (.failure(Failure.noEntry), waiting)
                    }
                    waiters.append((deadline, continuation))
                    registrationEvent = "waiter registered"
                    return (nil, [])
                }
                if let receipts {
                    let completion = recordedCompletion.map { "; completed=\(receipts.offset(of: $0))" } ?? ""
                    let ready = registration.0.map { "; ready=\(Self.label($0))" } ?? ""
                    receipts.mark("\(phase) \(registrationEvent)\(completion)\(ready)", at: registrationInstant)
                }
                for waiter in registration.1 { waiter.resume(throwing: Failure.noEntry) }
                if let ready = registration.0 { continuation.resume(with: ready) }
            }
        } onCancel: {
            self.finish(.failure(.cancelled))
        }
    }

    private static func label<Problem: Error>(_ outcome: Result<Value, Problem>) -> String {
        switch outcome {
        case .success: return "success"
        case .failure(let error):
            switch error as? Failure {
            case .noEntry?: return "noEntry"
            case .closed?: return "closed"
            case .cancelled?: return "cancelled"
            case nil: return "error"
            }
        }
    }
}

/// Test-local monotonic receipts. No radio or credential payloads are recorded.
public final class TestReceipts: @unchecked Sendable {
    private static let epoch = ContinuousClock.now
    private static let maximumEntries = 256
    private let started = ContinuousClock.now
    private let caseID: String
    private let lock = NSLock()
    private var entries: [String] = []
    private var omitted = 0
    public init(caseID: String = "unlabelled") {
        self.caseID = caseID
        _ = Self.epoch
    }
    public func mark(_ phase: String) {
        mark(phase, at: ContinuousClock.now)
    }
    /// Explicit instants are captured at a protected boundary and recorded
    /// after its lock is released. Thread details identify the recording thread,
    /// not actor isolation; `recorded` distinguishes later recording from capture.
    public func mark(_ phase: String, at instant: ContinuousClock.Instant) {
        var threadID: UInt64 = 0
        pthread_threadid_np(nil, &threadID)
        let recorded = ContinuousClock.now
        let entry = "shared=\(offset(of: instant)) local=\(instant - started) recorded=\(offset(of: recorded)) thread=\(threadID) main=\(Thread.isMainThread) case=\(caseID): \(phase)"
        lock.withLock {
            if entries.count < Self.maximumEntries {
                entries.append(entry)
            } else {
                omitted += 1
            }
        }
    }
    public func offset(of instant: ContinuousClock.Instant) -> Duration { instant - Self.epoch }
    public var summary: String {
        lock.withLock { entries.joined(separator: "\n") + "\nomitted=\(omitted)" }
    }
    public func flush() {
        mark("test scope exiting")
        let snapshot = summary
        print("KIT RECEIPTS \(caseID)\n\(snapshot)")
    }
}
