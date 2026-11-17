// NereusSDR for iOS: the Digital Spectrum Recorder's past picture as the Core sends it: rows, a spectrum and the kept hours
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's Digital Spectrum Recorder.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation

/// The asking pan's display, which `dsr.rows` and `dsr.spectrum` carry so
/// the Core draws the past with the pan's own FFT settings (link document,
/// "The past picture"): the same `fftSize`, `windowType`, `centreHz`,
/// `spanHz` and `pixels` its display subscription carries.
public struct DsrPanView: Equatable, Sendable {
    public var fftSize: Int
    public var windowType: Int
    public var centreHz: Int64
    public var spanHz: Int64
    /// The pan's width in points the Core reduces each row to, 1 to 4096.
    public var pixels: Int

    public init(fftSize: Int, windowType: Int, centreHz: Int64, spanHz: Int64, pixels: Int) {
        self.fftSize = fftSize
        self.windowType = windowType
        self.centreHz = centreHz
        self.spanHz = spanHz
        self.pixels = pixels
    }
}

/// One answer of `dsr.rows` or `dsr.spectrum`: rows of the past, newest
/// first, each `binCount` points from `firstBinHz` up in steps of
/// `hzPerBin`. The wire carries the times as base64 of little-endian 64-bit
/// ns since 1970 UTC, the bins as base64 of little-endian signed 16-bit
/// tenths of a dB, row after row, and `transmit` as one byte per row.
public struct DsrPastFrames: Equatable, Sendable {
    /// What a point reads where nothing kept lies under it.
    public static let nothingKeptDbm: Float = -200

    public let slice: Int
    public let firstBinHz: Double
    public let hzPerBin: Double
    public let binCount: Int
    /// Each row's moment, ns since 1970 UTC, newest first.
    public let utcNs: [Int64]
    /// Each row was kept while this station transmitted.
    public let transmit: [Bool]
    /// Tenths of a dB, `binCount` per row, row after row.
    public let tenthsDb: [Int16]
    /// Rows the Core has still to send for the request.
    public let rowsLeft: Int
    /// Where the next chunk ends, for asking again.
    public let nextToUtcNs: Int64

    public init(slice: Int, firstBinHz: Double, hzPerBin: Double, binCount: Int, utcNs: [Int64], transmit: [Bool],
                tenthsDb: [Int16], rowsLeft: Int = 0, nextToUtcNs: Int64 = 0) {
        self.slice = slice
        self.firstBinHz = firstBinHz
        self.hzPerBin = hzPerBin
        self.binCount = binCount
        self.utcNs = utcNs
        self.transmit = transmit
        self.tenthsDb = tenthsDb
        self.rowsLeft = rowsLeft
        self.nextToUtcNs = nextToUtcNs
    }

    public var rowCount: Int { utcNs.count }

    /// One row's points in dBm.
    public func row(_ index: Int) -> ArraySlice<Int16> {
        let start = index * binCount
        return tenthsDb[start..<(start + binCount)]
    }

    public func dbm(row: Int, bin: Int) -> Float {
        Float(tenthsDb[row * binCount + bin]) / 10
    }

    /// The answer's values as the Core packs them; nil when they do not
    /// agree (a time or a transmit byte for each row, `binCount` bins each).
    public static func decode(slice: Int, firstBinHz: Double, hzPerBin: Double, binCount: Int, times: String,
                              bins: String, transmit: String?, rowsLeft: Int, nextToUtcNs: Int64) -> DsrPastFrames? {
        guard binCount >= 0, let timeData = Data(base64Encoded: times), let binData = Data(base64Encoded: bins),
              timeData.count % 8 == 0, binData.count % 2 == 0 else {
            return nil
        }
        let utcNs = unpackTimes(timeData)
        let tenths: [Int16] = stride(from: 0, to: binData.count, by: 2).map { offset in
            let low = UInt16(binData[binData.startIndex + offset])
            let high = UInt16(binData[binData.startIndex + offset + 1])
            return Int16(bitPattern: low | high << 8)
        }
        guard tenths.count == utcNs.count * binCount else {
            return nil
        }
        var marks = [Bool](repeating: false, count: utcNs.count)
        if let transmit, !transmit.isEmpty {
            guard let bytes = Data(base64Encoded: transmit), bytes.count == utcNs.count else {
                return nil
            }
            marks = bytes.map { $0 != 0 }
        }
        return DsrPastFrames(slice: slice, firstBinHz: firstBinHz, hzPerBin: hzPerBin, binCount: binCount,
                             utcNs: utcNs, transmit: marks, tenthsDb: tenths, rowsLeft: rowsLeft,
                             nextToUtcNs: nextToUtcNs)
    }

    /// Little-endian 64-bit values, as `times` and the hours' `starts` pack them.
    public static func unpackTimes(_ data: Data) -> [Int64] {
        stride(from: 0, to: data.count - data.count % 8, by: 8).map { offset in
            var value: UInt64 = 0
            for byte in 0..<8 {
                value |= UInt64(data[data.startIndex + offset + byte]) << (8 * byte)
            }
            return Int64(bitPattern: value)
        }
    }

    public static func packTimes(_ times: [Int64]) -> Data {
        var data = Data(capacity: times.count * 8)
        for time in times {
            let bits = UInt64(bitPattern: time)
            for byte in 0..<8 {
                data.append(UInt8(truncatingIfNeeded: bits >> (8 * byte)))
            }
        }
        return data
    }

    public static func packBins(_ tenths: [Int16]) -> Data {
        var data = Data(capacity: tenths.count * 2)
        for value in tenths {
            let bits = UInt16(bitPattern: value)
            data.append(UInt8(truncatingIfNeeded: bits))
            data.append(UInt8(truncatingIfNeeded: bits >> 8))
        }
        return data
    }
}

/// One kept hour, as `dsr.hours` sends it: its start and how busy the band
/// was, 0 for silence to 1 for a busy band.
public struct DsrHour: Equatable, Sendable {
    public let startUtcNs: Int64
    public let activity: Double

    public init(startUtcNs: Int64, activity: Double) {
        self.startUtcNs = startUtcNs
        self.activity = activity
    }

    /// `starts` (base64 of 64-bit ns) and `activity` (a JSON array), newest
    /// first; nil when they do not agree.
    public static func decode(starts: String, activity: String) -> [DsrHour]? {
        guard let data = Data(base64Encoded: starts), data.count % 8 == 0,
              let levels = try? JSONDecoder().decode([Double].self, from: Data(activity.utf8)) else {
            return nil
        }
        let times = DsrPastFrames.unpackTimes(data)
        guard times.count == levels.count else {
            return nil
        }
        return zip(times, levels).map { DsrHour(startUtcNs: $0, activity: min(1, max(0, $1))) }
    }
}

/// What `dsr.saveStretch` keeps.
public enum DsrStretchKind: String, Equatable, Sendable {
    /// Every receiver's kept I/Q, which replays like rewind.
    case wholePan = "WholePan"
    /// Slice A's sound over the stretch, in the Files format.
    case sound = "Sound"
}
