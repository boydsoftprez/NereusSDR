// NereusSDR for iOS: pictures of the AM Mod Monitor on the TX panel in each state the approved board draws, upright, sideways and at large type
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// D102's pictures, beside the approved board's: the real main screen with
/// the TX panel open and scrolled to the monitor, on a fake Core, in each
/// board state (carrier OK, carrier low with the flashers latched, not
/// transmitting, the Meters style, an older Core, not connected, USB with
/// no monitor), the settings sheet, sideways, and at large type. Readings
/// are synthetic (D4). With `NEREUS_AMMOD_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_AMMOD_SHOTS`), each is written there.
@Suite("AM Mod Monitor pictures", .serialized)
@MainActor
struct ModMonitorShotTests {
    private let center = NotificationCenter()
    static let portrait = CGSize(width: 402, height: 874)
    static let landscape = CGSize(width: 874, height: 402)

    @Test("each board state, upright and sideways, at the default and a large text size")
    func boardStates() async throws {
        let rig = try await ModMonitorTests.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        let transmit = rig.model.main.transmit
        transmit.tapPtt()
        #expect(await ModMonitorTests.settle { transmit.ptt.state.isKeyed })

        // Keyed in AM, carrier OK.
        await rig.station.deliverModMonitor(ModMonitorTests.carrierOk())
        try await shoot("ammod-portrait-carrier-ok", rig) { monitor.live && monitor.posText == "112%" }
        try await shoot("ammod-landscape", rig, size: Self.landscape)
        try await shoot("ammod-portrait-large-type", rig, large: true)
        try await shoot("ammod-portrait-large-type-upper", rig, large: true, upBy: 0.9)
        try await shoot("ammod-landscape-large-type", rig, size: Self.landscape, large: true)
        try await shoot("ammod-landscape-large-type-upper", rig, size: Self.landscape, large: true, upBy: 0.9)
        try await shoot("ammod-landscape-upper", rig, size: Self.landscape, upBy: 0.9)
        try await column("ammod-column-carrier-ok", rig)
        try await column("ammod-column-large-type", rig, large: true)

        // The Meters style.
        monitor.setMeterStyle(.meters)
        try await shoot("ammod-portrait-meters-style", rig)
        monitor.setMeterStyle(.bars)

        // The settings sheet, from the monitor's Settings button.
        try await sheet("ammod-portrait-settings-sheet", rig)
        try await sheet("ammod-portrait-settings-sheet-large-type", rig, large: true)

        // Carrier low on PA feedback, both flashers latched.
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 3, name: "statusJson", value: .utf8(ModMonitorTests.pureSignalRunning)),
        ])))
        #expect(await ModMonitorTests.settle { monitor.pureSignalRunning })
        monitor.choose(.paFeedback)
        await rig.station.deliverModMonitor(ModMonitorTests.carrierLow(), stream: "txAmModulationFeedback")
        try await shoot("ammod-portrait-carrier-low", rig) { monitor.live && monitor.posLit && monitor.negLit }
        try await column("ammod-column-carrier-low", rig)
        monitor.choose(.txIq)

        // In AM, not transmitting.
        transmit.tapPtt()
        #expect(await ModMonitorTests.settle { transmit.ptt.state == .idle })
        await rig.station.endModMonitor()
        try await shoot("ammod-portrait-not-transmitting", rig) { !monitor.live && monitor.enabled }

        // USB: no monitor.
        try await ModMonitorTests.setMode("USB", rig)
        try await shoot("ammod-portrait-usb-no-monitor", rig) { !monitor.shown }
        try await ModMonitorTests.setMode("AM", rig)

        // Not connected.
        await rig.model.disconnect()
        try await shoot("ammod-portrait-not-connected", rig) {
            monitor.availability == .notConnected && monitor.shown
        }

        // An older Core.
        let older = try await ModMonitorTests.connected(mode: "AM", center: center, monitor: false)
        try await shoot("ammod-portrait-older-core", older) {
            older.monitor.availability == .olderCore
        }
        await older.model.disconnect()
    }

    // MARK: Inside

    /// The main screen with the TX panel open, scrolled to its end, where the monitor is.
    private func shoot(_ name: String, _ rig: ModMonitorTests.Rig, size: CGSize = portrait, large: Bool = false,
                       upBy: CGFloat = 0, ready: () -> Bool = { true }) async throws {
        let root = ModMonitorShotRoot(model: rig.model)
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: size, sideways: size.width > size.height, scrollToEnd: true,
                         upBy: upBy, ready: ready)
    }

    /// The TX panel alone at full length: the whole monitor in one picture.
    private func column(_ name: String, _ rig: ModMonitorTests.Rig, large: Bool = false) async throws {
        let main = rig.model.main
        let root = TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes,
                           meters: main.band.catalog?.meters, scrolls: false, modMonitor: main.modMonitor)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxHeight: .infinity, alignment: .top)
            .background(ChromeColours.panel)
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: CGSize(width: TxPanel.width, height: large ? 2_600 : 1_900))
    }

    private func sheet(_ name: String, _ rig: ModMonitorTests.Rig, large: Bool = false) async throws {
        let monitor = try #require(rig.model.main.modMonitor)
        let root = ModMonitorSettingsSheet(model: monitor, close: {})
            .dynamicTypeSize(large ? .accessibility1 : .large)
            .preferredColorScheme(.dark)
        try await render(root, name: name, size: Self.portrait)
    }

    private func render(_ view: some View, name: String, size: CGSize, sideways: Bool = false,
                        scrollToEnd: Bool = false, upBy: CGFloat = 0, ready: () -> Bool = { true }) async throws {
        let window = try ModMonitorTests.window(size: size)
        let host = UIHostingController(rootView: view)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        // The monitor on screen asks the Core; the picture waits for what it shows.
        #expect(await ModMonitorTests.settle(ready), "\(name): the state the picture shows")
        await ShotWait.laidOut(window)
        if scrollToEnd {
            // The TX panel's own scroll view: moved to its end, where the monitor sits.
            // The TX panel's is the tallest scroll view whose content is the panel's width
            // (sideways, the panel's own inset beside the camera widens the view, not the content).
            let panel = Self.scrollViews(in: window)
                .filter { abs($0.contentSize.width - TxPanel.width) < 1 && $0.contentSize.height > $0.bounds.height }
                .max { $0.contentSize.height < $1.contentSize.height }
            #expect(panel != nil, "\(name): the TX panel scrolls, among \(Self.scrollViews(in: window).map { "\($0.bounds.size) \($0.contentSize)" })")
            if let panel {
                let end = max(0, panel.contentSize.height - panel.bounds.height + panel.adjustedContentInset.bottom)
                panel.setContentOffset(CGPoint(x: 0, y: max(0, end - upBy * panel.bounds.height)), animated: false)
            }
            await ShotWait.laidOut(window)
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_AMMOD_SHOTS"], !directory.isEmpty,
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
private struct ModMonitorShotRoot: View {
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
