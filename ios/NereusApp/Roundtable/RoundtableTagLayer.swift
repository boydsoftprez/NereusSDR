// NereusSDR for iOS: Roundtable's tags along the foot of the phone's spectrum, folding as the desktop's do
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the pan tags laid out by the desktop's
// rules (src/gui/roundtable/RoundtableTagLayout.cpp), a tap that tunes and
// the +N list. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: while you are hidden the tags show every room as when live,
// their marking dimmed (RoundtableTagDisplay.dimmed), as the desktop's do
// (JJ's answer of 2026-10-25). J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.

// 2026-10-30: Approved frequency-first people, reports and phone interactions.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

// 2026-10-31: Compact semantic cards share selected signal formatting and fitted geometry.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

// 2026-11-01: Keep long identities readable and distinguish ended observations.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-02: Prioritize active reports and show authoritative combined overlaps.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-03: Overlay redesign truth rules (design D1 to D9), the card data and
// peak hold, as the desktop's RoundtableTagLayout.cpp.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import SwiftUI
import UIKit

/// One room as a tag on the pan (the desktop's RoundtableTag).
struct RoundtableTag: Equatable {
    var roomId: String
    /// The room's center.
    var hz: Int64
    var glyph: RoundtableFlagLine.Glyph = .talking
    /// The talker's callsign, You, Unknown, Double or Quiet.
    var label: String = ""
    var card: RoundtableTagCard? = nil
    var members: Int64 = 0
    /// Your slice's room.
    var mine = false
    /// A message there names you.
    var mentionsMe = false
}

/// RF attribution is separate from the directory of people at this frequency.
struct RoundtableTagCard: Equatable {
    enum State: Equatable {
        case waiting, known(String), unknown, doubled, unattributed

        var caption: String? {
            switch self {
            case .doubled: "Combined RF"
            default: nil
            }
        }
        var title: String? {
            switch self {
            case .waiting: nil
            case .known(let name): name
            case .unknown: "Unknown signal"
            case .doubled: "Double"
            case .unattributed: "Unattributed RF"
            }
        }
    }
    struct Report: Equatable, Identifiable {
        var id: String
        var reporter: String
        var reading: RoundtableModel.Reading
        var age: String
        var stale: Bool
    }
    var state: State = .waiting
    var ended = false
    var participants: [String] = []
    var reports: [Report] = []
    var format: RoundtableSignalFormat = .dbm
    /// A dropped link (overlay design D7): nothing here claims to be live.
    var frozen = false

    /// Historical observations must not look like a new unknown or double, and
    /// a live talker is never captioned as past (overlay design D6).
    var caption: String? {
        guard state.title != nil else { return nil }
        if frozen { return "Last known" }
        guard ended else { return state.caption }
        switch state {
        case .known: return "Last talker"
        case .unknown, .unattributed: return "Last heard"
        case .doubled: return "Last double"
        case .waiting: return nil
        }
    }

    var accessibilityText: String {
        let attribution = [caption, state.title, participants.isEmpty ? nil : participants.joined(separator: ", "), ended ? "Turn ended" : nil].compactMap { $0 }.joined(separator: ", ")
        let readings = reports.map { report in
            let signal = RoundtableText.signalLines(report.reading, format: format).joined(separator: ", ")
                + (format == .dbm ? " dBm" : "")
            let subject = state == .doubled ? "combined RF" : (state.title ?? "this signal")
            return "\(report.reporter) heard \(subject), \(signal), \(report.age) ago\(report.stale ? ", stale" : "")"
        }
        return ([attribution] + readings + ["Signal display: " + format.title, "Read reports for details"])
            .filter { !$0.isEmpty }.joined(separator: ". ")
    }
}

/// The final visible content also owns drawing height, keepouts and hit bounds.
struct RoundtableTagCardPlan: Equatable {
    struct Report: Equatable, Identifiable {
        var id: String
        var reporter: String
        var signal: [String]
        var age: String
        var stale: Bool
    }
    var leadingPadding: CGFloat = 6
    var trailingPadding: CGFloat = 7
    var showsFrequency = false
    var people: [String] = []
    var omittedPeople = 0
    var stateCaption: String?
    var stateTitle: String?
    var stateFontSize: CGFloat = 10
    var participants: [String] = []
    var omittedParticipants = 0
    var omittedReports = 0
    var reports: [Report] = []
    var signalTitle = ""
    var showsDetails = false
    var reportHeight: CGFloat = 18
    var height: CGFloat = 8
}

/// What the card for one frequency says, for the drawing code to draw (the
/// desktop's RoundtableCardModel). Times are in milliseconds.
struct RoundtableCardData: Equatable {
    enum Kind: Equatable { case talking, you, double, unknown, lastTalker, lastDouble, quiet }

    /// One listener's reading. In a double it is combined RF, so `km` and
    /// `bearing` stay nil (overlay design D2). Past 20 seconds it is old (D13).
    struct Row: Equatable {
        var call: String
        var dbm: Double
        var age: Int64
        var km: Int64?
        var bearing: Int64?
        var offsetHz: Int64
        var you: Bool
        var old: Bool
        /// The held peak in dBm, for the tick. Nil on an old reading, an ended
        /// turn or a dropped link: those have no tick.
        var peak: Double?
    }

    var kind: Kind = .quiet
    var hz: Int64
    /// On the air, or every name for a last double.
    var keyed: [String] = []
    /// The stations at this exact frequency.
    var people: [String] = []
    /// How long a live turn has run; stops where a link dropped.
    var clock: Int64 = 0
    /// How long ago a last turn ended.
    var endedAgo: Int64 = 0
    var rows: [Row] = []
    /// Reconnecting or offline (D7).
    var frozen = false
    /// The link's note, else "Double ended 3s ago" for 8 seconds.
    var note: String?
    /// Where the live turn's talker is, which can be up to 5 kHz off `hz`. Nil
    /// with no live turn.
    var liveHz: Int64?
}

/// Each listener's held peak (overlay design D12): client side, per source room,
/// listener and turn. It rises at once, falls 2 dB a second and resets on a new
/// turn. Never on the wire.
final class RoundtablePeakTracker {
    /// D12: a held peak falls this fast.
    private static let fallDbPerSecond = 2.0
    /// Forgets everything past this many listeners. A lost tick comes straight
    /// back, so a long session cannot grow it without bound.
    private static let maxEntries = 1024

    private struct Held {
        var turn: Int64
        var peak: Double
        var at: Int64
    }

    private var rooms: [String: [String: Held]] = [:]
    private var entries = 0

    /// The held peak after this reading.
    func update(sourceRoom: String, ref: String, turn: Int64, dbm: Double, now: Int64) -> Double {
        if var held = rooms[sourceRoom]?[ref] {
            if held.turn == turn {
                let seconds = Double(max(0, now - held.at)) / 1000
                held.peak = max(held.peak - Self.fallDbPerSecond * seconds, dbm)
            } else {
                held.turn = turn
                held.peak = dbm
            }
            held.at = now
            rooms[sourceRoom]?[ref] = held
            return held.peak
        }
        if entries >= Self.maxEntries {
            rooms.removeAll()
            entries = 0
        }
        rooms[sourceRoom, default: [:]][ref] = Held(turn: turn, peak: dbm, at: now)
        entries += 1
        return dbm
    }
}

/// Where one tag, or one fold of tags, sits: `left` and `width` in points
/// from the pan's left edge; `roomIds` lists the rooms in it by frequency.
struct RoundtableTagPlacement: Equatable {
    var left: Double
    var width: Double
    var roomIds: [String]
    var folded = false

    var centerX: Double { left + width / 2 }
    var right: Double { left + width }
}

