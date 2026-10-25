// NereusSDR for iOS: the main screen for listening: the toolbar, the band edge to edge, the RX panel and the sound notice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the Roundtable line on each flag and
// the Roundtable sheet over the band, with the hold PTT and the dial strip.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 18, Roundtable's tags on the band and the
// swipes from its edges, the panels following the finger from the swipe's
// side. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the iPad's Roundtable applet in the
// column: the flag line scrolls to it, the tab bar's swipe widens it, the
// phone's sheet with the column hidden, and the first run as a form sheet.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: the swipes' model is a @StateObject, made
// once when the screen makes its own. J.J. Boyd (KG4VCF), AI-assisted via
// Claude Code.

import NereusBand
import NereusMirror
import SwiftUI

/// The Panadapter tab (R-IOS-11, spec section 5.1, D7, D11): the toolbar,
/// then the band edge to edge with its flags, markers, touches and zoom.
/// The RX panel slides in over the band from the left, with a dimmed band
/// behind it that closes it on a tap. The speaker's tap opens the Sound
/// panel from its button (D77); the link dot opens connection diagnostics.
/// When headphones go away the sound
/// pauses and the band shows the notice (D68); the whole notice is the tap
/// that plays the band on the speaker.
///
/// The toolbar's Pan 1 and Display drop their sheets under the toolbar
/// over the band (D73); a tap outside the open sheet closes it, and so
/// does a cover coming up over the band (link lost, radio off, offline),
/// so the cover's words never sit under a sheet. Under a cover the band
/// fades almost away, so nothing from it reads behind the cover's words.
///
/// Sideways it is the same layout turned: the toolbar with the Core's name
/// in the middle, the band across the whole width, the panel inside the
/// camera's side.
///
/// On an iPad (spec section 5.6, D30, D31) the RX and TX panels don't slide
/// in: on its side they sit in the applet column on the band's right, under
/// the analog S-meter, and upright in three columns below the band
/// (``IPadLayout``). The toolbar, flags, band plan, PTT and tabs are the
/// phone's.
///
/// A flag's Roundtable line opens the Roundtable sheet over the band (spec
/// 11.3): half height, full height, below the keyed gauges while keyed,
/// and above a 92-point strip with PTT and the dial when a dial is on.
/// While it is open PTT is press and hold, and closing it lets go.
///
/// On a phone, a swipe in from the band's left or right edge, or up from
/// the tab bar, opens what Setup maps it to (spec 11.3, S1): the RX or TX
/// panel follow the finger in from the swipe's side and spring back from a
/// short drag; the Roundtable sheet opens as the flag line opens it. A
/// swipe back on the dimmed band, or a tap there, closes a panel. Nothing
/// swipes while a sheet or panel is up, and the TX panel opened by a swipe
/// never keys: opening it sends nothing.
struct MainScreen: View {
    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    /// Transmit: the PTT, the TX panel and the keyed view.
    @ObservedObject private var transmit: TransmitModel
    @ObservedObject private var roundtable: RoundtableModel
    /// The connection's covers over the band: the link lost, the radio off,
    /// the phone offline, and back on the air. Nil where no flow runs (tests).
    let flow: ConnectionFlow?
    /// The phone has no network: the link chip says Offline.
    let offline: Bool
    /// Switches to the Setup tab.
    let openSetup: () -> Void
    /// Switches to the Radio tab.
    let openRadio: () -> Void
    /// The toolbar's link dot opens the same destination as Tools.
    let openPerformance: () -> Void
    let openTransmitSettings: () -> Void

