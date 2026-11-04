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
// 2026-11-04: A pending card follows the nearest keyed station within 5 kHz; tags stay quiet.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

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
// 2026-11-03: Overlay redesign card, placement and tags (design D10, D11, D13 to D15,
// D17 to D20), per-item widths as the desktop's layoutRoundtableTags.
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
    /// Who is on the air here; a double lists each current participant.
    var keyed: [String] = []
    /// The directory lists its stations, so `members` counts people here.
    var stationed = false
    /// The source room of its stations.
    var source: String? = nil
    /// Other exact frequencies a merged double covers.
    var mergedHz: [Int64] = []
    /// How wide it draws, to its own text; 0 takes the layout's width.
    var width: Double = 0
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

/// Which of a card's pieces are drawn at one width and one height limit
/// (overlay design D10, D17 to D19). All of it is fixed line heights; the only
/// text it measures is two keyed names, through the cached widths, so a reading,
/// an age or a clock changing never comes here again.
struct RoundtableCardPlan: Equatable {
    var width: CGFloat
    /// Below 150 wide: no clock, no format at the caption's right, S units for Both.
    var narrow: Bool
    /// Below 122 wide: no age column, and no frequency in the head.
    var tight: Bool
    /// Reading rows drawn.
    var rows: Int
    /// Readings not drawn.
    var moreRows: Int
    var showsMoreRows: Bool
    /// Keyed names drawn, in a double.
    var names: Int
    var namesInRow: Bool
    /// Between two names on one line: the board's 10, or 6 on a narrow card where 10 does not fit.
    var namesGap: CGFloat
    /// Keyed names not drawn.
    var moreKeyed: Int
    var showsMoreKeyed: Bool
    /// The card's note line ("Reconnecting", "Double ended 3s ago").
    var hasNote: Bool
    /// A last double's names.
    var hasPastNames: Bool
    var hasCaption: Bool
    /// "No readings yet", with no readings at all.
    var hasEmptyLine: Bool

    // MARK: The phone's and the iPad's sizes (overlay.css .rcp)

    static let border: CGFloat = 1
    static let rail: CGFloat = 3
    static let padVertical: CGFloat = 4
    static let padHorizontal: CGFloat = 6
    static let gap: CGFloat = 2
    static let headHeight: CGFloat = 16
    static let nameLine: CGFloat = 14
    static let noteLine: CGFloat = 11
    /// Its rule, 2 points of padding and a line.
    static let captionHeight: CGFloat = 14
    /// A 12 point line, 1 point and the meter.
    static let rowHeight: CGFloat = 16
    /// A line and 1 point of padding.
    static let footHeight: CGFloat = 12

    /// How tall the card draws. Every piece is one line of a fixed height.
    var height: CGFloat {
        var sum = Self.headHeight
        var pieces = 1
        func add(_ piece: CGFloat) {
            sum += piece
            pieces += 1
        }
        if names > 0 || (moreKeyed > 0 && showsMoreKeyed) {
            var block = Self.gap
            if names > 0 {
                block += namesInRow ? Self.nameLine
                    : CGFloat(names) * Self.nameLine + CGFloat(names - 1) * Self.gap
            }
            if moreKeyed > 0 && showsMoreKeyed {
                block += Self.noteLine + (names > 0 ? Self.gap : 0)
            }
            add(block)
        }
        if hasPastNames { add(Self.noteLine) }
        if hasNote { add(Self.noteLine) }
        if hasCaption { add(Self.captionHeight) }
        if rows > 0 { add(CGFloat(rows) * Self.rowHeight + CGFloat(rows - 1) * Self.gap) }
        if hasEmptyLine { add(Self.noteLine) }
        if moreRows > 0 && showsMoreRows { add(Self.footHeight) }
        return 2 * Self.border + Self.rail + 2 * Self.padVertical + sum + CGFloat(pieces - 1) * Self.gap
    }
}

/// Which of the two places a card took (D17, D18).
enum RoundtableCardSpot: Equatable {
    case under, beside
}

/// A card on the pan.
struct RoundtablePlacedCard: Equatable {
    var rect: CGRect
    var plan: RoundtableCardPlan
    var placement: RoundtableCardSpot
}

/// What a compact tag says (D20).
struct RoundtableTagFace: Equatable {
    var head: String
    /// "+N" the rest beside the head; a double with names has none.
    var count: String?
    /// A double's names, under the head.
    var names: [String]
}

