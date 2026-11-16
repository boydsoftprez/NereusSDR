// NereusSDR for iOS: pictures of the SSB Mod Monitor on the TX panel in each state, upright, sideways, at large type and in the iPad column
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The real main screen with the TX panel open and scrolled to the monitor,
/// on a fake Core, in each state: live with the needles, OVER, the Bars style,
/// PA FB, not transmitting, an older Core, not connected, AM with its own
/// monitor; the settings sheet, sideways, large type, and the iPad's panels.
/// Readings are synthetic. With `NEREUS_SSBMOD_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_SSBMOD_SHOTS`), each is written there.
@Suite("SSB Mod Monitor pictures", .serialized)
@MainActor
struct SsbModMonitorShotTests {
    private let center = NotificationCenter()
    static let portrait = CGSize(width: 402, height: 874)
    static let landscape = CGSize(width: 874, height: 402)

    @Test("each state, upright and sideways, at the default and a large text size")
    func states() async throws {
        let rig = try await SsbModMonitorTests.connected(mode: "USB", center: center)
        let monitor = try SsbModMonitorTests.monitor(rig)
        let transmit = rig.model.main.transmit
        transmit.tapPtt()
        #expect(await ModMonitorTests.settle { transmit.ptt.state.isKeyed })

        // Keyed in USB: the needles, as it starts.
        await rig.station.deliverSsbModMonitor(SsbModMonitorTests.reading())
        try await shoot("ssbmod-portrait-live-meters", rig) { monitor.live && monitor.peakText == "-3.0 dBFS" }
        try await shoot("ssbmod-portrait-live-meters-upper", rig, upBy: 0.9)
        try await shoot("ssbmod-landscape", rig, size: Self.landscape)
        try await shoot("ssbmod-landscape-upper", rig, size: Self.landscape, upBy: 0.9)
        try await shoot("ssbmod-portrait-large-type", rig, large: true)
        try await shoot("ssbmod-portrait-large-type-upper", rig, large: true, upBy: 0.9)
        try await column("ssbmod-column-live-meters", rig)
        try await column("ssbmod-column-large-type", rig, large: true)
        try await appletColumn("ssbmod-ipad-applet-column", rig)
        try await frontPanel("ssbmod-ipad-front-panel", rig)

        // The Bars style.
        monitor.setMeterStyle(.bars)
        try await column("ssbmod-column-live-bars", rig)
        monitor.setMeterStyle(.meters)

        // No crest value yet.
        await rig.station.deliverSsbModMonitor(SsbModMonitorTests.reading(crest: nil))
        try await column("ssbmod-column-no-crest", rig) { monitor.live && monitor.crestText == "--" }

        // The settings sheet, from the monitor's Settings button.
        try await sheet("ssbmod-settings-sheet", rig)
        try await sheet("ssbmod-settings-sheet-large-type", rig, large: true)

        // OVER on PA FB, LSB.
        await SsbModMonitorTests.pureSignal(rig)
        #expect(await ModMonitorTests.settle { monitor.pureSignalRunning })
        monitor.choose(.paFeedback)
        try await ModMonitorTests.setMode("LSB", rig)
        await rig.station.deliverSsbModMonitor(SsbModMonitorTests.reading(peak: 96, avg: 41, hold: 96, peakDb: -0.4,
                                                                          avgDb: -7.7, holdDb: -0.4, crest: 7.3),
                                               stream: "txSsbModulationFeedback")
        try await column("ssbmod-column-over-pa-fb-lsb", rig) { monitor.overLit && monitor.chipText?.hasPrefix("LSB") == true }
        monitor.setMeterStyle(.bars)
        try await column("ssbmod-column-over-pa-fb-bars", rig) { monitor.overLit }
        monitor.setMeterStyle(.meters)
        monitor.choose(.txIq)
        await SsbModMonitorTests.pureSignal(rig, on: false)
        try await ModMonitorTests.setMode("USB", rig)

        // In USB, not transmitting.
        transmit.tapPtt()
        #expect(await ModMonitorTests.settle { transmit.ptt.state == .idle })
        await rig.station.endSsbModMonitor()
        try await shoot("ssbmod-portrait-not-transmitting", rig) { !monitor.live && monitor.enabled }
        try await column("ssbmod-column-not-transmitting", rig) { !monitor.live && monitor.enabled }

        // AM: its own monitor, this one gone.
        try await ModMonitorTests.setMode("AM", rig)
        try await column("ssbmod-column-am-has-its-own", rig) { !monitor.shown }
        try await ModMonitorTests.setMode("USB", rig)

        // Not connected.
        await rig.model.disconnect()
        try await column("ssbmod-column-not-connected", rig) { monitor.availability == .notConnected && monitor.shown }

        // An older Core.
        let older = try await SsbModMonitorTests.connected(mode: "USB", center: center, available: false)
        let olderMonitor = try SsbModMonitorTests.monitor(older)
        try await shoot("ssbmod-portrait-older-core", older) { olderMonitor.availability == .olderCore }
        try await column("ssbmod-column-older-core", older) { olderMonitor.availability == .olderCore }
        await older.model.disconnect()
    }

