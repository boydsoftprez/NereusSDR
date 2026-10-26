// NereusSDR for iOS: native keyed meter targets, selection, missing sources and review pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import MetalKit
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Keyed meter overlay on screen", .serialized)
@MainActor
struct KeyedMeterOverlayShotTests {
    @Test("keyed overlay exposes Radio and Custom tabs as 44 point targets")
    func keyedTabs() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        let saved = UserDefaults.standard.data(forKey: KeyedMeterSettings.key)
        UserDefaults.standard.removeObject(forKey: KeyedMeterSettings.key)
        defer {
            SetupTypedEntryTests.setApplicationAccessibility(accessibility)
            if let saved { UserDefaults.standard.set(saved, forKey: KeyedMeterSettings.key) }
            else { UserDefaults.standard.removeObject(forKey: KeyedMeterSettings.key) }
        }
        let defaults = try #require(UserDefaults(suiteName: "KeyedOverlayShots-\(UUID().uuidString)"))
        let platform = TestPlatform()
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults), platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx, .txStageReadings, .accessoryOperate])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.main.slices.entries.count == 2 })
        // FakeStation delivery drains the session, while AppModel consumes its
        // messages on a separate pump. Wait for fillTransmit's accessory creates
        // before applying the local no-equipment deltas below.
        try #require(await settle {
            model.mirror.isSnapshotComplete && !model.mirror.isStale
                && model.mirror.capabilityVersion("txReadingsVersion") == TxStage.readingsVersion
                && model.mirror.object("amplifier")?["present"] == .bool(true)
                && model.mirror.object("tuner")?["isPresent"] == .bool(true)
        })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        // No equipment configured: tabs and choices must be omitted, not fake zero gauges.
        for (key, present) in [("amplifier", "present"), ("tuner", "isPresent")] {
            model.mirror.apply(.delta(.init(key: key, properties: [
                .init(name: "connectionPhase", value: .enumeration(0)),
                .init(name: "configuredHost", value: .utf8("")), .init(name: present, value: .bool(false))])))
        }
        model.main.accessories.refresh()
        model.main.transmit.tapPtt()
        #expect(await settle { model.main.transmit.ptt.transmitting })
        model.mirror.apply(TransmitScreenTests.txStateDelta([
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(86)),
            .init(ordinal: 11, name: "swr", value: .f64(1.18)),
            .init(ordinal: 13, name: "micLevelDb", value: .f64(-8.4)),
            .init(ordinal: 37, name: "eqDb", value: .f64(-4.2)), .init(ordinal: 42, name: "alcGainDb", value: .f64(4.1))]))
        model.main.transmit.refresh()
        for (name, sideways, light, large) in [
            ("pttm-radio-dark", false, false, false), ("pttm-radio-light", false, true, false),
            ("pttm-radio-large", false, false, true), ("pttm-radio-landscape", true, false, false),
            ("pttm-radio-landscape-large", true, false, true)
        ] {
            try await shoot(name, model: model, sideways: sideways, light: light, large: large) { window in
                #expect(SetupTypedEntryTests.element("keyedTab-Amp", in: window) == nil)
                #expect(SetupTypedEntryTests.element("keyedTab-Tuner", in: window) == nil)
                #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "86 W")
            }
        }
        try await shoot("pttm-no-equipment-picker", model: model) { window in
            try await activate("keyedMeterSlot-0", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterChoice-ampPower", in: window) == nil)
            #expect(SetupTypedEntryTests.element("keyedMeterChoice-tunerPower", in: window) == nil)
            try await activate("keyedMeterDone", in: window)
            #expect(model.main.keyedMeters.settings.meters(on: .radio) == [.rfPower, .swr, .mic])
            try await activate("keyedMeterSlot-0", in: window)
        }
        for (name, operate, watts) in [("operate-keyed", true, 42.0), ("standby-keyed", false, 98.0)] {
            model.mirror.apply(.delta(.init(key: "amplifier", properties: [
                .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true)),
                .init(name: "operate", value: .bool(operate)), .init(name: "transmitting", value: .bool(true)),
                .init(name: "forwardPowerW", value: .f64(1236)), .init(name: "swr", value: .f64(1.12))])))
            model.mirror.apply(.delta(.init(key: "txState", properties: [
                .init(name: "forwardPowerWatts", value: .f64(watts)), .init(name: "swr", value: .f64(1.12))])))
            model.main.transmit.refresh()
            model.main.accessories.refresh()
            try await shoot("meter-overlay-" + name, model: model) { window in
                try await activate("keyedTab-Radio", in: window)
                #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == (operate ? "1236 W" : "98 W"))
                #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "1.12")
            }
        }
        model.main.transmit.tapPtt()
        try #require(await settle { !model.main.transmit.ptt.transmitting })
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "operate", value: .bool(true)), .init(name: "transmitting", value: .bool(false))])))
        model.main.transmit.refresh()
        model.main.accessories.refresh()
        try await shoot("meter-overlay-operate-idle", model: model, expectedKeyed: false) { window in
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window) == nil)
        }
        model.main.transmit.tapPtt()
        try #require(await settle { model.main.transmit.ptt.transmitting })
        // Real nullable source mappings: only current present/connected equipment supplies them.
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true)),
            .init(name: "transmitting", value: .bool(true)), .init(name: "forwardPowerW", value: .f64(850)), .init(name: "swr", value: .f64(1.12)),
            .init(name: "temperatureC", value: .f64(42))])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "isPresent", value: .bool(true)),
            .init(name: "fwdPower", value: .f64(830)), .init(name: "swr", value: .f64(1.08))])))
        model.main.accessories.refresh()
        let before = significantCommands(station)
        try await shoot("pttm-amp", model: model) { window in
            try await activate("keyedTab-Amp", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "850 W")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "1.12")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-2", in: window)?.accessibilityValue == "42 °C")
        }
        try await shoot("pttm-tuner", model: model) { window in
            try await activate("keyedTab-Tuner", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "830 W")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "1.08")
        }
        try await shoot("pttm-custom-picker", model: model) { window in
            try await activate("keyedTab-Custom", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "-4.2 dB")
            try await activate("keyedMeterSlot-0", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterDone", in: window) != nil)
        }
        try await shoot("pttm-custom", model: model) { window in
            try await activate("keyedMeterSlot-0", in: window)
            try await activate("keyedMeterChoice-rfPower", in: window)
            #expect(model.main.keyedMeters.settings.meters(on: .custom) == [.rfPower, .eq, .alcGain])
            #expect(model.main.keyedMeters.settings.meters(on: .radio) == [.rfPower, .swr, .mic])
            try await activate("keyedMeterSlot-1", in: window)
            try await activate("keyedMeterDone", in: window)
            #expect(model.main.keyedMeters.settings.meters(on: .custom) == [.rfPower, .eq, .alcGain])
            // Reach the last choice through the actual native ScrollView, not offscreen AX activation.
            try await activate("keyedMeterSlot-1", in: window)
            let picker = try #require(SetupTypedEntryTests.element("keyedMeterChooser", in: window))
            let scroll = try #require(descendants(window).compactMap { $0 as? UIScrollView }
                .filter { $0.contentSize.height > $0.bounds.height }
                .max { $0.contentSize.height < $1.contentSize.height })
            scroll.setContentOffset(CGPoint(x: 0, y: max(0, scroll.contentSize.height - scroll.bounds.height)), animated: false)
            await ShotWait.laidOut(window)
            let lastChoice = try #require(SetupTypedEntryTests.element("keyedMeterChoice-alcGroup", in: window))
            #expect(picker.accessibilityFrame.contains(lastChoice.accessibilityFrame))
            try await activate("keyedMeterChoice-alcGroup", in: window)
            #expect(model.main.keyedMeters.settings.meters(on: .custom) == [.rfPower, .alcGroup, .alcGain])
            await TransmitScreenTests.barrier(model, station)
            #expect(significantCommands(station) == before, "Meter selection sent a command")
            // Unkey closes the chooser and rekey restores this phone's page and slots.
            try await activate("keyedMeterSlot-0", in: window)
            model.main.transmit.tapPtt()
            #expect(await settle { !model.main.transmit.ptt.transmitting })
            await ShotWait.laidOut(window)
            #expect(SetupTypedEntryTests.element("keyedMeterDone", in: window) == nil)
            model.main.transmit.tapPtt()
            #expect(await settle { model.main.transmit.ptt.transmitting })
            await ShotWait.laidOut(window)
            #expect(model.main.keyedMeters.settings.page == .custom)
            #expect(model.main.keyedMeters.settings.meters(on: .custom) == [.rfPower, .alcGroup, .alcGain])
        }
        await TransmitScreenTests.barrier(model, station)
        #expect(significantCommands(station) == before + Array(repeating: "tx.unkey", count: PttController.copies) + Array(repeating: "tx.key", count: PttController.copies))
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "connectionPhase", value: .enumeration(0)),
            .init(name: "configuredHost", value: .utf8("configured-fixture.invalid")),
            .init(name: "present", value: .bool(false))])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "connectionPhase", value: .enumeration(5))])))
        model.main.accessories.refresh()
        for sideways in [false, true] {
            try await shoot("pttm-offline-\(sideways ? "landscape" : "portrait")", model: model,
                            sideways: sideways, large: true) { window in
                let page = model.main.keyedMeters.settings.page
                _ = SetupTypedEntryTests.element("keyedTab-Amp", in: window)?.accessibilityActivate()
                #expect(model.main.keyedMeters.settings.page == page)
                #expect(model.main.accessories.rfKitReadings.forwardW == nil)
            }
        }
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties:
            TransmitScreenTests.holder(TransmitScreenTests.thisDeviceId, short: "iPhone", keyed: true)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0))])))
        model.main.transmit.refresh()
        for sideways in [false, true] {
            try await shoot("pttm-missing-picker-\(sideways ? "landscape" : "portrait")", model: model,
                            sideways: sideways, large: true) { window in
                #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "--")
                try await activate("keyedMeterSlot-0", in: window)
            }
        }
        model.main.transmit.tapPtt()
        await model.disconnect()
    }

    /// DC15 as amended after the PR 653 review (the manager's ruling,
    /// 2026-10-25, for JJ's veto): on the sideways phone the PTT moved past
    /// a slice flag keys there and stays there for the whole key, whatever
    /// the flags do, so a second tap at the same spot lands on it and
    /// unkeys. The keyed meters narrow beside it on the wider side (option
    /// 1). Unkeyed it takes its usual place again.
    @Test("sideways, keyed where the PTT moved past a flag, it stays and a tap at that spot unkeys")
    func keyedPttStaysWhereKeyed() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let defaults = try #require(UserDefaults(suiteName: "KeyedOverlayHeld-\(UUID().uuidString)"))
        let platform = TestPlatform()
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults), platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx, .txStageReadings])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.main.slices.entries.count == 2 })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        #expect(!model.main.transmit.ptt.transmitting)
        await TransmitScreenTests.barrier(model, station)
        let before = significantCommands(station)

        try await shoot("ptt-held-landscape", model: model, sideways: true, expectedKeyed: false) { window in
            // The receive rule in the band's points, put on the screen: where
            // the PTT goes for the flags as they stand.
            let bandView = try #require(descendants(window).compactMap { $0 as? MTKView }.first)
            let band = window.convert(window.convert(bandView.bounds, from: bandView), to: window.screen.coordinateSpace)
            let home = PttButton.frame(bandSize: band.size, leadingInset: 62)
            // The button column down the band's left, which reaches the PTT's row here.
            let column = CGRect(x: 62 + BandColumnLayout.origin, y: 0,
                                width: BandColumnLayout.columnWidth(columns: BandColumnLayout.columns(bandHeight: band.height)),
                                height: band.height)
            @MainActor func usual() -> CGRect {
                BandColumnLayout.pttFrame(home: home, avoid: [column], flags: model.main.slices.flagRects,
                                          maxX: band.width - 62 - PttButton.inset)
                    .offsetBy(dx: band.minX, dy: band.minY)
            }
            @MainActor func ptt() -> CGRect? { SetupTypedEntryTests.element("ptt", in: window)?.accessibilityFrame }
            // Receiving: flag A reaches the PTT's row and the PTT has moved past it.
            #expect(await settle {
                guard let a = SetupTypedEntryTests.element("flagA", in: window)?.accessibilityFrame,
                      let ptt = ptt() else { return false }
                return ptt.minX > a.maxX && a.maxY > ptt.minY && a.minY < ptt.maxY
            }, "the PTT did not move past flag A")
            await ShotWait.laidOut(window)
            let receive = try #require(await element("ptt", in: window)).accessibilityFrame
            #expect(receive.minX > home.minX + band.minX + 100, "the PTT is not mid-band: \(receive)")
            #expect(abs(receive.minX - usual().minX) < 0.5 && abs(receive.minY - usual().minY) < 0.5,
                    "the receive rule does not match the screen: \(receive) vs \(usual())")
            let spot = CGPoint(x: receive.midX, y: receive.midY)

            // Keyed (whatever keyed it): the PTT stays put.
            model.main.transmit.tapPtt()
            try #require(await settle { model.main.transmit.ptt.transmitting })
            await ShotWait.laidOut(window)
            #expect(ptt() == receive, "the PTT moved when transmitting began: \(String(describing: ptt()))")
            // The flags move while keyed (slice A tuned down 3 kHz, which
            // would bring the receiving PTT left with it): the PTT stays.
            let flagA = try #require(await element("flagA", in: window)).accessibilityFrame
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(7_233_400))])))
            #expect(await settle {
                (SetupTypedEntryTests.element("flagA", in: window)?.accessibilityFrame.minX ?? flagA.minX)
                    < flagA.minX - 20
            }, "flag A did not move")
            await ShotWait.laidOut(window)
            let held = try #require(ptt())
            #expect(held == receive, "the PTT moved while keyed: \(held), keyed at \(receive)")
            #expect(held.contains(spot))
            // The keyed meters beside it, on the wider side, narrowed to fit
            // with 44 point targets and clear of it (geometry's checks).
            let gauges = try #require(SetupTypedEntryTests.element("keyedGauges", in: window)).accessibilityFrame
            #expect(gauges.width < KeyedOverlay.gaugesWidth && gauges.width >= KeyedOverlay.gaugesNarrowest,
                    "the keyed meters are not narrowed: \(gauges)")
            #expect(gauges.maxY > held.minY && !gauges.intersects(held), "the meters are not beside the PTT: \(gauges)")
            #expect(gauges.maxX <= held.minX - BandColumnLayout.clearance + 0.5
                        || gauges.minX >= held.maxX + BandColumnLayout.clearance - 0.5)
            try geometry(window, expectedKeyed: true)
            // The PTT is what is drawn at the keyed spot, over any flag that
            // opened under it, and SwiftUI hands a touch to what is drawn on top.
            let face = pixel(at: CGPoint(x: spot.x, y: spot.y + 26), in: window)
            #expect(face.red > 0.5 && face.red > 2 * face.green && face.red > 2 * face.blue,
                    "the keyed PTT is not on top at its spot: \(face)")
            try save(window, name: "ptt-held-landscape-keyed")

            // A second tap at the same spot: the PTT takes it and unkeys.
            let button = try #require(SetupTypedEntryTests.element("ptt", in: window))
            #expect(button.accessibilityFrame.contains(spot))
            #expect(button.accessibilityActivate(), "the PTT did not take the tap")
            try #require(await settle { !model.main.transmit.ptt.transmitting })
            await ShotWait.laidOut(window)

            // Unkeyed it takes its usual place again, for the flags as they stand now.
            #expect(await settle {
                guard let frame = ptt() else { return false }
                return abs(frame.minX - usual().minX) < 0.5 && frame.minX != receive.minX
            }, "the PTT did not come back to its usual place after unkey: \(String(describing: ptt())), usual \(usual())")
        }
        await TransmitScreenTests.barrier(model, station)
        #expect(significantCommands(station) == before + Array(repeating: "tx.key", count: PttController.copies)
                + Array(repeating: "tx.unkey", count: PttController.copies))
        await model.disconnect()
    }

    /// The colour drawn at a point on the screen, as the review pictures draw it.
    private func pixel(at point: CGPoint, in window: UIWindow) -> (red: CGFloat, green: CGFloat, blue: CGFloat) {
        let local = window.convert(point, from: window.screen.coordinateSpace)
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        let image = UIGraphicsImageRenderer(bounds: CGRect(origin: local, size: CGSize(width: 1, height: 1)),
                                            format: format).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        var bytes = [UInt8](repeating: 0, count: 4)
        guard let cgImage = image.cgImage,
              let context = CGContext(data: &bytes, width: 1, height: 1, bitsPerComponent: 8, bytesPerRow: 4,
                                      space: CGColorSpaceCreateDeviceRGB(),
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return (0, 0, 0)
        }
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: 1, height: 1))
        return (CGFloat(bytes[0]) / 255, CGFloat(bytes[1]) / 255, CGFloat(bytes[2]) / 255)
    }

    private func element(_ id: String, in window: UIWindow) async -> NSObject? {
        for _ in 0..<250 {
            if let found = SetupTypedEntryTests.element(id, in: window) { return found }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return nil
    }

    private func significantCommands(_ station: FakeStation) -> [String] {
        station.messages.compactMap(TransmitScreenTests.invoke).map(\.verb)
            .filter { $0 != "tx.keepalive" && !$0.hasPrefix("test.barrier.") }
    }

    private func activate(_ id: String, in window: UIWindow) async throws {
        let element = try #require(SetupTypedEntryTests.element(id, in: window), "missing \(id)")
        #expect(element.accessibilityActivate(), "\(id) did not activate")
        await ShotWait.laidOut(window)
    }

    private func shoot(_ name: String, model: AppModel, sideways: Bool = false, light: Bool = false,
                       large: Bool = false, expectedKeyed: Bool = true, check: (UIWindow) async throws -> Void) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let window = try BandFlagShotTests.window(size: size)
        if sideways { window.frame.origin.y = 120 }
        let host = ShotHosting.host(KeyedMeterShotRoot(model: model)
            .environment(\.dynamicTypeSize, large ? .accessibility1 : .large)
            .preferredColorScheme(light ? .light : .dark))
        if sideways { host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62) }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer { window.isHidden = true; window.rootViewController = nil }
        let draw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        BandFlagShotTests.feed(model.main.band, width: draw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(model.main.band, in: window, after: draw)
        await ShotWait.laidOut(window)
        try await check(window)
        await ShotWait.laidOut(window)
        try geometry(window, expectedKeyed: expectedKeyed)
        try save(window, name: name)
        // An open chooser belongs to this view's key, never the next render's view instance.
    }

    private func save(_ window: UIWindow, name: String) throws {
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty {
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            try image.pngData()?.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
            print("Wrote keyed overlay \(name)")
        }
    }

    private func geometry(_ window: UIWindow, expectedKeyed: Bool) throws {
        let bandView = try #require(descendants(window).compactMap { $0 as? MTKView }.first)
        let local = window.convert(bandView.bounds, from: bandView)
        let band = window.convert(local, to: window.screen.coordinateSpace)
        let ptt = try #require(SetupTypedEntryTests.element("ptt", in: window)).accessibilityFrame
        let tabs = try #require(SetupTypedEntryTests.element("Sections", in: window)).accessibilityFrame
        for id in ["keyedTab-Radio", "keyedTab-Custom", "keyedMeterSlot-0", "keyedMeterSlot-1", "keyedMeterSlot-2", "keyedGauges", "keyedMeterChooser", "keyedMeterDone"] {
            guard let element = SetupTypedEntryTests.element(id, in: window) else {
                #expect(!expectedKeyed || !["keyedTab-Radio", "keyedTab-Custom", "keyedMeterSlot-0", "keyedMeterSlot-1", "keyedMeterSlot-2"].contains(id), "missing \(id)")
                continue
            }
            let frame = element.accessibilityFrame
            #expect(!frame.isEmpty, "empty \(id)")
            #expect(band.insetBy(dx: -1, dy: -1).contains(frame), "\(id) outside band: \(frame), band \(band)")
            #expect(!frame.intersects(ptt), "\(id) covers PTT: \(frame), PTT \(ptt)")
            #expect(!frame.intersects(tabs), "\(id) covers tab bar")
            if id.hasPrefix("keyedTab-") || id.hasPrefix("keyedMeterSlot-") || id == "keyedMeterDone" {
                #expect(frame.height >= 44 && frame.width >= 44, "\(id) below minimum touch size")
            }
        }
    }

    private func descendants(_ view: UIView) -> [UIView] {
        [view] + view.subviews.flatMap(descendants)
    }
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<1500 {
            if condition() { return true }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}

private struct KeyedMeterShotRoot: View {
    @ObservedObject var model: AppModel
    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
