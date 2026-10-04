// NereusSDR for iOS: decodes the Core's NSDX v1 display extras datagrams
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Decodes one NSDX v1 datagram against the accepted context of its
/// endpoint (display extras document, sections 3.2 and 3.4). Stateless: a
/// datagram stands on its own, and a refused one changes nothing.
public enum DisplayExtrasDecoder {
    /// Why a datagram was refused (`none` when it was accepted). The names
    /// match the station's decoder and the link's conformance vectors.
    public enum Reason: String, Equatable, Sendable {
        case none
        case badMagic
        case unsupportedVersion
        case unknownSections
        case truncated
        case oversized
        case malformed
        case contextMismatch
    }

    /// What a datagram decodes against: its endpoint's accepted context.
    public struct Context: Equatable, Sendable {
        public var endpointId: UInt32
        public var contextGeneration: UInt32
        public var minDbm: Float
        public var maxDbm: Float
        public var traceSamples: Int

        public init(endpointId: UInt32, contextGeneration: UInt32, minDbm: Float, maxDbm: Float,
                    traceSamples: Int) {
            self.endpointId = endpointId
            self.contextGeneration = contextGeneration
            self.minDbm = minDbm
            self.maxDbm = maxDbm
            self.traceSamples = traceSamples
        }

        public init(_ context: MediaControlEvent.DisplayContext) {
            self.init(endpointId: context.endpointId, contextGeneration: context.contextGeneration,
                      minDbm: Float(context.minDbm), maxDbm: Float(context.maxDbm),
                      traceSamples: context.traceSamples)
        }
    }

    /// The outcome: the extras only when accepted.
    public struct Result: Equatable, Sendable {
        public let reason: Reason
        public let extras: DisplayExtras?

        public var accepted: Bool { extras != nil }
    }

    /// The largest datagram, as for an NSDC frame (16 KiB).
    public static let maximumBytes = 16 * 1024
    static let magic: [UInt8] = Array("NSDX".utf8)
    static let version: UInt8 = 1
    static let knownSections: UInt8 = 0x1F

    /// Decodes `datagram` against `context`; a nil context (the endpoint has
    /// none accepted) is a context mismatch once the header has been read.
    public static func decode(_ datagram: Data, context: Context?) -> Result {
        if datagram.count > maximumBytes {
            return refusal(.oversized)
        }
        let bytes = [UInt8](datagram)
        var reader = Reader(bytes)
        // The magic and the version, then the rest of the header.
        guard let magicBytes = reader.take(4) else {
            return refusal(.truncated)
        }
        guard magicBytes == magic else {
            return refusal(.badMagic)
        }
        guard let version = reader.u8() else {
            return refusal(.truncated)
        }
        guard version == Self.version else {
            return refusal(.unsupportedVersion)
        }
        guard let sections = reader.u8(), let headerBytes = reader.u16(), let endpointId = reader.u32(),
              let generation = reader.u32(), let sequence = reader.u32() else {
            return refusal(.truncated)
        }
        if sections & ~knownSections != 0 {
            return refusal(.unknownSections)
        }
        if sections == 0 || Int(headerBytes) != DisplayExtrasRequest.headerBytes {
            return refusal(.malformed)
        }
        guard let context, endpointId == context.endpointId, generation == context.contextGeneration else {
            return refusal(.contextMismatch)
        }

        var blobs: [DisplayExtras.PeakBlob]?
        if sections & DisplayExtrasRequest.peakBlobsSection != 0 {
            guard let count = reader.u8() else {
                return refusal(.truncated)
            }
            if Int(count) > DisplayExtrasRequest.maximumBlobs {
                return refusal(.malformed)
            }
            var found: [DisplayExtras.PeakBlob] = []
            for _ in 0..<count {
                guard let sample = reader.u16(), let dbm = reader.f32() else {
                    return refusal(.truncated)
                }
                if Int(sample) >= context.traceSamples || !dbm.isFinite {
                    return refusal(.malformed)
                }
                found.append(DisplayExtras.PeakBlob(traceSample: Int(sample), dbm: dbm))
            }
            blobs = found
        }
        var hold: [Float]?
        if sections & DisplayExtrasRequest.peakHoldSection != 0 {
            var offset = reader.position
            // Any fault in the plane is one that is not an absolute plane
            // of the trace's length, as the station's decoder has it.
            guard let row = DisplayFrameDecoder.decodeAbsolutePlane(bytes, offset: &offset,
                                                                    length: context.traceSamples,
                                                                    minDbm: context.minDbm,
                                                                    maxDbm: context.maxDbm) else {
                return refusal(.malformed)
            }
            reader.position = offset
            hold = row
        }
        var floor: Float?
        if sections & DisplayExtrasRequest.noiseFloorSection != 0 {
            guard let value = reader.f32() else {
                return refusal(.truncated)
            }
            if !value.isFinite {
                return refusal(.malformed)
            }
            floor = value
        }
        var levels: DisplayExtras.WaterfallLevels?
        if sections & DisplayExtrasRequest.waterfallLevelsSection != 0 {
            guard let low = reader.f32(), let high = reader.f32() else {
                return refusal(.truncated)
            }
            if !low.isFinite || !high.isFinite {
                return refusal(.malformed)
            }
            levels = DisplayExtras.WaterfallLevels(lowDbm: low, highDbm: high)
        }
        var fastAttack: Bool?
        if sections & DisplayExtrasRequest.noiseFloorStateSection != 0 {
            guard let state = reader.u8() else {
                return refusal(.truncated)
            }
            // Bit 0 is fast attack; any other bit refuses.
            if state & ~0x01 != 0 {
                return refusal(.malformed)
            }
            fastAttack = state & 0x01 != 0
        }
        if !reader.atEnd {
            return refusal(.malformed)
        }
        return Result(reason: .none, extras: DisplayExtras(
            endpointId: endpointId, contextGeneration: generation, encoderSequence: sequence,
            peakBlobs: blobs, peakHoldDbm: hold, noiseFloorDbm: floor, waterfallLevels: levels,
            noiseFloorFastAttack: fastAttack))
    }

    private static func refusal(_ reason: Reason) -> Result {
        Result(reason: reason, extras: nil)
    }

    /// Big-endian reads; nil past the end.
    private struct Reader {
        private let storage: [UInt8]
        var position = 0

        init(_ storage: [UInt8]) {
            self.storage = storage
        }

        var atEnd: Bool { position == storage.count }

        mutating func take(_ count: Int) -> [UInt8]? {
            guard count <= storage.count - position else {
                return nil
            }
            defer { position += count }
            return Array(storage[position..<(position + count)])
        }

        mutating func u8() -> UInt8? {
            take(1)?[0]
        }

        mutating func u16() -> UInt16? {
            take(2)?.reduce(0) { $0 << 8 | UInt16($1) }
        }

        mutating func u32() -> UInt32? {
            take(4)?.reduce(0) { $0 << 8 | UInt32($1) }
        }

        mutating func f32() -> Float? {
            u32().map(Float.init(bitPattern:))
        }
    }
}
