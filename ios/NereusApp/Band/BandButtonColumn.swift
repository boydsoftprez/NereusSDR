// NereusSDR for iOS: the desktop's button column at the band's top-left, on the iPad and the phone held sideways, and the menus it opens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// What the band column has open beside it (R-IOS-58, spec DC7).
enum BandColumnMenu: Hashable {
    case band
    case antenna
    case display
    case vax
    case attenuator
}

/// The frames the band column puts on the band, in ``BandColumnLayout/space``:
/// the column itself and, while one is open, its pill or menu. `MainScreen`
/// reads them to keep the PTT and the keyed meters clear (spec DC8).
struct BandColumnFrameKey: PreferenceKey {
    static let defaultValue: [CGRect] = []

    static func reduce(value: inout [CGRect], nextValue: () -> [CGRect]) {
        value.append(contentsOf: nextValue())
    }
}

/// Where the column, its menus, the PTT and the keyed meters go.
@MainActor
enum BandColumnLayout {
    /// The band's coordinate space, which the column's frames are in.
    static let space = "bandColumnSpace"
    /// The desktop's 68 by 22 buttons drawn 80 by 30 for a finger, as the
    /// board draws them (spec section 12 item 10).
    // From src/gui/SpectrumOverlayPanel.cpp:250-251 (kBtnW 68, kBtnH 22), drawn larger for a finger
    static let buttonWidth: CGFloat = 80
    static let buttonHeight: CGFloat = 30
    // From src/gui/SpectrumOverlayPanel.cpp:254-255 (kPad 2, kGap 2)
    static let padding: CGFloat = 2
    static let gap: CGFloat = 2
    /// The column's place from the band's top-left, as the desktop puts its strip at (4, 4).
    static let origin: CGFloat = 4
    /// The column's width, its padding included.
    static let columnWidth = buttonWidth + 2 * padding
    /// The column's buttons: ◀, +RX, +TNF, BAND, ANT, Display, VAX and ATT.
    static let buttonCount = 8

    /// The column's height with `rows` buttons down it, its padding included.
    static func columnHeight(rows: Int) -> CGFloat {
        CGFloat(rows) * buttonHeight + CGFloat(max(rows - 1, 0)) * gap + 2 * padding
    }

    /// The column's width with `columns` columns of buttons side by side.
    static func columnWidth(columns: Int) -> CGFloat {
        CGFloat(columns) * buttonWidth + CGFloat(max(columns - 1, 0)) * gap + 2 * padding
    }

    /// How many columns the buttons take on a band `bandHeight` tall: one
    /// while all eight fit down the band, else two columns of four side by
    /// side, all in view (spec DC14, JJ 2026-10-24).
    static func columns(bandHeight: CGFloat) -> Int {
        columnHeight(rows: buttonCount) <= menuMaxHeight(bandHeight: bandHeight) ? 1 : 2
    }
    /// Between the column and a menu beside it.
    static let menuGap: CGFloat = 2
    /// The space the PTT and the keyed meters keep from what is open.
    static let clearance: CGFloat = 8
    /// The menus' widths: the band grid four across (as the board's
    /// `.dp-bandgrid`), the flag's antenna menu, the VAX channels, the step
    /// attenuator, and the pill (the board's `.dp-pill`, 340).
    static let bandMenuWidth: CGFloat = 236
    static let antennaMenuWidth = FlagPopoverLayer.antennaMenuWidth
    static let vaxMenuWidth: CGFloat = 270
    static let attenuatorMenuWidth: CGFloat = 290
    static let pillWidth: CGFloat = 340

    static func width(of menu: BandColumnMenu) -> CGFloat {
        switch menu {
        case .band: bandMenuWidth
        case .antenna: antennaMenuWidth
        case .display: pillWidth
        case .vax: vaxMenuWidth
        case .attenuator: attenuatorMenuWidth
        }
    }

    /// The tallest a menu or the pill may be: down to the band's foot, less
    /// the column's top margin, so it never runs past the band and scrolls
    /// instead (spec DC7, DC8).
    static func menuMaxHeight(bandHeight: CGFloat) -> CGFloat {
        max(60, bandHeight - 2 * origin)
    }

