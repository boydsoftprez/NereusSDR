// NereusSDR for iOS: Roundtable's page on the Tools tab: every frequency by band, and one room read without tuning
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the Tools tab's Roundtable page and
// its room page. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, a room's row its own view for the
// iPad's widened applet to share. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: the rooms grouped and sorted by band
// once (groups), for the widened applet's list to age each second.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

// 2026-10-30: Approved frequency-first people, reports and phone interactions.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import NereusBand
import SwiftUI

/// The Roundtable page's words and rows, apart from any view (spec 11.3,
/// U1): band chips with counts, then one row per room with its state, who,
/// how many stations and messages, and @N where a message names you. A
/// slice's room is marked with its letter.
@MainActor
enum RoundtableToolsList {
    /// The chip that shows every band.
    static let allChip = "all"

    struct Chip: Equatable, Identifiable {
        /// ``allChip`` or the band's name (`40m`).
        var id: String
        var title: String
        var count: Int
    }

    struct Row: Equatable, Identifiable {
        var id: String
        var hz: Int64
        var frequency: String
        var glyph: RoundtableFlagLine.Glyph
        var who: String
        var sub: String
        /// The letters of this phone's slices tuned to the room.
        var letters: [String]
        var mentions: Int
        var quiet: Bool
        var mine: Bool { !letters.isEmpty }
    }

    /// The note under the list, with the active slice's letter.
    static func note(sliceLetter: String) -> String {
        "A tap on a row reads that frequency; slice \(sliceLetter) stays where it is. Only a tag on the band "
            + "or Tune \(sliceLetter) here tunes. Quiet rooms keep their last talker for an hour."
    }

    /// A room's band: the Core's, or worked out from its frequency.
    static func band(_ room: RoundtableModel.RoomSummary) -> String {
        room.band.isEmpty || room.band == "other" ? RoundtableModel.bandOf(room.hz) : room.band
    }

    static func bandTitle(_ band: String) -> String {
        band == "other" ? "Other" : band.uppercased()
    }

    /// All, then each band with a room, lowest first, each with its count.
    static func chips(_ rooms: [RoundtableModel.RoomSummary]) -> [Chip] {
        var counts: [String: Int] = [:]
        var lowest: [String: Int64] = [:]
        for room in rooms {
            let band = band(room)
            counts[band, default: 0] += 1
            lowest[band] = min(lowest[band] ?? .max, room.hz)
        }
        let bands = counts.keys.sorted { (lowest[$0] ?? 0, $0) < (lowest[$1] ?? 0, $1) }
        return [Chip(id: allChip, title: "All", count: rooms.count)]
            + bands.map { Chip(id: $0, title: bandTitle($0), count: counts[$0] ?? 0) }
    }

    /// The chip the page opens on: the active slice's band when a room is
    /// there, else All.
    static func defaultChip(_ rooms: [RoundtableModel.RoomSummary], sliceRoomId: String?, sliceHz: Int64?) -> String {
        if let sliceRoomId, let room = rooms.first(where: { $0.id == sliceRoomId }) {
            return band(room)
        }
        if let sliceHz {
            let here = RoundtableModel.bandOf(sliceHz)
            if rooms.contains(where: { band($0) == here }) {
                return here
            }
        }
        return allChip
    }

    /// The rooms under `chip`, by frequency.
    static func rows(_ rooms: [RoundtableModel.RoomSummary], chip: String, sliceRooms: [Int64: String],
                     letters: [Int64: String], mentionCounts: [String: Int], now: Int64) -> [Row] {
        rows(ordered: ordered(rooms, chip: chip), sliceRooms: sliceRooms, letters: letters,
             mentionCounts: mentionCounts, now: now)
    }

    /// The rooms under `chip`, by frequency, then id.
    static func ordered(_ rooms: [RoundtableModel.RoomSummary], chip: String) -> [RoundtableModel.RoomSummary] {
        rooms.filter { chip == allChip || band($0) == chip }
            .sorted { ($0.hz, $0.id) < ($1.hz, $1.id) }
    }

    /// Each band's chip with its rooms in order: worked out once per change
    /// of the band list, so a list that ages each second sorts nothing.
    static func groups(_ rooms: [RoundtableModel.RoomSummary]) -> [(chip: Chip, rooms: [RoundtableModel.RoomSummary])] {
        chips(rooms).filter { $0.id != allChip }.map { ($0, ordered(rooms, chip: $0.id)) }
    }

