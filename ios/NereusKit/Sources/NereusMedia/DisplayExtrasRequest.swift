// NereusSDR for iOS: the display extras a spectrum subscription asks the Core to compute
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// What a `subscribe` asks the Core to compute for its endpoint (display
/// extras document, section 2; R-IOS-11). The app carries none of these
/// computations (D4, spec section 4.11): the Core runs them and sends their
/// results in an NSDX datagram beside each frame. Every field is optional;
/// each present one asks for what it names. The ranges are the document's,
/// and a request outside them is refused before anything is sent, since
/// the Core would refuse it as one it cannot read.
public struct DisplayExtrasRequest: Equatable, Sendable {
    /// The top-N peak markers.
    public struct PeakBlobs: Equatable, Sendable {
        /// 1 to 20.
        public var count: Int
        /// 0 (no hold: markers follow each frame) or 100 to 60000.
        public var holdMs: Int
        /// 0 (a marker goes at the end of its hold) or 1 to 60.
        public var fallDbPerSec: Double
        /// Keeps markers inside the slice's receive filter.
        public var insideOnly: Bool

        public init(count: Int, holdMs: Int, fallDbPerSec: Double, insideOnly: Bool) {
            self.count = count
            self.holdMs = holdMs
            self.fallDbPerSec = fallDbPerSec
            self.insideOnly = insideOnly
        }
    }

    /// The active peak hold trace; its section comes only while `enabled`.
    public struct ActivePeakHold: Equatable, Sendable {
        public var enabled: Bool
        /// 100 to 60000.
        public var holdMs: Int
        /// 0.1 to 120.
        public var fallDbPerSec: Double
        /// Keeps the trace running while the endpoint's slice transmits
        /// (version 3). Nil leaves the member out, which the Core reads as
        /// true; only a Core that sent `displayExtrasVersion` 3 is sent it.
        public var onTx: Bool?

        public init(enabled: Bool, holdMs: Int, fallDbPerSec: Double, onTx: Bool? = nil) {
            self.enabled = enabled
            self.holdMs = holdMs
            self.fallDbPerSec = fallDbPerSec
            self.onTx = onTx
        }
    }

    /// The noise-floor line, moved by `shiftDb`; its section comes only while `enabled`.
    public struct NoiseFloor: Equatable, Sendable {
        public var enabled: Bool
        /// -12 to 12.
        public var shiftDb: Double
        /// Asks for the fast-attack state section (version 4) while
        /// `enabled`. Nil leaves the member out; only a Core that sent
        /// `displayExtrasVersion` 4 is sent it.
        public var fastAttack: Bool?

        public init(enabled: Bool, shiftDb: Double, fastAttack: Bool? = nil) {
            self.enabled = enabled
            self.shiftDb = shiftDb
            self.fastAttack = fastAttack
        }
    }

    /// The waterfall's low and high levels.
    public struct WaterfallLevels: Equatable, Sendable {
        public enum Mode: String, Equatable, Sendable {
            case manual
            case agc
            case noiseFloorAgc
            case clarity
        }

        public var mode: Mode
        /// -400 to 100 each.
        public var lowDbm: Double
        public var highDbm: Double
        /// -60 to 60, whole decibels.
        public var offsetDb: Int

        public init(mode: Mode, lowDbm: Double, highDbm: Double, offsetDb: Int) {
            self.mode = mode
            self.lowDbm = lowDbm
            self.highDbm = highDbm
            self.offsetDb = offsetDb
        }
    }

    /// Why a request was not sent: the field that is out of its range. For
    /// the log and the app's own code, never for the operator.
    public enum Invalid: Error, Equatable, Sendable {
        case peakBlobs
        case activePeakHold
        case noiseFloor
        case waterfallLevels
        case calibrationOffsetDb
        case averageTimeMs
        case waterfallAverageTimeMs
    }

