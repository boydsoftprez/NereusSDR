// NereusSDR for iOS: the Modes tab on screen for an ANAN-G2 and a Hermes Lite 2, for comparing with the board
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18, R-IOS-27, D15, D17: the real Modes tab, connected to a fake
/// Core that lets this phone transmit, with the board's two slices, the
/// Core's step attenuator and Alex antennas, and first the ANAN-G2's
/// catalogue, then the Hermes Lite 2's, hosted in a window on the
/// simulator. With `NEREUS_MODES_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MODES_SHOTS`), each is written there as a PNG for
/// comparing with `05-tabs.jpg`: the tab as the phone shows it, and the
/// whole page in one tall picture.
@Suite("Modes tab on screen", .serialized)
@MainActor
struct ModesShotTests {
    @Test("the Modes tab for an ANAN-G2 and for a Hermes Lite 2")
    func modesShots() async throws {
        for (fixture, name) in [(ModesTabBindingTests.anan, "anan-g2"), (ModesTabBindingTests.hermesLite, "hermes-lite-2")] {
            let defaults = try #require(UserDefaults(suiteName: "ModesShotTests"))
            let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
            let station = try FakeStation(additions: [.remoteTx])
            await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                                transportFactory: station.transportFactory)
            #expect(await station.waitUntilLive())
            let json = try #require(ModesTabBindingTests.catalogueJSON(fixture))
            await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(json)),
                .init(ordinal: 1, name: "revision", value: .i64(2)),
            ])))
            await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "devices", className: "StationDevicesFacade",
                                                                        properties: [
                .init(ordinal: 2, name: "stationLabel", value: .utf8("KG4VCF/shack")),
            ])))
            // The board's scene: slice A at 7.236.400 LSB on 40 m with NR2 and NB, slice B beside it.
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
                .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
                .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
                .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
                .init(ordinal: 14, name: "band", value: .enumeration(3)),
                .init(ordinal: 58, name: "nbMode", value: .enumeration(1)),
                .init(ordinal: 59, name: "activeNr", value: .enumeration(2)),
                .init(ordinal: 39, name: "ssqlThresh", value: .f64(20)),
                .init(ordinal: 7, name: "afGain", value: .i64(62)),
            ])))
            await station.deliver(BandFlagShotTests.slice(1, active: false))
            try await ModesTabBindingTests.addFrontEnd(station, model: model, catalogue: fixture, radioHardwareVersion: 7)
            #expect(await settle(seconds: 30) {
                model.main.modes.sliceChoices.count == 2 && model.main.modes.attenuationDb != nil
                    && model.main.transmit.settingsEditable && model.main.modes.modeLabel == "LSB"
            })
            // The link's round trip is timed at its first heartbeat, 20 s in.
            _ = await settle(seconds: 30) { model.roundTripMs != nil }

            try await shoot("modes-\(name)", model: model, size: CGSize(width: 402, height: 874), whole: false)
            try await shoot("modes-\(name)-page", model: model, size: CGSize(width: 402, height: 2700), whole: true)
            // A typed offset: the number pad over the page.
            model.main.modes.openRitPad()
            try await shoot("modes-\(name)-rit-pad", model: model, size: CGSize(width: 402, height: 874), whole: false)
            model.main.modes.closePad()
            await model.disconnect()
        }
    }

    // MARK: Inside

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    private func shoot(_ name: String, model: AppModel, size: CGSize, whole: Bool) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = UIHostingController(rootView: ModesShotRoot(model: model, whole: whole).preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MODES_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The Modes tab as `RootView` lays it out over the tab bar, or the whole
/// page unscrolled.
private struct ModesShotRoot: View {
    @ObservedObject var model: AppModel
    let whole: Bool

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: AppTab.modes.title) {
                LinkChip(link: LinkState(connection: model.connection, roundTripMs: model.roundTripMs),
                         core: model.main.coreName)
            }
            if whole {
                ModesPage(model: model.main.modes, micLevel: model.main.micLevel)
                Spacer(minLength: 0)
            } else {
                ScrollView {
                    ModesPage(model: model.main.modes, micLevel: model.main.micLevel)
                }
                .overlay {
                    ValuePadLayer(pad: model.main.modes.pad)
                }
                TabBar(selection: .constant(.modes), sideways: false)
            }
        }
        .background(ChromeColours.panel.ignoresSafeArea())
    }
}
