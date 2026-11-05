// NereusSDR for iOS: the Roundtable sheet over the band: half and full height, the room, the pinned box and the hold PTT
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the phone's Roundtable sheet.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the room's head, connection line and
// message box made their own views for the iPad's applet to share, and the
// first-run question asked in a form sheet on an iPad. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: keyed while the first-run question is
// open, the hold PTT shows at its foot; the TX line's clock is read in the
// room's timeline. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave re-review: keyed reads
// PttButton.holdKeyed, the state the hold PTT's quick-tap unkey reads.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: JJ's answer: on a phone the first-run question opens the
// sheet at full height, so Stay hidden and Go live show without a scroll.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

// 2026-10-30: Approved frequency-first people, reports and phone interactions.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-03: Viewing a frequency without tuning, in the sheet (overlay design D21 to D24).
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-04: One sheet on every frequency: your own uses the frequency view, Back stays
// up on your own, and the more button has Signal and Cancel (overlay design D22, D25, D36, D38).
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import SwiftUI

/// The Roundtable sheet (spec 11.3, D17, U2, U3, U14): a flag's Roundtable
/// line opens it at half height over the band, and a tap on the handle
/// makes it full height. Half shows the talker and the readings with the
/// Offset column; full adds the earlier turns with Name and the messages,
/// in one scroll. The message box is pinned at its foot with PTT beside
/// it, smaller and 24 points clear, press and hold with a dashed ring.
///
/// Keyed, the sheet drops below the keyed gauges and shows the TX line and
/// every station's reading of you. With a tuning dial on, the sheet stops
/// above the 92-point strip that keeps PTT at the left and the dial at the
/// right; keyed with a dial, the box steps aside and its draft is kept.
/// Before the station's first answer the sheet asks it
/// (``RoundtableFirstRunSheet``) at full height and sends nothing until then.
struct RoundtableSheet: View {
    enum Detent: Equatable {
        case half
        case full
    }

    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var slices: BandSlicesModel
    @Binding var detent: Detent
    /// A tuning dial is on: PTT is in the strip below, not beside the box.
    let dialOn: Bool
    /// The session is up: PTT can be used.
    let linkUp: Bool
    /// The station's callsign and grid, as the Core's settings give them.
    let stationCallsign: String
    let stationGrid: String
    /// The first-run note was shown on this phone.
    @Binding var firstRunNoteSeen: Bool
    let close: () -> Void
    /// Back to your own frequency, in place: the sheet stays up (D35, D38).
    let back: () -> Void
    var holdObserver: ((PttHold.Action) -> Void)?
    /// On an iPad the first-run question is a form sheet in the middle of
    /// the screen (spec 11.4): the sheet shows a line that asks it here,
    /// and this opens it. Nil asks in the sheet itself, as on a phone.
    var askFirstRun: (() -> Void)?
    /// This client's pan at a frequency and family, for a view's Your pan.
    var panLevel: ((Int64, String) -> Double?)?

    /// The question was answered here: no note follows it.
    @State private var answeredHere = false
    @State private var compactLandscape = false

    static let handleHeight: CGFloat = 22

    /// The first-run question asked in the sheet itself (a phone) takes the
    /// full height, so its two buttons show without a scroll. An iPad asks
    /// in its form sheet and keeps the height it has.
    static func firstRunAtFullHeight(asking: Bool, asksHere: Bool) -> Bool {
        asking && asksHere
    }

    private var keyed: Bool { PttButton.holdKeyed(transmit.ptt) }
    private var room: RoundtableModel.Room? { roundtable.room }
    private var roomId: String? { roundtable.shownRoomId }
    private var tunedHere: Bool { roundtable.tunedToShownRoom }
    private var activeLetter: String {
        slices.entries.first { $0.id == slices.activeSliceId }?.slice.letter ?? "A"
    }
    private var asking: Bool { roundtable.askingFirstRun }
    private var noting: Bool {
        roundtable.available && roundtable.firstRunAnswered && !firstRunNoteSeen && !answeredHere
    }
    /// A frequency is being viewed, not your own (overlay design D21).
    private var viewing: Bool { roundtable.isViewing }
    /// A short landscape sheet needs its height for the reports: your own frequency's
    /// greyed Tune, which has nothing to do there, sits with them, not pinned.
    private var ownTuneScrolls: Bool { compactLandscape && !viewing }

