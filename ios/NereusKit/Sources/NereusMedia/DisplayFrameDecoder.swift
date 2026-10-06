// NereusSDR for iOS: decodes one endpoint's NSDC v1 display frames from the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Decodes one display endpoint's NSDC v1 datagrams, written from
/// docs/architecture/2026-09-20-display-codec-v1.md. Keep one decoder per
/// endpoint: a delta is rebuilt from this decoder's previous reconstruction.
///
/// A datagram that is refused or needs a keyframe never changes the history
/// (the last accepted context, sequence and reconstructed rows).
public final class DisplayFrameDecoder {
    /// The fixed header, in bytes.
    static let headerBytes = 42
    /// The most samples one row may hold.
    static let maximumRowSamples = 4096
    /// The codec's bound on one frame, in bytes (16 KiB).
    static let maximumFrameBytes = 16 * 1024

    private static let magic: UInt32 = 0x4E53_4443  // "NSDC"
    private static let version: UInt8 = 1
    private static let keyframeFlag: UInt8 = 0x01
    private static let waterfallAdvanceFlag: UInt8 = 0x02
    private static let wideFlag: UInt8 = 0x04
    private static let blockSizes = [16, 32, 64, 128]

    /// The accepted context and reconstruction a delta builds on.
    private struct History {
        let endpointId: UInt32
        let contextGeneration: UInt32
        let minDbm: Float
        let maxDbm: Float
        let sequence: UInt32
        let trace: [UInt8]
        let waterfall: [UInt8]
        let wide: [UInt8]
    }

    private var history: History?
    /// A delta was lost in the accepted generation; only a keyframe helps.
    private var sequenceGapPending = false
    /// A delta of this newer generation arrived; only its keyframe helps.
    private var pendingGeneration: UInt32?

    public init() {}

    /// Whether the decoder cannot use a delta until the Core sends a keyframe:
    /// nothing has been accepted yet, a frame was lost, or the Core moved to a
    /// newer context.
    public var needsKeyframe: Bool {
        history == nil || sequenceGapPending || pendingGeneration != nil
    }

    public func decode(_ datagram: Data) -> DisplayDecodeResult {
        let packet: Packet
        switch Self.parse(Array(datagram)) {
        case .failure(let failure):
            return Self.refusal(.rejected, failure.reason)
        case .success(let parsed):
            packet = parsed
        }

        guard let previous = history else {
            if !packet.keyframe {
                return Self.refusal(.needKeyframe, .noHistory)
            }
            return accept(packet, over: nil)
        }
        if packet.endpointId != previous.endpointId {
            return Self.refusal(.rejected, .contextMismatch)
        }
        if packet.contextGeneration != previous.contextGeneration {
            if !Self.isNewer(packet.contextGeneration, than: previous.contextGeneration) {
                return Self.refusal(.rejected, .oldContext)
            }
            if !packet.keyframe {
                pendingGeneration = Self.newest(pendingGeneration, packet.contextGeneration)
                return Self.refusal(.needKeyframe, .contextMismatch)
            }
            return accept(packet, over: nil)
        }
        // The same generation: its interval and row lengths cannot change.
        if packet.minDbm != previous.minDbm || packet.maxDbm != previous.maxDbm
            || packet.trace.count != previous.trace.count
            || packet.waterfall.count != previous.waterfall.count
            || packet.wide.count != previous.wide.count {
            return Self.refusal(.rejected, .contextMismatch)
        }
        if !Self.isNewer(packet.sequence, than: previous.sequence) {
            return Self.refusal(.rejected, .staleSequence)
        }
        if packet.keyframe {
            return accept(packet, over: nil)
        }
        if sequenceGapPending || packet.sequence != previous.sequence &+ 1 {
            sequenceGapPending = true
            return Self.refusal(.needKeyframe, .sequenceGap)
        }
        return accept(packet, over: previous)
    }

    /// Reconstructs `packet` (a delta over `previous`, or a keyframe when
    /// `previous` is nil) and makes it the history.
    private func accept(_ packet: Packet, over previous: History?) -> DisplayDecodeResult {
        guard let trace = Self.reconstruct(packet.trace, over: previous?.trace),
              let waterfall = Self.reconstruct(packet.waterfall, over: previous?.waterfall),
              let wide = Self.reconstruct(packet.wide, over: previous?.wide) else {
            return Self.refusal(.rejected, .malformed)
        }
        history = History(endpointId: packet.endpointId, contextGeneration: packet.contextGeneration,
                          minDbm: packet.minDbm, maxDbm: packet.maxDbm, sequence: packet.sequence,
                          trace: trace, waterfall: waterfall, wide: wide)
        if packet.keyframe {
            sequenceGapPending = false
            if let pending = pendingGeneration,
               !Self.isNewer(pending, than: packet.contextGeneration) {
                pendingGeneration = nil
            }
        }
        let minimum = Double(packet.minDbm)
        let maximum = Double(packet.maxDbm)
        let dbm = { (row: [UInt8]) -> [Float] in
            row.map { Self.dbm($0, minimum: minimum, maximum: maximum) }
        }
        let frame = DisplayFrame(
            endpointId: packet.endpointId, contextGeneration: packet.contextGeneration,
            encoderSequence: packet.sequence, producerTimestamp: packet.timestamp,
            isKeyframe: packet.keyframe, waterfallAdvance: packet.waterfallAdvance,
            minDbm: packet.minDbm, maxDbm: packet.maxDbm,
            traceDbm: dbm(trace), waterfallDbm: dbm(waterfall), wideDbm: dbm(wide))
        return DisplayDecodeResult(disposition: .accepted, reason: .none, frame: frame)
    }

