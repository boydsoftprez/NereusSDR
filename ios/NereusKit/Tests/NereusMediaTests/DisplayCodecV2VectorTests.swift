// NereusSDR for iOS: the NSDC v2 decoder against the golden vectors the desktop decoder reads
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-22: NSDC v2 golden vector tests (planes, frames, sequences, truncation).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import LinkTestSupport
import Testing
@testable import NereusMedia

/// Display codec v2 document sections 4, 8 and 9. The vectors are
/// `tests/data/display_codec_v2/vectors.json`, written by the desktop's
/// encoder and read by its decoder test too, so both decoders are held to one
/// encoder.
@Suite struct DisplayCodecV2VectorTests {
    // MARK: The vector file

    struct Vectors: Decodable {
        let schema: Int
        let format: String
        let planes: [PlaneCase]
        let frames: [FrameCase]
        let sequences: [SequenceCase]
    }

    struct PlaneCase: Decodable {
        let name: String
        let blockSize: Int
        let plane: String
        let samples: [Int]
    }

    struct ExpectedFrame: Decodable {
        let endpointId: UInt32
        let contextGeneration: UInt32
        let encoderSequence: UInt32
        let producerTimestamp: String
        let minDbm: Double
        let maxDbm: Double
        let waterfallAdvance: Bool
        let traceCarried: Bool
        let traceSamples: Int
        let waterfallSamples: Int
        let wideSamples: Int
        let trace: [Int]
        let waterfall: [Int]
        let wide: [Int]
    }

    struct FrameCase: Decodable {
        let name: String
        let admittedFrameBytes: Int
        let messages: [String]
        let frame: ExpectedFrame
    }

    struct ExpectedResult: Decodable {
        let disposition: String
        let reason: String
        let frames: [ExpectedFrame]
    }

    struct ExpectedCounters: Decodable {
        let framesLost: UInt64
        let partsLost: UInt64
        let staleMessages: UInt64
        let duplicateParts: UInt64
        let malformed: UInt64
    }

    struct SequenceCase: Decodable {
        let name: String
        let messages: [String]
        let results: [ExpectedResult]
        let counters: ExpectedCounters
    }

    static func vectors() throws -> Vectors {
        // tests/data/link/v1 is two folders below the repository's tests/data.
        let file = LinkFixtureLoader.root.deletingLastPathComponent().deletingLastPathComponent()
            .appendingPathComponent("display_codec_v2/vectors.json")
        let vectors = try JSONDecoder().decode(Vectors.self, from: Data(contentsOf: file))
        #expect(vectors.schema == 1)
        #expect(vectors.format == "nsdc2-vectors")
        return vectors
    }

    static func bytes(_ hex: String) throws -> [UInt8] {
        let digits = Array(hex.utf8)
        guard digits.count % 2 == 0 else {
            throw LinkFixtureLoader.Malformed(description: "odd hex length")
        }
        return try stride(from: 0, to: digits.count, by: 2).map { index in
            guard let value = UInt8(String(decoding: digits[index..<(index + 2)], as: UTF8.self), radix: 16) else {
                throw LinkFixtureLoader.Malformed(description: "bad hex at \(index)")
            }
            return value
        }
    }

    // MARK: Planes

