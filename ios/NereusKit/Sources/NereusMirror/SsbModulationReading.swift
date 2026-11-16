// NereusSDR for iOS: the SSB Mod Monitor's readings, one record the Core keeps on txSsbModulation or txSsbModulationFeedback
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The SSB Mod Monitor's readings as the Core sends them (link document
/// section 7.7, `txSsbModMonitorVersion` 1): one record, `id` "0", on the
/// stream for its source, present only while a device watches and the radio
/// is keyed in USB or LSB. The Core does the measuring, crest included; the
/// phone shows each value as sent. A record with a field missing, mistyped or
/// out of range is refused, as the desktop's window refuses it.
public struct SsbModulationReading: Sendable, Equatable {
    /// Which of the Core's two measurements a monitor watches, and its
    /// `txSsbModMonitor.reset` source number.
    public enum Source: Int64, Sendable, CaseIterable {
        /// The transmit I/Q the Core sends its radio.
        case txIq = 0
        /// The PA's output, from PureSignal's feedback path.
        case paFeedback = 1

        /// The record stream that carries this source's readings.
        public var stream: String {
            switch self {
            case .txIq:
                return SsbModulationReading.txStream
            case .paFeedback:
                return SsbModulationReading.feedbackStream
            }
        }
    }

    /// The capability that gates the streams and the reset.
    public static let capabilityName = "txSsbModMonitorVersion"
    public static let txStream = "txSsbModulation"
    public static let feedbackStream = "txSsbModulationFeedback"
    /// Each stream holds one record.
    public static let capacity = 1
    /// Clears the Core's analyzer for that source, for every device watching.
    public static let resetVerb = "txSsbModMonitor.reset"
    /// The most points a trace carries.
    public static let maxScopePoints = 512
    /// What the record can carry; a value outside refuses the record.
    public static let percentRange = 0.0...3276.7
    public static let dbfsRange = -120.0...71.0
    public static let crestRange = -200.0...200.0
    public static let scopeRateRange = 0.0...1_536_000.0

    /// The Core's clock when it read these, in ms since the epoch.
    public var atMs: Double
    /// The envelope in percent of full scale: the largest window peak since
    /// the Core's previous record, the slow average, and the peak as the Core holds it.
    public var peakPct: Double
    public var avgPct: Double
    public var peakHoldPct: Double
    /// The same in dB relative to full scale.
    public var peakDbfs: Double
    public var avgDbfs: Double
    public var peakHoldDbfs: Double
    /// Peak over RMS of the envelope, as the Core reads it; nil while the Core sends none.
    public var crestDb: Double?
    public var signalPresent: Bool
    /// The rate of the trace's points.
    public var scopeRateHz: Double
    /// The envelope trace, oldest first, in percent of full scale.
    public var scopePct: [Double]

    public init(atMs: Double = 0, peakPct: Double, avgPct: Double, peakHoldPct: Double, peakDbfs: Double,
                avgDbfs: Double, peakHoldDbfs: Double, crestDb: Double? = nil, signalPresent: Bool,
                scopeRateHz: Double = 0, scopePct: [Double] = []) {
        self.atMs = atMs
        self.peakPct = peakPct
        self.avgPct = avgPct
        self.peakHoldPct = peakHoldPct
        self.peakDbfs = peakDbfs
        self.avgDbfs = avgDbfs
        self.peakHoldDbfs = peakHoldDbfs
        self.crestDb = crestDb
        self.signalPresent = signalPresent
        self.scopeRateHz = scopeRateHz
        self.scopePct = scopePct
    }

    /// The readings in `record`, or nil when any field is missing, mistyped or out of range.
    public init?(record: LinkMessage.RecordBatch.Record) {
        let fields = record.fields
        func number(_ name: String, _ range: ClosedRange<Double>) -> Double? {
            if case .number(let value)? = fields[name], value.isFinite, range.contains(value) {
                return value
            }
            return nil
        }
        guard let atMs = number("atMs", 0.0...Double.greatestFiniteMagnitude),
              let peak = number("peakPct", Self.percentRange), let avg = number("avgPct", Self.percentRange),
              let hold = number("peakHoldPct", Self.percentRange), let peakDb = number("peakDbfs", Self.dbfsRange),
              let avgDb = number("avgDbfs", Self.dbfsRange), let holdDb = number("peakHoldDbfs", Self.dbfsRange),
              let rate = number("scopeRateHz", Self.scopeRateRange), rate == rate.rounded(),
              case .bool(let present)? = fields["signalPresent"],
              case .string(let text)? = fields["scopePctTenths"], let trace = Self.scopePercent(text) else {
            return nil
        }
        // Absent while the Core has no signal to read it from.
        var crest: Double?
        if fields["crestDb"] != nil {
            guard let value = number("crestDb", Self.crestRange) else {
                return nil
            }
            crest = value
        }
        self.init(atMs: atMs,
                  peakPct: peak, avgPct: avg, peakHoldPct: hold, peakDbfs: peakDb, avgDbfs: avgDb,
                  peakHoldDbfs: holdDb, crestDb: crest, signalPresent: present, scopeRateHz: rate, scopePct: trace)
    }

    /// A trace of non-negative little-endian int16 tenths of a percent, in
    /// base64, as percents; nil when it is not base64, has an odd byte, a
    /// negative point, or more than ``maxScopePoints``. An empty text is an empty trace.
    public static func scopePercent(_ base64: String) -> [Double]? {
        guard let data = Data(base64Encoded: base64), data.count % 2 == 0, data.count <= maxScopePoints * 2 else {
            return nil
        }
        let bytes = [UInt8](data)
        var points: [Double] = []
        points.reserveCapacity(bytes.count / 2)
        for index in stride(from: 0, to: bytes.count, by: 2) {
            let tenths = Int16(bitPattern: UInt16(bytes[index]) | (UInt16(bytes[index + 1]) << 8))
            guard tenths >= 0 else {
                return nil
            }
            points.append(Double(tenths) / 10)
        }
        return points
    }
}