    /// Rows for rooms already in order.
    static func rows(ordered rooms: [RoundtableModel.RoomSummary], sliceRooms: [Int64: String],
                     letters: [Int64: String], mentionCounts: [String: Int], now: Int64) -> [Row] {
        rooms.map { room in
                let glyph = RoundtableFlagLine.glyph(ofState: room.state)
                let mine = sliceRooms.filter { $0.value == room.id }.keys.sorted().compactMap { letters[$0] }
                return Row(id: room.id, hz: room.hz, frequency: room.stations.isEmpty ? RoundtableText.shortFrequency(room.hz) : RoundtableTagLayout.frequencyText(room.hz),
                           glyph: glyph, who: room.stations.isEmpty ? who(glyph, talkers: room.talkers)
                               : room.stations.map { RoundtableText.withNth($0.callsign, $0.nth) }.joined(separator: ", "),
                           sub: sub(room, glyph: glyph, now: now), letters: mine,
                           mentions: mentionCounts[room.id] ?? 0, quiet: glyph == .quiet)
            }
    }

    static func who(_ glyph: RoundtableFlagLine.Glyph, talkers: [String]) -> String {
        let talker = talkers.first.flatMap { $0.isEmpty ? nil : $0 }
        switch glyph {
        case .talking:
            return talker ?? "Unknown talker"
        case .unknown:
            return "Unknown talker"
        case .doubled:
            return talker.map { "Doubled: \($0)" } ?? "Doubled"
        case .quiet, .waiting, .offline, .you:
            return "Quiet"
        }
    }

    /// The line under who: a quiet room's last talker and when, else its
    /// stations and messages. (The band list carries no reading times, so
    /// the newest reading's age is on the room's page, not here.)
    static func sub(_ room: RoundtableModel.RoomSummary, glyph: RoundtableFlagLine.Glyph, now: Int64) -> String {
        if glyph == .quiet {
            let last = room.talkers.first.flatMap { $0.isEmpty ? nil : $0 } ?? room.lastMessage?.callsign
            if let last, !last.isEmpty, room.last > 0 {
                return "last \(last), \(RoundtableText.ageText(now - room.last)) ago"
            }
            return stations(room.members)
        }
        var text = stations(room.members)
        if room.messages > 0 {
            text += " \u{00B7} \(room.messages) message\(room.messages == 1 ? "" : "s")"
        }
        return text
    }

    private static func stations(_ count: Int64) -> String {
        "\(count) station\(count == 1 ? "" : "s")"
    }
}

