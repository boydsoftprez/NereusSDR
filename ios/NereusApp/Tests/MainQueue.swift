// NereusSDR for iOS: await main-queue work already scheduled by a test
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusMirror
import Testing

/// Await blocks already queued on the main dispatch queue. This does not
/// establish ordering for actor tasks, command queues or later work.
enum MainQueue {
    /// Returns once the main queue has run everything queued on it before now.
    static func drained() async {
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            DispatchQueue.main.async {
                continuation.resume()
            }
        }
    }
}

/// Round trips through the link to the fake Core, so a check that the app
/// sent nothing covers everything it queued before: the main queue runs
/// what is already on it, then a command goes after every command the app
/// queued, and its answer comes back once the fake has read them all and
/// the app has read every answer the fake sent before it. A second round
/// trip covers what the app sends on reading those answers.
enum LinkBarrier {
    @MainActor
    static func roundTrip(_ commands: CommandClient) async throws {
        for _ in 0 ..< 2 {
            await MainQueue.drained()
            let barrier = await commands.start(FakeStation.barrierVerb, arguments: [], copies: 1,
                                               timeout: .seconds(30))
            #expect(try await barrier.result().accepted)
        }
        await MainQueue.drained()
    }
}
