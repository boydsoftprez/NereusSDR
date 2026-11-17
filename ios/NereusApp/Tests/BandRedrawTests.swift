// NereusSDR for iOS: a display frame redraws the band alone, never the screens over it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-11-16: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//   each change the band draws asks for its own draw; the marks follow the Update period.

import Combine
import CoreGraphics
import Foundation
import NereusLink
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
        #expect(band.awayMarks.first?.linesSince == 9)
        _ = watches
    }

    /// The band's view no longer draws on every update of the screen: each
    /// change it draws asks for its own draw.
    @Test("each change the band draws asks for a draw: settings, band plan, gates, pause, and the media closing")
    func eachDrawnChangeAsksForADraw() throws {
        let band = try Self.band()
        let catalog = try #require(BandFlagShotTests.catalogue())
        var draws = 0
        let watch = band.draws.sink { draws += 1 }
        defer { watch.cancel() }
        func drawn(_ what: String, _ change: () -> Void) {
            let before = draws
            change()
            #expect(draws > before, "\(what) asked for no draw")
        }
        drawn("a display setting") {
            var settings = band.settings
            settings.noiseFloorLine.toggle()
            band.settings = settings
        }
        drawn("the band plan") { band.catalog = catalog }
        drawn("the media gates") { band.gates = MediaFeatureGates(agreedMinor: 11) { _ in 1 } }
        drawn("Pause") { band.paused = true }
        drawn("Pause off") { band.paused = false }
        drawn("the media closing") { band.receive(.mediaState(.closed)) }
        drawn("the media failing") { band.receive(.mediaState(.failed)) }
        // An unchanged setting draws nothing new.
        let before = draws
        band.settings = band.settings
        band.paused = false
        #expect(draws == before)
    }

    @Test("the media closing on the transmit display draws the band with its receive settings again")
    func mediaClosingLeavesTheTransmitDisplay() throws {
        let band = try Self.band()
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("tests"), "endpointId": .number(1),
            "revision": .number(2), "contextGeneration": .number(2), "sourceStream": .number(0),
            "sourceCentreHz": .number(14_200_000), "sampleRateHz": .number(96_000),
            "centreHz": .number(14_200_000), "spanHz": .number(8_000), "wideCentreHz": .number(0),
            "wideSpanHz": .number(0), "traceSamples": .number(64), "waterfallSamples": .number(64),
            "wideSamples": .number(0), "minDbm": .number(-80), "maxDbm": .number(30), "fps": .number(15),
            "framesPerLine": .number(1), "grantedFftSize": .number(32_768), "grantedTier": .string("wide"),
            "requestedPixels": .number(64), "grantedPixels": .number(64), "limit": .string("none"),
            "transmit": .bool(true),
        ]
        let context = try #require(MediaControlDecoder.context(payload, wideband: false, grant: true, transmit: true))
        band.receive(.context(context))
        try #require(band.showsTransmit)
        var draws = 0
        let watch = band.draws.sink { draws += 1 }
        defer { watch.cancel() }
        band.receive(.mediaState(.closed))
        #expect(!band.showsTransmit && !band.transmitOverlay)
        #expect(draws >= 1, "the band kept the transmit display's look")
    }

    @Test("a sound-only mark moves when the Update period moves its rows")
    func awayMarksFollowTheUpdatePeriod() throws {
        let band = try Self.band()
        band.markAway("Sound only")
        var marks = 0
        let watch = band.away.objectWillChange.sink { _ in marks += 1 }
        defer { watch.cancel() }
        var settings = band.settings
        settings.waterfallPeriodMs += 20
        band.settings = settings
        #expect(marks >= 1)
        #expect(band.away.placement.waterfallPeriodMs == settings.waterfallPeriodMs)
    }
}
