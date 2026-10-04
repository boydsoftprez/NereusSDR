// NereusSDR for iOS: the AM Mod Monitor's readings, one record the Core keeps on txAmModulation or txAmModulationFeedback
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The AM Mod Monitor's readings as the Core sends them (link document
/// section 7.7, `txModMonitorVersion` 1): one record, `id` "0", on the
/// stream for its source, present only while a device watches and the
/// radio is keyed in AM, SAM or DSB. The holds fall as the Core lets them
/// fall; the phone draws them as sent and never decays them itself.
public struct AmModulationReading: Sendable, Equatable {
    /// Which of the Core's two measurements a monitor watches, and its
    /// `txModMonitor.reset` source number.
    public enum Source: Int64, Sendable, CaseIterable {
        /// The transmit I/Q the Core sends its radio.
        case txIq = 0
        /// The PA's output, from PureSignal's feedback receiver.
        case paFeedback = 1

        /// The record stream that carries this source's readings.
        public var stream: String {
            switch self {
            case .txIq:
                return AmModulationReading.txStream
            case .paFeedback:
                return AmModulationReading.feedbackStream
            }
        }
    }

    /// The capability that gates the streams, the reset and the feedback receiver.
    public static let capabilityName = "txModMonitorVersion"
    public static let txStream = "txAmModulation"
    public static let feedbackStream = "txAmModulationFeedback"
    /// Each stream holds one record.
    public static let capacity = 1
    /// Clears the Core's analyzer for that source, for every device watching.
    public static let resetVerb = "txModMonitor.reset"
    /// The Core's feedback receiver, `"0"` to `"4"`, shared by every device.
    public static let feedbackReceiverKey = "ModMon/FbStream"
    /// The Core's feedback receiver when it has none set: rx1.
    public static let defaultFeedbackReceiver = 1
    public static let feedbackReceivers = 0...4
    /// The most points a trace carries.
    public static let maxScopePoints = 512

    /// The Core's clock when it read these, in ms since the epoch.
    public var atMs: Double
    /// The peak modulation in percent since the Core's previous record.
    public var posPeakPct: Double
    public var negPeakPct: Double
    /// The peaks held, as the Core holds them.
    public var posHoldPct: Double
    public var negHoldPct: Double
    /// The carrier, linear 0 to 1 at full scale, and in dBFS (-120 with none).
    public var carrierLevel: Double
    public var carrierDbfs: Double
    public var carrierPresent: Bool
    public var carrierLow: Bool
    public var carrierHigh: Bool
    /// The rate of the trace's points.
    public var scopeRateHz: Double
    /// The envelope trace, oldest first, in percent modulation.
    public var scopePct: [Double]

    public init(atMs: Double = 0, posPeakPct: Double, negPeakPct: Double, posHoldPct: Double, negHoldPct: Double,
                carrierLevel: Double, carrierDbfs: Double, carrierPresent: Bool, carrierLow: Bool = false,
                carrierHigh: Bool = false, scopeRateHz: Double = 0, scopePct: [Double] = []) {
        self.atMs = atMs
        self.posPeakPct = posPeakPct
        self.negPeakPct = negPeakPct
        self.posHoldPct = posHoldPct
        self.negHoldPct = negHoldPct
        self.carrierLevel = carrierLevel
        self.carrierDbfs = carrierDbfs
        self.carrierPresent = carrierPresent
        self.carrierLow = carrierLow
        self.carrierHigh = carrierHigh
        self.scopeRateHz = scopeRateHz
        self.scopePct = scopePct
    }

    /// The readings in `record`, or nil when any reading is missing or of
    /// the wrong kind. A trace that cannot be read is left empty.
    public init?(record: LinkMessage.RecordBatch.Record) {
        let fields = record.fields
        func number(_ name: String) -> Double? {
            if case .number(let value)? = fields[name], value.isFinite {
                return value
            }
            return nil
        }
        func flag(_ name: String) -> Bool? {
            if case .bool(let value)? = fields[name] {
                return value
            }
            return nil
        }
        guard let posPeak = number("posPeakPct"), let negPeak = number("negPeakPct"),
              let posHold = number("posHoldPct"), let negHold = number("negHoldPct"),
              let level = number("carrierLevel"), let dbfs = number("carrierDbfs"),
              let present = flag("carrierPresent"), let low = flag("carrierLow"), let high = flag("carrierHigh") else {
            return nil
        }
        var trace: [Double] = []
        if case .string(let text)? = fields["scopePctTenths"] {
            trace = Self.scopePercent(text)
        }
        self.init(atMs: number("atMs") ?? 0, posPeakPct: posPeak, negPeakPct: negPeak, posHoldPct: posHold,
                  negHoldPct: negHold, carrierLevel: level, carrierDbfs: dbfs, carrierPresent: present,
                  carrierLow: low, carrierHigh: high, scopeRateHz: number("scopeRateHz") ?? 0, scopePct: trace)
    }

    /// The positive hold less the negative hold.
    public var asymmetryPct: Double {
        posHoldPct - negHoldPct
    }

    /// A trace of little-endian int16 tenths of a percent, in base64, as
    /// percents, the newest ``maxScopePoints`` at most; empty when the text
    /// is not base64. An odd last byte is dropped.
    public static func scopePercent(_ base64: String) -> [Double] {
        guard !base64.isEmpty, let data = Data(base64Encoded: base64) else {
            return []
        }
        let bytes = [UInt8](data)
        let count = bytes.count / 2
        let first = max(0, count - maxScopePoints)
        var points: [Double] = []
        points.reserveCapacity(count - first)
        for index in first..<count {
            let raw = UInt16(bytes[2 * index]) | (UInt16(bytes[2 * index + 1]) << 8)
            points.append(Double(Int16(bitPattern: raw)) / 10)
        }
        return points
    }
}
