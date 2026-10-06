// NereusSDR for iOS: approved TX quick-control layout in native phone and iPad views
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Approved native TX panel pictures", .serialized)
@MainActor
struct TxPanelApprovedShotTests {
    @Test("portrait light and dark, large type, landscape, configured blocked equipment and the iPad column")
    func nativeLayoutStates() async throws {
        let station = try FakeStation(additions: [.remoteTx, .accessoryOperate, .txStageReadings])
        let model = AppModel()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        try #require(await station.waitUntilLive())
        try #require(await ShotWait.until { model.connection == .connected })
        await TransmitScreenTests.fillTransmit(station)
        try #require(await ShotWait.until {
            model.main.transmit.rfPower == 100
                && model.mirror.object("amplifier")?["present"] == .bool(true)
                && model.mirror.object("amplifier")?["operate"] == .bool(true)
                && model.mirror.object("tuner")?["isPresent"] == .bool(true)
                && model.mirror.object("tuner")?["isOperate"] == .bool(true)
        })
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(ordinal: 0, name: "connectionPhase", value: .enumeration(6)),
            .init(name: "forwardPowerW", value: .f64(1234)),
            .init(name: "swr", value: .f64(1.23)),
            .init(name: "temperatureC", value: .f64(48.5)),
        ])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(ordinal: 0, name: "connectionPhase", value: .enumeration(6)),
            .init(ordinal: 15, name: "hasAntennaSwitch", value: .bool(true)),
            .init(name: "fwdPower", value: .f64(87)),
            .init(name: "swr", value: .f64(1.45)),
            .init(name: "relayC1", value: .i64(12)),
            .init(name: "relayL", value: .i64(34)),
            .init(name: "relayC2", value: .i64(56)),
        ])))
        model.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(ordinal: 13, name: "antiVoxRun", value: .bool(true)),
            .init(ordinal: 12, name: "antiVoxTauMs", value: .i64(20)),
            .init(ordinal: 76, name: "antiVoxGainDb", value: .i64(0)),
        ])))
        model.main.transmit.refresh()
        model.main.accessories.refresh()
        #expect(model.main.accessories.powerGeniusReadings.forwardW == 1234)
        #expect(model.main.accessories.tunerGeniusReadings.forwardW == 87)
        for dark in [false, true] {
            try await shoot("native-txpanel-portrait-\(dark ? "dark" : "light")", model: model,
                            size: CGSize(width: 402, height: 874), dark: dark)
        }
        try await shoot("native-txpanel-large-type", model: model, size: CGSize(width: 402, height: 874), large: true)
        try await shoot("native-txpanel-landscape", model: model, size: CGSize(width: 874, height: 402))
        try await shoot("native-txpanel-landscape-large", model: model, size: CGSize(width: 874, height: 402), large: true)
        // Same canonical meters: true idle values still have numeric zero/one, never --.
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "forwardPowerW", value: .f64(0)), .init(name: "swr", value: .f64(1)),
            .init(name: "temperatureC", value: .f64(0)),
        ])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "fwdPower", value: .f64(0)), .init(name: "swr", value: .f64(1)),
            .init(name: "relayC1", value: .i64(0)), .init(name: "relayL", value: .i64(0)),
            .init(name: "relayC2", value: .i64(0)),
        ])))
        try await shoot("native-txpanel-equipment-idle-zero", model: model, size: CGSize(width: 402, height: 874), idle: true)
        for key in ["amplifier", "tuner"] {
            model.mirror.apply(.delta(.init(key: key, properties: [
                .init(name: "connectionPhase", value: .enumeration(5)),
            ])))
        }
        try await shoot("native-txpanel-equipment-readings-unavailable", model: model, size: CGSize(width: 402, height: 874), unavailable: true)
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(name: "forwardPowerW", value: .f64(1234)), .init(name: "swr", value: .f64(1.23)),
            .init(name: "temperatureC", value: .f64(48.5)),
        ])))
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(name: "fwdPower", value: .f64(87)), .init(name: "swr", value: .f64(1.45)),
            .init(name: "relayC1", value: .i64(12)), .init(name: "relayL", value: .i64(34)),
            .init(name: "relayC2", value: .i64(56)),
        ])))
        // A switched-off device stays omitted even if its last present value remains.
        for key in ["amplifier", "tuner"] {
            model.mirror.apply(.delta(.init(key: key, properties: [
                .init(ordinal: 0, name: "connectionPhase", value: .enumeration(0)),
            ])))
        }
        try await shoot("native-txpanel-equipment-switched-off", model: model,
                        size: CGSize(width: 402, height: 874), equipmentAbsent: true)
        model.mirror.apply(.delta(.init(key: "tuner", properties: [
            .init(ordinal: 0, name: "connectionPhase", value: .enumeration(6)),
        ])))
        // A configured device retrying retains its disabled controls and actual reason.
        model.mirror.apply(.delta(.init(key: "amplifier", properties: [
            .init(ordinal: 8, name: "present", value: .bool(false)),
            .init(ordinal: 0, name: "connectionPhase", value: .enumeration(5)),
        ])))
        model.main.transmit.refresh()
        model.main.accessories.refresh()
        try await shoot("native-txpanel-configured-blocked", model: model, size: CGSize(width: 402, height: 874))
        try await shoot("native-txpanel-ipad-column", model: model, size: CGSize(width: 340, height: 900), column: true)
        model.settings.apply(.settingsValue(.init(key: "SwrProtectionLimit", origin: "", properties: [
            .init(name: "SwrProtectionLimit", value: .utf8("2.0")),
        ])))
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 11, name: "swr", value: .f64(3.2)),
        ])))
        model.main.transmit.refresh()
        try #require(model.main.transmit.highSwr != nil)
        try await shoot("native-txpanel-high-swr", model: model, size: CGSize(width: 402, height: 874), highSwr: true)
        await model.disconnect()
    }

    private func shoot(_ name: String, model: AppModel, size: CGSize, dark: Bool = true,
                       large: Bool = false, column: Bool = false, equipmentAbsent: Bool = false, highSwr: Bool = false, idle: Bool = false, unavailable: Bool = false) async throws {
        model.main.accessories.refresh()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let main = model.main
        let panel = TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel,
                            modes: main.modes, meters: main.band.catalog?.meters, width: nil,
                            modMonitor: main.modMonitor, take: main.take)
        let root = Group {
            if column { AppletColumn(main: main) } else { panel }
        }
        .preferredColorScheme(dark ? .dark : .light)
        .dynamicTypeSize(large ? .accessibility3 : .large)
        let host = UIHostingController(rootView: root)
        host.view.frame = window.bounds
        let wasAccessible = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true; window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasAccessible)
        }
        await ShotWait.laidOut(window)
        if equipmentAbsent {
            for id in ["txAmpOperate", "txTunerOperate", "tunerTune", "txPgxlPower", "txPgxlSwr", "txPgxlTemperature",
                       "txTgxlPower", "txTgxlSwr", "txTgxlRelay0", "txTgxlRelay1", "txTgxlRelay2"] {
                #expect(SetupTypedEntryTests.element(id, in: window) == nil, "\(id)")
            }
        }
        let scrolls = descendants(window).compactMap { $0 as? UIScrollView }.filter { $0.contentSize.height > $0.bounds.height }
        let scroll = try #require(scrolls.first, "The actual panel parent must scroll all approved controls")
        var targetIDs = ["micLevelGauge", "txStageMeters", "txVox", "txProc"]
            + TxStage.all.map { "txStage-" + $0.property }
        if !equipmentAbsent {
            targetIDs += ["txPgxlPower", "txPgxlSwr", "txPgxlTemperature", "txTgxlPower", "txTgxlSwr",
                          "txTgxlRelay0", "txTgxlRelay1", "txTgxlRelay2"]
        }
        let observed = try await observe(targetIDs, scroll: scroll, window: window)
        scroll.setContentOffset(.zero, animated: false)
        await ShotWait.laidOut(window)
        if !equipmentAbsent {
            let unavailableAmp = main.accessories.powerGeniusReadings.forwardW == nil
            for (id, text) in [
                ("txPgxlPower", unavailableAmp ? "--" : idle ? "0 W" : "1234 W"),
                ("txPgxlSwr", unavailableAmp ? "--" : idle ? "1.00:1" : "1.23:1"),
                ("txPgxlTemperature", unavailableAmp ? "--" : idle ? "0.0 °C" : "48.5 °C"),
                ("txTgxlPower", unavailable ? "--" : idle ? "0 W" : "87 W"), ("txTgxlSwr", unavailable ? "--" : idle ? "1.00:1" : "1.45:1"),
                ("txTgxlRelay0", unavailable ? "--" : idle ? "0" : "12"), ("txTgxlRelay1", unavailable ? "--" : idle ? "0" : "34"), ("txTgxlRelay2", unavailable ? "--" : idle ? "0" : "56"),
            ] {
                let readout = try #require(observed[id], "\(id)")
                #expect(readout.value == text, "\(id)")
            }
        }
        let mic = try #require(observed["micLevelGauge"])
        let stages = try #require(observed["txStageMeters"])
        let voice = try #require(observed["txVox"])
        let processing = try #require(observed["txProc"])
        #expect(mic.minY < stages.minY)
        #expect(stages.minY < voice.minY)
        #expect(voice.minY < processing.minY)
        for stage in TxStage.all {
            #expect(observed["txStage-\(stage.property)"] != nil, "\(stage.property)")
        }
        if !equipmentAbsent {
            let relays = try #require(observed["txTgxlRelay2"])
            #expect(relays.maxY <= mic.minY)
        }
        if highSwr {
            let pill = try #require(SetupTypedEntryTests.element("txHighSwr", in: window))
            #expect(pill.accessibilityFrame.height >= 44)
            #expect(SetupTypedEntryTests.element("txSwrProt", in: window) == nil)
            try save(name, window: window)
            return
        }
        try save(name, window: window)
        // Every scrolled picture is a native UIScrollView offset, after layout has committed.
        for candidate in scrolls {
            print("TX native scroll \(name): \(type(of: candidate)) frame=\(candidate.frame) bounds=\(candidate.bounds) content=\(candidate.contentSize)")
        }
        do {
            for (suffix, fraction) in [("audio-voice", 0.45), ("processing-profile", 1.0)] {
                let offset = max(scroll.contentSize.height - scroll.bounds.height, 0) * fraction
                scroll.setContentOffset(CGPoint(x: 0, y: offset), animated: false)
                await ShotWait.laidOut(window)
                if fraction == 1 {
                    let viewport = scroll.convert(scroll.bounds, to: nil)
                    let settings = try #require(SetupTypedEntryTests.element("txSettings", in: window))
                    print("TX native pin \(name): offset=\(scroll.contentOffset) viewport=\(viewport) Settings=\(settings.accessibilityFrame)")
                    let pinTop = column ? viewport.minY : window.convert(window.bounds, to: nil).minY
                    #expect(settings.accessibilityFrame.minY >= pinTop - 1)
                    #expect(settings.accessibilityFrame.maxY <= viewport.maxY + 1,
                            "Settings stays pinned in the actual phone or iPad parent scroll")
                    for id in ["txTune", "txMox", "txTwoTone", "txPsa"] {
                        let key = try #require(SetupTypedEntryTests.element(id, in: window))
                        #expect(key.accessibilityFrame.height >= 44)
                        #expect(key.accessibilityFrame.minY >= pinTop - 1, "\(id)")
                        #expect(key.accessibilityFrame.maxY <= viewport.maxY + 1, "\(id)")
                    }
                }
                try save(name + "-" + suffix, window: window)
            }
        }
    }

    private struct ObservedTarget {
        let minY: CGFloat
        let maxY: CGFloat
        let value: String?
    }

    /// Materialize SwiftUI's real accessibility nodes by moving its actual parent scroll.
    private func observe(_ ids: [String], scroll: UIScrollView, window: UIWindow) async throws -> [String: ObservedTarget] {
        var result: [String: ObservedTarget] = [:]
        let maximum = max(scroll.contentSize.height - scroll.bounds.height, 0)
        let step = max(scroll.bounds.height / 2, 1)
        let count = Int(ceil(maximum / step))
        for index in 0...count {
            let offset = min(CGFloat(index) * step, maximum)
            scroll.setContentOffset(CGPoint(x: 0, y: offset), animated: false)
            await ShotWait.laidOut(window)
            let viewport = scroll.convert(scroll.bounds, to: nil)
            for id in ids where result[id] == nil {
                guard let element = SetupTypedEntryTests.element(id, in: window) else { continue }
                let frame = element.accessibilityFrame
                guard frame.height > 0, frame.intersects(viewport) else { continue }
                let target = ObservedTarget(minY: frame.minY + scroll.contentOffset.y,
                                            maxY: frame.maxY + scroll.contentOffset.y,
                                            value: element.accessibilityValue)
                result[id] = target
                print("TX observed \(id): offset=\(scroll.contentOffset.y) viewport=\(viewport) frame=\(frame) documentY=\(target.minY)...\(target.maxY)")
            }
        }
        for id in ids { try #require(result[id] != nil, "The actual scroll must expose \(id)") }
        return result
    }

    private func descendants(_ view: UIView) -> [UIView] {
        view.subviews.flatMap { [$0] + descendants($0) }
    }

    private func save(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        let data = try #require(image.pngData())
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try data.write(to: url)
        print("Wrote \(url.path)")
    }
}