    /// Where the pan at `index` of `count` starts down a band `bandHeight`
    /// tall, as ``PanStack`` lays them: equal shares with a 1-point hairline
    /// between each two.
    static func panTop(index: Int, count: Int, bandHeight: CGFloat) -> CGFloat {
        guard count > 1, index > 0 else {
            return 0
        }
        let share = (bandHeight - CGFloat(count - 1)) / CGFloat(count)
        return CGFloat(index) * (share + 1)
    }

    /// The PTT's frame: at `home`, or just right of whatever of the column's
    /// frames reaches down to its row (the board's `shiftSideways`); then
    /// right of any full flag it would sit on, so it never covers a flag's
    /// words (`flags`, the band's ``BandSlicesModel/flagRects``). While
    /// `keyed` it keeps off the column alone and leaves the flags be, so the
    /// keyed meters above it keep their room (spec DC15, JJ 2026-10-25).
    /// Never past `maxX`, the band's right edge less its insets: with more
    /// flags than fit it stops there, on the last.
    static func pttFrame(home: CGRect, avoid: [CGRect], flags: [CGRect] = [], keyed: Bool = false,
                         maxX: CGFloat = .infinity) -> CGRect {
        var frame = home
        let reaching = avoid.filter { $0.maxY > home.minY && $0.minY < home.maxY }
        if let edge = reaching.map(\.maxX).max(), edge + clearance > home.minX {
            frame.origin.x = edge + clearance
        }
        guard !keyed else {
            return frame
        }
        let beside = frame.minX
        let inRow = flags.filter { $0.maxY > home.minY && $0.minY < home.maxY }
        // Each step moves right past one flag, so this ends.
        for _ in 0...inRow.count {
            guard let edge = inRow.filter({ $0.minX < frame.maxX + clearance && $0.maxX + clearance > frame.minX })
                .map(\.maxX).max() else {
                break
            }
            frame.origin.x = edge + clearance
        }
        frame.origin.x = max(beside, min(frame.minX, maxX - frame.width))
        return frame
    }

    /// The column's note from the foot of the band: clear of the zoom buttons.
    static let noteBottomInset = ZoomButtons.inset + ZoomButtons.buttonSize.height + clearance
    /// The column's note from the band's right edge.
    static let noteTrailingInset: CGFloat = 12
    /// The narrowest the note goes in a gap before it takes the widest one.
    static let noteMinWidth: CGFloat = 160

    /// Where the column's note goes (the board's `.dp-colnote`): at the
    /// band's lower right, above the zoom buttons, no wider than `maxWidth`,
    /// in the rightmost gap between what it keeps clear of (`avoid`: the
    /// column, its open menu or pill, the flags and the PTT) that is wide
    /// enough, else in the widest.
    static func notePlace(bandSize: CGSize, trailingInset: CGFloat, height: CGFloat, maxWidth: CGFloat,
                          avoid: [CGRect]) -> CGRect {
        let bottom = bandSize.height - noteBottomInset
        let top = bottom - height
        let right = bandSize.width - trailingInset - noteTrailingInset
        var gaps: [(start: CGFloat, end: CGFloat)] = []
        var cursor: CGFloat = 0
        for blocker in avoid.filter({ $0.minY < bottom && $0.maxY > top }).sorted(by: { $0.minX < $1.minX }) {
            if blocker.minX - clearance > cursor {
                gaps.append((cursor, min(blocker.minX - clearance, right)))
            }
            cursor = max(cursor, blocker.maxX + clearance)
        }
        if right > cursor {
            gaps.append((cursor, right))
        }
        gaps = gaps.filter { $0.end > $0.start }
        let gap = gaps.last { $0.end - $0.start >= noteMinWidth }
            ?? gaps.max { $0.end - $0.start < $1.end - $1.start }
            ?? (right - noteMinWidth, right)
        let width = min(maxWidth, gap.end - gap.start)
        return CGRect(x: gap.end - width, y: top, width: width, height: height)
    }

    /// Where the keyed meters go.
    struct KeyedPlace: Equatable {
        let top: CGFloat
        /// The meters' region starts this far in from the band's left.
        let leading: CGFloat
        /// The meters sit at the region's left edge (above the PTT) rather than centred in it.
        let alignLeading: Bool
        /// The region ends this far in from the band's right: beside a PTT
        /// held in their rows (``PttLatch``).
        var trailing: CGFloat = 0
    }

