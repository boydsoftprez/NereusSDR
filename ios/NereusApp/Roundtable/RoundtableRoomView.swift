// NereusSDR for iOS: one Roundtable room in the sheet: the talker, readings, earlier turns and messages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the room as the desktop's panel
// shows it, on the phone. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI
import UIKit

/// Roundtable's words and numbers, as the desktop's panel writes them
/// (src/gui/roundtable/RoundtablePanel.cpp).
enum RoundtableText {
    /// S9 is -73 dBm; 6 dB is one S unit.
    static let s9Dbm = -73.0
    static let dbPerSUnit = 6.0
    /// A reading older than this is stale: grey, its bar stopped, its age amber.
    static let staleReadingMs: Int64 = 20_000
    /// How long a new message or a mention stays on screen before it is seen.
    static let onScreenMs: Int64 = 2_000
    /// A message's limit in characters, its counter from 400, and its lines.
    static let messageLimit = 500
    static let counterFrom = 400
    static let messageLines = 10

    static func sText(_ dbm: Double) -> String {
        let over = Int((dbm - s9Dbm).rounded())
        if over < 1 {
            let units = Int((9.0 - (s9Dbm - dbm) / dbPerSUnit).rounded())
            return "S\(min(9, max(0, units)))"
        }
        return "S9 +\(over)"
    }

    /// The reading's bar, 0.02 to 1.
    static func sFraction(_ dbm: Double) -> Double {
        min(1, max(0.02, (dbm + 127) / 100))
    }

    static func offsetText(_ hz: Int64) -> String {
        if abs(hz) < 50 {
            return "on"
        }
        let rounded = abs((hz + (hz > 0 ? 50 : -50)) / 100 * 100)
        return (hz > 0 ? "+" : "-") + "\(rounded) Hz"
    }

    static func ageText(_ ms: Int64) -> String {
        let seconds = max(0, ms / 1000)
        if seconds < 60 {
            return "\(seconds) s"
        }
        if seconds < 3600 {
            return "\(seconds / 60) min"
        }
        return "\(seconds / 3600) h"
    }

    static func clockText(_ ms: Int64) -> String {
        let seconds = max(0, ms / 1000)
        return "\(seconds / 60):" + String(format: "%02d", seconds % 60)
    }

    static func utcText(_ unixMs: Int64) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(identifier: "UTC")
        formatter.dateFormat = "HH:mm"
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(unixMs) / 1000))
    }

    static func withNth(_ callsign: String, _ nth: Int64) -> String {
        nth > 1 ? "\(callsign) (\(nth))" : callsign
    }

    /// 7185000 reads "7.185.000".
    static func headerFrequency(_ hz: Int64) -> String {
        let whole = max(0, hz)
        return "\(whole / 1_000_000)." + String(format: "%03d", (whole / 1000) % 1000) + "."
            + String(format: "%03d", whole % 1000)
    }

    /// 7249000 reads "7.249".
    static func shortFrequency(_ hz: Int64) -> String {
        let whole = max(0, hz)
        return "\(whole / 1_000_000)." + String(format: "%03d", (whole / 1000) % 1000)
    }

    static func modeLabel(_ family: String) -> String {
        family == "fdvl" || family == "fdvu" ? "FreeDV" : family.uppercased()
    }

    /// What the connection line says, and its colour's meaning.
    enum Tone: Equatable {
        case live
        case warn
        case you
        case dim
    }

    /// The connection line under the room's header (spec 11.1, U4).
    static func connectionLine(link: RoundtableModel.LinkState, newestReadingMs: Int64?, linkSince: Int64,
                               now: Int64) -> (text: String, tone: Tone) {
        switch link {
        case .live:
            if let newest = newestReadingMs, newest > 0 {
                return ("Live \u{00B7} last reading \(ageText(now - newest)) ago", .live)
            }
            return ("Live", .live)
        case .reconnecting:
            return ("Reconnecting \u{00B7} nothing new for \(ageText(now - linkSince))", .warn)
        case .offline:
            return (linkSince > 0 ? "Offline since \(utcText(linkSince)) UTC" : "Offline", .warn)
        case .connecting:
            return ("Connecting", .dim)
        case .hidden:
            return ("You are hidden", .you)
        case .off:
            return ("Roundtable is off", .dim)
        }
    }

    /// A message names `callsign`: its words hold the callsign as a word, any case.
    static func names(_ text: String?, callsign: String) -> Bool {
        let call = RoundtableModel.baseCallsign(callsign)
        guard let text, !call.isEmpty else {
            return false
        }
        let pattern = "(?<![A-Za-z0-9/])" + NSRegularExpression.escapedPattern(for: call) + "(?![A-Za-z0-9])"
        return text.range(of: pattern, options: [.regularExpression, .caseInsensitive]) != nil
    }

    /// A message that names you: shown, not yours, and naming your callsign.
    static func isMention(_ message: RoundtableModel.Message, mine: Bool, callsign: String) -> Bool {
        message.state == "shown" && !mine && names(message.text, callsign: callsign)
    }

    /// The mentions not yet read: the newest `count` messages that name you.
    static func unreadMentionIds(_ messages: [RoundtableModel.Message], count: Int, myRef: String?,
                                 callsign: String) -> [Int64] {
        guard count > 0 else {
            return []
        }
        let mentions = messages.filter { isMention($0, mine: isMine($0, myRef: myRef, callsign: callsign),
                                                   callsign: callsign) }
        return Array(mentions.suffix(count).map(\.id))
    }

    static func isMine(_ message: RoundtableModel.Message, myRef: String?, callsign: String) -> Bool {
        if let myRef, let ref = message.ref, !myRef.isEmpty {
            return ref == myRef
        }
        return !callsign.isEmpty
            && RoundtableModel.baseCallsign(message.callsign) == RoundtableModel.baseCallsign(callsign)
    }
}