    public static let blobCountRange = 1...20
    public static let blobHoldRange = 100...60_000
    public static let blobFallRange = 1.0...60.0
    public static let peakHoldRange = 100...60_000
    public static let peakHoldFallRange = 0.1...120.0
    public static let shiftRange = -12.0...12.0
    public static let levelRange = -400.0...100.0
    public static let offsetRange = -60...60
    public static let calibrationRange = -30.0...30.0
    public static let averageTimeRange = 10...9999

    /// The NSDX section bits (display extras document, section 3.2).
    public static let peakBlobsSection: UInt8 = 0x01
    public static let peakHoldSection: UInt8 = 0x02
    public static let noiseFloorSection: UInt8 = 0x04
    public static let waterfallLevelsSection: UInt8 = 0x08
    /// One byte, bit 0 the noise floor's fast attack (version 4).
    public static let noiseFloorStateSection: UInt8 = 0x10
    /// The fixed header, in bytes.
    public static let headerBytes = 20
    /// The most blobs a section carries.
    public static let maximumBlobs = 20

    public var peakBlobs: PeakBlobs?
    public var activePeakHold: ActivePeakHold?
    public var noiseFloor: NoiseFloor?
    public var waterfallLevels: WaterfallLevels?
    /// Normalise every dBm to a 1 Hz bandwidth.
    public var normalize: Bool?
    /// -30 to 30: every dBm moves by it.
    public var calibrationOffsetDb: Double?
    /// 10 to 9999: the spectrum's averaging time, and the waterfall's when
    /// `waterfallAverageTimeMs` is absent.
    public var averageTimeMs: Int?
    /// 10 to 9999: the waterfall's own averaging time. Absent, the waterfall
    /// takes `averageTimeMs`'s; with neither, its plane's `averageAlpha`.
    public var waterfallAverageTimeMs: Int?

    public init(peakBlobs: PeakBlobs? = nil, activePeakHold: ActivePeakHold? = nil, noiseFloor: NoiseFloor? = nil,
                waterfallLevels: WaterfallLevels? = nil, normalize: Bool? = nil, calibrationOffsetDb: Double? = nil,
                averageTimeMs: Int? = nil, waterfallAverageTimeMs: Int? = nil) {
        self.peakBlobs = peakBlobs
        self.activePeakHold = activePeakHold
        self.noiseFloor = noiseFloor
        self.waterfallLevels = waterfallLevels
        self.normalize = normalize
        self.calibrationOffsetDb = calibrationOffsetDb
        self.averageTimeMs = averageTimeMs
        self.waterfallAverageTimeMs = waterfallAverageTimeMs
    }

    /// No field is present: the subscription asks for nothing extra.
    public var isEmpty: Bool {
        self == DisplayExtrasRequest()
    }

    /// The NSDX sections these fields ask for.
    public var sections: UInt8 {
        var bits: UInt8 = 0
        if peakBlobs != nil {
            bits |= Self.peakBlobsSection
        }
        if activePeakHold?.enabled == true {
            bits |= Self.peakHoldSection
        }
        if noiseFloor?.enabled == true {
            bits |= Self.noiseFloorSection
        }
        if waterfallLevels != nil {
            bits |= Self.waterfallLevelsSection
        }
        if noiseFloor?.enabled == true, noiseFloor?.fastAttack == true {
            bits |= Self.noiseFloorStateSection
        }
        return bits
    }

    /// The largest NSDX datagram these fields can bring for a trace of
    /// `traceSamples` samples (section 3.3):
    /// `20 + 121·[blobs] + A(n)·[hold] + 4·[floor] + 8·[levels]`, where
    /// `A(n) = 3 + 5·ceil(n/128) + n`; 0 when no section is asked for. With
    /// one message a frame more, it is what the Core charges for the extras.
    public func worstCaseBytesPerFrame(traceSamples: Int) -> Int {
        let bits = sections
        guard bits != 0 else {
            return 0
        }
        var bytes = Self.headerBytes
        if bits & Self.peakBlobsSection != 0 {
            bytes += 1 + 6 * Self.maximumBlobs
        }
        if bits & Self.peakHoldSection != 0, traceSamples > 0 {
            bytes += 3 + 5 * ((traceSamples + 127) / 128) + traceSamples
        }
        if bits & Self.noiseFloorSection != 0 {
            bytes += 4
        }
        if bits & Self.waterfallLevelsSection != 0 {
            bytes += 8
        }
        if bits & Self.noiseFloorStateSection != 0 {
            bytes += 1
        }
        return bytes
    }