    /// The keyed meters' place: as today while nothing of the column's
    /// reaches their height; centred right of it while they fit there;
    /// above the moved PTT, at its left, when they don't (the board's meters
    /// "beside it or above it"). With the PTT `held` at a moved spot
    /// (``PttLatch``) in their rows: beside it, right or else left, at full
    /// width while they fit there; else on the wider side, narrowed to fit
    /// down to `narrowest` (the manager's ruling, 2026-10-25, for JJ's veto),
    /// inside `leadingInset` and `trailingInset`.
    static func keyedPlace(top: CGFloat, height: CGFloat, bandWidth: CGFloat, needed: CGFloat,
                           avoid: [CGRect], ptt: CGRect?, held: Bool = false, narrowest: CGFloat = .infinity,
                           leadingInset: CGFloat = 0, trailingInset: CGFloat = 0) -> KeyedPlace {
        let reaching = avoid.filter { $0.minY < top + height && $0.maxY > top }
        guard let edge = reaching.map(\.maxX).max() else {
            return KeyedPlace(top: top, leading: 0, alignLeading: false)
        }
        if held, let ptt, reaching.contains(ptt) {
            let others = reaching.filter { $0 != ptt }
            // Where the meters' gauges may start and end on each side of the PTT.
            let before = others.filter { $0.midX < ptt.midX }.map(\.maxX).max() ?? 0
            let after = others.filter { $0.midX > ptt.midX }.map(\.minX).min() ?? .infinity
            let left = (start: max(leadingInset, before + clearance), end: ptt.minX - clearance)
            let right = (start: ptt.maxX + clearance, end: min(bandWidth - trailingInset, after - clearance))
            func place(_ side: (start: CGFloat, end: CGFloat)) -> KeyedPlace {
                KeyedPlace(top: top, leading: side.start - 10, alignLeading: false,
                           trailing: bandWidth - side.end - 10)
            }
            if let full = [right, left].first(where: { $0.end - $0.start >= needed }) {
                return place(full)
            }
            let wider = right.end - right.start >= left.end - left.start ? right : left
            if wider.end - wider.start >= narrowest {
                return place(wider)
            }
        }
        // The meters' own 10 point side margins sit inside the region.
        let leading = edge + clearance - 10
        if bandWidth - leading >= needed + 20 {
            return KeyedPlace(top: top, leading: leading, alignLeading: false)
        }
        if let ptt {
            return KeyedPlace(top: max(0, ptt.minY - 6 - height), leading: max(0, ptt.minX - 10), alignLeading: true)
        }
        return KeyedPlace(top: top, leading: leading, alignLeading: false)
    }
}

/// Where the PTT was when this phone's transmitting began, held for the
/// whole key (spec DC15 as amended after the PR 653 review: the manager's
/// ruling, 2026-10-25, for JJ's veto): the tap that unkeys lands where
/// the tap that keyed did, whatever the flags or the column do meanwhile.
/// Receiving it follows the PTT's usual place; keyed it keeps that place
/// while it still stands on the band: a rotation or a new band size moves
/// the PTT's home or narrows the band past it, and then the PTT takes its
/// keyed place (DC15's, beside the column, over any flag) and holds that.
struct PttLatch: Equatable {
    /// Where the PTT is held.
    let frame: CGRect
    /// The PTT's home when it was held.
    let home: CGRect

    /// What the latch hangs on, for a view's `onChange`.
    struct Inputs: Equatable {
        let keyed: Bool
        let home: CGRect
        let maxX: CGFloat
        /// The PTT's place while receiving (``BandColumnLayout/pttFrame(home:avoid:flags:keyed:maxX:)``).
        let receive: CGRect
        /// Its place keyed with nothing held: the same call with `keyed` (DC15's place).
        let keyedRule: CGRect
    }

    /// The latch holds for this layout: the same home, still on the band.
    func holds(home: CGRect, maxX: CGFloat) -> Bool {
        self.home == home && frame.maxX <= maxX
    }