/// The room's reading rows, as the desktop's panel orders them.
enum RoundtableReadings {
    struct Row: Equatable, Identifiable {
        var member: RoundtableModel.Member
        /// Your own reading while hidden: shown to you only, NOT SENT.
        var unsent = false
        var id: String { member.ref.isEmpty ? "own" : member.ref }
    }

    /// The section's title: whose readings these are.
    static func title(room: RoundtableModel.Room, keyed: Bool) -> String {
        let turn = room.now.first
        if keyed {
            return "READINGS OF YOU"
        }
        guard let turn else {
            return "IN THE ROOM"
        }
        if turn.kind == "doubled" {
            return "READINGS OF BOTH"
        }
        guard turn.kind != "unknown", let first = turn.talkers.first else {
            return "READINGS OF THE TALKER"
        }
        return "READINGS OF " + RoundtableText.withNth(first.callsign, first.nth)
    }

    /// Every member with a reading, less the talkers (and you while keyed);
    /// nearest first, then those with no distance by callsign. Hidden, your
    /// own reading joins them, marked NOT SENT.
    static func rows(room: RoundtableModel.Room, keyed: Bool, ownUnsent: RoundtableModel.Member? = nil) -> [Row] {
        let talkers = Set((room.now.first?.talkers ?? []).compactMap(\.ref))
        var rows = room.members.filter { member in
            !talkers.contains(member.ref) && !(keyed && member.you) && member.reading != nil
        }.map { Row(member: $0) }
        if let own = ownUnsent, !keyed, !rows.contains(where: \.member.you) {
            rows.append(Row(member: own, unsent: true))
        }
        return rows.sorted { a, b in
            switch (a.member.km, b.member.km) {
            case let (x?, y?) where x != y:
                return x < y
            case (.some, nil):
                return true
            case (nil, .some):
                return false
            default:
                return a.member.callsign < b.member.callsign
            }
        }
    }
}

/// One room in the Roundtable sheet (spec 11.1 and 11.3, D17): the hidden
/// banner and the read bar, then the talker (or, keyed, the TX line), the
/// readings with Offset and Age, and at full height the earlier turns
/// with Name and the messages, in one scroll. Its pills ("@1 A message
/// names you", "N new") sit above the box, which the sheet pins.
struct RoundtableRoomView: View {
    @ObservedObject var roundtable: RoundtableModel
    let room: RoundtableModel.Room
    /// Full height: the earlier turns and the messages follow the readings.
    let full: Bool
    /// This phone is transmitting.
    let keyed: Bool
    /// The keyed clock's start.
    var keyedSeconds: Int = 0
    /// A slice of this phone is tuned to the room: it may post and name.
    let tunedHere: Bool
    /// The station's callsign, when the room has no row of you.
    let stationCallsign: String
    /// This phone's own reading of the room (the slice's level), for the NOT SENT row while hidden.
    var ownDbm: Double?
    /// The active slice's letter, for "Tune A here".
    var sliceLetter = "A"
    /// Keyed with a dial on, the box stepped aside: its draft is kept.
    var draftKept = false
    var tuneHere: (() -> Void)?
    /// Asks the sheet for full height (a pill's jump).
    var expand: (() -> Void)?

    @State private var naming: Int64?
    @State private var nameText = ""
    /// Messages there when the room first showed: not new.
    @State private var firstSeen: Set<Int64>?
    /// Messages on screen, since when (uptime ms).
    @State private var onScreen: [Int64: Int64] = [:]
    /// Messages seen (on screen for 2 s).
    @State private var seen: Set<Int64> = []
    @State private var atEnd = true
    @State private var waiting = 0
    @State private var lastCount = 0

