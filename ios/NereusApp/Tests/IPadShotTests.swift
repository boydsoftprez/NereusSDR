// NereusSDR for iOS: the iPad on its side and upright, for comparing with the board's pictures 18 and 19
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

/// R-IOS-24, D30, D31: the real main screen and tab bar on an 11-inch iPad,
/// connected to a fake Core with the ANAN-G2 catalogue and two slices where
/// the board draws them. On its side the band gives the applet column its
/// 320 points and takes them back when the column hides; upright the band
/// keeps the whole width over the front panel. With `NEREUS_IPAD_SHOTS` set
/// to a directory (through `TEST_RUNNER_NEREUS_IPAD_SHOTS`), each screen is
/// written there as a PNG for comparing with `18-ipad-landscape.jpg` and
/// `19-ipad-upright.jpg`. It runs on an iPad simulator only.
@Suite("iPad on screen", .serialized)
@MainActor
struct IPadShotTests {
    struct Screen {
        let name: String
        let onItsSide: Bool
        let columnShown: Bool

        /// The 11-inch iPad's screen, either way up.
        var size: CGSize {
            onItsSide ? CGSize(width: 1210, height: 834) : CGSize(width: 834, height: 1210)
        }

        /// The band's width in the layout, in points.
        var bandWidth: CGFloat {
            let layout: IPadLayout = onItsSide ? .column : .frontPanel
            return layout.bandWidth(screenWidth: size.width, columnShown: columnShown)
        }
    }

    @Test("the iPad on its side with the column, without it, and upright with the front panel",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func iPadShots() async throws {
        let defaults = try #require(UserDefaults(suiteName: "IPadShotTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // The board's reading: S8, -79 dBm, on slice A's flag and on the S-meter.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-79)),
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-79)),
        ])))
        #expect(await settle(seconds: 30) {
            model.main.slices.entries.count == 2 && model.main.coreName != nil
                && model.main.slices.active?.signalPeakDbm == -79
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })

        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        for screen in [Screen(name: "ipad-landscape", onItsSide: true, columnShown: true),
                       Screen(name: "ipad-landscape-column-hidden", onItsSide: true, columnShown: false),
                       Screen(name: "ipad-upright", onItsSide: false, columnShown: true)] {
            try await shoot(screen, model: model)
        }
        await model.disconnect()
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

    private func shoot(_ screen: Screen, model: AppModel) async throws {
        let size = screen.size
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        // On its side the window sits clear of the upright status bar and
        // home indicator, and takes the iPad's own on-its-side safe area.
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.onItsSide ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: screen.columnShown)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if screen.onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        // The band asks for its width in pixels: the column's 320 points are
        // the band's while the column hides, and upright it has the whole width.
        let scale = window.screen.scale
        #expect(band.requestedPixels == Int((screen.bandWidth * scale).rounded()))
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(band.markers.count == 2)

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out on an iPad, the column shown or not.
private struct IPadShotRoot: View {
    @ObservedObject var model: AppModel
    let columnShown: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, columnShown: columnShown)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: true)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
