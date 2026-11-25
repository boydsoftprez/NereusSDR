// NereusSDR for iOS: a flag folded by a tap on its letter, and opened again, on the real band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-15: New. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ, 2026-11-15: a tap on the flag's letter folds it to the one-line tag
/// (letter, frequency, TX badge), and a tap on the tag opens it again; the
/// phone keeps each receiver's choice. The real band view with its flags,
/// hosted on the simulator as ``BandFlagShotTests`` hosts it. With
/// `NEREUS_BAND_SHOTS` set to a directory, each screen is written there.
@Suite("A flag folded by its letter", .serialized)
@MainActor
struct FlagFoldShotTests {
    static let size = CGSize(width: 402, height: 640)

    @Test(arguments: [false, true])
    func theFlagFoldsAndOpens(_ folded: Bool) async throws {
        let catalog = BandFlagShotTests.catalogue()
        let store = MirrorStore(send: { _ in })
        store.apply(BandFlagShotTests.slice(0, active: true))
        store.apply(BandFlagShotTests.slice(1, active: false))
        let slices = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        let band = BandModel()
        band.catalog = catalog
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        let suite = "FlagFoldShotTests.\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        settings.setFlagFolded(folded, letter: "A")
        #expect(settings.flagFolded("A") == folded)

        let window = try BandFlagShotTests.window(size: Self.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: slices, settings: settings,
                             foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
        }
        .frame(width: Self.size.width, height: Self.size.height)
        .background(Color.black)
        let host = ShotHosting.host(root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: Self.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(Self.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        // The layer lays the flags out with the phone's folds. The two
        // slices are too close for two flags: open, A (active) has its flag
        // and B folds; with A folded by its letter, B's flag opens and A's
        // tag drops below it.
        let geometry = try #require(band.pointGeometry(size: Self.size)).geometry
        let placements = FlagLayout.layout(slices: slices.entries.map(\.slice), activeSliceId: 0,
                                           geometry: geometry,
                                           folded: { settings.flagFolded($0.letter) })
        let byId = Dictionary(uniqueKeysWithValues: zip(slices.entries.map(\.id), placements))
        let a = try #require(byId[0])
        let b = try #require(byId[1])
        #expect(a.isFolded == folded)
        #expect(b.isFolded == !folded)
        #expect(!FlagLayout.overlaps(a.rect, b.rect))
        // A's triangle hangs from its tag, below B's flag, when folded.
        let marker = try #require(band.markers.first { $0.sliceId == 0 })
        if folded {
            #expect(marker.triangleTopPoints == a.rect.maxY)
        }

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("flag-\(folded ? "folded" : "open").png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