    @State private var rxPanelOpen: Bool
    @State private var txPanelOpen: Bool
    @State private var soundPanelOpen = false
    @State private var sheet: OpenSheet?
    /// On an iPad on its side, the applet column shows (its corner button).
    @State private var columnShown: Bool
    /// The Roundtable sheet, open at this height, or closed.
    @State private var roundtableSheet: RoundtableSheet.Detent?
    /// Where the keyed gauges end on the band, while keyed.
    @State private var gaugesFoot: CGFloat?
    @State private var firstRunNoteSeen: Bool
    /// The arrangement the screen took last (an iPad's column, or not).
    @State private var layoutNow: IPadLayout = .phone
    /// The applet column widened to Roundtable's room list plus the room.
    @State private var roundtableWide = false
    /// Bumped to scroll the column to the Roundtable applet.
    @State private var appletScrollRequest = 0
    /// The first-run question in a form sheet (an iPad).
    @State private var firstRunForm = false
    /// Told each press and release of the hold PTT (the UI tests' record).
    private let holdObserver: ((PttHold.Action) -> Void)?
    /// The Panadapter tab's swipes, shared with the root's tab bar.
    @StateObject private var swipes: SwipeShortcutModel
    /// The swipes act here (the phone's root); off in pictures and tests that host the screen alone.
    private let swipesOn: Bool
    /// Where each panel sits and the side it slid in from: the toolbar's
    /// sides unless a swipe opened it from the other.
    @State private var rxPanelSide = HorizontalEdge.leading
    @State private var rxPanelEntry = Edge.leading
    @State private var txPanelSide = HorizontalEdge.trailing
    @State private var txPanelEntry = Edge.trailing
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.horizontalSizeClass) private var widthClass

    /// `rxPanelOpen` and `txPanelOpen` open the screen with that panel out,
    /// `sheet` with that sheet open.
    init(app: AppModel, main: MainScreenModel, flow: ConnectionFlow? = nil, rxPanelOpen: Bool = false,
         txPanelOpen: Bool = false, sheet: OpenSheet? = nil, columnShown: Bool = true,
         roundtableSheet: RoundtableSheet.Detent? = nil, holdObserver: ((PttHold.Action) -> Void)? = nil,
         swipes: SwipeShortcutModel? = nil, roundtableWide: Bool = false,
         openSetup: @escaping () -> Void = {}, openRadio: @escaping () -> Void = {},
         openPerformance: @escaping () -> Void = {}, openTransmitSettings: @escaping () -> Void = {}) {
        self.app = app
        self.main = main
        transmit = main.transmit
        roundtable = app.roundtable
        _roundtableSheet = State(initialValue: roundtableSheet)
        _roundtableWide = State(initialValue: roundtableWide)
        _firstRunNoteSeen = State(initialValue: app.phoneSettings.roundtableFirstRunNoteSeen)
        self.holdObserver = holdObserver
        // Made once for this screen when the root shares none (pictures, tests).
        _swipes = StateObject(wrappedValue: swipes ?? SwipeShortcutModel(settings: app.phoneSettings))
        swipesOn = swipes != nil
        _txPanelOpen = State(initialValue: txPanelOpen)
        self.flow = flow
        self.openSetup = openSetup
        self.openRadio = openRadio
        self.openPerformance = openPerformance
        self.openTransmitSettings = openTransmitSettings
        offline = flow?.offline ?? false
        _rxPanelOpen = State(initialValue: rxPanelOpen)
        _sheet = State(initialValue: sheet)
        _columnShown = State(initialValue: columnShown)
    }

    var body: some View {
        GeometryReader { proxy in
            let sideways = proxy.size.width > proxy.size.height
            let layout = IPadLayout.arrangement(horizontal: widthClass, size: proxy.size)
            Group {
                switch layout {
                case .phone:
                    VStack(spacing: 0) {
                        toolbar(sideways: sideways, layout: layout)
                        ConnectionSummaryRow(app: app)
                        band(sideways: sideways, leadingInset: proxy.safeAreaInsets.leading,
                             trailingInset: proxy.safeAreaInsets.trailing, phone: true)
                    }
                case .column:
                    // The iPad on its side (D30): the band on the left, the applet column on the right.
                    VStack(spacing: 0) {
                        toolbar(sideways: true, layout: layout)
                        ConnectionSummaryRow(app: app)
                        HStack(spacing: 0) {
                            band(sideways: true, leadingInset: 0, trailingInset: 0)
                            if columnShown {
                                AppletColumn(main: main, openRadio: openRadio, openSettings: openTransmitSettings,
                                             roundtable: roundtable.available ? appletConfig : nil)
                                    .transition(.move(edge: .trailing))
                            }
                        }
                    }
                case .frontPanel:
                    // The iPad upright (D31): the band the whole width, the applets below it.
                    VStack(spacing: 0) {
                        toolbar(sideways: false, layout: layout)
                        ConnectionSummaryRow(app: app)
                        band(sideways: false, leadingInset: 0, trailingInset: 0)
                        FrontPanelColumns(main: main, openRadio: openRadio, openSettings: openTransmitSettings)
                            .frame(height: IPadLayout.frontPanelHeight(underToolbar: proxy.size.height - Toolbar.height))
                    }
                }
            }
            .onAppear { main.setBandSideways(sideways) }
            .onChange(of: sideways) { main.setBandSideways(sideways) }
            .onChange(of: layout, initial: true) { _, now in layoutNow = now }
        }
        .environment(\.roundtableInColumn, appletInColumn)
        .onChange(of: appletInColumn) { _, inColumn in
            if inColumn {
                // The applet shows the room: the phone's sheet gives way to it.
                roundtableSheet = nil
            } else {
                roundtableWide = false
            }
        }
        .onChange(of: roundtable.askingFirstRun) { _, asking in
            if !asking {
                firstRunForm = false
            }
        }
        .sheet(isPresented: $firstRunForm) {
            // On an iPad the first-run question is a form sheet in the middle of the screen (spec 11.4).
            RoundtableFirstRunSheet(roundtable: roundtable, callsign: app.freedv.callsign,
                                    grid: app.freedv.gridSquare) {
                firstRunForm = false
                setFirstRunNoteSeen(true)
            }
            .background(RoundtableColours.ground)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        // Diversity..., from a flag's more menu.
        .modifier(FlagDiversitySheet(controls: main.flagControls, app: app))
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: rxPanelOpen)
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: txPanelOpen)
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: columnShown)
        .onAppear {
            swipes.attach(tuning: main.tuning, flagControls: main.flagControls)
        }
        .onChange(of: panelsUp, initial: true) { _, up in
            if swipesOn {
                swipes.panelsUp = up
            }
        }
        .onChange(of: swipes.opened) { _, opened in
            if swipesOn, let opened {
                open(opened)
            }
        }
        .onChange(of: rxPanelOpen) { _, open in
            if !open {
                // Closed, the panel goes back to the toolbar's side.
                rxPanelSide = .leading
                rxPanelEntry = .leading
            }
        }
        .onChange(of: txPanelOpen) { _, open in
            if !open {
                txPanelSide = .trailing
                txPanelEntry = .trailing
            }
        }
    }

    private func toolbar(sideways: Bool, layout: IPadLayout) -> some View {
        Toolbar(app: app, main: main, sideways: sideways, rxPanelOpen: $rxPanelOpen,
                txPanelOpen: $txPanelOpen, soundPanelOpen: $soundPanelOpen, sheet: $sheet, offline: offline,
                openRadio: openRadio, openPerformance: openPerformance,
                layout: layout, columnShown: $columnShown)
    }

    /// The keyed gauges and transmit's notices. Under a cover (LINK LOST
    /// and the others) they fade with the band, so the cover's words, which
    /// already say the Core stops transmitting on its own, read alone.
    @ViewBuilder
    private func keyedOverlay(sideways: Bool) -> some View {
        let overlay = KeyedOverlay(transmit: transmit, accessories: main.accessories, keyedMeters: main.keyedMeters,
                                   take: main.take, meters: main.band.catalog?.meters,
                                   showsStrip: main.band.settings.bandPlanStrip,
                                   spectrumShare: main.band.shownSpectrumShare, sideways: sideways,
                                   gaugesFoot: { foot in gaugesFoot = foot })
        if let flow {
            overlay.modifier(BandUnderCover(flow: flow) {})
        } else {
            overlay
        }
    }

    private func band(sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat,
                      phone: Bool = false) -> some View {
        let rxDrag = panelDrag(.rxPanel)
        let txDrag = panelDrag(.txPanel)
        let rxSide = rxDrag.map { Self.side($0.origin) } ?? rxPanelSide
        let txSide = txDrag.map { Self.side($0.origin) } ?? txPanelSide
        return ZStack(alignment: .topLeading) {
            // The swipes act on every layout; on an iPad both edges start Off (spec 11.4, S2).
            bandLayers(sideways: sideways, leadingInset: sideways ? leadingInset : 0,
                       trailingInset: sideways ? trailingInset : 0, swipesHere: swipesOn)
            GeometryReader { proxy in
                if let audio = app.audio {
                    SoundNotice(audio: audio, sideways: sideways, bandSize: proxy.size,
                                showsStrip: main.band.settings.bandPlanStrip,
                                spectrumShare: main.band.shownSpectrumShare)
                }
            }
            if let devices = main.devices {
                SeveralDevicesLayers(devices: devices, slices: main.slices, take: main.take,
                                     showsStrip: main.band.settings.bandPlanStrip,
                                     spectrumShare: main.band.shownSpectrumShare, sideways: sideways,
                                     leadingInset: sideways ? leadingInset : 0)
            }
            keyedOverlay(sideways: sideways)
            SharingChip(subscriber: main.subscriber, transmit: transmit)
            .padding(.top, 6)
            .padding(.leading, 8 + (sideways ? leadingInset : 0))
            GeometryReader { proxy in
                SessionNoteCard(session: app.longSession, sideways: sideways, bandSize: proxy.size,
                                showsStrip: main.band.settings.bandPlanStrip,
                                spectrumShare: main.band.shownSpectrumShare)
            }
            .allowsHitTesting(false)
            if roundtableSheet == nil || dialStrip {
                // With the Roundtable sheet open and a dial on, PTT stays here, in the strip, press and hold.
                PttButton(transmit: transmit, linkUp: pttLinkUp,
                          mode: roundtableSheet == nil ? .tap : .hold(diameter: PttButton.diameter),
                          holdObserver: holdObserver)
                    .padding(.leading, PttButton.inset + (sideways ? leadingInset : 0))
                    .padding(.bottom, roundtableSheet == nil ? PttButton.bottomInset
                             : (DialLayer.stripHeight - PttButton.diameter) / 2)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottomLeading)
            }
            // A spot's details or a badge's list, over PTT and under the covers.
            SpotLayer.Popups(spots: app.spots, sideways: sideways)
                .padding(.trailing, sideways ? trailingInset : 0)
            if let detent = roundtableSheet {
                GeometryReader { proxy in
                    let place = Self.roundtablePlace(bandHeight: proxy.size.height, detent: detent,
                                                     gaugesFoot: transmit.ptt.transmitting ? gaugesFoot : nil,
                                                     dialStrip: dialStrip)
                    RoundtableSheet(roundtable: roundtable, transmit: transmit, slices: main.slices,
                                    detent: Binding(get: { roundtableSheet ?? .half },
                                                    set: { roundtableSheet = $0 }),
                                    dialOn: dialStrip, linkUp: pttLinkUp,
                                    stationCallsign: app.freedv.callsign, stationGrid: app.freedv.gridSquare,
                                    firstRunNoteSeen: firstRunNoteSeenBinding,
                                    close: closeRoundtable, holdObserver: holdObserver,
                                    askFirstRun: layoutNow.isIPad ? { firstRunForm = true } : nil)
                        .frame(maxWidth: sideways ? Self.roundtableSidewaysWidth : .infinity)
                        .frame(height: place.height)
                        .padding(.leading, sideways ? leadingInset : 0)
                        .offset(y: place.top)
                        .frame(maxWidth: .infinity, alignment: sideways ? .trailing : .center)
                }
                .padding(.trailing, sideways ? trailingInset : 0)
                .transition(.move(edge: .bottom))
            }
            if let flow {
                LinkLostBanner(flow: flow, mirror: app.mirror, devices: main.devices)
            }
            if rxPanelOpen || txPanelOpen || rxDrag != nil || txDrag != nil {
                let drag = rxDrag ?? txDrag
                let closeSide = txPanelOpen || txDrag != nil ? txSide : rxSide
                ChromeColours.scrim
                    .contentShape(Rectangle())
                    .onTapGesture {
                        rxPanelOpen = false
                        txPanelOpen = false
                    }
                    // A swipe back toward the panel's side closes it too.
                    .simultaneousGesture(DragGesture(minimumDistance: TuneGestures.dragThresholdPoints)
                        .onEnded { value in
                            if Self.swipesBack(value.translation, panelSide: closeSide) {
                                rxPanelOpen = false
                                txPanelOpen = false
                            }
                        })
                    .opacity(drag.map { min(1, $0.distance / TxPanel.width) } ?? 1)
                    .transition(.opacity)
                    .accessibilityLabel(txPanelOpen ? "Close the TX panel" : "Close the RX panel")
                    .accessibilityAddTraits(.isButton)
            }
            if txPanelOpen || txDrag != nil {
                // Opening this panel, by its button or a swipe, sends nothing: it never keys.
                HStack(spacing: 0) {
                    if txSide == .trailing {
                        Spacer(minLength: 0)
                    }
                    TxPanel(transmit: transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes, meters: main.band.catalog?.meters, openRadio: {
                        txPanelOpen = false
                        openRadio()
                    }, modMonitor: main.modMonitor, take: main.take, openSettings: openTransmitSettings)
                        .padding(txSide == .trailing ? .trailing : .leading,
                                 sideways ? (txSide == .trailing ? trailingInset : leadingInset) : 0)
                        .background(ChromeColours.panel)
                        .overlay(alignment: txSide == .trailing ? .leading : .trailing) {
                            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
                        }
                        .shadow(color: .black.opacity(0.6), radius: 14)
                    if txSide == .leading {
                        Spacer(minLength: 0)
                    }
                }
                // Keep the pinned keys and scroll body in the same moving drawer geometry.
                .geometryGroup()
                .offset(x: Self.followOffset(txDrag, side: txSide,
                                             width: TxPanel.width + (sideways ? max(leadingInset, trailingInset) : 0)))
                .transition(.move(edge: txPanelEntry))
            }
            if rxPanelOpen || rxDrag != nil {
                HStack(spacing: 0) {
                    if rxSide == .trailing {
                        Spacer(minLength: 0)
                    }
                    RxPanel(model: main.rx, sliceColour: main.sliceColour)
                        .padding(rxSide == .leading ? .leading : .trailing,
                                 sideways ? (rxSide == .leading ? leadingInset : trailingInset) : 0)
                        .background(ChromeColours.panel)
                        .overlay(alignment: rxSide == .leading ? .trailing : .leading) {
                            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
                        }
                        .shadow(color: .black.opacity(0.6), radius: 14)
                    if rxSide == .leading {
                        Spacer(minLength: 0)
                    }
                }
                .offset(x: Self.followOffset(rxDrag, side: rxSide,
                                             width: RxPanel.width + (sideways ? max(leadingInset, trailingInset) : 0)))
                .transition(.move(edge: rxPanelEntry))
            }
            if let sheet {
                Color.black.opacity(0.001)
                    .contentShape(Rectangle())
                    .onTapGesture { self.sheet = nil }
                    .accessibilityLabel("Close the sheet")
                    .accessibilityAddTraits(.isButton)
                dropped(sheet)
                    .frame(maxWidth: sideways ? DropSheet<EmptyView, EmptyView>.sidewaysWidth : .infinity)
                    .padding(.top, 4)
                    .padding(.bottom, 8)
                    .padding(.horizontal, 8)
                    .padding(.leading, sideways ? leadingInset : 0)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                    .transition(.opacity)
            }
        }
        .clipped()
        .ignoresSafeArea(edges: sideways ? .horizontal : [])
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: roundtableSheet)
        .onChange(of: roundtableSheet == nil) { _, closed in
            if closed {
                // Closing the sheet lets go of a held PTT.
                transmit.releaseHeldPtt()
            }
        }
    }

    // MARK: Roundtable

    /// The sheet's widest sideways, as the drop sheets'.
    static let roundtableSidewaysWidth: CGFloat = 546
    /// Half height: the sheet's top this far down the band (the board's).
    static let roundtableHalfTop: CGFloat = 0.48
    /// Its gap below the keyed gauges.
    static let roundtableBelowGauges: CGFloat = 8

    /// Where the sheet sits on a band `bandHeight` tall: half or full,
    /// below the keyed gauges while keyed, and above the dial's strip.
    static func roundtablePlace(bandHeight: CGFloat, detent: RoundtableSheet.Detent, gaugesFoot: CGFloat?,
                                dialStrip: Bool) -> (top: CGFloat, height: CGFloat) {
        var top: CGFloat = detent == .full ? 0 : (bandHeight * roundtableHalfTop).rounded()
        if let gaugesFoot {
            // Keyed: the gauges stay, and the sheet drops below them.
            top = max(top, gaugesFoot + roundtableBelowGauges)
        }
        let bottom = bandHeight - (dialStrip ? DialLayer.stripHeight : 0)
        return (top, max(0, bottom - top))
    }

    /// A dial is on that keeps the strip under the open sheet: the
    /// waterfall knob or the thumbwheel (the sheet knob rises over the band).
    private var dialStrip: Bool {
        guard roundtableSheet != nil else {
            return false
        }
        return Self.keepsStrip(app.phoneSettings.dialKind)
    }

    static func keepsStrip(_ kind: DialKind) -> Bool {
        kind == .waterfallKnob || kind == .thumbwheel
    }

    private var pttLinkUp: Bool {
        app.connection == .connected || holdObserver != nil
    }

    /// Each slice's Roundtable line, by its flag.
    private var roundtableLines: [Int: RoundtableFlagLine] {
        var lines: [Int: RoundtableFlagLine] = [:]
        for entry in main.slices.entries {
            if let line = RoundtableFlagLine.line(roundtable, sliceId: Int64(entry.id)) {
                lines[entry.id] = line
            }
        }
        return lines
    }

    private func openRoundtable(_ sliceId: Int) {
        sheet = nil
        rxPanelOpen = false
        txPanelOpen = false
        main.slices.activate(sliceId)
        roundtable.activeSliceId = Int64(sliceId)
        if appletInColumn {
            // On an iPad with the column showing, the line scrolls the column to the applet (spec 11.4).
            appletScrollRequest += 1
            return
        }
        roundtableSheet = .half
    }

    /// The Roundtable applet shows in the iPad's applet column.
    private var appletInColumn: Bool {
        layoutNow == .column && columnShown && roundtable.available
    }

    private var firstRunNoteSeenBinding: Binding<Bool> {
        Binding(get: { firstRunNoteSeen }, set: { seen in setFirstRunNoteSeen(seen) })
    }

    private func setFirstRunNoteSeen(_ seen: Bool) {
        firstRunNoteSeen = seen
        app.phoneSettings.roundtableFirstRunNoteSeen = seen
    }

    private var appletConfig: RoundtableAppletConfig {
        RoundtableAppletConfig(roundtable: roundtable, settings: app.phoneSettings,
                               stationCallsign: app.freedv.callsign,
                               wide: Binding(get: { roundtableWide }, set: { wide in
                                   withAnimation(reduceMotion ? nil : .easeOut(duration: 0.22)) {
                                       roundtableWide = wide
                                   }
                               }),
                               scrollRequest: appletScrollRequest,
                               firstRunNoteSeen: firstRunNoteSeenBinding,
                               askFirstRun: { firstRunForm = true })
    }

    private func closeRoundtable() {
        roundtableSheet = nil
        transmit.releaseHeldPtt()
    }

    // MARK: Swipes

    /// A sheet or panel is up: no swipe opens anything.
    private var panelsUp: Bool {
        sheet != nil || rxPanelOpen || txPanelOpen || soundPanelOpen || roundtableSheet != nil
    }

    /// The swipe under the finger, when it brings in `target` from a band edge.
    private func panelDrag(_ target: SwipeTarget) -> SwipeShortcutModel.Drag? {
        guard swipesOn, let drag = swipes.drag, drag.target == target, drag.origin != .tabBar else {
            return nil
        }
        return drag
    }

    static func side(_ origin: SwipeOrigin) -> HorizontalEdge {
        origin == .rightEdge ? .trailing : .leading
    }

    /// A panel following a swipe: its hidden part off the side it comes
    /// from, `width` across in all, as far in as the finger.
    static func followOffset(_ drag: SwipeShortcutModel.Drag?, side: HorizontalEdge, width: CGFloat) -> CGFloat {
        guard let drag else {
            return 0
        }
        let hidden = max(0, width - drag.distance)
        return side == .leading ? -hidden : hidden
    }

    /// A drag on the dimmed band that heads back toward the panel's side.
    static func swipesBack(_ translation: CGSize, panelSide: HorizontalEdge) -> Bool {
        let toward = panelSide == .leading ? -translation.width : translation.width
        return toward >= SwipeShortcuts.openDistance / 2 && toward > abs(translation.height)
    }

    /// A swipe let go far enough: its page opens, from the swipe's side.
    /// The TX panel only shows; nothing is sent, so it never keys.
    private func open(_ opened: SwipeShortcutModel.Opened) {
        guard !opened.target.opensTools else {
            return
        }
        withAnimation(reduceMotion ? nil : .easeOut(duration: 0.22)) {
            switch opened.target {
            case .rxPanel:
                rxPanelSide = Self.side(opened.origin)
                rxPanelEntry = opened.origin.edge == .bottom ? .bottom
                    : rxPanelSide == .leading ? .leading : .trailing
                txPanelOpen = false
                rxPanelOpen = true
            case .txPanel:
                txPanelSide = opened.origin == .leftEdge ? .leading : .trailing
                txPanelEntry = opened.origin.edge == .bottom ? .bottom
                    : txPanelSide == .leading ? .leading : .trailing
                rxPanelOpen = false
                txPanelOpen = true
            case .roundtableMine:
                if appletInColumn {
                    // On an iPad the same swipe widens the applet and puts it back (spec 11.4, S2).
                    roundtableWide.toggle()
                } else {
                    // As the flag line opens it: half height, for the active slice.
                    openRoundtable(main.slices.activeSliceId ?? main.slices.entries.first?.id ?? 0)
                }
            case .off, .roundtableAll, .spotHub, .tools:
                break
            }
            swipes.finish()
        }
    }

    /// The band itself: the trace, waterfall, scales, band-plan strip,
    /// flags, markers, zoom and the tuning dial, if one is chosen. Under a
    /// cover it fades almost away, and a cover coming up closes an open
    /// drop sheet.
    @ViewBuilder
    private func bandLayers(sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat,
                            swipesHere: Bool) -> some View {
        let layers = ZStack(alignment: .topLeading) {
            BandView(model: main.band)
            // The sound-only marks sit over the band and beneath every
            // control on it, so one scrolling down passes under the dial.
            BandAwayMarks(band: main.band)
            if dialStrip {
                // The strip under the Roundtable sheet that keeps PTT and the dial.
                RoundtableColours.ground
                    .frame(height: DialLayer.stripHeight)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
                    .allowsHitTesting(false)
            }
            BandGestureLayer(band: main.band, slices: main.slices, settings: app.phoneSettings, tuning: main.tuning,
                             keyed: transmit.ptt.transmitting, txFilterHz: transmit.txFilterHz,
                             onAirSliceId: transmit.radioOnAirSliceId, txZeroLineSliceId: transmit.txZeroLineSliceId,
                             foreign: main.foreign, sideways: sideways,
                             trailingInset: trailingInset, leadingInset: leadingInset, display: main.display,
                             spots: app.spots,
                             readout: main.meterReadout,
                             flagLevel: { [weak sMeter = main.sMeter] in sMeter?.flagLevel(for: $0) },
                             meterRxMode: main.meterRxMode,
                             firstKeyRefused: BandSlicesModel.refusedForTransmitChoice(transmit.ptt),
                             flagControls: main.flagControls,
                             transmitReason: BandGestureLayer.transmitReason(transmit, take: main.take),
                             take: main.take,
                             roundtableLines: roundtableLines,
                             openRoundtable: { id in openRoundtable(id) },
                             dialStrip: dialStrip, roundtable: roundtable,
                             swipes: swipesHere ? swipes : nil)
            DbmScaleArrows(display: main.display, band: main.band, trailingInset: trailingInset)
            BandReadouts(band: main.band, trailingInset: trailingInset)
            TransmitDisplayBanner(band: main.band)
            StackNotice(band: main.band, sideways: sideways)
            LevelCalBandLine(band: main.band, levelCal: app.levelCal, sideways: sideways)
        }
        let faded = Group {
            if let devices = main.devices {
                layers.modifier(BandUnderReceiverTaken(devices: devices, slices: main.slices))
            } else {
                layers
            }
        }
        if let flow {
            faded.modifier(BandUnderCover(flow: flow) { sheet = nil })
        } else {
            faded
        }
    }

    @ViewBuilder
    private func dropped(_ kind: OpenSheet) -> some View {
        switch kind {
        case .pan:
            PanSheet(model: main.pan, sliceColour: main.sliceColour)
        case .display:
            DisplaySheet(model: main.display, core: main.coreDisplay) {
                sheet = nil
                openSetup()
            }
        case .slices:
            SliceListSheet(model: main.sliceList, slices: main.slices) {
                sheet = nil
            }
        }
    }
}