    /// Where the PTT is drawn.
    static func shown(_ latch: PttLatch?, _ inputs: Inputs) -> CGRect {
        guard inputs.keyed else {
            return inputs.receive
        }
        if let latch, latch.holds(home: inputs.home, maxX: inputs.maxX) {
            return latch.frame
        }
        return inputs.keyedRule
    }

    /// The latch after a layout: receiving, the place the PTT is drawn at;
    /// keyed, what it held while that holds, else the keyed place.
    static func next(_ latch: PttLatch?, _ inputs: Inputs) -> PttLatch {
        PttLatch(frame: shown(latch, inputs), home: inputs.home)
    }
}

/// The PTT's ``PttLatch``, shared by the views that place the PTT, the keyed
/// meters and the column's note. A reference the screen holds without
/// watching, so a flag's move redraws only the views that follow it.
@MainActor
final class PttHold: ObservableObject {
    @Published private(set) var latch: PttLatch?

    func update(_ inputs: PttLatch.Inputs) {
        let next = PttLatch.next(latch, inputs)
        if next != latch {
            latch = next
        }
    }
}

/// The desktop's button column (SpectrumOverlayPanel's strip) at the band's
/// top-left inside the safe area, on the iPad and the phone held sideways
/// (R-IOS-58, spec DC7, DC8, 6.2): ◀, +RX, +TNF, BAND, ANT, Display, VAX
/// and ATT, each 30 points tall at every text size, the open one in the
/// glass. ◀ folds it to one button and back. BAND, ANT, VAX and ATT open
/// the menus the phone already has beside the column, Display the pill; one
/// is open at a time and a tap anywhere else on the band closes it. A
/// button that cannot act stays, greyed, with its reason beside it.
///
/// On a band too short for all eight down it (the phone sideways) the
/// buttons stand in two columns of four side by side, read down the first
/// and then the second, all in view (spec DC14). What the column did, and
/// what the last Add a pan or Close this pan did, shows for a moment at the
/// band's lower right, clear of the column, its menus, the flags and the
/// PTT (the board's `.dp-colnote`).
///
/// The view fills the band; only the column, the open menu and the layer
/// that closes it take touches.
struct BandButtonColumn: View {
    let pan: PanIdentity
    @Binding var open: BandColumnMenu?
    @Binding var folded: Bool
    let main: MainScreenModel
    @ObservedObject var sheet: PanSheetModel
    @ObservedObject var slices: BandSlicesModel
    /// The pans, for the status line on the pan last touched's band.
    @ObservedObject var stack: PanStackModel
    /// This pan's flags' menus (ANT, VAX), its pill's model and its band.
    let controls: PanControls
    /// The pill's open tab, kept while it closes and opens.
    @Binding var displayTab: DisplayPillTab
    /// The phone's rounded edges sideways; 0 on the iPad.
    var leadingInset: CGFloat = 0
    var trailingInset: CGFloat = 0
    let openSetup: () -> Void
    let onSetupDefaults: (DisplayPillTab, PanIdentity) -> Void
    let onOpenPage: (String) -> Void
    let onOpen: (PanIdentity) -> Void
    /// Where the PTT is held while keyed (``PttLatch``), for the note to keep clear of.
    var pttHold: PttHold?

    /// The Core's words for the column's last refused request, shown for a moment.
    @State private var shownNote: String?
    @State private var noteGeneration = 0
    /// The pans' status line while it shows.
    @State private var shownStatus: String?
    /// The note's height as drawn, for its place.
    @State private var noteHeight: CGFloat = 30

    /// How long a note stays: the board's 3.2 s (its `say`). A shot test
    /// may hold it longer to draw it.
    static var noteDuration: Duration = .milliseconds(3200)
    /// The note's widest: the board's 360 sideways, 420 on the iPad.
    static let noteMaxWidthSideways: CGFloat = 360
    static let noteMaxWidth: CGFloat = 420
    /// The desktop's ◀ and ▶.
    // From src/gui/SpectrumOverlayPanel.cpp:815-816 ("◀" expanded, "▶" folded)
    static let foldText = "\u{25C0}"
    static let unfoldText = "\u{25B6}"

