// NereusSDR for iOS: the VAX channels of the Core's computer, read from its vax object and vaxLevels record
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The VAX channels of the computer the Core runs on, as its desktop VAX
/// applet shows them (link document sections 6.3, 7.1 and 7.7, `vaxVersion`
/// 1): the `vax` object (class `StationVax`), sent only to a device that
/// declared `vax` 1 in its hello, on a Core whose computer publishes VAX
/// devices. Four channels, each with the slices feeding it, its receive
/// level and mute, and its device name, then the transmit slice and the
/// level of VAX used as the microphone. The meters travel on the
/// `vaxLevels` record stream, one record.
///
/// Reading is tolerant: a property missing or of another kind reads as not
/// sent, never as a failure.
public struct StationVax: Equatable, Sendable {
    public struct Channel: Equatable, Sendable, Identifiable {
        /// 1 to 4.
        public var number: Int
        /// The letters of the slices feeding it, in slice order ("AB"); empty for none.
        public var slices: String
        /// Its receive level, 0 to 1.
        public var rxGain: Double?
        public var muted: Bool?
        /// The device name the desktop applet shows; empty when not sent.
        public var device: String

        public var id: Int { number }

        public init(number: Int, slices: String = "", rxGain: Double? = nil, muted: Bool? = nil, device: String = "") {
            self.number = number
            self.slices = slices
            self.rxGain = rxGain
            self.muted = muted
            self.device = device
        }
    }

    /// The meters, as the one `vaxLevels` record carries them.
    public struct Levels: Equatable, Sendable {
        /// Channels 1 to 4, 0 to 1; nil where not sent.
        public var channels: [Double?]
        /// The VAX microphone, 0 to 1.
        public var tx: Double?
        /// The Core's clock when it read them, in ms.
        public var atMs: Double?

        public init(channels: [Double?] = Array(repeating: nil, count: StationVax.channelCount), tx: Double? = nil,
                    atMs: Double? = nil) {
            self.channels = channels
            self.tx = tx
            self.atMs = atMs
        }

        public init(record: LinkMessage.RecordBatch.Record) {
            func level(_ name: String) -> Double? {
                guard case .number(let value)? = record.fields[name], value.isFinite else {
                    return nil
                }
                return min(max(value, 0), 1)
            }
            var atMs: Double?
            if case .number(let value)? = record.fields["atMs"], value.isFinite {
                atMs = value
            }
            self.init(channels: (1...StationVax.channelCount).map { level("ch\($0)Level") }, tx: level("txLevel"),
                      atMs: atMs)
        }
    }

    /// The feature a device declares in its hello, and the capability the Core answers with.
    public static let featureName = "vax"
    public static let capabilityName = "vaxVersion"
    /// The agreed minor the VAX channels arrived in.
    public static let minor: UInt16 = 11
    public static let objectKey = "vax"
    public static let className = "StationVax"
    /// The meters' stream and the most records it holds.
    public static let levelsStream = "vaxLevels"
    public static let levelsCapacity = 1
    public static let channelCount = 4
    /// Every VAX level goes from 0 to 1.
    public static let gainRange: ClosedRange<Double> = 0...1
    public static let txGainProperty = "txGain"

    public static func slicesProperty(_ channel: Int) -> String { "ch\(channel)Slices" }
    public static func rxGainProperty(_ channel: Int) -> String { "ch\(channel)RxGain" }
    public static func mutedProperty(_ channel: Int) -> String { "ch\(channel)Muted" }
    public static func deviceProperty(_ channel: Int) -> String { "ch\(channel)Device" }

    public var channels: [Channel]
    /// The transmit slice's letter; empty for none.
    public var txSlice: String
    /// The level of VAX used as the microphone, 0 to 1.
    public var txGain: Double?

    public init(channels: [Channel], txSlice: String = "", txGain: Double? = nil) {
        self.channels = channels
        self.txSlice = txSlice
        self.txGain = txGain
    }

    /// Reads the `vax` object's values.
    public init(values: [String: MirrorValue]) {
        func text(_ name: String) -> String {
            if case .text(let value)? = values[name] {
                return value
            }
            return ""
        }
        func level(_ name: String) -> Double? {
            switch values[name] {
            case .double(let value)? where value.isFinite:
                return value
            case .int(let value)?:
                return Double(value)
            default:
                return nil
            }
        }
        func flag(_ name: String) -> Bool? {
            if case .bool(let value)? = values[name] {
                return value
            }
            return nil
        }
        self.init(channels: (1...Self.channelCount).map { number in
            Channel(number: number, slices: text(Self.slicesProperty(number)), rxGain: level(Self.rxGainProperty(number)),
                    muted: flag(Self.mutedProperty(number)), device: text(Self.deviceProperty(number)))
        }, txSlice: text("txSlice"), txGain: level(Self.txGainProperty))
    }
}