/// What sharing the Core puts over the band (spec sections 5.8 and 5.9):
/// RECEIVER TAKEN while this band's slices are gone to another device's
/// take, and the Core's newest notice. Its open question is asked over
/// every screen, from the root (``ConfirmationWindowAnchor``).
struct SeveralDevicesLayers: View {
    @ObservedObject var devices: SeveralDevicesClient
    @ObservedObject var slices: BandSlicesModel
    /// Take transmit, for a TX button's refusal that offers it.
    var take: TransmitTakeModel? = nil
    let showsStrip: Bool
    let spectrumShare: CGFloat
    let sideways: Bool
    var leadingInset: CGFloat = 0

    var body: some View {
        let taken = takenCover
        GeometryReader { proxy in
            let layout = BandLayout(size: proxy.size, scale: 1, showsStrip: showsStrip, spectrumShare: spectrumShare)
            let place = Self.noticePlace(bandWidth: proxy.size.width, waterfallTop: layout.waterfall.minY,
                                         sideways: sideways, leadingInset: leadingInset, avoid: slices.flagRects)
            ZStack(alignment: .top) {
                if let taken {
                    ReceiverTakenOverlay(devices: devices, notice: taken)
                }
                NoticeBanner(devices: devices, excluded: taken.map { [$0.id] } ?? [], refusal: slices.refusal,
                             dismissRefusal: { slices.dismissRefusal() }, take: take)
                    .frame(width: place.width)
                    .offset(x: place.x, y: place.y)
                    .frame(width: proxy.size.width, alignment: .topLeading)
            }
            .frame(width: proxy.size.width, height: proxy.size.height, alignment: .top)
        }
    }

