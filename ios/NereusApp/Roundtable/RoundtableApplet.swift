// NereusSDR for iOS: the iPad's Roundtable applet in the applet column, narrow and widened, and the applets' order
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 19, the iPad's Roundtable applet.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: the widened list sorts its rooms once
// per change, not each second; the TX line's clock is read in the room's
// timeline. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// The applets in the iPad's column, top to bottom.
enum AppletKind: String, CaseIterable, Equatable {
    case sMeter
    case roundtable
    case rx
    case tx
}

/// Where the Roundtable applet sits among the column's applets (spec
/// 11.4): second, after the S-Meter, unless moved. The others keep their
/// order around it.
enum AppletOrder {
    /// Second, after the S-Meter.
    static let roundtableDefault = 1
    static let lastPosition = 3

    static func clamped(_ position: Int) -> Int {
        min(max(position, 0), lastPosition)
    }

    /// The column's applets with Roundtable at `position`.
    static func order(roundtableAt position: Int) -> [AppletKind] {
        var kinds: [AppletKind] = [.sMeter, .rx, .tx]
        kinds.insert(.roundtable, at: clamped(position))
        return kinds
    }
}

private struct RoundtableInColumnKey: EnvironmentKey {
    static let defaultValue = false
}

extension EnvironmentValues {
    /// A flag's Roundtable line shows the applet in the column instead of
    /// opening the sheet (an iPad on its side with the column showing).
    var roundtableInColumn: Bool {
        get { self[RoundtableInColumnKey.self] }
        set { self[RoundtableInColumnKey.self] = newValue }
    }
}

/// What the applet column needs for its Roundtable applet: the main
/// screen's Roundtable state and the station.
struct RoundtableAppletConfig {
    let roundtable: RoundtableModel
    let settings: PhoneSettings
    let stationCallsign: String
    /// The column widened to the room list plus the room.
    let wide: Binding<Bool>
    /// Bumped to scroll the column to the applet (a flag's Roundtable line).
    let scrollRequest: Int
    /// The first-run note was shown on this device.
    let firstRunNoteSeen: Binding<Bool>
    /// Opens the first-run question, a form sheet in the middle of the screen.
    let askFirstRun: () -> Void
}

/// The Roundtable applet (spec 11.4, picture 6): a title bar like the
/// S-Meter's with Widen and the applet's menu, then the room's head, the
/// connection line, and the room laid in the column's scroll (the talker,
/// the readings without their bar, the earlier turns and the messages).
/// The column pins the box at its foot while the applet is in view.
struct RoundtableApplet: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var slices: BandSlicesModel
    let stationCallsign: String
    let wide: Bool
    let toggleWide: () -> Void
    var moveUp: (() -> Void)?
    var moveDown: (() -> Void)?
    var embedding: RoundtableRoomEmbedding?
    @Binding var firstRunNoteSeen: Bool
    let askFirstRun: () -> Void

    /// The applet's id in the column's scroll.
    static let id = "roundtableApplet"
    static let widen = "\u{21D4} Widen"
    static let narrow = "\u{21D4} Narrow"

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            RoundtableAppletTitle(wide: wide, toggleWide: toggleWide, moveUp: moveUp, moveDown: moveDown)
            RoundtableAppletRoom(roundtable: roundtable, transmit: transmit, slices: slices,
                                 stationCallsign: stationCallsign, narrow: true, embedding: embedding,
                                 firstRunNoteSeen: $firstRunNoteSeen, askFirstRun: askFirstRun)
        }
        .background(RoundtableColours.ground)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier(Self.id)
    }
}

/// The applet's title bar: "Roundtable", Widen or Narrow, and its menu
/// (move it up or down the column, widen or narrow).
struct RoundtableAppletTitle: View {
    let wide: Bool
    let toggleWide: () -> Void
    var moveUp: (() -> Void)?
    var moveDown: (() -> Void)?