/// A stretch of the pan, in points from its left edge, that no tag may
/// cover (a flag hanging over the tag row).
struct RoundtableKeepOut: Equatable {
    var left: Double
    var right: Double
}

/// How the pan shows its tags (design 11.1, U4): a dropped link, reconnecting
/// or offline, greys them as last known (overlay design D7), connecting hides them. `dimmed` is live while you are
/// hidden: the rooms and talkers as when live, the state marking dimmed.
enum RoundtableTagDisplay: Equatable {
    case hidden
    case live
    case stale
    case dimmed
}

/// The desktop's tag rules (RoundtableTagLayout.cpp), on the phone:
///
/// - Tags that would overlap form a group. A group's tags that can sit
///   side by side, each within one tag's width of its own frequency, move
///   aside; otherwise the group's tags fold into one +N.
/// - A fold holds two or more. A lone neighbor only moves aside.
/// - Your slice's room and rooms whose messages name you never fold.
/// - Folds widen only if the tags still don't fit across the pan: then
///   every overlapping group folds, and a group reaches further each round.
/// - Zooming in spreads the tags, so folds open by themselves.
enum RoundtableTagLayout {
    // MARK: The look (board.html's .rt-tag and .rt-fold, as the desktop draws them)

    static let height: CGFloat = 22
    /// The tag's foot to the band-plan strip's top.
    static let footGap: CGFloat = 8
    /// Between tags, and from either edge.
    static let gap: Double = 4
    static let minWidth: Double = 56
    static let maxWidth: Double = 128
    static let reportWidth: Double = 176
    static let padLeft: CGFloat = 6
    static let padRight: CGFloat = 7
    /// The glyph to the text.
    static let innerGap: CGFloat = 5
    static let glyphSize: CGFloat = 9
    /// A fold's stacked glyphs overlap by 3 points; it stacks three at most.
    static let foldStackStep: CGFloat = 6
    static let foldStackMax = 3
    /// The +N list's width and its rows' height.
    static let foldListWidth: CGFloat = 200
    static let foldRowHeight: CGFloat = 32
    static let labelFontSize: CGFloat = 11
    static let minimumStateFontSize: CGFloat = 8

    // MARK: Layout

    /// Lays `tags` out across a pan `width` points wide whose left edge is
    /// `leftHz`, at `hzPerPoint`. Every tag and fold is `tagWidth` wide,
    /// with at least `gap` between neighbors and from either edge, and
    /// clear of `keepOut`. Tags off the pan are left out. The placements
    /// are in order of frequency.
    static func layout(_ tags: [RoundtableTag], hzPerPoint: Double, leftHz: Int64, width: Double, tagWidth: Double,
                       gap: Double, keepOut: [RoundtableKeepOut] = []) -> [RoundtableTagPlacement] {
        guard hzPerPoint > 0, width > 0, tagWidth > 0 else {
            return []
        }
        let gap = max(0, gap)
        var visible: [Visible] = []
        for tag in tags {
            let x = Double(tag.hz - leftHz) / hzPerPoint
            if x < 0 || x > width {
                continue
            }
            visible.append(Visible(x: x, hz: tag.hz, roomId: tag.roomId, pinned: tag.mine || tag.mentionsMe))
        }
        visible = stableSorted(visible) { a, b in a.hz != b.hz ? a.hz < b.hz : a.roomId < b.roomId }

        let stretches = stretchesOf(width: width, tagWidth: tagWidth, gap: gap, keepOut: keepOut)
        let capacity = stretches.reduce(0) { $0 + $1.capacity }
        if capacity <= 0 {
            return []
        }

        // First only the groups that cannot move aside fold. If the tags
        // still don't fit across the pan, every overlapping group folds,
        // and then the groups reach further each round until they fit or
        // span the pan.
        var reach = tagWidth + gap
        var forceFold = false
        var items: [Item]
        while true {
            items = groupTags(visible, tagWidth: tagWidth, gap: gap, reach: reach, forceFold: forceFold)
            if items.count <= capacity || reach > width {
                break
            }
            if !forceFold {
                forceFold = true
            } else {
                reach *= 1.5
            }
        }

        // A geometric fold must not consume the only clear slot before a
        // selected cochannel whose frequency happens to sort later.
        let pinnedIds = Set(visible.filter(\.pinned).map(\.roomId))
        while items.count > capacity,
              let remove = items.lastIndex(where: { Set($0.roomIds).isDisjoint(with: pinnedIds) }) {
            items.remove(at: remove)
        }

        // Each tag to the stretch nearest its frequency that still has
        // room, keeping their order: a stretch already passed is not gone
        // back to. Tags that find no room are left out.
        var byStretch = Array(repeating: [Int](), count: stretches.count)
        var current = 0
        for k in items.indices {
            var target = max(nearestStretch(stretches, x: items[k].x), current)
            while target < stretches.count && byStretch[target].count >= stretches[target].capacity {
                target += 1
            }
            if target >= stretches.count {
                break
            }
            byStretch[target].append(k)
            current = target
        }

        // Within a stretch, side by side, as near their frequencies as the
        // space allows: pushed right off its left edge and their left
        // neighbors, then back left off its right edge and their right
        // neighbors.
        var out: [RoundtableTagPlacement] = []
        for s in stretches.indices {
            let members = byStretch[s]
            var lefts: [Double] = []
            var edge = stretches[s].left
            for k in members {
                let left = max(items[k].x - tagWidth / 2, edge)
                lefts.append(left)
                edge = left + tagWidth + gap
            }
            edge = stretches[s].right
            for m in stride(from: members.count - 1, through: 0, by: -1) {
                lefts[m] = min(lefts[m], edge - tagWidth)
                edge = lefts[m] - gap
            }
            for m in members.indices {
                let item = items[members[m]]
                out.append(RoundtableTagPlacement(left: lefts[m], width: tagWidth, roomIds: item.roomIds,
                                                  folded: item.folded))
            }
        }
        return out
    }

    /// One tag on the pan before it is placed.
    private struct Visible {
        var x: Double
        var hz: Int64
        var roomId: String
        /// Mine or names you: never folds.
        var pinned: Bool
    }

    /// A tag or a fold, centered on x.
    private struct Item {
        var x: Double
        var roomIds: [String]
        var folded: Bool
    }

    /// A stretch of the pan tags may use.
    private struct Stretch {
        var left: Double
        var right: Double
        var capacity: Int
    }

    /// Whether a group's tags can sit side by side, each no further than
    /// one tag's width from its own frequency. With the tags `step` apart,
    /// tag k sits at start + k * step; the best start puts the largest move
    /// as small as it can be, half the spread of x_k - k * step.
    private static func slidesApart(_ visible: [Visible], _ group: [Int], step: Double, tagWidth: Double) -> Bool {
        var lowest = 0.0
        var highest = 0.0
        for (k, index) in group.enumerated() {
            let offset = visible[index].x - Double(k) * step
            lowest = k == 0 ? offset : min(lowest, offset)
            highest = k == 0 ? offset : max(highest, offset)
        }
        return (highest - lowest) / 2 <= tagWidth
    }

    /// Tags that would overlap their neighbor share a group, which reaches
    /// `reach` points at most. A group whose unpinned tags number two or
    /// more folds them when they cannot move aside, or always when
    /// `forceFold`.
    private static func groupTags(_ visible: [Visible], tagWidth: Double, gap: Double, reach: Double,
                                  forceFold: Bool) -> [Item] {
        let step = tagWidth + gap
        var groups: [[Int]] = []
        for i in visible.indices {
            if let group = groups.last, let last = group.last, let first = group.first {
                let fromLast = visible[i].x - visible[last].x
                let fromFirst = visible[i].x - visible[first].x
                if fromLast < step && fromFirst < reach {
                    groups[groups.count - 1].append(i)
                    continue
                }
            }
            groups.append([i])
        }

        var items: [Item] = []
        for group in groups {
            let unpinned = group.filter { !visible[$0].pinned }
            let fold = unpinned.count >= 2
                && (forceFold || !slidesApart(visible, group, step: step, tagWidth: tagWidth))
            for index in group where !fold || visible[index].pinned {
                items.append(Item(x: visible[index].x, roomIds: [visible[index].roomId], folded: false))
            }
            if fold {
                let sum = unpinned.reduce(0.0) { $0 + visible[$1].x }
                items.append(Item(x: sum / Double(unpinned.count), roomIds: unpinned.map { visible[$0].roomId },
                                  folded: true))
            }
        }
        return stableSorted(items) { $0.x < $1.x }
    }