    init(pan: PanIdentity, open: Binding<BandColumnMenu?>, folded: Binding<Bool>, main: MainScreenModel,
         displayTab: Binding<DisplayPillTab>, leadingInset: CGFloat = 0, trailingInset: CGFloat = 0,
         openSetup: @escaping () -> Void,
         onSetupDefaults: @escaping (DisplayPillTab, PanIdentity) -> Void,
         onOpenPage: @escaping (String) -> Void, onOpen: @escaping (PanIdentity) -> Void,
         pttHold: PttHold? = nil) {
        self.pan = pan
        _open = open
        _folded = folded
        self.main = main
        let controls = main.controls(for: pan)
        self.controls = controls
        sheet = controls.sheet
        slices = controls.band.slices
        stack = main.panStack
        _displayTab = displayTab
        self.leadingInset = leadingInset
        self.trailingInset = trailingInset
        self.openSetup = openSetup
        self.onSetupDefaults = onSetupDefaults
        self.onOpenPage = onOpenPage
        self.onOpen = onOpen
        self.pttHold = pttHold
    }

    var body: some View {
        GeometryReader { proxy in
            let x = leadingInset + BandColumnLayout.origin
            let y = BandColumnLayout.origin
            let maxHeight = BandColumnLayout.menuMaxHeight(bandHeight: proxy.size.height)
            let columns = folded ? 1 : BandColumnLayout.columns(bandHeight: proxy.size.height)
            let columnWidth = BandColumnLayout.columnWidth(columns: columns)
            ZStack(alignment: .topLeading) {
                if open != nil {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { open = nil }
                        .accessibilityLabel("Close the menu")
                        .accessibilityAddTraits(.isButton)
                        .accessibilityIdentifier("bandColumnClose")
                }
                // Two columns of four fit every band the phone draws; on a
                // band too short even for those they scroll, the open
                // button kept in view.
                ViewThatFits(in: .vertical) {
                    column(columns: columns)
                    ScrollViewReader { reader in
                        ScrollView(.vertical, showsIndicators: false) { column(columns: columns) }
                            .frame(width: columnWidth)
                            .onAppear { reveal(open, reader) }
                            .onChange(of: open) { _, menu in reveal(menu, reader) }
                    }
                }
                .frame(maxHeight: maxHeight, alignment: .top)
                .reportsBandColumnFrame()
                .padding(.leading, x)
                .padding(.top, y)
                if let open, !folded {
                    menu(open)
                        .frame(maxHeight: maxHeight, alignment: .top)
                        .padding(.leading, x + columnWidth + BandColumnLayout.menuGap)
                        .padding(.top, y)
                        .transition(.opacity)
                }
                note(bandSize: proxy.size, x: x, y: y, columnWidth: columnWidth, columns: columns,
                     maxHeight: maxHeight)
                if open == .attenuator {
                    ColumnPadLayer(modes: main.modes)
                }
            }
            .frame(width: proxy.size.width, height: proxy.size.height, alignment: .topLeading)
        }
        .onChange(of: folded) { _, isFolded in
            if isFolded {
                open = nil
            }
        }
        .onChange(of: sheet.note) { _, note in
            show(note)
        }
        // The pans' status line, on the pan last touched, while it is new.
        .task(id: StatusShown(serial: stack.statusSerial, here: stack.lastTouched == pan)) {
            await showStatus()
        }
    }

    /// What the status line's showing follows.
    private struct StatusShown: Equatable {
        let serial: Int
        let here: Bool
    }

    // MARK: The note

    /// The status line and the column's refusal at the band's lower right.
    @ViewBuilder
    private func note(bandSize: CGSize, x: CGFloat, y: CGFloat, columnWidth: CGFloat, columns: Int,
                      maxHeight: CGFloat) -> some View {
        if shownStatus != nil || shownNote != nil {
            let place = notePlace(bandSize: bandSize, x: x, y: y, columnWidth: columnWidth, columns: columns,
                                  maxHeight: maxHeight)
            VStack(alignment: .trailing, spacing: 4) {
                if let shownStatus {
                    // What the last Add a pan or Close this pan did (spec section 6.3).
                    noteText(shownStatus, colour: ChromeColours.text)
                        .accessibilityIdentifier("bandColumnPanStatus")
                }
                if let shownNote {
                    noteText(shownNote, colour: ChromeColours.refusalText)
                        .accessibilityIdentifier("bandColumnNote")
                }
            }
            .frame(width: place.width, alignment: .trailing)
            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { noteHeight = $0 }
            .offset(x: place.minX, y: place.minY)
            .allowsHitTesting(false)
            .transition(.opacity)
        }
    }

