// NereusSDR for iOS: a display frame redraws the band alone, never the screens over it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import CoreGraphics
import Foundation
import NereusMedia
@testable import NereusSDR
import Testing

/// The Core's frames come up to 30 a second. Each redraws the band's Metal
/// view; the flags, spots and readouts over the band are built again only
/// when something they show changes. Built again for every frame, they kept
/// the phone working hard enough to grow hot, and a hot phone halves the
/// band's frame rate (``SessionPolicy``).
@Suite("Band redraws")
@MainActor
struct BandRedrawTests {
    /// A band on the Core's context, with its first frame in.
    private static func band() throws -> BandModel {
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        // A phone-sized band keeps a phone-sized waterfall.
        _ = band.prepareToDraw(size: CGSize(width: 400, height: 800), scale: 1)
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 1)))
        return band
    }

    @Test("a display frame redraws the band and leaves the screens over it alone")
    func framesRedrawOnlyTheBand() throws {
        let band = try Self.band()
        var screens = 0
        var draws = 0
        let watches = [band.objectWillChange.sink { _ in screens += 1 }, band.draws.sink { draws += 1 }]
        for sequence in 2...31 {
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
        }
        #expect(draws == 30)
        #expect(screens <= 1, "only the frame-rate readout's once a second, at most; got \(screens)")
        _ = watches
    }

    @Test("a change the screens show still reaches them, and redraws the band")
    func screenChangesStillReachTheScreens() throws {
        let band = try Self.band()
        var screens = 0
        var draws = 0
        let watches = [band.objectWillChange.sink { _ in screens += 1 }, band.draws.sink { draws += 1 }]
        band.markAway("Sound only")
        #expect(screens >= 1 && draws == 1)
        _ = watches
    }

    @Test("a sound-only mark moving down the waterfall redraws only the marks' layer")
    func awayMarksMoveOnTheirOwn() throws {
        let band = try Self.band()
        band.markAway("Sound only")
        var screens = 0
        var marks = 0
        let watches = [band.objectWillChange.sink { _ in screens += 1 },
                       band.away.objectWillChange.sink { _ in marks += 1 }]
        for sequence in 2...11 {
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
        }
        #expect(marks == 10)
        #expect(screens <= 1)
        #expect(band.awayMarks.first?.linesSince == 10)
        _ = watches
    }
}
