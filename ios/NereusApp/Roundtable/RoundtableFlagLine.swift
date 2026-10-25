// NereusSDR for iOS: the Roundtable line at the foot of a slice's flag, and the state glyphs Roundtable draws
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the flag line read as the desktop's
// roundtableFlagLineFor reads it. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the line's hint names where it takes
// you (the iPad's applet column). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// What a slice's flag says of Roundtable (spec 11.2 and 11.3, D17): the
/// state glyph, "Roundtable · 9 · K4ABC", @N while a message names you,
/// "9 hear you" while you talk there, and the link's state when it is not
/// live. Read as the desktop reads it (src/gui/roundtable/RoundtableTagLayout.cpp,
/// roundtableFlagLineFor). Nil hides the line: no Roundtable on this Core,
/// or Roundtable off.
struct RoundtableFlagLine: Equatable {
    /// The shapes Roundtable draws (the desktop's RoundtableGlyph).
    enum Glyph: Equatable {
        case quiet
        case talking
        case unknown
        case doubled
        case you
        case waiting
        case offline
    }

    var glyph: Glyph
    var text: String
    /// Messages that name you, not yet read.
    var mentions: Int = 0

    /// The line's height at the foot of the flag.
    static let height: CGFloat = 24
    static let prefix = "Roundtable \u{00B7} "

    /// The line for one slice: its room (`roomId`, nil when the slice is in
    /// none), the band list's summary of it, and the room as followed.
    static func line(available: Bool, link: RoundtableModel.LinkState, summary: RoundtableModel.RoomSummary?,
                     room: RoundtableModel.Room?, mentions: Int) -> RoundtableFlagLine? {
        guard available else {
            return nil
        }
        switch link {
        case .off:
            return nil
        case .connecting:
            return RoundtableFlagLine(glyph: .waiting, text: prefix + "Connecting")
        case .reconnecting:
            return RoundtableFlagLine(glyph: .waiting, text: prefix + "Reconnecting")
        case .offline:
            return RoundtableFlagLine(glyph: .offline, text: prefix + "Offline")
        case .live, .hidden:
            // Hidden still sees everything (D21): the room, as when live.
            break
        }
        guard let summary, summary.members > 0 else {
            return RoundtableFlagLine(glyph: .quiet, text: prefix + "Nobody here")
        }
        if let room, youAreTalking(room) {
            // U3: while you transmit there, the others whose reading of you is present.
            let hearing = room.members.filter { !$0.you && ($0.reading?.present ?? false) }.count
            return RoundtableFlagLine(glyph: .you, text: prefix + "\(hearing) hear you", mentions: mentions)
        }
        let glyph = glyph(ofState: summary.state)
        return RoundtableFlagLine(glyph: glyph, text: prefix + "\(summary.members) \u{00B7} "
                                  + label(glyph, talkers: summary.talkers), mentions: mentions)
    }

    /// The line for a slice, read from the model.
    @MainActor
    static func line(_ model: RoundtableModel, sliceId: Int64) -> RoundtableFlagLine? {
        let roomId = model.sliceRooms[sliceId]
        let summary = roomId.flatMap { id in model.rooms.first { $0.id == id } }
        return line(available: model.available, link: model.linkState, summary: summary,
                    room: roomId.flatMap(model.view(of:)), mentions: roomId.map { model.mentionCounts[$0] ?? 0 } ?? 0)
    }

    static func glyph(ofState state: String) -> Glyph {
        switch state {
        case "talking":
            return .talking
        case "doubled":
            return .doubled
        case "unknown":
            return .unknown
        default:
            return .quiet
        }
    }

    static func label(_ glyph: Glyph, talkers: [String]) -> String {
        switch glyph {
        case .you:
            return "You"
        case .talking:
            return talkers.first.flatMap { $0.isEmpty ? nil : $0 } ?? "Unknown"
        case .doubled:
            return "Doubled"
        case .unknown:
            return "Unknown"
        case .quiet, .waiting, .offline:
            return "Quiet"
        }
    }

    /// A turn is yours: one of its talkers is your member, or it is on the
    /// air and your member is keyed (the desktop's roundtableTurnIsYours).
    static func turnIsYours(_ room: RoundtableModel.Room, _ turn: RoundtableModel.Turn, open: Bool) -> Bool {
        guard let you = room.members.first(where: \.you), !you.ref.isEmpty else {
            return false
        }
        if turn.talkers.contains(where: { $0.ref == you.ref }) {
            return true
        }
        return open && you.keyed
    }

    /// One of the room's turns on the air is yours.
    static func youAreTalking(_ room: RoundtableModel.Room) -> Bool {
        room.now.contains { turnIsYours(room, $0, open: true) }
    }
}

