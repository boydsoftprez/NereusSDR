// NereusSDR for iOS: the waterfall carries on when the phone turns, upright to sideways and back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusMedia
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11: turning the phone changes the band's width, so the band asks
/// the Core for wider or narrower lines. The waterfall keeps its history
/// through the turn, re-mapped to the new width, and carries on scrolling;
/// it never starts blank. With `NEREUS_BAND_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_BAND_SHOTS`), the band before and after
/// each turn is written there.
@Suite("The waterfall through a turn", .serialized)
@MainActor
struct WaterfallTurnShotTests {
    static let upright = CGSize(width: 402, height: 640)
    static let sideways = CGSize(width: 874, height: 300)

    @Test("the waterfall keeps its history when the phone turns, and carries on scrolling")
    func turnKeepsTheHistory() async throws {
        let band = BandModel()
        band.catalog = BandFlagShotTests.catalogue()
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        let window = try BandFlagShotTests.window(size: Self.upright)
        let host = UIHostingController(rootView: AnyView(Self.root(band, size: Self.upright)))
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: Self.upright)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // The band is as wide as the window: its rows are the window's width
        // in pixels, as the Core is asked for them.
        let scale = window.screen.scale
        func rowPixels(_ size: CGSize) -> Int {
            BandGeometry.requestedPixels(forWidthPixels: Double(size.width * scale))
        }

        // Upright: the band fills its waterfall.
        let uprightDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        let uprightWidth = uprightDraw.requestedPixels
        #expect(uprightWidth == rowPixels(Self.upright))
        BandFlagShotTests.feed(band, width: uprightWidth, lines: 400)
        try await ShotWait.requireBandShown(band, in: window, after: uprightDraw)
        #expect(band.state.history.columns == uprightWidth)
        let uprightLines = band.state.history.count
        #expect(uprightLines > 300)
        try shoot(window, "waterfall-1-upright")

        // Turned sideways: the band is wider, and the Core's lines follow.
        turn(window, host: host, band: band, to: Self.sideways)
        let sidewaysDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        let sidewaysWidth = sidewaysDraw.requestedPixels
        #expect(sidewaysDraw.width > uprightDraw.width)
        #expect(sidewaysWidth == rowPixels(Self.sideways))
        #expect(band.requestedPixels == rowPixels(Self.sideways))
        BandFlagShotTests.feed(band, width: sidewaysWidth, lines: 12)
        try await ShotWait.requireBandShown(band, in: window, after: sidewaysDraw)
        let history = band.state.history
        #expect(history.columns == sidewaysWidth)
        // The lines from before the turn are still there, under the new ones.
        #expect(history.count == min(uprightLines + 12, history.capacity))
        #expect(history.count > 12)
        try shoot(window, "waterfall-2-sideways-after-turn")

        // And back upright.
        turn(window, host: host, band: band, to: Self.upright)
        let uprightAgain = try await ShotWait.requireBandLaidOut(band, in: window)
        #expect(uprightAgain.width == uprightDraw.width)
        BandFlagShotTests.feed(band, width: uprightAgain.requestedPixels, lines: 12)
        try await ShotWait.requireBandShown(band, in: window, after: uprightAgain)
        #expect(band.state.history.columns == uprightWidth)
        #expect(band.state.history.count > 24)
        try shoot(window, "waterfall-3-upright-after-turning-back")
    }

    // MARK: Inside

    private static func root(_ band: BandModel, size: CGSize) -> some View {
        BandView(model: band)
            .frame(width: size.width, height: size.height)
            .background(Color.black)
    }

    private func turn(_ window: UIWindow, host: UIHostingController<AnyView>, band: BandModel,
                      to size: CGSize) {
        window.frame = CGRect(origin: .zero, size: size)
        host.rootView = AnyView(Self.root(band, size: size))
        host.view.frame = CGRect(origin: .zero, size: size)
        // The caller awaits this view's draw at the new size before feeding.
    }

    private func shoot(_ window: UIWindow, _ name: String) throws {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