/// The Tools tab's Roundtable page: the chips, the connection line, the
/// rooms and the note. A tap on a row reads that room and opens its page;
/// it never tunes a slice.
struct RoundtableToolsPage: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var slices: BandSlicesModel
    var stationCallsign = ""
    var stationGrid = ""
    let open: (RoundtableModel.RoomSummary) -> Void

    @Environment(\.bandOnScreen) private var onScreen
    @State private var pathSettingsOpen = false
    @State private var chip: String?
    @State private var startedHere = false
    @State private var firstRunForm = false

    private static let chipFill = Color(red: 0x1C / 255, green: 0x1C / 255, blue: 0x20 / 255)
    private static let chipEdge = Color(red: 0x26 / 255, green: 0x26 / 255, blue: 0x2B / 255)
    private static let chipText = Color(red: 0xC8 / 255, green: 0xC8 / 255, blue: 0xCC / 255)
    private static let chipOnFill = Color(red: 0x1E / 255, green: 0x3D / 255, blue: 0x5F / 255)
    private static let chipOnEdge = Color(red: 0x4A / 255, green: 0x7B / 255, blue: 0xA8 / 255)
    private static let chipOnCount = Color(red: 0xA9 / 255, green: 0xC4 / 255, blue: 0xDE / 255)

    private var sliceLetter: String { slices.active?.slice.letter ?? "A" }

    private var letters: [Int64: String] {
        Dictionary(slices.entries.map { (Int64($0.id), $0.slice.letter) }, uniquingKeysWith: { first, _ in first })
    }

    private var shownChip: String {
        let chips = RoundtableToolsList.chips(roundtable.frequencyRows)
        if let chip, chips.contains(where: { $0.id == chip }) {
            return chip
        }
        let sliceRoom = slices.activeSliceId.flatMap { roundtable.sliceRooms[Int64($0)] }
        return RoundtableToolsList.defaultChip(roundtable.frequencyRows, sliceRoomId: sliceRoom,
                                               sliceHz: slices.active.map { Int64($0.slice.frequencyHz) })
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let reason = roundtable.sourceControlReason {
                Text(reason)
                    .font(.system(size: 13))
                    .foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("roundtableTools.reason")
            }
            if let reason = roundtable.frequencyReason {
                Text(reason).font(.system(size: 12)).foregroundStyle(RoundtableColours.dim)
            }
            chips
            connection
            if let note = roundtable.note, note != roundtable.sourceControlReason {
                Text(note)
                    .font(.system(size: 12))
                    .foregroundStyle(RoundtableColours.unknown)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("roundtableTools.controlNote")
            }
            if roundtable.askingFirstRun {
                RoundtableFirstRunPrompt { firstRunForm = true }
            }
            if roundtable.frequencyRows.isEmpty {
                Text("Nobody on Roundtable is on the air.")
                    .font(.system(size: 13))
                    .foregroundStyle(RoundtableColours.dim)
                    .accessibilityIdentifier("roundtableTools.empty")
            } else {
                SpotHubPage.Card {
                    TimelineView(.periodic(from: .now, by: 1)) { context in
                        let now = Int64(context.date.timeIntervalSince1970 * 1000)
                        let rows = RoundtableToolsList.rows(roundtable.frequencyRows, chip: shownChip,
                                                            sliceRooms: roundtable.sliceRooms, letters: letters,
                                                            mentionCounts: roundtable.mentionCounts, now: now)
                        VStack(spacing: 0) {
                            ForEach(Array(rows.enumerated()), id: \.element.id) { index, row in
                                if index > 0 {
                                    SpotHubPage.Line()
                                }
                                rowView(row)
                            }
                        }
                    }
                }
            }
            Text(roundtable.frequencyAvailable
                 ? "People are shown at their actual frequency. Read leaves your slices where they are; Tune is explicit."
                 : RoundtableToolsList.note(sliceLetter: sliceLetter))
                .font(.system(size: 12))
                .foregroundStyle(RoundtableColours.dim2)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 2)
                .accessibilityIdentifier("roundtableTools.note")
        }
        .onAppear { roundtable.activeSliceId = slices.activeSliceId.map(Int64.init) }
        .onChange(of: slices.activeSliceId) { _, id in roundtable.activeSliceId = id.map(Int64.init) }
        .onChange(of: roundtable.askingFirstRun) { _, asking in
            if !asking {
                firstRunForm = false
                startedHere = false
            } else if startedHere && onScreen {
                firstRunForm = true
            }
        }
        .onChange(of: roundtable.connectionGeneration) { _, _ in
            firstRunForm = false
            startedHere = false
        }
        .onChange(of: onScreen) { _, shown in
            if !shown {
                firstRunForm = false
                startedHere = false
            }
        }
        .onDisappear {
            firstRunForm = false
            startedHere = false
        }
        .sheet(isPresented: $firstRunForm) {
            RoundtableFirstRunSheet(roundtable: roundtable, callsign: stationCallsign, grid: stationGrid) {
                firstRunForm = false
                startedHere = false
            }
            .background(RoundtableColours.ground)
        }
    }

    private var chips: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                ForEach(RoundtableToolsList.chips(roundtable.frequencyRows)) { item in
                    let on = item.id == shownChip
                    Button {
                        chip = item.id
                    } label: {
                        HStack(spacing: 4) {
                            Text(item.title).font(.system(size: 12.5, weight: .bold))
                                .foregroundStyle(on ? Color.white : Self.chipText)
                            Text("\(item.count)").font(.system(size: 12, weight: .semibold))
                                .foregroundStyle(on ? Self.chipOnCount : RoundtableColours.dim2)
                        }
                        .padding(.horizontal, 11)
                        .frame(height: 30)
                        .background(on ? Self.chipOnFill : Self.chipFill, in: Capsule())
                        .overlay(Capsule().strokeBorder(on ? Self.chipOnEdge : Self.chipEdge, lineWidth: 1))
                        .frame(minHeight: 44)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("\(item.title), \(item.count) room\(item.count == 1 ? "" : "s")")
                    .accessibilityAddTraits(on ? .isSelected : [])
                    .accessibilityIdentifier("roundtableTools.chip.\(item.id)")
                }
            }
        }
    }

    private var connection: some View {
        TimelineView(.periodic(from: .now, by: 1)) { context in
            let now = Int64(context.date.timeIntervalSince1970 * 1000)
            let newest = roundtable.room?.members.compactMap(\.reading?.t).max()
            let line = RoundtableText.connectionLine(link: roundtable.linkState, newestReadingMs: newest,
                                                     linkSince: roundtable.linkSince, now: now)
            let colour: Color = switch line.tone {
            case .live: RoundtableColours.talking
            case .warn: RoundtableColours.unknown
            case .you: RoundtableColours.you
            case .dim: RoundtableColours.dim
            }
            HStack(spacing: 8) {
                HStack(spacing: 6) {
                    Circle().fill(colour).frame(width: 6, height: 6)
                    Text(line.text).font(.system(size: 12))
                        .foregroundStyle(line.tone == .live ? RoundtableColours.dim : colour)
                }
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("roundtableTools.connection")
                Spacer(minLength: 0)
                Button { pathSettingsOpen = true } label: {
                    Image(systemName: "gearshape").frame(minWidth: 44, minHeight: 44)
                }
                .accessibilityLabel("Roundtable path settings")
                .accessibilityIdentifier("roundtableTools.pathSettings")
                .sheet(isPresented: $pathSettingsOpen) { RoundtablePathSettings(roundtable: roundtable) }
                Button(roundtable.sourceControlTitle) {
                    let start = !roundtable.sourceRunning
                    let generation = roundtable.connectionGeneration
                    startedHere = start
                    Task {
                        guard roundtable.connectionGeneration == generation else { return }
                        let reason = await roundtable.setRunning(start)
                        guard roundtable.connectionGeneration == generation else { return }
                        if reason != nil {
                            startedHere = false
                        }
                        if startedHere && onScreen && roundtable.askingFirstRun {
                            firstRunForm = true
                        }
                        if roundtable.firstRunAnswered {
                            startedHere = false
                        }
                    }
                }
                .frame(minWidth: 72, minHeight: 44)
                .buttonStyle(RoundtableButtonStyle(main: !roundtable.sourceRunning))
                .disabled(roundtable.sourceControlReason != nil)
                .accessibilityIdentifier("roundtableTools.control")
            }
        }
    }

    private func rowView(_ row: RoundtableToolsList.Row) -> some View {
        RoundtableRoomRow(row: row) {
            if let room = roundtable.frequencyRows.first(where: { $0.id == row.id }) {
                open(room)
            }
        }
    }
}

