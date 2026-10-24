// NereusSDR for iOS: the fake Core answers the display controls' grid settings, slices on a pan and notches
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

/// The fake Core the phone's display control tests run against answers
/// as the Core does: a grid setting echoed or refused with its own value,
/// a slice added on a pan up to the radio's limit, a notch recorded, and a
/// slice moved to another pan.
@Suite("FakeStation, the display controls", .serialized)
@MainActor
struct FakeStationDisplayTests {
    struct Rig {
        let station: FakeStation
        let session: StationSession
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feeding: Task<Void, Never>
    }

    private func connected(_ station: FakeStation) async throws -> Rig {
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let settings = SettingsProxyClient(session: session)
        let commands = CommandClient(session: session)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                settings.handle(event)
                await commands.handle(event)
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return Rig(station: station, session: session, mirror: mirror, settings: settings, commands: commands,
                   feeding: feeding)
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

    @Test("a grid write is echoed, and a queued refusal refuses the next with the kept value")
    func gridSetting() async throws {
        let rig = try await connected(try FakeStation())
        defer { rig.feeding.cancel() }
        let key = "DisplayGridMax_20m"
        #expect(await rig.settings.write(key, "-40") == .accepted)
        #expect(rig.settings.value(key) == "-40")

        rig.station.refuseNextSetting(key, reason: "r")
        #expect(await rig.settings.write(key, "-55") == .rejected(reason: "r"))
        #expect(rig.settings.value(key) == "-40")
        // The refusal was for one write only.
        #expect(await rig.settings.write("DisplayGridMin_20m", "-130") == .accepted)
        #expect(await rig.settings.write(key, "-50") == .accepted)
        #expect(rig.settings.value(key) == "-50")
        await rig.session.disconnect()
    }

    @Test("addSliceOnPan adds a slice on that pan up to the radio's limit, then refuses in the Core's words")
    func addSliceOnPan() async throws {
        let rig = try await connected(try FakeStation())
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilities["effectiveMaxSlices"] == .int(5))
        #expect(rig.station.panKey(ofSlice: 0) == "pan-0")
        // The fixture's slice:0 is one of five.
        for index in 1...4 {
            let result = try await rig.commands.invoke(FakeStation.addSliceOnPanVerb, arguments: [
                CommandArgument(name: "panId", value: .text("pan-1")),
            ], timeout: TestBackstop.hang)
            #expect(result.accepted)
            #expect(result.affectedKeys == ["slice:\(index)"])
            #expect(await poll { rig.mirror.object("slice:\(index)")?["panKey"] == .text("pan-1") })
            #expect(rig.station.panKey(ofSlice: Int64(index)) == "pan-1")
        }
        let refused = try await rig.commands.invoke(FakeStation.addSliceOnPanVerb, arguments: [
            CommandArgument(name: "panId", value: .text("pan-1")),
        ], timeout: TestBackstop.hang)
        #expect(!refused.accepted)
        #expect(refused.reason == "This radio supports a maximum of 5 slices")
        #expect(FakeStation.sliceCapReason(1) == "This radio supports a maximum of 1 slice")
        #expect(rig.mirror.object("slice:5") == nil)
        await rig.session.disconnect()
    }

    @Test("notch.add is recorded with its three values and answered with the revision and id")
    func addNotch() async throws {
        let rig = try await connected(try FakeStation())
        defer { rig.feeding.cancel() }
        let result = try await rig.commands.invoke(FakeStation.addNotchVerb, arguments: [
            CommandArgument(name: "sliceId", value: .int(0)),
            CommandArgument(name: "centreHz", value: .double(14_230_000)),
            CommandArgument(name: "widthHz", value: .double(100)),
        ], timeout: TestBackstop.hang)
        #expect(result.accepted)
        #expect(result.affectedKeys == ["notches"])
        #expect(result.values["id"] == .int(1) && result.values["revision"] == .int(1))
        let added = rig.station.notchesAdded
        #expect(added.count == 1)
        #expect(added.first?.sliceId == "0")
        #expect(added.first?.centreHz == 14_230_000)
        #expect(added.first?.widthHz == 100)

        let unread = try await rig.commands.invoke(FakeStation.addNotchVerb, arguments: [
            CommandArgument(name: "sliceId", value: .int(0)),
        ], timeout: TestBackstop.hang)
        #expect(!unread.accepted)
        #expect(unread.reason == FakeStation.unreadableNotchReason)
        #expect(rig.station.notchesAdded.count == 1)
        await rig.session.disconnect()
    }

    @Test("a slice's panKey write moves the slice")
    func movePan() async throws {
        let rig = try await connected(try FakeStation())
        defer { rig.feeding.cancel() }
        _ = await rig.mirror.write("slice:0", property: "panKey", value: .text("pan-1"))
        #expect(await poll { rig.mirror.object("slice:0")?["panKey"] == .text("pan-1") })
        #expect(rig.station.panKey(ofSlice: 0) == "pan-1")
        await rig.session.disconnect()
    }
}
