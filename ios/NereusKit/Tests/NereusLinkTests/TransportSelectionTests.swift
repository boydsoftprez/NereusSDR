// NereusSDR for iOS: the route-selection gate preserves already resolved outcomes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct TransportSelectionTests {
    private func transport() -> ScriptedTransport {
        ScriptedTransport(presentedSHA256: Data(repeating: 7, count: 32), openFailure: nil)
    }

    @Test func completedBeforeWaitReturnsFirstTransportAndClosesDuplicate() async throws {
        let selection = TransportSelection()
        let first = transport()
        let duplicate = transport()
        selection.complete(first)
        selection.fail(LinkTransportError.failed("later"))
        selection.complete(duplicate)
        let selected = try await selection.value()
        #expect((selected as? ScriptedTransport) === first)
        #expect(!first.isClosedByApp)
        #expect(duplicate.isClosedByApp)
    }

    @Test func failedBeforeWaitKeepsFirstErrorAndClosesLateTransport() async {
        let selection = TransportSelection()
        let late = transport()
        selection.fail(LinkTransportError.failed("first"))
        selection.fail(LinkTransportError.failed("later"))
        selection.cancel()
        selection.complete(late)
        do {
            _ = try await selection.value()
            Issue.record("failed selection returned a transport")
        } catch {
            #expect(error as? LinkTransportError == .failed("first"))
        }
        #expect(late.isClosedByApp)
    }

    @Test func cancelledBeforeWaitThrowsCancellationAndClosesLateTransport() async {
        let selection = TransportSelection()
        let late = transport()
        selection.cancel()
        selection.fail(LinkTransportError.failed("later"))
        selection.complete(late)
        await #expect(throws: CancellationError.self) { _ = try await selection.value() }
        #expect(late.isClosedByApp)
    }

    @Test func cancellationOfUnclaimedSuccessClosesItAndChangesStoredResult() async {
        let selection = TransportSelection()
        let unclaimed = transport()
        selection.complete(unclaimed)
        selection.cancel()
        selection.cancel()
        await #expect(throws: CancellationError.self) { _ = try await selection.value() }
        #expect(unclaimed.isClosedByApp)
    }
}