    var body: some View {
        HStack(spacing: 0) {
            Text("Roundtable")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
            Spacer(minLength: 0)
            Button(action: toggleWide) {
                Text(wide ? RoundtableApplet.narrow : RoundtableApplet.widen)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(ChromeColours.icon)
                    .padding(.horizontal, 8)
                    .frame(height: 18)
                    .background(Color.white.opacity(0.08), in: RoundedRectangle(cornerRadius: 3))
                    .frame(minWidth: 44, minHeight: AnalogSMeter.titleHeight)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(wide ? "Narrow Roundtable" : "Widen Roundtable")
            .accessibilityIdentifier("roundtableWiden")
            Menu {
                Button(wide ? "Narrow" : "Widen", action: toggleWide)
                if !wide {
                    Button("Move up") { moveUp?() }
                        .disabled(moveUp == nil)
                    Button("Move down") { moveDown?() }
                        .disabled(moveDown == nil)
                }
            } label: {
                Image(systemName: "line.3.horizontal")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.icon)
                    .frame(width: 44, height: AnalogSMeter.titleHeight)
                    .contentShape(Rectangle())
            }
            .accessibilityLabel("Roundtable menu")
            .accessibilityIdentifier("roundtableAppletMenu")
        }
        .padding(.leading, 8)
        .frame(maxWidth: .infinity, minHeight: AnalogSMeter.titleHeight, maxHeight: AnalogSMeter.titleHeight)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }
}

/// The room as the applet shows it: its head and connection line, then
/// the first-run question's line, the note, or the room. Narrow, the room
/// is laid in the column's scroll; widened it scrolls itself.
struct RoundtableAppletRoom: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var slices: BandSlicesModel
    let stationCallsign: String
    let narrow: Bool
    var embedding: RoundtableRoomEmbedding?
    @Binding var firstRunNoteSeen: Bool
    let askFirstRun: () -> Void

    private var keyed: Bool { transmit.ptt.transmitting }
    private var noting: Bool {
        roundtable.available && roundtable.firstRunAnswered && !firstRunNoteSeen
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if let room = roundtable.room, !roundtable.askingFirstRun {
                RoundtableRoomHead(room: room)
                    .padding(.horizontal, 12)
                    .frame(maxWidth: .infinity, minHeight: 28, alignment: .leading)
            }
            RoundtableConnectionLine(roundtable: roundtable, room: roundtable.room)
            if let reason = roundtable.coreReason {
                note(reason, detail: nil)
            } else if roundtable.askingFirstRun {
                RoundtableFirstRunPrompt(ask: askFirstRun)
            } else {
                if noting {
                    RoundtableFirstRunNote(hidden: roundtable.hidden) { firstRunNoteSeen = true }
                }
                if let room = roundtable.room {
                    RoundtableRoomView(roundtable: roundtable, room: room, full: true, keyed: keyed,
                                       keyedSince: PttButton.keyedSince(transmit.ptt),
                                       keyedForSeconds: Int(transmit.keyedForSeconds),
                                       tunedHere: roundtable.tunedToShownRoom,
                                       stationCallsign: stationCallsign,
                                       ownDbm: slices.active?.signalDbm,
                                       sliceLetter: slices.active?.slice.letter ?? "A",
                                       tuneHere: roundtable.readingRoomId == nil ? nil : { tuneHere(room) },
                                       narrow: narrow, embedded: embedding)
                } else {
                    note("Nobody on Roundtable is tuned here.", detail: "Tap a tag on the band to tune there.")
                }
            }
        }
    }

    private func note(_ title: String, detail: String?) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.system(size: 13, weight: .bold)).foregroundStyle(RoundtableColours.bright)
                .fixedSize(horizontal: false, vertical: true)
            if let detail {
                Text(detail).font(.system(size: 12)).foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .topLeading)
        .accessibilityIdentifier("roundtableEmpty")
    }

    private func tuneHere(_ room: RoundtableModel.Room) {
        slices.tap(to: Double(room.hz))
        Task { await roundtable.backToSlice() }
    }
}

/// The foot the column pins while the applet is in view: the pills, then
/// the box. Keyed, there is no box. PTT stays on the band, a tap as ever,
/// so nothing beside the box keys.
struct RoundtableAppletFoot: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var scroll: RoundtableRoomScroll
    let stationCallsign: String
    let jump: (Int64) -> Void
    let toEnd: () -> Void

    var body: some View {
        if !transmit.ptt.transmitting {
            VStack(spacing: 0) {
                if let room = roundtable.room {
                    RoundtableRoomPills(waitingBelow: RoundtableRoomPills.newCount(waiting: scroll.waiting,
                                                                                   atEnd: scroll.atEnd, full: true),
                                        mentions: RoundtableRoomView.unreadMentions(roundtable, room: room,
                                                                                    stationCallsign: stationCallsign),
                                        jump: jump,
                                        toEnd: {
                                            toEnd()
                                            scroll.clearWaiting()
                                        })
                }
                RoundtableComposer(roundtable: roundtable) { EmptyView() }
            }
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("roundtableAppletFoot")
        }
    }

    /// The box is pinned while at least this much of the applet is in view.
    static let pinnedOverlap: CGFloat = 60

    /// The applet at `applet` (in the column's scroll) is in view enough
    /// in a column `viewportHeight` tall for its box to be pinned.
    static func boxPinned(applet: CGRect, viewportHeight: CGFloat) -> Bool {
        guard viewportHeight > 0, !applet.isEmpty else {
            return false
        }
        let overlap = min(applet.maxY, viewportHeight) - max(applet.minY, 0)
        return overlap >= pinnedOverlap
    }
}