    private static let endId = "roundtableEnd"

    private var you: RoundtableModel.Member? { room.members.first(where: \.you) }
    private var callsign: String { you?.callsign ?? stationCallsign }
    private var away: Bool { you?.away ?? false }
    private var live: Bool { roundtable.linkState == .live }

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { context in
            let now = Int64(context.date.timeIntervalSince1970 * 1000)
            ScrollViewReader { reader in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 0) {
                        if roundtable.linkState == .hidden {
                            hiddenBanner
                        }
                        if roundtable.readingRoomId != nil {
                            readBar
                        }
                        if keyed {
                            txLine
                        } else {
                            talker(now: now)
                        }
                        readings(now: now)
                        if full {
                            earlierTurns
                            messages(now: now)
                        }
                        Color.clear.frame(height: 1).id(Self.endId)
                            .onAppear { atEnd = true; waiting = 0 }
                            .onDisappear { atEnd = false }
                    }
                }
                .scrollIndicators(.hidden)
                .overlay(alignment: .bottom) {
                    // Keyed, the readings of you fill the sheet; the pills wait for the key to come off.
                    if !keyed {
                        pills(reader: reader)
                    }
                }
                .onChange(of: room.messages.count) { _, count in
                    arrived(count: count, reader: reader)
                }
                .onAppear {
                    if firstSeen == nil {
                        firstSeen = Set(room.messages.map(\.id))
                        lastCount = room.messages.count
                    }
                }
            }
            .onChange(of: context.date) { _, date in
                checkOnScreen(nowUptime: Int64(ProcessInfo.processInfo.systemUptime * 1000))
                _ = date
            }
        }
        .accessibilityIdentifier("roundtableRoom")
    }

    // MARK: Banner and read bar

    private var hiddenBanner: some View {
        HStack(alignment: .center, spacing: 10) {
            (Text("You are hidden. ").bold().foregroundColor(RoundtableColours.bright)
                + Text("You see everything; nobody sees your station or your readings.")
                .foregroundColor(RoundtableColours.text))
                .font(.system(size: 12))
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 4)
            Button("Go live") {
                Task { await roundtable.setHidden(false) }
            }
            .buttonStyle(RoundtableButtonStyle(main: true))
            .accessibilityIdentifier("roundtableGoLive")
        }
        .padding(10)
        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(RoundtableColours.rule, lineWidth: 1))
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
    }

    private var readBar: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Text("Reading \(RoundtableText.shortFrequency(room.hz))")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(RoundtableColours.bright)
                Spacer(minLength: 4)
                if let tuneHere {
                    Button("Tune \(sliceLetter) here") { tuneHere() }
                        .buttonStyle(RoundtableButtonStyle(main: true))
                        .accessibilityIdentifier("roundtableTuneHere")
                }
            }
            Button("Back to my slice") {
                Task { await roundtable.backToSlice() }
            }
            .buttonStyle(RoundtableButtonStyle(main: false))
            .accessibilityIdentifier("roundtableBackToSlice")
        }
        .padding(10)
        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: 6))
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
    }

    // MARK: Talker

    private var openTurn: RoundtableModel.Turn? { room.now.first }

    private func gridOf(_ ref: String?) -> String {
        room.members.first { $0.ref == ref }?.grid ?? ""
    }

    @ViewBuilder
    private func talker(now: Int64) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            if let turn = openTurn {
                let clock = RoundtableText.clockText(now - turn.start)
                if turn.kind == "doubled" {
                    caption("TWO AT ONCE")
                    HStack(alignment: .firstTextBaseline, spacing: 6) {
                        RoundtableGlyphView(glyph: .doubled, size: 10)
                        Text(turn.talkers.map { RoundtableText.withNth($0.callsign, $0.nth) }.joined(separator: " + "))
                            .font(.system(size: 15, weight: .bold).monospaced())
                            .foregroundStyle(RoundtableColours.doubled)
                            .lineLimit(2)
                        Spacer(minLength: 4)
                        Text("DOUBLED").font(.system(size: 11, weight: .bold)).foregroundStyle(RoundtableColours.bright)
                            .padding(.horizontal, 4)
                            .background(RoundtableColours.doubled.opacity(0.35), in: RoundedRectangle(cornerRadius: 2))
                    }
                    small("for \(clock) \u{00B7} each reading is both signals together")
                } else if turn.kind == "unknown" {
                    HStack(alignment: .firstTextBaseline, spacing: 6) {
                        RoundtableGlyphView(glyph: .unknown, size: 10)
                        Text("Unknown talker").font(.system(size: 14, weight: .bold))
                            .foregroundStyle(RoundtableColours.unknown)
                        Spacer(minLength: 4)
                        Text("keyed \(clock)").font(.system(size: 12, weight: .semibold).monospacedDigit())
                            .foregroundStyle(RoundtableColours.text)
                    }
                    small("not on NereusSDR \u{00B7} on air \(clock)")
                    if !turn.names.isEmpty {
                        small("Named " + turn.names.map { "\($0.name) by \($0.count)" }.joined(separator: ", "))
                    }
                    nameEditor(turn: turn, caption: "NAME THIS TURN")
                } else {
                    let first = turn.talkers.first
                    HStack(alignment: .firstTextBaseline, spacing: 6) {
                        RoundtableGlyphView(glyph: .talking, size: 10)
                        Text(first.map { RoundtableText.withNth($0.callsign, $0.nth) } ?? "Unknown")
                            .font(.system(size: 15, weight: .bold).monospaced())
                            .foregroundStyle(RoundtableColours.bright)
                            .lineLimit(1)
                        Text(gridOf(first?.ref)).font(.system(size: 11)).foregroundStyle(RoundtableColours.dim)
                        Spacer(minLength: 4)
                        Text("talking \(clock)").font(.system(size: 12, weight: .semibold).monospacedDigit())
                            .foregroundStyle(RoundtableColours.text)
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("roundtableTalker")
                }
            } else {
                caption("QUIET")
                Text("Nobody is talking").font(.system(size: 14, weight: .bold))
                    .foregroundStyle(RoundtableColours.dim)
                if let last = room.earlier.first {
                    small(lastTurnText(last, now: now))
                }
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .overlay(alignment: .bottom) { rule }
    }

    private func lastTurnText(_ last: RoundtableModel.Turn, now: Int64) -> String {
        let ago = RoundtableText.ageText(now - (last.end ?? now))
        if RoundtableFlagLine.turnIsYours(room, last, open: false) {
            return "Last turn was yours, \(ago) ago"
        }
        if let first = last.talkers.first, last.kind != "unknown" {
            return "Last turn by \(RoundtableText.withNth(first.callsign, first.nth)), \(ago) ago"
        }
        return "Last turn by an unknown talker, \(ago) ago"
    }

    /// The TX line while keyed (U3): TX, "You · 9 hear you", the clock and
    /// the frequency.
    private var txLine: some View {
        let hearing = room.members.filter { !$0.you && ($0.reading?.present ?? false) }.count
        return HStack(spacing: 8) {
            Text("TX").font(.system(size: 11, weight: .bold)).foregroundStyle(.white)
                .padding(.horizontal, 4)
                .background(ChromeColours.txRed, in: RoundedRectangle(cornerRadius: 2))
            Text("You \u{00B7} \(hearing) hear you").font(.system(size: 12, weight: .bold))
                .foregroundStyle(RoundtableColours.you)
            Text(PttButton.clock(seconds: keyedSeconds)).font(.system(size: 12).monospacedDigit())
                .foregroundStyle(RoundtableColours.text)
            if draftKept {
                tag("Draft kept").accessibilityIdentifier("roundtableDraftKept")
            }
            Spacer(minLength: 4)
            Text(RoundtableText.headerFrequency(room.hz)).font(.system(size: 12).monospaced())
                .foregroundStyle(RoundtableColours.text)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(ChromeColours.txRed.opacity(0.14))
        .overlay(alignment: .bottom) { rule }
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("roundtableTxLine")
    }

    // MARK: Readings

    @ViewBuilder
    private func readings(now: Int64) -> some View {
        let own = ownUnsent
        let rows = RoundtableReadings.rows(room: room, keyed: keyed, ownUnsent: own)
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                caption(RoundtableReadings.title(room: room, keyed: keyed))
                if !rows.isEmpty {
                    small(rows.first?.member.km != nil ? "nearest first" : "by callsign")
                }
            }
            .padding(.horizontal, 12)
            .padding(.top, 8)
            .padding(.bottom, 4)
            if rows.isEmpty {
                Text(room.members.contains { !$0.you }
                     ? room.members.filter { !$0.you }.map { RoundtableText.withNth($0.callsign, $0.nth) }
                        .joined(separator: ", ")
                     : "Nobody else here yet. Your readings of the next talker will appear here.")
                    .font(.system(size: 12))
                    .foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 12)
                    .padding(.bottom, 8)
            } else if keyed {
                // Every station's reading of you, two to a line.
                let pairs = stride(from: 0, to: rows.count, by: 2).map { Array(rows[$0..<min($0 + 2, rows.count)]) }
                ForEach(pairs.indices, id: \.self) { index in
                    HStack(spacing: 12) {
                        ForEach(pairs[index]) { row in
                            compactRow(row, now: now)
                        }
                        if pairs[index].count == 1 {
                            Spacer().frame(maxWidth: .infinity)
                        }
                    }
                    .padding(.horizontal, 12)
                    .frame(height: 22)
                }
            } else {
                header
                ForEach(rows) { row in
                    readingRow(row, now: now)
                }
            }
            if rows.contains(where: \.member.away) {
                small("AWAY: nobody at the radio. Its readings still count, but it can't post or name turns.")
                    .padding(.horizontal, 12)
                    .padding(.vertical, 4)
            }
        }
        .padding(.bottom, 6)
        .overlay(alignment: .bottom) { rule }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableReadings")
    }

    /// Hidden: your own reading of the talker, shown to you only.
    private var ownUnsent: RoundtableModel.Member? {
        guard roundtable.linkState == .hidden, tunedHere, !keyed, let ownDbm, openTurn != nil else {
            return nil
        }
        return RoundtableModel.Member(ref: "", callsign: callsign.isEmpty ? "You" : callsign, nth: 1, grid: nil,
                                      km: nil, offsetHz: 0, joining: false, away: false, keyed: false,
                                      software: "NereusSDR", softwareVersion: "", you: true,
                                      reading: .init(dbm: ownDbm, t: Int64(Date().timeIntervalSince1970 * 1000),
                                                     present: true, turn: nil))
    }

    private var header: some View {
        HStack(spacing: 6) {
            Text("Station").frame(maxWidth: .infinity, alignment: .leading)
            Text("km").frame(width: 46, alignment: .trailing)
            Text("Offset").frame(width: 52, alignment: .trailing)
            Text("Reading").frame(width: 92, alignment: .trailing)
            Text("Age").frame(width: 34, alignment: .trailing)
        }
        .font(.system(size: 11))
        .foregroundStyle(RoundtableColours.dim2)
        .padding(.horizontal, 12)
        .frame(height: 18)
    }

    private func readingRow(_ row: RoundtableReadings.Row, now: Int64) -> some View {
        let member = row.member
        let reading = member.reading
        let age = reading.map { now - $0.t } ?? 0
        let stale = age > RoundtableText.staleReadingMs
        let tint: Color = stale ? RoundtableColours.stale : member.you ? RoundtableColours.you : RoundtableColours.text
        return HStack(spacing: 6) {
            HStack(spacing: 4) {
                if member.you {
                    Circle().fill(RoundtableColours.you).frame(width: 6, height: 6)
                }
                Text(member.you ? "You" : RoundtableText.withNth(member.callsign, member.nth))
                    .font(.system(size: 12, weight: .bold).monospaced())
                    .foregroundStyle(tint)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                if member.away {
                    tag("AWAY")
                }
                if row.unsent {
                    tag("NOT SENT")
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Text(member.km.map { $0.formatted(.number.grouping(.automatic)) } ?? "")
                .frame(width: 46, alignment: .trailing)
            Text(RoundtableText.offsetText(member.offsetHz))
                .frame(width: 52, alignment: .trailing)
            HStack(spacing: 5) {
                bar(reading?.dbm, stale: stale, you: member.you)
                Text(reading.map { RoundtableText.sText($0.dbm) } ?? "")
                    .font(.system(size: 12, weight: .bold).monospacedDigit())
                    .foregroundStyle(tint)
                    .lineLimit(1)
                    .frame(width: 46, alignment: .trailing)
            }
            .frame(width: 92, alignment: .trailing)
            Text(RoundtableText.ageText(age))
                .font(.system(size: 11, weight: stale ? .bold : .regular).monospacedDigit())
                .foregroundStyle(stale ? RoundtableColours.unknown : RoundtableColours.dim)
                .frame(width: 34, alignment: .trailing)
        }
        .font(.system(size: 12).monospacedDigit())
        .foregroundStyle(stale ? RoundtableColours.stale : RoundtableColours.text)
        .padding(.horizontal, 12)
        .frame(height: 24)
        .background(member.you ? RoundtableColours.youRow : .clear)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("roundtableReading-\(member.you ? "You" : member.callsign)")
    }

    private func compactRow(_ row: RoundtableReadings.Row, now: Int64) -> some View {
        let member = row.member
        let stale = (member.reading.map { now - $0.t } ?? 0) > RoundtableText.staleReadingMs
        return HStack(spacing: 4) {
            Text(RoundtableText.withNth(member.callsign, member.nth))
                .font(.system(size: 12, weight: .bold).monospaced())
                .lineLimit(1)
                .minimumScaleFactor(0.8)
            if member.away {
                tag("AWAY")
            }
            Spacer(minLength: 2)
            bar(member.reading?.dbm, stale: stale, you: false).frame(width: 24)
            Text(member.reading.map { RoundtableText.sText($0.dbm) } ?? "")
                .font(.system(size: 12, weight: .bold).monospacedDigit())
                .frame(width: 44, alignment: .trailing)
        }
        .foregroundStyle(stale ? RoundtableColours.stale : RoundtableColours.text)
        .frame(maxWidth: .infinity)
        .accessibilityElement(children: .combine)
    }

    private func bar(_ dbm: Double?, stale: Bool, you: Bool) -> some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.white.opacity(0.1))
                Capsule()
                    .fill(stale ? RoundtableColours.stale : you ? RoundtableColours.you : RoundtableColours.talking)
                    .frame(width: proxy.size.width * RoundtableText.sFraction(dbm ?? -127))
            }
        }
        .frame(height: 4)
        .accessibilityHidden(true)
    }

    // MARK: Earlier turns

    @ViewBuilder
    private var earlierTurns: some View {
        if !room.earlier.isEmpty {
            VStack(alignment: .leading, spacing: 0) {
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    caption("EARLIER TURNS")
                    small("last 30 minutes")
                }
                .padding(.horizontal, 12)
                .padding(.top, 8)
                .padding(.bottom, 2)
                ForEach(room.earlier) { turn in
                    earlierRow(turn)
                    if naming == turn.id {
                        nameEditor(turn: turn, caption: nil)
                            .padding(.horizontal, 12)
                            .padding(.bottom, 6)
                    }
                }
            }
            .padding(.bottom, 6)
            .overlay(alignment: .bottom) { rule }
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("roundtableEarlier")
        }
    }

    private func earlierRow(_ turn: RoundtableModel.Turn) -> some View {
        let yours = RoundtableFlagLine.turnIsYours(room, turn, open: false)
        let unknown = turn.kind == "unknown" || turn.talkers.isEmpty
        let who: String = yours ? "You"
            : turn.kind == "doubled" ? turn.talkers.map(\.callsign).joined(separator: " + ")
            : unknown ? "Unknown" : RoundtableText.withNth(turn.talkers[0].callsign, turn.talkers[0].nth)
        let glyph: RoundtableFlagLine.Glyph = yours ? .you : turn.kind == "doubled" ? .doubled
            : unknown ? .unknown : .talking
        let length = RoundtableText.clockText((turn.end ?? turn.start) - turn.start)
        return HStack(spacing: 6) {
            RoundtableGlyphView(glyph: glyph, size: 8)
            Text(who)
                .font(.system(size: 12, weight: .semibold).monospaced())
                .foregroundStyle(yours ? RoundtableColours.you : unknown ? RoundtableColours.unknown
                                 : RoundtableColours.text)
                .lineLimit(1)
            if !turn.names.isEmpty {
                Text("named " + turn.names.map { $0.count > 1 ? "\($0.name) (\($0.count))" : $0.name }
                    .joined(separator: ", "))
                    .font(.system(size: 11)).foregroundStyle(RoundtableColours.dim).lineLimit(1)
            }
            if unknown && !yours {
                Button("Name") {
                    nameText = ""
                    naming = naming == turn.id ? nil : turn.id
                }
                .buttonStyle(RoundtableButtonStyle(main: false))
                .disabled(!canName)
                .accessibilityIdentifier("roundtableName-\(turn.id)")
            }
            Spacer(minLength: 4)
            Text("\(length) \u{00B7} \(turn.heard) heard  \(RoundtableText.utcText(turn.start))")
                .font(.system(size: 11).monospacedDigit())
                .foregroundStyle(RoundtableColours.dim)
                .lineLimit(1)
        }
        .padding(.horizontal, 12)
        .frame(minHeight: 28)
    }

    private var canName: Bool { tunedHere && live && !away }

    /// The names to offer: the last day's on this frequency, not your own, up to three.
    private var suggestions: [String] {
        let mine = RoundtableModel.baseCallsign(callsign)
        return Array(roundtable.recentNames(room.id).filter { RoundtableModel.baseCallsign($0) != mine }.prefix(3))
    }

    @ViewBuilder
    private func nameEditor(turn: RoundtableModel.Turn, caption title: String?) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            if let title {
                caption(title)
            }
            HStack(spacing: 6) {
                ForEach(suggestions, id: \.self) { suggestion in
                    Button(suggestion) { submitName(turn.id, suggestion) }
                        .buttonStyle(RoundtableButtonStyle(main: false))
                        .disabled(!canName)
                }
                TextField("", text: $nameText, prompt: Text("Callsign").foregroundColor(RoundtableColours.dim2))
                    .font(.system(size: 12).monospaced())
                    .textInputAutocapitalization(.characters)
                    .autocorrectionDisabled()
                    .foregroundStyle(RoundtableColours.text)
                    .padding(.horizontal, 6)
                    .frame(height: 30)
                    .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(RoundtableColours.fieldEdge, lineWidth: 1))
                    .onChange(of: nameText) { _, text in
                        if text.count > 16 { nameText = String(text.prefix(16)) }
                    }
                    .disabled(!canName)
                Button("Name") { submitName(turn.id, nameText) }
                    .buttonStyle(RoundtableButtonStyle(main: true))
                    .disabled(!canName || nameText.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
    }

    private func submitName(_ turnId: Int64, _ name: String) {
        let trimmed = name.trimmingCharacters(in: .whitespaces).uppercased()
        guard !trimmed.isEmpty else {
            return
        }
        naming = nil
        nameText = ""
        Task { await roundtable.name(turn: String(turnId), as: trimmed) }
    }

    // MARK: Messages

    private var myRef: String? { you?.ref }

    private var unreadMentions: [Int64] {
        RoundtableText.unreadMentionIds(room.messages, count: roundtable.mentionCounts[room.id] ?? 0, myRef: myRef,
                                        callsign: callsign)
    }

    private func isNew(_ message: RoundtableModel.Message) -> Bool {
        guard let firstSeen else {
            return false
        }
        return !firstSeen.contains(message.id) && !seen.contains(message.id)
    }

    @ViewBuilder
    private func messages(now: Int64) -> some View {
        let fresh = room.messages.filter(isNew).count
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                caption("MESSAGES")
                small(fresh > 0 ? "\(room.messages.count), \(fresh) new" : "\(room.messages.count)")
            }
            .padding(.horizontal, 12)
            .padding(.top, 8)
            .padding(.bottom, 2)
            if room.messages.isEmpty {
                small("No messages on \(RoundtableText.headerFrequency(room.hz)) so far.")
                    .padding(.horizontal, 12)
                    .padding(.bottom, 8)
            }
            ForEach(room.messages) { message in
                messageRow(message)
                    .id(message.id)
                    .onAppear { onScreen[message.id] = Int64(ProcessInfo.processInfo.systemUptime * 1000) }
                    .onDisappear { onScreen[message.id] = nil }
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableMessages")
    }

    private func messageRow(_ message: RoundtableModel.Message) -> some View {
        let mine = RoundtableText.isMine(message, myRef: myRef, callsign: callsign)
        let mention = RoundtableText.isMention(message, mine: mine, callsign: callsign)
        let fresh = isNew(message)
        let sender = room.members.first { $0.ref == message.ref }
        return VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: 6) {
                Text(RoundtableText.withNth(message.callsign, message.nth))
                    .font(.system(size: 12, weight: .bold).monospaced())
                    .foregroundStyle(mine ? RoundtableColours.you : RoundtableColours.bright)
                if let grid = sender?.grid, !grid.isEmpty {
                    Text(grid).font(.system(size: 11)).foregroundStyle(RoundtableColours.dim)
                }
                if let dbm = message.dbm {
                    tag(RoundtableText.sText(dbm))
                }
                if fresh {
                    Text("new").font(.system(size: 11, weight: .bold)).foregroundStyle(RoundtableColours.accent)
                }
                Spacer(minLength: 4)
                Text(RoundtableText.utcText(message.t)).font(.system(size: 11).monospacedDigit())
                    .foregroundStyle(RoundtableColours.dim)
                Menu {
                    if mine {
                        Button("Delete", role: .destructive) {
                            Task { await roundtable.delete(String(message.id)) }
                        }
                    } else {
                        Button("Mute \(message.callsign)") {
                            Task { await roundtable.mute(message.callsign) }
                        }
                        Menu("Report") {
                            ForEach([("Spam", "spam"), ("Abuse", "abuse"), ("Off topic", "offTopic"),
                                     ("Other", "other")], id: \.1) { label, reason in
                                Button(label) {
                                    Task { await roundtable.report(kind: "message", target: String(message.id),
                                                                   reason: reason) }
                                }
                            }
                        }
                        Button("Copy") { UIPasteboard.general.string = message.text ?? "" }
                    }
                } label: {
                    Text("\u{22EF}").font(.system(size: 14, weight: .bold)).foregroundStyle(RoundtableColours.dim)
                        .frame(width: 32, height: 24)
                        .contentShape(Rectangle())
                }
                .accessibilityLabel(mine ? "Delete" : "Mute, report or copy")
            }
            Text(messageText(message))
                .font(.system(size: 12))
                .foregroundStyle(message.state == "shown" ? RoundtableColours.text : RoundtableColours.dim2)
                .fixedSize(horizontal: false, vertical: true)
                .textSelection(.enabled)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(mention ? RoundtableColours.you.opacity(0.06) : .clear)
        .overlay(alignment: .leading) {
            if mention || fresh {
                Rectangle().fill(mention ? RoundtableColours.you : RoundtableColours.accent).frame(width: 3)
            }
        }
        .overlay(alignment: .bottom) { rule }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableMessage-\(message.id)")
    }

    private func messageText(_ message: RoundtableModel.Message) -> String {
        switch message.state {
        case "removed":
            return "Message removed"
        case "deleted":
            return "Deleted by sender"
        case "expired":
            return "Message removed after a year"
        default:
            return message.text ?? ""
        }
    }

    // MARK: Pills and arrivals

    @ViewBuilder
    private func pills(reader: ScrollViewProxy) -> some View {
        let mentions = unreadMentions
        VStack(spacing: 6) {
            if let first = mentions.first {
                pill("@\(mentions.count) A message names you \u{2193}", identifier: "roundtableMentionPill") {
                    jump(to: first, reader: reader)
                }
            } else if waiting > 0, !atEnd, full {
                pill("\(waiting) new \u{2193}", identifier: "roundtableNewPill") {
                    withAnimation { reader.scrollTo(Self.endId, anchor: .bottom) }
                    waiting = 0
                }
            }
        }
        .padding(.bottom, 8)
    }

    private func pill(_ text: String, identifier: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(text)
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(Color.white)
                .padding(.horizontal, 12)
                .frame(height: 28)
                .background(RoundtableColours.mainButton, in: Capsule())
                .overlay(Capsule().strokeBorder(RoundtableColours.accent, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier(identifier)
    }

    private func jump(to id: Int64, reader: ScrollViewProxy) {
        if full {
            withAnimation { reader.scrollTo(id, anchor: .center) }
        } else {
            expand?()
            Task { @MainActor in
                try? await Task.sleep(for: .milliseconds(300))
                withAnimation { reader.scrollTo(id, anchor: .center) }
            }
        }
    }

    /// Arrivals: at the end the view follows; further up they wait below and the pill counts them.
    private func arrived(count: Int, reader: ScrollViewProxy) {
        let added = count - lastCount
        lastCount = count
        guard added > 0, full else {
            return
        }
        if atEnd {
            withAnimation { reader.scrollTo(Self.endId, anchor: .bottom) }
        } else {
            waiting += added
        }
    }

    /// Once a message has been on screen for 2 s it is no longer new, and
    /// an unread mention is marked read, on every device.
    private func checkOnScreen(nowUptime: Int64) {
        let mentions = Set(unreadMentions)
        var newestMention: Int64?
        for (id, since) in onScreen where nowUptime - since >= RoundtableText.onScreenMs {
            if !seen.contains(id) {
                seen.insert(id)
            }
            if mentions.contains(id), let message = room.messages.first(where: { $0.id == id }) {
                newestMention = max(newestMention ?? 0, message.t)
            }
        }
        if let newestMention {
            Task { await roundtable.markMentionRead(upToMs: newestMention) }
        }
    }

    // MARK: Pieces

    private var rule: some View {
        Rectangle().fill(RoundtableColours.rule).frame(height: 1)
    }

    private func caption(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11, weight: .bold))
            .tracking(0.6)
            .foregroundStyle(RoundtableColours.dim)
    }

    private func small(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(RoundtableColours.dim)
            .fixedSize(horizontal: false, vertical: true)
    }

    private func tag(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11, weight: .bold))
            .foregroundStyle(RoundtableColours.dim)
            .padding(.horizontal, 3)
            .overlay(RoundedRectangle(cornerRadius: 2).strokeBorder(RoundtableColours.fieldEdge, lineWidth: 1))
            .lineLimit(1)
            .fixedSize()
    }
}

/// Roundtable's buttons: the main one blue, the others outlined.
struct RoundtableButtonStyle: ButtonStyle {
    let main: Bool
    @Environment(\.isEnabled) private var enabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 12, weight: main ? .bold : .semibold))
            .foregroundStyle(enabled ? Color.white : RoundtableColours.dim2)
            .padding(.horizontal, 12)
            .frame(minHeight: 30)
            .background(main && enabled ? RoundtableColours.mainButton : RoundtableColours.field,
                        in: RoundedRectangle(cornerRadius: 6))
            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(
                main && enabled ? RoundtableColours.accent : RoundtableColours.fieldEdge, lineWidth: 1))
            .opacity(configuration.isPressed ? 0.7 : 1)
            .contentShape(Rectangle())
    }
}