    /// Decodes one plane at `offset` that must be absolute throughout (a
    /// keyframe plane) and exactly `length` samples long, dequantised on
    /// `minDbm...maxDbm`, and moves `offset` past it. Nil, with `offset`
    /// unchanged, for anything else: a residual block, another length, a bad
    /// interval, or bytes that run out. The NSDX peak hold section is one
    /// (the station's `decodeDisplayCodecAbsolutePlane`).
    static func decodeAbsolutePlane(_ bytes: [UInt8], offset: inout Int, length: Int,
                                    minDbm: Float, maxDbm: Float) -> [Float]? {
        guard offset >= 0, offset <= bytes.count, (1...maximumRowSamples).contains(length),
              minDbm.isFinite, maxDbm.isFinite, maxDbm > minDbm else {
            return nil
        }
        var reader = ByteReader(bytes, at: offset)
        guard let samples = try? readPlane(&reader, length: length, keyframe: true),
              let row = reconstruct(samples, over: nil) else {
            return nil
        }
        offset = reader.position
        let minimum = Double(minDbm)
        let maximum = Double(maxDbm)
        return row.map { dbm($0, minimum: minimum, maximum: maximum) }
    }

    /// A quantised value's level in dBm.
    static func dbm(_ q: UInt8, minimum: Double, maximum: Double) -> Float {
        Float(min(max(minimum + Double(q) * (maximum - minimum) / 255.0, minimum), maximum))
    }

    private static func refusal(_ disposition: DisplayDecodeDisposition,
                                _ reason: DisplayDecodeReason) -> DisplayDecodeResult {
        DisplayDecodeResult(disposition: disposition, reason: reason, frame: nil)
    }

    /// The unsigned half-range rule: `candidate` is newer than `accepted`
    /// when it lies less than half the 32-bit range after it.
    static func isNewer(_ candidate: UInt32, than accepted: UInt32) -> Bool {
        let difference = candidate &- accepted
        return difference != 0 && difference < 0x8000_0000
    }

    private static func newest(_ current: UInt32?, _ candidate: UInt32) -> UInt32 {
        guard let current, !isNewer(candidate, than: current) else {
            return candidate
        }
        return current
    }

    // MARK: Parsing

    /// One quantised sample as it travels: an absolute value or a residual
    /// against the previous reconstructed plane.
    private enum Sample {
        case absolute(UInt8)
        case residual(Int)
    }

    /// A datagram whose layout is valid, before any history is applied.
    private struct Packet {
        let keyframe: Bool
        let waterfallAdvance: Bool
        let endpointId: UInt32
        let contextGeneration: UInt32
        let sequence: UInt32
        let timestamp: UInt64
        let minDbm: Float
        let maxDbm: Float
        let trace: [Sample]
        let waterfall: [Sample]
        let wide: [Sample]
    }

    private static func parse(_ bytes: [UInt8]) -> Result<Packet, ParseFailure> {
        if bytes.count > maximumFrameBytes {
            return .failure(ParseFailure(.oversized))
        }
        var reader = ByteReader(bytes)
        do {
            guard try reader.u32() == magic else {
                return .failure(ParseFailure(.badMagic))
            }
            guard try reader.u8() == version else {
                return .failure(ParseFailure(.unsupportedVersion))
            }
            let flags = try reader.u8()
            let size = try reader.u16()
            if flags & ~(keyframeFlag | waterfallAdvanceFlag | wideFlag) != 0 {
                return .failure(ParseFailure(.unknownFlags))
            }
            if Int(size) != headerBytes {
                return .failure(ParseFailure(.malformed))
            }
            let endpointId = try reader.u32()
            let generation = try reader.u32()
            let sequence = try reader.u32()
            let timestamp = try reader.u64()
            let minDbm = Float(bitPattern: try reader.u32())
            let maxDbm = Float(bitPattern: try reader.u32())
            let traceLength = Int(try reader.u16())
            let waterfallLength = Int(try reader.u16())
            let wideLength = Int(try reader.u16())

            let keyframe = flags & keyframeFlag != 0
            let hasWide = flags & wideFlag != 0
            let rowsValid = (1...maximumRowSamples).contains(traceLength)
                && (1...maximumRowSamples).contains(waterfallLength)
                && wideLength <= maximumRowSamples
                && hasWide == (wideLength != 0)
            guard minDbm.isFinite, maxDbm.isFinite, maxDbm > minDbm, rowsValid else {
                return .failure(ParseFailure(.malformed))
            }

            // The planes: any fault from here on is a malformed frame.
            do {
                let trace = try readPlane(&reader, length: traceLength, keyframe: keyframe)
                let waterfall = try readPlane(&reader, length: waterfallLength, keyframe: keyframe)
                let wide = hasWide ? try readPlane(&reader, length: wideLength, keyframe: keyframe) : []
                guard reader.atEnd else {
                    return .failure(ParseFailure(.malformed))
                }
                return .success(Packet(
                    keyframe: keyframe, waterfallAdvance: flags & waterfallAdvanceFlag != 0,
                    endpointId: endpointId, contextGeneration: generation, sequence: sequence,
                    timestamp: timestamp, minDbm: minDbm, maxDbm: maxDbm,
                    trace: trace, waterfall: waterfall, wide: wide))
            } catch {
                return .failure(ParseFailure(.malformed))
            }
        } catch let failure as ParseFailure {
            return .failure(failure)
        } catch {
            return .failure(ParseFailure(.malformed))
        }
    }

