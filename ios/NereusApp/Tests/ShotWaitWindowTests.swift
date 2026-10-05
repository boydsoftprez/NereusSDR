// NereusSDR for iOS: a previous band window cannot make the next picture ready
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Band screenshot window readiness", .serialized)
@MainActor
struct ShotWaitWindowTests {
    @Test("a previous upright draw cannot ready a new sideways window")
    func previousWindowDoesNotReadyNext() async throws {
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let upright = UIWindow(windowScene: scene)
        upright.frame = CGRect(x: 0, y: 0, width: 402, height: 200)
        upright.rootViewController = UIHostingController(rootView: BandView(model: band))
        upright.isHidden = false
        let uprightDraw = try await ShotWait.requireBandLaidOut(band, in: upright)
        #expect(uprightDraw.requestedPixels == Int(402 * upright.screen.scale))
        upright.isHidden = true
        upright.rootViewController = nil

        let sideways = UIWindow(windowScene: scene)
        sideways.frame = CGRect(x: 0, y: 0, width: 874, height: 200)
        sideways.rootViewController = UIHostingController(rootView: BandView(model: band))
        defer {
            sideways.isHidden = true
            sideways.rootViewController = nil
        }
        // The new window has not been displayed or drawn. A late callback
        // from the old view must not authorize its size or generation.
        band.notePresented(uprightDraw.key, revision: band.revision + 1)
        let readyWithoutADraw = await ShotWait.bandLaidOut(band, in: sideways, seconds: 0.1)
        #expect(readyWithoutADraw == nil)

        sideways.isHidden = false
        let sidewaysDraw = try await ShotWait.requireBandLaidOut(band, in: sideways)
        #expect(sidewaysDraw.viewID != uprightDraw.viewID)
        #expect(sidewaysDraw.width == Int(874 * sideways.screen.scale))
        #expect(sidewaysDraw.width > uprightDraw.width)
        BandFlagShotTests.feed(band, width: sidewaysDraw.requestedPixels, lines: 12)
        let fed = band.revision
        try await ShotWait.requireBandShown(band, in: sideways, after: sidewaysDraw)
        #expect(band.presentedDraws[sidewaysDraw.key].map { $0 >= fed } == true)
        print("upright view=\(uprightDraw.viewID) generation=\(uprightDraw.generation) width=\(uprightDraw.width) revision=\(uprightDraw.revision)")
        print("sideways view=\(sidewaysDraw.viewID) generation=\(sidewaysDraw.generation) width=\(sidewaysDraw.width) revision=\(fed)")
    }

    @Test("a held receive frame leaves the current keyed drawable ready")
    func heldReceiveFrameUsesCurrentReceipt() async throws {
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        band.transmit = .init(keyedHere: true, carrierHz: BandFlagShotTests.centre)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 200)
        window.rootViewController = UIHostingController(rootView: BandView(model: band))
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let draw = try await ShotWait.requireBandLaidOut(band, in: window)
        let before = band.revision
        BandFlagShotTests.feed(band, width: draw.requestedPixels, lines: 1)
        #expect(band.revision == before, "the keyed band holds the receiver's frame")
        try await ShotWait.requireBandShown(band, in: window, after: draw)
        #expect(band.presentedDraws[draw.key].map { $0 >= before } == true)
    }
}
