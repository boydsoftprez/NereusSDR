// NereusSDR for iOS: tests that the 3D stack's rows pack and upload as the per-column lists did
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-26 J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Metal
import NereusMedia
import Testing
@testable import NereusBand

/// The 3D stack's rows go to the GPU as each column's level and whether it
/// was captured, side by side. They are packed in one loop into one buffer
/// (a list of two for every column, 2195 a row, made 97 times on every
/// whole upload before, 2026-10-26), and a whole upload is one copy, not
/// one a row. Both give the bytes the lists gave.
@Suite(.enabled(if: MTLCreateSystemDefaultDevice() != nil, "no Metal device: the textures cannot be made"))
struct StackedUploadTests {
    static let referenceHz = 7_150_000.0

    /// A stack of `rows` rows, some levels missing, the spectrum beside the
    /// pan on every other row.
    static func stack(rows: Int) -> StackedTraceHistory {
        var stack = StackedTraceHistory()
        var state: UInt64 = 0x9E37_79B9_7F4A_7C15
        for index in 0..<rows {
            let line = (0..<500).map { sample -> Float in
                state = state &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
                return sample % 37 == 0 ? .nan : -130 + Float(state >> 58)
            }
            let pan = BandCoverage(centerHz: 7_200_000 + Double(index) * 10, spanHz: 48_000)
            let wide = index % 2 == 0 ? BandCoverage(centerHz: pan.centerHz, spanHz: pan.spanHz / 0.35) : nil
            let frame = DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: UInt32(index + 1),
                                     producerTimestamp: UInt64(index + 1), isKeyframe: index == 0,
                                     waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: line,
                                     waterfallDbm: line, wideDbm: wide == nil ? [] : [Float](repeating: -125, count: 300))
            stack.append(frame: frame, coverage: pan, wide: wide, time: Double(index))
        }
        return stack
    }

    /// The pack as it was: a list of two for every column.
    static func listed(_ stack: StackedTraceHistory, row: Int) -> [Float] {
        let levels = Array(stack.storageLine(row))
        let captured = Array(stack.capturedLine(row))
        return (0..<stack.columns).flatMap { [levels[$0], captured[$0]] }
    }

    @Test func eachRowPacksAsTheListsDid() {
        let stack = Self.stack(rows: 30)
        let whole = stack.packedStorage()
        #expect(whole.count == stack.columns * 2 * stack.capacity)
        for row in 0..<stack.capacity {
            let packed = stack.packedStorageLine(row)
            #expect(packed.map(\.bitPattern) == Self.listed(stack, row: row).map(\.bitPattern), "row \(row)")
            let slice = whole[(row * stack.columns * 2)..<((row + 1) * stack.columns * 2)]
            #expect(slice.map(\.bitPattern) == packed.map(\.bitPattern), "row \(row) of the whole")
        }
        // Captured is 1 where the line or the wide row reached, 0 past them.
        let newest = stack.packedStorageLine(stack.storageRow(age: 0))
        #expect(newest.enumerated().filter { $0.offset % 2 == 1 }.allSatisfy { $0.element == 0 || $0.element == 1 })
    }

    @Test(arguments: [30, 200])
    func theWholeUploadHoldsEveryHeldRow(rows: Int) throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let stack = Self.stack(rows: rows)
        let rowsDescriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rg32Float, width: stack.columns,
                                                                      height: stack.capacity, mipmapped: false)
        let windowsDescriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba32Float, width: 1,
                                                                         height: stack.capacity, mipmapped: false)
        let texture = try #require(device.makeTexture(descriptor: rowsDescriptor))
        let windows = try #require(device.makeTexture(descriptor: windowsDescriptor))
        BandRenderer.uploadWhole(stack, referenceHz: Self.referenceHz, to: texture, windows: windows)
        var levels = [Float](repeating: -1, count: stack.columns * 2 * stack.capacity)
        texture.getBytes(&levels, bytesPerRow: stack.columns * 2 * MemoryLayout<Float>.stride,
                         from: MTLRegionMake2D(0, 0, stack.columns, stack.capacity), mipmapLevel: 0)
        var spans = [SIMD4<Float>](repeating: SIMD4(-1, -1, -1, -1), count: stack.capacity)
        windows.getBytes(&spans, bytesPerRow: MemoryLayout<SIMD4<Float>>.stride,
                         from: MTLRegionMake2D(0, 0, 1, stack.capacity), mipmapLevel: 0)
        for age in 0..<stack.count {
            let row = stack.storageRow(age: age)
            let held = levels[(row * stack.columns * 2)..<((row + 1) * stack.columns * 2)]
            #expect(held.map(\.bitPattern) == Self.listed(stack, row: row).map(\.bitPattern), "age \(age)")
            let window = try #require(stack.window(age: age))
            #expect(spans[row].x == Float(window.lowHz - Self.referenceHz) && spans[row].y == Float(window.spanHz))
            #expect(spans[row].w == (stack.quiet(age: age) == nil ? 0 : 1))
        }
    }
}
