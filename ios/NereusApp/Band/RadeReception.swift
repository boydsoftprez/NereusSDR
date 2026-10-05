// NereusSDR for iOS: the RADE row on a slice's flag: who is heard, whether the receiver is locked on, how well, how far off
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What a RADE slice's flag reads under its level bar (spec section 5.1
/// item 4, D9, R-IOS-11): the callsign the last end of over carried, else
/// "RADE"; a dot for whether the receiver is locked on; the signal to
/// noise; and the frequency offset. It is the desktop's short form with no
/// extra label, and VoiceOver reads it in words.
///
/// The phone shows what the Core sends and nothing more: no smoothing, no
/// timers, and the callsign clears when the Core clears it. `synced` and
/// `offsetHz` come from the Core's `radeSynced` and `radeFreqOffsetHz`, sent
/// only by a Core that offers `radeStatusVersion` 1; on an older Core they
/// are nil and the row reads the callsign and the older-Core reason, greyed.
/// The drawing follows the Core's RADE note (rade-flag-for-phone.md): locked
/// on with a reading draws `K1ABC ● 12dB +38Hz`, anything else `RADE ○ ---`.
///
/// `reason` is the Core's `radeReason`, sent only by a Core that offers
/// `radeReasonVersion` 1: why the slice is in RADE with no working decoder.
/// While it is not empty the row reads `RADE ○ off` and the Core's sentence
/// goes under it, where the desktop's flag puts it in the row's tooltip
/// (VfoWidget.cpp setRadeReason and setRadeSynced).
struct RadeReception: Equatable {
    /// The callsign the Core last decoded (`lastRadeRxCallsign`); empty until one is.
    var callsign: String
    /// The Core's signal to noise in dB (`snrDb`); nil while it has no reading.
    var snrDb: Double?
    /// Whether the receiver is locked on; nil while the Core does not say.
    var synced: Bool?
    /// The received signal's frequency offset in hertz; nil while the Core does not say.
    var offsetHz: Double?
    /// Why the slice has no working RADE decoder, as the Core words it; nil
    /// while it decodes or while the Core does not say.
    var reason: String? = nil

    /// The row's words on a Core that does not send RADE sync.
    static let olderCoreReason = "This Core does not send RADE sync. Updating the Core may help."
    /// Below this signal to noise the dot turns yellow (the desktop's 5 dB).
    static let marginalBelowDb: Double = 5
    /// The row's prefix when no callsign is decoded.
    static let noCallsignPrefix = "RADE"
    /// The mode labels that carry the row.
    static let modeLabels: Set<String> = ["RADE-U", "RADE-L"]

    /// The row's dot.
    enum Dot: Equatable {
        /// Locked on, signal to noise 5 dB or more.
        case good
        /// Locked on, signal to noise below 5 dB.
        case marginal
        /// Not locked on, or locked on with no reading.
        case hollow

        var character: String {
            self == .hollow ? "○" : "●"
        }
    }

    /// What the row shows.
    enum Row: Equatable {
        /// The Core does not send RADE sync: the prefix and the reason, greyed.
        case olderCore
        /// Prefix (callsign or "RADE"), dot, reading ("12dB" or "---"), and the offset when there is a reading and the Core sent one.
        case reading(prefix: String, dot: Dot, value: String, offset: String?)
        /// The slice has no working RADE decoder: the prefix, the hollow dot
        /// and "off", with the Core's reason under them.
        case off(prefix: String, reason: String)
    }

    /// The row's word while the slice has no working decoder (the desktop's).
    static let offText = "off"

    /// The row's first word: the callsign, else "RADE".
    var prefix: String {
        callsign.isEmpty ? Self.noCallsignPrefix : callsign
    }

    var row: Row {
        // The desktop's order: a reason wins over sync and the reading.
        if let reason, !reason.isEmpty {
            return .off(prefix: prefix, reason: reason)
        }
        guard let synced else {
            return .olderCore
        }
        guard synced, let snr = reading else {
            return .reading(prefix: prefix, dot: .hollow, value: "---", offset: nil)
        }
        let dot: Dot = snr.raw < Self.marginalBelowDb ? .marginal : .good
        return .reading(prefix: prefix, dot: dot, value: "\(snr.whole)dB",
                        offset: offset.map { ($0.raw >= 0 ? "+" : "") + "\($0.whole)Hz" })
    }

    /// The row as one line of text, as the Core's note writes it: `K1ABC ● 12dB +38Hz`.
    var text: String {
        switch row {
        case .olderCore:
            return "\(prefix) \(Self.olderCoreReason)"
        case .off(let prefix, let reason):
            return [prefix, Dot.hollow.character, Self.offText, reason].joined(separator: " ")
        case .reading(let prefix, let dot, let value, let offset):
            return [prefix, dot.character, value, offset].compactMap { $0 }.joined(separator: " ")
        }
    }

    /// The row takes more than its one line: the older Core's reason, or
    /// the Core's reason for no decoder, wrapped under it.
    var wraps: Bool {
        switch row {
        case .olderCore, .off:
            return true
        case .reading:
            return false
        }
    }

    /// The row in words, for VoiceOver.
    var spokenText: String {
        let lead = "RADE reception: "
        let who = callsign.isEmpty ? "no callsign decoded" : callsign
        if case .off(_, let reason) = row {
            return lead + who + ", off. " + reason
        }
        guard let synced else {
            return lead + who + ". " + Self.olderCoreReason
        }
        guard synced else {
            return lead + who + ", not locked on, no signal to noise reading"
        }
        guard let snr = reading else {
            return lead + who + ", locked on, no signal to noise reading"
        }
        var words = lead + who + ", locked on, signal to noise \(snr.whole) dB"
        if snr.raw < Self.marginalBelowDb {
            words += ", marginal"
        }
        if let offset {
            words += ", \(abs(offset.whole)) hertz " + (offset.raw >= 0 ? "high" : "low")
        }
        return words
    }

    /// The signal to noise and its whole dB, cut toward zero as the desktop does.
    private var reading: (raw: Double, whole: Int)? {
        guard let snrDb, let whole = Self.whole(snrDb) else {
            return nil
        }
        return (snrDb, whole)
    }

    /// The offset and its whole hertz, cut toward zero. As on the desktop the
    /// sign comes from the value before the cut: "+" when it is zero or above,
    /// nothing otherwise, so -0.4 reads "0Hz" and 0.4 reads "+0Hz".
    private var offset: (raw: Double, whole: Int)? {
        guard let offsetHz, let whole = Self.whole(offsetHz) else {
            return nil
        }
        return (offsetHz, whole)
    }

    private static func whole(_ value: Double) -> Int? {
        guard value.isFinite, abs(value) < 1e9 else {
            return nil
        }
        return Int(value)
    }
}