    var body: some View {
        VStack(spacing: 0) {
            // Keyed, the sheet below the gauges is the TX line and the readings of you, as the board draws it.
            if !keyed && !compactLandscape {
                handle
            }
            if asking, let askFirstRun {
                header
                RoundtableFirstRunPrompt(ask: askFirstRun)
                Spacer(minLength: 0)
                keyedFoot
            } else if asking {
                RoundtableFirstRunSheet(roundtable: roundtable, callsign: stationCallsign, grid: stationGrid) {
                    answeredHere = true
                    firstRunNoteSeen = true
                }
                keyedFoot
            } else {
                if noting {
                    RoundtableFirstRunNote(hidden: roundtable.hidden) { firstRunNoteSeen = true }
                }
                if viewing {
                    inspectorBar
                } else if !keyed {
                    if roundtable.frequencyAvailable {
                        inspectorBar
                    } else {
                        header
                        if !compactLandscape { connection }
                    }
                }
                content
                if !(dialOn && keyed) {
                    RoundtableComposer(roundtable: roundtable, leadingWidth: dialOn ? 0
                                       : (compactLandscape ? 44 + 8 : PttButton.holdDiameter + PttButton.holdClearance),
                                       compact: compactLandscape) {
                        if !dialOn {
                            PttButton(transmit: transmit, linkUp: linkUp, mode: .hold(diameter: compactLandscape ? 44 : PttButton.holdDiameter),
                                      holdObserver: holdObserver)
                            Spacer().frame(width: compactLandscape ? 8 : PttButton.holdClearance)
                        }
                    }
                }
            }
        }
        .onGeometryChange(for: Bool.self) { geometry in
            geometry.size.width > 500 && geometry.size.height < 350
        } action: { compact in
            compactLandscape = compact
            if compact { detent = .full }
        }
        .background(RoundtableColours.ground)
        .clipShape(UnevenRoundedRectangle(topLeadingRadius: WorkspacePalette.panelRadius, topTrailingRadius: WorkspacePalette.panelRadius))
        .overlay {
            UnevenRoundedRectangle(topLeadingRadius: WorkspacePalette.panelRadius, topTrailingRadius: WorkspacePalette.panelRadius)
                .strokeBorder(RoundtableColours.rule, lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.5), radius: 12, y: -2)
        .onAppear { roundtable.activeSliceId = slices.activeSliceId.map(Int64.init) }
        .onAppear { fullForFirstRun() }
        .onChange(of: asking) { _, _ in fullForFirstRun() }
        .onChange(of: slices.activeSliceId) { _, id in roundtable.activeSliceId = id.map(Int64.init) }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableSheet")
    }

    private func fullForFirstRun() {
        if Self.firstRunAtFullHeight(asking: asking, asksHere: askFirstRun == nil) {
            detent = .full
        }
    }

    /// Keyed while the question is open, the band's PTT hidden under the
    /// sheet: the hold PTT, so a key made before the sheet opened (a tap's,
    /// TUNE's) can come off here with a tap. With a dial on, PTT is in the
    /// strip below already.
    static func showsKeyedFoot(asking: Bool, keyed: Bool, dialOn: Bool) -> Bool {
        asking && keyed && !dialOn
    }

