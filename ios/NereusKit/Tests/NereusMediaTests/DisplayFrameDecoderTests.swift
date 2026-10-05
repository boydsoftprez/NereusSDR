// NereusSDR for iOS: tests for the display frame decoder, with the link's NSDC vectors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusMedia

@Suite struct DisplayFrameDecoderTests {
    // MARK: The link's NSDC vectors (link document section 16.4)

    /// Every expectation field an NSDC vector may hold.
    private static let nsdcFields: Set<String> = [
        "after", "disposition", "reason", "keyframe", "endpointId", "contextGeneration",
        "minDbm", "maxDbm", "encoderSequence", "producerTimestamp", "waterfallAdvance",
        "traceDbm", "waterfallDbm", "wideDbm", "tolerance",
    ]

    @Test func everyNsdcVectorDecodesToItsExpectation() throws {
        let all = try LinkFixtureLoader.mediaVectors()
        let nsdc = all.values.filter { $0.codec == "nsdc1" }.sorted { $0.id < $1.id }
        #expect(Set(nsdc.map(\.id)) == [
            "media-nsdc1-full", "media-nsdc1-delta",
            "media-nsdc1-delta-after-loss", "media-nsdc1-keyframe-after-loss",
            "media-nsdc1-malformed-delta", "media-nsdc1-malformed-stale-delta", "media-nsdc1-malformed-keyframe",
            // The transmit display a Core sends while keyed (txDisplayVersion 1).
            "media-nsdc1-transmit",
        ])
        for vector in nsdc {
            if let failure = try check(vector, in: all) {
                Issue.record("\(vector.id): \(failure)")
            }
        }
    }

    /// Returns why `vector` fails, or nil when it passes.
    private func check(_ vector: LinkFixtureLoader.MediaVector,
                       in all: [String: LinkFixtureLoader.MediaVector]) throws -> String? {
        let expect = vector.expect
        if let unknown = Set(expect.keys).subtracting(Self.nsdcFields).sorted().first {
            return "unknown expectation field \"\(unknown)\""
        }
        let decoder = DisplayFrameDecoder()
        for earlier in try LinkFixtureLoader.after(vector, in: all) {
            _ = decoder.decode(earlier.bytes)
        }
        let result = decoder.decode(vector.bytes)

        guard let dispositionName = expect["disposition"] as? String,
              let disposition = DisplayDecodeDisposition(rawValue: dispositionName),
              let reasonName = expect["reason"] as? String,
              let reason = DisplayDecodeReason(rawValue: reasonName) else {
            return "disposition or reason is missing or unknown"
        }
        if result.disposition != disposition {
            return "disposition: expected \(disposition), got \(result.disposition)"
        }
        if result.reason != reason {
            return "reason: expected \(reason), got \(result.reason)"
        }
        guard let keyframe = expect["keyframe"] as? Bool else {
            return "keyframe is missing"
        }
        // The keyframe flag is the header's (byte 5, bit 0), with or without a frame.
        if (vector.bytes[vector.bytes.startIndex + 5] & 0x01 != 0) != keyframe {
            return "keyframe: the header's flag is not \(keyframe)"
        }

        guard let frame = result.frame else {
            if disposition == .accepted {
                return "an accepted vector gave no frame"
            }
            if expect["tolerance"] != nil || expect["traceDbm"] != nil {
                return "a vector without a frame carries frame fields"
            }
            return nil
        }
        if disposition != .accepted {
            return "a \(disposition) result carried a frame"
        }
        guard let tolerance = expect["tolerance"] as? [String: Any], tolerance.count == 1,
              let limit = (tolerance["dbm"] as? NSNumber)?.doubleValue else {
            return "an NSDC tolerance is {\"dbm\": <dB>}"
        }
        if frame.isKeyframe != keyframe {
            return "keyframe: expected \(keyframe), got \(frame.isKeyframe)"
        }
        if frame.waterfallAdvance != expect["waterfallAdvance"] as? Bool {
            return "waterfallAdvance: expected \(String(describing: expect["waterfallAdvance"]))"
        }
        let integers: [(String, UInt64)] = [
            ("endpointId", UInt64(frame.endpointId)),
            ("contextGeneration", UInt64(frame.contextGeneration)),
            ("encoderSequence", UInt64(frame.encoderSequence)),
            ("producerTimestamp", frame.producerTimestamp),
        ]
        for (field, value) in integers {
            guard let wanted = (expect[field] as? NSNumber)?.uint64Value else {
                return "\(field) is missing"
            }
            if value != wanted {
                return "\(field): expected \(wanted), got \(value)"
            }
        }
        // The tolerance applies to every number of the decoded frame.
        for (field, value) in [("minDbm", frame.minDbm), ("maxDbm", frame.maxDbm)] {
            guard let wanted = (expect[field] as? NSNumber)?.doubleValue else {
                return "\(field) is missing"
            }
            if abs(Double(value) - wanted) > limit {
                return "\(field): expected \(wanted), got \(value)"
            }
        }
        let rows: [(String, [Float])] = [
            ("traceDbm", frame.traceDbm), ("waterfallDbm", frame.waterfallDbm), ("wideDbm", frame.wideDbm),
        ]
        for (field, row) in rows {
            guard let wanted = (expect[field] as? [NSNumber])?.map(\.doubleValue) else {
                return "\(field) must be an array of numbers"
            }
            if row.count != wanted.count {
                return "\(field): expected \(wanted.count) samples, got \(row.count)"
            }
            for (index, (got, want)) in zip(row, wanted).enumerated() where abs(Double(got) - want) > limit {
                return "\(field)[\(index)]: expected \(want), got \(got)"
            }
        }
        return nil
    }

    // MARK: Loss and recovery

    @Test func aLostDeltaNeedsAKeyframeUntilOneArrives() throws {
        let decoder = DisplayFrameDecoder()
        #expect(decoder.needsKeyframe, "nothing has been accepted yet")
        #expect(decoder.decode(try Self.vector("full")).disposition == .accepted)
        #expect(!decoder.needsKeyframe)

        let gap = decoder.decode(try Self.vector("delta-after-loss"))
        #expect(gap == DisplayDecodeResult(disposition: .needKeyframe, reason: .sequenceGap, frame: nil))
        #expect(decoder.needsKeyframe)
        // The delta that was lost arriving late cannot help either.
        let late = decoder.decode(try Self.vector("delta"))
        #expect(late.disposition == .needKeyframe && late.reason == .sequenceGap)
        #expect(decoder.needsKeyframe)

        let keyframe = decoder.decode(try Self.vector("keyframe-after-loss"))
        #expect(keyframe.disposition == .accepted && keyframe.reason == .none)
        #expect(keyframe.frame?.encoderSequence == 4)
        #expect(!decoder.needsKeyframe)
    }

    @Test func aDeltaWithNoHistoryNeedsAKeyframe() throws {
        let decoder = DisplayFrameDecoder()
        let result = decoder.decode(try Self.vector("delta"))
        #expect(result == DisplayDecodeResult(disposition: .needKeyframe, reason: .noHistory, frame: nil))
        #expect(decoder.needsKeyframe)
    }

    @Test func anOlderOrEqualSequenceIsStale() {
        let decoder = DisplayFrameDecoder()
        #expect(decoder.decode(FrameBuilder(sequence: 10).bytes()).disposition == .accepted)
        for sequence: UInt32 in [10, 9, 10 &- 0x7FFF_FFFF, 10 &+ 0x8000_0000] {
            let result = decoder.decode(FrameBuilder(sequence: sequence).bytes())
            #expect(result == DisplayDecodeResult(disposition: .rejected, reason: .staleSequence, frame: nil),
                    "sequence \(sequence)")
        }
        // Just under half the range ahead is newer; a keyframe needs no chain.
        let ahead = decoder.decode(FrameBuilder(sequence: 10 &+ 0x7FFF_FFFF).bytes())
        #expect(ahead.disposition == .accepted)
    }

    @Test func theSequenceWrapsAtThirtyTwoBits() {
        let decoder = DisplayFrameDecoder()
        let first = FrameBuilder(sequence: UInt32.max, trace: [100, 101], waterfall: [7])
        #expect(decoder.decode(first.bytes()).disposition == .accepted)
        let next = FrameBuilder(sequence: 0, trace: [102, 99], waterfall: [7])
        let result = decoder.decode(next.bytes(keyframe: false, previous: first))
        #expect(result.disposition == .accepted)
        #expect(result.frame?.traceDbm == [102, 99].map { next.dbm($0) })
    }

    @Test func contextGenerationsAreOrdered() {
        let decoder = DisplayFrameDecoder()
        let base = FrameBuilder(generation: 5, sequence: 1)
        #expect(decoder.decode(base.bytes()).disposition == .accepted)

        let older = decoder.decode(FrameBuilder(generation: 4, sequence: 2).bytes())
        #expect(older == DisplayDecodeResult(disposition: .rejected, reason: .oldContext, frame: nil))

        let newerDelta = FrameBuilder(generation: 6, sequence: 1)
        let needs = decoder.decode(newerDelta.bytes(keyframe: false, previous: base))
        #expect(needs == DisplayDecodeResult(disposition: .needKeyframe, reason: .contextMismatch, frame: nil))
        #expect(decoder.needsKeyframe)

        let newer = FrameBuilder(generation: 6, sequence: 1, minDbm: -130, maxDbm: -30, trace: [1, 2, 3])
        let accepted = decoder.decode(newer.bytes())
        #expect(accepted.disposition == .accepted)
        #expect(accepted.frame?.contextGeneration == 6)
        #expect(!decoder.needsKeyframe)
    }

    @Test func anotherEndpointOrAChangedShapeIsAMismatch() {
        let decoder = DisplayFrameDecoder()
        #expect(decoder.decode(FrameBuilder(sequence: 1).bytes()).disposition == .accepted)
        let other = decoder.decode(FrameBuilder(endpointId: 2, sequence: 2).bytes())
        #expect(other == DisplayDecodeResult(disposition: .rejected, reason: .contextMismatch, frame: nil))
        let shape = decoder.decode(FrameBuilder(sequence: 2, trace: [1, 2, 3, 4, 5]).bytes())
        #expect(shape == DisplayDecodeResult(disposition: .rejected, reason: .contextMismatch, frame: nil))
        let range = decoder.decode(FrameBuilder(sequence: 2, maxDbm: -20).bytes())
        #expect(range == DisplayDecodeResult(disposition: .rejected, reason: .contextMismatch, frame: nil))
    }

    // MARK: Reconstruction

    @Test func residualsOfEveryWidthRebuildTheRows() {
        for width in 1...8 {
            let decoder = DisplayFrameDecoder()
            let span = 1 << (width - 1)
            // Near mid-range, so every residual the width holds stays in 0...255.
            let base = (0..<40).map { UInt8(width == 8 ? 128 : 100 + $0 % 7) }
            let first = FrameBuilder(sequence: 1, trace: base, waterfall: base.reversed(), wide: [0, 255, 128])
            #expect(decoder.decode(first.bytes()).disposition == .accepted)
            // Residuals from -span to span - 1, the whole range the width holds.
            let moved = base.enumerated().map { UInt8(Int($1) - span + $0 % (2 * span)) }
            let second = FrameBuilder(sequence: 2, trace: moved, waterfall: base.reversed(), wide: [0, 255, 128])
            let result = decoder.decode(second.bytes(keyframe: false, previous: first, residualWidth: width))
            #expect(result.disposition == .accepted, "width \(width)")
            #expect(result.frame?.traceDbm == moved.map { second.dbm($0) }, "width \(width)")
            #expect(result.frame?.wideDbm == [0, 255, 128].map { second.dbm($0) }, "width \(width)")
        }
    }

    @Test func everyBlockSizeDecodesAndTheEndsOfTheRangeAreExact() {
        let row = (0..<300).map { UInt8($0 % 256) }
        for code in 0..<4 {
            let builder = FrameBuilder(sequence: 1, trace: row, waterfall: row, blockCode: code)
            let result = DisplayFrameDecoder().decode(builder.bytes())
            #expect(result.disposition == .accepted, "block code \(code)")
            #expect(result.frame?.traceDbm.first == -140)
            #expect(result.frame?.traceDbm[255] == -40)
            #expect(result.frame?.wideDbm == [])
        }
    }

    // MARK: Malformed input

    /// Decodes `bytes` after `nsdc1-full`, expects `reason`, then checks the
    /// history survived: `nsdc1-delta` still decodes to its expectation.
    private func expectRefused(_ bytes: Data, _ reason: DisplayDecodeReason,
                               sourceLocation: SourceLocation = #_sourceLocation) throws {
        let decoder = DisplayFrameDecoder()
        #expect(decoder.decode(try Self.vector("full")).disposition == .accepted,
                sourceLocation: sourceLocation)
        let result = decoder.decode(bytes)
        #expect(result == DisplayDecodeResult(disposition: .rejected, reason: reason, frame: nil),
                sourceLocation: sourceLocation)
        #expect(!decoder.needsKeyframe, sourceLocation: sourceLocation)
        let delta = decoder.decode(try Self.vector("delta"))
        let reference = DisplayFrameDecoder()
        _ = reference.decode(try Self.vector("full"))
        #expect(delta == reference.decode(try Self.vector("delta")), sourceLocation: sourceLocation)
        #expect(delta.disposition == .accepted, sourceLocation: sourceLocation)
    }

    @Test func aTruncatedHeaderIsTruncated() throws {
        let full = try Self.vector("full")
        for length in [0, 3, 4, 5, 7, 20, 41] {
            try expectRefused(full.prefix(length), .truncated)
        }
    }

    @Test func aTruncatedPlaneIsMalformed() throws {
        let full = try Self.vector("full")
        for length in [42, 44, 50, full.count - 1] {
            try expectRefused(full.prefix(length), .malformed)
        }
    }

    @Test func aWrongMagicIsBadMagic() throws {
        var bytes = try Self.vector("full")
        bytes[bytes.startIndex] = UInt8(ascii: "X")
        try expectRefused(bytes, .badMagic)
    }

    @Test func anUnknownVersionIsUnsupported() throws {
        for version: UInt8 in [0, 2, 255] {
            var bytes = try Self.vector("full")
            bytes[bytes.startIndex + 4] = version
            try expectRefused(bytes, .unsupportedVersion)
        }
    }

    @Test func anUnknownFlagIsRefused() throws {
        var bytes = try Self.vector("full")
        bytes[bytes.startIndex + 5] |= 0x08
        try expectRefused(bytes, .unknownFlags)
    }

    @Test func aRowLongerThan4096SamplesIsMalformed() throws {
        // The station's decoder refuses a row over 4096 samples as malformed.
        let long = [UInt8](repeating: 9, count: 4097)
        try expectRefused(FrameBuilder(sequence: 2, trace: long).bytes(), .malformed)
        try expectRefused(FrameBuilder(sequence: 2, waterfall: long).bytes(), .malformed)
        try expectRefused(FrameBuilder(sequence: 2, wide: long).bytes(), .malformed)
        // Only the declared length, too: the header says 4097, the plane never arrives.
        var bytes = try Self.vector("full")
        bytes[bytes.startIndex + 36] = 0x10
        bytes[bytes.startIndex + 37] = 0x01
        try expectRefused(bytes, .malformed)
        // 4096 is allowed.
        let most = [UInt8](repeating: 9, count: 4096)
        #expect(DisplayFrameDecoder().decode(FrameBuilder(sequence: 1, trace: most).bytes()).disposition
                == .accepted)
    }

    @Test func aFrameOverTheCodecBoundIsOversized() throws {
        var bytes = try Self.vector("full")
        bytes.append(Data(count: 16 * 1024 + 1 - bytes.count))
        try expectRefused(bytes, .oversized)
    }

    @Test func badHeadersAndPlanesAreMalformed() throws {
        let builder = FrameBuilder(sequence: 2)
        let header = { (edit: (inout Data) -> Void) -> Data in
            var bytes = builder.bytes()
            edit(&bytes)
            return bytes
        }
        // Header size other than 42.
        try expectRefused(header { $0[7] = 43 }, .malformed)
        // An empty required row, a wide flag without a wide row, a wide row without the flag.
        try expectRefused(FrameBuilder(sequence: 2, trace: []).bytes(), .malformed)
        try expectRefused(header { $0[5] |= 0x04 }, .malformed)
        try expectRefused(FrameBuilder(sequence: 2, wide: [1]).bytes(wideFlag: false), .malformed)
        // A range that is not finite or not increasing.
        try expectRefused(FrameBuilder(sequence: 2, minDbm: -40, maxDbm: -40).bytes(), .malformed)
        try expectRefused(FrameBuilder(sequence: 2, minDbm: .nan).bytes(), .malformed)
        try expectRefused(FrameBuilder(sequence: 2, maxDbm: .infinity).bytes(), .malformed)
        // Planes: bad block size code, block count, sample count, mode, width, payload size.
        let plane = FrameBuilder.headerBytes
        try expectRefused(header { $0[plane] = 4 }, .malformed)
        try expectRefused(header { $0[plane + 2] += 1 }, .malformed)
        try expectRefused(header { $0[plane + 5] += 1 }, .malformed)
        try expectRefused(header { $0[plane + 3] = 2 }, .malformed)
        try expectRefused(header { $0[plane + 4] = 7 }, .malformed)
        try expectRefused(header { $0[plane + 7] += 1 }, .malformed)
        // Bytes after the last plane.
        try expectRefused(header { $0.append(0) }, .malformed)
    }

    @Test func aKeyframeWithAResidualBlockIsMalformed() throws {
        let first = FrameBuilder(sequence: 1)
        var bytes = FrameBuilder(sequence: 2).bytes(keyframe: false, previous: first, residualWidth: 4)
        bytes[5] |= 0x01
        try expectRefused(bytes, .malformed)
    }

    @Test func aResidualThatLeavesTheRangeIsMalformed() throws {
        let decoder = DisplayFrameDecoder()
        let first = FrameBuilder(sequence: 1, trace: [255, 0], waterfall: [0])
        #expect(decoder.decode(first.bytes()).disposition == .accepted)
        // Stored values the builder cannot make: +1 on 255, then -1 on 0.
        for (index, stored) in [(0, 0b1100_0000 as UInt8), (1, 0b0100_0000 as UInt8)] {
            var bytes = FrameBuilder(sequence: 2, trace: [255, 0], waterfall: [0])
                .bytes(keyframe: false, previous: first, residualWidth: 2)
            // Trace plane: 3-byte plane header, 5-byte block header, one payload byte.
            bytes[FrameBuilder.headerBytes + 8] = stored
            let result = decoder.decode(bytes)
            #expect(result == DisplayDecodeResult(disposition: .rejected, reason: .malformed, frame: nil),
                    "sample \(index)")
        }
        let intact = FrameBuilder(sequence: 2, trace: [254, 1], waterfall: [0])
        #expect(decoder.decode(intact.bytes(keyframe: false, previous: first, residualWidth: 2)).disposition
                == .accepted)
    }

    @Test func randomBytesNeverCrash() {
        var generator = SystemRandomNumberGenerator()
        let decoder = DisplayFrameDecoder()
        for _ in 0..<2000 {
            let length = Int.random(in: 0...200, using: &generator)
            var bytes = Data((0..<length).map { _ in UInt8.random(in: 0...255, using: &generator) })
            if length >= 5, Bool.random(using: &generator) {
                bytes.replaceSubrange(0..<5, with: [0x4E, 0x53, 0x44, 0x43, 0x01])
            }
            #expect(decoder.decode(bytes).disposition != .accepted || length >= 42)
        }
    }

    // MARK: Helpers

    private static func vector(_ name: String) throws -> Data {
        let all = try LinkFixtureLoader.mediaVectors()
        guard let vector = all["media-nsdc1-\(name)"] else {
            throw LinkFixtureLoader.Malformed(description: "no vector media-nsdc1-\(name)")
        }
        return vector.bytes
    }
}

