// NereusSDR for iOS: tests for the NSDX display extras decoder and request, with the link's nsdx1 vectors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// The display extras document (`2026-09-23-display-extras-v1.md`),
/// sections 2 to 4, and R-IOS-11. The app computes none of the extras (D4);
/// it decodes what the Core sends.
@Suite struct DisplayExtrasDecoderTests {
    // MARK: The link's nsdx1 vectors (section 4)

    /// Every expectation field an nsdx1 vector may hold.
    private static let nsdxFields: Set<String> = [
        "context", "accepted", "reason", "endpointId", "contextGeneration", "encoderSequence",
        "peakBlobs", "peakHoldDbm", "noiseFloorDbm", "waterfallLevelsDbm", "noiseFloorFastAttack", "tolerance",
    ]

    @Test func everyNsdxVectorDecodesToItsExpectation() throws {
        let all = try LinkFixtureLoader.mediaVectors()
        let nsdx = all.values.filter { $0.codec == "nsdx1" }.sorted { $0.id < $1.id }
        #expect(Set(nsdx.map(\.id)) == [
            "media-nsdx1-full", "media-nsdx1-noise-floor", "media-nsdx1-noise-floor-state",
            "media-nsdx1-other-generation", "media-nsdx1-unknown-section", "media-nsdx1-truncated",
        ])
        for vector in nsdx {
            if let failure = check(vector) {
                Issue.record("\(vector.id): \(failure)")
            }
        }
    }

    /// Returns why `vector` fails, or nil when it passes.
    private func check(_ vector: LinkFixtureLoader.MediaVector) -> String? {
        let expect = vector.expect
        if let unknown = Set(expect.keys).subtracting(Self.nsdxFields).sorted().first {
            return "unknown expectation field \"\(unknown)\""
        }
        guard let context = Self.context(expect["context"]) else {
            return "context is missing or not the five fields"
        }
        guard let accepted = expect["accepted"] as? Bool, let reasonName = expect["reason"] as? String,
              let reason = DisplayExtrasDecoder.Reason(rawValue: reasonName) else {
            return "accepted or reason is missing or unknown"
        }
        let result = DisplayExtrasDecoder.decode(vector.bytes, context: context)
        if result.accepted != accepted {
            return "accepted: expected \(accepted), got \(result.accepted)"
        }
        if result.reason != reason {
            return "reason: expected \(reason), got \(result.reason)"
        }
        guard let extras = result.extras else {
            let present = Set(expect.keys).subtracting(["context", "accepted", "reason"])
            return present.isEmpty ? nil : "a refused vector carries \(present.sorted())"
        }
        // Exactly the frame fields, the tolerance and the sections it carries.
        var keys: Set<String> = ["context", "accepted", "reason", "endpointId", "contextGeneration",
                                 "encoderSequence", "tolerance"]
        if extras.peakBlobs != nil {
            keys.insert("peakBlobs")
        }
        if extras.peakHoldDbm != nil {
            keys.insert("peakHoldDbm")
        }
        if extras.noiseFloorDbm != nil {
            keys.insert("noiseFloorDbm")
        }
        if extras.waterfallLevels != nil {
            keys.insert("waterfallLevelsDbm")
        }
        if extras.noiseFloorFastAttack != nil {
            keys.insert("noiseFloorFastAttack")
        }
        if Set(expect.keys) != keys {
            return "keys: expected \(keys.sorted()), the vector has \(expect.keys.sorted())"
        }
        guard let tolerance = expect["tolerance"] as? [String: Any], tolerance.count == 1,
              let limit = (tolerance["dbm"] as? NSNumber)?.doubleValue else {
            return "an nsdx1 tolerance is {\"dbm\": <dB>}"
        }
        let integers: [(String, UInt32)] = [("endpointId", extras.endpointId),
                                            ("contextGeneration", extras.contextGeneration),
                                            ("encoderSequence", extras.encoderSequence)]
        for (field, value) in integers where (expect[field] as? NSNumber)?.uint32Value != value {
            return "\(field): expected \(String(describing: expect[field])), got \(value)"
        }
        func near(_ got: Float, _ want: Any?) -> Bool {
            guard let want = (want as? NSNumber)?.doubleValue else {
                return false
            }
            return abs(Double(got) - want) <= limit
        }
        if let blobs = extras.peakBlobs {
            guard let wanted = expect["peakBlobs"] as? [[String: Any]], wanted.count == blobs.count else {
                return "peakBlobs: expected \(String(describing: expect["peakBlobs"])), got \(blobs)"
            }
            for (index, (blob, want)) in zip(blobs, wanted).enumerated() {
                if Set(want.keys) != ["pixel", "dbm"] || (want["pixel"] as? NSNumber)?.intValue != blob.traceSample
                    || !near(blob.dbm, want["dbm"]) {
                    return "peakBlobs[\(index)]: expected \(want), got \(blob)"
                }
            }
        }
        if let hold = extras.peakHoldDbm {
            guard let wanted = expect["peakHoldDbm"] as? [Any], wanted.count == hold.count else {
                return "peakHoldDbm: expected \(context.traceSamples) samples, got \(hold.count)"
            }
            for (index, (got, want)) in zip(hold, wanted).enumerated() where !near(got, want) {
                return "peakHoldDbm[\(index)]: expected \(want), got \(got)"
            }
        }
        if let floor = extras.noiseFloorDbm, !near(floor, expect["noiseFloorDbm"]) {
            return "noiseFloorDbm: expected \(String(describing: expect["noiseFloorDbm"])), got \(floor)"
        }
        if let fastAttack = extras.noiseFloorFastAttack, expect["noiseFloorFastAttack"] as? Bool != fastAttack {
            return "noiseFloorFastAttack: expected \(String(describing: expect["noiseFloorFastAttack"])), got \(fastAttack)"
        }
        if let levels = extras.waterfallLevels {
            guard let wanted = expect["waterfallLevelsDbm"] as? [String: Any], Set(wanted.keys) == ["lowDbm", "highDbm"],
                  near(levels.lowDbm, wanted["lowDbm"]), near(levels.highDbm, wanted["highDbm"]) else {
                return "waterfallLevelsDbm: expected \(String(describing: expect["waterfallLevelsDbm"])), got \(levels)"
            }
        }
        return nil
    }

    /// An expectation's `context`: exactly its five fields.
    private static func context(_ value: Any?) -> DisplayExtrasDecoder.Context? {
        guard let object = value as? [String: Any],
              Set(object.keys) == ["endpointId", "contextGeneration", "minDbm", "maxDbm", "traceSamples"],
              let endpointId = (object["endpointId"] as? NSNumber)?.uint32Value,
              let generation = (object["contextGeneration"] as? NSNumber)?.uint32Value,
              let minDbm = (object["minDbm"] as? NSNumber)?.floatValue,
              let maxDbm = (object["maxDbm"] as? NSNumber)?.floatValue,
              let samples = (object["traceSamples"] as? NSNumber)?.intValue else {
            return nil
        }
        return DisplayExtrasDecoder.Context(endpointId: endpointId, contextGeneration: generation,
                                            minDbm: minDbm, maxDbm: maxDbm, traceSamples: samples)
    }

    // MARK: Refusals, in the document's order (section 3.4)

    /// `nsdx1-full`'s bytes and the context they decode against.
    private static func full() throws -> (bytes: Data, context: DisplayExtrasDecoder.Context) {
        let vector = try #require(try LinkFixtureLoader.mediaVectors()["media-nsdx1-full"])
        return (vector.bytes, try #require(context(vector.expect["context"])))
    }

    private func expectRefused(_ bytes: Data, _ reason: DisplayExtrasDecoder.Reason,
                               context: DisplayExtrasDecoder.Context?,
                               sourceLocation: SourceLocation = #_sourceLocation) {
        let result = DisplayExtrasDecoder.decode(bytes, context: context)
        #expect(result.reason == reason, sourceLocation: sourceLocation)
        #expect(result.extras == nil, sourceLocation: sourceLocation)
    }

    @Test func theHeaderIsCheckedInOrder() throws {
        let (full, context) = try Self.full()
        // Oversized comes before everything else, the magic included.
        var huge = Data("XXXX".utf8)
        huge.append(Data(count: 16 * 1024 + 1 - huge.count))
        expectRefused(huge, .oversized, context: context)
        for length in [0, 3] {
            expectRefused(full.prefix(length), .truncated, context: context)
        }
        var magic = full
        magic[magic.startIndex] = UInt8(ascii: "Y")
        expectRefused(magic, .badMagic, context: context)
        // NSDC is not NSDX.
        var nsdc = full
        nsdc[nsdc.startIndex + 3] = UInt8(ascii: "C")
        expectRefused(nsdc, .badMagic, context: context)
        expectRefused(full.prefix(4), .truncated, context: context)
        for version: UInt8 in [0, 2, 255] {
            var bytes = full
            bytes[bytes.startIndex + 4] = version
            // The version is read before the rest of the header is needed.
            expectRefused(bytes.prefix(5), .unsupportedVersion, context: context)
        }
        for length in [5, 6, 8, 12, 16, 19] {
            expectRefused(full.prefix(length), .truncated, context: context)
        }
        // Version 4 knows 0x10 (the noise floor's state); every higher bit is unknown.
        for bit: UInt8 in [0x20, 0x40, 0x80] {
            var bytes = full
            bytes[bytes.startIndex + 5] |= bit
            // Unknown sections come before the context and the header size.
            bytes[bytes.startIndex + 7] = 21
            expectRefused(bytes, .unknownSections, context: nil)
        }
        var none = full
        none[none.startIndex + 5] = 0
        expectRefused(none, .malformed, context: nil)
        var size = full
        size[size.startIndex + 7] = 21
        expectRefused(size, .malformed, context: nil)
        // Then the context: none accepted, another endpoint or another generation.
        expectRefused(full, .contextMismatch, context: nil)
        var endpoint = context
        endpoint.endpointId += 1
        expectRefused(full, .contextMismatch, context: endpoint)
        var generation = context
        generation.contextGeneration += 1
        expectRefused(full, .contextMismatch, context: generation)
        #expect(DisplayExtrasDecoder.decode(full, context: context).accepted)
    }

    @Test func eachSectionIsChecked() throws {
        let context = DisplayExtrasDecoder.Context(endpointId: 3, contextGeneration: 9, minDbm: -140, maxDbm: -40,
                                                   traceSamples: 8)
        let good = DatagramBuilder(blobs: [(2, -90), (7, -95)], hold: [UInt8](repeating: 0, count: 8),
                                   floor: -120, levels: (-130, -70), state: 0x01)
        let decoded = DisplayExtrasDecoder.decode(good.bytes(), context: context)
        let extras = try #require(decoded.extras)
        #expect(extras.peakBlobs == [.init(traceSample: 2, dbm: -90), .init(traceSample: 7, dbm: -95)])
        // A sample the hold has not reached travels as the context's minDbm.
        #expect(extras.peakHoldDbm == [Float](repeating: -140, count: 8))
        #expect(extras.noiseFloorDbm == -120)
        #expect(extras.waterfallLevels == .init(lowDbm: -130, highDbm: -70))
        #expect(extras.noiseFloorFastAttack == true)
        // The state byte: bit 0 only; any other bit is malformed.
        #expect(DisplayExtrasDecoder.decode(DatagramBuilder(floor: -120, state: 0).bytes(), context: context)
            .extras?.noiseFloorFastAttack == false)
        expectRefused(DatagramBuilder(floor: -120, state: 0x02).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(floor: -120, state: 0x81).bytes(), .malformed, context: context)

        // More than 20 blobs, a blob past the trace, a float that is not finite.
        expectRefused(DatagramBuilder(blobs: Array(repeating: (0, -90), count: 21)).bytes(), .malformed,
                      context: context)
        #expect(DisplayExtrasDecoder.decode(DatagramBuilder(blobs: Array(repeating: (0, -90), count: 20)).bytes(),
                                            context: context).accepted)
        expectRefused(DatagramBuilder(blobs: [(8, -90)]).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(blobs: [(1, .nan)]).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(floor: .infinity).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(levels: (-130, -.infinity)).bytes(), .malformed, context: context)
        // A peak hold plane of another length, or with a residual block.
        expectRefused(DatagramBuilder(hold: [UInt8](repeating: 1, count: 7)).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(hold: [UInt8](repeating: 1, count: 9)).bytes(), .malformed, context: context)
        expectRefused(DatagramBuilder(hold: [UInt8](repeating: 1, count: 8), residual: true).bytes(), .malformed,
                      context: context)
        // Bytes that run out inside a section, and a byte after the last.
        let bytes = good.bytes()
        expectRefused(bytes.prefix(DatagramBuilder.headerBytes), .truncated, context: context)
        expectRefused(bytes.prefix(DatagramBuilder.headerBytes + 3), .truncated, context: context)
        expectRefused(bytes.dropLast(), .truncated, context: context)
        expectRefused(DatagramBuilder(floor: -120).bytes().dropLast(2), .truncated, context: context)
        // Inside the peak hold plane, as the station's decoder has it: malformed.
        expectRefused(DatagramBuilder(hold: [UInt8](repeating: 1, count: 8)).bytes().dropLast(), .malformed,
                      context: context)
        var trailing = bytes
        trailing.append(0)
        expectRefused(trailing, .malformed, context: context)
    }

    @Test func randomBytesNeverCrash() {
        var generator = SystemRandomNumberGenerator()
        let context = DisplayExtrasDecoder.Context(endpointId: 1, contextGeneration: 1, minDbm: -140, maxDbm: -40,
                                                   traceSamples: 32)
        for _ in 0..<2000 {
            let length = Int.random(in: 0...200, using: &generator)
            var bytes = Data((0..<length).map { _ in UInt8.random(in: 0...255, using: &generator) })
            if length >= 20, Bool.random(using: &generator) {
                bytes.replaceSubrange(0..<16, with: [0x4E, 0x53, 0x44, 0x58, 1, bytes[5] & 0x0F, 0, 20,
                                                     0, 0, 0, 1, 0, 0, 0, 1])
            }
            _ = DisplayExtrasDecoder.decode(bytes, context: context)
        }
    }

    // MARK: The request (section 2)

    static let everything = DisplayExtrasRequest(
        peakBlobs: .init(count: 5, holdMs: 500, fallDbPerSec: 6, insideOnly: false),
        activePeakHold: .init(enabled: true, holdMs: 2000, fallDbPerSec: 6, onTx: false),
        noiseFloor: .init(enabled: true, shiftDb: 0, fastAttack: true),
        waterfallLevels: .init(mode: .clarity, lowDbm: -122, highDbm: -62, offsetDb: 0),
        normalize: false, calibrationOffsetDb: 0, averageTimeMs: 30, waterfallAverageTimeMs: 120)

    @Test func theWorstCaseSizeIsTheDocumentsFormula() {
        // Section 3.3 at version 4: every section at the largest trace, the
        // noise floor's one-byte state included, is 4413 bytes.
        #expect(Self.everything.worstCaseBytesPerFrame(traceSamples: 4096) == 4413)
        for samples in [1, 32, 127, 128, 129, 1170, 4096] {
            let a = 3 + 5 * ((samples + 127) / 128) + samples
            #expect(Self.everything.worstCaseBytesPerFrame(traceSamples: samples) == 20 + 121 + a + 4 + 8 + 1)
            #expect(DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 1))
                .worstCaseBytesPerFrame(traceSamples: samples) == 24)
            #expect(DisplayExtrasRequest(activePeakHold: .init(enabled: true, holdMs: 100, fallDbPerSec: 1))
                .worstCaseBytesPerFrame(traceSamples: samples) == 20 + a)
        }
        // Sections that are off, or fields that ask for no section, cost nothing.
        let quiet = DisplayExtrasRequest(activePeakHold: .init(enabled: false, holdMs: 100, fallDbPerSec: 1),
                                         noiseFloor: .init(enabled: false, shiftDb: 0), normalize: true,
                                         calibrationOffsetDb: 3, averageTimeMs: 50, waterfallAverageTimeMs: 50)
        #expect(quiet.sections == 0)
        #expect(quiet.worstCaseBytesPerFrame(traceSamples: 4096) == 0)
        #expect(DisplayExtrasRequest().isEmpty)
        #expect(!quiet.isEmpty)
    }

    @Test func everyRangeIsTheDocuments() throws {
        try Self.everything.validate()
        typealias Request = DisplayExtrasRequest
        let good: [Request] = [
            Request(peakBlobs: .init(count: 1, holdMs: 0, fallDbPerSec: 0, insideOnly: true)),
            Request(peakBlobs: .init(count: 20, holdMs: 60_000, fallDbPerSec: 60, insideOnly: false)),
            Request(peakBlobs: .init(count: 3, holdMs: 100, fallDbPerSec: 1, insideOnly: false)),
            Request(activePeakHold: .init(enabled: false, holdMs: 100, fallDbPerSec: 0.1)),
            Request(activePeakHold: .init(enabled: true, holdMs: 60_000, fallDbPerSec: 120)),
            Request(noiseFloor: .init(enabled: true, shiftDb: -12)),
            Request(noiseFloor: .init(enabled: true, shiftDb: 12)),
            Request(waterfallLevels: .init(mode: .manual, lowDbm: -400, highDbm: 100, offsetDb: -60)),
            Request(waterfallLevels: .init(mode: .noiseFloorAgc, lowDbm: 100, highDbm: -400, offsetDb: 60)),
            Request(calibrationOffsetDb: -30), Request(calibrationOffsetDb: 30),
            Request(averageTimeMs: 10), Request(averageTimeMs: 9999),
            Request(waterfallAverageTimeMs: 10), Request(waterfallAverageTimeMs: 9999),
        ]
        for request in good {
            #expect(throws: Never.self, "\(request)") { try request.validate() }
        }
        let bad: [(Request, DisplayExtrasRequest.Invalid)] = [
            (Request(peakBlobs: .init(count: 0, holdMs: 0, fallDbPerSec: 0, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 21, holdMs: 0, fallDbPerSec: 0, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 3, holdMs: 99, fallDbPerSec: 0, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 3, holdMs: 60_001, fallDbPerSec: 0, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 3, holdMs: 0, fallDbPerSec: 0.5, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 3, holdMs: 0, fallDbPerSec: 61, insideOnly: false)), .peakBlobs),
            (Request(peakBlobs: .init(count: 3, holdMs: 0, fallDbPerSec: .nan, insideOnly: false)), .peakBlobs),
            (Request(activePeakHold: .init(enabled: true, holdMs: 99, fallDbPerSec: 1)), .activePeakHold),
            (Request(activePeakHold: .init(enabled: false, holdMs: 100, fallDbPerSec: 0.05)), .activePeakHold),
            (Request(activePeakHold: .init(enabled: true, holdMs: 100, fallDbPerSec: 121)), .activePeakHold),
            (Request(noiseFloor: .init(enabled: true, shiftDb: 12.5)), .noiseFloor),
            (Request(noiseFloor: .init(enabled: false, shiftDb: .infinity)), .noiseFloor),
            (Request(waterfallLevels: .init(mode: .agc, lowDbm: -401, highDbm: 0, offsetDb: 0)), .waterfallLevels),
            (Request(waterfallLevels: .init(mode: .agc, lowDbm: 0, highDbm: 101, offsetDb: 0)), .waterfallLevels),
            (Request(waterfallLevels: .init(mode: .agc, lowDbm: 0, highDbm: 1, offsetDb: 61)), .waterfallLevels),
            (Request(calibrationOffsetDb: 30.5), .calibrationOffsetDb),
            (Request(calibrationOffsetDb: .nan), .calibrationOffsetDb),
            (Request(averageTimeMs: 9), .averageTimeMs), (Request(averageTimeMs: 10_000), .averageTimeMs),
            (Request(waterfallAverageTimeMs: 9), .waterfallAverageTimeMs),
            (Request(waterfallAverageTimeMs: 10_000), .waterfallAverageTimeMs),
        ]
        for (request, invalid) in bad {
            #expect(throws: invalid, "\(request)") { try request.validate() }
        }
    }

    @Test func theFieldsAreExactlyTheOnesAskedForInTheSurfacesShapes() throws {
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let row = try #require(((object["mediaControl"] as? [String: Any])?["guiToCore"] as? [String: Any])?["subscribe"]
            as? [String: Any])
        let conditional = try #require(row["conditional"] as? [String: [String]])
        let nested = try #require(row["nested"] as? [String: [[String]]])
        let extrasKeys = Set(conditional.filter { $0.value == ["displayExtrasVersion"] }.keys)
        let fields = Self.everything.subscribeFields
        #expect(Set(fields.keys) == extrasKeys)
        for (key, value) in fields {
            if case .object(let members) = value {
                #expect(nested[key]?.contains(members.keys.sorted()) == true, "\(key)")
            } else {
                #expect(nested[key] == nil, "\(key)")
            }
        }
        if case .object(let levels)? = fields["waterfallLevels"] {
            #expect(levels["mode"] == .string("clarity"))
        } else {
            Issue.record("waterfallLevels is an object")
        }
        #expect(DisplayExtrasRequest().subscribeFields.isEmpty)
        #expect(Set(DisplayExtrasRequest(normalize: true, averageTimeMs: 50).subscribeFields.keys)
                == ["normalize", "averageTimeMs"])
    }
}