    /// The pan less the keep-out stretches, each side `gap` from an edge or
    /// a keep-out, with how many tags each holds.
    private static func stretchesOf(width: Double, tagWidth: Double, gap: Double,
                                    keepOut: [RoundtableKeepOut]) -> [Stretch] {
        let blocks = stableSorted(keepOut) { $0.left < $1.left }
        var out: [Stretch] = []
        var left = gap
        func close(_ right: Double) {
            if right - left >= tagWidth {
                let capacity = Int(((right - left + gap) / (tagWidth + gap)).rounded(.down))
                out.append(Stretch(left: left, right: right, capacity: capacity))
            }
        }
        for block in blocks {
            if block.right <= 0 || block.left >= width || block.right <= block.left {
                continue
            }
            close(min(block.left - gap, width - gap))
            left = max(left, block.right + gap)
        }
        close(width - gap)
        return out
    }

    /// The stretch nearest x.
    private static func nearestStretch(_ stretches: [Stretch], x: Double) -> Int {
        var best = 0
        var bestDistance = -1.0
        for (k, stretch) in stretches.enumerated() {
            let distance = x < stretch.left ? stretch.left - x : x > stretch.right ? x - stretch.right : 0
            if bestDistance < 0 || distance < bestDistance {
                best = k
                bestDistance = distance
            }
        }
        return best
    }

    /// A sort that keeps equal elements in their order, as std::stable_sort.
    private static func stableSorted<T>(_ values: [T], by less: (T, T) -> Bool) -> [T] {
        values.enumerated().sorted { a, b in
            if less(a.element, b.element) {
                return true
            }
            if less(b.element, a.element) {
                return false
            }
            return a.offset < b.offset
        }.map(\.element)
    }

    // MARK: From the model

    /// The tags for every room in the band list. `sliceIds` are this
    /// station's slices: their rooms are mine.
    @MainActor
    static func tags(_ model: RoundtableModel, sliceIds: [Int64]) -> [RoundtableTag] {
        let mine = Set(sliceIds.compactMap { model.frequencyAvailable ? model.sliceContexts[$0]?.id : model.sliceRooms[$0] })
        var result = tags(rooms: model.frequencyRows, mine: mine, mentionCounts: model.mentionCounts, room: model.view(of:))
        let activeId = model.activeContext?.id
        let view = activeId.flatMap(model.view(of:))
        if model.frequencyAvailable {
            let rows = model.frequencyRows
            for index in result.indices {
                guard result[index].roomId == activeId,
                      let summary = rows.first(where: { $0.id == result[index].roomId }) else { continue }
                result[index].card = card(for: summary, view: view,
                                          now: Int64(Date().timeIntervalSince1970 * 1000), format: model.signalFormat)
                result[index].card?.frozen = display(model) == .stale
            }
        }
        return result
    }

    /// The tags for `rooms`, with `mine` the rooms of this station's
    /// slices and `room` each room as followed (which says your own turn).
    static func tags(rooms: [RoundtableModel.RoomSummary], mine: Set<String>, mentionCounts: [String: Int],
                     room: (String) -> RoundtableModel.Room?) -> [RoundtableTag] {
        var out: [RoundtableTag] = []
        for summary in rooms {
            guard !summary.id.isEmpty, summary.hz > 0 else {
                continue
            }
            let isMine = mine.contains(summary.id)
            var glyph = RoundtableFlagLine.glyph(ofState: summary.state)
            // U8: your own turn is cyan, in your slice's room.
            if isMine, let view = room(summary.id), RoundtableFlagLine.youAreTalking(view) {
                glyph = .you
            }
            // A double says so and names who is on it (overlay design D1, D8).
            let doubleLabel = (["Double"] + summary.keyedNames).joined(separator: "\n")
            out.append(RoundtableTag(roomId: summary.id, hz: summary.hz, glyph: glyph,
                                     label: summary.stations.isEmpty ? RoundtableFlagLine.label(glyph, talkers: summary.talkers)
                                        : glyph == .doubled ? doubleLabel
                                        : summary.stations.map { RoundtableText.withNth($0.callsign, $0.nth) }.joined(separator: "\n"),
                                     members: summary.members, mine: isMine,
                                     mentionsMe: (mentionCounts[summary.id] ?? 0) > 0))
        }
        return out
    }

    // MARK: Truth rules (overlay design D1 to D9, as the desktop's RoundtableTagLayout.cpp)

    /// D3: how far from the dial a turn or a station still counts, inclusive.
    nonisolated static let nearbyHz: Int64 = 5000
    /// D4: how long "Double ended 3s ago" stays.
    static let doubleEndedNoteMs: Int64 = 8000
    /// D11: the meter runs on the flag's S scale.
    static let meterFloorDbm = -121.0
    static let meterSpanDb = 88.0

    /// Where a reading sits on a card's meter: -121 dBm is empty, -33 dBm is full.
    static func meterFraction(_ dbm: Double) -> Double {
        min(1, max(0, (dbm - meterFloorDbm) / meterSpanDb))
    }

    /// How far a turn sits from `hz`. A legacy room's turns name no frequency:
    /// the room is the frequency. A double's turn.hz is its RF room's center, so
    /// an endpoint or a listener's reading of it at `hz` puts it on this dial.
    private static func turnDistance(_ turn: RoundtableModel.Turn, hz: Int64, view: RoundtableModel.Room) -> Int64 {
        guard let turnHz = turn.hz, turnHz > 0 else { return 0 }
        if turn.kind == "doubled" {
            let source = turn.sourceRoom
            if turn.talkers.contains(where: { $0.hz == hz && $0.sourceRoom == source })
                || view.members.contains(where: { $0.hz == hz && $0.sourceRoom == source && $0.reading?.turn == turn.id }) {
                return 0
            }
        }
        return abs(turnHz - hz)
    }

    /// D3: the live turn to show: the exact-Hz one, else the nearest within the
    /// window in this family, and on a tie the one that started last. A
    /// frequency view skips a turn with no source room: its id means nothing
    /// outside one.
    static func liveTurn(hz: Int64, family: String, view: RoundtableModel.Room) -> RoundtableModel.Turn? {
        guard view.family == family else { return nil }
        var best: (turn: RoundtableModel.Turn, distance: Int64)?
        for turn in view.now where turn.end == nil && (!view.frequencyView || turn.sourceRoom != nil) {
            let distance = turnDistance(turn, hz: hz, view: view)
            guard distance <= nearbyHz else { continue }
            if let current = best, (distance, -turn.start) >= (current.distance, -current.turn.start) { continue }
            best = (turn, distance)
        }
        return best?.turn
    }

    static func liveTurn(summary: RoundtableModel.RoomSummary, view: RoundtableModel.Room) -> RoundtableModel.Turn? {
        liveTurn(hz: summary.hz, family: summary.family, view: view)
    }

