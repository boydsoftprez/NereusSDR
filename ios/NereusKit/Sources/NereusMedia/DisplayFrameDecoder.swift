// NereusSDR for iOS: decodes one endpoint's NSDC v1 and v2 display frames from the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-22: NSDC v2 path (header and flag rules, the Rice plane, pairing of
//   split parts, counters, reset), chosen by the version byte.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-23: The v2 header's interval compares by value, as the desktop's
//   decoder does, so -0.0 equals +0.0. J.J. Boyd (KG4VCF), AI-assisted via
//   Claude Code.

import Foundation

/// Decodes one display endpoint's NSDC datagrams. Version 1 is written from
/// docs/architecture/2026-09-20-display-codec-v1.md, version 2 from
/// docs/architecture/2026-10-22-display-codec-v2-intra-rows.md; the version
/// byte chooses the path. Keep one decoder per endpoint: a v1 delta is
/// rebuilt from this decoder's previous reconstruction.
///
/// A v1 datagram that is refused or needs a keyframe never changes the
/// history (the last accepted context, sequence and reconstructed rows). A v2
/// message depends on no history: the decoder keeps only its context, the
/// last released sequence, at most one held part of a split frame, and the
/// last decoded planes used to fill a released frame's absent planes.
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

    // NSDC v2 (v2 document section 4).
    private static let versionTwoByte: UInt8 = 2
    private static let v2TraceFlag: UInt8 = 0x08
    private static let v2WaterfallFlag: UInt8 = 0x10
    private static let v2SplitFlag: UInt8 = 0x20
    /// Every flag bit version 2 defines; any other bit is `unknownFlags`.
    private static let v2KnownFlags: UInt8 = 0x3E
    /// The only valid flag combinations (section 4.1).
    private static let v2ValidFlags: Set<UInt8> = [0x08, 0x1A, 0x1E, 0x2A, 0x32, 0x36]
    /// Plane coding 2: previous-bin Rice.
    private static let v2PlaneCoding: UInt8 = 2
    private static let v2BlockSizes = [16, 32, 64]
    /// A block's `k` of 7 means its samples follow raw, 8 bits each.
    private static let v2RawEscape = 7

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
    /// The NSDC v2 context, its last released sequence, held part and fills.
    private var versionTwo: VersionTwoContext?

    /// What the decoder has counted. NSDC v2 messages and refusals made
    /// before the version is known (oversized, bad magic, unsupported
    /// version, truncated before the version byte) move it; the v1 path does
    /// not. It survives `reset()`.
    public private(set) var counters = DisplayDecodeCounters()

    public init() {}

    /// Forgets every context: the v1 history and the v2 context, its held
    /// part (discarded, not released) and its fill planes. The counters stay.
    public func reset() {
        history = nil
        sequenceGapPending = false
        pendingGeneration = nil
        versionTwo = nil
    }

    /// Whether the decoder cannot use a delta until the Core sends a keyframe:
    /// nothing has been accepted yet, a frame was lost, or the Core moved to a
    /// newer context.
    public var needsKeyframe: Bool {
        history == nil || sequenceGapPending || pendingGeneration != nil
    }

    public func decode(_ datagram: Data) -> DisplayDecodeResult {
        let bytes = Array(datagram)
        if let reason = Self.versionRefusal(bytes) {
            counters.malformed += 1
            return Self.refusal(.rejected, reason)
        }
        if bytes[4] == Self.versionTwoByte {
            return decodeVersionTwo(bytes)
        }
        return decodeVersionOne(bytes)
    }

    /// The checks both versions share, in v1's order, before the version
    /// byte chooses a path; nil when the datagram is a version 1 or 2 NSDC
    /// datagram within the bound.
    private static func versionRefusal(_ bytes: [UInt8]) -> DisplayDecodeReason? {
        if bytes.count > maximumFrameBytes {
            return .oversized
        }
        if bytes.count < 4 {
            return .truncated
        }
        let found = bytes[0..<4].reduce(UInt32(0)) { $0 << 8 | UInt32($1) }
        if found != magic {
            return .badMagic
        }
        if bytes.count < 5 {
            return .truncated
        }
        if bytes[4] != version && bytes[4] != versionTwoByte {
            return .unsupportedVersion
        }
        return nil
    }

    private func decodeVersionOne(_ bytes: [UInt8]) -> DisplayDecodeResult {
        let packet: Packet
        switch Self.parse(bytes) {
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

    // MARK: NSDC v2

    /// Which message of a frame this is (section 4.1).
    private enum VersionTwoKind: Equatable {
        /// A whole frame: `0x08`, `0x1A` or `0x1E`.
        case whole
        /// The trace part of a split frame, `0x2A`.
        case tracePart
        /// The row part of a split frame, `0x32` or `0x36`.
        case rowPart
    }

    /// Every header field but the flags; two parts agree when these are equal.
    /// The interval compares by value, never by bit pattern, so -0.0 equals
    /// +0.0 (the desktop's decoder has the same rule). A NaN end never gets
    /// here: the parse refuses a non-finite interval as malformed.
    private struct VersionTwoHeader: Equatable {
        let endpointId: UInt32
        let contextGeneration: UInt32
        let sequence: UInt32
        let timestamp: UInt64
        let minDbm: Float
        let maxDbm: Float
        let traceLength: Int
        let waterfallLength: Int
        let wideLength: Int

        /// The same context: the interval and the three row lengths.
        func sameShape(as other: VersionTwoHeader) -> Bool {
            minDbm == other.minDbm && maxDbm == other.maxDbm
                && traceLength == other.traceLength && waterfallLength == other.waterfallLength
                && wideLength == other.wideLength
        }
    }

    /// A version 2 message whose structure is valid; its planes are the
    /// quantized samples it carries (nil when absent).
    private struct VersionTwoMessage {
        let header: VersionTwoHeader
        let kind: VersionTwoKind
        let trace: [UInt8]?
        let waterfall: [UInt8]?
        let wide: [UInt8]?
    }

    /// The accepted v2 context (section 9.1).
    private struct VersionTwoContext {
        let header: VersionTwoHeader
        /// `L`, the last released sequence; nil before the first release.
        var lastReleased: UInt32?
        /// `H`, at most one part of a split frame waiting for its partner.
        var held: VersionTwoMessage?
        /// The last decoded planes, only to fill a released frame's absent ones.
        var fillTrace: [UInt8]?
        var fillWaterfall: [UInt8]?
        var fillWide: [UInt8]?

        init(header: VersionTwoHeader) {
            self.header = header
        }
    }

    private func decodeVersionTwo(_ bytes: [UInt8]) -> DisplayDecodeResult {
        let message: VersionTwoMessage
        switch Self.parseVersionTwo(bytes) {
        case .failure(let failure):
            counters.malformed += 1
            return Self.refusal(.rejected, failure.reason)
        case .success(let parsed):
            message = parsed
        }

        var released: [DisplayFrame] = []
        var context: VersionTwoContext
        if let current = versionTwo {
            if message.header.endpointId != current.header.endpointId {
                return Self.refusal(.rejected, .contextMismatch)
            }
            if message.header.contextGeneration != current.header.contextGeneration {
                if !Self.isNewer(message.header.contextGeneration, than: current.header.contextGeneration) {
                    return Self.refusal(.rejected, .oldContext)
                }
                // A newer generation: the held part goes out as a partial
                // frame of the old context, then the new context starts
                // with no `L` and no fills.
                var old = current
                if let held = old.held {
                    old.held = nil
                    released.append(release(held, partial: true, in: &old))
                }
                context = VersionTwoContext(header: message.header)
            } else {
                if !message.header.sameShape(as: current.header) {
                    return Self.refusal(.rejected, .contextMismatch)
                }
                context = current
            }
        } else {
            context = VersionTwoContext(header: message.header)
        }

        let result = pair(message, in: &context, after: released)
        if result.disposition != .rejected {
            versionTwo = context
        }
        return result
    }

    /// Applies a message within its context's generation: the table of
    /// section 9.2. `earlier` holds frames already released by a generation
    /// change; it is non-empty only for a fresh context, where nothing can
    /// be refused.
    private func pair(_ message: VersionTwoMessage, in context: inout VersionTwoContext,
                      after earlier: [DisplayFrame]) -> DisplayDecodeResult {
        var frames = earlier
        let sequence = message.header.sequence
        if let last = context.lastReleased, !Self.isNewer(sequence, than: last) {
            counters.staleMessages += 1
            return Self.refusal(.rejected, .staleSequence)
        }
        guard let held = context.held else {
            if message.kind == .whole {
                frames.append(release(message, partial: false, in: &context))
            } else {
                context.held = message
            }
            return Self.released(frames)
        }
        let heldSequence = held.header.sequence
        if sequence == heldSequence {
            if message.kind == .whole {
                // A sequence is whole or split, never both.
                counters.malformed += 1
                return Self.refusal(.rejected, .malformed)
            }
            if message.kind == held.kind {
                counters.duplicateParts += 1
                return Self.refusal(.rejected, .staleSequence)
            }
            if message.header != held.header {
                counters.malformed += 1
                return Self.refusal(.rejected, .malformed)
            }
            let trace = message.kind == .tracePart ? message : held
            let row = message.kind == .rowPart ? message : held
            let merged = VersionTwoMessage(header: message.header, kind: .whole, trace: trace.trace,
                                           waterfall: row.waterfall, wide: row.wide)
            context.held = nil
            frames.append(release(merged, partial: false, in: &context))
            return Self.released(frames)
        }
        if Self.isNewer(sequence, than: heldSequence) {
            context.held = nil
            frames.append(release(held, partial: true, in: &context))
            if message.kind == .whole {
                frames.append(release(message, partial: false, in: &context))
            } else {
                context.held = message
            }
            return Self.released(frames)
        }
        // Newer than `L` and older than the held part: released now, the
        // held part stays.
        frames.append(release(message, partial: message.kind != .whole, in: &context))
        return Self.released(frames)
    }

    /// `accepted` with the frames, or `held` when nothing was released.
    private static func released(_ frames: [DisplayFrame]) -> DisplayDecodeResult {
        if frames.isEmpty {
            return DisplayDecodeResult(disposition: .held, reason: .none, frames: [])
        }
        return DisplayDecodeResult(disposition: .accepted, reason: .none, frames: frames)
    }

    /// Releases `message` (whole, merged or partial): counts it, makes its
    /// sequence `L`, fills its absent planes from the last decoded ones (or
    /// minimum dBm), then keeps the planes it carried as the new fills.
    private func release(_ message: VersionTwoMessage, partial: Bool,
                         in context: inout VersionTwoContext) -> DisplayFrame {
        let header = message.header
        if let last = context.lastReleased {
            counters.framesLost += UInt64(header.sequence &- last &- 1)
        }
        context.lastReleased = header.sequence
        if partial {
            counters.partsLost += 1
        }
        let trace = message.trace ?? context.fillTrace
            ?? [UInt8](repeating: 0, count: header.traceLength)
        let waterfall = message.waterfall ?? context.fillWaterfall
            ?? [UInt8](repeating: 0, count: header.waterfallLength)
        let wide = message.wide ?? context.fillWide
            ?? [UInt8](repeating: 0, count: header.wideLength)
        if let carried = message.trace {
            context.fillTrace = carried
        }
        if let carried = message.waterfall {
            context.fillWaterfall = carried
        }
        if let carried = message.wide {
            context.fillWide = carried
        }
        let minimum = Double(header.minDbm)
        let maximum = Double(header.maxDbm)
        let dbm = { (row: [UInt8]) -> [Float] in
            row.map { Self.dbm($0, minimum: minimum, maximum: maximum) }
        }
        return DisplayFrame(
            endpointId: header.endpointId, contextGeneration: header.contextGeneration,
            encoderSequence: header.sequence, producerTimestamp: header.timestamp,
            isKeyframe: false, waterfallAdvance: message.waterfall != nil,
            minDbm: header.minDbm, maxDbm: header.maxDbm,
            traceDbm: dbm(trace), waterfallDbm: dbm(waterfall), wideDbm: dbm(wide),
            traceCarried: message.trace == nil)
    }

    /// The structure of a version 2 message, before any state is read: the
    /// header, the flag combination and every plane.
    private static func parseVersionTwo(_ bytes: [UInt8]) -> Result<VersionTwoMessage, ParseFailure> {
        var reader = ByteReader(bytes)
        do {
            // Magic and version were checked before the version chose this path.
            _ = try reader.u32()
            _ = try reader.u8()
            let flags = try reader.u8()
            let size = try reader.u16()
            if flags & ~v2KnownFlags != 0 {
                return .failure(ParseFailure(.unknownFlags))
            }
            if Int(size) != headerBytes {
                return .failure(ParseFailure(.malformed))
            }
            let header = VersionTwoHeader(
                endpointId: try reader.u32(), contextGeneration: try reader.u32(),
                sequence: try reader.u32(), timestamp: try reader.u64(),
                minDbm: Float(bitPattern: try reader.u32()),
                maxDbm: Float(bitPattern: try reader.u32()),
                traceLength: Int(try reader.u16()), waterfallLength: Int(try reader.u16()),
                wideLength: Int(try reader.u16()))

            let hasTrace = flags & v2TraceFlag != 0
            let hasWaterfall = flags & v2WaterfallFlag != 0
            let hasWide = flags & wideFlag != 0
            let rowsValid = (1...maximumRowSamples).contains(header.traceLength)
                && (1...maximumRowSamples).contains(header.waterfallLength)
                && header.wideLength <= maximumRowSamples
            // The wide flag is set exactly when a waterfall plane is present
            // and the context has a wide row.
            let wideValid = !hasWaterfall || hasWide == (header.wideLength != 0)
            guard v2ValidFlags.contains(flags), rowsValid, wideValid,
                  header.minDbm.isFinite, header.maxDbm.isFinite, header.maxDbm > header.minDbm else {
                return .failure(ParseFailure(.malformed))
            }

            // The planes, in trace, waterfall, wide order: any fault from
            // here on is a malformed message.
            do {
                let trace = hasTrace ? try readVersionTwoPlane(&reader, length: header.traceLength) : nil
                let waterfall = hasWaterfall
                    ? try readVersionTwoPlane(&reader, length: header.waterfallLength) : nil
                let wide = hasWide ? try readVersionTwoPlane(&reader, length: header.wideLength) : nil
                guard reader.atEnd else {
                    return .failure(ParseFailure(.malformed))
                }
                let kind: VersionTwoKind
                if flags & v2SplitFlag == 0 {
                    kind = .whole
                } else {
                    kind = hasTrace ? .tracePart : .rowPart
                }
                return .success(VersionTwoMessage(header: header, kind: kind, trace: trace,
                                                  waterfall: waterfall, wide: wide))
            } catch {
                return .failure(ParseFailure(.malformed))
            }
        } catch let failure as ParseFailure {
            return .failure(failure)
        } catch {
            return .failure(ParseFailure(.malformed))
        }
    }

    /// Decodes one version 2 plane at `offset` of exactly `length` quantized
    /// samples and moves `offset` past it; nil, with `offset` unchanged, for
    /// anything the plane rules refuse (section 4.2).
    static func decodeVersionTwoPlane(_ bytes: [UInt8], offset: inout Int, length: Int) -> [UInt8]? {
        guard offset >= 0, offset <= bytes.count, (1...maximumRowSamples).contains(length) else {
            return nil
        }
        var reader = ByteReader(bytes, at: offset)
        guard let samples = try? readVersionTwoPlane(&reader, length: length) else {
            return nil
        }
        offset = reader.position
        return samples
    }

    /// Reads one plane: coding, block size code, payload bytes, the bit stream.
    private static func readVersionTwoPlane(_ reader: inout ByteReader, length: Int) throws -> [UInt8] {
        let coding = try reader.u8()
        let code = Int(try reader.u8())
        let payloadBytes = Int(try reader.u16())
        guard coding == v2PlaneCoding, code < v2BlockSizes.count else {
            throw ParseFailure(.malformed)
        }
        let payload = try reader.take(payloadBytes)
        guard let samples = decodeRice(payload, length: length, blockSize: v2BlockSizes[code]) else {
            throw ParseFailure(.malformed)
        }
        return samples
    }

    /// The bit stream of a plane: the first sample in 8 bits, then blocks of
    /// `blockSize`, each a 3-bit `k` and its samples (raw when `k` is 7,
    /// otherwise zigzag Rice residuals against the previous sample, in 8-bit
    /// arithmetic that wraps), then a zero pad to the byte. Nil when the
    /// payload runs out, a `z` passes 255, a pad bit is set or bytes remain.
    private static func decodeRice(_ payload: [UInt8], length: Int, blockSize: Int) -> [UInt8]? {
        var bits = BitReader(payload)
        guard let first = bits.read(8) else {
            return nil
        }
        var row: [UInt8] = []
        row.reserveCapacity(length)
        row.append(UInt8(first))
        while row.count < length {
            let count = min(blockSize, length - row.count)
            guard let k = bits.read(3) else {
                return nil
            }
            for _ in 0..<count {
                if k == v2RawEscape {
                    guard let raw = bits.read(8) else {
                        return nil
                    }
                    row.append(UInt8(raw))
                    continue
                }
                // A quotient above 255 >> k would make `z` exceed 255.
                guard let quotient = bits.zeros(atMost: 255 >> k), let low = bits.read(k) else {
                    return nil
                }
                let z = quotient << k | low
                let residual = z & 1 == 0 ? z >> 1 : -((z + 1) >> 1)
                row.append(row[row.count - 1] &+ UInt8(truncatingIfNeeded: residual))
            }
        }
        guard bits.endsOnZeroPad else {
            return nil
        }
        return row
    }

    /// Reads bits most significant first.
    private struct BitReader {
        private let bytes: [UInt8]
        private var position = 0

        init(_ bytes: [UInt8]) {
            self.bytes = bytes
        }

        private var remaining: Int { bytes.count * 8 - position }

        private mutating func bit() -> Int {
            defer { position += 1 }
            return Int(bytes[position >> 3] >> (7 - position & 7)) & 1
        }

        /// The next `count` bits as an unsigned value, or nil past the end.
        mutating func read(_ count: Int) -> Int? {
            guard count <= remaining else {
                return nil
            }
            var value = 0
            for _ in 0..<count {
                value = value << 1 | bit()
            }
            return value
        }

        /// The zero bits before the next one bit (which is consumed), or nil
        /// when there are more than `limit` of them or the bits run out.
        mutating func zeros(atMost limit: Int) -> Int? {
            var count = 0
            while remaining > 0 {
                if bit() == 1 {
                    return count
                }
                count += 1
                if count > limit {
                    return nil
                }
            }
            return nil
        }

        /// The stream ends within the last byte and its pad bits are zero.
        var endsOnZeroPad: Bool {
            guard remaining < 8 else {
                return false
            }
            var reader = self
            while reader.remaining > 0 {
                if reader.bit() != 0 {
                    return false
                }
            }
            return true
        }
    }
}