    /// Reads one plane of exactly `length` samples.
    private static func readPlane(_ reader: inout ByteReader, length: Int,
                                  keyframe: Bool) throws -> [Sample] {
        let code = Int(try reader.u8())
        let blockCount = Int(try reader.u16())
        guard code < blockSizes.count else {
            throw ParseFailure(.malformed)
        }
        let blockSize = blockSizes[code]
        guard blockCount == (length + blockSize - 1) / blockSize else {
            throw ParseFailure(.malformed)
        }
        var samples: [Sample] = []
        samples.reserveCapacity(length)
        for _ in 0..<blockCount {
            let mode = try reader.u8()
            let width = Int(try reader.u8())
            let count = Int(try reader.u8())
            let payloadBytes = Int(try reader.u16())
            guard count == min(blockSize, length - samples.count) else {
                throw ParseFailure(.malformed)
            }
            let payload = try reader.take(payloadBytes)
            switch mode {
            case 1:
                guard width == 8, payloadBytes == count else {
                    throw ParseFailure(.malformed)
                }
                samples.append(contentsOf: payload.map { Sample.absolute($0) })
            case 0:
                guard !keyframe, (1...8).contains(width),
                      payloadBytes == (count * width + 7) / 8 else {
                    throw ParseFailure(.malformed)
                }
                let bias = 1 << (width - 1)
                for index in 0..<count {
                    samples.append(.residual(unpack(payload, index: index, width: width) - bias))
                }
            default:
                throw ParseFailure(.malformed)
            }
        }
        return samples
    }

    /// The `index`th `width`-bit value of `payload`, packed MSB first.
    private static func unpack(_ payload: [UInt8], index: Int, width: Int) -> Int {
        var value = 0
        var bit = index * width
        for _ in 0..<width {
            let byte = Int(payload[bit / 8])
            value = (value << 1) | ((byte >> (7 - bit % 8)) & 1)
            bit += 1
        }
        return value
    }

    /// The quantised plane `samples` describes, or nil when a residual has
    /// nothing to build on or leaves 0...255.
    private static func reconstruct(_ samples: [Sample], over previous: [UInt8]?) -> [UInt8]? {
        var row: [UInt8] = []
        row.reserveCapacity(samples.count)
        for (index, sample) in samples.enumerated() {
            switch sample {
            case .absolute(let q):
                row.append(q)
            case .residual(let residual):
                guard let previous, index < previous.count else {
                    return nil
                }
                let value = Int(previous[index]) + residual
                guard (0...255).contains(value) else {
                    return nil
                }
                row.append(UInt8(value))
            }
        }
        return row
    }

    private struct ParseFailure: Error {
        let reason: DisplayDecodeReason
        init(_ reason: DisplayDecodeReason) {
            self.reason = reason
        }
    }

    /// Big-endian reads that throw `truncated` past the end.
    private struct ByteReader {
        private let storage: [UInt8]
        private var offset = 0

        init(_ storage: [UInt8], at start: Int = 0) {
            self.storage = storage
            offset = start
        }

        var atEnd: Bool { offset == storage.count }
        var position: Int { offset }

        mutating func take(_ count: Int) throws -> [UInt8] {
            guard count <= storage.count - offset else {
                throw ParseFailure(.truncated)
            }
            defer { offset += count }
            return Array(storage[offset..<(offset + count)])
        }

        mutating func u8() throws -> UInt8 {
            try take(1)[0]
        }

        mutating func u16() throws -> UInt16 {
            try take(2).reduce(0) { $0 << 8 | UInt16($1) }
        }

        mutating func u32() throws -> UInt32 {
            try take(4).reduce(0) { $0 << 8 | UInt32($1) }
        }

        mutating func u64() throws -> UInt64 {
            try take(8).reduce(0) { $0 << 8 | UInt64($1) }
        }
    }
}