    @Test func everyPlaneDecodesToItsSamples() throws {
        let planes = try Self.vectors().planes
        #expect(!planes.isEmpty)
        for item in planes {
            let bytes = try Self.bytes(item.plane)
            var offset = 0
            let samples = DisplayFrameDecoder.decodeVersionTwoPlane(bytes, offset: &offset,
                                                                    length: item.samples.count)
            #expect(samples == item.samples.map { UInt8($0) }, "\(item.name)")
            #expect(offset == bytes.count, "\(item.name): the plane ends where its bytes end")
            let sizes = [16, 32, 64]
            #expect(bytes.count > 1 && sizes.indices.contains(Int(bytes[1])) && sizes[Int(bytes[1])] == item.blockSize,
                    "\(item.name): block size code")
        }
    }

    @Test func aPlaneOfAnotherLengthOrCutShortIsRefused() throws {
        for item in try Self.vectors().planes {
            let bytes = try Self.bytes(item.plane)
            for length in [item.samples.count - 1, item.samples.count + 1] where length >= 1 {
                var offset = 0
                #expect(DisplayFrameDecoder.decodeVersionTwoPlane(bytes, offset: &offset, length: length) == nil,
                        "\(item.name) read as \(length) samples")
                #expect(offset == 0)
            }
            for cut in 0..<bytes.count {
                var offset = 0
                let prefix = Array(bytes.prefix(cut))
                #expect(DisplayFrameDecoder.decodeVersionTwoPlane(prefix, offset: &offset,
                                                                  length: item.samples.count) == nil,
                        "\(item.name) cut at \(cut)")
                #expect(offset == 0)
            }
        }
    }

    // MARK: Frames

    @Test func everyFrameDecodesAloneOrPaired() throws {
        let frames = try Self.vectors().frames
        #expect(!frames.isEmpty)
        for item in frames {
            let messages = try item.messages.map { Data(try Self.bytes($0)) }
            let orders = messages.count == 2 ? [messages, messages.reversed()] : [messages]
            #expect(messages.count == 1 || messages.count == 2, "\(item.name)")
            for (orderIndex, order) in orders.enumerated() {
                let context = "\(item.name), order \(orderIndex)"
                let decoder = DisplayFrameDecoder()
                for earlier in order.dropLast() {
                    let held = decoder.decode(earlier)
                    #expect(held == DisplayDecodeResult(disposition: .held, reason: .none, frames: []), "\(context)")
                }
                let result = decoder.decode(try #require(order.last))
                #expect(result.disposition == .accepted && result.reason == .none, "\(context)")
                #expect(result.frames.count == 1, "\(context)")
                #expect(result.frame == result.frames.first, "\(context)")
                if let frame = result.frame, let failure = Self.mismatch(frame, item.frame) {
                    Issue.record("\(context): \(failure)")
                }
                #expect(decoder.counters == DisplayDecodeCounters(), "\(context)")
            }
        }
    }

    // MARK: Sequences

    @Test func everySequenceGivesItsResultsAndCounters() throws {
        let sequences = try Self.vectors().sequences
        #expect(!sequences.isEmpty)
        for item in sequences {
            #expect(item.messages.count == item.results.count, "\(item.name)")
            let decoder = DisplayFrameDecoder()
            for (index, (hex, expected)) in zip(item.messages, item.results).enumerated() {
                let context = "\(item.name), message \(index)"
                let result = decoder.decode(Data(try Self.bytes(hex)))
                if let failure = Self.mismatch(result, expected) {
                    Issue.record("\(context): \(failure)")
                }
            }
            let wanted = item.counters
            #expect(decoder.counters == DisplayDecodeCounters(
                framesLost: wanted.framesLost, partsLost: wanted.partsLost, staleMessages: wanted.staleMessages,
                duplicateParts: wanted.duplicateParts, malformed: wanted.malformed), "\(item.name)")
        }
    }

    // MARK: Truncation

    /// No message cut at any byte traps; a valid message cut short never
    /// decodes. (A malformed message with a byte after its last plane is a
    /// valid one less that byte, so only valid messages are held to refusal.)
    @Test func noMessageCutAtAnyByteTrapsOrDecodes() throws {
        let vectors = try Self.vectors()
        let messages = vectors.frames.flatMap(\.messages) + vectors.sequences.flatMap(\.messages)
        var validMessages = 0
        for hex in messages {
            let bytes = try Self.bytes(hex)
            let valid = DisplayFrameDecoder().decode(Data(bytes)).disposition != .rejected
            validMessages += valid ? 1 : 0
            for cut in 0..<bytes.count {
                let result = DisplayFrameDecoder().decode(Data(bytes.prefix(cut)))
                if valid {
                    #expect(result.disposition == .rejected && result.frames.isEmpty,
                            "\(hex.prefix(24)) cut at \(cut)")
                }
            }
        }
        #expect(validMessages > 0)
    }

    // MARK: State the vectors cannot hold

    @Test func resetDiscardsTheHeldPartAndTheFillsButKeepsTheCounters() throws {
        let sequences = try Self.vectors().sequences
        let item = try #require(sequences.first { $0.name == "lost-row-part-fresh" })
        let held = Data(try Self.bytes(item.messages[0]))
        let whole = Data(try Self.bytes(item.messages[1]))

        let decoder = DisplayFrameDecoder()
        #expect(decoder.decode(Data([0x00])).reason == .truncated)
        #expect(decoder.decode(held).disposition == .held)
        decoder.reset()
        #expect(decoder.counters.malformed == 1, "the counters survive reset")
        // The held part was discarded, not released: the whole frame comes alone.
        let result = decoder.decode(whole)
        #expect(result.disposition == .accepted && result.frames.count == 1)
        #expect(result.frame?.encoderSequence == 4)
        #expect(decoder.counters == DisplayDecodeCounters(malformed: 1))
    }

    @Test func versionTwoNeverAsksForAKeyframeOrMarksOne() throws {
        for item in try Self.vectors().sequences {
            let decoder = DisplayFrameDecoder()
            for hex in item.messages {
                let result = decoder.decode(Data(try Self.bytes(hex)))
                #expect(result.disposition != .needKeyframe, "\(item.name)")
                #expect(result.frames.allSatisfy { !$0.isKeyframe }, "\(item.name)")
                #expect(result.frame == result.frames.first, "\(item.name)")
            }
        }
    }

    // MARK: Comparison

    static func mismatch(_ result: DisplayDecodeResult, _ expected: ExpectedResult) -> String? {
        guard let disposition = DisplayDecodeDisposition(rawValue: expected.disposition),
              let reason = DisplayDecodeReason(rawValue: expected.reason) else {
            return "unknown disposition \(expected.disposition) or reason \(expected.reason)"
        }
        if result.disposition != disposition {
            return "disposition: expected \(disposition), got \(result.disposition) (\(result.reason))"
        }
        if result.reason != reason {
            return "reason: expected \(reason), got \(result.reason)"
        }
        if result.frames.count != expected.frames.count {
            return "frames: expected \(expected.frames.count), got \(result.frames.count)"
        }
        if result.frame != result.frames.first {
            return "frame is not frames.first"
        }
        for (index, (frame, wanted)) in zip(result.frames, expected.frames).enumerated() {
            if let failure = mismatch(frame, wanted) {
                return "frame \(index): \(failure)"
            }
        }
        return nil
    }

    /// Why `frame` is not `expected`, or nil; planes compare exactly against
    /// the dequantized expected samples.
    static func mismatch(_ frame: DisplayFrame, _ expected: ExpectedFrame) -> String? {
        let minimum = Float(expected.minDbm)
        let maximum = Float(expected.maxDbm)
        let integers: [(String, UInt64, UInt64?)] = [
            ("endpointId", UInt64(frame.endpointId), UInt64(expected.endpointId)),
            ("contextGeneration", UInt64(frame.contextGeneration), UInt64(expected.contextGeneration)),
            ("encoderSequence", UInt64(frame.encoderSequence), UInt64(expected.encoderSequence)),
            ("producerTimestamp", frame.producerTimestamp, UInt64(expected.producerTimestamp)),
        ]
        for (field, got, wanted) in integers where got != wanted {
            return "\(field): expected \(String(describing: wanted)), got \(got)"
        }
        if frame.minDbm != minimum || frame.maxDbm != maximum {
            return "interval: expected \(minimum)...\(maximum), got \(frame.minDbm)...\(frame.maxDbm)"
        }
        if frame.isKeyframe {
            return "a v2 frame is never a keyframe"
        }
        if frame.waterfallAdvance != expected.waterfallAdvance {
            return "waterfallAdvance: expected \(expected.waterfallAdvance)"
        }
        if frame.traceCarried != expected.traceCarried {
            return "traceCarried: expected \(expected.traceCarried)"
        }
        let rows: [(String, [Float], [Int], Int)] = [
            ("trace", frame.traceDbm, expected.trace, expected.traceSamples),
            ("waterfall", frame.waterfallDbm, expected.waterfall, expected.waterfallSamples),
            ("wide", frame.wideDbm, expected.wide, expected.wideSamples),
        ]
        for (field, got, samples, count) in rows {
            if samples.count != count {
                return "\(field): the vector lists \(samples.count) samples of \(count)"
            }
            let wanted = samples.map {
                DisplayFrameDecoder.dbm(UInt8($0), minimum: Double(minimum), maximum: Double(maximum))
            }
            if got != wanted {
                let index = zip(got, wanted).enumerated().first { $0.element.0 != $0.element.1 }?.offset
                    ?? min(got.count, wanted.count)
                return "\(field): \(got.count) samples, first difference at \(index)"
            }
        }
        return nil
    }
}