    // MARK: Inside

    /// The main screen with the TX panel open, scrolled to its end, where the monitor is.
    private func shoot(_ name: String, _ rig: ModMonitorTests.Rig, size: CGSize = portrait, large: Bool = false,
                       upBy: CGFloat = 0, ready: () -> Bool = { true }) async throws {
        let root = SsbModMonitorShotRoot(model: rig.model)
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: size, sideways: size.width > size.height, scrollToEnd: true,
                         upBy: upBy, ready: ready)
    }

    /// The TX panel alone at full length: the whole monitor in one picture.
    private func column(_ name: String, _ rig: ModMonitorTests.Rig, large: Bool = false,
                        ready: () -> Bool = { true }) async throws {
        let main = rig.model.main
        let root = TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes,
                           meters: main.band.catalog?.meters, scrolls: false, modMonitor: main.modMonitor,
                           ssbModMonitor: main.ssbModMonitor)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxHeight: .infinity, alignment: .top)
            .background(ChromeColours.panel)
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: CGSize(width: TxPanel.width, height: large ? 2_600 : 1_900), ready: ready)
    }

    /// The iPad's applet column, as the app makes it: the monitor must come through its wiring.
    private func appletColumn(_ name: String, _ rig: ModMonitorTests.Rig) async throws {
        let root = AppletColumn(main: rig.model.main)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: CGSize(width: 340, height: 2_400),
                         expect: "ssbModMonitor")
    }

    /// The iPad's front panel columns, the same.
    private func frontPanel(_ name: String, _ rig: ModMonitorTests.Rig) async throws {
        let root = FrontPanelColumns(main: rig.model.main)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: CGSize(width: 1_000, height: 1_400), expect: "ssbModMonitor")
    }

    private func sheet(_ name: String, _ rig: ModMonitorTests.Rig, large: Bool = false) async throws {
        let monitor = try SsbModMonitorTests.monitor(rig)
        let root = SsbModMonitorSettingsSheet(model: monitor, close: {})
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: Self.portrait)
    }

    private func render(_ view: some View, name: String, size: CGSize, sideways: Bool = false,
                        scrollToEnd: Bool = false, upBy: CGFloat = 0, expect identifier: String? = nil,
                        ready: () -> Bool = { true }) async throws {
        let window = try ModMonitorTests.window(size: size)
        let host = ShotHosting.host(view)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        let wasAccessible = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasAccessible)
        }
        await ShotWait.laidOut(window)
        // The monitor on screen asks the Core; the picture waits for what it shows.
        #expect(await ModMonitorTests.settle(ready), "\(name): the state the picture shows")
        await ShotWait.laidOut(window)
        if let identifier {
            #expect(await ModMonitorTests.settle { SetupTypedEntryTests.element(identifier, in: window) != nil },
                    "\(name): \(identifier) is on screen")
        }
        if scrollToEnd {
            let panel = Self.scrollViews(in: window)
                .filter { abs($0.contentSize.width - TxPanel.width) < 1 && $0.contentSize.height > $0.bounds.height }
                .max { $0.contentSize.height < $1.contentSize.height }
            #expect(panel != nil, "\(name): the TX panel scrolls")
            if let panel {
                let end = max(0, panel.contentSize.height - panel.bounds.height + panel.adjustedContentInset.bottom)
                panel.setContentOffset(CGPoint(x: 0, y: max(0, end - upBy * panel.bounds.height)), animated: false)
            }
            await ShotWait.laidOut(window)
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_SSBMOD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        var pending: [UIView] = [view]
        while let next = pending.popLast() {
            if let scroll = next as? UIScrollView {
                found.append(scroll)
            }
            pending.append(contentsOf: next.subviews)
        }
        return found
    }
}

/// The app's root as `RootView` lays it out, with the TX panel open.
private struct SsbModMonitorShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: true)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