    /// Throws for the first field outside its range.
    public func validate() throws {
        if let blobs = peakBlobs {
            guard Self.blobCountRange.contains(blobs.count),
                  blobs.holdMs == 0 || Self.blobHoldRange.contains(blobs.holdMs),
                  blobs.fallDbPerSec.isFinite,
                  blobs.fallDbPerSec == 0 || Self.blobFallRange.contains(blobs.fallDbPerSec) else {
                throw Invalid.peakBlobs
            }
        }
        if let hold = activePeakHold {
            guard Self.peakHoldRange.contains(hold.holdMs), hold.fallDbPerSec.isFinite,
                  Self.peakHoldFallRange.contains(hold.fallDbPerSec) else {
                throw Invalid.activePeakHold
            }
        }
        if let floor = noiseFloor {
            guard floor.shiftDb.isFinite, Self.shiftRange.contains(floor.shiftDb) else {
                throw Invalid.noiseFloor
            }
        }
        if let levels = waterfallLevels {
            guard levels.lowDbm.isFinite, levels.highDbm.isFinite, Self.levelRange.contains(levels.lowDbm),
                  Self.levelRange.contains(levels.highDbm), Self.offsetRange.contains(levels.offsetDb) else {
                throw Invalid.waterfallLevels
            }
        }
        if let offset = calibrationOffsetDb {
            guard offset.isFinite, Self.calibrationRange.contains(offset) else {
                throw Invalid.calibrationOffsetDb
            }
        }
        if let time = averageTimeMs, !Self.averageTimeRange.contains(time) {
            throw Invalid.averageTimeMs
        }
        if let time = waterfallAverageTimeMs, !Self.averageTimeRange.contains(time) {
            throw Invalid.waterfallAverageTimeMs
        }
    }

    /// The `subscribe` keys these fields add: exactly the ones present.
    public var subscribeFields: [String: LinkJSON] {
        var fields: [String: LinkJSON] = [:]
        if let blobs = peakBlobs {
            fields["peakBlobs"] = .object([
                "count": .number(Double(blobs.count)), "holdMs": .number(Double(blobs.holdMs)),
                "fallDbPerSec": .number(blobs.fallDbPerSec), "insideOnly": .bool(blobs.insideOnly),
            ])
        }
        if let hold = activePeakHold {
            var members: [String: LinkJSON] = [
                "enabled": .bool(hold.enabled), "holdMs": .number(Double(hold.holdMs)),
                "fallDbPerSec": .number(hold.fallDbPerSec),
            ]
            if let onTx = hold.onTx {
                members["onTx"] = .bool(onTx)
            }
            fields["activePeakHold"] = .object(members)
        }
        if let floor = noiseFloor {
            var members: [String: LinkJSON] = ["enabled": .bool(floor.enabled), "shiftDb": .number(floor.shiftDb)]
            if let fastAttack = floor.fastAttack {
                members["fastAttack"] = .bool(fastAttack)
            }
            fields["noiseFloor"] = .object(members)
        }
        if let levels = waterfallLevels {
            fields["waterfallLevels"] = .object([
                "mode": .string(levels.mode.rawValue), "lowDbm": .number(levels.lowDbm),
                "highDbm": .number(levels.highDbm), "offsetDb": .number(Double(levels.offsetDb)),
            ])
        }
        if let normalize {
            fields["normalize"] = .bool(normalize)
        }
        if let calibrationOffsetDb {
            fields["calibrationOffsetDb"] = .number(calibrationOffsetDb)
        }
        if let averageTimeMs {
            fields["averageTimeMs"] = .number(Double(averageTimeMs))
        }
        if let waterfallAverageTimeMs {
            fields["waterfallAverageTimeMs"] = .number(Double(waterfallAverageTimeMs))
        }
        return fields
    }
}