/// Builds NSDC v1 frames from quantised rows, from the codec document's
/// byte layout, for the cases the link's vectors do not hold.
private struct FrameBuilder {
    static let headerBytes = 42

    var endpointId: UInt32 = 1
    var generation: UInt32 = 1
    var sequence: UInt32
    var timestamp: UInt64 = 1_000
    var minDbm: Float = -140
    var maxDbm: Float = -40
    var trace: [UInt8] = [10, 20, 30, 40]
    var waterfall: [UInt8] = [50, 60]
    var wide: [UInt8] = []
    var blockCode = 3

    func dbm(_ q: UInt8) -> Float {
        DisplayFrameDecoder.dbm(q, minimum: Double(minDbm), maximum: Double(maxDbm))
    }

    /// A keyframe, or with `keyframe` false a delta over `previous` whose
    /// blocks are residuals of `residualWidth` bits (absolute when nil).
    func bytes(keyframe: Bool = true, previous: FrameBuilder? = nil,
               residualWidth: Int? = nil, wideFlag: Bool? = nil) -> Data {
        var out = Data("NSDC".utf8)
        var flags: UInt8 = keyframe ? 0x01 : 0
        flags |= 0x02
        if wideFlag ?? !wide.isEmpty {
            flags |= 0x04
        }
        out.append(1)
        out.append(flags)
        Self.put(UInt64(Self.headerBytes), 2, into: &out)
        Self.put(UInt64(endpointId), 4, into: &out)
        Self.put(UInt64(generation), 4, into: &out)
        Self.put(UInt64(sequence), 4, into: &out)
        Self.put(timestamp, 8, into: &out)
        Self.put(UInt64(minDbm.bitPattern), 4, into: &out)
        Self.put(UInt64(maxDbm.bitPattern), 4, into: &out)
        Self.put(UInt64(trace.count), 2, into: &out)
        Self.put(UInt64(waterfall.count), 2, into: &out)
        Self.put(UInt64(wide.count), 2, into: &out)
        let width = keyframe ? nil : residualWidth
        plane(trace, over: previous?.trace, width: width, into: &out)
        plane(waterfall, over: previous?.waterfall, width: width, into: &out)
        if !wide.isEmpty {
            plane(wide, over: previous?.wide, width: width, into: &out)
        }
        return out
    }

