// NereusSDR for iOS: the keyed view's arithmetic, the transmit window and the band drawn while keyed
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import Testing

/// Task 54f (R-IOS-11): the keyed view is centred on the carrier at the
/// phone's own span, held within 48 kHz either side of it, never under
/// 1 kHz, and follows the carrier; the transmit window is the transmit
/// grid widened by the transmit levels, rounded outward to a tenth of a dB;
/// while keyed the band is drawn with the transmit grid, levels and palette.
@Suite struct TransmitDisplayTests {
    static let carrier = 14_200_000.0

    @Test func theFirstKeyIsFourKilohertzEitherSideOfTheCarrier() {
        let view = TransmitDisplay.firstView(carrierHz: Self.carrier, spanHz: TransmitDisplay.firstSpanHz)
        #expect(view == TuneGestures.View(centerHz: Self.carrier, spanHz: 8_000))
        #expect(BandDisplaySettings.desktopDefaults.txViewSpanHz == 8_000)
    }

    @Test func theViewIsHeldWithinTheTransmitDisplaysReach() {
        // Wider than the reach: the whole of it, about the carrier.
        let wide = TransmitDisplay.clamped(.init(centerHz: Self.carrier + 5_000, spanHz: 200_000),
                                           carrierHz: Self.carrier)
        #expect(wide == .init(centerHz: Self.carrier, spanHz: 96_000))
        // Panned past the top: back so its top edge is 48 kHz over the carrier.
        let panned = TransmitDisplay.clamped(.init(centerHz: Self.carrier + 60_000, spanHz: 10_000),
                                             carrierHz: Self.carrier)
        #expect(panned == .init(centerHz: Self.carrier + 43_000, spanHz: 10_000))
        let under = TransmitDisplay.clamped(.init(centerHz: Self.carrier - 60_000, spanHz: 10_000),
                                            carrierHz: Self.carrier)
        #expect(under == .init(centerHz: Self.carrier - 43_000, spanHz: 10_000))
        // Under 1 kHz: 1 kHz.
        let narrow = TransmitDisplay.clamped(.init(centerHz: Self.carrier, spanHz: 200), carrierHz: Self.carrier)
        #expect(narrow.spanHz == 1_000)
        // Inside: untouched.
        let inside = TuneGestures.View(centerHz: Self.carrier + 1_500, spanHz: 6_000)
        #expect(TransmitDisplay.clamped(inside, carrierHz: Self.carrier) == inside)
    }

    @Test func theViewFollowsTheCarrier() {
        let view = TuneGestures.View(centerHz: Self.carrier + 1_000, spanHz: 8_000)
        let moved = TransmitDisplay.followed(view, from: Self.carrier, to: Self.carrier + 2_500)
        #expect(moved == .init(centerHz: Self.carrier + 3_500, spanHz: 8_000))
    }

    @Test func theTransmitWindowIsTheGridWidenedByTheLevels() {
        // The desktop's defaults: grid 20 down 100 (to -80), levels -70 to 30.
        #expect(BandDisplaySettings.desktopDefaults.txDbmWindow == -80...30)
        #expect(TransmitDisplay.dbmWindow(gridTopDbm: -10.04, gridRangeDb: 50, waterfallLowDbm: -40,
                                          waterfallHighDbm: -20) == -60.1 ... -10.0)
        // Rounded outward.
        let window = TransmitDisplay.dbmWindow(gridTopDbm: 12.34, gridRangeDb: 100, waterfallLowDbm: -95.55,
                                               waterfallHighDbm: 0)
        #expect(abs(window.lowerBound - -95.6) < 1e-9)
        #expect(abs(window.upperBound - 12.4) < 1e-9)
        // Held within -400 to 100.
        #expect(TransmitDisplay.dbmWindow(gridTopDbm: 200, gridRangeDb: 200, waterfallLowDbm: -500,
                                          waterfallHighDbm: 150) == -400...100)
    }

    @Test func keyedTheBandIsDrawnWithTheTransmitGridLevelsAndPalette() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.waterfallLevelMode = .clarity
        settings.useSpectrumMinMax = true
        settings.noiseFloorLine = true
        settings.enterBand("5")
        settings.scaleTopDbm = -50
        let keyed = settings.keyedOverlay
        #expect(keyed.scaleRange == -80...20)
        #expect(keyed.waterfallLevelMode == .manual)
        #expect(!keyed.useSpectrumMinMax)
        #expect(keyed.phoneLevels.lowDbm == -70 && keyed.phoneLevels.highDbm == 30)
        #expect(keyed.waterfallPaletteId == 1)
        #expect(!keyed.noiseFloorLine)
        // The peak-hold trace stays keyed only with Update during TX on.
        #expect(!keyed.activePeakHold && !keyed.peakBlobs)
        var peaks = settings
        peaks.activePeakHold = true
        peaks.peakBlobs = true
        #expect(!peaks.keyedOverlay.activePeakHold && !peaks.keyedOverlay.peakBlobs)
        peaks.activePeakHoldOnTx = true
        #expect(peaks.keyedOverlay.activePeakHold && !peaks.keyedOverlay.peakBlobs)
        // The receive scale each band keeps is not touched.
        #expect(keyed.bandScales == settings.bandScales)
        #expect(settings.scaleTopDbm == -50)
    }

    @Test @MainActor func settingsKeptBeforeTheTransmitDisplayReadItsDefaults() throws {
        let defaults = try #require(UserDefaults(suiteName: "TransmitDisplayTests"))
        defaults.removePersistentDomain(forName: "TransmitDisplayTests")
        defer { defaults.removePersistentDomain(forName: "TransmitDisplayTests") }
        // A pan kept by an earlier version: no transmit display members.
        defaults.set(Data(#"{"grid": false, "desktopValuesVersion": 1}"#.utf8),
                     forKey: BandDisplaySettingsStore.keyPrefix + "1")
        let read = BandDisplaySettingsStore(defaults: defaults).settings(forPan: "1")
        #expect(!read.grid)
        #expect(read.txGridTopDbm == 20 && read.txGridRangeDb == 100)
        #expect(read.txWaterfallLowDbm == -70 && read.txWaterfallHighDbm == 30)
        #expect(read.txWaterfallPaletteId == 1 && read.txViewSpanHz == 8_000 && !read.displayDuplex)
    }
}
