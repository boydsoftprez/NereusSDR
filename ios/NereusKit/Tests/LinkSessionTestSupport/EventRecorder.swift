// NereusSDR for iOS: collects what a session reports, for the tests to read
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Reads a session's events as they arrive.
public final class EventRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var recorded: [StationSession.Event] = []
    private var task: Task<Void, Never>?
    private let waiters = ConditionWaiters()

    /// Reads `session`'s events. A session has one event stream and one
    /// reader, so a test that feeds the events on (to a mirror) passes
    /// `forward`, which sees each event before it is recorded: an event
    /// the recorder holds is one `forward` has finished with.
    public init(_ session: StationSession,
                forward: (@Sendable (StationSession.Event) async -> Void)? = nil) {
        task = Task { [weak self] in
            for await event in session.events {
                await forward?(event)
                self?.append(event)
            }
        }
    }

    deinit {
        task?.cancel()
    }

    private func append(_ event: StationSession.Event) {
        lock.withLock { recorded.append(event) }
        waiters.release()
    }

    public var events: [StationSession.Event] { lock.withLock { recorded } }

    public var refusals: [Refusal] {
        events.compactMap { event in
            if case .refused(let refusal) = event {
                return refusal
            }
            return nil
        }
    }

    public var states: [StationSession.State] {
        events.compactMap { event in
            if case .stateChanged(let state) = event {
                return state
            }
            return nil
        }
    }

    public var messages: [LinkMessage] {
        events.compactMap { event in
            if case .message(let message) = event {
                return message
            }
            return nil
        }
    }

    /// Waits, without sleeping, for the events the session has already
    /// reported to reach the recorder, and returns whether `condition` holds.
    @discardableResult
    public func settle(until condition: ([StationSession.Event]) -> Bool = { _ in false }) async -> Bool {
        for _ in 0..<2_000 {
            if condition(events) {
                return true
            }
            await Task.yield()
        }
        return condition(events)
    }

    /// Waits until the recorder holds events for which `condition` holds
    /// (so `forward` has finished with each of them), without polling;
    /// returns false only if that has not happened by `timeout`.
    @discardableResult
    public func handled(within timeout: Duration = .seconds(30),
                        until condition: @escaping @Sendable ([StationSession.Event]) -> Bool) async -> Bool {
        await waiters.wait(within: timeout) { [self] in condition(events) }
    }

    /// The state the recorder last saw the session report.
    public var lastState: StationSession.State {
        states.last ?? .idle
    }

    /// Waits in real time, up to `timeout`, for `condition` to hold; for
    /// tests over real sockets, where the far end answers when it answers.
    @discardableResult
    public func wait(timeout: Duration = .seconds(10),
              until condition: @escaping @Sendable ([StationSession.Event]) -> Bool) async -> Bool {
        let deadline = ContinuousClock.now.advanced(by: timeout)
        while ContinuousClock.now < deadline {
            if condition(events) {
                return true
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
        return condition(events)
    }
}