    @ViewBuilder
    private var keyedFoot: some View {
        if Self.showsKeyedFoot(asking: asking, keyed: keyed, dialOn: dialOn) {
            HStack(spacing: 0) {
                PttButton(transmit: transmit, linkUp: linkUp, mode: .hold(diameter: PttButton.holdDiameter),
                          holdObserver: holdObserver)
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
        }
    }

    // MARK: Handle and header

    private var handle: some View {
        Button {
            detent = detent == .half ? .full : .half
        } label: {
            Capsule().fill(RoundtableColours.dim2).frame(width: 36, height: 4)
                .frame(maxWidth: .infinity, minHeight: Self.handleHeight)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .simultaneousGesture(DragGesture(minimumDistance: 12).onEnded { drag in
            if drag.translation.height < -20 {
                detent = .full
            } else if drag.translation.height > 20 {
                if detent == .full {
                    detent = .half
                } else {
                    close()
                }
            }
        })
        .accessibilityLabel(detent == .half ? "Full height" : "Half height")
        .accessibilityIdentifier("roundtableHandle")
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text("Roundtable").font(.system(size: 14, weight: .bold)).foregroundStyle(RoundtableColours.bright)
            if let room {
                RoundtableRoomHead(room: room)
            }
            Spacer(minLength: 4)
            Button(action: close) {
                Image(systemName: "xmark")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(RoundtableColours.text)
                    .frame(width: 30, height: 30)
                    .background(Color.white.opacity(0.08), in: Circle())
                    .frame(width: 44, height: 44)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Close Roundtable")
            .accessibilityIdentifier("roundtableClose")
        }
        .padding(.leading, 12)
        .padding(.trailing, 4)
        .frame(height: compactLandscape ? 44 : 40)
    }

    /// The one sheet's head: Back or your frequency, then more and close (overlay.js renderInsSheet).
    private var inspectorBar: some View {
        RoundtableInspectorBar(roundtable: roundtable, viewing: viewing, closeId: "roundtableClose",
                               onBack: back, onClose: close)
            .frame(height: compactLandscape ? 44 : 40)
    }

    private var connection: some View {
        RoundtableConnectionLine(roundtable: roundtable, room: room)
    }

    // MARK: The room

    @ViewBuilder
    private var content: some View {
        if let reason = roundtable.coreReason {
            message(reason, detail: nil)
        } else if roundtable.frequencyAvailable, let room {
            // One sheet on every frequency (D38). Your own keeps its earlier turns at full height, where they can be named.
            RoundtableRoomView(roundtable: roundtable, room: room, full: true, keyed: keyed,
                               keyedSince: nil, keyedForSeconds: 0, tunedHere: tunedHere,
                               stationCallsign: stationCallsign, ownDbm: nil, sliceLetter: activeLetter,
                               compact: compactLandscape,
                               frequencyHead: AnyView(
                                RoundtableFrequencyHost(roundtable: roundtable, slices: slices, contextId: room.id,
                                                        viewing: viewing, keyed: transmit.ptt.transmitting, linkUp: linkUp,
                                                        panLevel: panLevel, showsBack: false, showsTune: ownTuneScrolls,
                                                        onTune: { _, _ in }, onBack: back)),
                               frequencyFoot: ownTuneScrolls ? nil : AnyView(
                                RoundtableFrequencyTuneBar(roundtable: roundtable, slices: slices, contextId: room.id,
                                                           viewing: viewing, keyed: transmit.ptt.transmitting, linkUp: linkUp,
                                                           onTune: { hz, family in
                                                               slices.tune(toHz: hz, family: family)
                                                               close()
                                                           })),
                               showsEarlier: !viewing && detent == .full, viewing: viewing)
                // Your own frequency's id changes with every tune step, and its received
                // messages do not: the view keeps its scroll, seen state and typed name.
                .id(viewing ? room.id : RoundtableFrequencyView.ownViewId)
        } else if let room {
            RoundtableRoomView(roundtable: roundtable, room: room, full: detent == .full, keyed: keyed,
                               keyedSince: PttButton.keyedSince(transmit.ptt),
                               keyedForSeconds: Int(transmit.keyedForSeconds), tunedHere: tunedHere, stationCallsign: stationCallsign,
                               ownDbm: slices.entries.first { $0.id == slices.activeSliceId }?.signalDbm,
                               sliceLetter: activeLetter, draftKept: dialOn && keyed && !draft.isEmpty,
                               tuneHere: roundtable.readingRoomId == nil ? nil : { tuneHere(room) },
                               expand: { detent = .full }, compact: compactLandscape)
        } else {
            message("Nobody on Roundtable is tuned here.",
                    detail: roundtable.frequencyAvailable ? "Tap a tag on the band to view that frequency."
                        : "Tap a tag on the band to tune there.")
        }
    }

    private func message(_ title: String, detail: String?) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.system(size: 13, weight: .bold)).foregroundStyle(RoundtableColours.bright)
                .fixedSize(horizontal: false, vertical: true)
            if let detail {
                Text(detail).font(.system(size: 12)).foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }
        .padding(12)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .accessibilityIdentifier("roundtableEmpty")
    }

    private func tuneHere(_ room: RoundtableModel.Room) {
        slices.tune(toHz: room.hz, family: room.family)
        Task { await roundtable.backToSlice() }
    }

    // MARK: The box

    private var draft: String {
        roomId.map(roundtable.draft) ?? ""
    }

    /// Send is on while tuned here, live, not away, and the words fit.
    static func canSend(text: String, tunedHere: Bool, live: Bool, away: Bool, sending: Bool) -> Bool {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        return tunedHere && live && !away && !sending && !trimmed.isEmpty && fits(text)
    }

    /// The words fit: 500 characters (code points) and 10 lines.
    static func fits(_ text: String) -> Bool {
        text.unicodeScalars.count <= RoundtableText.messageLimit
            && text.split(separator: "\n", omittingEmptySubsequences: false).count <= RoundtableText.messageLines
    }

