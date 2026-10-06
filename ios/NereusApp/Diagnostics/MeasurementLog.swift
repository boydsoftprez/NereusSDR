// NereusSDR for iOS: the measurement log: a session's traffic by kind, battery, heat and band request, a row a minute
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia
import os
import UIKit

/// The measurement log (plan Task 68 step 1; R-IOS-23, R-IOS-22, R-IOS-10;
/// spec section 8, Measurement): while a session runs, one row for each
/// minute with the bytes in and out by kind (``TrafficCounter/Kind``), the
/// battery level and whether the phone is charging, the hottest the phone
/// got in that minute, and what the band asked the Core for, with the
/// network and whether the app was in front. A row also ends early when
/// the network, the band's request or the app's place changes, and when
/// the session ends, so each row holds one set of conditions and its own
/// length in seconds.
///
/// The rows are kept on this phone, in a file that outlives the app, up to
/// ``mostRows``; the oldest go first. They leave the phone only in the
/// support bundle's phone log, where the operator sends them.
///
/// ``SessionController`` drives it from its own clock and ticks, so the
/// tests move a fake clock and tick by hand.
@MainActor
final class MeasurementLog {
    /// What a row is measured under; a change ends the row.
    struct Conditions: Equatable, Sendable {
        /// The phone's network, as the connection row names it.
        var network: String
        /// The app is in front (true), or the phone is locked or in another app.
        var inFront: Bool
        /// The data mode the band is at, after Low Power Mode and the heat.
        var mode: SessionPolicy.Mode
        /// What the band asks the Core for.
        var request: SessionPolicy.Request
    }

    /// The phone's power, read at each sample.
    struct Power: Equatable, Sendable {
        /// The battery's level in percent, or nil when the phone does not say.
        var batteryPercent: Int?
        var charging: Bool
        var heat: SessionPolicy.Heat
    }

    /// A row's length: a minute.
    static let rowSeconds: TimeInterval = 60
    /// The most rows kept: seven days of minutes.
    static let mostRows = 10_080
    /// Rows dropped at once when the log is full, so the file is rewritten
    /// about once a day rather than every minute.
    static let dropAtOnce = 1_440

    /// The support bundle's heading for the log.
    static let heading = "Measurements: one row for each minute of a session, times in UTC"
    /// Said under the heading when no row has been kept.
    static let noRows = "No measurements have been kept on this phone."
    /// The columns, in each row's order.
    static let columns: [String] = {
        var names = ["start", "seconds", "network", "app", "mode", "band_frames_a_second", "band_detail",
                     "battery_percent", "charging", "heat"]
        for kind in TrafficCounter.Kind.allCases {
            names.append("\(kind.rawValue)_in")
            names.append("\(kind.rawValue)_out")
        }
        return names
    }()

    /// Where the phone keeps the log.
    static var defaultFile: URL? {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?
            .appendingPathComponent("Measurements", isDirectory: true)
            .appendingPathComponent("measurements.csv")
    }

    /// The kept rows, oldest first, as their lines.
    private(set) var rows: [String] = []

    private struct OpenRow {
        let start: Date
        let conditions: Conditions
        let traffic: TrafficCounter.ByKind
        var heat: SessionPolicy.Heat
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "measurements")

    private let file: URL?
    private let now: () -> Date
    private let traffic: () -> TrafficCounter.ByKind
    private var open: OpenRow?

    /// Keeps its rows in `file`, or in memory alone when it is nil.
    init(file: URL?, now: @escaping () -> Date, traffic: @escaping () -> TrafficCounter.ByKind) {
        self.file = file
        self.now = now
        self.traffic = traffic
        load()
    }

    /// Whether a session's row is open.
    var isRecording: Bool { open != nil }

    /// A session starts: its first row opens now. A row still open ends first.
    func begin(_ conditions: Conditions, power: Power) {
        end(power: power)
        open = OpenRow(start: now(), conditions: conditions, traffic: traffic(), heat: power.heat)
    }

    /// Reads the conditions and the power during a session. The row ends,
    /// and the next opens, when a minute has passed or the conditions changed.
    func sample(_ conditions: Conditions, power: Power) {
        guard var row = open else {
            return
        }
        row.heat = Self.hotter(row.heat, power.heat)
        let at = now()
        guard at > row.start else {
            // No time has passed: the row takes the new conditions as its own.
            open = OpenRow(start: row.start, conditions: conditions, traffic: row.traffic, heat: row.heat)
            return
        }
        open = row
        guard row.conditions != conditions || at.timeIntervalSince(row.start) >= Self.rowSeconds else {
            return
        }
        let reading = traffic()
        close(row, at: at, reading: reading, power: power)
        open = OpenRow(start: at, conditions: conditions, traffic: reading, heat: power.heat)
    }