    /// The live turn, or only when none is near, the one that ended last.
    static func chosenTurn(hz: Int64, family: String,
                           view: RoundtableModel.Room) -> (turn: RoundtableModel.Turn, historical: Bool)? {
        if let live = liveTurn(hz: hz, family: family, view: view) { return (live, false) }
        guard view.family == family else { return nil }
        let ended = view.earlier.filter {
            (!view.frequencyView || $0.sourceRoom != nil) && turnDistance($0, hz: hz, view: view) <= nearbyHz
        }.max { ($0.end ?? $0.start) < ($1.end ?? $1.start) }
        return ended.map { ($0, true) }
    }

    static func chosenTurn(in room: RoundtableModel.Room) -> (turn: RoundtableModel.Turn, historical: Bool)? {
        chosenTurn(hz: room.hz, family: room.family, view: room)
    }

    /// Who a turn's talkers are. D5: a live double's current participants, those
    /// in its source room still keyed, so one who has gone is dropped; an ended
    /// double keeps every name. A legacy room has no source room to check against.
    static func participants(of turn: RoundtableModel.Turn, in view: RoundtableModel.Room) -> [RoundtableModel.Talker] {
        guard view.frequencyView, turn.kind == "doubled" else { return turn.talkers }
        let source = turn.sourceRoom
        let inSource = turn.talkers.filter { $0.sourceRoom == nil || $0.sourceRoom == source }
        guard turn.end == nil else { return inSource }
        return inSource.filter { talker in
            guard let ref = talker.ref else { return false }
            let talkerSource = talker.sourceRoom ?? source
            return view.members.contains { $0.ref == ref && $0.sourceRoom == talkerSource && $0.keyed }
        }
    }

    /// The member ids of a turn's current talkers, in the turn's source room.
    static func talkerIds(of turn: RoundtableModel.Turn, in view: RoundtableModel.Room) -> Set<String> {
        Set(participants(of: turn, in: view).compactMap { talker in
            talker.ref.map { ref in ((talker.sourceRoom ?? turn.sourceRoom).map { $0 + "/" } ?? "") + ref }
        })
    }

    /// D2, D9: the listeners with a usable reading of this turn, nearest first.
    /// Never a transmitter, and turn ids do not cross a source room.
    private static func reporters(of turn: RoundtableModel.Turn, in view: RoundtableModel.Room,
                                  historical: Bool) -> [RoundtableModel.Member] {
        let talkers = talkerIds(of: turn, in: view)
        return view.members.filter { member in
            member.sourceRoom == turn.sourceRoom && !talkers.contains(member.id) && (historical || !member.keyed)
                && member.reading?.turn == turn.id && RoundtableText.validSignal(member.reading) != nil
        }.sorted { ($0.km ?? Int64.max, $0.id) < ($1.km ?? Int64.max, $1.id) }
    }

    /// The tag's card, by the truth rules above. A frequency whose stream has
    /// not arrived says only what the directory knows (D1).
    static func card(for summary: RoundtableModel.RoomSummary, view: RoundtableModel.Room?, now: Int64,
                     format: RoundtableSignalFormat = .dbm) -> RoundtableTagCard? {
        let known = view?.family == summary.family ? view : nil
        guard let known, let chosen = chosenTurn(hz: summary.hz, family: summary.family, view: known) else {
            var card = RoundtableTagCard(format: format)
            if known == nil || (known?.members.isEmpty == true && known?.now.isEmpty == true) {
                if summary.state == "doubled" {
                    card.state = .doubled
                    card.participants = summary.keyedNames
                } else if summary.state == "talking", summary.keyedNames.count == 1 {
                    card.state = .known(summary.keyedNames[0])
                }
            }
            return card
        }
        let turn = chosen.turn
        var card = RoundtableTagCard(ended: chosen.historical, format: format)
        if turn.kind == "doubled" {
            card.state = .doubled
            card.participants = participants(of: turn, in: known).filter { !$0.callsign.isEmpty }
                .map { RoundtableText.withNth($0.callsign, $0.nth) }
        } else if turn.kind == "unknown" {
            card.state = .unknown
        } else if turn.talkers.count == 1 {
            card.state = .known(RoundtableText.withNth(turn.talkers[0].callsign, turn.talkers[0].nth))
        } else {
            card.state = .unattributed
            return card
        }
        card.reports = reporters(of: turn, in: known, historical: chosen.historical).compactMap { member in
            guard let reading = member.reading else { return nil }
            return .init(id: member.id, reporter: RoundtableText.withNth(member.callsign, member.nth),
                         reading: reading, age: RoundtableText.ageText(now - reading.t),
                         stale: now - reading.t > RoundtableText.staleReadingMs)
        }
        return card
    }

    /// What the card for one frequency says (the desktop's roundtableCardModelFor).
    /// `peaks` holds each listener's peak; nil leaves the ticks off.
    static func cardData(summary: RoundtableModel.RoomSummary, view: RoundtableModel.Room?, now: Int64,
                         link: RoundtableModel.LinkState, linkSince: Int64,
                         peaks: RoundtablePeakTracker? = nil) -> RoundtableCardData {
        var card = RoundtableCardData(hz: summary.hz)
        // D7: a dropped link freezes, and says which way it dropped.
        card.frozen = link == .reconnecting || link == .offline
        let linkNote: String?
        switch link {
        case .reconnecting: linkNote = "Reconnecting"
        case .offline: linkNote = linkSince > 0 ? "Offline since \(RoundtableText.utcText(linkSince)) UTC" : "Offline"
        case .hidden: linkNote = "You are hidden"
        case .off, .connecting, .live: linkNote = nil
        }
        card.note = linkNote
        card.people = summary.stations.map { RoundtableText.withNth($0.callsign, $0.nth) }
        let known = view?.family == summary.family ? view : nil

        guard let known, let chosen = chosenTurn(hz: summary.hz, family: summary.family, view: known) else {
            // Known only from the directory (D1): who is keyed, with no readings.
            if known == nil || (known?.members.isEmpty == true && known?.now.isEmpty == true) {
                switch summary.state {
                case "doubled":
                    card.kind = .double
                    card.keyed = summary.keyedNames
                case "talking" where !summary.keyedNames.isEmpty:
                    card.kind = .talking
                    card.keyed = summary.keyedNames
                case "unknown":
                    card.kind = .unknown
                default:
                    break
                }
            }
            return card
        }

        let turn = chosen.turn
        let doubled = turn.kind == "doubled"
        let people = participants(of: turn, in: known)
        card.keyed = people.filter { !$0.callsign.isEmpty }.map { RoundtableText.withNth($0.callsign, $0.nth) }
        if chosen.historical {
            card.kind = doubled ? .lastDouble : .lastTalker
            card.endedAgo = max(0, now - (turn.end ?? now))
        } else {
            if doubled {
                card.kind = .double
            } else if RoundtableFlagLine.turnIsYours(known, turn, open: true) {
                card.kind = .you
            } else if turn.kind == "unknown" || card.keyed.isEmpty {
                card.kind = .unknown
                card.keyed = []
            } else {
                card.kind = .talking
            }
            // The clock stops where the link dropped: past that nothing is known.
            card.clock = max(0, (card.frozen && linkSince > 0 ? linkSince : now) - turn.start)
            // A double's turn.hz is its room's center: tune to who is on it.
            let exact = doubled ? people.compactMap(\.hz).min { abs($0 - summary.hz) < abs($1 - summary.hz) } : nil
            card.liveHz = exact ?? turn.hz ?? summary.hz
        }

        // D4: the remaining talker's new turn says the double just ended. Its
        // rows come only from readings of this turn, so none carry over. A link
        // note takes the one line while the link is not live.
        if !chosen.historical, !doubled, linkNote == nil {
            let newestEnd = known.earlier.filter {
                $0.kind == "doubled" && $0.sourceRoom == turn.sourceRoom
                    && turnDistance($0, hz: summary.hz, view: known) <= nearbyHz
            }.compactMap(\.end).max()
            if let newestEnd, max(0, now - newestEnd) < doubleEndedNoteMs {
                card.note = "Double ended \(max(0, now - newestEnd) / 1000)s ago"
            }
        }

        // D2, D9, D12, D13: present listeners, never a transmitter; combined RF
        // has no path; a tick only on a fresh reading of a live turn on a live link.
        let tracker = chosen.historical || card.frozen ? nil : peaks
        card.rows = reporters(of: turn, in: known, historical: chosen.historical).compactMap { member in
            guard let reading = member.reading else { return nil }
            let age = max(0, now - reading.t)
            let old = age > RoundtableText.staleReadingMs
            let held = tracker?.update(sourceRoom: member.sourceRoom ?? "", ref: member.ref, turn: turn.id,
                                       dbm: reading.dbm, now: now)
            return RoundtableCardData.Row(call: RoundtableText.withNth(member.callsign, member.nth), dbm: reading.dbm,
                                          age: age, km: doubled ? nil : member.km,
                                          bearing: doubled ? nil : member.bearingDeg, offsetHz: member.offsetHz,
                                          you: member.you, old: old, peak: old ? nil : held)
        }
        return card
    }