/// Builds NSDX v1 datagrams from the document's layout (section 3.2), for
/// the cases the link's vectors do not hold.
private struct DatagramBuilder {
    static let headerBytes = 20

    var endpointId: UInt32 = 3
    var generation: UInt32 = 9
    var sequence: UInt32 = 1
    var blobs: [(UInt16, Float)]?
    /// Quantised peak hold samples.
    var hold: [UInt8]?
    var residual = false
    var floor: Float?
    var levels: (Float, Float)?
    /// The noise floor's state byte (section 0x10).
    var state: UInt8?

    init(blobs: [(UInt16, Float)]? = nil, hold: [UInt8]? = nil, residual: Bool = false, floor: Float? = nil,
         levels: (Float, Float)? = nil, state: UInt8? = nil) {
        self.state = state
        self.blobs = blobs
        self.hold = hold
        self.residual = residual
        self.floor = floor
        self.levels = levels
    }

    func bytes() -> Data {
        var sections: UInt8 = 0
        sections |= blobs == nil ? 0 : 0x01
        sections |= hold == nil ? 0 : 0x02
        sections |= floor == nil ? 0 : 0x04
        sections |= levels == nil ? 0 : 0x08
        sections |= state == nil ? 0 : 0x10
        var out = Data("NSDX".utf8)
        out.append(1)
        out.append(sections)
        Self.put(UInt32(Self.headerBytes), 2, into: &out)
        Self.put(endpointId, 4, into: &out)
        Self.put(generation, 4, into: &out)
        Self.put(sequence, 4, into: &out)
        if let blobs {
            out.append(UInt8(blobs.count))
            for (sample, dbm) in blobs {
                Self.put(UInt32(sample), 2, into: &out)
                Self.put(dbm.bitPattern, 4, into: &out)
            }
        }
        if let hold {
            // One 128-sample block: absolute, or a residual block of width 8.
            out.append(3)
            Self.put(1, 2, into: &out)
            out.append(contentsOf: [residual ? 0 : 1, 8, UInt8(hold.count)])
            Self.put(UInt32(hold.count), 2, into: &out)
            out.append(contentsOf: hold)
        }
        if let floor {
            Self.put(floor.bitPattern, 4, into: &out)
        }
        if let levels {
            Self.put(levels.0.bitPattern, 4, into: &out)
            Self.put(levels.1.bitPattern, 4, into: &out)
        }
        if let state {
            out.append(state)
        }
        return out
    }

    private static func put(_ value: UInt32, _ bytes: Int, into out: inout Data) {
        for index in (0..<bytes).reversed() {
            out.append(UInt8((value >> (8 * UInt32(index))) & 0xFF))
        }
    }
}