    private func noteText(_ text: String, colour: Color) -> some View {
        Text(text)
            .font(.system(size: 12))
            .foregroundStyle(colour)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .workspacePanel()
    }

    /// The note's place, clear of the column, its open menu or pill, the
    /// flags and where the PTT goes.
    private func notePlace(bandSize: CGSize, x: CGFloat, y: CGFloat, columnWidth: CGFloat, columns: Int,
                           maxHeight: CGFloat) -> CGRect {
        let rows = folded ? 1 : BandColumnLayout.buttonCount / columns
        let column = CGRect(x: x, y: y, width: columnWidth,
                            height: min(BandColumnLayout.columnHeight(rows: rows), maxHeight))
        var avoid = [column]
        if let open, !folded {
            // The menu's full height: what it draws is never taller.
            avoid.append(CGRect(x: column.maxX + BandColumnLayout.menuGap, y: y,
                                width: BandColumnLayout.width(of: open), height: maxHeight))
        }
        let flags = slices.flagRects
        let home = PttButton.frame(bandSize: bandSize, leadingInset: leadingInset)
        let keyed = main.transmit.ptt.transmitting
        let maxX = bandSize.width - trailingInset - PttButton.inset
        // Keyed, the PTT stays where it was when transmitting began (PttLatch).
        let ptt = PttLatch.shown(pttHold?.latch, PttLatch.Inputs(
            keyed: keyed, home: home, maxX: maxX,
            receive: BandColumnLayout.pttFrame(home: home, avoid: avoid, flags: flags, maxX: maxX),
            keyedRule: BandColumnLayout.pttFrame(home: home, avoid: avoid, flags: flags, keyed: true, maxX: maxX)))
        return BandColumnLayout.notePlace(bandSize: bandSize, trailingInset: trailingInset, height: noteHeight,
                                          maxWidth: leadingInset > 0 ? Self.noteMaxWidthSideways
                                              : Self.noteMaxWidth,
                                          avoid: avoid + flags + [ptt])
    }

    private func show(_ note: String?) {
        noteGeneration &+= 1
        let generation = noteGeneration
        shownNote = note
        guard note != nil else {
            return
        }
        Task { @MainActor in
            try? await Task.sleep(for: Self.noteDuration)
            if generation == noteGeneration {
                shownNote = nil
            }
        }
    }

    /// Shows the pans' status line for what is left of its moment, on the
    /// pan last touched: a pan's band drawn just after (Pan 1's alone
    /// again after Close this pan) still shows it.
    private func showStatus() async {
        guard stack.lastTouched == pan, let status = stack.status, let since = stack.statusSince else {
            shownStatus = nil
            return
        }
        let left = Self.noteDuration - (ContinuousClock.now - since)
        guard left > .zero else {
            shownStatus = nil
            return
        }
        shownStatus = status
        try? await Task.sleep(for: left)
        if !Task.isCancelled {
            shownStatus = nil
        }
    }

    // MARK: The column

    /// The buttons in their order: down one column, or down the first of
    /// two and then the second (spec DC14).
    @ViewBuilder
    private func column(columns: Int) -> some View {
        Group {
            if folded {
                foldButton
            } else if columns > 1 {
                HStack(alignment: .top, spacing: BandColumnLayout.gap) {
                    VStack(alignment: .leading, spacing: BandColumnLayout.gap) {
                        foldButton
                        rxButton(columns: columns)
                        tnfButton(columns: columns)
                        // From src/gui/SpectrumOverlayPanel.cpp:500-535 ("BAND", "ANT", "Display", "VAX", "ATT"), in its order
                        menuButton("BAND", .band, id: "band")
                    }
                    VStack(alignment: .leading, spacing: BandColumnLayout.gap) {
                        menuButton("ANT", .antenna, id: "antenna")
                        menuButton("Display", .display, id: "display")
                        menuButton("VAX", .vax, id: "vax")
                        menuButton("ATT", .attenuator, id: "attenuator")
                    }
                }
            } else {
                VStack(alignment: .leading, spacing: BandColumnLayout.gap) {
                    foldButton
                    rxButton(columns: columns)
                    tnfButton(columns: columns)
                    // From src/gui/SpectrumOverlayPanel.cpp:500-535 ("BAND", "ANT", "Display", "VAX", "ATT"), in its order
                    menuButton("BAND", .band, id: "band")
                    menuButton("ANT", .antenna, id: "antenna")
                    menuButton("Display", .display, id: "display")
                    menuButton("VAX", .vax, id: "vax")
                    menuButton("ATT", .attenuator, id: "attenuator")
                }
            }
        }
        .padding(BandColumnLayout.padding)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(pan.title)
        .accessibilityIdentifier("bandColumn")
    }

