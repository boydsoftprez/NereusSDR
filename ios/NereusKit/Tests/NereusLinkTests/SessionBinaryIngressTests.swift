// NereusSDR for iOS: binary media ingress bounds
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

@Suite struct SessionBinaryIngressTests {
    private actor Gate {
        private var entered = false
        private var enteredWaiter: CheckedContinuation<Void, Never>?
        private var releaseWaiter: CheckedContinuation<Void, Never>?

        func block() async {
            if entered { return }
            entered = true
            enteredWaiter?.resume()
            enteredWaiter = nil
            await withCheckedContinuation { releaseWaiter = $0 }
        }

        func waitUntilEntered() async {
            if entered { return }
            await withCheckedContinuation { enteredWaiter = $0 }
        }

        func release() {
            releaseWaiter?.resume()
            releaseWaiter = nil
        }
    }

    @Test func highRateMediaIsBoundedAndNeverEntersControlStream() async {
        let ingress = SessionBinaryIngress()
        let gate = Gate()
        ingress.setHandler { _, _, _ in await gate.block() }
        let frame = Data([2] + Array(repeating: UInt8(0x42), count: 1500))
        ingress.offer(frame, generation: 1, routeId: 0)
        await gate.waitUntilEntered()
        for _ in 0..<100 { ingress.offer(frame, generation: 1, routeId: 0) }
        #expect(ingress.queued.count == 16)
        #expect(ingress.queued.bytes == 16 * 1501)
        ingress.offer(Data([3, 1, 2]), generation: 1, routeId: 0)
        ingress.offer(Data([2]), generation: 1, routeId: 0)
        #expect(ingress.queued.count == 16)
        await gate.release()
    }
}
