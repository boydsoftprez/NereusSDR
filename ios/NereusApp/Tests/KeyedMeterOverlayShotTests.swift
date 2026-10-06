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
        // Real nullable source mappings: only current present/connected equipment supplies them.
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true)),
            .init(name: "forwardPowerW", value: .f64(850)), .init(name: "swr", value: .f64(1.12)),
            .init(name: "temperatureC", value: .f64(42))])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "isPresent", value: .bool(true)),
            .init(name: "fwdPower", value: .f64(830)), .init(name: "swr", value: .f64(1.08))])))
        model.main.accessories.refresh()
        let before = significantCommands(station)
        try await shoot("pttm-amp", model: model) { window in
            try await activate("keyedTab-Amp", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "850 W")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "1.12:1")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-2", in: window)?.accessibilityValue == "42 °C")
        }
        try await shoot("pttm-tuner", model: model) { window in
            try await activate("keyedTab-Tuner", in: window)
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-0", in: window)?.accessibilityValue == "830 W")
            #expect(SetupTypedEntryTests.element("keyedMeterSlot-1", in: window)?.accessibilityValue == "1.08:1")
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
                       large: Bool = false, check: (UIWindow) async throws -> Void) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let window = try BandFlagShotTests.window(size: size)
        if sideways { window.frame.origin.y = 120 }
        let host = UIHostingController(rootView: KeyedMeterShotRoot(model: model)
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
        try geometry(window)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty {
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            try image.pngData()?.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
            print("Wrote keyed overlay \(name)")
        }
        // An open chooser belongs to this view's key, never the next render's view instance.
    }

    private func geometry(_ window: UIWindow) throws {
        let bandView = try #require(descendants(window).compactMap { $0 as? MTKView }.first)
        let local = window.convert(bandView.bounds, from: bandView)
        let band = window.convert(local, to: window.screen.coordinateSpace)
        let ptt = try #require(SetupTypedEntryTests.element("ptt", in: window)).accessibilityFrame
        let tabs = try #require(SetupTypedEntryTests.element("Sections", in: window)).accessibilityFrame
        for id in ["keyedTab-Radio", "keyedTab-Custom", "keyedMeterSlot-0", "keyedMeterSlot-1", "keyedMeterSlot-2", "keyedGauges", "keyedMeterChooser", "keyedMeterDone"] {
            guard let element = SetupTypedEntryTests.element(id, in: window) else {
                #expect(!["keyedTab-Radio", "keyedTab-Custom", "keyedMeterSlot-0", "keyedMeterSlot-1", "keyedMeterSlot-2"].contains(id), "missing \(id)")
                continue
            }
            let frame = element.accessibilityFrame
            #expect(!frame.isEmpty, "empty \(id)")
            #expect(band.insetBy(dx: -1, dy: -1).contains(frame), "\(id) outside band: \(frame), band \(band)")
            #expect(!frame.intersects(ptt), "\(id) covers PTT")
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
