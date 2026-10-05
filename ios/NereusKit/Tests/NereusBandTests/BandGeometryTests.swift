// NereusSDR for iOS: tests for the band's axes: frequency, dBm and trace samples to points
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
@testable import NereusBand

/// R-IOS-11: the band's axes.
@Suite struct BandGeometryTests {
    static let band = BandGeometry(centerHz: 7_236_400, spanHz: 48_000, size: CGSize(width: 1179, height: 600),
                                   dbmRange: -140 ... -40)

    @Test func frequencyRoundTripsWithinOneHertzAcrossTheSpan() {
        let band = Self.band
        var hz = band.lowHz
        while hz <= band.highHz {
            #expect(abs(band.hz(forX: band.x(forHz: hz)) - hz) <= 1)
            hz += 997
        }
        #expect(band.x(forHz: band.lowHz) == 0)
        #expect(abs(band.x(forHz: band.highHz) - 1179) < 1e-9)
        #expect(abs(band.x(forHz: band.centerHz) - 589.5) < 1e-9)
    }

    @Test func dbmRunsDownFromTheTop() {
        let band = Self.band
        #expect(band.y(forDbm: -40) == 0)
        #expect(band.y(forDbm: -140) == 600)
        #expect(band.y(forDbm: -90) == 300)
        #expect(abs(band.dbm(forY: band.y(forDbm: -97.5)) + 97.5) < 1e-9)
    }

    @Test func zoomChangesTheSpanAboutTheChosenPoint() {
        let band = Self.band
        let chosen = 7_230_000.0
        let before = band.x(forHz: chosen)
        let zoomedIn = band.zoomed(by: 0.5, about: chosen)
        #expect(zoomedIn.spanHz == 24_000)
        #expect(abs(zoomedIn.x(forHz: chosen) - before) < 1e-6)
        let zoomedOut = band.zoomed(by: 3, about: chosen)
        #expect(zoomedOut.spanHz == 144_000)
        #expect(abs(zoomedOut.x(forHz: chosen) - before) < 1e-6)
        // A factor that means nothing changes nothing.
        #expect(band.zoomed(by: 0, about: chosen) == band)
        #expect(band.zoomed(by: .nan, about: chosen) == band)
    }

    @Test(arguments: [1179, 600, 257, 1])
    func aTraceSampleSitsAtTheFrequencyAtItsCentre(_ traceSamples: Int) {
        let band = Self.band
        for index in [0, traceSamples / 3, traceSamples - 1] {
            let hz = band.hz(forTraceSample: index, traceSamples: traceSamples)
            #expect(abs(band.x(forTraceSample: index, traceSamples: traceSamples) - band.x(forHz: hz)) < 1e-6)
        }
        // A grant smaller than the width still spreads over the whole band.
        #expect(band.x(forTraceSample: 0, traceSamples: traceSamples) == 0.5 * 1179 / Double(traceSamples))
    }

    @Test func theRequestedWidthIsThePixelWidthWithinTheDocumentsRange() {
        #expect(BandGeometry.requestedPixels(forWidthPixels: 1179) == 1179)
        #expect(BandGeometry.requestedPixels(forWidthPixels: 1178.6) == 1179)
        #expect(BandGeometry.requestedPixels(forWidthPixels: 0) == 1)
        #expect(BandGeometry.requestedPixels(forWidthPixels: 5000) == 4096)
        #expect(BandGeometry.requestedPixels(forWidthPixels: .infinity) == 1)
    }

    @Test func gridTicksAreOneTwoOrFiveStepsInsideTheSpan() {
        let band = Self.band
        let ticks = band.frequencyTicks(minimumSpacing: 64 * 3)
        #expect(ticks.stepHz == 10_000)
        #expect(ticks.ticks == [7_220_000, 7_230_000, 7_240_000, 7_250_000, 7_260_000])
        #expect(BandGeometry.niceStep(atLeast: 1_000) == 1_000)
        #expect(BandGeometry.niceStep(atLeast: 1_001) == 2_000)
        #expect(BandGeometry.niceStep(atLeast: 3_000) == 5_000)
        #expect(BandGeometry.niceStep(atLeast: 7_000) == 10_000)
        #expect(band.dbmTicks(stepDb: 10) == [-40, -50, -60, -70, -80, -90, -100, -110, -120, -130, -140])
    }

    @Test func frequencyLabelsAreMegahertz() {
        #expect(BandGeometry.frequencyLabel(hz: 7_225_000, stepHz: 5_000) == "7.225")
        #expect(BandGeometry.frequencyLabel(hz: 7_225_500, stepHz: 500) == "7.2255")
        #expect(BandGeometry.frequencyLabel(hz: 14_100_000, stepHz: 100_000) == "14.100")
    }

    @Test func theLayoutStacksSpectrumStripScaleAndWaterfall() {
        let layout = BandLayout(size: CGSize(width: 1179, height: 1200), scale: 3)
        #expect(layout.spectrum.minY == 0)
        #expect(layout.strip.maxY == layout.spectrum.maxY)
        // Small, the default: 6 points plus 4, at 3x; it stops at the dBm scale.
        #expect(layout.strip.height == 30)
        #expect(layout.strip.maxX == layout.dbmScale.minX)
        #expect(layout.frequencyScale.minY == layout.spectrum.maxY)
        #expect(layout.waterfall.minY == layout.frequencyScale.maxY)
        #expect(layout.waterfall.maxY == 1200)
        #expect(layout.dbmScale.maxX == 1179)
        #expect(layout.dbmScale.width == 120)
        #expect(layout.waterfallLines == Int(layout.waterfall.height))
        let geometry = layout.spectrumGeometry(centerHz: 7_200_000, spanHz: 50_000, dbmRange: -140 ... -40)
        #expect(geometry.size.width == 1179)
        #expect(geometry.size.height == layout.strip.minY)
    }
}
