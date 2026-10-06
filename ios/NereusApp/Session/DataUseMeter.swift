// NereusSDR for iOS: counts the data a session uses, and the month's on cellular, with the 5 GB warning
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink

/// The data use counters (R-IOS-23, spec section 5.4 item 11): the bytes in
/// and out for this session, and for the calendar month on cellular and on
/// Wi-Fi. It reads the app's ``TrafficCounter`` at each ``sample(cellular:)``
/// and puts the difference since the last reading on the network the phone
/// is on now, so the app samples every few seconds and at each change of
/// network. The month's counts are kept on this phone and start again on
/// the first of each month; past ``warningBytes`` on cellular in a month
/// the warning is due once, when its switch is on.
@MainActor
final class DataUseMeter: ObservableObject {
    /// Bytes in and out.
    struct Count: Equatable, Sendable {
        var bytesIn: UInt64 = 0
        var bytesOut: UInt64 = 0

        var total: UInt64 { bytesIn &+ bytesOut }

        mutating func add(bytesIn: UInt64, bytesOut: UInt64) {
            self.bytesIn &+= bytesIn
            self.bytesOut &+= bytesOut
        }
    }

    /// App payload rates from one pair of counter samples, in bits per second.
    struct Rate: Equatable, Sendable {
        var incoming: Double
        var outgoing: Double

        static func text(_ bitsPerSecond: Double) -> String {
            guard bitsPerSecond.isFinite, bitsPerSecond >= 0 else { return "Unavailable" }
            let (scaled, unit): (Double, String) = bitsPerSecond >= 1_000_000
                ? (bitsPerSecond / 1_000_000, "Mbps")
                : bitsPerSecond >= 1_000 ? (bitsPerSecond / 1_000, "kbps") : (bitsPerSecond, "bps")
            let rounded = (scaled * 10).rounded() / 10
            return "\(String(format: rounded == rounded.rounded() ? "%.0f" : "%.1f", rounded)) \(unit)"
        }
    }

    /// The warning's threshold: 5 GB a month on cellular.
    static let warningBytes: UInt64 = 5_000_000_000

    /// This session's data, from ``sessionStarted()``.
    @Published private(set) var session = Count()
    /// This session is on cellular now.
    @Published private(set) var sessionOnCellular = false
    /// The calendar month's data on cellular and on Wi-Fi.
    @Published private(set) var monthCellular = Count()
    @Published private(set) var monthWifi = Count()
    @Published private(set) var rate: Rate?

    private let settings: PhoneSettings
    private let reading: () -> TrafficCounter.Totals
    private let now: () -> Date
    private let calendar: Calendar
    private var last: TrafficCounter.Totals
    private var lastAt: Date
    private var rateAt: Date?
    private var counting = false

    init(settings: PhoneSettings, reading: @escaping () -> TrafficCounter.Totals = { TrafficCounter.shared.reading },
         now: @escaping () -> Date = Date.init, calendar: Calendar = .current) {
        self.settings = settings
        self.reading = reading
        self.now = now
        self.calendar = calendar
        last = reading()
        lastAt = now()
        loadMonth()
    }

    /// A session starts: its count starts at nothing, and only what comes
    /// from here on is counted.
    func sessionStarted() {
        last = reading()
        lastAt = now()
        invalidateRate()
        session = Count()
        counting = true
    }

    /// The session ended: what came up to now is counted, then nothing more
    /// until the next session.
    func sessionEnded(cellular: Bool) {
        sample(cellular: cellular)
        counting = false
        invalidateRate()
    }

    /// A path change cannot attribute the previous path's sample to the new one.
    func invalidateRate() {
        rate = nil
        rateAt = nil
    }

    /// Old readings are unavailable if sampling has stopped.
    var currentRate: Rate? {
        guard let rate, let rateAt, counting else { return nil }
        let age = now().timeIntervalSince(rateAt)
        return age >= 0 && age <= 15 ? rate : nil
    }