/// One room in a list of rooms: the glyph, frequency, who and the line
/// under it, your slices' letters and @N. The Tools page and the iPad's
/// widened applet share it.
struct RoundtableRoomRow: View {
    let row: RoundtableToolsList.Row
    /// The frequency column's width (the widened applet's list is narrower).
    var frequencyWidth: CGFloat = 70
    var identifier: String?
    let open: () -> Void

    private static let mineFill = Color(red: 0x4A / 255, green: 0x7B / 255, blue: 0xA8 / 255).opacity(0.08)
    private static let badgeText = Color(red: 0x04 / 255, green: 0x16 / 255, blue: 0x1C / 255)

    var body: some View {
        Button {
            open()
        } label: {
            HStack(spacing: 8) {
                RoundtableGlyphView(glyph: row.glyph, size: 10)
                    .frame(width: 14)
                Text(row.frequency)
                    .font(.system(size: 13, weight: .bold).monospaced())
                    .foregroundStyle(row.quiet ? RoundtableColours.dim : RoundtableColours.bright)
                    .frame(width: frequencyWidth, alignment: .leading)
                VStack(alignment: .leading, spacing: 1) {
                    Text(row.who).font(.system(size: 13))
                        .foregroundStyle(row.quiet ? RoundtableColours.dim : RoundtableColours.text)
                    Text(row.sub).font(.system(size: 11.5)).foregroundStyle(RoundtableColours.dim)
                }
                .lineLimit(1)
                .frame(maxWidth: .infinity, alignment: .leading)
                HStack(spacing: 4) {
                    ForEach(row.letters, id: \.self) { letter in
                        Text(letter)
                            .font(.system(size: 11, weight: .heavy).monospaced())
                            .foregroundStyle(Self.badgeText)
                            .padding(.horizontal, 4)
                            .frame(minWidth: 16)
                            .background(RoundtableColours.you, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                    }
                    if row.mentions > 0 {
                        Text("@\(row.mentions)")
                            .font(.system(size: 11, weight: .bold).monospaced())
                            .foregroundStyle(Self.badgeText)
                            .padding(.horizontal, 4)
                            .frame(minWidth: 16)
                            .background(RoundtableColours.you, in: Capsule())
                    }
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 4)
            .frame(minHeight: 44)
            .background(row.mine ? Self.mineFill : Color.clear)
            .overlay(alignment: .leading) {
                if row.mine {
                    Rectangle().fill(RoundtableColours.you).frame(width: 3)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityElement(children: .combine)
        .accessibilityHint("Reads this frequency. Your slices stay where they are.")
        .accessibilityIdentifier(identifier ?? "roundtableTools.row.\(row.id)")
    }
}

/// One room read from the Tools tab: the connection line and the room,
/// full height. Reading it tunes nothing; Tune A here tunes the active
/// slice there. Leaving the page stops reading it.
struct RoundtableToolsRoomPage: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var transmit: TransmitModel
    let roomId: String
    let stationCallsign: String

    private var room: RoundtableModel.Room? {
        roundtable.room?.id == roomId ? roundtable.room : roundtable.view(of: roomId)
    }

    private var tunedHere: Bool {
        roundtable.frequencyAvailable ? roundtable.activeContext?.id == roomId : roundtable.sliceRooms.values.contains(roomId)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 8) {
                if let reason = roundtable.coreReason {
                    Text(reason).font(.system(size: 13)).foregroundStyle(RoundtableColours.dim)
                        .fixedSize(horizontal: false, vertical: true)
                } else if let room {
                    RoundtableRoomView(roundtable: roundtable, room: room, full: true,
                                       keyed: transmit.ptt.transmitting, tunedHere: tunedHere,
                                       stationCallsign: stationCallsign,
                                       ownDbm: slices.active?.signalDbm,
                                       sliceLetter: slices.active?.slice.letter ?? "A",
                                       tuneHere: roomId == roundtable.sliceRoomId ? nil : { tune(room) })
                } else {
                    Text("Reading this frequency.").font(.system(size: 13)).foregroundStyle(RoundtableColours.dim)
                }
            }
            .padding(.vertical, 8)
        }
        .background(RoundtableColours.ground.ignoresSafeArea(edges: [.horizontal]))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableTools.room")
        .task(id: roomId) {
            // The active slice's own room is in view already; any other is read.
            if roomId != roundtable.sliceRoomId {
                await roundtable.read(roomId)
            }
        }
        .onDisappear {
            if roundtable.readingRoomId == roomId {
                Task { await roundtable.unread() }
            }
        }
    }

    private func tune(_ room: RoundtableModel.Room) {
        slices.tap(to: Double(room.hz))
        Task { await roundtable.backToSlice() }
    }
}

/// Station preferences are available on the phone even when nobody is on frequency.
struct RoundtablePathSettings: View {
    @ObservedObject var roundtable: RoundtableModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Form {
                if let note = roundtable.note { Text(note).font(.footnote).foregroundStyle(.secondary) }
                Picker("Signal", selection: Binding(get: { roundtable.signalFormat }, set: { roundtable.setSignalFormat($0) })) {
                    ForEach(RoundtableSignalFormat.allCases, id: \.self) { format in
                        Text(format.title).tag(format)
                    }
                }
                .accessibilityIdentifier("roundtableSignalFormat")
                Text("Signal display is kept on this device.").font(.footnote).foregroundStyle(.secondary)
                Picker("Distance", selection: Binding(get: { roundtable.distanceUnits }, set: { units in
                    Task { await roundtable.setDistanceUnits(units) }
                })) {
                    Text("Miles (mi)").tag("mi")
                    Text("Kilometres (km)").tag("km")
                }
                .accessibilityIdentifier("roundtableDistanceUnits")
                Toggle("Show true bearing (°T)", isOn: Binding(get: { roundtable.showBearing }, set: { show in
                    Task { await roundtable.setShowBearing(show) }
                }))
                .accessibilityIdentifier("roundtableShowBearing")
                Text("Distance and initial true bearing run from the identified talker to the reporter. Unknown identity or missing grids leave the path blank.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            .navigationTitle("Roundtable settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
        .preferredColorScheme(.dark)
        .presentationDetents([.medium, .large])
    }
}
