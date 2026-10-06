// NereusSDR for iOS: what a test picture waits for before it is taken: the band laid out and its frame on screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import MetalKit
import NereusBand
@testable import NereusSDR
import QuartzCore
import Testing
import UIKit

/// The pictures in the tests wait on what they draw, never on time: a fixed
/// sleep that is long enough on a quiet computer is too short on a busy one,
/// and the picture (and any pixel checked in it) then shows a band not yet
/// drawn. Each wait is bounded at 30 s, and a picture that never becomes
/// ready fails a `#require` before any PNG is taken.
@MainActor
enum ShotWait {
    /// The current Metal view and its completed drawable at the size the
    /// window laid out. The Core's row width comes from this draw, not from
    /// the model's last view (which may have belonged to another window).
    struct BandDrawing {
        let key: BandModel.DrawKey
        let requestedPixels: Int
        let revision: UInt64

        var viewID: UUID { key.viewID }
        var generation: UUID { key.generation }
        var width: Int { key.width }
        var height: Int { key.height }
    }

    static func bandLaidOut(_ band: BandModel, in window: UIWindow, seconds: Double = 30) async -> BandDrawing? {
        var ready: BandDrawing?
        let reached = await until(seconds: seconds) {
            window.layoutIfNeeded()
            guard let view = bandView(for: band, in: window),
                  let size = drawablePixels(of: view),
                  let owner = view.delegate as? BandView.Coordinator,
                  let key = owner.currentDrawKey,
                  key.width == size.width, key.height == size.height else { return false }
            guard let revision = band.presentedDraws[key] else { return false }
            let requested = BandGeometry.requestedPixels(forWidthPixels: Double(size.width))
            guard band.requestedPixels == requested else { return false }
            ready = BandDrawing(key: key, requestedPixels: requested, revision: revision)
            return true
        }
        return reached ? ready : nil
    }

    static func requireBandLaidOut(_ band: BandModel, in window: UIWindow) async throws -> BandDrawing {
        try #require(await bandLaidOut(band, in: window), "This window's band did not finish a draw at its size")
    }

    /// The current revision completed on the same view and drawable size,
    /// whether the feed advanced it or was deliberately held while keyed.
    static func bandShown(_ band: BandModel, in window: UIWindow, after ready: BandDrawing,
                          seconds: Double = 30) async -> Bool {
        let fed = band.revision
        // A keyed band deliberately holds receive frames. In that case the
        // current revision was already drawn before the attempted feed.
        return await until(seconds: seconds) {
            guard let view = bandView(for: band, in: window),
                  let size = drawablePixels(of: view),
                  let owner = view.delegate as? BandView.Coordinator,
                  owner.currentDrawKey == ready.key,
                  size.width == ready.width, size.height == ready.height else {
                return false
            }
            return band.presentedDraws[ready.key].map { $0 >= fed } ?? false
        }
    }

    static func requireBandShown(_ band: BandModel, in window: UIWindow, after ready: BandDrawing) async throws {
        try #require(await bandShown(band, in: window, after: ready),
                     "This window's band did not present the current revision")
    }

    static func bandView(for band: BandModel, in window: UIWindow) -> MTKView? {
        var pending = window.subviews
        while let view = pending.popLast() {
            if let metal = view as? MTKView, metal.window === window,
               let owner = metal.delegate as? BandView.Coordinator, owner.model === band {
                return metal
            }
            pending.append(contentsOf: view.subviews)
        }
        return nil
    }

    private static func drawablePixels(of view: MTKView) -> (width: Int, height: Int)? {
        let scale = view.contentScaleFactor
        let expectedWidth = Int((view.bounds.width * scale).rounded())
        let expectedHeight = Int((view.bounds.height * scale).rounded())
        let width = Int(view.drawableSize.width.rounded())
        let height = Int(view.drawableSize.height.rounded())
        guard width > 1, height > 1, abs(width - expectedWidth) <= 1,
              abs(height - expectedHeight) <= 1 else { return nil }
        return (width, height)
    }

    /// A window with no band: its layout done and the updates SwiftUI queued
    /// for it run, twice over for updates that queue more.
    static func laidOut(_ window: UIWindow) async {
        for _ in 0 ..< 2 {
            window.layoutIfNeeded()
            CATransaction.flush()
            await MainQueue.drained()
        }
    }

    /// Checks `condition` on each turn of the main actor, up to 30 s.
    static func until(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let limit = Date().addingTimeInterval(seconds)
        while Date() < limit {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(10))
        }
        return condition()
    }
}