/// The card, the tags and the folds for one pan, as the layer draws them.
struct RoundtableTagArrangement: Equatable {
    var card: RoundtablePlacedCard?
    var placements: [RoundtableTagPlacement]
    /// The tags left to draw, by room. Each carries its own width.
    var shown: [RoundtableTag]
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
        /// Says this instead of the call: "Your pan".
        var label: String? = nil
    }

    /// One station around this frequency, for a frequency view's list (D22).
    struct Person: Equatable {
        var name: String
        var keyed: Bool
        var you: Bool
        /// `member.hz - viewed hz`.
        var offsetHz: Int64
    }

    var kind: Kind = .quiet
    var hz: Int64
    /// On the air, or every name for a last double.
    var keyed: [String] = []
    /// Which of `keyed` is this station, so a double can say You.
    var keyedYou: String?
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
    var family = ""
    /// Each station in this frequency's neighborhood: the stream's members, or
    /// until it arrives the directory's (D22).
    var onFrequency: [Person] = []
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
    /// The tag's foot to the band-plan strip's top (overlay.js buildPhone's foot).
    static let footGap: CGFloat = 6
    /// Between tags, and from either edge.
    static let gap: Double = 4
    static let minWidth: Double = 56
    static let maxWidth: Double = 128
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
    /// D20: a double's tag names up to two, each on a line under its head, past the glyph.
    static let doubleTagNames = 2
    static let tagNameLine: CGFloat = 16
    static let tagNameIndent: CGFloat = 14

    // MARK: Layout

    /// Lays `tags` out across a pan `width` points wide whose left edge is
    /// `leftHz`, at `hzPerPoint`. Each tag is its own `width` wide, or `tagWidth`
    /// when it has none, and a fold is `tagWidth` wide, with at least `gap`
    /// between neighbors and from either edge, and clear of `keepOut`. Tags off
    /// the pan, or with no clear spot, are left out. The placements are in order
    /// of frequency.
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
            visible.append(Visible(x: x, hz: tag.hz, roomId: tag.roomId, pinned: tag.mine || tag.mentionsMe,
                                   width: tag.width > 0 ? tag.width : tagWidth))
        }
        visible = stableSorted(visible) { a, b in a.hz != b.hz ? a.hz < b.hz : a.roomId < b.roomId }

        let stretches = stretchesOf(width: width, gap: gap, keepOut: keepOut)
        if stretches.isEmpty {
            return []
        }
        // A tag wider than every stretch has no clear spot, and is left off.
        let roomiest = stretches.map { $0.right - $0.left }.max() ?? 0
        visible.removeAll { $0.width > roomiest }

        // First only the groups that cannot move aside fold. If the tags
        // still don't fit across the pan, every overlapping group folds,
        // and then the groups reach further each round until they fit or
        // span the pan.
        var reach = (visible.map(\.width).max() ?? tagWidth) + gap
        reach = max(reach, tagWidth + gap)
        var forceFold = false
        var items: [Item]
        while true {
            items = groupTags(visible, foldWidth: tagWidth, gap: gap, reach: reach, forceFold: forceFold)
            if itemsThatFit(items, stretches, gap: gap) == items.count || reach > width {
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
        while itemsThatFit(items, stretches, gap: gap) < items.count,
              let remove = items.lastIndex(where: { Set($0.roomIds).isDisjoint(with: pinnedIds) }) {
            items.remove(at: remove)
        }

        // Each tag to the stretch nearest its frequency that still has
        // room, keeping their order: a stretch already passed is not gone
        // back to. Tags that find no room are left out.
        var byStretch = Array(repeating: [Int](), count: stretches.count)
        var used = Array(repeating: 0.0, count: stretches.count)
        var current = 0
        for k in items.indices {
            guard let nearest = nearestStretch(stretches, x: items[k].x, width: items[k].width) else {
                continue
            }
            var target = max(nearest, current)
            while target < stretches.count
                    && used[target] + items[k].width > stretches[target].right - stretches[target].left {
                target += 1
            }
            if target >= stretches.count {
                continue
            }
            byStretch[target].append(k)
            used[target] += items[k].width + gap
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
                let left = max(items[k].x - items[k].width / 2, edge)
                lefts.append(left)
                edge = left + items[k].width + gap
            }
            edge = stretches[s].right
            for m in stride(from: members.count - 1, through: 0, by: -1) {
                lefts[m] = min(lefts[m], edge - items[members[m]].width)
                edge = lefts[m] - gap
            }
            for m in members.indices {
                let item = items[members[m]]
                out.append(RoundtableTagPlacement(left: lefts[m], width: item.width, roomIds: item.roomIds,
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
        var width: Double
    }

    /// A tag or a fold, centered on x.
    private struct Item {
        var x: Double
        var roomIds: [String]
        var folded: Bool
        var width: Double
    }

    /// A stretch of the pan tags may use.
    private struct Stretch {
        var left: Double
        var right: Double
    }

    /// Whether a group's tags can sit side by side, each no further than the
    /// group's widest tag from its own frequency. Side by side, tag k's center
    /// sits at start + c_k, c_k being the widths and gaps before it; the best
    /// start puts the largest move as small as it can be, half the spread of
    /// x_k - c_k.
    private static func slidesApart(_ visible: [Visible], _ group: [Int], gap: Double) -> Bool {
        var lowest = 0.0
        var highest = 0.0
        var center = 0.0
        var widest = 0.0
        for (k, index) in group.enumerated() {
            if k > 0 {
                center += (visible[group[k - 1]].width + visible[index].width) / 2 + gap
            }
            let offset = visible[index].x - center
            lowest = k == 0 ? offset : min(lowest, offset)
            highest = k == 0 ? offset : max(highest, offset)
            widest = max(widest, visible[index].width)
        }
        return (highest - lowest) / 2 <= widest
    }

    /// Tags that would overlap their neighbor share a group, which reaches
    /// `reach` points at most. A group whose unpinned tags number two or
    /// more folds them when they cannot move aside, or always when
    /// `forceFold`.
    private static func groupTags(_ visible: [Visible], foldWidth: Double, gap: Double, reach: Double,
                                  forceFold: Bool) -> [Item] {
        var groups: [[Int]] = []
        for i in visible.indices {
            if let group = groups.last, let last = group.last, let first = group.first {
                let fromLast = visible[i].x - visible[last].x
                let fromFirst = visible[i].x - visible[first].x
                let step = (visible[last].width + visible[i].width) / 2 + gap
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
            let fold = unpinned.count >= 2 && (forceFold || !slidesApart(visible, group, gap: gap))
            for index in group where !fold || visible[index].pinned {
                items.append(Item(x: visible[index].x, roomIds: [visible[index].roomId], folded: false,
                                  width: visible[index].width))
            }
            if fold {
                let sum = unpinned.reduce(0.0) { $0 + visible[$1].x }
                items.append(Item(x: sum / Double(unpinned.count), roomIds: unpinned.map { visible[$0].roomId },
                                  folded: true, width: foldWidth))
            }
        }
        return stableSorted(items) { $0.x < $1.x }
    }

    /// The pan less the keep-out stretches, each side `gap` from an edge or a keep-out.
    private static func stretchesOf(width: Double, gap: Double, keepOut: [RoundtableKeepOut]) -> [Stretch] {
        let blocks = stableSorted(keepOut) { $0.left < $1.left }
        var out: [Stretch] = []
        var left = gap
        func close(_ right: Double) {
            if right > left {
                out.append(Stretch(left: left, right: right))
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

    /// How many of `items`, in order, the stretches hold side by side. An item
    /// that finds no room is passed over, and the next may still fit.
    private static func itemsThatFit(_ items: [Item], _ stretches: [Stretch], gap: Double) -> Int {
        var placed = 0
        var current = 0
        var used = 0.0
        for item in items {
            var s = current
            var taken = used
            while s < stretches.count && taken + item.width > stretches[s].right - stretches[s].left {
                s += 1
                taken = 0
            }
            if s < stretches.count {
                current = s
                used = taken + item.width + gap
                placed += 1
            }
        }
        return placed
    }

    /// The stretch nearest x that is wide enough for `width`; nil when none is.
    private static func nearestStretch(_ stretches: [Stretch], x: Double, width: Double) -> Int? {
        var best: Int?
        var bestDistance = -1.0
        for (k, stretch) in stretches.enumerated() where stretch.right - stretch.left >= width {
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
                                          now: Int64(Date().timeIntervalSince1970 * 1000), format: model.signalFormat,
                                          rows: rows)
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
                                     mentionsMe: (mentionCounts[summary.id] ?? 0) > 0,
                                     keyed: summary.keyedNames.isEmpty ? summary.talkers : summary.keyedNames,
                                     stationed: !summary.stations.isEmpty,
                                     source: summary.stations.first?.sourceRoom))
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

    /// What the stream will bring for a frequency it has not reached yet (D3): the
    /// nearest keyed station within the window in this family, in the words of its
    /// own exact row. That row's state is the source-room rule (D1), so keyed
    /// neighbors in different rooms never make a Double here. The card and flag read
    /// this; a tag reads only its own row.
    static func nearbyOnAir(hz: Int64, family: String,
                            rows: [RoundtableModel.RoomSummary]) -> (state: String, names: [String], members: Int64)? {
        let near = rows.filter { $0.family == family && abs($0.hz - hz) <= nearbyHz }
        var best: RoundtableModel.RoomSummary?
        for row in near where row.stations.contains(where: \.keyed) {
            if let current = best, abs(current.hz - hz) <= abs(row.hz - hz) { continue }
            best = row
        }
        guard let best, let talker = best.stations.first(where: \.keyed) else { return nil }
        let members = near.reduce(Int64(0)) { $0 + $1.members }
        if best.state == "doubled" { return ("doubled", best.keyedNames, members) }
        return ("talking", [RoundtableText.withNth(talker.callsign, talker.nth)], members)
    }

    /// A frequency's state and names from the directory alone: its own row, or with
    /// nothing on it, what is on the air within the window (D3).
    private static func directoryTruth(summary: RoundtableModel.RoomSummary,
                                       rows: [RoundtableModel.RoomSummary]) -> (state: String, names: [String]) {
        let settled = summary.state == "doubled" || summary.state == "unknown"
            || (summary.state == "talking" && !summary.keyedNames.isEmpty)
        if !settled, let nearby = nearbyOnAir(hz: summary.hz, family: summary.family, rows: rows) {
            return (nearby.state, nearby.names)
        }
        return (summary.state, summary.keyedNames)
    }

    /// The tag's card, by the truth rules above. A frequency whose stream has
    /// not arrived says only what the directory knows (D1).
    static func card(for summary: RoundtableModel.RoomSummary, view: RoundtableModel.Room?, now: Int64,
                     format: RoundtableSignalFormat = .dbm, rows: [RoundtableModel.RoomSummary] = []) -> RoundtableTagCard? {
        let known = view?.family == summary.family ? view : nil
        guard let known, let chosen = chosenTurn(hz: summary.hz, family: summary.family, view: known) else {
            var card = RoundtableTagCard(format: format)
            if known == nil || (known?.members.isEmpty == true && known?.now.isEmpty == true) {
                let truth = directoryTruth(summary: summary, rows: rows)
                if truth.state == "doubled" {
                    card.state = .doubled
                    card.participants = truth.names
                } else if truth.state == "talking", truth.names.count == 1 {
                    card.state = .known(truth.names[0])
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

    /// A view's list: each station against the viewed frequency (D22). Until the
    /// stream arrives the directory says who is nearby, as the server's members do.
    static func onFrequency(hz: Int64, family: String, members: [RoundtableModel.Member],
                            directory: [RoundtableModel.Station]) -> [RoundtableCardData.Person] {
        var seen = Set<String>()
        if !members.isEmpty {
            return members.compactMap { member in
                let at = member.hz ?? hz
                guard abs(at - hz) <= nearbyHz, seen.insert(member.id).inserted else { return nil }
                return .init(name: RoundtableText.withNth(member.callsign, member.nth), keyed: member.keyed,
                             you: member.you, offsetHz: at - hz)
            }
        }
        return directory.compactMap { station in
            guard station.family == family, abs(station.hz - hz) <= nearbyHz, seen.insert(station.id).inserted else {
                return nil
            }
            return .init(name: RoundtableText.withNth(station.callsign, station.nth), keyed: station.keyed,
                         you: false, offsetHz: station.hz - hz)
        }
    }

    /// What the card for one frequency says (the desktop's roundtableCardModelFor).
    /// `peaks` holds each listener's peak; nil leaves the ticks off.
    static func cardData(summary: RoundtableModel.RoomSummary, view: RoundtableModel.Room?, now: Int64,
                         link: RoundtableModel.LinkState, linkSince: Int64,
                         peaks: RoundtablePeakTracker? = nil,
                         directory: [RoundtableModel.Station] = [],
                         rows: [RoundtableModel.RoomSummary] = []) -> RoundtableCardData {
        var card = RoundtableCardData(hz: summary.hz)
        card.family = summary.family
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
        card.onFrequency = onFrequency(hz: summary.hz, family: summary.family, members: known?.members ?? [],
                                       directory: directory.isEmpty ? summary.stations : directory)

        guard let known, let chosen = chosenTurn(hz: summary.hz, family: summary.family, view: known) else {
            // Known only from the directory (D1): who is keyed, with no readings.
            if known == nil || (known?.members.isEmpty == true && known?.now.isEmpty == true) {
                let truth = directoryTruth(summary: summary, rows: rows)
                switch truth.state {
                case "doubled":
                    card.kind = .double
                    card.keyed = truth.names
                case "talking" where !truth.names.isEmpty:
                    card.kind = .talking
                    card.keyed = truth.names
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
        card.keyedYou = people.first { talker in
            known.members.contains { member in
                member.you && member.ref == talker.ref && member.sourceRoom == (talker.sourceRoom ?? turn.sourceRoom)
            }
        }.map { RoundtableText.withNth($0.callsign, $0.nth) }
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

    // MARK: The card's words (overlay design D10, overlay.js cardHtml)

    /// D17: below this wide the row drops its age and the head its frequency.
    static let cardTightBelow: CGFloat = 122
    /// D17: below this wide the signal shows S units when the format is Both.
    static let cardNarrowBelow: CGFloat = 150
    /// As the desktop: at most this many reading rows, then "+N more".
    static let cardRowCap = 5
    /// The card's top limit, under the pan's top edge.
    static let cardTopGap: CGFloat = 2
    /// Between two names on one line (overlay.css .rc__names--row).
    static let cardNamesGap: CGFloat = 10
    /// The same on a narrow card that 10 would not fit: names still lead with their dots.
    static let cardNarrowNamesGap: CGFloat = 6
    /// The pan's own left edge margin.
    static let cardLeftGap: CGFloat = 3
    /// The pan's right edge margin, and between the card and what it keeps clear of.
    static let cardEdgeGap: CGFloat = 4
    /// D17 and D18: the card's width under the flag, and the least it takes beside it.
    static let compactCardWidth: CGFloat = 176
    static let compactCardMinWidth: CGFloat = 104
    static let regularCardWidth: CGFloat = 232
    static let regularCardMinWidth: CGFloat = 200
    /// D18: an iPad's card starts at the dial, to stay off a lower-sideband signal.
    static let regularCardDialGap: CGFloat = 4

    static func cardHead(_ card: RoundtableCardData, narrow: Bool) -> String {
        switch card.kind {
        case .double:
            return card.keyed.count > 2 ? "Double \u{00B7} \(card.keyed.count)" : "Double"
        case .unknown:
            return "Unknown"
        case .you:
            return narrow ? "You" : "You \u{00B7} on the air"
        case .lastDouble:
            return "Last double"
        case .talking, .lastTalker, .quiet:
            return card.keyed.first ?? "Unknown"
        }
    }

    /// The name a keyed station shows as: "You" for this station.
    static func cardName(_ name: String, in card: RoundtableCardData) -> String {
        name == card.keyedYou ? "You" : name
    }

    static func cardCaption(_ card: RoundtableCardData, narrow: Bool) -> String {
        if card.frozen {
            return card.kind == .double && !narrow ? "Last known \u{00B7} combined" : "Last known"
        }
        switch card.kind {
        case .double, .lastDouble:
            return "Combined RF"
        case .you:
            return "Hearing you"
        case .lastTalker:
            return "Last heard \(cardAge(card.endedAgo)) ago"
        case .talking, .unknown, .quiet:
            return "Heard by"
        }
    }

    /// The word at the caption's right, for the rows' signal column.
    static func cardFormatTitle(_ format: RoundtableSignalFormat) -> String {
        switch format {
        case .dbm: "dBm"
        case .sUnits: "S"
        case .both: "S / dBm"
        }
    }

    /// "4s", "3m", "1h".
    static func cardAge(_ ms: Int64) -> String {
        let seconds = max(0, ms) / 1000
        if seconds < 60 { return "\(seconds)s" }
        if seconds < 3600 { return "\(seconds / 60)m" }
        return "\(seconds / 3600)h"
    }

    /// "1:18".
    static func cardClock(_ ms: Int64) -> String {
        let seconds = max(0, ms) / 1000
        return "\(seconds / 60):" + String(format: "%02d", seconds % 60)
    }

    /// The card's clock. No turn's start known yet, as a view whose stream has not
    /// arrived: no clock, as the desktop, rather than "0:00".
    static func cardClockText(_ card: RoundtableCardData) -> String? {
        card.clock > 0 ? cardClock(card.clock) : nil
    }

    /// The head's frequency: MHz, three decimals when the frequency allows.
    static func cardFrequency(_ hz: Int64) -> String {
        let decimals = hz % 1000 == 0 ? 3 : hz % 100 == 0 ? 4 : 5
        return String(format: "%.\(decimals)f", Double(hz) / 1.0e6)
    }

    /// A row's signal on one line. Below 150 wide, Both shows S units (D17).
    static func cardSignal(_ dbm: Double, format: RoundtableSignalFormat, narrow: Bool) -> String {
        let number = String(format: "%.0f", dbm).replacingOccurrences(of: "-", with: "\u{2212}")
        switch format {
        case .dbm: return number
        case .sUnits: return RoundtableText.sText(dbm)
        case .both: return narrow ? RoundtableText.sText(dbm) : RoundtableText.sText(dbm) + " " + number
        }
    }

    /// A row's meter: its state's colour, your own in cyan, a double striped, an
    /// old reading or a dropped link grey (D7, D11, D13).
    static func meterStyle(kind: RoundtableCardData.Kind, row: RoundtableCardData.Row,
                           frozen: Bool) -> RoundtableMeterBar.Style {
        if frozen || row.old { return .stale }
        switch kind {
        case .double: return row.you ? .youInDouble : .double
        case .you: return .you
        case .talking: return row.you ? .you : .talking
        case .unknown: return row.you ? .you : .unknown
        case .lastTalker, .lastDouble, .quiet: return row.you ? .you : .ended
        }
    }

    // MARK: The card's plan

    /// The inputs a plan depends on. A reading, an age or a clock is none of them.
    private struct PlanKey: Hashable {
        var width: CGFloat
        var room: CGFloat
        var rowCount: Int
        var format: RoundtableSignalFormat
        var kind: RoundtableCardData.Kind
        var keyed: [String]
        var keyedYou: String?
        var hasNote: Bool
        var frozen: Bool
    }

    @MainActor
    private static var planCache: [PlanKey: RoundtableCardPlan?] = [:]
    private static let planCacheMax = 256
    /// How many plans were worked out, for the tests.
    @MainActor
    private(set) static var planComputations = 0

    @MainActor
    private static var textWidths: [String: CGFloat] = [:]
    private static let textWidthsMax = 512
    /// How many strings were measured, for the tests.
    @MainActor
    private(set) static var textMeasurements = 0

    /// A string's width in the system font, measured once.
    @MainActor
    private static func textWidth(_ text: String, size: CGFloat, weight: UIFont.Weight) -> CGFloat {
        let key = "\(size)/\(weight.rawValue)/\(text)"
        if let known = textWidths[key] { return known }
        if textWidths.count >= textWidthsMax { textWidths.removeAll() }
        textMeasurements += 1
        let width = (text as NSString).size(withAttributes: [.font: UIFont.systemFont(ofSize: size, weight: weight)]).width
        textWidths[key] = width
        return width
    }

    /// Every piece of the card drawn, with `rows` reading rows.
    @MainActor
    private static func fullPlan(_ card: RoundtableCardData, width: CGFloat, rows: Int) -> RoundtableCardPlan {
        let narrow = width < cardNarrowBelow
        let names = card.kind == .double ? card.keyed.count : 0
        var inRow = false
        var namesGap = cardNamesGap
        if names == 2 {
            // Two names share a line whenever they fit, at any width: on a short pan
            // the line they save is a reading row. A dot and its gap before each
            // name, and the gap between them. The real short pan's card is 2 points
            // short of two five letter calls at the board's gap, so a narrow card
            // closes that gap before it stacks them.
            let room = width - 2 * RoundtableCardPlan.border - 2 * RoundtableCardPlan.padHorizontal
            let wanted = card.keyed.reduce(CGFloat(0)) {
                $0 + 12 + textWidth(cardName($1, in: card), size: 11, weight: .bold)
            }
            if narrow && wanted + cardNamesGap > room { namesGap = cardNarrowNamesGap }
            inRow = wanted + namesGap <= room
        }
        return RoundtableCardPlan(width: width, narrow: narrow, tight: width < cardTightBelow, rows: rows,
                                  moreRows: card.rows.count - rows, showsMoreRows: true, names: names,
                                  namesInRow: inRow, namesGap: namesGap, moreKeyed: 0, showsMoreKeyed: true,
                                  hasNote: card.note != nil,
                                  hasPastNames: card.kind == .lastDouble && !card.keyed.isEmpty,
                                  hasCaption: true, hasEmptyLine: card.rows.isEmpty)
    }

    /// The most reading rows that fit with every other piece; -1 when even none do.
    /// A double always keeps one reading row if it can: see `oneRowDouble`.
    @MainActor
    static func rowsThatFit(_ card: RoundtableCardData, width: CGFloat, room: CGFloat) -> Int {
        var rows = min(card.rows.count, cardRowCap)
        while rows >= 1 {
            if fullPlan(card, width: width, rows: rows).height <= room { return rows }
            rows -= 1
        }
        if oneRowDouble(card, width: width, room: room) != nil { return 1 }
        return fullPlan(card, width: width, rows: 0).height <= room ? 0 : -1
    }

    /// Stacked names can leave a double no row. The caption goes before the last
    /// row: the Double head and the striped meter still say what it is. Live or
    /// frozen, a double keeps one reading; "+N more" goes first on a frozen card,
    /// which holds "Last known" longest (D7), and after the caption on a live one.
    @MainActor
    private static func oneRowDouble(_ card: RoundtableCardData, width: CGFloat, room: CGFloat) -> RoundtableCardPlan? {
        guard card.kind == .double, !card.rows.isEmpty else { return nil }
        var plan = fullPlan(card, width: width, rows: 1)
        if plan.height <= room { return plan }
        if card.frozen { plan.showsMoreRows = false } else { plan.hasCaption = false }
        if plan.height <= room { return plan }
        if card.frozen { plan.hasCaption = false } else { plan.showsMoreRows = false }
        return plan.height <= room ? plan : nil
    }

    /// The card at `width` in `room` points of height, or nil when not even its
    /// head fits. On a short pan the rows give way first, with "+N more", then the
    /// lesser lines. The Double head and the keyed names go last (D19). Cached on
    /// its inputs: nothing here runs again for a reading, an age or the clock.
    @MainActor
    static func cardPlan(_ card: RoundtableCardData, width: CGFloat, room: CGFloat,
                         format: RoundtableSignalFormat = .dbm) -> RoundtableCardPlan? {
        let key = PlanKey(width: width, room: room, rowCount: card.rows.count, format: format, kind: card.kind,
                          keyed: card.keyed, keyedYou: card.keyedYou, hasNote: card.note != nil, frozen: card.frozen)
        if let known = planCache[key] { return known }
        if planCache.count >= planCacheMax { planCache.removeAll() }
        planComputations += 1
        let plan = computePlan(card, width: width, room: room)
        planCache[key] = plan
        return plan
    }

    @MainActor
    private static func computePlan(_ card: RoundtableCardData, width: CGFloat, room: CGFloat) -> RoundtableCardPlan? {
        let rows = rowsThatFit(card, width: width, room: room)
        if rows == 1, let oneRow = oneRowDouble(card, width: width, room: room) { return oneRow }
        if rows >= 0 { return fullPlan(card, width: width, rows: rows) }
        var plan = fullPlan(card, width: width, rows: 0)
        // A dropped link keeps its "Last known" caption longest (D7).
        let frozen = card.frozen
        let losses: [(inout RoundtableCardPlan) -> Void] = [
            { $0.hasEmptyLine = false },
            { if frozen { $0.showsMoreRows = false } else { $0.hasCaption = false } },
            { if frozen { $0.hasCaption = false } else { $0.showsMoreRows = false } },
            { $0.hasNote = false },
            { $0.hasPastNames = false },
        ]
        for lose in losses {
            lose(&plan)
            if plan.height <= room { return plan }
        }
        while plan.names > 0 {
            plan.names -= 1
            plan.moreKeyed += 1
            plan.namesInRow = false
            if plan.height <= room { return plan }
        }
        plan.showsMoreKeyed = false
        return plan.height <= room ? plan : nil
    }

    // MARK: The card's place (D17, D18, overlay.js placeCard)

    /// What of the pan is free from `top` to `bottom`, left to right.
    private static func freeStretches(_ blocks: [CGRect], left: CGFloat, right: CGFloat, top: CGFloat,
                                      bottom: CGFloat) -> [ClosedRange<CGFloat>] {
        var out: [ClosedRange<CGFloat>] = []
        var edge = left
        for block in blocks.filter({ $0.maxY > top && $0.minY < bottom }).sorted(by: { $0.minX < $1.minX }) {
            let end = min(block.minX - cardEdgeGap, right)
            if end > edge { out.append(edge...end) }
            edge = max(edge, block.maxX + cardEdgeGap)
        }
        if right > edge { out.append(edge...right) }
        return out
    }

    private static func clamp(_ value: CGFloat, _ low: CGFloat, _ high: CGFloat) -> CGFloat {
        max(low, min(value, high))
    }

    /// Where the card goes: under every flag at full width when that shows as many
    /// rows, else in the free strip that shows the most. `flags` are the flags and
    /// their round buttons, each its own rect; `keepOut` is the rest of what it stays
    /// off (the band's button column). Nil when it fits nowhere.
    @MainActor
    static func placedCard(flags: [CGRect], keepOut: [CGRect] = [], dialX: CGFloat, foot: CGFloat, panWidth: CGFloat,
                           compact: Bool, card: RoundtableCardData,
                           format: RoundtableSignalFormat = .dbm) -> RoundtablePlacedCard? {
        let full = compact ? compactCardWidth : regularCardWidth
        let least = compact ? compactCardMinWidth : regularCardMinWidth
        let left = cardLeftGap
        let right = panWidth - cardEdgeGap
        let top = cardTopGap
        guard right - left > 0, foot > top else { return nil }

        let flagFoot = flags.map(\.maxY).max() ?? top
        let underRoom = foot - max(flagFoot + cardEdgeGap, top)
        let underRows = rowsThatFit(card, width: full, room: underRoom)

        // The strip beside the flags: under each thing it could start below (a
        // flag's round buttons end above the flag does), the place that shows the
        // most rows, then the nearest the dial, then the wider.
        struct Strip {
            var stretch: ClosedRange<CGFloat>
            var width: CGFloat
            var room: CGFloat
            var rows: Int
            var distance: CGFloat
        }
        let blocks = flags + keepOut
        let tops = Set([top] + blocks.map { $0.maxY + cardEdgeGap }.filter { $0 > top && $0 < foot }).sorted()
        var strip: Strip?
        for from in tops {
            for stretch in freeStretches(blocks, left: left, right: right, top: from, bottom: foot)
            where stretch.upperBound - stretch.lowerBound >= least {
                let wide = stretch.upperBound - stretch.lowerBound
                let distance = dialX < stretch.lowerBound ? stretch.lowerBound - dialX
                    : dialX > stretch.upperBound ? dialX - stretch.upperBound : 0
                let width = min(full, wide)
                let next = Strip(stretch: stretch, width: width, room: foot - from,
                                 rows: rowsThatFit(card, width: width, room: foot - from), distance: distance)
                guard let best = strip else {
                    strip = next
                    continue
                }
                let better: Bool
                if next.rows != best.rows {
                    better = next.rows > best.rows
                } else if next.rows < 0 && next.room != best.room {
                    better = next.room > best.room
                } else if next.distance != best.distance {
                    better = next.distance < best.distance
                } else {
                    better = next.width > best.width
                }
                if better { strip = next }
            }
        }
        let stripRows = strip?.rows ?? -1

        func under(_ plan: RoundtableCardPlan) -> RoundtablePlacedCard? {
            let height = plan.height
            let wanted = compact ? dialX - full / 2 : dialX + regularCardDialGap
            // Under the flags only the keep-outs can stand in its way.
            var best: CGFloat?
            for stretch in freeStretches(keepOut, left: left, right: right, top: foot - height, bottom: foot)
            where stretch.upperBound - stretch.lowerBound >= full {
                let x = clamp(wanted, stretch.lowerBound, stretch.upperBound - full)
                if best.map({ abs(x - wanted) < abs($0 - wanted) }) ?? true { best = x }
            }
            return best.map {
                RoundtablePlacedCard(rect: CGRect(x: $0, y: foot - height, width: full, height: height), plan: plan,
                                     placement: .under)
            }
        }
        func beside() -> RoundtablePlacedCard? {
            guard let strip, let plan = cardPlan(card, width: strip.width, room: strip.room, format: format) else {
                return nil
            }
            let height = plan.height
            let x = clamp(dialX - strip.width / 2, strip.stretch.lowerBound, strip.stretch.upperBound - strip.width)
            return RoundtablePlacedCard(rect: CGRect(x: x, y: foot - height, width: strip.width, height: height),
                                        plan: plan, placement: .beside)
        }

        if underRows >= 0, stripRows <= underRows,
           let plan = cardPlan(card, width: full, room: underRoom, format: format), let placed = under(plan) {
            return placed
        }
        if stripRows >= 0, let placed = beside() {
            return placed
        }
        // No place shows every lesser line: the one that keeps the most.
        let inFull = cardPlan(card, width: full, room: underRoom, format: format).flatMap(under)
        return [beside(), inFull].compactMap { $0 }.max { $0.rect.height < $1.rect.height }
    }

    /// The card's rect (`.null` when it fits nowhere), its rows, the readings it
    /// leaves out and which place it took.
    @MainActor
    static func placeCard(flags: [CGRect], dialX: CGFloat, foot: CGFloat, panWidth: CGFloat, compact: Bool,
                          card: RoundtableCardData, format: RoundtableSignalFormat = .dbm,
                          keepOut: [CGRect] = [])
        -> (rect: CGRect, rows: Int, moreRows: Int, placement: RoundtableCardSpot) {
        guard let placed = placedCard(flags: flags, keepOut: keepOut, dialX: dialX, foot: foot, panWidth: panWidth,
                                      compact: compact, card: card, format: format) else {
            return (.null, 0, card.rows.count, .under)
        }
        return (placed.rect, placed.plan.rows, placed.plan.moreRows, placed.placement)
    }

    // MARK: Tags (D20)

    /// What a tag says. A double says the word and up to two names; a talker or a
    /// quiet frequency says one name and "+N" the rest.
    static func face(_ tag: RoundtableTag) -> RoundtableTagFace {
        let count: String?
        if tag.stationed {
            count = tag.members > 1 ? "+\(tag.members - 1)" : nil
        } else {
            count = "\(tag.members)"
        }
        let first = tag.label.split(separator: "\n").first.map(String.init) ?? tag.label
        switch tag.glyph {
        case .doubled:
            let names = Array(tag.keyed.prefix(doubleTagNames))
            return RoundtableTagFace(head: "Double", count: names.isEmpty ? count : nil, names: names)
        case .you:
            return RoundtableTagFace(head: "You", count: count, names: [])
        case .unknown:
            return RoundtableTagFace(head: "Unknown", count: count, names: [])
        case .talking:
            return RoundtableTagFace(head: tag.keyed.first ?? first, count: count, names: [])
        case .quiet, .waiting, .offline:
            return RoundtableTagFace(head: first, count: count, names: [])
        }
    }

    /// One tag's height: a double's names stack under its head.
    static func tagHeight(_ tag: RoundtableTag) -> CGFloat {
        height + CGFloat(face(tag).names.count) * tagNameLine
    }

    /// D20: one double spanning several exact frequencies in one source room is
    /// one tag. Two doubles in one source room that share no name stay apart.
    static func mergeDoubleTags(_ tags: [RoundtableTag]) -> [RoundtableTag] {
        var out: [RoundtableTag] = []
        out.reserveCapacity(tags.count)
        for tag in tags {
            if tag.glyph == .doubled, let source = tag.source,
               let kept = out.firstIndex(where: { other in
                   other.glyph == .doubled && other.source == source
                       && other.keyed.contains(where: tag.keyed.contains)
               }) {
                for name in tag.keyed where !out[kept].keyed.contains(name) {
                    out[kept].keyed.append(name)
                }
                out[kept].label = (["Double"] + out[kept].keyed).joined(separator: "\n")
                out[kept].members += tag.members
                out[kept].mine = out[kept].mine || tag.mine
                out[kept].mentionsMe = out[kept].mentionsMe || tag.mentionsMe
                out[kept].mergedHz.append(tag.hz)
                continue
            }
            out.append(tag)
        }
        return out
    }

    /// The card and the tags for one pan. `foot` is where they stand, `panWidth`
    /// the pan's usable width. A tag with no clear spot is left off, and a
    /// frequency another slice's flag speaks for (`spokenHz`) has none. Only
    /// strings new since the last call are measured.
    @MainActor
    static func arrange(tags: [RoundtableTag], card: RoundtableCardData?, cardRoom: String?,
                        format: RoundtableSignalFormat, spokenHz: Set<Int64>, foot: CGFloat, panWidth: CGFloat,
                        hzPerPoint: Double, lowHz: Double, flags: [CGRect], keepOut: [CGRect],
                        compact: Bool) -> RoundtableTagArrangement {
        var placed: RoundtablePlacedCard?
        if let card, card.kind != .quiet, hzPerPoint > 0 {
            let dialX = CGFloat((Double(card.hz) - lowHz) / hzPerPoint)
            if dialX >= 0, dialX <= panWidth {
                placed = placedCard(flags: flags, keepOut: keepOut, dialX: dialX, foot: foot, panWidth: panWidth,
                                    compact: compact, card: card, format: format)
            }
        }

        var shown: [RoundtableTag] = []
        for var tag in mergeDoubleTags(tags) {
            if placed != nil, let card,
               tag.roomId == cardRoom || tag.hz == card.hz || tag.mergedHz.contains(card.hz) {
                continue
            }
            if spokenHz.contains(tag.hz) || tag.mergedHz.contains(where: spokenHz.contains) {
                continue
            }
            tag.width = itemWidth(tag)
            shown.append(tag)
        }

        // The row is as tall as the tallest tag standing on the foot.
        let rowHeight = shown.map { tagHeight($0) }.max() ?? height
        var clear = RoundtableTagLayer.keepOut(flags + keepOut, top: foot - rowHeight, width: panWidth,
                                               height: rowHeight)
        if let rect = placed?.rect {
            clear.append(RoundtableKeepOut(left: Double(rect.minX), right: Double(rect.maxX)))
        }
        let visible = shown.filter { foot - tagHeight($0) >= 0 }
        let placements = layout(visible, hzPerPoint: hzPerPoint, leftHz: Int64(lowHz.rounded()),
                                width: Double(panWidth), tagWidth: foldWidth(), gap: gap, keepOut: clear)
        return RoundtableTagArrangement(card: placed, placements: placements, shown: visible)
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
    static func foldHeading(count: Int, sliceLetter: String, viewing: Bool = false) -> String {
        viewing ? "\(count) frequencies here \u{00B7} a tap views one"
            : "\(count) frequencies here \u{00B7} a tap tunes slice \(sliceLetter)"
    }

    /// A tag's own width, to its own text and at most `maxWidth` (the board sizes
    /// each tag to its text, overlay.css .rct). Measured once for each string.
    @MainActor
    static func itemWidth(_ tag: RoundtableTag) -> Double {
        let face = face(tag)
        // 2 points over: advances round, and a name must not cut at its own width.
        let fixed = 2 + padLeft + glyphSize + innerGap + padRight + 2
        var width = fixed + textWidth(face.head, size: labelFontSize, weight: .bold)
        if let count = face.count {
            width += textWidth(" ", size: labelFontSize, weight: .bold)
                + textWidth(count, size: labelFontSize, weight: .semibold)
        }
        for name in face.names {
            width = max(width, 2 + padLeft + tagNameIndent + textWidth(name, size: labelFontSize, weight: .bold)
                + padRight + 2)
        }
        return min(Double(width.rounded(.up)), maxWidth)
    }

    /// A fold: its stacked glyphs and a two digit +N.
    @MainActor
    static func foldWidth() -> Double {
        let stack = 1 + 5 + glyphSize + CGFloat(foldStackMax - 1) * foldStackStep + 4
        let wanted = stack + textWidth("+99", size: labelFontSize, weight: .bold) + padRight + 1
        return max(minWidth, Double(wanted.rounded(.up)))
    }
}

// MARK: The view

/// The tags along the foot of the spectrum, above the band-plan strip and clear
/// of the flags, and the active frequency's card: a tap on a tag tunes the active
/// slice to its room; a tap on +N lists its rooms by frequency, and a row tunes.
/// On a server that can read another frequency a tap on a tag or a row views it
/// and tunes nothing (overlay design D21), and your own opens its action sheet.
/// Keyed, a tag tunes nothing. A dropped link greys them as last known; connecting
/// hides them; while you are hidden their marking is dimmed. They take touches only
/// inside their own rects, so the band still drags.
struct RoundtableTagLayer: View {
    @ObservedObject var roundtable: RoundtableModel
    let sliceIds: [Int64]
    /// The pan: its width and the frequency at its left edge.
    let geometryWidth: CGFloat
    let lowHz: Double
    let hzPerPoint: Double
    /// The tag row's top, for a one line tag.
    let top: CGFloat
    /// The band-plan strip's top, where each tag's tick ends.
    let tickFoot: CGFloat
    /// The flags' rects on the band, which the card and the tags keep clear of.
    let flagRects: [CGRect]
    let keyed: Bool
    let sliceLetter: String
    /// The rest they keep clear of: the band's side buttons, the dBm scale's arrows.
    var panKeepOut: [CGRect] = []
    /// The pan's width less the dBm scale; nil is all of `geometryWidth`.
    var usableWidth: CGFloat? = nil
    /// Where other slices' full flags stand, each saying what is on its own frequency.
    var otherFlagHz: Set<Int64> = []
    /// iPhone (D17) or iPad (D18); nil goes by the width class and the device.
    var compact: Bool? = nil
    /// Reads reports without tuning, using the main screen’s existing sheet.
    var readReports: ((String) -> Void)? = nil
    /// Views a frequency, on a server that can show one (overlay design D21). Nothing tunes.
    var viewFrequency: ((String) -> Void)? = nil
    /// The active slice's exact frequency, for the card menu's Tune.
    var activeSliceHz: Int64? = nil
    /// Tunes the active slice to an exact frequency: the card menu's Tune.
    var tuneHz: ((Int64) -> Void)? = nil
    /// Tunes the active slice to a room.
    let tune: (RoundtableModel.RoomSummary) -> Void

    @Environment(\.horizontalSizeClass) private var widthClass
    @State private var openFold: [String]?
    @State private var detailsTarget: MenuTarget?
    @State private var showDetails = false

    /// The frequency the card's action sheet is about.
    private struct MenuTarget: Equatable {
        var roomId: String
        var hz: Int64
    }

    /// What a tap on a tag or a fold's row does.
    enum TagTap: Equatable {
        /// D21: views the frequency and tunes nothing.
        case view(String)
        /// D25: your own frequency's action sheet.
        case ownMenu(String)
        /// A legacy server cannot view: the sheet of details today's tag offers.
        case legacyDetails(String)
        /// A legacy server cannot view: tunes, as it always has.
        case tune(String)
    }

    /// A tag's tap (D21, D25). `canView` is a server that can read another frequency.
    static func tagTap(roomId: String, canView: Bool, activeContext: String?, hasDetails: Bool) -> TagTap {
        if canView {
            return roomId == activeContext ? .ownMenu(roomId) : .view(roomId)
        }
        return hasDetails ? .legacyDetails(roomId) : .tune(roomId)
    }

    /// A row in a fold's list: it views, and never tunes, on a capable server.
    static func foldRowTap(roomId: String, canView: Bool) -> TagTap {
        canView ? .view(roomId) : .tune(roomId)
    }

    /// Where the card's Tune goes (controller's ruling): the live talker's exact
    /// frequency. Nil, and so disabled, while keyed, with no live turn, or when
    /// the slice is already exactly there.
    static func ownTuneTarget(_ card: RoundtableCardData, keyed: Bool, sliceHz: Int64?) -> Int64? {
        guard !keyed, let live = card.liveHz, live != sliceHz else {
            return nil
        }
        return live
    }

    private var canView: Bool { roundtable.frequencyAvailable && viewFrequency != nil }

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            tagContent
        }
    }

    private var isCompact: Bool {
        compact ?? (widthClass != .regular || UIDevice.current.userInterfaceIdiom == .phone)
    }

    /// Where the card and the tags stand.
    private var foot: CGFloat { top + RoundtableTagLayout.height }

    @ViewBuilder
    private var tagContent: some View {
        let display = RoundtableTagLayout.display(roundtable)
        let tags = RoundtableTagLayout.tags(roundtable, sliceIds: sliceIds)
        let activeContext = roundtable.frequencyAvailable ? roundtable.activeContext : nil
        let card = display == .hidden ? nil : activeContext.map { roundtable.cardData(context: $0.id) }
        if display != .hidden, top >= 0, !tags.isEmpty {
            let arrangement = RoundtableTagLayout.arrange(
                tags: tags, card: card, cardRoom: activeContext?.id, format: roundtable.signalFormat,
                spokenHz: roundtable.frequencyAvailable ? otherFlagHz : [], foot: foot, panWidth: usableWidth ?? geometryWidth, hzPerPoint: hzPerPoint,
                lowHz: lowHz, flags: flagRects, keepOut: panKeepOut, compact: isCompact)
            let placements = arrangement.placements
            let byId = Dictionary(arrangement.shown.map { ($0.roomId, $0) }, uniquingKeysWith: { first, _ in first })
            let stale = display == .stale
            let dimmed = display == .dimmed
            ZStack(alignment: .topLeading) {
                ticks(placements, byId: byId, stale: stale, dimmed: dimmed)
                if let card, let placed = arrangement.card {
                    cardTick(card, placed: placed, dimmed: dimmed)
                    cardButton(card, placed: placed, tag: tags.first { $0.roomId == activeContext?.id },
                               dimmed: dimmed)
                        .offset(x: placed.rect.minX, y: placed.rect.minY)
                }
                ForEach(placements, id: \.roomIds) { placement in
                    tagView(placement, byId: byId, stale: stale, dimmed: dimmed)
                        .offset(x: CGFloat(placement.left), y: foot - placedHeight(placement, byId: byId))
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
            .confirmationDialog("People at " + (detailsTarget.map { RoundtableTagLayout.frequencyText($0.hz) } ?? "this frequency"),
                                isPresented: $showDetails, titleVisibility: .visible) {
                if let target = detailsTarget {
                    Button("Read reports") { readReports?(target.roomId) }
                    Button("Signal: \(roundtable.signalFormat.title) · Change") { roundtable.cycleSignalFormat() }
                    if canView {
                        let live = Self.ownTuneTarget(roundtable.cardData(context: target.roomId), keyed: keyed,
                                                      sliceHz: activeSliceHz)
                        Button("Tune slice \(sliceLetter) here") { if let live { tuneHz?(live) } }.disabled(live == nil)
                    } else {
                        Button("Tune slice \(sliceLetter) here") { tuneTo(target.roomId) }.disabled(keyed)
                    }
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

    /// The flags that stand in the tag row, as spans the tags keep off. A
    /// flag that only meets the row's edge (a flag stacked just above it,
    /// whose rect ends a fraction of a point into the row) leaves it free.
    static func keepOut(_ flagRects: [CGRect], top: CGFloat, width: CGFloat,
                        height: CGFloat = RoundtableTagLayout.height) -> [RoundtableKeepOut] {
        let row = CGRect(x: 0, y: top, width: width, height: height)
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
        let bottom = tickFoot
        let start = foot
        return ZStack(alignment: .topLeading) {
            ForEach(placements.flatMap(\.roomIds), id: \.self) { id in
                if let tag = byId[id] {
                    let x = CGFloat((Double(tag.hz) - lowHz) / hzPerPoint).rounded() + 0.5
                    let colour = stale ? RoundtableColours.stale : Self.border(tag.glyph, dimmed: dimmed)
                    Path { path in
                        path.move(to: CGPoint(x: x, y: start))
                        path.addLine(to: CGPoint(x: x, y: max(start, bottom - 1)))
                    }
                    .stroke(colour.opacity(0.75),
                            style: StrokeStyle(lineWidth: 1, dash: [3, 2]))
                }
            }
        }
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }

    /// A dashed tick joins the card to the dial.
    private func cardTick(_ card: RoundtableCardData, placed: RoundtablePlacedCard, dimmed: Bool) -> some View {
        let x = CGFloat((Double(card.hz) - lowHz) / hzPerPoint).rounded() + 0.5
        let start = placed.rect.maxY
        let bottom = tickFoot
        return Path { path in
            path.move(to: CGPoint(x: x, y: start))
            path.addLine(to: CGPoint(x: x, y: max(start, bottom - 1)))
        }
        .stroke(RoundtableCardView.colour(card, dimmed: dimmed).opacity(0.75),
                style: StrokeStyle(lineWidth: 1, dash: [3, 2]))
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }

    private func cardButton(_ card: RoundtableCardData, placed: RoundtablePlacedCard, tag: RoundtableTag?,
                            dimmed: Bool) -> some View {
        let roomId = tag?.roomId ?? "\(card.hz)"
        return Button {
            openFold = nil
            if canView, let activeId = roundtable.activeContext?.id {
                perform(.ownMenu(activeId), hz: card.hz)
            } else if let tag {
                perform(Self.tagTap(roomId: tag.roomId, canView: false, activeContext: nil,
                                    hasDetails: tag.card != nil && readReports != nil), hz: tag.hz)
            }
        } label: {
            RoundtableCardView(card: card, plan: placed.plan, format: roundtable.signalFormat, dimmed: dimmed,
                               reconnecting: roundtable.linkState == .reconnecting,
                               offline: roundtable.linkState == .offline)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(tag.map { accessibilityLabel(RoundtableTagPlacement(left: 0, width: 0, roomIds: [$0.roomId]), tags: [$0]) } ?? "Roundtable")
        .accessibilityHint(canView || (tag?.card != nil && readReports != nil)
            ? "Offers Read reports, signal display and Tune slice \(sliceLetter) here"
            : "Tunes slice \(sliceLetter) there")
        .accessibilityIdentifier("roundtableTag.\(roomId)")
        .accessibilityActions {
            if let tag, tag.card != nil {
                if let readReports { Button("Read reports") { readReports(tag.roomId) } }
                Button("Change signal display") { roundtable.cycleSignalFormat() }
            }
        }
    }

    private func placedHeight(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag]) -> CGFloat {
        guard !placement.folded, let tag = placement.roomIds.first.flatMap({ byId[$0] }) else {
            return RoundtableTagLayout.height
        }
        return RoundtableTagLayout.tagHeight(tag)
    }

    @ViewBuilder
    private func tagView(_ placement: RoundtableTagPlacement, byId: [String: RoundtableTag],
                         stale: Bool, dimmed: Bool) -> some View {
        let tags = placement.roomIds.compactMap { byId[$0] }
        let first = tags.first
        let border = stale ? Self.staleBorder : placement.folded ? Self.foldBorder
            : first.map { Self.border($0.glyph, dimmed: dimmed) } ?? Self.foldBorder
        let open = placement.folded && openFold == placement.roomIds
        let height = placedHeight(placement, byId: byId)
        Button {
            tapped(placement, byId: byId)
        } label: {
            Group {
                if placement.folded {
                    HStack(spacing: 4) {
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
                    }
                    .padding(.leading, 5)
                    .padding(.trailing, RoundtableTagLayout.padRight)
                } else if let tag = first {
                    tagFace(tag, stale: stale, dimmed: dimmed)
                        .padding(.leading, RoundtableTagLayout.padLeft)
                        .padding(.trailing, RoundtableTagLayout.padRight)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .frame(width: CGFloat(placement.width), height: height, alignment: .topLeading)
            .background(RoundedRectangle(cornerRadius: 4) // kept-radius: roundtable-tag
                .fill(open ? Self.openFill : Self.fill))
            .overlay(alignment: .top) {
                // A double's striped top edge, inside the border (D20).
                if !placement.folded, first?.glyph == .doubled, !stale {
                    RoundtableStripes(on: RoundtableCardView.doubleText, off: RoundtableCardView.doubleOff,
                                      onWidth: 4, offWidth: 3)
                        .frame(height: 2)
                        .padding(.horizontal, 1)
                        .padding(.top, 1)
                        .opacity(dimmed ? RoundtableColours.dimmedOpacity : 1)
                        .clipShape(RoundedRectangle(cornerRadius: 4)) // kept-radius: roundtable-tag
                }
            }
            .overlay(RoundedRectangle(cornerRadius: 4) // kept-radius: roundtable-tag
                .strokeBorder(open ? Self.openBorder : border, lineWidth: 1))
            .overlay {
                // The frequency being viewed (overlay.css .rct.is-peek).
                if !placement.folded, first?.roomId == roundtable.readingRoomId, canView {
                    RoundedRectangle(cornerRadius: 5) // kept-radius: roundtable-tag
                        .strokeBorder(Self.openBorder, lineWidth: 2)
                        .padding(-2)
                }
            }
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
            : canView ? (first?.roomId == roundtable.activeContext?.id
                         ? "Offers Read reports, signal display and Tune slice \(sliceLetter) here"
                         : "Views this frequency. Your slices stay where they are.")
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

    /// The compact tag's words: its head line, then a double's names (overlay.js tagHtml).
    private func tagFace(_ tag: RoundtableTag, stale: Bool, dimmed: Bool) -> some View {
        let face = RoundtableTagLayout.face(tag)
        let isDouble = tag.glyph == .doubled
        let quiet = tag.glyph == .quiet
        let text = stale ? RoundtableColours.dim : quiet ? Self.quietText : Self.labelColour
        let headColour = isDouble && !stale ? RoundtableCardView.doubleText : text
        return VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: RoundtableTagLayout.innerGap) {
                glyph(tag.glyph, stale: stale, dimmed: dimmed)
                HStack(spacing: 0) {
                    Text(face.head)
                        .font(.system(size: RoundtableTagLayout.labelFontSize, weight: .bold))
                        .foregroundStyle(headColour)
                        .lineLimit(1)
                        .layoutPriority(0)
                    if let count = face.count {
                        Text(" ").font(.system(size: RoundtableTagLayout.labelFontSize, weight: .bold))
                        Text(count)
                            .font(.system(size: RoundtableTagLayout.labelFontSize, weight: .semibold))
                            .foregroundStyle(stale ? RoundtableColours.stale : RoundtableColours.dim)
                            .fixedSize()
                            .layoutPriority(1)
                    }
                }
            }
            .frame(height: RoundtableTagLayout.height - 2)
            ForEach(Array(face.names.enumerated()), id: \.offset) { _, name in
                Text(name)
                    .font(.system(size: RoundtableTagLayout.labelFontSize, weight: .bold))
                    .foregroundStyle(text)
                    .lineLimit(1)
                    .padding(.leading, RoundtableTagLayout.tagNameIndent)
                    .frame(height: RoundtableTagLayout.tagNameLine, alignment: .leading)
            }
        }
        .padding(.top, 1)
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
            perform(Self.tagTap(roomId: id, canView: canView, activeContext: roundtable.activeContext?.id,
                                hasDetails: tag.card != nil && readReports != nil), hz: tag.hz)
        }
    }

    private func perform(_ tap: TagTap, hz: Int64) {
        switch tap {
        case .view(let id):
            viewFrequency?(id)
        case .ownMenu(let id), .legacyDetails(let id):
            detailsTarget = MenuTarget(roomId: id, hz: hz)
            showDetails = true
        case .tune(let id):
            tuneTo(id)
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
            Text(RoundtableTagLayout.foldHeading(count: tags.count, sliceLetter: sliceLetter, viewing: canView))
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
                    perform(Self.foldRowTap(roomId: tag.roomId, canView: canView), hz: tag.hz)
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

    // MARK: Colours (board.html's .rct, .rt-fold and .rt-foldlist, design D15)

    static let fill = Color(red: 10 / 255, green: 10 / 255, blue: 12 / 255).opacity(0.88)
    static let openFill = Color(red: 0x1E / 255, green: 0x3D / 255, blue: 0x5F / 255)
    static let openBorder = Color(red: 0x5E / 255, green: 0x8D / 255, blue: 0xB8 / 255)
    static let foldBorder = Color(red: 0x71 / 255, green: 0x71 / 255, blue: 0x75 / 255)
    static let staleBorder = Color(red: 0x60 / 255, green: 0x60 / 255, blue: 0x66 / 255)
    static let mineRing = Color(red: 0, green: 0xD4 / 255, blue: 1).opacity(0.35)
    static let labelColour = Color(red: 0xEC / 255, green: 0xEC / 255, blue: 0xF0 / 255)
    static let quietText = Color(red: 0xC8 / 255, green: 0xC8 / 255, blue: 0xCE / 255)
    static let rowText = Color(red: 0xE8 / 255, green: 0xE8 / 255, blue: 0xEC / 255)
    static let listFill = Color(red: 0x1B / 255, green: 0x1B / 255, blue: 0x1F / 255)
    static let listBorder = Color(red: 0x2C / 255, green: 0x2C / 255, blue: 0x31 / 255)
    static let listRule = Color(red: 0x2A / 255, green: 0x2A / 255, blue: 0x2E / 255)
    static let rowRule = Color(red: 0x1E / 255, green: 0x1E / 255, blue: 0x22 / 255)

    /// The tag's border: the state's colour, the quiet one darker than its
    /// glyph, a double the darker red; `dimmed` (you are hidden) at
    /// RoundtableColours.dimmedOpacity.
    static func border(_ glyph: RoundtableFlagLine.Glyph, dimmed: Bool = false) -> Color {
        let colour = glyph == .quiet ? staleBorder : glyph == .doubled ? RoundtableCardView.doubleEdge
            : RoundtableColours.colour(glyph)
        return dimmed ? colour.opacity(RoundtableColours.dimmedOpacity) : colour
    }
}

// MARK: The card

/// The live card for the active frequency (overlay design D10, D11, D13, D15).
/// It only draws: `plan` says what fits, and readings, ages and the clock change in
/// place, each row keeping its identity, so nothing shifts or flickers (D14).
struct RoundtableCardView: View {
    let card: RoundtableCardData
    let plan: RoundtableCardPlan
    let format: RoundtableSignalFormat
    var dimmed = false
    var reconnecting = false
    var offline = false

    static let fill = Color(red: 10 / 255, green: 10 / 255, blue: 12 / 255).opacity(0.92)
    static let doubleEdge = Color(red: 0xC2 / 255, green: 0x5A / 255, blue: 0x5C / 255)
    static let doubleText = Color(red: 0xE0 / 255, green: 0x6C / 255, blue: 0x6E / 255)
    static let doubleOff = Color(red: 0x3A / 255, green: 0x1A / 255, blue: 0x1B / 255)
    static let grey = Color(red: 0x60 / 255, green: 0x60 / 255, blue: 0x66 / 255)
    static let greyBar = Color(red: 0x7E / 255, green: 0x7E / 255, blue: 0x85 / 255)
    static let lastText = Color(red: 0xC8 / 255, green: 0xC8 / 255, blue: 0xCE / 255)
    static let oldText = Color(red: 0x90 / 255, green: 0x90 / 255, blue: 0x96 / 255)
    static let dimmedEdge = Color(red: 160 / 255, green: 160 / 255, blue: 168 / 255).opacity(0.55)

    /// The state's colour on the rail, the border and the tick. A dropped link has none (D7).
    static func colour(_ card: RoundtableCardData, dimmed: Bool = false) -> Color {
        if card.frozen { return grey }
        if dimmed { return dimmedEdge }
        switch card.kind {
        case .talking: return RoundtableColours.talking
        case .you: return RoundtableColours.you
        case .unknown: return RoundtableColours.unknown
        case .double: return doubleEdge
        case .lastTalker, .lastDouble, .quiet: return grey
        }
    }

    private var glyph: RoundtableFlagLine.Glyph {
        switch card.kind {
        case .talking: .talking
        case .you: .you
        case .unknown: .unknown
        case .double, .lastDouble: .doubled
        case .lastTalker, .quiet: .quiet
        }
    }

    private var whoColour: Color {
        if card.frozen { return Self.lastText }
        switch card.kind {
        case .double: return Self.doubleText
        case .you: return RoundtableColours.you
        case .unknown: return RoundtableColours.unknown
        case .lastTalker, .lastDouble, .quiet: return Self.lastText
        case .talking: return RoundtableTagLayer.labelColour
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            rail
            VStack(alignment: .leading, spacing: RoundtableCardPlan.gap) {
                head
                if plan.names > 0 || (plan.moreKeyed > 0 && plan.showsMoreKeyed) { names }
                if plan.hasPastNames {
                    noteText(card.keyed.map { RoundtableTagLayout.cardName($0, in: card) }.joined(separator: " + "))
                }
                if plan.hasNote, let note = card.note { noteLine(note) }
                if plan.hasCaption { caption }
                if plan.rows > 0 {
                    VStack(spacing: RoundtableCardPlan.gap) {
                        ForEach(Array(card.rows.prefix(plan.rows).enumerated()), id: \.offset) { _, row in
                            RoundtableCardRow(row: row, kind: card.kind, frozen: card.frozen, format: format,
                                              narrow: plan.narrow, tight: plan.tight)
                        }
                    }
                }
                if plan.hasEmptyLine {
                    noteText(card.note?.hasPrefix("Double ended") == true ? "Waiting for readings" : "No readings yet")
                }
                if plan.moreRows > 0 && plan.showsMoreRows {
                    Text("+\(plan.moreRows) more \u{203A}")
                        .font(.system(size: 9.5, weight: .medium))
                        .foregroundStyle(RoundtableColours.accentText)
                        .frame(height: RoundtableCardPlan.footHeight, alignment: .bottomLeading)
                }
            }
            .padding(.vertical, RoundtableCardPlan.padVertical)
            .padding(.horizontal, RoundtableCardPlan.padHorizontal)
        }
        .frame(maxWidth: .infinity, alignment: .topLeading)
        .padding(RoundtableCardPlan.border)
        .frame(width: plan.width, height: plan.height, alignment: .topLeading)
        .background(RoundedRectangle(cornerRadius: 5).fill(Self.fill)) // kept-radius: roundtable-tag
        .clipShape(RoundedRectangle(cornerRadius: 5)) // kept-radius: roundtable-tag
        .overlay(RoundedRectangle(cornerRadius: 5) // kept-radius: roundtable-tag
            .strokeBorder(Self.colour(card, dimmed: dimmed), lineWidth: RoundtableCardPlan.border))
        .shadow(color: .black.opacity(0.75), radius: 9, y: 6)
        .saturation(offline ? 0 : 1)
        .opacity(dimmed && !card.frozen ? 0.86 : 1)
    }

    @ViewBuilder
    private var rail: some View {
        if card.kind == .double && !card.frozen {
            RoundtableStripes(on: Self.doubleText, off: Self.doubleOff, onWidth: 5, offWidth: 4)
                .frame(height: RoundtableCardPlan.rail)
        } else {
            Rectangle().fill(Self.colour(card, dimmed: dimmed)).frame(height: RoundtableCardPlan.rail)
        }
    }

    private var head: some View {
        HStack(spacing: 5) {
            RoundtableGlyphView(glyph: glyph, size: RoundtableTagLayout.glyphSize, dimmed: dimmed && !card.frozen)
                .saturation(card.frozen || card.kind == .lastDouble ? 0.15 : 1)
            Text(RoundtableTagLayout.cardHead(card, narrow: plan.narrow))
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(whoColour)
                .lineLimit(1)
                .truncationMode(.tail)
            // The clock is on a double's head only.
            if card.kind == .double && !plan.narrow, let clock = RoundtableTagLayout.cardClockText(card) {
                Text(clock)
                    .font(.system(size: 10, weight: .semibold, design: .monospaced))
                    .foregroundStyle(RoundtableColours.dim)
                    .fixedSize()
            }
            Spacer(minLength: 0)
            if !plan.tight {
                Text(RoundtableTagLayout.cardFrequency(card.hz))
                    .font(.system(size: 9.5, weight: .semibold, design: .monospaced))
                    .foregroundStyle(RoundtableColours.accentText)
                    .fixedSize()
            }
        }
        .frame(height: RoundtableCardPlan.headHeight)
    }

    private func nameView(_ name: String) -> some View {
        let shown = RoundtableTagLayout.cardName(name, in: card)
        let you = name == card.keyedYou
        return HStack(spacing: 5) {
            RoundtableKeyedDot(colour: card.frozen ? Self.greyBar : you ? RoundtableColours.you : Self.doubleText,
                               pulses: !card.frozen)
            Text(shown)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(you && !card.frozen ? RoundtableColours.you : RoundtableTagLayer.labelColour)
                .lineLimit(1)
                .fixedSize()
        }
        .frame(height: RoundtableCardPlan.nameLine, alignment: .leading)
        .frame(minWidth: 0, alignment: .leading)
        .clipped()
    }

    private var names: some View {
        VStack(alignment: .leading, spacing: RoundtableCardPlan.gap) {
            if plan.names > 0 {
                let shown = Array(card.keyed.prefix(plan.names))
                if plan.namesInRow {
                    HStack(spacing: plan.namesGap) {
                        ForEach(Array(shown.enumerated()), id: \.offset) { _, name in nameView(name) }
                    }
                } else {
                    ForEach(Array(shown.enumerated()), id: \.offset) { _, name in nameView(name) }
                }
            }
            if plan.moreKeyed > 0 && plan.showsMoreKeyed {
                noteText("+\(plan.moreKeyed) keyed")
            }
        }
        .padding(.bottom, RoundtableCardPlan.gap)
    }

    private func noteText(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 9.5, weight: .medium))
            .foregroundStyle(RoundtableColours.dim)
            .lineLimit(1)
            .frame(height: RoundtableCardPlan.noteLine, alignment: .leading)
    }

    @ViewBuilder
    private func noteLine(_ note: String) -> some View {
        if reconnecting {
            HStack(spacing: 4) {
                RoundtableGlyphView(glyph: .waiting, size: 8)
                Text(note)
                    .font(.system(size: 9.5, weight: .medium))
                    .foregroundStyle(RoundtableColours.unknown)
                    .lineLimit(1)
            }
            .frame(height: RoundtableCardPlan.noteLine, alignment: .leading)
        } else {
            noteText(note)
        }
    }

    private var caption: some View {
        let live = card.kind == .double && !card.frozen
        return HStack(spacing: 6) {
            Text(RoundtableTagLayout.cardCaption(card, narrow: plan.narrow))
                .font(.system(size: 9.5, weight: live ? .semibold : .medium))
                .foregroundStyle(live ? Self.doubleText : RoundtableColours.dim)
                .lineLimit(1)
            Spacer(minLength: 0)
            if plan.rows > 0 && !plan.narrow {
                Text(RoundtableTagLayout.cardFormatTitle(format))
                    .font(.system(size: 9.5, weight: .medium))
                    .foregroundStyle(RoundtableColours.dim)
                    .fixedSize()
            }
        }
        .frame(height: RoundtableCardPlan.noteLine)
        .padding(.top, RoundtableCardPlan.captionHeight - RoundtableCardPlan.noteLine)
        .overlay(alignment: .top) {
            Rectangle().fill(RoundtableTagLayer.listRule).frame(height: 1)
        }
    }
}

/// One reading: who, what, how old, and the meter beneath (D11, D13).
struct RoundtableCardRow: View {
    let row: RoundtableCardData.Row
    let kind: RoundtableCardData.Kind
    let frozen: Bool
    let format: RoundtableSignalFormat
    let narrow: Bool
    let tight: Bool
    /// A frequency view's distance and bearing, in their own column (D22).
    var path: String? = nil

    var body: some View {
        let old = row.old
        VStack(spacing: 1) {
            HStack(alignment: .firstTextBaseline, spacing: 6) {
                // A call that does not fit is cut, never overlapped.
                Text(row.label ?? (row.you ? "You" : row.call))
                    .font(.system(size: 10.5, weight: .semibold))
                    .foregroundStyle(old ? RoundtableCardView.oldText
                                     : row.you ? RoundtableColours.you : RoundtableTagLayer.labelColour)
                    .lineLimit(1)
                    .fixedSize()
                    .frame(minWidth: 0, maxWidth: .infinity, alignment: .leading)
                    .clipped()
                if let path {
                    Text(path)
                        .font(.system(size: 10, weight: .medium, design: .monospaced))
                        .foregroundStyle(RoundtableColours.dim)
                        .lineLimit(1)
                        .fixedSize()
                }
                Text(RoundtableTagLayout.cardSignal(row.dbm, format: format, narrow: narrow))
                    .font(.system(size: 10.5, weight: .semibold, design: .monospaced))
                    .foregroundStyle(old ? RoundtableCardView.oldText : RoundtableTagLayer.labelColour)
                    .lineLimit(1)
                    .fixedSize()
                if !tight {
                    Text(RoundtableTagLayout.cardAge(row.age))
                        .font(.system(size: 9.5, weight: .medium, design: .monospaced))
                        .foregroundStyle(old ? RoundtableColours.unknown : RoundtableColours.dim)
                        .lineLimit(1)
                        .fixedSize()
                        .frame(minWidth: 12, alignment: .trailing)
                }
            }
            .frame(height: RoundtableCardPlan.rowHeight - 1 - RoundtableMeterBar.height)
            RoundtableMeterBar(fraction: RoundtableTagLayout.meterFraction(row.dbm),
                               peak: row.peak.map(RoundtableTagLayout.meterFraction),
                               style: RoundtableTagLayout.meterStyle(kind: kind, row: row, frozen: frozen))
        }
    }
}

/// A keyed name's dot, breathing while it is on the air.
private struct RoundtableKeyedDot: View {
    let colour: Color
    let pulses: Bool
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var faded = false

    var body: some View {
        Circle().fill(colour)
            .frame(width: 7, height: 7)
            .opacity(pulses && faded ? 0.45 : 1)
            .onChange(of: pulses && !reduceMotion, initial: true) { _, now in
                if now {
                    withAnimation(.easeInOut(duration: 0.55).repeatForever(autoreverses: true)) { faded = true }
                } else {
                    withTransaction(Transaction(animation: nil)) { faded = false }
                }
            }
    }
}