    /// The counter under the box: from 400 characters, or past 10 lines.
    static func counter(_ text: String) -> String? {
        let lines = text.split(separator: "\n", omittingEmptySubsequences: false).count
        if lines > RoundtableText.messageLines {
            return "\(lines) lines, up to \(RoundtableText.messageLines)"
        }
        let count = text.unicodeScalars.count
        return count >= RoundtableText.counterFrom ? "\(count) / \(RoundtableText.messageLimit)" : nil
    }
}

// MARK: Pieces the sheet and the iPad's applet share

extension RoundtableModel {
    /// The room on screen: the one being read, else the active slice's.
    var shownRoomId: String? { readingRoomId ?? sliceRoomId }

    /// A slice of this device is tuned to the room on screen: it may post and name.
    var tunedToShownRoom: Bool {
        guard let shownRoomId else {
            return false
        }
        return frequencyAvailable ? activeContext?.id == shownRoomId : sliceRooms.values.contains(shownRoomId)
    }

    /// A frequency is being viewed without tuning there (overlay design D21). A
    /// legacy server's read bar is the room's own.
    var isViewing: Bool { frequencyAvailable && readingRoomId != nil }

    /// The station has not answered Roundtable's first question yet.
    var askingFirstRun: Bool {
        available && linkState != .off && !firstRunAnswered
    }
}

/// The one inspector's head on a sheet or a panel: Back on a viewed frequency, or
/// Roundtable and your frequency on your own, then more and close (overlay.js renderInsSheet).
struct RoundtableInspectorBar: View {
    @ObservedObject var roundtable: RoundtableModel
    let viewing: Bool
    let closeId: String
    let onBack: () -> Void
    let onClose: () -> Void

    @State private var moreOpen = false

    static func signalTitle(_ format: RoundtableSignalFormat) -> String {
        "Signal: \(format.title) \u{00B7} Change"
    }

    var body: some View {
        HStack(alignment: .center, spacing: 8) {
            if viewing {
                RoundtableBackButton(chip: true, action: onBack)
            } else {
                // The word gives way on a narrow panel, so the frequency is never cut.
                ViewThatFits(in: .horizontal) {
                    HStack(alignment: .firstTextBaseline, spacing: 8) {
                        Text("Roundtable").font(.system(size: 14, weight: .bold)).foregroundStyle(RoundtableColours.bright)
                            .fixedSize()
                        ownHead.fixedSize()
                    }
                    ownHead
                }
                .layoutPriority(1)
            }
            Spacer(minLength: 4)
            HStack(spacing: 0) {
                icon("ellipsis") { moreOpen = true }
                    .accessibilityLabel("More")
                    .accessibilityIdentifier("roundtableMore")
                icon("xmark", action: onClose)
                    .accessibilityLabel(viewing ? "Close" : "Close Roundtable")
                    .accessibilityIdentifier(closeId)
            }
        }
        .padding(.leading, 12)
        .padding(.trailing, 4)
        .confirmationDialog(roundtable.room.map { RoundtableTagLayout.frequencyText($0.hz) + " MHz" } ?? "Roundtable",
                            isPresented: $moreOpen, titleVisibility: .visible) {
            Button(Self.signalTitle(roundtable.signalFormat)) { roundtable.cycleSignalFormat() }
            // The system's popover menu drops a cancel-role row, and the design shows Cancel (D36).
            if #available(iOS 26, *) {
                Button("Cancel") {}
            } else {
                Button("Cancel", role: .cancel) {}
            }
        }
    }

    /// Who the frequency's own list counts, as the view below does, before its stream names them.
    @ViewBuilder
    private var ownHead: some View {
        if let room = roundtable.room {
            RoundtableRoomHead(room: room, stations: roundtable.cardData(context: room.id).onFrequency.count)
        }
    }

    private func icon(_ name: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: name)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(RoundtableColours.text)
                .frame(width: 30, height: 30)
                .background(Color.white.opacity(0.08), in: Circle())
                .frame(width: 44, height: 44)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// The room's frequency, mode and station count (and AWAY when you are),
/// as the sheet's header and the applet's head show them.
struct RoundtableRoomHead: View {
    let room: RoundtableModel.Room
    /// Who the frequency's own list counts, before its stream names them.
    var stations: Int? = nil

