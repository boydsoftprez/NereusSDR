// NereusSDR for iOS: tests for the rest of the desktop's display on the band: grid, labels, overlays, peak hold, rewind
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusLink
import NereusMedia
import Testing
@testable import NereusBand

/// R-IOS-11 (Task 54e part 2): the parity audit's rows 13, 14, 16, 19, 21,
/// 22, 25 to 28, 31 and 33 on the band. Every frame and colour is
/// synthetic (D4).
@MainActor
@Suite(.serialized) struct DisplayParityMoreTests {
    static let width = BandRendererTests.width
    static let height = BandRendererTests.height

    static func render(_ settings: BandDisplaySettings, frame: DisplayFrame? = nil, extras: DisplayExtras? = nil,
                       history: WaterfallHistory = WaterfallHistory(capacity: 1),
                       adjust: (inout BandOverlays) -> Void = { _ in }) throws -> Image {
        let target = try Offscreen(width: width, height: height)
        let renderer = try BandRenderer(device: target.device)
        var overlays = BandRendererTests.overlays(settings)
        adjust(&overlays)
        return try target.render(renderer, frame: frame, history: history, extras: extras, overlays: overlays)
    }

    // MARK: Row 13: the grid's look

    @Test func theGridIsTheDesktopsWhiteAt40With20ForItsFineDottedLines() throws {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.gridColour == "#FFFFFF28" && settings.hGridColour == "#FFFFFF28")
        #expect(settings.gridFineColour == "#FFFFFF14")
        var plain = BandRendererTests.plain()
        plain.grid = true
        plain.hGridColour = "#FF000080"
        let geometry = BandRendererTests.geometry(plain)
        let image = try Self.render(plain)
        // The level lines in their own colour.
        let y = Int(geometry.y(forDbm: -90).rounded(.down))
        #expect(image.pixel(100, y).r > image.pixel(100, y).g + 40)
        // Between two frequency lines, dotted fine lines at a fifth of the step.
        let ticks = geometry.frequencyTicks(minimumSpacing: Double(BandRenderer.frequencyLabelSpacingPoints))
        let fineHz = try #require(ticks.ticks.first) + ticks.stepHz / 5
        let x = Int(geometry.x(forHz: fineHz).rounded(.down))
        let column = (0..<Int(geometry.size.height)).map { image.grey(x, $0) }
        let lit = column.filter { $0 > 16 }.count
        #expect(lit > column.count / 4 && lit < column.count * 3 / 4, "fine grid lit \(lit) of \(column.count)")
    }

    // MARK: Row 26: frequency label alignment; row 33: the dBm scale

    @Test func frequencyLabelsSitAsAlignedAndOffDrawsNone() throws {
        var settings = BandRendererTests.plain()
        let layout = BandRendererTests.layout(settings)
        let geometry = BandRendererTests.geometry(settings)
        let ticks = geometry.frequencyTicks(minimumSpacing: Double(BandRenderer.frequencyLabelSpacingPoints))
        let tick = Int(geometry.x(forHz: ticks.ticks[ticks.ticks.count / 2]))
        let row = Int(layout.frequencyScale.midY)
        func yellow(_ image: Image, _ range: Range<Int>) -> Bool {
            range.contains { x in (row - 3...row + 3).contains { image.pixel(x, $0).r > 150 && image.pixel(x, $0).b < 110 } }
        }
        settings.frequencyLabelAlignment = .left
        var image = try Self.render(settings)
        #expect(yellow(image, (tick - 7)..<(tick - 3)) && !yellow(image, (tick + 3)..<(tick + 7)))
        settings.frequencyLabelAlignment = .right
        image = try Self.render(settings)
        #expect(!yellow(image, (tick - 7)..<(tick - 3)) && yellow(image, (tick + 3)..<(tick + 7)))
        settings.frequencyLabelAlignment = .off
        image = try Self.render(settings)
        #expect(!yellow(image, 30..<Self.width))
    }

    @Test func withoutTheDbmScaleTheSpectrumRunsToTheEdge() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.showDbmScale = false
        let layout = BandLayout(size: CGSize(width: 400, height: 600), scale: 1, settings: settings)
        #expect(layout.dbmScale.width == 0 && layout.dbmArrows.width == 0)
        #expect(layout.strip.maxX == 400)
    }

    // MARK: Row 25: zero lines and waterfall overlays

    @Test func theZeroLineIsDashedAt0DbmInTheReceiveZeroLineColour() throws {
        var settings = BandRendererTests.plain()
        settings.scaleTopDbm = 10
        settings.scaleBottomDbm = -90
        settings.rxZeroLineColour = "#FF0000FF"
        let y = Int(BandRendererTests.geometry(settings).y(forDbm: 0).rounded(.down))
        #expect(try Self.render(settings).pixel(3, y).r < 40)
        settings.showZeroLine = true
        let image = try Self.render(settings)
        let row = (0..<200).map { image.pixel($0, y).r > 200 }
        let lit = row.filter { $0 }.count
        #expect(lit > 60 && lit < 140, "dashed: \(lit) of 200 lit")
    }

    @Test func theWaterfallTakesItsOpacityAndTheActiveSlicesFilterAndZeroLine() throws {
        var settings = BandRendererTests.plain()
        let greys = try BandFixtures.palette(id: 3, [(0, "#000000"), (1, "#FFFFFF")])
        var history = WaterfallHistory(capacity: 400)
        for _ in 0..<400 {
            history.append(line: BandRendererTests.flat(100, 0), levels: .init(lowDbm: -100, highDbm: 0))
        }
        let layout = BandRendererTests.layout(settings)
        let row = Int(layout.waterfall.midY)
        let marker = SliceMarkers.Marker(sliceId: 0, centerHz: BandRendererTests.centre + 5_000,
                                         passbandHz: (BandRendererTests.centre + 5_000)...(BandRendererTests.centre + 8_000),
                                         style: SliceMarkers.style(for: "#00FF00", selected: true))
        func image() throws -> Image {
            try Self.render(settings, history: history) {
                $0.palette = greys
                $0.markers = [marker]
            }
        }
        #expect(try image().grey(20, row).isNear(255, tolerance: 3))
        settings.waterfallOpacityPercent = 50
        let dimmed = try image().pixel(20, row)
        #expect(dimmed.r.isNear(Int((255 + 10) / 2), tolerance: 4), "\(dimmed)")
        settings.waterfallOpacityPercent = 100
        let geometry = BandRendererTests.geometry(settings)
        settings.showRxZeroLineOnWaterfall = true
        settings.rxZeroLineColour = "#FF0000FF"
        let zeroX = Int(geometry.x(forHz: marker.centerHz))
        // Over the slice's own centre line, the zero line makes the column red.
        let drawn = try image()
        #expect((zeroX - 1...zeroX).contains { drawn.pixel($0, row).r > 200 && drawn.pixel($0, row).g < 140 },
                "\(drawn.pixel(zeroX, row))")
        #expect(!BandDisplaySettings.desktopDefaults.showRxFilterOnWaterfall)
    }

    // MARK: Row 28: the classic peak hold

    @Test func peakHoldKeepsEachSamplesHighestAndStartsOverAfterItsDelay() {
        var state = BandState(waterfallLines: 4)
        state.peakHoldDelayMs = 1000
        func frame(_ value: Float, at sample: Int, sequence: UInt32) -> DisplayFrame {
            var trace = [Float](repeating: -120, count: 10)
            trace[sample] = value
            return BandFixtures.frame(trace: trace, sequence: sequence)
        }
        let manual = BandDisplaySettings.desktopDefaults.manualLevels
        state.receive(frame: frame(-60, at: 2, sequence: 1), manualLevels: manual)
        state.receive(frame: frame(-70, at: 5, sequence: 2), manualLevels: manual)
        #expect(state.peakHold?[2] == -60 && state.peakHold?[5] == -70)
        // 33 ms a frame (the fixture's clock): frame 31 is a second on, and starts over.
        state.receive(frame: frame(-80, at: 7, sequence: 32), manualLevels: manual)
        #expect(state.peakHold?[2] == -120 && state.peakHold?[7] == -80)
        state.peakHoldDelayMs = nil
        #expect(state.peakHold == nil)
    }

    @Test func peakHoldIsDrawnDottedInTheTracesColour() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.traceColour = "#FF0000"
        let hold = BandRendererTests.flat(200, -80)
        let y = Int(BandRendererTests.geometry(settings).y(forDbm: -80))
        let image = try Self.render(settings) { $0.peakHold = hold }
        let row = (0..<200).map { x in (y - 1...y).contains { image.pixel(x, $0).r > 100 } }
        let lit = row.filter { $0 }.count
        #expect(lit > 50 && lit < 160, "dotted: \(lit) of 200")
    }

    // MARK: Row 31: this phone's own palette

    @Test func theCustomPaletteColoursTheWaterfall() throws {
        var settings = BandRendererTests.plain()
        settings.waterfallPaletteId = BandPalette.customPaletteId
        settings.customPalette = [PaletteStop(at: 0, colour: "#000000"), PaletteStop(at: 1, colour: "#00FF00")]
        var history = WaterfallHistory(capacity: 4)
        history.append(line: BandRendererTests.flat(100, 0), levels: .init(lowDbm: -100, highDbm: 0))
        let reds = try BandFixtures.palette(id: 1, [(0, "#000000"), (1, "#FF0000")])
        let top = Int(BandRendererTests.layout(settings).waterfall.minY)
        let pixel = try Self.render(settings, history: history) { $0.palette = reds }.pixel(200, top)
        #expect(pixel.g > 240 && pixel.r < 10, "\(pixel)")
        let expectedRGBA = SIMD4<Float>(17.0 / 255, 34.0 / 255, 51.0 / 255, 68.0 / 255)
        #expect(BandPalette.rgbaWithAlpha("#11223344") == expectedRGBA)
        #expect(BandPalette.hex(SIMD4(1, 0, 0.5, 1)) == "#FF0080FF")
    }

    // MARK: Row 14: rewind and time

    @Test func eachLineKeepsItsTimeAndTheWaterfallCanBeLookedBack() throws {
        var history = WaterfallHistory(capacity: 1000)
        for index in 0..<1000 {
            history.append(line: BandRendererTests.flat(100, index < 500 ? -100 : 0),
                           levels: .init(lowDbm: -100, highDbm: 0), time: 1000 + Double(index))
        }
        #expect(history.time(age: 0) == 1999 && history.time(age: 999) == 1000)
        var settings = BandRendererTests.plain()
        settings.waterfallPeriodMs = 100
        settings.rewindSeconds = 60
        #expect(settings.rewindLines == 600)
        settings.rewindSeconds = 1200
        settings.waterfallPeriodMs = 10
        #expect(settings.rewindLines == BandDisplaySettings.rewindLinesLimit)
        // With a screen of lines they fit the smallest texture height an iPhone allows.
        #expect(BandDisplaySettings.rewindLinesLimit + 1_192 <= BandDisplaySettings.waterfallRowsLimit)
        let greys = try BandFixtures.palette(id: 3, [(0, "#000000"), (1, "#FFFFFF")])
        let top = Int(BandRendererTests.layout(settings).waterfall.minY)
        #expect(try Self.render(settings, history: history) { $0.palette = greys }.grey(20, top).isNear(255))
        // 500 lines back, the older, darker lines are at the top.
        #expect(try Self.render(settings, history: history) {
            $0.palette = greys
            $0.lookBackLines = 500
        }.grey(20, top).isNear(0, tolerance: 3))
    }

    @Test func aHeldWaterfallAddsNoLine() {
        var state = BandState(waterfallLines: 4)
        state.waterfallHeld = true
        let manual = BandDisplaySettings.desktopDefaults.manualLevels
        state.receive(frame: BandFixtures.frame(trace: BandRendererTests.flat(8)), manualLevels: manual)
        #expect(state.history.count == 0)
        state.waterfallHeld = false
        state.receive(frame: BandFixtures.frame(trace: BandRendererTests.flat(8), sequence: 2), manualLevels: manual)
        #expect(state.history.count == 1)
    }

    // MARK: Row 21: use the scale's top and bottom; row 27: the grid follows the floor

    @Test func useSpectrumMinMaxColoursAgainstTheScaleAndAsksTheCoreForNoLevels() throws {
        var settings = BandDisplaySettings.desktopDefaults
        settings.useSpectrumMinMax = true
        #expect(settings.phoneLevels == .init(lowDbm: -140, highDbm: -40))
        let request = try #require(settings.extrasRequest(gates: BandDisplaySettingsTests.open))
        #expect(request.waterfallLevels?.mode == .manual)
        #expect(request.waterfallLevels?.lowDbm == -140 && request.waterfallLevels?.highDbm == -40)
        var state = BandState(waterfallLines: 4, expectsExtras: true)
        state.phoneSetsLevels = true
        let frame = BandFixtures.frame(trace: [Float](repeating: -90, count: 4))
        state.receive(frame: frame, manualLevels: settings.phoneLevels)
        state.receive(extras: BandFixtures.extras(for: frame, levels: (-200, -150)), manualLevels: settings.phoneLevels)
        // The Core's levels would make -90 full scale; the scale's put it mid-way.
        let position = Int(state.history.line(age: 0)?.first ?? 0)
        #expect((126...129).contains(position), "\(position)")
    }

    @Test func theGridsBottomFollowsTheFloorWithItsOffsetAndKeepsTheRangeWhenAsked() throws {
        var settings = BandDisplaySettings.desktopDefaults
        var tracker = GridFloorTracker()
        var moved = false
        var held = false
        tracker.noteDisplayFloor(-120, fastAttack: false)
        // Off: the pan's own scale.
        held = tracker.run(settings, transmitting: false, at: 0)
        #expect(!held)
        #expect(tracker.applied(to: settings).scaleRange == -140 ... -40)
        settings.gridFollowsNoiseFloor = true
        settings.gridNoiseFloorOffsetDb = -5
        settings.clarityEnabled = false
        tracker.noteDisplayFloor(-120, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 1)
        #expect(moved)
        #expect(tracker.applied(to: settings).scaleRange == -125 ... -40)
        // No new reading: nothing moves.
        held = tracker.run(settings, transmitting: false, at: 2)
        #expect(!held)
        settings.gridKeepsRange = true
        tracker.reset()
        tracker.noteDisplayFloor(-120, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 3)
        #expect(moved)
        #expect(tracker.applied(to: settings).scaleRange == -125 ... -25)
        // The Core is asked for the floor for the grid alone, with the NF shift (0 here).
        let request = try #require(settings.extrasRequest(gates: BandDisplaySettingsTests.open))
        #expect(request.noiseFloor == .init(enabled: true, shiftDb: 0))
    }

    // MARK: Row 19: the quantisation window

    @Test func theQuantisationWindowHoldsTheScaleAndTheLevelsInForce() {
        var settings = BandDisplaySettings.desktopDefaults
        // Manual levels inside the scale: the scale's own range.
        settings.waterfallLevelMode = .manual
        #expect(settings.quantisationWindow(coreLevels: nil, held: nil) == -140 ... -40)
        // Levels outside it widen it.
        settings.waterfallLowDbm = -160
        #expect(settings.quantisationWindow(coreLevels: nil, held: nil) == -160 ... -40)
        // Clarity: the Core's levels with 10 dB either side, held while they fit.
        settings.waterfallLevelMode = .clarity
        let window = settings.quantisationWindow(coreLevels: .init(lowDbm: -150, highDbm: -30), held: nil)
        #expect(window == -160 ... -20)
        #expect(settings.quantisationWindow(coreLevels: .init(lowDbm: -148, highDbm: -32), held: window) == window)
        // A drag of the scale asks again only at whole ten dB.
        settings.waterfallLevelMode = .manual
        settings.waterfallLowDbm = -122
        settings.scaleBottomDbm = -133
        #expect(settings.quantisationWindow(coreLevels: nil, held: nil) == -140 ... -40)
    }

    // MARK: Row 16: detectors, averaging and decimation on the link

    @Test func decimationGoesOnlyToACoreThatTakesIt() throws {
        let takes = MediaFeatureGates(agreedMinor: 11) { _ in 2 }
        let older = MediaFeatureGates(agreedMinor: 11) { $0 == "spectrumGrantVersion" ? 1 : 2 }
        #expect(takes.decimation && !older.decimation)
        var subscription = BandDisplaySettingsTests.subscription()
        subscription.decimation = 4
        try DisplayEndpointRequest.validate(subscription, gates: takes)
        #expect(DisplayEndpointRequest.subscribe(subscription, connectionId: "c", gates: takes)["decimation"] == .number(4))
        #expect(throws: DisplayEndpointRequest.Invalid.decimation) {
            try DisplayEndpointRequest.validate(subscription, gates: older)
        }
        subscription.decimation = 33
        #expect(throws: DisplayEndpointRequest.Invalid.decimation) {
            try DisplayEndpointRequest.validate(subscription, gates: takes)
        }
        #expect(SpectrumDetector.allCases.map(\.rawValue) == Array(DisplayEndpointRequest.detectorRange))
        #expect(SpectrumAveraging.allCases.map(\.rawValue) == [0, 1, 2, 3])
    }

    // MARK: Row 30: reset

    @Test func resetKeepsEachBandsScaleThePlansSizeAndTheExtendedView() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.enterBand("5")
        settings.scaleTopDbm = -30
        settings.bandPlanSize = .huge
        settings.extendedView = true
        settings.traceColour = "#FF0000"
        settings.showFps = true
        let reset = settings.resetKeepingPlace()
        #expect(reset.traceColour == BandDisplaySettings.desktopDefaults.traceColour && !reset.showFps)
        #expect(reset.scaleTopDbm == -30 && reset.bandScales["5"]?.topDbm == -30 && reset.scaleBand == "5")
        #expect(reset.bandPlanSize == .huge && reset.extendedView)
    }
}
