// NereusSDR for iOS: diagnostics for a local rendezvous test child that exits early
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)

import Testing

@Suite struct LocalRendezvousTests {
    @Test func anExitedTurnFakeStopsTheWaitWithItsError() async throws {
        let child = try LocalRendezvous.Child(executable: "/usr/bin/env",
                                              arguments: ["python3", "-c", "import sys; print('fake died', file=sys.stderr); sys.exit(7)"])
        defer { child.stop() }
        do {
            try await LocalRendezvous.waitUntil("the TURN fake's release", within: .seconds(5),
                                                whileAlive: { child.exitDiagnostic }) { false }
            Issue.record("the wait returned while the fake had exited")
        } catch let error as LocalRendezvous.Failure {
            #expect(error.description.contains("status 7"))
            #expect(error.description.contains("fake died"))
        }
    }
}

#endif
