// NereusSDR for iOS: each band's dB Max and dB Min through the Core, against a fake Core, newer and older
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// DC3, R-IOS-56, spec 5.4: on a Core whose Setup description reaches
/// version 29 the band draws the Core's dB Max and dB Min for the band it
/// is on, and Top, Range, the dBm arrows, a drag on the scale and Setup's
/// dB Max and dB Min write that band's keys; while keyed they move the
/// transmit grid on this phone. An older Core keeps the phone's own scale
/// and is sent nothing; a lost link greys the rows.
@Suite("Core band scale", .serialized)
@MainActor
struct CoreBandScaleTests {
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<5_000 {
            await Task.yield()
        }
    }

    /// A model connected to a fake Core whose Setup description is at
    /// `version` (0 for none), on 20 m as its fixture's slice is, with
    /// display settings kept in `defaults`.
    private func connected(version: Int64 = CoreBandScale.sharedVersion,
                           defaults: UserDefaults? = nil) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation()
        let kept: UserDefaults
        if let defaults {
            kept = defaults
        } else {
            kept = try #require(UserDefaults(suiteName: "CoreBandScaleTests-\(UUID().uuidString)"))
        }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: kept))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        if version > 0 {
            var capabilities = model.mirror.capabilities
            capabilities[CoreBandScale.versionCapability] = .int(version)
            await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
                LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
            })))
        }
        let scale = model.main.bandScale
        #expect(await settle {
            model.main.band.settings.scaleBand == "5" && scale.isShared == (version >= CoreBandScale.sharedVersion)
                && scale.isConnected
        })
        await quiet()
        return (model, station)
    }

    /// The Core's value for `key`, as another device's write reaches this one.
    private func coreValue(_ station: FakeStation, _ key: String, _ text: String) async {
        await station.deliver(.settingsValue(LinkMessage.SettingsValue(key: key, origin: "", properties: [
            .init(name: key, value: .utf8(text)),
        ])))
    }

    /// The active slice moves to the Core's band `band`.
    private func slice(_ station: FakeStation, band: Int64) async {
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 30, name: "band", value: .i64(band)),
        ])))
    }

    /// The grid writes the fake Core has had, oldest first: key and value.
    private func gridWrites(_ station: FakeStation) -> [(key: String, text: String?)] {
        station.messages.compactMap {
            guard case .settingsWrite(let write) = $0, write.key.hasPrefix("DisplayGrid") else {
                return nil
            }
            let text: String?
            if case .utf8(let value)? = write.properties.first?.value {
                text = value
            } else {
                text = nil
            }
            return (write.key, text)
        }
    }

    // MARK: The keys

    @Test("each band's keys are the desktop's: 20 m, 40 m, 90 m, and 2 m on XVTR's")
    func keys() {
        #expect(CoreBandScale.maxKey(band: 5) == "DisplayGridMax_20m")
        #expect(CoreBandScale.minKey(band: 5) == "DisplayGridMin_20m")
        #expect(CoreBandScale.maxKey(band: 3) == "DisplayGridMax_40m")
        #expect(CoreBandScale.maxKey(band: 15) == "DisplayGridMax_90m")
        #expect(CoreBandScale.minKey(band: 15) == "DisplayGridMin_90m")
        #expect(CoreBandScale.maxKey(band: 27) == "DisplayGridMax_XVTR")
        #expect(CoreBandScale.maxKey(band: 99) == nil && CoreBandScale.minKey(band: -1) == nil)
        #expect(CoreBandScale.defaultMaxDbm == -40 && CoreBandScale.defaultMinDbm == -140)
    }

    // MARK: Drawn from the Core

    @Test("the band draws the Core's scale for the band it is on, and another band's when the slice moves")
    func drawsTheCoresScale() async throws {
        let (model, station) = try await connected()
        let band = model.main.band
        let display = model.main.display
        let scale = model.main.bandScale
        await coreValue(station, "DisplayGridMax_20m", "-50")
        await coreValue(station, "DisplayGridMin_20m", "-130")
        await coreValue(station, "DisplayGridMax_40m", "-60")
        await coreValue(station, "DisplayGridMin_40m", "-120")
        #expect(await settle { band.sharedScale == .init(topDbm: -50, bottomDbm: -130) })
        #expect(display.top == -50 && display.range == 80)
        #expect(band.drawnSettings.scaleTopDbm == -50 && band.drawnSettings.scaleBottomDbm == -130)
        #expect(scale.range(band: 5)?.maxDbm == -50 && scale.range(band: 5)?.minDbm == -130)
        #expect(display.scaleNote == nil && !display.scaleGreyed && !display.scaleUnconfirmed)
        // The phone's own scale is not touched.
        #expect(band.settings.scaleTopDbm == BandDisplaySettings.scaleTopDefaultDbm)

        await slice(station, band: 3)
        #expect(await settle { band.sharedScale == .init(topDbm: -60, bottomDbm: -120) })
        #expect(display.top == -60 && display.range == 60)
        // A band the Core keeps nothing for draws the desktop's default.
        await slice(station, band: 6)
        #expect(await settle { band.sharedScale == .init(topDbm: -40, bottomDbm: -140) })
        // A peer's change on the band shown is drawn at once.
        await slice(station, band: 5)
        await coreValue(station, "DisplayGridMax_20m", "-45")
        #expect(await settle { band.sharedScale == .init(topDbm: -45, bottomDbm: -130) })
        #expect(gridWrites(station).isEmpty)
        await model.disconnect()
    }

    @Test("on Setup's All pans, or a pan the phone does not show, Pan 1's band still follows its slice and the arrows write the new band's key")
    func bandFollowsTheSliceWhateverTheScope() async throws {
        let (model, station) = try await connected()
        let main = model.main
        let band = main.band
        let display = main.display
        let scale = main.bandScale
        main.displayEditScope = .all
        await slice(station, band: 3)
        #expect(await settle { band.settings.scaleBand == "3" && scale.shownBand == 3 })
        display.nudgeTop(up: true)
        #expect(await settle { self.gridWrites(station).last?.key == "DisplayGridMax_40m" })
        main.displayEditScope = .pan(PanIdentity(number: 2))
        await slice(station, band: 6)
        #expect(await settle { band.settings.scaleBand == "6" && scale.shownBand == 6 })
        await model.disconnect()
    }

    // MARK: Written to the Core

    @Test("the arrows, a drag, Top, Range and Setup's dB Max write 20 m's keys, shown at once and unconfirmed until the echo")
    func writes() async throws {
        let (model, station) = try await connected()
        let band = model.main.band
        let display = model.main.display
        let scale = model.main.bandScale
        await coreValue(station, "DisplayGridMax_20m", "-50")
        await coreValue(station, "DisplayGridMin_20m", "-130")
        #expect(await settle { band.sharedScale == .init(topDbm: -50, bottomDbm: -130) })

        // The ▲: 10 dB on the top, the bottom left alone and not sent.
        #expect(DisplaySheetModel.arrowStepDb == 10)
        display.nudgeTop(up: true)
        #expect(display.top == -40 && display.scaleUnconfirmed && scale.isUnconfirmed(band: 5))
        #expect(band.drawnSettings.scaleTopDbm == -40)
        #expect(await settle { !display.scaleUnconfirmed })
        #expect(display.top == -40 && display.range == 90)
        #expect(gridWrites(station).map(\.key) == ["DisplayGridMax_20m"])
        #expect(gridWrites(station).last?.text == "-40")

        // A drag moves both ends, whole dB.
        display.dragScale(topDbm: -30.4, bottomDbm: -120.4)
        #expect(display.top == -30 && display.scaleUnconfirmed)
        #expect(await settle { !display.scaleUnconfirmed })
        #expect(gridWrites(station).suffix(2).map(\.key) == ["DisplayGridMax_20m", "DisplayGridMin_20m"])
        #expect(gridWrites(station).suffix(2).map(\.text) == ["-30", "-120"])
        #expect(scale.range(band: 5)?.maxDbm == -30 && scale.range(band: 5)?.minDbm == -120)

        // Range keeps the top and sends only the bottom.
        display.setRange(70)
        #expect(await settle { !display.scaleUnconfirmed })
        #expect(gridWrites(station).last?.key == "DisplayGridMin_20m" && gridWrites(station).last?.text == "-100")
        #expect(display.top == -30 && display.range == 70)

        // Setup's dB Max, the description's binding, writes the band's dB Max only.
        let suite = "CoreBandScaleTests-keys-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let keys = PhoneSetupKeys(main: model.main, defaults: defaults)
        #expect(keys.value(forPhoneKey: "DisplayGridMax") == .integer(-30))
        let count = gridWrites(station).count
        #expect(keys.set(.integer(-35), forPhoneKey: "DisplayGridMax"))
        #expect(keys.value(forPhoneKey: "DisplayGridMax") == .integer(-35))
        #expect(await settle { !display.scaleUnconfirmed })
        #expect(gridWrites(station).count == count + 1)
        #expect(gridWrites(station).last?.key == "DisplayGridMax_20m" && gridWrites(station).last?.text == "-35")
        #expect(keys.set(.integer(-110), forPhoneKey: "DisplayGridMin"))
        #expect(await settle { scale.range(band: 5)?.minDbm == -110 && !display.scaleUnconfirmed })
        #expect(gridWrites(station).last?.key == "DisplayGridMin_20m")
        // Nothing that equals the Core's value is sent.
        let settled = gridWrites(station).count
        display.setTop(-35)
        await quiet()
        #expect(gridWrites(station).count == settled)
        // The phone's own scale is never written.
        #expect(band.settings.scaleTopDbm == BandDisplaySettings.scaleTopDefaultDbm)
        await model.disconnect()
    }

    @Test("a refused write puts the Core's value back and shows its words until a write is taken")
    func refusal() async throws {
        let (model, station) = try await connected()
        let band = model.main.band
        let display = model.main.display
        let scale = model.main.bandScale
        display.nudgeTop(up: false)
        #expect(await settle { !display.scaleUnconfirmed && scale.range(band: 5)?.maxDbm == -50 })

        station.refuseNextSetting("DisplayGridMax_20m", reason: "r")
        display.nudgeTop(up: false)
        #expect(display.top == -60)
        // The band draws the Core's value once it has looked again.
        #expect(await settle { scale.refusal(band: 5) == "r" && !display.scaleUnconfirmed && display.top == -50 })
        #expect(scale.range(band: 5)?.maxDbm == -50)
        #expect(display.top == -50 && band.sharedScale?.topDbm == -50)
        #expect(model.main.coreDisplay.refusalReason("DisplayGridMax_20m") == "r")
        #expect(display.scaleNote == "r")
        // The Core's display note is the Core's own controls', not this one's.
        #expect(model.main.coreDisplay.note == nil)
        // Another band's refusal is that band's.
        #expect(scale.refusal(band: 3) == nil)

        display.nudgeTop(up: false)
        #expect(await settle { scale.refusal(band: 5) == nil && !display.scaleUnconfirmed && display.top == -60 })
        #expect(display.top == -60 && display.scaleNote == nil)
        #expect(model.main.coreDisplay.refusalReason("DisplayGridMax_20m") == nil)
        await model.disconnect()
    }

    // MARK: While keyed

    @Test("while keyed the arrows and a drag move the transmit grid on this phone; nothing goes to the Core's grid")
    func keyed() async throws {
        let (model, station) = try await connected()
        let band = model.main.band
        let display = model.main.display
        let before = band.settings
        band.transmit = .init(keyedHere: true, carrierHz: 14_200_000)
        #expect(display.keyedScale)
        display.nudgeTop(up: true)
        #expect(band.settings.txGridTopDbm == before.txGridTopDbm + DisplaySheetModel.arrowStepDb)
        display.dragScale(topDbm: 10, bottomDbm: -70)
        #expect(band.settings.txGridTopDbm == 10 && band.settings.txGridRangeDb == 80)
        #expect(display.scaleNote == nil && !display.scaleGreyed && !display.scaleUnconfirmed)
        await quiet()
        #expect(gridWrites(station).isEmpty)
        await model.disconnect()
    }

    // MARK: An older Core

    @Test("below version 29 Top and Range stay on this phone, with the reason, and nothing is sent")
    func olderCore() async throws {
        let (model, station) = try await connected(version: 12)
        let band = model.main.band
        let display = model.main.display
        #expect(!model.main.bandScale.isShared)
        #expect(display.scaleNote == "Needs a newer Core to share the scale")
        #expect(!display.scaleGreyed)
        display.setTop(-60)
        display.nudgeTop(up: false)
        #expect(band.settings.scaleTopDbm == -70 && display.top == -70)
        #expect(band.sharedScale == nil)
        await quiet()
        #expect(gridWrites(station).isEmpty)
        await model.disconnect()
    }

    // MARK: Upgrading

    @Test("each band's scale an older build kept is dropped: nothing is sent and each band shows the Core's")
    func upgrade() async throws {
        let suite = "CoreBandScaleTests-upgrade-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let scales: [String: Any] = ["5": ["topDbm": -70.0, "bottomDbm": -150.0],
                                     "3": ["topDbm": -20.0, "bottomDbm": -90.0]]
        defaults.set(try JSONSerialization.data(withJSONObject: [
            "bandScales": scales, "scaleBand": "5", "scaleTopDbm": -70.0, "scaleBottomDbm": -150.0,
        ] as [String: Any]), forKey: BandDisplaySettingsStore.keyPrefix + BandSubscriber.panId)
        let (model, station) = try await connected(defaults: defaults)
        let band = model.main.band
        await coreValue(station, "DisplayGridMax_20m", "-50")
        await coreValue(station, "DisplayGridMin_20m", "-130")
        #expect(await settle { band.sharedScale == .init(topDbm: -50, bottomDbm: -130) })
        #expect(model.main.display.top == -50)
        await slice(station, band: 3)
        #expect(await settle { band.sharedScale == .init(topDbm: -40, bottomDbm: -140) })
        await quiet()
        #expect(gridWrites(station).isEmpty)
        let data = try #require(defaults.data(forKey: BandDisplaySettingsStore.keyPrefix + BandSubscriber.panId))
        let kept = try #require(try JSONSerialization.jsonObject(with: data) as? [String: Any])
        #expect(kept["bandScales"] == nil)
        await model.disconnect()
    }

    // MARK: The link lost

    @Test("with the link lost the scale rows are greyed with Not connected and nothing is sent")
    func linkLost() async throws {
        let (model, station) = try await connected()
        let band = model.main.band
        let display = model.main.display
        await coreValue(station, "DisplayGridMax_20m", "-50")
        #expect(await settle { band.sharedScale?.topDbm == -50 })
        await station.dropLink()
        #expect(await settle { display.scaleGreyed })
        #expect(display.scaleNote == "Not connected")
        #expect(!display.canRaiseTop && !display.canLowerTop)
        let suite = "CoreBandScaleTests-lost-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let keys = PhoneSetupKeys(main: model.main, defaults: defaults)
        #expect(keys.unavailableReason(forPhoneKey: "DisplayGridMax") == "Not connected")
        #expect(!keys.set(.integer(-30), forPhoneKey: "DisplayGridMax"))
        let count = gridWrites(station).count
        display.nudgeTop(up: true)
        display.setTop(-20)
        display.dragScale(topDbm: -10, bottomDbm: -100)
        await quiet()
        #expect(gridWrites(station).count == count)
        // The band keeps the Core's last scale; the phone's own is not touched.
        #expect(band.sharedScale?.topDbm == -50 && display.top == -50)
        #expect(band.settings.scaleTopDbm == BandDisplaySettings.scaleTopDefaultDbm)
        await model.disconnect()
    }

    // MARK: SWL

    @Test("on band 15 the keys are 90 m's")
    func swl() async throws {
        let (model, station) = try await connected()
        let display = model.main.display
        await slice(station, band: 15)
        #expect(await settle { model.main.bandScale.shownBand == 15 })
        display.nudgeTop(up: false)
        #expect(await settle { !display.scaleUnconfirmed })
        let writes = gridWrites(station)
        #expect(writes.map(\.key) == ["DisplayGridMax_90m", "DisplayGridMin_90m"])
        #expect(writes.map(\.text) == ["-50", "-140"])
        await model.disconnect()
    }
}