    private var foldButton: some View {
        columnButton(folded ? Self.unfoldText : Self.foldText, id: "fold", lit: false) {
            folded.toggle()
        }
        .accessibilityLabel(folded ? "Show the buttons" : "Fold the buttons away")
    }

    private func rxButton(columns: Int) -> some View {
        let rxReason = sheet.addSliceOnPanReason(radio: main.radioName)
        return withReason(rxReason, columns: columns) {
            // From src/gui/SpectrumOverlayPanel.cpp:474 ("+RX")
            columnButton("+RX", id: "rx", lit: false, disabled: rxReason != nil) {
                sheet.addSlice(on: pan)
            }
            .accessibilityLabel("Add a slice on this pan")
            .accessibilityHint(rxReason ?? "")
        }
    }

    private func tnfButton(columns: Int) -> some View {
        let tnfReason = sheet.canAddNotch ? nil : sheet.notchUnavailableReason
        return withReason(tnfReason, columns: columns) {
            // From src/gui/SpectrumOverlayPanel.cpp:489 ("+TNF")
            columnButton("+TNF", id: "tnf", lit: false, disabled: tnfReason != nil) {
                sheet.addNotch()
            }
            .accessibilityLabel("Add a notch in the active slice's passband")
            .accessibilityHint(tnfReason ?? "")
        }
    }

    /// Scrolls the open menu's button into view, where the column scrolls.
    private func reveal(_ menu: BandColumnMenu?, _ reader: ScrollViewProxy) {
        guard let menu else {
            return
        }
        reader.scrollTo(Self.buttonId(menu))
    }

    static func buttonId(_ menu: BandColumnMenu) -> String {
        switch menu {
        case .band: "band"
        case .antenna: "antenna"
        case .display: "display"
        case .vax: "vax"
        case .attenuator: "attenuator"
        }
    }

    private func menuButton(_ label: String, _ menu: BandColumnMenu, id: String) -> some View {
        columnButton(label, id: id, lit: open == menu) {
            open = open == menu ? nil : menu
        }
        .accessibilityValue(open == menu ? "Open" : "")
        .id(id)
    }