    private var count: Int { stations ?? room.members.count }

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text(RoundtableText.headerFrequency(room.hz))
                .font(.system(size: 13, weight: .semibold).monospaced())
                .foregroundStyle(RoundtableColours.you)
            Text(RoundtableText.modeLabel(room.family))
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(RoundtableColours.you)
            Text("\(count) station\(count == 1 ? "" : "s")")
                .font(.system(size: 11)).foregroundStyle(RoundtableColours.dim)
            if room.members.first(where: \.you)?.away == true {
                Text("AWAY").font(.system(size: 11, weight: .bold)).foregroundStyle(RoundtableColours.dim)
            }
        }
        .lineLimit(1)
    }
}

/// The one line under a room's header that says whether Roundtable is
/// live (spec 11.1, U4).
struct RoundtableConnectionLine: View {
    @ObservedObject var roundtable: RoundtableModel
    let room: RoundtableModel.Room?
    /// Viewing another frequency, this slice stays put (overlay.js padHead).
    var viewingLetter: String? = nil

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { context in
            let now = Int64(context.date.timeIntervalSince1970 * 1000)
            let newest = room?.members.compactMap(\.reading?.t).max()
            let line = RoundtableText.connectionLine(link: roundtable.linkState, newestReadingMs: newest,
                                                     linkSince: roundtable.linkSince, now: now)
            let colour: Color = switch line.tone {
            case .live: RoundtableColours.talking
            case .warn: RoundtableColours.unknown
            case .you: RoundtableColours.you
            case .dim: RoundtableColours.dim
            }
            HStack(spacing: 6) {
                Circle().fill(colour).frame(width: 6, height: 6)
                Text(line.tone == .live ? viewingLetter.map { "Live \u{00B7} viewing \u{00B7} slice \($0) stays put" } ?? line.text
                     : line.text)
                    .font(.system(size: 11)).foregroundStyle(line.tone == .live ? RoundtableColours.dim
                                                             : colour)
                    .lineLimit(1)
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 12)
            .frame(height: 20)
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("roundtableConnection")
        }
        .overlay(alignment: .bottom) { Rectangle().fill(RoundtableColours.rule).frame(height: 1) }
    }
}

/// The message box pinned at a room's foot: the box and Send, then why
/// Send is off or why a post failed. `leading` sits before the box (the
/// phone sheet's hold PTT); `leadingWidth` indents the line under it to
/// match.
struct RoundtableComposer<Leading: View>: View {
    @ObservedObject var roundtable: RoundtableModel
    let leadingWidth: CGFloat
    let leading: Leading
    var compact = false

    @State private var sending = false
    @State private var failures: [String: String] = [:]
    @State private var pendingPosts: [String: (text: String, clientId: String)] = [:]
    private var failureKey: String { roomId.map(roundtable.draftKey) ?? "" }
    private var failed: String? {
        get { failures[failureKey] }
        nonmutating set { failures[failureKey] = newValue }
    }
    @FocusState private var boxFocused: Bool

    init(roundtable: RoundtableModel, leadingWidth: CGFloat = 0, compact: Bool = false, @ViewBuilder leading: () -> Leading) {
        self.roundtable = roundtable
        self.leadingWidth = leadingWidth
        self.compact = compact
        self.leading = leading()
    }

    private var room: RoundtableModel.Room? { roundtable.room }
    private var roomId: String? { roundtable.shownRoomId }
    private var tunedHere: Bool { roundtable.tunedToShownRoom }
    private var you: RoundtableModel.Member? { room?.members.first(where: \.you) }
    private var away: Bool { you?.away ?? false }
    private var live: Bool { roundtable.linkState == .live }
    private var hiddenHere: Bool { roundtable.hidden || roundtable.linkState == .hidden }

    private var draft: String {
        roomId.map(roundtable.draft) ?? ""
    }

    private var draftBinding: Binding<String> {
        Binding(get: { draft }, set: { text in
            if let roomId {
                roundtable.setDraft(text, for: roomId)
            }
            failed = nil
        })
    }

