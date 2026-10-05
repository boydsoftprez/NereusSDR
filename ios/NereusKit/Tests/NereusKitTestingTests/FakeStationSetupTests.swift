// NereusSDR for iOS: the fake Core sends Setup descriptions only when made with them, and a page added later arrives
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

@Suite("FakeStation, Setup descriptions", .serialized)
@MainActor
struct FakeStationSetupTests {
    private func connected(_ additions: FakeStation.Additions)
        async throws -> (FakeStation, MirrorStore, SetupDescriptionFeed, Task<Void, Never>) {
        let station = try FakeStation(additions: additions)
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let feed = SetupDescriptionFeed(store: mirror)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return (station, mirror, feed, feeding)
    }

    private func poll(_ condition: @MainActor () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    @Test("made with the addition, the fake advertises version 11 and sends its categories in order")
    func deliversAndAddsAPage() async throws {
        let (station, mirror, feed, feeding) = try await connected([.setupDescription])
        defer { feeding.cancel() }
        #expect(mirror.capabilityVersion("setupDescriptionVersion") == 11)
        await station.deliverSetup([
            ("general", FakeStation.syntheticSetup("general", title: "Options", version: 3)),
            ("dsp", ""),
        ])
        #expect(await poll { feed.description(for: "general") != nil })
        #expect(feed.categoryOrder == ["general", "dsp"])
        #expect(feed.status(for: "dsp") == .unpublished)
        #expect(feed.description(for: "general")?.pages.map(\.title) == ["Options"])
        await station.publishSetup("general", json: FakeStation.syntheticSetup(
            "general", title: "Options", version: 3, extraPage: "Startup & Preferences"), revision: 2)
        #expect(await poll { feed.description(for: "general")?.pages.count == 2 })
        #expect(feed.description(for: "general")?.pages.map(\.title) == ["Options", "Startup & Preferences"])
    }

    @Test("without the addition the fake sends no Setup capability")
    func olderCore() async throws {
        let (_, mirror, _, feeding) = try await connected([])
        defer { feeding.cancel() }
        #expect(mirror.capabilityVersion("setupDescriptionVersion") == 0)
    }
}