    /// Where the Core's notice sits on the band: across the top of the
    /// waterfall, and sideways no wider than 546 points, centred. Over a
    /// full flag or its round buttons (`avoid`, R-IOS-42, R-IOS-11) it
    /// keeps clear of them, Take control on a listened slice's flag
    /// included, as the board draws it: sideways in the widest gap between
    /// the flags that reach down to it (clear of PTT on the left), hugging
    /// the flag beside it; otherwise below the flags it would cover.
    static func noticePlace(bandWidth: CGFloat, waterfallTop: CGFloat, sideways: Bool, leadingInset: CGFloat,
                            avoid: [CGRect]) -> (x: CGFloat, y: CGFloat, width: CGFloat) {
        let margin: CGFloat = 10
        let width = max(0, sideways ? min(noticeMaxWidthSideways, bandWidth - 2 * margin) : bandWidth - 2 * margin)
        let y = waterfallTop + (sideways ? 14 : 8)
        let x = (bandWidth - width) / 2
        let reaching = avoid.filter { $0.maxY > y }
        let covered = reaching.filter { $0.minX < x + width && $0.maxX > x }
        guard !covered.isEmpty else {
            return (x, y, width)
        }
        if sideways {
            let leftEdge = leadingInset + PttButton.inset + PttButton.diameter + margin
            let rightEdge = bandWidth - margin
            var gaps: [(start: CGFloat, end: CGFloat)] = []
            var cursor = leftEdge
            for blocker in reaching.sorted(by: { $0.minX < $1.minX }) {
                let end = blocker.minX - margin
                if end > cursor {
                    gaps.append((cursor, end))
                }
                cursor = max(cursor, blocker.maxX + margin)
            }
            if rightEdge > cursor {
                gaps.append((cursor, rightEdge))
            }
            if let widest = gaps.indices.max(by: { gaps[$0].end - gaps[$0].start < gaps[$1].end - gaps[$1].start }),
               gaps[widest].end - gaps[widest].start >= noticeMinWidthBeside {
                let gap = gaps[widest]
                let besideWidth = min(noticeMaxWidthSideways, gap.end - gap.start)
                // Left of every flag it sits against the first; otherwise against the flag on its left.
                let besideX = widest == 0 && gap.start == leftEdge ? gap.end - besideWidth : gap.start
                return (besideX, y, besideWidth)
            }
        }
        return (x, (covered.map(\.maxY).max() ?? y) + 8, width)
    }

