// NereusSDR for iOS: what the 3D stack's rows cost to keep and draw: each row's work, the GPU's frame, the memory
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-26 J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import CoreGraphics
import Foundation
import Metal
import NereusMedia
import Testing
@testable import NereusBand

/// The cost of the 3D stack at its column density, measured on the machine
/// the tests run on: the CPU time of each row as it goes in (the columns'
/// peaks, the smoothing, the store), the CPU time to encode a 3D frame, the
/// GPU's own time for it, and the memory of the rows. It prints its figures
/// and checks nothing against a budget: a Mac's figures do not stand for a
/// phone's. It runs only with `NEREUS_STACK_COST=1`.
@MainActor
@Suite(.serialized, .enabled(if: ProcessInfo.processInfo.environment["NEREUS_STACK_COST"] == "1"))
struct StackedDensityCostTests {
    /// An iPhone 18 Pro Max's band in portrait: 440 points across at 3x,
    /// the line the band asks for one sample a pixel.
    static let width = 1320
    static let height = 1400
    static let pan = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)
    static let wide = BandCoverage(centerHz: 7_200_000, spanHz: 48_000 / 0.35)

    static func cpuNanoseconds() -> UInt64 {
        clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID)
    }

    static func median(_ values: [Double]) -> Double {
        values.sorted()[values.count / 2]
    }

    /// Noise about -125 dBm with three carriers, a new draw each line.
    static func line(_ count: Int, generator: inout SystemRandomNumberGenerator) -> [Float] {
        var line = (0..<count).map { _ in -125 + 10 * log10(Float.random(in: 0.0001...1, using: &generator)) + 4 }
        for share in [0.31, 0.5, 0.72] {
            line[Int(Double(count) * share)] = -50
        }
        return line
    }

    static func frame(_ line: [Float], wide: [Float], sequence: UInt32) -> DisplayFrame {
        DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: sequence,
                     producerTimestamp: UInt64(sequence) * 33_000_000, isKeyframe: sequence == 1,
                     waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: line, waterfallDbm: line,
                     wideDbm: wide)
    }

    @Test(arguments: [1320, 660])
    func eachRowsWork(lineBins: Int) {
        var generator = SystemRandomNumberGenerator()
        let wideBins = Int((Double(lineBins) / 0.35).rounded(.up))
        var stack = StackedTraceHistory()
        var times: [Double] = []
        for sequence in 1...400 {
            let frame = Self.frame(Self.line(lineBins, generator: &generator),
                                   wide: Self.line(wideBins, generator: &generator), sequence: UInt32(sequence))
            let start = Self.cpuNanoseconds()
            stack.append(frame: frame, coverage: Self.pan, wide: Self.wide, time: Double(sequence))
            let elapsed = Double(Self.cpuNanoseconds() - start) / 1e3
            if sequence > 100 {
                times.append(elapsed)
            }
        }
        var packs: [Double] = []
        for row in 0..<stack.capacity {
            let start = Self.cpuNanoseconds()
            let packed = stack.packedStorageLine(row)
            packs.append(Double(Self.cpuNanoseconds() - start) / 1e3)
            #expect(packed.count == stack.columns * 2)
        }
        let rowBytes = stack.columns * 8
        print("STACK-COST row line=\(lineBins) columns=\(stack.columns) "
              + String(format: "append median %.1f us, p90 %.1f us; pack for upload median %.1f us; ",
                       Self.median(times), times.sorted()[times.count * 9 / 10], Self.median(packs))
              + "texture \(rowBytes * stack.capacity) B, CPU rows \(stack.columns * stack.capacity * 8) B, "
              + "upload \(rowBytes) B a row")
        #expect(stack.count == stack.capacity)
    }

    /// Draws the band in 3D from two stacks in turn, frame by frame, so
    /// both see the same GPU clocks: one at about the old density (769 columns
    /// across the widest row, from lines of 269 samples) and one from the
    /// phone's line of one sample a pixel at today's density. The same band
    /// in 2D is drawn between them for reference.
    @Test(arguments: [50, 0])
    func eachFramesWork(angle: Int) throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderers = try (BandRenderer(device: target.device), BandRenderer(device: target.device),
                             BandRenderer(device: target.device))
        var settings = BandDisplaySettings.desktopDefaults
        settings.spectrumView = .stacked
        settings.threeDAngle = angle
        settings.threeDSpan = 100
        let layout = BandLayout(size: CGSize(width: Self.width, height: Self.height), scale: 3, settings: settings)
        var state = BandState(waterfallLines: layout.waterfallLines)
        state.note(context: 1, coverage: Self.pan, wide: Self.wide)
        var generator = SystemRandomNumberGenerator()
        func stack(lineBins: Int, columns: Int = StackedTrace.maxColumns) -> StackedTraceHistory {
            var stack = StackedTraceHistory(smoothsRows: true, columns: columns)
            let wideBins = Int((Double(lineBins) / 0.35).rounded(.up))
            for sequence in 1...StackedTrace.keptRows {
                let frame = Self.frame(Self.line(lineBins, generator: &generator),
                                       wide: Self.line(wideBins, generator: &generator), sequence: UInt32(sequence))
                stack.append(frame: frame, coverage: Self.pan, wide: Self.wide, time: Double(sequence))
            }
            return stack
        }
        let old = stack(lineBins: 269, columns: 769)
        let new = stack(lineBins: Self.width)
        let frame = Self.frame(Self.line(Self.width, generator: &generator), wide: [], sequence: 1)
        state.receive(frame: frame, manualLevels: .init(lowDbm: -130, highDbm: -60))
        var overlays = BandOverlays(centerHz: Self.pan.centerHz, spanHz: Self.pan.spanHz, scale: 3,
                                    settings: settings, bandPlan: nil, palette: nil)
        overlays.frameCoverage = Self.pan
        func gpuMilliseconds(_ buffer: (any MTLCommandBuffer)?) -> Double {
            guard let buffer, buffer.gpuEndTime > buffer.gpuStartTime else {
                return 0
            }
            return (buffer.gpuEndTime - buffer.gpuStartTime) * 1e3
        }
        var gpu: ([Double], [Double], [Double]) = ([], [], [])
        var encode: ([Double], [Double]) = ([], [])
        for index in 0..<400 {
            var stacked = overlays
            stacked.stacked = StackedTraceOverlay(noiseFloorDbm: -125, glide: Double(index % 8) / 8,
                                                  availableSpanFactor: 1 / 0.35)
            var start = Self.cpuNanoseconds()
            let before = renderers.0.draw(frame: state.frame, history: state.history, stack: old, extras: nil,
                                          overlays: stacked, into: target.texture)
            let encodeBefore = Double(Self.cpuNanoseconds() - start) / 1e6
            before?.waitUntilCompleted()
            start = Self.cpuNanoseconds()
            let after = renderers.1.draw(frame: state.frame, history: state.history, stack: new, extras: nil,
                                         overlays: stacked, into: target.texture)
            let encodeAfter = Double(Self.cpuNanoseconds() - start) / 1e6
            after?.waitUntilCompleted()
            let flat = renderers.2.draw(frame: state.frame, history: state.history, stack: nil, extras: nil,
                                        overlays: overlays, into: target.texture)
            flat?.waitUntilCompleted()
            if index >= 50 {
                gpu.0.append(gpuMilliseconds(before))
                gpu.1.append(gpuMilliseconds(after))
                gpu.2.append(gpuMilliseconds(flat))
                encode.0.append(encodeBefore)
                encode.1.append(encodeAfter)
            }
        }
        let rowSpan = StackedTrace.rowSpan(spanPercent: 100, shape: StackedTraceShape.forAngle(angle),
                                           availableFactor: 1 / 0.35)
        func vertices(_ stack: StackedTraceHistory) -> String {
            let mesh = StackedTrace.meshColumns(columns: stack.columns, rowSpan: rowSpan)
            return "mesh \(mesh), fill \(mesh * 2 * (StackedTrace.visibleRows + 2)), lines \(mesh * 4 * StackedTrace.visibleRows)"
        }
        func figures(_ times: [Double]) -> String {
            String(format: "median %.2f ms, p90 %.2f ms", Self.median(times), times.sorted()[times.count * 9 / 10])
        }
        print("STACK-COST frame angle=\(angle) rowSpan=" + String(format: "%.3f", rowSpan)
              + " plot=\(Int(layout.spectrum.width))x\(Int(layout.spectrum.height)); "
              + "old \(old.columns) columns (\(vertices(old))): GPU \(figures(gpu.0)), encode \(figures(encode.0)); "
              + "new \(new.columns) columns (\(vertices(new))): GPU \(figures(gpu.1)), encode \(figures(encode.1)); "
              + "2D GPU \(figures(gpu.2)); new/old GPU median "
              + String(format: "%.2f", Self.median(gpu.1) / max(Self.median(gpu.0), 1e-9)))
        #expect(!gpu.0.isEmpty && old.columns == 769)
    }
}