    /// The session ended: its last row ends now.
    func end(power: Power) {
        guard var row = open else {
            return
        }
        row.heat = Self.hotter(row.heat, power.heat)
        open = nil
        close(row, at: now(), reading: traffic(), power: power)
    }

    /// The log for the support bundle: its heading, then the columns and
    /// the rows, or a line saying there are none.
    func bundleLines() -> [String] {
        guard !rows.isEmpty else {
            return [Self.heading, Self.noRows]
        }
        return [Self.heading, Self.columns.joined(separator: ",")] + rows
    }

    // MARK: Rows

    private func close(_ row: OpenRow, at end: Date, reading: TrafficCounter.ByKind, power: Power) {
        let seconds = end.timeIntervalSince(row.start)
        guard seconds > 0 else {
            return
        }
        let line = Self.line(start: row.start, seconds: seconds, conditions: row.conditions,
                             power: Power(batteryPercent: power.batteryPercent, charging: power.charging,
                                          heat: row.heat),
                             traffic: reading.since(row.traffic))
        rows.append(line)
        if rows.count > Self.mostRows {
            rows.removeFirst(min(rows.count, Self.dropAtOnce + rows.count - Self.mostRows))
            rewrite()
        } else {
            append(line)
        }
    }

    /// One row: `2026-09-27T19:42:00Z,60.0,Wi-Fi,In front,Full,30,full,81,no,nominal,...`.
    static func line(start: Date, seconds: TimeInterval, conditions: Conditions, power: Power,
                     traffic: TrafficCounter.ByKind) -> String {
        var fields = [
            start.formatted(Date.ISO8601FormatStyle()),
            String(format: "%.1f", seconds),
            conditions.network,
            conditions.inFront ? "In front" : "Away",
            DataModeText.title(conditions.mode),
            conditions.request.fps.map(String.init) ?? "0",
            conditions.request.subscribes ? (conditions.request.detail == .half ? "half" : "full") : "none",
            power.batteryPercent.map(String.init) ?? "unknown",
            power.charging ? "yes" : "no",
            heatWord(power.heat),
        ]
        for kind in TrafficCounter.Kind.allCases {
            fields.append(String(traffic[kind].bytesIn))
            fields.append(String(traffic[kind].bytesOut))
        }
        return fields.joined(separator: ",")
    }

    static func heatWord(_ heat: SessionPolicy.Heat) -> String {
        switch heat {
        case .nominal:
            return "nominal"
        case .fair:
            return "fair"
        case .serious:
            return "serious"
        case .critical:
            return "critical"
        }
    }

    static func hotter(_ a: SessionPolicy.Heat, _ b: SessionPolicy.Heat) -> SessionPolicy.Heat {
        rank(b) > rank(a) ? b : a
    }

    private static func rank(_ heat: SessionPolicy.Heat) -> Int {
        switch heat {
        case .nominal:
            return 0
        case .fair:
            return 1
        case .serious:
            return 2
        case .critical:
            return 3
        }
    }

    /// The phone's battery level in percent, or nil when it does not say.
    /// Battery monitoring is switched on here, as ``ThermalAndPowerWatcher``
    /// does, since the level reads unknown without it.
    static func systemBatteryPercent() -> Int? {
        let device = UIDevice.current
        if !device.isBatteryMonitoringEnabled {
            device.isBatteryMonitoringEnabled = true
        }
        let level = device.batteryLevel
        guard level >= 0, level <= 1 else {
            return nil
        }
        return Int((level * 100).rounded())
    }

    // MARK: The file

    private func load() {
        guard let file, let text = try? String(contentsOf: file, encoding: .utf8) else {
            return
        }
        rows = text.split(separator: "\n", omittingEmptySubsequences: true).map(String.init)
        if rows.count > Self.mostRows {
            rows.removeFirst(rows.count - Self.mostRows)
            rewrite()
        }
    }

    private func append(_ line: String) {
        guard let file else {
            return
        }
        let data = Data((line + "\n").utf8)
        do {
            if let handle = try? FileHandle(forWritingTo: file) {
                defer { try? handle.close() }
                try handle.seekToEnd()
                try handle.write(contentsOf: data)
            } else {
                try FileManager.default.createDirectory(at: file.deletingLastPathComponent(),
                                                        withIntermediateDirectories: true)
                try data.write(to: file, options: .atomic)
            }
        } catch {
            Self.logger.warning("A measurement row could not be kept")
        }
    }

    private func rewrite() {
        guard let file else {
            return
        }
        do {
            try FileManager.default.createDirectory(at: file.deletingLastPathComponent(),
                                                    withIntermediateDirectories: true)
            let text = rows.isEmpty ? "" : rows.joined(separator: "\n") + "\n"
            try Data(text.utf8).write(to: file, options: .atomic)
        } catch {
            Self.logger.warning("The measurement log could not be rewritten")
        }
    }
}
