// NereusSDR for iOS: VAX Audio against a fake Core: channels, levels, mutes and meters, older and headless Cores
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18, spec section 5.2 item 4 (link document sections 6.3, 7.1,
/// 7.3 and 7.7, `vaxVersion` 1): VAX Audio shows the Core's four VAX
/// channels and its microphone row from the `vax` object; it writes each
/// level and mute to the Core and shows the Core's words for a refusal;
/// the microphone level changes only while this phone may transmit; the
/// meters are asked for only while the page is on screen; an older Core, a
/// headless Core and no Core leave every control shown and greyed with the
/// reason.
@Suite("VAX Audio", .serialized)
@MainActor
struct VaxAudioTests {
    private let platform = TestPlatform()

    @Test("the Core's channels: slices, levels, mutes and devices, then the transmit slice and level")
    func channels() async throws {
        let (model, station) = try await connected(additions: [.vax])
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        // vaxVersion 1 but no object in the snapshot: nothing to show.
        #expect(await settle { vax.reason == VaxAudioModel.olderCoreReason })
        try await station.deliverVax(.board)
        #expect(await settle { vax.reason == nil && vax.vax != nil })
        let channels = try #require(vax.vax?.channels)
        #expect(channels.map(\.slices) == ["AB", "", "C", ""])
        #expect(channels.map(\.rxGain) == [0.8, 0.5, 1, 1])
        #expect(channels.map(\.muted) == [false, true, false, false])
        #expect(channels.map(\.device) == ["NereusSDR VAX 1", "NereusSDR VAX 2", "NereusSDR VAX 3", "NereusSDR VAX 4"])
        #expect(vax.vax?.txSlice == "A" && vax.vax?.txGain == 0.6)
        #expect(channels.map { VaxAudioModel.slicesText($0.slices) } == ["Slices A+B", "None", "Slice C", "None"])
        #expect(VaxAudioModel.percent(0.6) == "60%" && VaxAudioModel.percent(1.4) == "100%")
        // This fake Core does not let the phone transmit, so the microphone level is greyed.
        #expect(vax.txReason == VaxAudioModel.noTransmitReason)
        await model.disconnect()
        #expect(await settle { vax.reason == VaxAudioModel.notConnectedReason && vax.vax == nil })
        #expect(vax.txReason == VaxAudioModel.notConnectedReason)
    }

    @Test("the meters are asked for only while the page is open, and follow the Core's record")
    func levels() async throws {
        let (model, station) = try await connected(additions: [.vax])
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        try await station.deliverVax(.board)
        #expect(await settle { vax.reason == nil })
        #expect(vax.levels == nil && vax.levelsReason == nil)
        #expect(!station.messages.contains { ToolsPagesTests.asksFor($0, StationVax.levelsStream) })
        vax.setOpen(true)
        #expect(await settle { vax.levels != nil })
        #expect(station.vaxLevelsWatched)
        let subscribe = try #require(station.messages.compactMap(AccessoryPagesTests.invoke)
            .first { $0.verb == "records.subscribe" && $0.args.first?.value == .utf8(StationVax.levelsStream) })
        #expect(subscribe.args.last == .init(name: "backlog", value: .i64(1)))
        #expect(vax.levels?.channels == [0.42, 0, 0.17, 0] && vax.levels?.tx == 0.05)
        await station.deliverVaxLevels(["ch1Level": .number(0.9), "ch2Level": .number(0.1), "ch3Level": .number(0),
                                        "ch4Level": .number(0), "txLevel": .number(0), "atMs": .number(1)])
        #expect(await settle { vax.levels?.channels.first == 0.9 })
        vax.setOpen(false)
        #expect(await settle { !station.vaxLevelsWatched })
        #expect(vax.levels == nil)
        #expect(station.messages.contains {
            AccessoryPagesTests.invoke($0)?.verb == "records.unsubscribe"
                && AccessoryPagesTests.invoke($0)?.args.first?.value == .utf8(StationVax.levelsStream)
        })
        await model.disconnect()
    }

    @Test("a level or mute goes to the Core, which sends it back; a refusal shows the Core's words")
    func writes() async throws {
        let (model, station) = try await connected(additions: [.vax])
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        try await station.deliverVax(.board)
        #expect(await settle { vax.reason == nil })
        vax.setRxGain(2, 0.254)
        #expect(await settle { vax.vax?.channels[1].rxGain == 0.25 })
        #expect(station.vaxScene.rxGains == [0.8, 0.25, 1, 1])
        vax.setMuted(1, true)
        #expect(await settle { vax.vax?.channels[0].muted == true })
        #expect(station.vaxScene.muted == [true, true, false, false])
        station.refuseNext(FakeStation.vaxKey, reason: FakeStation.vaxLevelReason)
        vax.setRxGain(3, 0.5)
        #expect(await settle { vax.note == FakeStation.vaxLevelReason })
        #expect(vax.vax?.channels[2].rxGain == 1)
        // Receive only: the microphone level is greyed and nothing is sent.
        vax.setTxGain(0.2)
        try await Task.sleep(for: .milliseconds(200))
        #expect(!station.messages.contains { Self.writes($0, "txGain") })
        #expect(station.vaxScene.txGain == 0.6)
        // The next accepted change clears the refusal.
        vax.setMuted(1, false)
        #expect(await settle { vax.note == nil && vax.vax?.channels[0].muted == false })
        await model.disconnect()
    }

    @Test("the microphone level changes while this phone may transmit")
    func microphoneLevel() async throws {
        let (model, station) = try await connected(additions: [.vax, .remoteTx])
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        try await station.deliverVax(.board)
        #expect(await settle { vax.reason == nil && vax.txReason == nil })
        vax.setTxGain(0.3)
        #expect(await settle { vax.vax?.txGain == 0.3 })
        #expect(station.vaxScene.txGain == 0.3)
        await model.disconnect()
    }

    @Test("an older Core greys every control with its reason and is sent nothing")
    func olderCore() async throws {
        let (model, station) = try await connected()
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        vax.setOpen(true)
        #expect(await settle { vax.reason == VaxAudioModel.olderCoreReason })
        #expect(vax.vax == nil && vax.levelsReason == VaxAudioModel.olderCoreReason)
        #expect(vax.txReason == VaxAudioModel.olderCoreReason)
        vax.setRxGain(1, 0.5)
        vax.setMuted(1, true)
        vax.setTxGain(0.5)
        try await Task.sleep(for: .milliseconds(200))
        #expect(!station.messages.contains { Self.writes($0, nil) })
        #expect(!station.messages.contains { ToolsPagesTests.asksFor($0, StationVax.levelsStream) })
        await model.disconnect()
    }

    @Test("a Core whose computer has no VAX devices greys the page with that reason and asks for no meters")
    func notHosted() async throws {
        let (model, station) = try await connected(additions: [.vaxUnhosted])
        let vax = VaxAudioModel(mirror: model.mirror, records: model.records)
        vax.setOpen(true)
        #expect(await settle { vax.reason == VaxAudioModel.noVaxReason })
        #expect(model.mirror.capabilityVersion(StationVax.capabilityName) == 0)
        #expect(vax.levelsReason == VaxAudioModel.noVaxReason && vax.txReason == VaxAudioModel.noVaxReason)
        vax.setRxGain(1, 0.5)
        try await Task.sleep(for: .milliseconds(200))
        #expect(!station.messages.contains { Self.writes($0, nil) })
        #expect(!station.messages.contains { ToolsPagesTests.asksFor($0, StationVax.levelsStream) })
        await model.disconnect()
    }

    @Test("the page asks for the meters when it appears and stops when it goes")
    func pageLifecycle() async throws {
        let (model, station) = try await connected(additions: [.vax])
        try await station.deliverVax(.board)
        #expect(await settle { model.mirror.object(StationVax.objectKey) != nil })
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        let host = UIHostingController(rootView: ToolScreen(model: VaxAudioModel(mirror: model.mirror,
                                                                                 records: model.records)) {
            VaxAudioPage(model: $0)
        })
        window.rootViewController = host
        window.isHidden = false
        #expect(await settle { station.vaxLevelsWatched })
        window.isHidden = true
        window.rootViewController = nil
        #expect(await settle { !station.vaxLevelsWatched })
        await model.disconnect()
    }

    // MARK: Helpers

    /// The message writes `property` to `vax` (any property when nil).
    nonisolated static func writes(_ message: LinkMessage, _ property: String?) -> Bool {
        guard case .propertyWrite(let write) = message, write.key == StationVax.objectKey else {
            return false
        }
        return property == nil || write.properties.contains { $0.name == property }
    }

    private func connected(additions: FakeStation.Additions = []) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "VaxAudioTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        return (model, station)
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}