    /// Fit whole semantic rows. A narrow strip discloses measurements through
    /// Reports instead of splitting words or squeezing three columns into it.
    static func cardPlan(_ tag: RoundtableTag, width: Double, maxRows: Int = 30) -> RoundtableTagCardPlan {
        guard let card = tag.card else { return RoundtableTagCardPlan() }
        var plan = RoundtableTagCardPlan(leadingPadding: width < 100 ? 4 : padLeft,
                                        trailingPadding: width < 100 ? 4 : padRight,
                                        signalTitle: card.format.title,
                                        reportHeight: card.format == .both ? 30 : 18)
        plan.omittedReports = card.reports.count
        plan.omittedParticipants = card.participants.count
        var remaining = CGFloat(max(0, maxRows)) * 13
        guard remaining >= 18 else { return plan }
        plan.showsDetails = true
        remaining -= 18
        if remaining >= 14 { plan.showsFrequency = true; remaining -= 14 }
        let stateHeight: CGFloat = card.state.title == nil ? 0 : card.caption == nil ? 16 : 29
        if remaining >= stateHeight {
            plan.stateCaption = card.caption
            plan.stateTitle = card.state == .doubled && card.participants.count > 2
                ? "Double \u{00B7} \(card.participants.count)" : card.state.title
            if let title = plan.stateTitle {
                let natural = (title as NSString).size(withAttributes: [.font: UIFont.systemFont(ofSize: 10, weight: .medium)]).width
                let innerWidth = max(1, CGFloat(width) - plan.leadingPadding - plan.trailingPadding - 1)
                plan.stateFontSize = max(minimumStateFontSize, min(10, 10 * innerWidth / max(1, natural)))
                while plan.stateFontSize > minimumStateFontSize,
                      (title as NSString).size(withAttributes: [.font: UIFont.systemFont(
                        ofSize: plan.stateFontSize, weight: .medium)]).width > innerWidth {
                    plan.stateFontSize = max(minimumStateFontSize, plan.stateFontSize - 0.25)
                }
            }
            remaining -= stateHeight
            // Participant identities are authoritative; the details link
            // discloses any that do not fit as complete rows.
            for name in card.participants {
                let inner = CGFloat(width) - plan.leadingPadding - plan.trailingPadding
                let fits = (name as NSString).size(withAttributes: [.font: UIFont.systemFont(ofSize: 10, weight: .semibold)]).width <= inner
                guard remaining >= 16 else { break }
                guard fits else { continue }
                plan.participants.append(name)
                remaining -= 16
            }
            plan.omittedParticipants = card.participants.count - plan.participants.count
            if width >= 160, !card.reports.isEmpty, remaining >= 17 + plan.reportHeight {
                remaining -= 17
                for report in card.reports {
                    let reporterWidth = CGFloat(width) - plan.leadingPadding - plan.trailingPadding - 96
                    let fits = (report.reporter as NSString).size(withAttributes: [.font: UIFont.systemFont(ofSize: 10, weight: .semibold)]).width <= reporterWidth
                    guard remaining >= plan.reportHeight else { break }
                    guard fits else { continue }
                    plan.reports.append(.init(id: report.id, reporter: report.reporter,
                        signal: RoundtableText.signalLines(report.reading, format: card.format),
                        age: report.age, stale: report.stale))
                    remaining -= plan.reportHeight
                }
                if plan.reports.isEmpty { remaining += 17 }
            }
            plan.omittedReports = card.reports.count - plan.reports.count
        }
        // Presence is secondary to live RF and remains available for a waiting card.
        if card.state == .waiting {
            let people = tag.label.split(separator: "\n").map(String.init)
            var shown = min(width < 160 ? 2 : 3, people.count)
            func presenceHeight(_ count: Int) -> CGFloat {
                CGFloat(count) * 16 + (count < people.count ? 13 : 0)
            }
            while shown > 0, presenceHeight(shown) > remaining { shown -= 1 }
            if presenceHeight(shown) <= remaining {
                plan.people = Array(people.prefix(shown))
                plan.omittedPeople = people.count - shown
                remaining -= presenceHeight(shown)
            }
        }
        plan.height = CGFloat(max(0, maxRows)) * 13 - remaining + 8
        return plan
    }

    static func tagHeight(_ tag: RoundtableTag, width: Double = 80, maxRows: Int = 30) -> CGFloat {
        if tag.card == nil { return max(height, CGFloat(min(3, tag.label.split(separator: "\n").count)) * 14 + 8) }
        return cardPlan(tag, width: width, maxRows: maxRows).height
    }

    /// Hidden while Roundtable is off, unavailable or connecting; stale
    /// while reconnecting or offline (overlay design D7); dimmed while you are
    /// hidden (you still see everything, D21); live otherwise.
    static func display(available: Bool, link: RoundtableModel.LinkState) -> RoundtableTagDisplay {
        guard available else {
            return .hidden
        }
        switch link {
        case .off, .connecting:
            return .hidden
        case .reconnecting, .offline:
            return .stale
        case .hidden:
            return .dimmed
        case .live:
            return .live
        }
    }

    @MainActor
    static func display(_ model: RoundtableModel) -> RoundtableTagDisplay {
        display(available: model.available, link: model.linkState)
    }

    /// A room's frequency as the +N list shows it: MHz with three
    /// decimals, more when the room is between kHz ("7.185", "7.0305").
    static func frequencyText(_ hz: Int64) -> String {
        var decimals = 6
        if hz % 1000 == 0 {
            decimals = 3
        } else if hz % 100 == 0 {
            decimals = 4
        } else if hz % 10 == 0 {
            decimals = 5
        }
        return String(format: "%.\(decimals)f", Double(hz) / 1.0e6)
    }

    /// Who a fold's row names: "Unknown talker" for an unknown, else the label.
    static func foldWho(_ tag: RoundtableTag) -> String {
        if tag.glyph == .doubled {
            return tag.label.split(separator: "\n").joined(separator: " \u{00B7} ")
        }
        return tag.glyph == .unknown ? "Unknown talker" : tag.label
    }

    /// The +N list's heading.
    static func foldHeading(count: Int, sliceLetter: String) -> String {
        "\(count) frequencies here \u{00B7} a tap tunes slice \(sliceLetter)"
    }