    /// The notice's place with at most one rectangle to keep clear of.
    static func noticePlace(bandWidth: CGFloat, waterfallTop: CGFloat, sideways: Bool, leadingInset: CGFloat,
                            avoid: CGRect?) -> (x: CGFloat, y: CGFloat, width: CGFloat) {
        noticePlace(bandWidth: bandWidth, waterfallTop: waterfallTop, sideways: sideways, leadingInset: leadingInset,
                    avoid: avoid.map { [$0] } ?? [])
    }

    static let noticeMaxWidthSideways: CGFloat = 546
    /// The narrowest a notice goes beside a flag before it moves below it instead.
    static let noticeMinWidthBeside: CGFloat = 260

    private var takenCover: SeveralDevicesClient.ReceivedNotice? {
        Self.takenCover(devices: devices, slices: slices)
    }

    /// The receiver-taken notice the band stops under: while none of this
    /// phone's slices is left on it.
    static func takenCover(devices: SeveralDevicesClient,
                           slices: BandSlicesModel) -> SeveralDevicesClient.ReceivedNotice? {
        guard slices.entries.isEmpty else {
            return nil
        }
        return devices.notices.last { $0.notice.kind == .receiverTaken }
    }
}

/// Under RECEIVER TAKEN the band fades almost away, as under the link's
/// covers, so nothing from it reads behind the cover's words.
private struct BandUnderReceiverTaken: ViewModifier {
    @ObservedObject var devices: SeveralDevicesClient
    @ObservedObject var slices: BandSlicesModel

