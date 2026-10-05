// NereusSDR for iOS: the Live Activity's words and number formats
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The words the lock-screen card and the island show, and how they write
/// a frequency and a length of time. The board's words (the Lock screen
/// and In another app sections, and Eight hours in).
enum ActivityWords {
    static let linkLost = "LINK LOST"
    static let lost = "Lost"
    static let offline = "Offline"
    /// The link went while this phone was keyed (the board's "Link lost while keyed").
    static let unkeyedItself = "The Core unkeyed itself."
    static let stoppedReconnecting = "Stopped reconnecting"
    static let cancel = "Cancel"
    static let reconnect = "Reconnect"
    static let unkey = "UNKEY"
    static let timeOutIn = "Time-out in "
    static let tx = "TX"
    static let backOnAir = "Back on the air."

    /// The radio on the air for someone else (parity row I16): "On the air
    /// from Radio", "On the air from JJ's Mac", or "On the air" when the
    /// Core names nobody.
    static func onAirFrom(_ holder: String) -> String {
        holder.isEmpty ? "On the air" : "On the air from \(holder)"
    }
    /// A card the app hasn't refreshed in time: the app may have stopped.
    static let noNews = "No news from the app. Open NereusSDR to check."
    /// The same while keyed: the card can't know whether it still transmits,
    /// and never says it stopped. UNKEY stays beside it.
    static let noNewsKeyed = "No news from the app. It may still be on the air."
    /// Beside the camera on a keyed card gone stale.
    static let noNewsShort = "No news"
    /// Just before iOS ends the card (spec section 5.5 item 13).
    static let eightHours = "Still listening. iOS ends this card after 8 hours; open NereusSDR to bring it back."

    /// "Unkeyed when you locked the phone. TX ran 0:42." (D24).
    static func unkeyedByLock(ran seconds: Int) -> String {
        "Unkeyed when you locked the phone. TX ran \(clock(seconds))."
    }

    /// UNKEY on the card or the island: "Unkeyed. TX ran 0:42."
    static func unkeyedHere(ran seconds: Int) -> String {
        "Unkeyed. TX ran \(clock(seconds))."
    }

    /// "Reconnecting, try 4: " before the countdown, or "...: trying now".
    static func reconnecting(attempt: Int) -> String {
        "Reconnecting, try \(attempt): "
    }

    static let tryingNow = "trying now"
    static let nextIn = "next in "

    /// "Fwd 100 W · SWR 1.15".
    static func telemetry(watts: Double, swr: Double) -> String {
        let shownWatts = Int(max(0, watts).rounded())
        return "Fwd \(shownWatts) W \u{00B7} SWR " + String(format: "%.2f", max(1, swr))
    }

    /// A round trip, "38 ms".
    static func roundTrip(_ ms: Int) -> String {
        "\(ms) ms"
    }

    /// Minutes and seconds, "0:42", "3:00", "12:05".
    static func clock(_ seconds: Int) -> String {
        let whole = max(0, seconds)
        return "\(whole / 60):" + String(format: "%02d", whole % 60)
    }

    /// The flag's frequency: megahertz, kilohertz and hertz in groups of three, `7.236.400`.
    static func frequency(_ hz: Double) -> String {
        let whole = hz.isFinite ? Int64(max(0, hz.rounded())) : 0
        let mhz = whole / 1_000_000
        let khz = (whole / 1_000) % 1_000
        let rest = whole % 1_000
        return "\(mhz)." + String(format: "%03lld", khz) + "." + String(format: "%03lld", rest)
    }

    /// Beside the camera: megahertz to the kilohertz, `7.236`.
    static func compactFrequency(_ hz: Double) -> String {
        let whole = hz.isFinite ? Int64(max(0, hz.rounded())) : 0
        return "\(whole / 1_000_000)." + String(format: "%03lld", (whole / 1_000) % 1_000)
    }
}