/// The widened applet (spec 11.4, picture 6's second): the column gives
/// the band up for the room list, by band, beside the room. A tap on a row
/// reads that room (your slice's own row goes back to it); nothing tunes.
struct RoundtableWideApplet: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var slices: BandSlicesModel
    let stationCallsign: String
    let toggleWide: () -> Void
    @Binding var firstRunNoteSeen: Bool
    let askFirstRun: () -> Void

    /// The room list's width (the board's).
    static let listWidth: CGFloat = 250

    private var letters: [Int64: String] {
        Dictionary(slices.entries.map { (Int64($0.id), $0.slice.letter) }, uniquingKeysWith: { first, _ in first })
    }

    var body: some View {
        VStack(spacing: 0) {
            RoundtableAppletTitle(wide: true, toggleWide: toggleWide)
            HStack(spacing: 0) {
                list
                    .frame(width: Self.listWidth)
                Rectangle().fill(RoundtableColours.rule).frame(width: 1)
                VStack(spacing: 0) {
                    RoundtableAppletRoom(roundtable: roundtable, transmit: transmit, slices: slices,
                                         stationCallsign: stationCallsign, narrow: false,
                                         firstRunNoteSeen: $firstRunNoteSeen, askFirstRun: askFirstRun)
                        .frame(maxHeight: .infinity, alignment: .top)
                    if !transmit.ptt.transmitting, roundtable.room != nil, !roundtable.askingFirstRun {
                        RoundtableComposer(roundtable: roundtable) { EmptyView() }
                    }
                }
                .frame(maxWidth: .infinity)
            }
        }
        .background(RoundtableColours.ground)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier(RoundtableApplet.id)
    }

    private var list: some View {
        // Grouped and sorted once per pass of the list; each second only ages the rows.
        let groups = RoundtableToolsList.groups(roundtable.rooms)
        return ScrollView {
            TimelineView(.periodic(from: .now, by: 1)) { context in
                let now = Int64(context.date.timeIntervalSince1970 * 1000)
                LazyVStack(alignment: .leading, spacing: 0) {
                    if groups.isEmpty {
                        Text("Nobody on Roundtable is on the air.")
                            .font(.system(size: 12))
                            .foregroundStyle(RoundtableColours.dim)
                            .padding(12)
                    }
                    ForEach(groups, id: \.chip.id) { group in
                        bandHead(group.chip)
                        let rows = RoundtableToolsList.rows(ordered: group.rooms,
                                                            sliceRooms: roundtable.sliceRooms, letters: letters,
                                                            mentionCounts: roundtable.mentionCounts, now: now)
                        ForEach(rows) { row in
                            RoundtableRoomRow(row: row, frequencyWidth: 60,
                                              identifier: "roundtableWide.row.\(row.id)") {
                                open(row.id)
                            }
                            .background(row.id == roundtable.shownRoomId && !row.mine
                                        ? Color.white.opacity(0.06) : Color.clear)
                            .accessibilityAddTraits(row.id == roundtable.shownRoomId ? .isSelected : [])
                        }
                    }
                }
            }
        }
        .scrollIndicators(.hidden)
        .accessibilityIdentifier("roundtableWideList")
    }

    private func bandHead(_ band: RoundtableToolsList.Chip) -> some View {
        HStack(spacing: 6) {
            Text(band.title).font(.system(size: 11, weight: .bold)).foregroundStyle(RoundtableColours.dim)
            Text("\(band.count) room\(band.count == 1 ? "" : "s")").font(.system(size: 11))
                .foregroundStyle(RoundtableColours.dim)
        }
        .padding(.horizontal, 12)
        .padding(.top, 10)
        .padding(.bottom, 4)
        .accessibilityAddTraits(.isHeader)
    }

    private func open(_ roomId: String) {
        Task {
            if roomId == roundtable.sliceRoomId {
                await roundtable.backToSlice()
            } else {
                _ = await roundtable.read(roomId)
            }
        }
    }
}