    func body(content: Content) -> some View {
        content.opacity(SeveralDevicesLayers.takenCover(devices: devices, slices: slices) == nil
                        ? 1 : LinkLostBanner.bandOpacityUnderCover)
    }
}

/// The paused-sound notice on the band, as picture 12 draws it (D68): the
/// whole notice is the tap target, and after the tap it says for a moment
/// where the band plays (on the speaker, or again after an interruption).
private struct SoundNotice: View {
    @ObservedObject var audio: AudioSessionController
    let sideways: Bool
    let bandSize: CGSize
    let showsStrip: Bool
    let spectrumShare: CGFloat

    /// The words shown for a moment after the tap, while shown.
    @State private var confirming: String?

    /// How long the words after the tap stay (the board's).
    static let confirmation: Duration = .milliseconds(1800)

    var body: some View {
        let layout = BandLayout(size: bandSize, scale: 1, showsStrip: showsStrip, spectrumShare: spectrumShare)
        Group {
            if let notice = audio.notice {
                Button {
                    audio.resumeFromNotice()
                    confirming = notice.resumedText
                    Task { @MainActor in
                        try? await Task.sleep(for: Self.confirmation)
                        confirming = nil
                    }
                } label: {
                    card(dot: ChromeColours.noticeWarn, glow: true, text: Self.styled(notice.text))
                }
                .buttonStyle(.plain)
                .accessibilityLabel(notice.text)
                .accessibilityIdentifier("soundNotice")
            } else if let confirming {
                card(dot: ChromeColours.noticeInfo, glow: false, text: Self.styled(confirming))
            }
        }
        .frame(maxWidth: sideways ? 546 : .infinity)
        .padding(.horizontal, 10)
        .frame(maxWidth: .infinity)
        .offset(y: layout.waterfall.minY + (sideways ? 14 : 8))
    }

    private func card(dot: Color, glow: Bool, text: Text) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Circle()
                .fill(dot)
                .frame(width: 10, height: 10)
                .shadow(color: glow ? dot.opacity(0.6) : .clear, radius: 3)
                .padding(.top, 4)
            text
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.text)
                .lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .contentShape(Rectangle())
    }

    /// The notice's first sentence in bold, as the board sets it.
    static func styled(_ text: String) -> Text {
        var styled = AttributedString(text)
        let first = text.range(of: ". ").map { String(text[..<$0.lowerBound]) + "." } ?? text
        if let range = styled.range(of: first) {
            styled[range].font = .system(size: 13, weight: .bold)
            styled[range].foregroundColor = ChromeColours.textBright
        }
        return Text(styled)
    }
}