    /// One width for every tag and fold: the widest tag's, within bounds.
    static func tagWidth(_ tags: [RoundtableTag]) -> Double {
        let label = UIFont.systemFont(ofSize: labelFontSize, weight: .bold)
        let count = UIFont.systemFont(ofSize: labelFontSize, weight: .semibold)
        let space = (" " as NSString).size(withAttributes: [.font: label]).width
        let fixed = 2 + padLeft + glyphSize + innerGap + padRight
        var natural = minWidth
        for tag in tags {
            let text = (tag.label as NSString).size(withAttributes: [.font: label]).width
            let members = ("\(tag.members)" as NSString).size(withAttributes: [.font: count]).width
            natural = max(natural, Double((fixed + text + space + members).rounded(.up)))
        }
        return tags.contains(where: { $0.card != nil }) ? reportWidth : min(natural, maxWidth)
    }
}

// MARK: The view

/// The tags along the foot of the spectrum, 8 points above the band-plan
/// strip and clear of the flags: a tap on one tunes the active slice to its
/// room; a tap on +N lists its rooms by frequency, and a row tunes. Keyed,
/// a tag tunes nothing. Offline greys them as last known; connecting hides
/// them; while you are hidden their marking is dimmed.
struct RoundtableTagLayer: View {
    @ObservedObject var roundtable: RoundtableModel
    let sliceIds: [Int64]
    /// The pan: its width and the frequency at its left edge.
    let geometryWidth: CGFloat
    let lowHz: Double
    let hzPerPoint: Double
    /// The tag row's top.
    let top: CGFloat
    /// The band-plan strip's top, where each tag's tick ends.
    let tickFoot: CGFloat
    /// The flags' rects on the band, which the tags keep clear of.
    let flagRects: [CGRect]
    let keyed: Bool
    let sliceLetter: String
    /// Reads reports without tuning, using the main screen’s existing sheet.
    var readReports: ((String) -> Void)? = nil
    /// Tunes the active slice to a room.
    let tune: (RoundtableModel.RoomSummary) -> Void

