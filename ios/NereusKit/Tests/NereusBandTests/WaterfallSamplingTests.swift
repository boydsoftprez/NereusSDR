// NereusSDR for iOS: real Metal sampling of magnified waterfall bins and captures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-28: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Foundation
import Metal
import Testing
@testable import NereusBand

@MainActor
@Suite(.serialized) struct WaterfallSamplingTests {
    private let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)

    private func settings() -> BandDisplaySettings {
        var settings = BandRendererTests.plain()
        settings.waterfallPeriodMs = 10
        return settings
    }

    private func overlays(scale: CGFloat = 1) throws -> BandOverlays {
        // The middle palette entry is green. A red/blue display blend must
        // be purple, not a lookup at an interpolated (green) palette index.
        BandRendererTests.overlays(settings(), palette: try BandFixtures.palette(id: 7,
            [(0, "#FF0000"), (0.5, "#00FF00"), (1, "#0000FF")]), scale: scale)
    }

    @Test func magnifiedHorizontalBinsBlendColoursAtPixelCentres() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 1)
        history.append(line: [-100, 0], levels: levels)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        let middle = image.pixel(199, top)
        #expect((90...170).contains(middle.r) && (90...170).contains(middle.b))
        #expect(middle.g < 5)
        #expect(image.pixel(99, top).r > 250)
        #expect(image.pixel(299, top).b > 250)
    }

    @Test func nativeBinCentresAndOneBinPeakStayExact() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 1)
        var line = [Float](repeating: -100, count: 400)
        line[100] = 0
        history.append(line: line, levels: levels)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        #expect(image.pixel(100, top).b > 250)
        #expect(image.pixel(99, top).r > 250 && image.pixel(101, top).r > 250)
        #expect(image.pixel(99, top).b < 5 && image.pixel(101, top).b < 5)
    }

    @Test func minificationKeepsTheOriginalSelectedNarrowBinPeak() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 1)
        var line = [Float](repeating: -100, count: 800)
        // Pixel 100's centre selects source bin 201 in the original
        // floor(across * columns) mapping. Averaging bins 200/201 would
        // halve this retained peak, outside magnification-only scope.
        line[201] = 0
        history.append(line: line, levels: levels)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        #expect(image.pixel(100, top).b > 250)
        #expect(image.pixel(100, top).r < 5)
        #expect(image.pixel(99, top).r > 250 && image.pixel(101, top).r > 250)
        // Although 200 bins across a 400-pixel view looks magnified by
        // counts alone, this row covers only a quarter of that view:
        // each source bin occupies half a pixel and must stay unaveraged.
        var covered = WaterfallHistory(capacity: 1)
        var narrow = [Float](repeating: -100, count: 200)
        narrow[101] = 0
        covered.append(line: narrow, levels: levels,
                       coverage: BandCoverage(centerHz: 7_185_000, spanHz: 10_000))
        let projected = try target.render(renderer, frame: nil, history: covered, extras: nil, overlays: overlays())
        #expect(projected.pixel(50, top).b > 250)
        #expect(projected.pixel(50, top).r < 5)
    }

    @Test func continuousCaptureColoursBlendVerticallyAtEveryDisplayScale() throws {
        for scale in [1.0, 3.0] {
            let target = try Offscreen(width: 400, height: 600)
            let renderer = try BandRenderer(device: target.device)
            var history = WaterfallHistory(capacity: 2)
            history.append(line: [-100], levels: levels, presentationTime: 1000, presentationPeriod: 0.1)
            history.append(line: [0], levels: levels, presentationTime: 1000.1, presentationPeriod: 0.1,
                           presentationContinuous: true)
            let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays(scale: scale))
            let top = Int(BandRendererTests.layout(settings(), scale: scale).waterfall.minY)
            let middle = image.pixel(200, top + Int(5 * scale))
            #expect((90...170).contains(middle.r) && (90...170).contains(middle.b))
            #expect(middle.g < 5)
            #expect(history.count == 2 && history.linesAppended == 2)
            #expect(WaterfallTimeAxis(topTime: 1000.1, periodMs: 10).age(at: 1000.045, history: history) == 1)
        }
    }

    @Test func realGapsAndHistoryEdgesStayBlank() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 2)
        history.append(line: [-100], levels: levels, presentationTime: 1000, presentationPeriod: 1 / 30)
        history.append(line: [0], levels: levels, presentationTime: 1000.15, presentationPeriod: 1 / 30)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        #expect(image.grey(200, top + 7).isNear(BandRendererTests.backgroundGrey, tolerance: 2))
        #expect(image.grey(200, top + 20).isNear(BandRendererTests.backgroundGrey, tolerance: 2))
    }

    @Test func invalidNeighbourCoverageDoesNotFadeValidDataOrFillTheEmptyEdge() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 2)
        history.append(line: [-100], levels: levels,
                       coverage: BandCoverage(centerHz: 7_190_000, spanHz: 20_000),
                       presentationTime: 1000, presentationPeriod: 0.1)
        history.append(line: [0], levels: levels,
                       coverage: BandCoverage(centerHz: 7_200_000, spanHz: 40_000),
                       presentationTime: 1000.1, presentationPeriod: 0.1, presentationContinuous: true)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        #expect(image.pixel(300, top + 2).b > 250)
        #expect(image.grey(300, top + 7).isNear(BandRendererTests.backgroundGrey, tolerance: 2))
        let overlap = image.pixel(100, top + 5)
        #expect((90...170).contains(overlap.r) && (90...170).contains(overlap.b))
    }

    @Test func eachTemporalNeighbourProjectsItsOwnBinFrequencies() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var history = WaterfallHistory(capacity: 2)
        history.append(line: [-100, 0], levels: levels,
                       coverage: BandCoverage(centerHz: 7_190_000, spanHz: 40_000),
                       presentationTime: 1000, presentationPeriod: 0.1)
        history.append(line: [-100, 0], levels: levels,
                       coverage: BandCoverage(centerHz: 7_200_000, spanHz: 40_000),
                       presentationTime: 1000.1, presentationPeriod: 0.1, presentationContinuous: true)
        let image = try target.render(renderer, frame: nil, history: history, extras: nil, overlays: overlays())
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        // At 7.200 MHz the older row is at its blue bin centre while the
        // newer one is between red and blue. A shared bin coordinate would
        // incorrectly give equal red and blue for both rows.
        let colour = image.pixel(199, top + 4)
        #expect((50...90).contains(colour.r) && (165...210).contains(colour.b))
        #expect(colour.g < 5)
    }

    @Test func inFlightSamplingSurvivesRingWrapAndRewind() throws {
        let first = try Offscreen(width: 400, height: 600)
        let second = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: first.device)
        var history = WaterfallHistory(capacity: 2)
        history.append(line: [-100], levels: levels, presentationTime: 1000, presentationPeriod: 0.1)
        history.append(line: [0], levels: levels, presentationTime: 1000.1, presentationPeriod: 0.1,
                       presentationContinuous: true)
        let gate = try #require(first.device.makeSharedEvent())
        renderer.commandBufferMade = { $0.encodeWaitForEvent(gate, value: 1) }
        let held = try #require(renderer.draw(frame: nil, history: history, extras: nil,
                                              overlays: overlays(), into: first.texture))
        renderer.commandBufferMade = nil
        history.append(line: [-100], levels: levels, presentationTime: 1000.2, presentationPeriod: 0.1,
                       presentationContinuous: true)
        var rewind = try overlays()
        rewind.lookBackLines = 1
        let next = try #require(renderer.draw(frame: nil, history: history, extras: nil,
                                              overlays: rewind, into: second.texture))
        gate.signaledValue = 1
        held.waitUntilCompleted()
        next.waitUntilCompleted()
        let top = Int(BandRendererTests.layout(settings()).waterfall.minY)
        let before = try first.read().pixel(200, top + 2)
        #expect((40...90).contains(before.r) && (165...215).contains(before.b))
        #expect(try second.read().pixel(200, top + 2).b > 250)
    }
}