    /// The hint under the box: why Send is off, or what a post carries.
    private var hint: String? {
        if !roundtable.firstRunAnswered { return "Choose Stay hidden or Go live before posting." }
        if roundtable.hidden { return "Hidden stations can’t post." }
        if !tunedHere {
            return "Tune a slice here to post. You can read everything."
        }
        if away {
            return "Your station is away, so it can't post or name turns."
        }
        if !live && !hiddenHere {
            return "Send is off until Roundtable is live. Your draft stays here."
        }
        return nil
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(alignment: .center, spacing: 0) {
                if !boxFocused { leading }
                if hiddenHere {
                    Text("Hidden stations can't post. Go live to join in.")
                        .font(.system(size: 12))
                        .foregroundStyle(RoundtableColours.dim)
                        .padding(.horizontal, 12)
                        .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                        .accessibilityIdentifier("roundtableHiddenBox")
                } else {
                    box
                }
            }
            if let failed {
                HStack(spacing: 6) {
                    Text("Not sent \u{00B7}").font(.system(size: 11, weight: .bold))
                        .foregroundStyle(RoundtableColours.unknown)
                    Text(failed).font(.system(size: 11)).foregroundStyle(RoundtableColours.dim).lineLimit(2)
                    Button("Try again") { send() }
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(RoundtableColours.accentText)
                        .accessibilityIdentifier("roundtableTryAgain")
                }
                .padding(.leading, leadingWidth)
            } else if let hint, !hiddenHere {
                Text(hint).font(.system(size: 11)).foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, leadingWidth)
                    .accessibilityIdentifier("roundtableHint")
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, compact ? 4 : 8)
        .background(RoundtableColours.ground)
        .overlay(alignment: .top) { Rectangle().fill(RoundtableColours.rule).frame(height: 1) }
    }

    private var box: some View {
        let text = draft
        let placeholder = tunedHere ? "Message \(RoundtableText.headerFrequency(room?.hz ?? 0))"
            : "Tune a slice to \(RoundtableText.headerFrequency(room?.hz ?? 0)) to post here"
        return HStack(alignment: .bottom, spacing: 6) {
            VStack(alignment: .trailing, spacing: 0) {
                TextField("", text: draftBinding, prompt: Text(placeholder).foregroundColor(RoundtableColours.dim2),
                          axis: .vertical)
                    .font(.system(size: 13))
                    .foregroundStyle(RoundtableColours.text)
                    .lineLimit(1...4)
                    .focused($boxFocused)
                    .disabled(!tunedHere || away)
                    .accessibilityIdentifier("roundtableBox")
                if let counter = RoundtableSheet.counter(text) {
                    Text(counter).font(.system(size: 11).monospacedDigit())
                        .foregroundStyle(RoundtableSheet.fits(text) ? RoundtableColours.dim : RoundtableColours.unknown)
                }
            }
            Button("Send") { send() }
                .buttonStyle(RoundtableButtonStyle(main: true))
                .disabled(!RoundtableSheet.canSend(text: text, tunedHere: tunedHere, live: live, away: away,
                                                   sending: sending))
                .accessibilityIdentifier("roundtableSend")
        }
        .padding(.leading, 14)
        .padding(.trailing, 6)
        .padding(.vertical, 6)
        .frame(minHeight: 44)
        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(RoundtableColours.fieldEdge, lineWidth: 1))
    }

    private func send() {
        guard let roomId else {
            return
        }
        let text = draft
        guard RoundtableSheet.canSend(text: text, tunedHere: tunedHere, live: live, away: away, sending: sending) else {
            return
        }
        sending = true
        failed = nil
        let draftKey = roundtable.draftKey(roomId)
        let originalFailureKey = failureKey
        let context = roundtable.activeContext
        let pending = pendingPosts[draftKey]
        let clientId = pending?.text == text ? pending!.clientId : UUID().uuidString
        pendingPosts[draftKey] = (text, clientId)
        Task {
            let result = await roundtable.post(text, in: roomId, clientId: clientId, context: context)
            sending = false
            switch result {
            case .posted:
                if pendingPosts[draftKey]?.clientId == clientId { pendingPosts[draftKey] = nil }
                // The draft goes only once the Core's Roundtable took it.
                if roundtable.drafts[draftKey] == text {
                    roundtable.drafts[draftKey] = nil
                }
            case .refused(let reason):
                failures[originalFailureKey] = reason
            }
        }
    }
}

/// Where the first-run question is asked elsewhere (the iPad's form
/// sheet): one line in the room's place that says so and opens it.
/// Nothing is sent before the answer.
struct RoundtableFirstRunPrompt: View {
    let ask: () -> Void

    static let text = "Roundtable is public. Choose how your station shows before you join in."
    static let button = "Choose"

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(Self.text)
                .font(.system(size: 12))
                .foregroundStyle(RoundtableColours.text)
                .fixedSize(horizontal: false, vertical: true)
            Button(Self.button, action: ask)
                .buttonStyle(RoundtableButtonStyle(main: true))
                .accessibilityIdentifier("roundtableFirstRunChoose")
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFirstRunPrompt")
    }
}
