// NereusSDR for iOS: a stand-in for the phone's microphone that records no sound
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
@testable import NereusSDR

/// A ``MicrophoneSource`` that only counts starts and stops, answers each
/// start with ``result``, and is running between a started start and a stop.
/// Its level runs between a started ``startLevel(_:lost:)`` and ``stopLevel()``,
/// and ``speak(peak:)`` hands the level's handler a peak as the phone's
/// microphone would.
final class FakeMicrophone: MicrophoneSource, @unchecked Sendable {
    /// Recursive: a waiter's condition reads the counts under it.
    private let lock = NSRecursiveLock()
    private var answer: MicrophoneStart = .started
    private var levelAnswer: MicrophoneStart = .started
    private var startCount = 0
    private var stopCount = 0
    private var levelStartCount = 0
    private var levelStopCount = 0
    private var on = false
    private var handler: (@Sendable (Float) -> Void)?
    private var lostHandler: (@Sendable (String) -> Void)?
    private var levelLostHandler: (@Sendable () -> Void)?
    private var calls: [String] = []
    /// Tasks waiting in ``when(_:)``, resumed by the change that makes their condition hold.
    private var waiters: [(condition: @Sendable (FakeMicrophone) -> Bool, go: CheckedContinuation<Void, Never>)] = []

    var result: MicrophoneStart {
        get { lock.withLock { answer } }
        set { lock.withLock { answer = newValue } }
    }

    /// How a start of the level answers.
    var levelResult: MicrophoneStart {
        get { lock.withLock { levelAnswer } }
        set { lock.withLock { levelAnswer = newValue } }
    }

    var starts: Int { lock.withLock { startCount } }
    var stops: Int { lock.withLock { stopCount } }
    var isRunning: Bool { lock.withLock { on } }
    var levelStarts: Int { lock.withLock { levelStartCount } }
    var levelStops: Int { lock.withLock { levelStopCount } }
    /// "start", "stop" and "reset" in the order they came.
    var events: [String] { lock.withLock { calls } }
    /// The level is on: the microphone would be open for the meter.
    var isMetering: Bool { lock.withLock { handler != nil } }

    /// Awaited before a start answers, so a test can hold the start.
    var hold: (@Sendable () async -> Void)? {
        get { lock.withLock { held } }
        set { lock.withLock { held = newValue } }
    }
    private var held: (@Sendable () async -> Void)?

    func start() async -> MicrophoneStart {
        await hold?()
        let result = lock.withLock {
            startCount += 1
            calls.append("start")
            if answer == .started {
                on = true
            }
            return answer
        }
        defer { release() }
        return result
    }

    /// Returns once `condition` holds: now, or at the start or stop that
    /// makes it hold. No clock: a test that never gets there is ended by
    /// its own time limit.
    func when(_ condition: @escaping @Sendable (FakeMicrophone) -> Bool) async {
        await withCheckedContinuation { (go: CheckedContinuation<Void, Never>) in
            lock.withLock { waiters.append((condition, go)) }
            release()
        }
    }

    private func release() {
        let ready = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            let ready = waiters.filter { $0.condition(self) }.map(\.go)
            waiters.removeAll { $0.condition(self) }
            return ready
        }
        for go in ready {
            go.resume()
        }
    }

    func stop() {
        lock.withLock {
            stopCount += 1
            calls.append("stop")
            on = false
        }
        release()
    }

    func startLevel(_ level: @escaping @Sendable (Float) -> Void,
                    lost: @escaping @Sendable () -> Void) async -> MicrophoneStart {
        lock.withLock {
            levelStartCount += 1
            if levelAnswer == .started {
                handler = level
                levelLostHandler = lost
            }
            return levelAnswer
        }
    }

    func stopLevel() {
        lock.withLock {
            levelStopCount += 1
            handler = nil
            levelLostHandler = nil
        }
    }

    func servicesReset() {
        lock.withLock {
            calls.append("reset")
        }
    }

    func onLost(_ lost: @escaping @Sendable (String) -> Void) {
        lock.withLock {
            lostHandler = lost
        }
    }

    /// Sending stops by itself, as ``MicCapture`` reports an input it could
    /// not build again: the lost handler hears `reason`.
    func lose(_ reason: String) {
        let lost = lock.withLock { () -> (@Sendable (String) -> Void)? in
            on = false
            return lostHandler
        }
        lost?(reason)
    }

    /// The level stops by itself, as ``MicCapture`` drops it when the input
    /// could not be built again after a route change: the level's handler
    /// goes and the level's lost handler hears it. Returns whether the
    /// level was on.
    @discardableResult
    func loseLevel() -> Bool {
        let lost = lock.withLock { () -> (@Sendable () -> Void)? in
            let lost = handler != nil ? levelLostHandler : nil
            handler = nil
            levelLostHandler = nil
            return lost
        }
        lost?()
        return lost != nil
    }

    /// The microphone hears a piece of sound whose peak is `peak`; false
    /// when the level is off, so nothing hears it.
    @discardableResult
    func speak(peak: Float) -> Bool {
        guard let level = lock.withLock({ handler }) else {
            return false
        }
        level(peak)
        return true
    }
}
