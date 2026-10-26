// NereusSDR for iOS: tests that the waterfall's whole upload fills the textures as the row-by-row loop did
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Metal
import Testing
@testable import NereusBand

/// R-IOS-11: after a turn the renderer uploads the whole waterfall in one
/// copy, and the waterfall and coverage textures hold every byte the copy a
/// row at a time put there.
@Suite(.enabled(if: MTLCreateSystemDefaultDevice() != nil, "no Metal device: the textures cannot be made"))
struct WaterfallUploadTests {
    static let capacity = 300
    static let referenceHz = 14_000_000.0

    /// The upload as it was written before it went in one copy, kept as the
    /// reference: each held line copied out of storage and replaced into its
    /// texture row, and its coverage into its coverage row.
    static func uploadRowByRow(_ history: WaterfallHistory, referenceHz: Double, to texture: any MTLTexture,
                               coverage coverageTexture: (any MTLTexture)?) {
        for age in 0..<history.count {
            let row = history.storageRow(age: age)
            var span = SIMD2<Float>(0, 0)
            if let coverage = history.coverage(storageRow: row), coverage.spanHz > 0 {
                span = SIMD2(Float(coverage.lowHz - referenceHz), Float(coverage.spanHz))
            }
            Array(history.storageLine(row)).withUnsafeBytes { raw in
                if let base = raw.baseAddress {
                    texture.replace(region: MTLRegionMake2D(0, row, history.columns, 1), mipmapLevel: 0,
                                    withBytes: base, bytesPerRow: history.columns)
                }
            }
            coverageTexture?.replace(region: MTLRegionMake2D(0, row, 1, 1), mipmapLevel: 0, withBytes: &span,
                                     bytesPerRow: MemoryLayout<SIMD2<Float>>.stride)
        }
    }

    /// The renderer's two textures for `history`, made as it makes them.
    static func textures(_ history: WaterfallHistory, device: any MTLDevice) throws
        -> (waterfall: any MTLTexture, coverage: any MTLTexture) {
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .r8Unorm, width: history.columns,
                                                                  height: history.capacity, mipmapped: false)
        descriptor.usage = .shaderRead
        let coverages = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rg32Float, width: 1,
                                                                 height: history.capacity, mipmapped: false)
        coverages.usage = .shaderRead
        return (try #require(device.makeTexture(descriptor: descriptor)),
                try #require(device.makeTexture(descriptor: coverages)))
    }

    static func bytes(of texture: any MTLTexture) -> [UInt8] {
        var bytes = [UInt8](repeating: 0xA5, count: texture.width * texture.height)
        texture.getBytes(&bytes, bytesPerRow: texture.width,
                         from: MTLRegionMake2D(0, 0, texture.width, texture.height), mipmapLevel: 0)
        return bytes
    }

    static func spans(of texture: any MTLTexture) -> [SIMD2<Float>] {
        var spans = [SIMD2<Float>](repeating: SIMD2(-1, -1), count: texture.height)
        texture.getBytes(&spans, bytesPerRow: MemoryLayout<SIMD2<Float>>.stride,
                         from: MTLRegionMake2D(0, 0, 1, texture.height), mipmapLevel: 0)
        return spans
    }

    /// A history of `lines` lines `columns` wide, every third line without
    /// its coverage. With `turnedFrom` the first half of the lines arrive
    /// that wide, so the rest re-map the history as a turn does.
    static func history(columns: Int, lines: Int, turnedFrom: Int? = nil) -> WaterfallHistory {
        var history = WaterfallHistory(capacity: capacity)
        let levels = WaterfallHistory.Levels(lowDbm: 0, highDbm: 255)
        var state: UInt64 = 0x9E37_79B9_7F4A_7C15
        for index in 0..<lines {
            let width = index < lines / 2 ? (turnedFrom ?? columns) : columns
            let line = (0..<width).map { _ -> Float in
                state = state &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
                return Float(state >> 56)
            }
            let coverage = index % 3 == 0 ? nil
                : BandCoverage(centerHz: referenceHz + Double(index) * 25, spanHz: 48_000 + Double(index))
            history.append(line: line, levels: levels, coverage: coverage)
        }
        return history
    }

    @Test(arguments: [1320, 2868], [false, true])
    func theWholeUploadMatchesTheRowByRowLoop(columns: Int, full: Bool) throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        // Partly full: rows fill from 0, so the head sits mid-storage. Full:
        // more lines than rows, so the history wraps, after a width change.
        let history = full ? Self.history(columns: columns, lines: Self.capacity + 77,
                                          turnedFrom: columns == 1320 ? 2868 : 1320)
            : Self.history(columns: columns, lines: 123)
        #expect(history.columns == columns)
        #expect(history.count == (full ? Self.capacity : 123))

        let reference = try Self.textures(history, device: device)
        Self.uploadRowByRow(history, referenceHz: Self.referenceHz, to: reference.waterfall,
                            coverage: reference.coverage)
        let whole = try Self.textures(history, device: device)
        BandRenderer.uploadWhole(history, referenceHz: Self.referenceHz, to: whole.waterfall,
                                 coverage: whole.coverage)

        #expect(Self.bytes(of: whole.waterfall) == Self.bytes(of: reference.waterfall))
        #expect(Self.spans(of: whole.coverage) == Self.spans(of: reference.coverage))
        // The lines really arrived: the newest is in its row.
        let newest = history.storageRow(age: 0)
        let row = Array(Self.bytes(of: whole.waterfall)[(newest * columns)..<((newest + 1) * columns)])
        #expect(row == Array(history.storageLine(newest)))
    }

    @Test func anEmptyHistoryUploadsNothing() throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        var history = Self.history(columns: 1320, lines: 5)
        history.clear()
        let whole = try Self.textures(history, device: device)
        let before = Self.bytes(of: whole.waterfall)
        BandRenderer.uploadWhole(history, referenceHz: Self.referenceHz, to: whole.waterfall,
                                 coverage: whole.coverage)
        #expect(Self.bytes(of: whole.waterfall) == before)
    }
}
