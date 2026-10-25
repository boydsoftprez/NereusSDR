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

    /// The PTT's frame: at `home`, or just right of whatever of the column's
    /// frames reaches down to its row (the board's `shiftSideways`).
    static func pttFrame(home: CGRect, avoid: [CGRect]) -> CGRect {
        let reaching = avoid.filter { $0.maxY > home.minY && $0.minY < home.maxY }
        guard let edge = reaching.map(\.maxX).max(), edge + clearance > home.minX else {
            return home
        }
        return CGRect(x: edge + clearance, y: home.minY, width: home.width, height: home.height)
    }

    /// Where the keyed meters go.
    struct KeyedPlace: Equatable {
        let top: CGFloat
        /// The meters' region starts this far in from the band's left.
        let leading: CGFloat
        /// The meters sit at the region's left edge (above the PTT) rather than centred in it.
        let alignLeading: Bool
    }

    /// The keyed meters' place: as today while nothing of the column's
    /// reaches their height; centred right of it while they fit there; above
    /// the moved PTT, at its left, when they don't (the board's meters "beside
    /// it or above it").
    static func keyedPlace(top: CGFloat, height: CGFloat, bandWidth: CGFloat, needed: CGFloat,
                           avoid: [CGRect], ptt: CGRect?) -> KeyedPlace {
        let reaching = avoid.filter { $0.minY < top + height && $0.maxY > top }
        guard let edge = reaching.map(\.maxX).max() else {
            return KeyedPlace(top: top, leading: 0, alignLeading: false)
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

/// The desktop's button column (SpectrumOverlayPanel's strip) at the band's
/// top-left inside the safe area, on the iPad and the phone held sideways
/// (R-IOS-58, spec DC7, DC8, 6.2): ◀, +RX, +TNF, BAND, ANT, Display, VAX
/// and ATT, each 30 points tall at every text size, the open one in the
/// glass. ◀ folds it to one button and back. BAND, ANT, VAX and ATT open
/// the menus the phone already has beside the column, Display the pill; one
/// is open at a time and a tap anywhere else on the band closes it. A
/// button that cannot act stays, greyed, with its reason beside it.
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
    /// The pans, for the status line under the pan last touched's column.
    @ObservedObject var stack: PanStackModel
    /// This pan's flags' menus (ANT, VAX), its pill's model and its band.
    let controls: PanControls
    /// The pill's open tab, kept while it closes and opens.
    @Binding var displayTab: DisplayPillTab
    /// The phone's rounded edge sideways; 0 on the iPad.
    var leadingInset: CGFloat = 0
    let openSetup: () -> Void
    let onSetupDefaults: (DisplayPillTab, PanIdentity) -> Void
    let onOpenPage: (String) -> Void
    let onOpen: (PanIdentity) -> Void

    /// The Core's words for the column's last refused request, shown for a moment under it.
    @State private var shownNote: String?
    @State private var noteGeneration = 0

    /// How long a refusal stays under the column (the board's 3.2 s, rounded up).
    static let noteSeconds: Duration = .seconds(4)
    /// The desktop's ◀ and ▶.
    // From src/gui/SpectrumOverlayPanel.cpp:815-816 ("◀" expanded, "▶" folded)
    static let foldText = "\u{25C0}"
    static let unfoldText = "\u{25B6}"

    init(pan: PanIdentity, open: Binding<BandColumnMenu?>, folded: Binding<Bool>, main: MainScreenModel,
         displayTab: Binding<DisplayPillTab>, leadingInset: CGFloat = 0, openSetup: @escaping () -> Void,
         onSetupDefaults: @escaping (DisplayPillTab, PanIdentity) -> Void,
         onOpenPage: @escaping (String) -> Void, onOpen: @escaping (PanIdentity) -> Void) {
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
        self.openSetup = openSetup
        self.onSetupDefaults = onSetupDefaults
        self.onOpenPage = onOpenPage
        self.onOpen = onOpen
    }

    var body: some View {
        GeometryReader { proxy in
            let x = leadingInset + BandColumnLayout.origin
            let y = BandColumnLayout.origin
            ZStack(alignment: .topLeading) {
                if open != nil {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture { open = nil }
                        .accessibilityLabel("Close the menu")
                        .accessibilityAddTraits(.isButton)
                        .accessibilityIdentifier("bandColumnClose")
                }
                VStack(alignment: .leading, spacing: 4) {
                    // On a band shorter than the column (the phone sideways)
                    // the column scrolls, its buttons kept at 30 points.
                    ViewThatFits(in: .vertical) {
                        column
                        ScrollView(.vertical, showsIndicators: false) { column }
                            .frame(width: BandColumnLayout.columnWidth)
                    }
                    .frame(maxHeight: BandColumnLayout.menuMaxHeight(bandHeight: proxy.size.height), alignment: .top)
                    .reportsBandColumnFrame()
                    if let status = stack.status, stack.lastTouched == pan, !folded {
                        // What the last Add a pan or Close this pan did (spec section 6.3).
                        Text(status)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                            .fixedSize(horizontal: false, vertical: true)
                            .padding(.horizontal, 10)
                            .padding(.vertical, 6)
                            .frame(maxWidth: 260, alignment: .leading)
                            .workspacePanel()
                            .accessibilityIdentifier("bandColumnPanStatus")
                    }
                    if let shownNote, !folded {
                        Text(shownNote)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.refusalText)
                            .fixedSize(horizontal: false, vertical: true)
                            .padding(.horizontal, 10)
                            .padding(.vertical, 6)
                            .frame(maxWidth: 260, alignment: .leading)
                            .workspacePanel()
                            .accessibilityIdentifier("bandColumnNote")
                    }
                }
                .padding(.leading, x)
                .padding(.top, y)
                if let open, !folded {
                    menu(open)
                        .frame(maxHeight: BandColumnLayout.menuMaxHeight(bandHeight: proxy.size.height),
                               alignment: .top)
                        .padding(.leading, x + BandColumnLayout.columnWidth + BandColumnLayout.menuGap)
                        .padding(.top, y)
                        .transition(.opacity)
                }
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
    }

    // MARK: The column

    private var column: some View {
        VStack(alignment: .leading, spacing: BandColumnLayout.gap) {
            columnButton(folded ? Self.unfoldText : Self.foldText, id: "fold", lit: false) {
                folded.toggle()
            }
            .accessibilityLabel(folded ? "Show the buttons" : "Fold the buttons away")
            if !folded {
                let rxReason = sheet.addSliceOnPanReason(radio: main.radioName)
                withReason(rxReason) {
                    // From src/gui/SpectrumOverlayPanel.cpp:474 ("+RX")
                    columnButton("+RX", id: "rx", lit: false, disabled: rxReason != nil) {
                        sheet.addSlice(on: pan)
                    }
                    .accessibilityLabel("Add a slice on this pan")
                    .accessibilityHint(rxReason ?? "")
                }
                let tnfReason = sheet.canAddNotch ? nil : sheet.notchUnavailableReason
                withReason(tnfReason) {
                    // From src/gui/SpectrumOverlayPanel.cpp:489 ("+TNF")
                    columnButton("+TNF", id: "tnf", lit: false, disabled: tnfReason != nil) {
                        sheet.addNotch()
                    }
                    .accessibilityLabel("Add a notch in the active slice's passband")
                    .accessibilityHint(tnfReason ?? "")
                }
                // From src/gui/SpectrumOverlayPanel.cpp:500-535 ("BAND", "ANT", "Display", "VAX", "ATT"), in its order
                menuButton("BAND", .band, id: "band")
                menuButton("ANT", .antenna, id: "antenna")
                menuButton("Display", .display, id: "display")
                menuButton("VAX", .vax, id: "vax")
                menuButton("ATT", .attenuator, id: "attenuator")
            }
        }
        .padding(BandColumnLayout.padding)
        .accessibilityElement(children: .contain)
        .accessibilityLabel(pan.title)
        .accessibilityIdentifier("bandColumn")
    }

    private func menuButton(_ label: String, _ menu: BandColumnMenu, id: String) -> some View {
        columnButton(label, id: id, lit: open == menu) {
            open = open == menu ? nil : menu
        }
        .accessibilityValue(open == menu ? "Open" : "")
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

    /// A greyed button with its reason beside it, outside the column's
    /// frame so the PTT keeps to the column.
    private func withReason(_ reason: String?, @ViewBuilder button: () -> some View) -> some View {
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
                        .offset(x: BandColumnLayout.buttonWidth + 6)
                        .allowsHitTesting(false)
                        .accessibilityHidden(true)
                        .accessibilityIdentifier("bandColumnReason")
                }
            }
    }

    /// The widest a greyed button's reason beside it runs.
    static let reasonWidth: CGFloat = 220

    private func show(_ note: String?) {
        noteGeneration &+= 1
        let generation = noteGeneration
        shownNote = note
        guard note != nil else {
            return
        }
        Task { @MainActor in
            try? await Task.sleep(for: Self.noteSeconds)
            if generation == noteGeneration {
                shownNote = nil
            }
        }
    }

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
