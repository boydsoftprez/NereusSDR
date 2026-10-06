// NereusSDR for iOS: tests for the microphone input's wake: the input thread flags and signals, one thread drains
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMedia

/// The input thread's only work besides the copy is ``MicrophoneInputWake/inputArrived()``:
/// a flag and a signal, never a queued block. However many callbacks come
/// while a drain runs, they wake the drain thread once more, not once each.
struct MicrophoneInputWakeTests {
    @Test func callbacksDuringADrainWakeItOnceMoreNotOnceEach() {
        let drains = DrainGate()
        guard let wake = MicrophoneInputWake({ drains.run() }) else {
            Issue.record("no wake")
            return
        }
        defer { wake.close() }

        wake.inputArrived()
        drains.waitForStart(1)
        // The first drain holds; the input keeps coming.
        for _ in 0..<100 {
            wake.inputArrived()
        }
        drains.release()
        drains.waitForStart(2)
        drains.release()
        wake.close()
        #expect(drains.started == 2, "a hundred callbacks during one drain wake one more drain, not a hundred")
    }

    @Test func nothingDrainsAfterClose() {
        let drains = DrainGate()
        guard let wake = MicrophoneInputWake({ drains.run() }) else {
            Issue.record("no wake")
            return
        }
        wake.inputArrived()
        drains.waitForStart(1)
        drains.release()
        wake.close()
        // close() returned once the thread ended: nothing is left to run these.
        for _ in 0..<10 {
            wake.inputArrived()
        }
        #expect(drains.started == 1, "an input after close starts no drain")
    }
}

/// Each drain counts itself and holds until the test releases it.
private final class DrainGate: @unchecked Sendable {
    private let condition = NSCondition()
    private var startedCount = 0
    private var releases = 0

    var started: Int {
        condition.lock()
        defer { condition.unlock() }
        return startedCount
    }

    func run() {
        condition.lock()
        startedCount += 1
        let mine = startedCount
        condition.broadcast()
        while releases < mine {
            condition.wait()
        }
        condition.unlock()
    }

    func waitForStart(_ count: Int) {
        condition.lock()
        while startedCount < count {
            condition.wait()
        }
        condition.unlock()
    }

    func release() {
        condition.lock()
        releases += 1
        condition.broadcast()
        condition.unlock()
    }
}