    @State private var openFold: [String]?
    @State private var detailsTag: RoundtableTag?
    @State private var showDetails = false

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            tagContent
        }
    }

    @ViewBuilder
    private var tagContent: some View {
        let display = RoundtableTagLayout.display(roundtable)
        let tags = RoundtableTagLayout.tags(roundtable, sliceIds: sliceIds)
        if display != .hidden, !tags.isEmpty, top >= 0 {
            let maximumRows = max(0, Int((top + RoundtableTagLayout.height - 8) / 13))
            let bounds = Self.contentBounds(tags, maximumRows: maximumRows, flagRects: flagRects,
                                            top: top, available: geometryWidth)
            let keepOut = bounds.keepOut
            let width = bounds.width
            let placements = RoundtableTagLayout.layout(tags, hzPerPoint: hzPerPoint,
                                                        leftHz: Int64(lowHz.rounded()),
                                                        width: Double(geometryWidth), tagWidth: width,
                                                        gap: RoundtableTagLayout.gap, keepOut: keepOut)
            let byId = Dictionary(tags.map { ($0.roomId, $0) }, uniquingKeysWith: { first, _ in first })
            let stale = display == .stale
            let dimmed = display == .dimmed
            ZStack(alignment: .topLeading) {
                ticks(placements, byId: byId, stale: stale, dimmed: dimmed)
                ForEach(placements, id: \.roomIds) { placement in
                    tagView(placement, byId: byId, stale: stale, dimmed: dimmed)
                        .offset(x: CGFloat(placement.left), y: top - stackExtra(placement, byId: byId))
                }
                if let fold = openFold, let placement = placements.first(where: { $0.roomIds == fold }) {
                    // A tap anywhere else closes the list, and tunes nothing.
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { openFold = nil }
                        .accessibilityLabel("Close the list of rooms")
                        .accessibilityAddTraits(.isButton)
                    foldList(placement, byId: byId)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .confirmationDialog("People at " + (detailsTag.map { RoundtableTagLayout.frequencyText($0.hz) } ?? "this frequency"),
                                isPresented: $showDetails, titleVisibility: .visible) {
                if let tag = detailsTag {
                    Button("Read reports") { readReports?(tag.roomId) }
                    Button("Signal: \(roundtable.signalFormat.title) · Change") { roundtable.cycleSignalFormat() }
                    Button("Tune slice \(sliceLetter) here") { tuneTo(tag.roomId) }.disabled(keyed)
                }
                Button("Cancel", role: .cancel) {}
            }
            .onChange(of: placements.map(\.roomIds)) { _, next in
                if let fold = openFold, !next.contains(fold) {
                    openFold = nil
                }
            }
        }
    }

    /// Width can cross the narrow-strip threshold after flags are excluded.
    /// Recompute clearance from the final rows at that actual width; the
    /// monotone width bound settles after the wide/narrow presentation switch.
    static func contentBounds(_ tags: [RoundtableTag], maximumRows: Int, flagRects: [CGRect],
                              top: CGFloat, available: CGFloat) -> (width: Double, keepOut: [RoundtableKeepOut]) {
        var width = RoundtableTagLayout.tagWidth(tags)
        func spans(_ width: Double) -> [RoundtableKeepOut] {
            let growth = tags.map { max(0, RoundtableTagLayout.tagHeight($0, width: width,
                maxRows: maximumRows) - RoundtableTagLayout.height) }.max() ?? 0
            let protected = flagRects.map { CGRect(x: $0.minX, y: $0.minY,
                width: $0.width, height: $0.height + growth) }
            return keepOut(protected, top: top, width: available)
        }
        for _ in 0..<3 {
            let keepOut = spans(width)
            let fitted = min(width, fittingWidth(tags, keepOut: keepOut, available: Double(available)))
            if fitted == width { return (width, keepOut) }
            width = fitted
        }
        return (width, spans(width))
    }

    /// Portrait flags can leave a narrow strip. Bound the compact box to the
    /// actual free stretch, including gap on both sides, so it remains visible.
    static func fittingWidth(_ tags: [RoundtableTag], keepOut: [RoundtableKeepOut], available: Double) -> Double {
        var edge = RoundtableTagLayout.gap
        var freeWidth = 0.0
        for span in keepOut.sorted(by: { $0.left < $1.left }) {
            freeWidth = max(freeWidth, span.left - RoundtableTagLayout.gap - edge)
            edge = max(edge, span.right + RoundtableTagLayout.gap)
        }
        freeWidth = max(freeWidth, available - RoundtableTagLayout.gap - edge)
        return min(RoundtableTagLayout.tagWidth(tags), max(1, freeWidth))
    }

    /// The flags that stand in the tag row, as spans the tags keep off. A
    /// flag that only meets the row's edge (a flag stacked just above it,
    /// whose rect ends a fraction of a point into the row) leaves it free.
    static func keepOut(_ flagRects: [CGRect], top: CGFloat, width: CGFloat) -> [RoundtableKeepOut] {
        let row = CGRect(x: 0, y: top, width: width, height: RoundtableTagLayout.height)
            .insetBy(dx: 0, dy: keepOutTolerance)
        return flagRects.filter { $0.intersects(row) }.map {
            RoundtableKeepOut(left: Double($0.minX), right: Double($0.maxX))
        }
    }

    /// How far a flag may reach into the tag row's top or bottom edge before it counts.
    static let keepOutTolerance: CGFloat = 1

    /// A dashed tick from each tag down to its room's frequency.
    private func ticks(_ placements: [RoundtableTagPlacement], byId: [String: RoundtableTag],
                       stale: Bool, dimmed: Bool) -> some View {
        let foot = tickFoot
        let start = top + RoundtableTagLayout.height
        return ZStack(alignment: .topLeading) {
            ForEach(placements.flatMap(\.roomIds), id: \.self) { id in
                if let tag = byId[id] {
                    let x = CGFloat((Double(tag.hz) - lowHz) / hzPerPoint).rounded() + 0.5
                    let colour = stale ? RoundtableColours.stale : Self.border(tag.glyph, dimmed: dimmed)
                    Path { path in
                        path.move(to: CGPoint(x: x, y: start))
                        path.addLine(to: CGPoint(x: x, y: max(start, foot - 1)))
                    }
                    .stroke(colour.opacity(0.7),
                            style: StrokeStyle(lineWidth: 1, dash: [3, 2]))
                }
            }
        }
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }

    @ViewBuilder
    private func tagView(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag],
                         stale: Bool, dimmed: Bool) -> some View {
        let tags = placement.roomIds.compactMap { byId[$0] }
        let first = tags.first
        let cardPlan = first.flatMap { tag in
            tag.card == nil ? nil : RoundtableTagLayout.cardPlan(tag, width: placement.width,
                maxRows: max(0, Int((top + RoundtableTagLayout.height - 8) / 13)))
        }
        let border = stale ? Self.staleBorder : placement.folded ? Self.foldBorder
            : first.map { Self.border($0.glyph, dimmed: dimmed) } ?? Self.foldBorder
        let open = placement.folded && openFold == placement.roomIds
        Button {
            tapped(placement, byId: byId)
        } label: {
            HStack(spacing: placement.folded ? 4 : RoundtableTagLayout.innerGap) {
                if placement.folded {
                    ZStack(alignment: .leading) {
                        ForEach(Array(tags.prefix(RoundtableTagLayout.foldStackMax).enumerated()), id: \.offset) {
                            index, tag in
                            glyph(tag.glyph, stale: stale, dimmed: dimmed)
                                .offset(x: CGFloat(index) * RoundtableTagLayout.foldStackStep)
                        }
                    }
                    .frame(width: RoundtableTagLayout.glyphSize
                           + CGFloat(min(tags.count, RoundtableTagLayout.foldStackMax) - 1)
                           * RoundtableTagLayout.foldStackStep, alignment: .leading)
                    Text("+\(placement.roomIds.count)")
                        .font(.system(size: RoundtableTagLayout.labelFontSize, weight: .bold))
                        .foregroundStyle(stale ? RoundtableColours.dim : RoundtableColours.text)
                } else if let tag = first {
                    if tag.card != nil {
                        cardView(tag, width: placement.width, stale: stale)
                    } else {
                        glyph(tag.glyph, stale: stale, dimmed: dimmed)
                        Text(tag.label).font(.system(size: RoundtableTagLayout.labelFontSize, weight: .bold))
                            .foregroundStyle(stale ? RoundtableColours.dim : Self.labelColour)
                            .lineLimit(3).truncationMode(.tail)
                    }
                }
            }
            // Leading, with no Spacer: a Spacer would take a gap of its own
            // that tagWidth does not count, and cut the label short.
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.leading, placement.folded ? 5 : cardPlan?.leadingPadding ?? RoundtableTagLayout.padLeft)
            .padding(.trailing, placement.folded ? RoundtableTagLayout.padRight
                     : cardPlan?.trailingPadding ?? RoundtableTagLayout.padRight)
            .frame(width: CGFloat(placement.width), height: placement.folded ? RoundtableTagLayout.height
                   : first.map { RoundtableTagLayout.tagHeight($0, width: placement.width,
                       maxRows: max(0, Int((top + RoundtableTagLayout.height - 8) / 13))) } ?? RoundtableTagLayout.height)
            .background(RoundedRectangle(cornerRadius: 4) // kept-radius: roundtable-tag
                .fill(open ? Self.openFill : Self.fill))
            .overlay(RoundedRectangle(cornerRadius: 4) // kept-radius: roundtable-tag
                .strokeBorder(open ? Self.openBorder : border, lineWidth: 1))
            .overlay {
                if !placement.folded, first?.mine == true, !stale {
                    RoundedRectangle(cornerRadius: 5) // kept-radius: roundtable-tag
                        .strokeBorder(dimmed ? Self.mineRing.opacity(RoundtableColours.dimmedOpacity)
                                             : Self.mineRing, lineWidth: 1)
                        .padding(-1)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(accessibilityLabel(placement, tags: tags))
        .accessibilityHint(placement.folded ? "Lists the rooms inside"
            : first?.card != nil && readReports != nil ? "Offers Read reports, signal display and Tune slice \(sliceLetter) here"
            : "Tunes slice \(sliceLetter) there")
        .accessibilityIdentifier(placement.folded ? "roundtableFold" : "roundtableTag.\(placement.roomIds[0])")
        .accessibilityActions {
            if !placement.folded, let tag = first, tag.card != nil {
                if let readReports { Button("Read reports") { readReports(tag.roomId) } }
                Button("Change signal display") { roundtable.cycleSignalFormat() }
            }
        }
    }

    private func cardView(_ tag: RoundtableTag, width: Double, stale: Bool) -> some View {
        let plan = RoundtableTagLayout.cardPlan(tag, width: width,
            maxRows: max(0, Int((top + RoundtableTagLayout.height - 8) / 13)))
        return VStack(alignment: .leading, spacing: 0) {
            if plan.showsFrequency {
                Text(RoundtableTagLayout.frequencyText(tag.hz) + " MHz")
                    .font(.system(size: 10, weight: .semibold).monospacedDigit())
                    .foregroundStyle(stale ? RoundtableColours.dim : RoundtableColours.accentText)
                    .frame(height: 14, alignment: .leading)
            }
            ForEach(Array(plan.people.enumerated()), id: \.offset) { _, person in
                Text(person).font(.system(size: 11, weight: .semibold))
                    .frame(height: 16, alignment: .leading)
            }
            if plan.omittedPeople > 0 {
                Text("+\(plan.omittedPeople) people").font(.system(size: 10))
                    .foregroundStyle(RoundtableColours.dim).frame(height: 13, alignment: .leading)
            }
            if let caption = plan.stateCaption {
                Text(caption).font(.system(size: 9)).foregroundStyle(RoundtableColours.dim)
                    .minimumScaleFactor(RoundtableTagLayout.minimumStateFontSize / 9)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .frame(height: 13, alignment: .bottomLeading)
            }
            if let title = plan.stateTitle {
                Text(title).font(.system(size: plan.stateFontSize, weight: .medium))
                    .minimumScaleFactor(RoundtableTagLayout.minimumStateFontSize / plan.stateFontSize)
                    .truncationMode(.middle)
                    .foregroundStyle(stale ? RoundtableColours.dim : Self.cardStateColour(tag.card?.state))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .frame(height: 16, alignment: .leading)
            }
            ForEach(Array(plan.participants.enumerated()), id: \.offset) { _, participant in
                Text(participant).font(.system(size: 10, weight: .semibold))
                    .frame(height: 16, alignment: .leading)
            }
            if !plan.reports.isEmpty {
                HStack(spacing: 4) {
                    Text(tag.card?.state == .doubled ? "Combined RF" : "Heard by").frame(maxWidth: .infinity, alignment: .leading)
                    Text(plan.signalTitle).frame(width: 61, alignment: .trailing)
                    Text("Age").frame(width: 27, alignment: .trailing)
                }
                .font(.system(size: 9)).foregroundStyle(RoundtableColours.dim)
                .frame(height: 17)
                .overlay(alignment: .top) { Rectangle().fill(RoundtableColours.rule).frame(height: 1) }
                ForEach(plan.reports) { report in
                    HStack(spacing: 4) {
                        Text(report.reporter).font(.system(size: 10, weight: .semibold))
                            .frame(maxWidth: .infinity, alignment: .leading)
                        VStack(alignment: .trailing, spacing: 0) {
                            ForEach(Array(report.signal.enumerated()), id: \.offset) { index, line in
                                Text(line).font(.system(size: index == 0 ? 10 : 9, weight: .semibold).monospacedDigit())
                            }
                        }.frame(width: 61, alignment: .trailing)
                        Text(report.age).font(.system(size: 9).monospacedDigit())
                            .foregroundStyle(report.stale ? RoundtableColours.unknown : RoundtableColours.dim)
                            .frame(width: 27, alignment: .trailing)
                    }
                    .foregroundStyle(stale || report.stale ? RoundtableColours.stale : Self.labelColour)
                    .frame(height: plan.reportHeight)
                }
            }
            if plan.showsDetails {
                Text(plan.omittedReports > 0 ? "+\(plan.omittedReports) reports ›"
                     : plan.omittedParticipants > 0 ? "+\(plan.omittedParticipants) TX ›" : "Reports ›").font(.system(size: 10, weight: .medium))
                    .foregroundStyle(stale ? RoundtableColours.dim : RoundtableColours.accentText)
                    .frame(height: 18, alignment: .leading)
            }
        }
        .lineLimit(1).minimumScaleFactor(0.85)
        .foregroundStyle(stale ? RoundtableColours.dim : Self.labelColour)
        .padding(.vertical, 4)
    }

    private static func cardStateColour(_ state: RoundtableTagCard.State?) -> Color {
        switch state {
        case .some(.unknown), .some(.unattributed): RoundtableColours.unknown
        case .some(.doubled): RoundtableColours.doubled
        default: labelColour
        }
    }

    private func stackExtra(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag]) -> CGFloat {
        guard !placement.folded, let id = placement.roomIds.first, let tag = byId[id] else { return 0 }
        return max(0, RoundtableTagLayout.tagHeight(tag, width: placement.width,
            maxRows: max(0, Int((top + RoundtableTagLayout.height - 8) / 13))) - RoundtableTagLayout.height)
    }

    private func glyph(_ glyph: RoundtableFlagLine.Glyph, stale: Bool, dimmed: Bool) -> some View {
        RoundtableGlyphView(glyph: glyph, size: RoundtableTagLayout.glyphSize, dimmed: dimmed && !stale)
            .saturation(stale ? 0 : 1)
            .opacity(stale ? 0.6 : 1)
    }

    private func accessibilityLabel(_ placement: RoundtableTagPlacement, tags: [RoundtableTag]) -> String {
        if placement.folded {
            return "\(placement.roomIds.count) Roundtable rooms"
        }
        guard let tag = tags.first else {
            return "Roundtable"
        }
        return "Roundtable \(RoundtableTagLayout.frequencyText(tag.hz)), \(tag.label), \(tag.members) stations. "
            + (tag.card?.accessibilityText ?? "")
    }

    private func tapped(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag]) {
        if placement.folded {
            openFold = openFold == placement.roomIds ? nil : placement.roomIds
            return
        }
        openFold = nil
        if let id = placement.roomIds.first, let tag = byId[id] {
            if tag.card != nil, readReports != nil {
                detailsTag = tag
                showDetails = true
            } else { tuneTo(id) }
        }
    }

    /// Keyed, a tag tunes nothing.
    private func tuneTo(_ roomId: String) {
        guard !keyed, let summary = roundtable.frequencyRows.first(where: { $0.id == roomId }) else {
            return
        }
        tune(summary)
    }

    /// The +N list: a heading, then the rooms by frequency; a row tunes.
    @ViewBuilder
    private func foldList(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag]) -> some View {
        let tags = placement.roomIds.compactMap { byId[$0] }
        let listHeight = 24 + CGFloat(tags.count) * RoundtableTagLayout.foldRowHeight + 8
        let x = min(max(4, CGFloat(placement.centerX) - RoundtableTagLayout.foldListWidth / 2),
                    max(4, geometryWidth - RoundtableTagLayout.foldListWidth - 4))
        let y = max(4, top - 6 - listHeight)
        VStack(alignment: .leading, spacing: 0) {
            Text(RoundtableTagLayout.foldHeading(count: tags.count, sliceLetter: sliceLetter))
                .font(.system(size: 11))
                .foregroundStyle(RoundtableColours.dim)
                .lineLimit(1)
                .padding(.horizontal, 10)
                .frame(height: 24, alignment: .center)
            Rectangle().fill(Self.listRule).frame(height: 1)
            ForEach(Array(tags.enumerated()), id: \.element.roomId) { index, tag in
                if index > 0 {
                    Rectangle().fill(Self.rowRule).frame(height: 1)
                }
                Button {
                    openFold = nil
                    tuneTo(tag.roomId)
                } label: {
                    HStack(spacing: 7) {
                        RoundtableGlyphView(glyph: tag.glyph, size: RoundtableTagLayout.glyphSize,
                                            dimmed: RoundtableTagLayout.display(roundtable) == .dimmed)
                        Text(RoundtableTagLayout.frequencyText(tag.hz))
                            .font(.system(size: 12, weight: .bold, design: .monospaced))
                            .foregroundStyle(Self.labelColour)
                        Text(RoundtableTagLayout.foldWho(tag))
                            .font(.system(size: 12, weight: .semibold))
                            .foregroundStyle(Self.rowText)
                            .lineLimit(1)
                            .truncationMode(.tail)
                        Spacer(minLength: 0)
                        Text("\(tag.members)")
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(RoundtableColours.dim)
                    }
                    .padding(.horizontal, 10)
                    .frame(height: RoundtableTagLayout.foldRowHeight - (index > 0 ? 1 : 0))
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("\(RoundtableTagLayout.frequencyText(tag.hz)), "
                                    + "\(RoundtableTagLayout.foldWho(tag)), \(tag.members) stations")
                .accessibilityIdentifier("roundtableFoldRow.\(tag.roomId)")
            }
        }
        .padding(.vertical, 4)
        .frame(width: RoundtableTagLayout.foldListWidth, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).fill(Self.listFill))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(Self.listBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.6), radius: 12, y: 10)
        .offset(x: x, y: y)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFoldList")
    }

    // MARK: Colours (board.html's .rt-tag, .rt-fold and .rt-foldlist)

    static let fill = Color(red: 10 / 255, green: 10 / 255, blue: 12 / 255).opacity(0.88)
    static let openFill = Color(red: 0x1E / 255, green: 0x3D / 255, blue: 0x5F / 255)
    static let openBorder = Color(red: 0x5E / 255, green: 0x8D / 255, blue: 0xB8 / 255)
    static let foldBorder = Color(red: 0x71 / 255, green: 0x71 / 255, blue: 0x75 / 255)
    static let staleBorder = Color(red: 0x60 / 255, green: 0x60 / 255, blue: 0x66 / 255)
    static let mineRing = Color(red: 0, green: 0xD4 / 255, blue: 1).opacity(0.35)
    static let labelColour = Color(red: 0xEC / 255, green: 0xEC / 255, blue: 0xF0 / 255)
    static let rowText = Color(red: 0xE8 / 255, green: 0xE8 / 255, blue: 0xEC / 255)
    static let listFill = Color(red: 0x1B / 255, green: 0x1B / 255, blue: 0x1F / 255)
    static let listBorder = Color(red: 0x2C / 255, green: 0x2C / 255, blue: 0x31 / 255)
    static let listRule = Color(red: 0x2A / 255, green: 0x2A / 255, blue: 0x2E / 255)
    static let rowRule = Color(red: 0x1E / 255, green: 0x1E / 255, blue: 0x22 / 255)

    /// The tag's border: the state's colour, the quiet one darker than its
    /// glyph; `dimmed` (you are hidden) at RoundtableColours.dimmedOpacity.
    static func border(_ glyph: RoundtableFlagLine.Glyph, dimmed: Bool = false) -> Color {
        let colour = glyph == .quiet ? staleBorder : RoundtableColours.colour(glyph)
        return dimmed ? colour.opacity(RoundtableColours.dimmedOpacity) : colour
    }
}