    private func plane(_ row: [UInt8], over previous: [UInt8]?, width: Int?, into out: inout Data) {
        let size = [16, 32, 64, 128][blockCode]
        let blocks = (row.count + size - 1) / size
        out.append(UInt8(blockCode))
        Self.put(UInt64(blocks), 2, into: &out)
        for block in 0..<blocks {
            let slice = Array(row[(block * size)..<min(row.count, (block + 1) * size)])
            if let width, let previous {
                let base = Array(previous[(block * size)..<min(previous.count, (block + 1) * size)])
                var bits: [Int] = []
                for (value, old) in zip(slice, base) {
                    let stored = Int(value) - Int(old) + (1 << (width - 1))
                    precondition((0..<(1 << width)).contains(stored), "residual does not fit")
                    bits += (0..<width).map { (stored >> (width - 1 - $0)) & 1 }
                }
                var payload = [UInt8](repeating: 0, count: (bits.count + 7) / 8)
                for (index, bit) in bits.enumerated() where bit == 1 {
                    payload[index / 8] |= UInt8(0x80 >> (index % 8))
                }
                out.append(contentsOf: [0, UInt8(width), UInt8(slice.count)])
                Self.put(UInt64(payload.count), 2, into: &out)
                out.append(contentsOf: payload)
            } else {
                out.append(contentsOf: [1, 8, UInt8(slice.count)])
                Self.put(UInt64(slice.count), 2, into: &out)
                out.append(contentsOf: slice)
            }
        }
    }

    private static func put(_ value: UInt64, _ bytes: Int, into out: inout Data) {
        for index in (0..<bytes).reversed() {
            out.append(UInt8((value >> (8 * UInt64(index))) & 0xFF))
        }
    }
}
