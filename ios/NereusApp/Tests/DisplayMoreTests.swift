// NereusSDR for iOS: the rest of the desktop's display on this phone: CTUN, look back, readouts, detectors and Setup's controls
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-11, R-IOS-27 (Task 54e part 2): the parity audit's rows 14, 16,
/// 19, 21, 22, 24, 28, 30, 31 and 34 in the app.
@Suite("The rest of the display", .serialized)
@MainActor
struct DisplayMoreTests {
    typealias Rig = DisplaySheetTests.Rig

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<3_000 {
            await Task.yield()
        }
    }

    /// `lines` frames of 16 samples, the older half at -120 dBm and the
    /// newer at -40, each advancing the waterfall.
    private func feed(_ band: BandModel, lines: Int, from first: Int = 1) {
        for sequence in first..<(first + lines) {
            let level: Float = sequence - first < lines / 2 ? -120 : -40
            band.receive(.displayFrame(BandFixturesApp.frame(level: level, sequence: UInt32(sequence))))
        }
    }

    // MARK: Row 34: CTUN

    @Test("CTUN starts on; off, the band follows the active slice wherever it tunes")
    func ctun() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.ctun)
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        // CTUN on: a tune inside the view leaves the view where it is.
        rig.main.band.requestView(TuneGestures.View(centerHz: 7_240_000, spanHz: 48_000))
        func tune(_ hz: Double) {
            rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            ])))
        }
        tune(7_245_000)
        await quiet()
        #expect(rig.main.band.requestedView?.centerHz == 7_240_000)
        // Off: the band goes to the slice at once, and follows it.
        display.setCtun(false)
        #expect(!rig.main.band.settings.ctun)
        #expect(rig.main.band.requestedView?.centerHz == 7_245_000)
        tune(7_247_000)
        #expect(await settle { rig.main.band.requestedView?.centerHz == 7_247_000 })
        let again = try Rig(suite: rig.suite)
        #expect(!again.main.display.ctun)
    }

    // MARK: Row 14: look back, LIVE and the time

    @Test("Look back holds the waterfall on the same lines as new ones arrive; LIVE brings it back")
    func lookBack() async throws {
        let rig = try Rig()
        await quiet()
        let band = rig.main.band
        band.endpointId = 1
        band.receive(.context(try #require(TuningShotTests.context(centreHz: 7_236_400))))
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        let visible = band.visibleLines
        // A screen of lines at the split's least spectrum, and the lines kept for looking back.
        var lowest = band.settings
        lowest.currentSpectrumSharePercent = BandDisplaySettings.spectrumShareRange.lowerBound
        let tallest = BandLayout(size: CGSize(width: 1206, height: 1500), scale: 3, settings: lowest).waterfallLines
        #expect(tallest > visible)
        #expect(band.state.history.capacity == min(tallest + band.settings.rewindLines,
                                                   BandDisplaySettings.waterfallRowsLimit))
        #expect(band.state.history.capacity <= BandDisplaySettings.waterfallRowsLimit)
        feed(band, lines: visible + 200)
        let display = rig.main.display
        #expect(display.lookBackSeconds == 0)
        #expect(DisplaySheetModel.lookBackText(0) == "Live")
        #expect(display.lookBackRange.max >= 5)
        display.setLookBack(seconds: 3)
        #expect(band.lookBackLines == 100)
        #expect(DisplaySheetModel.lookBackText(display.lookBackSeconds) == "-0:03")
        // Ten more lines: still on the same ones, ten further back.
        feed(band, lines: 10, from: visible + 201)
        #expect(band.lookBackLines == 110)
        #expect(band.overlays(scale: 3).lookBackLines == 110)
        // Never past what it holds less a screen.
        display.setLookBack(seconds: 10_000)
        #expect(band.lookBackLines == band.state.history.count - visible)
        display.goLive()
        #expect(band.lookBackLines == 0)
        #expect(BandReadouts.liveText == "LIVE")
    }

    @Test("dragging the split keeps the waterfall's lines and its storage: only the lines on screen follow the split")
    func splitDragKeepsTheWaterfall() async throws {
        let rig = try Rig()
        await quiet()
        let band = rig.main.band
        band.endpointId = 1
        band.receive(.context(try #require(TuningShotTests.context(centreHz: 7_236_400))))
        let size = CGSize(width: 1206, height: 1500)
        _ = band.prepareToDraw(size: size, scale: 3)
        feed(band, lines: 120)
        // Sized once for the tallest waterfall the split allows, and the lines kept for looking back.
        var lowest = band.settings
        lowest.currentSpectrumSharePercent = BandDisplaySettings.spectrumShareRange.lowerBound
        let tallest = BandLayout(size: size, scale: 3, settings: lowest).waterfallLines
        let capacity = band.state.history.capacity
        #expect(capacity == min(tallest + band.settings.rewindLines, BandDisplaySettings.waterfallRowsLimit))
        let generation = band.state.history.layoutGeneration
        for percent in [42.0, 47, 55, 63, 80, 20, 35] {
            band.settings.currentSpectrumSharePercent = percent
            _ = band.prepareToDraw(size: size, scale: 3)
            #expect(band.state.history.capacity == capacity, "at \(percent)%")
            #expect(band.state.history.layoutGeneration == generation, "at \(percent)%")
            #expect(band.state.history.count == 120)
            #expect(band.visibleLines == BandLayout(size: size, scale: 3, settings: band.settings).waterfallLines)
        }
    }

    @Test("the readouts word the frames a second, the bin width, the finger's frequency, the peak and the time")
    func readouts() async throws {
        #expect(BandReadouts.fpsText(29.96) == "30.0 fps")
        #expect(BandReadouts.binWidthText(46.875) == "46.875 Hz/bin")
        #expect(BandReadouts.cursorText(7_236_460) == "7.2365 MHz")
        #expect(BandReadouts.timeText(1_758_931_200 + 3_661, utc: true) == "01:01:01 UTC")
        let rig = try Rig()
        await quiet()
        let band = rig.main.band
        var now = 100.0
        band.clock = { now }
        band.endpointId = 1
        band.receive(.context(try #require(TuningShotTests.context(centreHz: 7_236_400, spanHz: 48_000))))
        var trace = [Float](repeating: -120, count: 100)
        trace[75] = -63.6
        band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: 1,
                                                producerTimestamp: 1, isKeyframe: true, waterfallAdvance: true,
                                                minDbm: -160, maxDbm: 0, traceDbm: trace, waterfallDbm: trace,
                                                wideDbm: [])))
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        let expectedHz = BandCoverage(centerHz: 7_236_400, spanHz: 48_000).hz(forSample: 75, samples: 100)
        #expect(BandReadouts.peakText(band: band) == "Peak -63.6 dBm, \(String(format: "%.4f", expectedHz / 1e6)) MHz")
        // The frames a second, counted on the band's own clock.
        for sequence in 2...40 {
            now += 1.0 / 30
            band.receive(.displayFrame(BandFixturesApp.frame(level: -100, sequence: UInt32(sequence))))
        }
        #expect(band.framesPerSecond.map { abs($0 - 30) < 1.5 } == true)
    }

    // MARK: Grid noise-floor tracking (Setup description V12, the Core's rule)

    @Test("the grid's minimum follows the Core's floors by the V12 rule, drawn and never kept")
    func gridTracksTheNoiseFloor() async throws {
        let rig = try Rig()
        await quiet()
        let band = rig.main.band
        var now = 100.0
        band.clock = { now }
        band.endpointId = 1
        band.receive(.context(try #require(TuningShotTests.context(centreHz: 7_236_400, spanHz: 48_000))))
        rig.main.changeDisplay {
            $0.gridFollowsNoiseFloor = true
            $0.clarityEnabled = false
            $0.gridNoiseFloorOffsetDb = -5
        }
        let kept = band.settings.scaleRange
        func extras(_ floor: Float, fastAttack: Bool?, sequence: UInt32) -> MediaControlEvent {
            .displayExtras(DisplayExtras(endpointId: 1, contextGeneration: 1, encoderSequence: sequence,
                                         noiseFloorDbm: floor, noiseFloorFastAttack: fastAttack))
        }
        band.receive(extras(-120, fastAttack: false, sequence: 1))
        #expect(band.drawnSettings.scaleRange == -125 ... kept.upperBound)
        #expect(band.settings.scaleRange == kept)
        // In fast attack the grid holds.
        now += 1
        band.receive(extras(-100, fastAttack: true, sequence: 2))
        #expect(band.drawnSettings.scaleRange.lowerBound == -125)
        // With Clarity on it follows the Core's noise-floor operation instead.
        rig.main.changeDisplay { $0.clarityEnabled = true }
        now += 1
        let payload: [String: LinkJSON] = ["op": .string("noise-floor"), "connectionId": .string("c"),
                                           "endpointId": .number(1), "revision": .number(1),
                                           "contextGeneration": .number(1), "floorDbm": .number(-110)]
        band.receive(.noiseFloor(try #require(MediaControlDecoder.noiseFloor(payload))))
        #expect(band.drawnSettings.scaleRange.lowerBound == -115)
        // A scale set by hand starts the tracking again from it.
        rig.main.changeDisplay { $0.scaleBottomDbm = -150 }
        #expect(band.drawnSettings.scaleRange.lowerBound == -150)
    }

    // MARK: Row 22: the waterfall's line period and Stop while transmitting

    @Test("the line period sets the Core's frames a line, and with Stop while transmitting the waterfall stands still")
    func linePeriodAndStopOnTx() async throws {
        let rig = try Rig()
        await quiet()
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        #expect(rig.sent.last?.framesPerLine == 1)
        rig.main.changeDisplay { $0.waterfallPeriodMs = 200 }
        #expect(await settle { rig.sent.last?.framesPerLine == 6 })
        let band = rig.main.band
        band.endpointId = 1
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        rig.main.changeDisplay { $0.waterfallStopOnTx = true }
        band.keyed = true
        feed(band, lines: 4)
        #expect(band.state.history.count == 0)
        band.keyed = false
        feed(band, lines: 4, from: 5)
        #expect(band.state.history.count == 4)
    }

    // MARK: Rows 16 and 19: detectors, averaging and the quantisation window

    @Test("the pan's detectors and averaging go to the Core, and the quantisation window follows the scale")
    func detectorsAndWindow() async throws {
        let rig = try Rig()
        await quiet()
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        let first = try #require(rig.sent.last)
        #expect(first.trace.detector == 0 && first.trace.averageMode == 3)
        #expect(first.waterfall.detector == 0 && first.waterfall.averageMode == 0)
        #expect(first.minDbm == -140 && first.maxDbm == -40)
        #expect(first.decimation == nil)
        rig.main.changeDisplay {
            $0.spectrumDetector = .rms
            $0.spectrumAveraging = .timeWindow
            $0.waterfallDetector = .average
            $0.waterfallAveraging = .recursive
            $0.decimation = 4
        }
        #expect(await settle { rig.sent.last?.trace.detector == 4 })
        let next = try #require(rig.sent.last)
        #expect(next.trace.averageMode == 2 && next.waterfall.detector == 2 && next.waterfall.averageMode == 1)
        // This Core does not take decimation: none goes.
        #expect(next.decimation == nil)
        rig.main.display.setTop(-20)
        #expect(await settle { rig.sent.last?.maxDbm == -20 })
    }

    // MARK: Row 28: the classic peak hold on the sheet

    @Test("Peak hold is the desktop's classic one, on any Core; the Core's is Active peak hold")
    func classicPeakHold() async throws {
        let rig = try Rig(extras: 0)
        await quiet()
        let display = rig.main.display
        #expect(DisplaySheetModel.Feature.peakHold.label == "Active peak hold")
        #expect(!display.classicPeakHold)
        display.toggleClassicPeakHold()
        #expect(rig.main.band.settings.peakHold)
        #expect(rig.main.band.state.peakHoldDelayMs == 2000)
    }

    // MARK: Rows 21, 30, 31: Setup's buttons

    @Test("Setup copies between the scale and the levels, adds stops to the phone's own palette, and resets")
    func setupButtons() async throws {
        let rig = try Rig()
        await quiet()
        let page = DisplayOnThisPhonePage(main: rig.main)
        let band = rig.main.band
        page.copyScaleToLevels()
        #expect(band.settings.waterfallHighDbm == -40 && band.settings.waterfallLowDbm == -140)
        page.change {
            $0.waterfallHighDbm = -60
            $0.waterfallLowDbm = -110
        }
        page.copyLevelsToScale()
        #expect(band.settings.scaleTopDbm == -60 && band.settings.scaleBottomDbm == -110)
        page.addCustomStop()
        #expect(band.settings.customPalette.count == PaletteStop.customDefault.count + 1)
        rig.main.display.selectPalette(BandPalette.customPaletteId)
        #expect(rig.main.display.paletteName == BandPalette.customPaletteName)
        page.change { $0.showFps = true }
        page.resetDisplay()
        #expect(!band.settings.showFps)
        #expect(band.settings.customPalette == PaletteStop.customDefault)
        #expect(band.settings.scaleTopDbm == -60)
        #expect(DisplayOnThisPhonePage.resetTitle == "Reset to defaults")
        // The new words are plain and promise nothing to come.
        for text in [DisplayOnThisPhonePage.resetQuestion, DisplayOnThisPhonePage.resetMessage,
                     DisplayOnThisPhonePage.decimationNote, DisplayOnThisPhonePage.fastAttackReason,
                     DisplayOnThisPhonePage.customPaletteFooter, DisplaySheetModel.ctunOnNote,
                     DisplaySheetModel.ctunOffNote] {
            let words = text.lowercased().split { !$0.isLetter }
            #expect(!words.contains("yet") && !words.contains("soon") && !words.contains("station"), "\(text)")
        }
    }
}

/// Synthetic frames for these tests (D4).
enum BandFixturesApp {
    static func frame(level: Float, sequence: UInt32, samples: Int = 16) -> DisplayFrame {
        let trace = [Float](repeating: level, count: samples)
        return DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: sequence,
                            producerTimestamp: UInt64(sequence) * 33_000_000, isKeyframe: sequence == 1,
                            waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: trace, waterfallDbm: trace,
                            wideDbm: [])
    }
}