/// Roundtable's colours, the desktop panel's (RoundtablePanel.cpp), each
/// at 4.5:1 or better on the sheet's ground.
enum RoundtableColours {
    static let ground = Color(red: 0x0C / 255, green: 0x0C / 255, blue: 0x0E / 255)
    static let rule = Color(red: 0x1F / 255, green: 0x1F / 255, blue: 0x23 / 255)
    static let text = Color(red: 0xDC / 255, green: 0xDC / 255, blue: 0xE1 / 255)
    static let bright = Color(red: 0xE7 / 255, green: 0xE7 / 255, blue: 0xEB / 255)
    static let dim = Color(red: 0xA6 / 255, green: 0xA6 / 255, blue: 0xAC / 255)
    static let dim2 = Color(red: 0x84 / 255, green: 0x84 / 255, blue: 0x8B / 255)
    static let accent = Color(red: 0x00 / 255, green: 0xB4 / 255, blue: 0xD8 / 255)
    static let you = Color(red: 0x00 / 255, green: 0xD4 / 255, blue: 0xFF / 255)
    static let talking = Color(red: 0x4C / 255, green: 0xAF / 255, blue: 0x6A / 255)
    static let unknown = Color(red: 0xD8 / 255, green: 0xA5 / 255, blue: 0x5F / 255)
    static let doubled = Color(red: 0xE0 / 255, green: 0x6C / 255, blue: 0x6E / 255)
    static let quiet = Color(red: 0x84 / 255, green: 0x84 / 255, blue: 0x8B / 255)
    static let stale = Color(red: 0x84 / 255, green: 0x84 / 255, blue: 0x8B / 255)
    static let field = Color(red: 0x16 / 255, green: 0x16 / 255, blue: 0x1A / 255)
    static let fieldEdge = Color(red: 0x3F / 255, green: 0x3F / 255, blue: 0x42 / 255)
    static let mainButton = Color(red: 0x2F / 255, green: 0x5F / 255, blue: 0x92 / 255)
    static let youRow = Color(red: 0x00 / 255, green: 0xD4 / 255, blue: 0xFF / 255).opacity(0.08)

    static func colour(_ glyph: RoundtableFlagLine.Glyph) -> Color {
        switch glyph {
        case .quiet:
            return quiet
        case .talking:
            return talking
        case .unknown, .waiting:
            return unknown
        case .doubled, .offline:
            return doubled
        case .you:
            return you
        }
    }
}

/// A state glyph: a hollow ring quiet, a filled dot talking, an amber ring
/// with a dot unknown, a striped red diamond doubled, a cyan dot you, an
/// amber ring open at the top waiting, a red ring offline.
struct RoundtableGlyphView: View {
    let glyph: RoundtableFlagLine.Glyph
    var size: CGFloat = 9

    var body: some View {
        let colour = RoundtableColours.colour(glyph)
        ZStack {
            switch glyph {
            case .quiet:
                Circle().strokeBorder(colour, lineWidth: 1.5)
            case .talking, .you:
                Circle().fill(colour)
            case .unknown:
                Circle().strokeBorder(colour, lineWidth: 1.5)
                Circle().fill(colour).padding(size * 0.3)
            case .doubled:
                Rectangle()
                    .fill(colour.opacity(0.35))
                    .overlay {
                        Rectangle().fill(colour).frame(width: size * 0.18)
                    }
                    .overlay(Rectangle().strokeBorder(colour, lineWidth: 1.2))
                    .rotationEffect(.degrees(45))
                    .padding(size * 0.15)
            case .waiting:
                Circle()
                    .trim(from: 0.08, to: 0.92)
                    .stroke(colour, lineWidth: 1.5)
                    .rotationEffect(.degrees(-90))
                    .padding(0.75)
            case .offline:
                Circle().strokeBorder(colour, lineWidth: 1.5)
            }
        }
        .frame(width: size, height: size)
        .accessibilityHidden(true)
    }
}

/// The line at the foot of a flag: the glyph, the words, @N in cyan, and
/// the caret. The whole line is one button that opens the Roundtable sheet.
struct RoundtableFlagLineView: View {
    let line: RoundtableFlagLine
    let letter: String
    var open: (() -> Void)?
    /// What a tap does, for VoiceOver.
    var hint = Self.sheetHint

    static let sheetHint = "Opens Roundtable"
    static let columnHint = "Shows Roundtable in the applet column"

    var body: some View {
        Button {
            open?()
        } label: {
            HStack(spacing: 5) {
                RoundtableGlyphView(glyph: line.glyph)
                Text(line.text)
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(RoundtableColours.text)
                    .lineLimit(1)
                    .minimumScaleFactor(0.85)
                Spacer(minLength: 2)
                if line.mentions > 0 {
                    Text("@\(line.mentions)")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(Color.black)
                        .padding(.horizontal, 5)
                        .frame(height: 16)
                        .background(RoundtableColours.you, in: Capsule())
                        .accessibilityIdentifier("roundtableMentions\(letter)")
                }
                Text("\u{203A}")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(RoundtableColours.dim)
            }
            .padding(.horizontal, 4)
            .frame(height: RoundtableFlagLine.height)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .overlay(alignment: .top) {
            Rectangle().fill(Color.white.opacity(0.12)).frame(height: 1)
        }
        .accessibilityLabel(line.text.replacingOccurrences(of: " \u{00B7} ", with: ", ")
                            + (line.mentions > 0 ? ", \(line.mentions) naming you" : ""))
        .accessibilityHint(hint)
        .accessibilityIdentifier("roundtableLine\(letter)")
    }
}
