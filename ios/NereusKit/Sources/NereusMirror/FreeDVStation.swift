// NereusSDR for iOS: one station FreeDV Reporter lists at the Core, read from the Core's freedvStations record
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One station on FreeDV Reporter as the Core hears it (link document
/// section 7.7, stream `freedvStations`, R-IOS-26). The Core runs the
/// reporter connection and works out distance and heading from its own
/// grid square; the phone shows what it sends.
///
/// Reading is tolerant: a field that is missing or of another kind reads as
/// empty (or its "not known" value), never as a failure, so a record from a
/// newer or older Core still lists.
public struct FreeDVStation: Sendable, Equatable, Identifiable {
    /// The station's FreeDV Reporter session id (the record's id).
    public var id: String
    public var callsign: String
    public var gridSquare: String
    /// From the Core's own grid square; 0 while either grid square is not known.
    public var distanceKm: Double
    public var headingDeg: Double
    /// `N` to `NNW`; empty while either grid square is not known.
    public var headingCardinal: String
    /// The station's software, `freedv-gui 2.0`.
    public var version: String
    /// Whole hertz; 0 not known.
    public var frequencyHz: Double
    public var txMode: String
    /// `Active`, `TX` or `RX Only`.
    public var status: String
    public var userMessage: String
    public var lastTx: Date?
    public var lastRxCallsign: String
    public var lastRxMode: String
    /// Nil when not known (the Core sends -99).
    public var snrDb: Int?
    public var lastUpdate: Date?
    public var transmitting: Bool
    /// Whom its latest receive report heard, while that report stands; empty otherwise.
    public var receivingFrom: String
    /// The Core's clock, ms since the epoch, when the message last changed; 0 never.
    public var messageChangedAtMs: Int64
    public var lastRx: Date?
    /// The band as the catalogue numbers it (11 for GEN), when the record
    /// names one. Only a Core at ``bandVersion`` or later names it, and the
    /// app reads it only from such a Core.
    public var band: Int?

    /// The stream's name, and the most records it holds.
    public static let streamName = "freedvStations"
    public static let capacity = 1000
    /// The capability under which the Core runs FreeDV Reporter.
    public static let capabilityName = "stationFreedvVersion"
    /// The first `stationFreedvVersion` whose records name each station's band.
    public static let bandVersion: Int64 = 2
    /// The FreeDV Reporter source's name for `spots.connect` and its console.
    public static let sourceName = "freedvReporter"
    /// The SNR the Core sends when it is not known.
    public static let unknownSnr = -99

    public init(id: String, callsign: String, gridSquare: String = "", distanceKm: Double = 0, headingDeg: Double = 0,
                headingCardinal: String = "", version: String = "", frequencyHz: Double = 0, txMode: String = "",
                status: String = "", userMessage: String = "", lastTx: Date? = nil, lastRxCallsign: String = "",
                lastRxMode: String = "", snrDb: Int? = nil, lastUpdate: Date? = nil, transmitting: Bool = false,
                receivingFrom: String = "", messageChangedAtMs: Int64 = 0, lastRx: Date? = nil, band: Int? = nil) {
        self.id = id
        self.callsign = callsign
        self.gridSquare = gridSquare
        self.distanceKm = distanceKm
        self.headingDeg = headingDeg
        self.headingCardinal = headingCardinal
        self.version = version
        self.frequencyHz = frequencyHz
        self.txMode = txMode
        self.status = status
        self.userMessage = userMessage
        self.lastTx = lastTx
        self.lastRxCallsign = lastRxCallsign
        self.lastRxMode = lastRxMode
        self.snrDb = snrDb
        self.lastUpdate = lastUpdate
        self.transmitting = transmitting
        self.receivingFrom = receivingFrom
        self.messageChangedAtMs = messageChangedAtMs
        self.lastRx = lastRx
        self.band = band
    }

    /// Reads one `freedvStations` record.
    public init(record: LinkMessage.RecordBatch.Record) {
        let fields = record.fields
        func text(_ name: String) -> String {
            if case .string(let value)? = fields[name] {
                return value
            }
            return ""
        }
        func number(_ name: String) -> Double? {
            if case .number(let value)? = fields[name], value.isFinite {
                return value
            }
            return nil
        }
        func flag(_ name: String) -> Bool {
            if case .bool(let value)? = fields[name] {
                return value
            }
            return false
        }
        var snr: Int?
        if let value = number("snrDb"), abs(value) < 1_000 {
            let whole = Int(value.rounded())
            snr = whole == Self.unknownSnr ? nil : whole
        }
        var band: Int?
        if let value = number("band"), value >= 0, value <= 65_535, value == value.rounded() {
            band = Int(value)
        }
        var changed: Int64 = 0
        if let value = number("messageChangedAtMs"), value > 0, value < 9.0e15 {
            changed = Int64(value)
        }
        self.init(id: record.id, callsign: text("callsign"), gridSquare: text("gridSquare"),
                  distanceKm: max(0, number("distanceKm") ?? 0), headingDeg: number("headingDeg") ?? 0,
                  headingCardinal: text("headingCardinal"), version: text("version"),
                  frequencyHz: max(0, number("frequencyHz") ?? 0), txMode: text("txMode"), status: text("status"),
                  userMessage: text("userMessage"), lastTx: Self.time(text("lastTxUtc")),
                  lastRxCallsign: text("lastRxCallsign"), lastRxMode: text("lastRxMode"), snrDb: snr,
                  lastUpdate: Self.time(text("lastUpdateUtc")), transmitting: flag("transmitting"),
                  receivingFrom: text("receivingFrom"), messageChangedAtMs: changed,
                  lastRx: Self.time(text("lastRxUtc")), band: band)
    }

    /// Distance and heading are known: the Core has both grid squares.
    public var hasBearing: Bool {
        !headingCardinal.isEmpty
    }

    // MARK: Times

    private static let wholeSeconds = Date.ISO8601FormatStyle()
    private static let fractionalSeconds = Date.ISO8601FormatStyle(includingFractionalSeconds: true)

    /// An ISO 8601 UTC time, with or without fractions of a second; nil when empty or unreadable.
    public static func time(_ text: String) -> Date? {
        guard !text.isEmpty else {
            return nil
        }
        if let date = try? wholeSeconds.parse(text) {
            return date
        }
        return try? fractionalSeconds.parse(text)
    }
}