    /// Counts the traffic since the last reading on the network the phone
    /// is on now. Returns true while the month's cellular warning is due.
    /// Sampling alone never marks a warning as shown.
    @discardableResult
    func sample(cellular: Bool) -> Bool {
        let next = reading()
        let bytesIn = next.bytesIn >= last.bytesIn ? next.bytesIn - last.bytesIn : 0
        let bytesOut = next.bytesOut >= last.bytesOut ? next.bytesOut - last.bytesOut : 0
        last = next
        let sampledAt = now()
        let elapsed = sampledAt.timeIntervalSince(lastAt)
        lastAt = sampledAt
        if counting, elapsed > 0 {
            rate = Rate(incoming: Double(bytesIn) * 8 / elapsed, outgoing: Double(bytesOut) * 8 / elapsed)
            rateAt = sampledAt
        } else {
            invalidateRate()
        }
        if sessionOnCellular != cellular {
            sessionOnCellular = cellular
        }
        rollMonthIfNeeded()
        guard counting, bytesIn > 0 || bytesOut > 0 else {
            return false
        }
        session.add(bytesIn: bytesIn, bytesOut: bytesOut)
        if cellular {
            monthCellular.add(bytesIn: bytesIn, bytesOut: bytesOut)
        } else {
            monthWifi.add(bytesIn: bytesIn, bytesOut: bytesOut)
        }
        saveMonth()
        return warningDue()
    }

    /// The month's warning remains due until its band note appears. This
    /// also lets a final sample be delivered in the next foreground session.
    func warningDue() -> Bool {
        rollMonthIfNeeded()
        return settings.cellularWarning && monthCellular.total >= Self.warningBytes
            && settings.string(Self.warnedMonthKey, default: "") != monthKey(now())
    }

    /// Records the month only after the note has appeared on the band.
    func markWarningShown() {
        guard warningDue() else { return }
        settings.storeQuietly(monthKey(now()), for: Self.warnedMonthKey)
    }

    // MARK: The month, kept on this phone

    static let monthKey = "data.month"
    static let cellularInKey = "data.month.cellularIn"
    static let cellularOutKey = "data.month.cellularOut"
    static let wifiInKey = "data.month.wifiIn"
    static let wifiOutKey = "data.month.wifiOut"
    static let warnedMonthKey = "data.month.warned"

    /// The month `date` falls in, `2026-09`, in the phone's calendar.
    func monthKey(_ date: Date) -> String {
        let parts = calendar.dateComponents([.year, .month], from: date)
        return String(format: "%04d-%02d", parts.year ?? 0, parts.month ?? 0)
    }

    private func loadMonth() {
        guard settings.string(Self.monthKey, default: "") == monthKey(now()) else {
            monthCellular = Count()
            monthWifi = Count()
            return
        }
        monthCellular = Count(bytesIn: stored(Self.cellularInKey), bytesOut: stored(Self.cellularOutKey))
        monthWifi = Count(bytesIn: stored(Self.wifiInKey), bytesOut: stored(Self.wifiOutKey))
    }

    private func rollMonthIfNeeded() {
        let key = monthKey(now())
        guard settings.string(Self.monthKey, default: "") != key else {
            return
        }
        monthCellular = Count()
        monthWifi = Count()
        saveMonth()
    }

    private func saveMonth() {
        // Kept without telling the screens that read the settings: the
        // counters change every few seconds, and only this meter reads them.
        settings.storeQuietly(monthKey(now()), for: Self.monthKey)
        settings.storeQuietly(Double(monthCellular.bytesIn), for: Self.cellularInKey)
        settings.storeQuietly(Double(monthCellular.bytesOut), for: Self.cellularOutKey)
        settings.storeQuietly(Double(monthWifi.bytesIn), for: Self.wifiInKey)
        settings.storeQuietly(Double(monthWifi.bytesOut), for: Self.wifiOutKey)
    }

    private func stored(_ key: String) -> UInt64 {
        let value = settings.double(key, default: 0)
        return value.isFinite && value > 0 ? UInt64(value) : 0
    }

    // MARK: The words

    /// A count as the page shows it: `18.6 MB`, or `1.24 GB` from a
    /// gigabyte up (decimal units, as the estimates are).
    static func text(_ bytes: UInt64) -> String {
        let value = Double(bytes)
        if value >= 1_000_000_000 {
            return String(format: "%.2f GB", value / 1_000_000_000)
        }
        return String(format: "%.1f MB", value / 1_000_000)
    }
}