    /// One of the column's buttons: raised, or in the glass while its menu
    /// is open; 30 points tall at every text size.
    private func columnButton(_ label: String, id: String, lit: Bool, disabled: Bool = false,
                              action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: 12, weight: .bold))
                .lineLimit(1)
                .minimumScaleFactor(0.8)
                .frame(width: BandColumnLayout.buttonWidth, height: BandColumnLayout.buttonHeight)
        }
        .buttonStyle(WorkspaceButtonStyle(lit ? .glass : .raised))
        .disabled(disabled)
        .accessibilityAddTraits(lit ? .isSelected : [])
        .accessibilityIdentifier("bandColumn.\(id)")
    }

    /// A greyed button with its reason beside the whole column (past the
    /// second column when there are two), outside the column's frame so
    /// the PTT keeps to the column.
    private func withReason(_ reason: String?, columns: Int, @ViewBuilder button: () -> some View) -> some View {
        button()
            .overlay(alignment: .leading) {
                if let reason {
                    Text(reason)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(2)
                        .minimumScaleFactor(0.8)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 3)
                        .workspacePanel(radius: WorkspacePalette.chipRadius)
                        .frame(width: Self.reasonWidth, alignment: .leading)
                        .offset(x: BandColumnLayout.columnWidth(columns: columns) - 2 * BandColumnLayout.padding + 6)
                        .allowsHitTesting(false)
                        .accessibilityHidden(true)
                        .accessibilityIdentifier("bandColumnReason")
                }
            }
    }

    /// The widest a greyed button's reason beside it runs.
    static let reasonWidth: CGFloat = 220

    // MARK: The menus

    /// The open menu, as wide as its kind, its frame on ``BandColumnFrameKey``.
    /// The caller caps its height at the band's; inside that it scrolls.
    @ViewBuilder
    private func menu(_ menu: BandColumnMenu) -> some View {
        let width = BandColumnLayout.width(of: menu)
        switch menu {
        case .display:
            displayPill
                .frame(width: width)
                .reportsBandColumnFrame()
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier("bandColumnMenu.display")
        case .band:
            ColumnMenuShell(identifier: "bandColumnMenu.band", padding: 8) {
                BandSelectGrid(model: sheet) { open = nil }
            }
            .frame(width: width)
            .reportsBandColumnFrame()
        case .antenna:
            ColumnMenuShell(identifier: "bandColumnMenu.antenna", padding: 6) {
                if let entry = slices.active, let modes = controls.flagControls.modes {
                    FlagAntennaMenu(controls: controls.flagControls, modes: modes, entry: entry, large: false) {
                        open = nil
                    }
                } else {
                    SheetNote(PanSheetModel.noSliceReason).padding(6)
                }
            }
            .frame(width: width)
            .reportsBandColumnFrame()
        case .vax:
            ColumnMenuShell(identifier: "bandColumnMenu.vax", padding: 0) {
                if let entry = slices.active {
                    FlagVaxSection(controls: controls.flagControls, entry: entry)
                } else {
                    ModesChrome.section("VAX") {
                        ModesChrome.note(PanSheetModel.noSliceReason)
                    }
                }
            }
            .frame(width: width)
            .reportsBandColumnFrame()
        case .attenuator:
            ColumnMenuShell(identifier: "bandColumnMenu.attenuator", padding: 0) {
                // From src/gui/SpectrumOverlayPanel.cpp:4752 ("Step attenuator")
                ModesChrome.section("Step attenuator") {
                    FrontEndAttenuatorWays(model: main.modes)
                    FrontEndStepAttenuator(model: main.modes)
                }
            }
            .frame(width: width)
            .reportsBandColumnFrame()
        }
    }

    /// The pill from a Core that describes the version 29 Display pages,
    /// today's sheet with its note from an older one, as the toolbar's
    /// Display opens it (spec section 9).
    @ViewBuilder
    private var displayPill: some View {
        if let source = main.displayPillSource {
            DisplayDropSheet(pages: source.pages, source: source, main: main, tab: $displayTab,
                             device: DisplayDropSheet.deviceWord, openSetup: openSetup,
                             onSetupDefaults: onSetupDefaults, onOpenPage: onOpenPage, onOpen: onOpen, pan: pan)
        } else {
            DisplaySheet(model: controls.display, core: main.coreDisplay, openSetup: openSetup)
        }
    }
}

/// A menu beside the column: the workspace panel, scrolling only when the
/// band is too short for it.
private struct ColumnMenuShell<Content: View>: View {
    let identifier: String
    let padding: CGFloat
    @ViewBuilder let content: () -> Content

    var body: some View {
        ViewThatFits(in: .vertical) {
            inner
            ScrollView { inner }
        }
        .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius, style: .continuous))
        .workspacePanel()
        .shadow(color: .black.opacity(0.55), radius: 14, y: 10)
        .contentShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier(identifier)
    }

    private var inner: some View {
        VStack(alignment: .leading, spacing: 0) {
            content()
        }
        .padding(padding)
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// The ATT menu's number pad, over the band.
private struct ColumnPadLayer: View {
    @ObservedObject var modes: ModesTabModel

    var body: some View {
        ValuePadLayer(pad: modes.pad)
    }
}

private extension View {
    /// Puts this view's frame in the band's space on ``BandColumnFrameKey``.
    func reportsBandColumnFrame() -> some View {
        background {
            GeometryReader { proxy in
                Color.clear.preference(key: BandColumnFrameKey.self,
                                       value: [proxy.frame(in: .named(BandColumnLayout.space))])
            }
        }
    }
}
