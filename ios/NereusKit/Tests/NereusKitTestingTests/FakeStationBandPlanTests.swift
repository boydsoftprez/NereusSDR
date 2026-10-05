// NereusSDR for iOS: the fake Core echoes a BandPlanName write, or refuses it with the Core's words
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

/// D79: the fake Core the app's band plan tests run against answers a
/// `settings.write` of `BandPlanName` as the Core does: echoed with the
/// writer's origin, or refused with its reason and its own value.
@Suite("FakeStation, the band plan setting", .serialized)
@MainActor
struct FakeStationBandPlanTests {
    private func poll(_ condition: @MainActor () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    @Test("a BandPlanName write is echoed, and a queued refusal refuses the next")
    func writesAreEchoedOrRefused() async throws {
        let station = try FakeStation()
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let settings = SettingsProxyClient(session: session)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                settings.handle(event)
            }
        }
        defer { feeding.cancel() }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })

        #expect(await settings.write(FakeStation.bandPlanKey, "IARU Region 1") == .accepted)
        #expect(settings.value(FakeStation.bandPlanKey) == "IARU Region 1")

        station.refuseNext(FakeStation.bandPlanKey, reason: FakeStation.unknownBandPlanReason)
        #expect(await settings.write(FakeStation.bandPlanKey, "Nowhere")
                    == .rejected(reason: FakeStation.unknownBandPlanReason))
        // The refusal carries the value the Core kept.
        #expect(settings.value(FakeStation.bandPlanKey) == "IARU Region 1")
        await session.disconnect()
    }

    @Test("with a catalogue that marks its plan active, a taken write moves the mark and an unknown plan is refused")
    func theMarkFollowsTheSetting() async throws {
        let station = try FakeStation()
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let settings = SettingsProxyClient(session: session)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                settings.handle(event)
            }
        }
        defer { feeding.cancel() }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        let json = #"{"bandPlans":[{"active":true,"default":true,"id":"a","name":"ARRL (US)"},"#
            + #"{"active":false,"default":false,"id":"i","name":"IARU Region 1"}]}"#
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(4)),
        ])))
        func active() -> [String] {
            guard case .text(let text)? = mirror.object("catalog")?["json"],
                  let object = (try? JSONSerialization.jsonObject(with: Data(text.utf8))) as? [String: Any],
                  let plans = object["bandPlans"] as? [[String: Any]] else {
                return []
            }
            return plans.filter { $0["active"] as? Bool == true }.compactMap { $0["name"] as? String }
        }
        #expect(await poll { active() == ["ARRL (US)"] })

        #expect(await settings.write(FakeStation.bandPlanKey, "IARU Region 1") == .accepted)
        #expect(await poll { active() == ["IARU Region 1"] })
        #expect(mirror.object("catalog")?["revision"] == .int(5))

        #expect(await settings.write(FakeStation.bandPlanKey, "Mars Region 9")
                    == .rejected(reason: FakeStation.unknownBandPlanReason))
        #expect(settings.value(FakeStation.bandPlanKey) == "IARU Region 1")
        #expect(active() == ["IARU Region 1"])
        await session.disconnect()
    }
}
