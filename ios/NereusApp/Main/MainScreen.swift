// NereusSDR for iOS: the main screen for listening: the toolbar, the band edge to edge, the RX panel and the sound notice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

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
/// On an iPad and on the phone held sideways the desktop's button column
/// sits at the band's top-left (R-IOS-58, DC7, DC8, ``BandButtonColumn``):
/// its menus and the Display pill open beside it, and the PTT and the keyed
/// meters keep just right of what it has out.
///
/// On an iPad (spec section 5.6, D30, D31) the RX and TX panels don't slide
/// in: on its side they sit in the applet column on the band's right, under
/// the analog S-meter, and upright in three columns below the band
/// (``IPadLayout``). The toolbar, flags, band plan, PTT and tabs are the
/// phone's.
///
/// With more than one pan open on the iPad the band's place is the
/// ``PanStack`` (R-IOS-59, DC9): each pan with its own band, column, pill
/// and band menu, the toolbar following the pan last touched. The PTT,
/// the keyed meters, the panels and the sheets stay over the whole stack.
struct MainScreen: View {
    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    /// Transmit: the PTT, the TX panel and the keyed view.
    @ObservedObject private var transmit: TransmitModel
    /// The iPad's open pans.
    @ObservedObject private var panStack: PanStackModel
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
    /// The Display pill's open tab, kept while the sheet closes and opens.
    @State private var displayTab: DisplayPillTab = .spectrum
    /// The Display pill's row a finger holds: only it draws while held (spec DC4).
    @State private var displayPeek: String?
    /// The band menu a hold on empty band opened, where it opened (DC6).
    @State private var bandMenu: BandMenuAnchor?
    /// On an iPad on its side, the applet column shows (its corner button).
    @State private var columnShown: Bool
    /// What the band's button column has open beside it, on the iPad and
    /// sideways (R-IOS-58, DC7, DC8).
    @State private var columnMenu: BandColumnMenu?
    /// The button column is folded to its one button.
    @State private var columnFolded = false
    /// With more than one pan, the pan whose column has `columnMenu` open.
    @State private var columnPan: PanIdentity
    /// With more than one pan, the pans below Pan 1 whose column is folded.
    @State private var foldedPans: Set<PanIdentity> = []
    /// The pan the band menu opened on.
    @State private var bandMenuPan: PanIdentity = .first
    /// The column's frames on the band, the PTT and keyed meters keep clear of.
    @State private var columnBoxes: [CGRect] = []
    /// The band's size, for the PTT's place.
    @State private var bandSize: CGSize = .zero
    /// Where the PTT was when transmitting began, held until unkey
    /// (``PttLatch``). Held, not watched: only the views that place the PTT
    /// redraw as it changes.
    @State private var pttHold = PttHold()
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.horizontalSizeClass) private var widthClass

    /// `rxPanelOpen` and `txPanelOpen` open the screen with that panel out,
    /// `sheet` with that sheet open.
    init(app: AppModel, main: MainScreenModel, flow: ConnectionFlow? = nil, rxPanelOpen: Bool = false,
         txPanelOpen: Bool = false, sheet: OpenSheet? = nil, displayTab: DisplayPillTab = .spectrum,
         columnShown: Bool = true, columnMenu: BandColumnMenu? = nil, columnPan: PanIdentity = .first,
         openSetup: @escaping () -> Void = {}, openRadio: @escaping () -> Void = {},
         openPerformance: @escaping () -> Void = {}, openTransmitSettings: @escaping () -> Void = {}) {
        self.app = app
        self.main = main
        transmit = main.transmit
        panStack = main.panStack
        _columnPan = State(initialValue: columnPan)
        _txPanelOpen = State(initialValue: txPanelOpen)
        self.flow = flow
        self.openSetup = openSetup
        self.openRadio = openRadio
        self.openPerformance = openPerformance
        self.openTransmitSettings = openTransmitSettings
        offline = flow?.offline ?? false
        _rxPanelOpen = State(initialValue: rxPanelOpen)
        _sheet = State(initialValue: sheet)
        _displayTab = State(initialValue: displayTab)
        _columnShown = State(initialValue: columnShown)
        _columnMenu = State(initialValue: columnMenu)
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
                             trailingInset: proxy.safeAreaInsets.trailing, column: sideways)
                    }
                case .column:
                    // The iPad on its side (D30): the band on the left, the applet column on the right.
                    VStack(spacing: 0) {
                        toolbar(sideways: true, layout: layout)
                        ConnectionSummaryRow(app: app)
                        HStack(spacing: 0) {
                            band(sideways: true, leadingInset: 0, trailingInset: 0, column: true)
                            if columnShown {
                                AppletColumn(main: main, openRadio: openRadio, openSettings: openTransmitSettings)
                                    .transition(.move(edge: .trailing))
                            }
                        }
                    }
                case .frontPanel:
                    // The iPad upright (D31): the band the whole width, the applets below it.
                    VStack(spacing: 0) {
                        toolbar(sideways: false, layout: layout)
                        ConnectionSummaryRow(app: app)
                        band(sideways: false, leadingInset: 0, trailingInset: 0, column: true)
                        FrontPanelColumns(main: main, openRadio: openRadio, openSettings: openTransmitSettings)
                            .frame(height: IPadLayout.frontPanelHeight(underToolbar: proxy.size.height - Toolbar.height))
                    }
                }
            }
            .onAppear { main.setBandSideways(sideways) }
            .onChange(of: sideways) { main.setBandSideways(sideways) }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        // Diversity..., from a flag's more menu.
        .modifier(FlagDiversitySheet(controls: main.flagControls, app: app))
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: rxPanelOpen)
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: txPanelOpen)
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: columnShown)
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
    private func keyedOverlay(sideways: Bool, avoid: [CGRect], movedPtt: CGRect?, heldPtt: Bool = false,
                              leadingInset: CGFloat = 0, trailingInset: CGFloat = 0) -> some View {
        // Every pan's band has its tuning, so the dial is the one Setup chose.
        let dialKind = app.phoneSettings.dialKind
        let overlay = KeyedOverlay(transmit: transmit, accessories: main.accessories, keyedMeters: main.keyedMeters,
                                   take: main.take, meters: main.band.catalog?.meters,
                                   showsStrip: main.band.settings.bandPlanStrip,
                                   spectrumShare: main.band.shownSpectrumShare, sideways: sideways,
                                   avoid: avoid, movedPtt: movedPtt, heldPtt: heldPtt,
                                   leadingInset: leadingInset, trailingInset: trailingInset, dialKind: dialKind)
        if let flow {
            overlay.modifier(BandUnderCover(flow: flow) {})
        } else {
            overlay
        }
    }

    /// The band and everything over it. `column` puts the desktop's button
    /// column at its top-left (the iPad, and the phone held sideways); the
    /// PTT and the keyed meters then keep just right of what it has out.
    private func band(sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat, column: Bool) -> some View {
        let pttHome = PttButton.frame(bandSize: bandSize, leadingInset: sideways ? leadingInset : 0)
        let boxes = column ? columnBoxes : []
        let pttAt = BandColumnLayout.pttFrame(home: pttHome, avoid: boxes)
        let pttMoved = pttAt != pttHome
        let stacked = column && panStack.open.count > 1
        // With the column, the PTT also keeps off a flag's words (R-IOS-58):
        // Pan 1's alone, or with the iPad's pans stacked those of the pan at
        // the foot, where the PTT sits, moved down by that pan's top.
        let footPan = panStack.open.last ?? .first
        let pttFlags = column ? (stacked ? main.controls(for: footPan).band.slices : main.slices) : nil
        let pttFlagsTop = stacked
            ? BandColumnLayout.panTop(index: panStack.open.count - 1, count: panStack.open.count,
                                      bandHeight: bandSize.height)
            : 0
        // While keyed the PTT stays where it was when transmitting began,
        // whatever the flags do, so the tap that unkeys lands on it (DC15 as
        // amended, the manager's ruling 2026-10-25 for JJ's veto; PttLatch);
        // the keyed meters keep clear of it there.
        // It never moves past the band's right edge.
        let keyed = transmit.ptt.transmitting
        let pttMaxX = bandSize.width - (sideways ? trailingInset : 0) - PttButton.inset
        // Two columns of four where one does not fit (DC14); the chip keeps right of them.
        let columnWidth = BandColumnLayout.columnWidth(
            columns: columnFolded || stacked ? 1 : BandColumnLayout.columns(bandHeight: bandSize.height))
        return ZStack(alignment: .topLeading) {
            if stacked {
                PanStack(stack: panStack) { pan in
                    stackedPan(pan, sideways: sideways, leadingInset: sideways ? leadingInset : 0,
                               trailingInset: sideways ? trailingInset : 0)
                }
            } else {
                bandLayers(main.controls(for: .first), sideways: sideways, leadingInset: sideways ? leadingInset : 0,
                           trailingInset: sideways ? trailingInset : 0)
            }
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
            PttPlace(flags: pttFlags, flagsTop: pttFlagsTop, home: pttHome, boxes: boxes, keyed: keyed,
                     maxX: pttMaxX, hold: pttHold) { placed, held in
                let moved = placed != pttHome
                keyedOverlay(sideways: sideways, avoid: boxes + (moved ? [placed] : []), movedPtt: moved ? placed : nil,
                             heldPtt: held, leadingInset: sideways ? leadingInset : 0,
                             trailingInset: sideways ? trailingInset : 0)
            }
            SharingChip(subscriber: main.subscriber, transmit: transmit)
            .padding(.top, 6)
            .padding(.leading, 8 + (sideways ? leadingInset : 0)
                     + (column ? BandColumnLayout.origin + columnWidth : 0))
            GeometryReader { proxy in
                SessionNoteCard(session: app.longSession, sideways: sideways, bandSize: proxy.size,
                                showsStrip: main.band.settings.bandPlanStrip,
                                spectrumShare: main.band.shownSpectrumShare)
            }
            .allowsHitTesting(false)
            if column && !stacked {
                BandButtonColumn(pan: .first, open: $columnMenu, folded: $columnFolded, main: main,
                                 displayTab: $displayTab, leadingInset: sideways ? leadingInset : 0,
                                 trailingInset: sideways ? trailingInset : 0,
                                 openSetup: {
                                     columnMenu = nil
                                     openSetup()
                                 }, onSetupDefaults: { tab, pan in
                                     columnMenu = nil
                                     openDescribedPage(tab.setupPageId, scope: .pan(pan))
                                 }, onOpenPage: { pageId in
                                     columnMenu = nil
                                     openDescribedPage(pageId)
                                 }, onOpen: { pan in
                                     app.phoneNavigation?.followDisplayPan(pan)
                                 }, pttHold: pttHold)
                    .environment(\.displayPeek, $displayPeek)
            }
            // This one holds the PTT's place while keyed; the one above reads it.
            PttPlace(flags: pttFlags, flagsTop: pttFlagsTop, home: pttHome, boxes: boxes, keyed: keyed,
                     maxX: pttMaxX, hold: pttHold, updatesHold: true) { placed, _ in
                // Moved by an offset, not by its leading padding: padding
                // past the safe area's width made the band wider than it was
                // offered, which narrowed the band, moved the flags and so
                // moved the PTT back, without end (DialShotTests on a Pro Max
                // sideways with the thumbwheel). An offset leaves the band's
                // size alone.
                PttButton(transmit: transmit, linkUp: app.connection == .connected)
                    .offset(x: placed.minX - pttHome.minX)
                    .padding(.leading, PttButton.inset + (sideways ? leadingInset : 0))
                    .padding(.bottom, PttButton.bottomInset)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottomLeading)
            }
            // A spot's details or a badge's list, over PTT and under the covers.
            SpotLayer.Popups(spots: app.spots, sideways: sideways)
                .padding(.trailing, sideways ? trailingInset : 0)
            if let anchor = bandMenu, !stacked {
                // Over PTT, as a spot's details are; a tap off it closes it.
                BandMenuLayer(anchor: anchor, display: main.display, scale: main.bandScale, pan: main.pan,
                              sideways: sideways, leadingInset: sideways ? leadingInset : 0,
                              trailingInset: sideways ? trailingInset : 0, ptt: pttMoved ? pttAt : nil,
                              close: { bandMenu = nil },
                              openDisplay: {
                                  bandMenu = nil
                                  sheet = .display
                              })
            }
            if let flow {
                LinkLostBanner(flow: flow, mirror: app.mirror, devices: main.devices)
            }
            if rxPanelOpen || txPanelOpen {
                ChromeColours.scrim
                    .contentShape(Rectangle())
                    .onTapGesture {
                        rxPanelOpen = false
                        txPanelOpen = false
                    }
                    .transition(.opacity)
                    .accessibilityLabel(txPanelOpen ? "Close the TX panel" : "Close the RX panel")
                    .accessibilityAddTraits(.isButton)
            }
            if txPanelOpen {
                HStack(spacing: 0) {
                    Spacer(minLength: 0)
                    TxPanel(transmit: transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes, meters: main.band.catalog?.meters, openRadio: {
                        txPanelOpen = false
                        openRadio()
                    }, modMonitor: main.modMonitor, take: main.take, openSettings: openTransmitSettings)
                        .padding(.trailing, sideways ? trailingInset : 0)
                        .background(ChromeColours.panel)
                        .overlay(alignment: .leading) {
                            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
                        }
                        .shadow(color: .black.opacity(0.6), radius: 14)
                }
                // Keep the pinned keys and scroll body in the same moving drawer geometry.
                .geometryGroup()
                .transition(.move(edge: .trailing))
            }
            if rxPanelOpen {
                HStack(spacing: 0) {
                    RxPanel(model: main.rx, sliceColour: main.sliceColour)
                        .padding(.leading, sideways ? leadingInset : 0)
                        .background(ChromeColours.panel)
                        .overlay(alignment: .trailing) {
                            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
                        }
                        .shadow(color: .black.opacity(0.6), radius: 14)
                    Spacer(minLength: 0)
                }
                .transition(.move(edge: .leading))
            }
            // With the column, Display is its pill, never the drop sheet.
            if let sheet, !(column && sheet == .display) {
                Color.black.opacity(0.001)
                    .contentShape(Rectangle())
                    .onTapGesture { self.sheet = nil }
                    .accessibilityLabel("Close the sheet")
                    .accessibilityAddTraits(.isButton)
                dropped(sheet)
                    .environment(\.displayPeek, $displayPeek)
                    .frame(maxWidth: sideways ? DropSheet<EmptyView, EmptyView>.sidewaysWidth : .infinity)
                    .padding(.top, 4)
                    .padding(.bottom, 8)
                    .padding(.horizontal, 8)
                    .padding(.leading, sideways ? leadingInset : 0)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                    .transition(.opacity)
            }
        }
        .coordinateSpace(name: BandColumnLayout.space)
        .onGeometryChange(for: CGSize.self) { $0.size } action: { bandSize = $0 }
        .onPreferenceChange(BandColumnFrameKey.self) { columnBoxes = $0 }
        .clipped()
        .ignoresSafeArea(edges: sideways ? .horizontal : [])
        // A sheet or a panel opening closes the band menu and the column's menu.
        .onChange(of: sheet != nil || rxPanelOpen || txPanelOpen) { _, covered in
            if covered {
                bandMenu = nil
                columnMenu = nil
            }
        }
        // One open thing at a time (DC6, DC7): the band menu, a column menu, a flag's menu.
        .onChange(of: bandMenu != nil) { _, opened in
            if opened {
                columnMenu = nil
            }
        }
        .onChange(of: columnMenu) { _, menu in
            if menu != nil {
                bandMenu = nil
                main.flagControls.close()
            }
        }
        .onReceive(main.flagControls.$open) { flag in
            if flag != nil {
                columnMenu = nil
            }
        }
        // With the column, the toolbar's Display and the band menu's Display
        // settings... open the pill beside it (DC7, DC8).
        .onChange(of: sheet) { _, opened in
            if column, opened == .display {
                openPillInColumn()
            }
        }
        .onChange(of: column) { _, shown in
            if shown {
                if sheet == .display {
                    openPillInColumn()
                }
            } else {
                if columnMenu == .display {
                    sheet = .display
                }
                columnMenu = nil
            }
        }
        .onAppear {
            if column, sheet == .display {
                openPillInColumn()
            }
        }
        // A pan closing takes its open menu and band menu with it.
        .onChange(of: panStack.open) { _, open in
            if !open.contains(columnPan) || open.count < 2 && columnPan != .first {
                columnMenu = nil
                columnPan = .first
            }
            if !open.contains(bandMenuPan) || open.count < 2 && bandMenuPan != .first {
                bandMenu = nil
                bandMenuPan = .first
            }
            foldedPans.formIntersection(open)
        }
    }

    /// The pill beside the column in place of the drop sheet: on the iPad
    /// with more than one pan, beside the column of the pan last touched.
    private func openPillInColumn() {
        sheet = nil
        let pan = panStack.open.count > 1 ? main.toolbarPan : .first
        columnPan = pan
        if pan == .first {
            columnFolded = false
        } else {
            foldedPans.remove(pan)
        }
        columnMenu = .display
    }

    /// One pan in the stack: its band, its column with its menus and pill,
    /// and the band menu a hold on its empty band opens.
    private func stackedPan(_ pan: PanIdentity, sideways: Bool, leadingInset: CGFloat,
                            trailingInset: CGFloat) -> some View {
        let controls = main.controls(for: pan)
        let open = Binding<BandColumnMenu?>(get: { columnPan == pan ? columnMenu : nil }, set: { menu in
            if menu != nil {
                columnPan = pan
                panStack.lastTouched = pan
            } else if columnPan != pan {
                return
            }
            columnMenu = menu
        })
        let folded = Binding<Bool>(get: { pan == .first ? columnFolded : foldedPans.contains(pan) }, set: { fold in
            if pan == .first {
                columnFolded = fold
            } else if fold {
                foldedPans.insert(pan)
            } else {
                foldedPans.remove(pan)
            }
        })
        let menu = Binding<BandMenuAnchor?>(get: { bandMenuPan == pan ? bandMenu : nil }, set: { anchor in
            if anchor != nil {
                bandMenuPan = pan
                panStack.lastTouched = pan
            } else if bandMenuPan != pan {
                return
            }
            bandMenu = anchor
        })
        return ZStack(alignment: .topLeading) {
            bandLayers(controls, sideways: sideways, leadingInset: leadingInset, trailingInset: trailingInset,
                       menuBinding: menu)
            BandButtonColumn(pan: pan, open: open, folded: folded, main: main, displayTab: $displayTab,
                             leadingInset: leadingInset, trailingInset: trailingInset,
                             openSetup: {
                                 columnMenu = nil
                                 openSetup()
                             }, onSetupDefaults: { tab, pan in
                                 columnMenu = nil
                                 openDescribedPage(tab.setupPageId, scope: .pan(pan))
                             }, onOpenPage: { pageId in
                                 columnMenu = nil
                                 openDescribedPage(pageId)
                             }, onOpen: { pan in
                                 panStack.lastTouched = pan
                                 app.phoneNavigation?.followDisplayPan(pan)
                             },
                             // The PTT sits on the pan at the foot: that pan's note keeps clear of it where it is held.
                             pttHold: pan == panStack.open.last ? pttHold : nil)
                .environment(\.displayPeek, $displayPeek)
            if let anchor = menu.wrappedValue {
                BandMenuLayer(anchor: anchor, display: controls.display, scale: controls.bandScale,
                              pan: controls.sheet, sideways: sideways, leadingInset: leadingInset,
                              trailingInset: trailingInset, ptt: nil,
                              close: { bandMenu = nil },
                              openDisplay: {
                                  bandMenu = nil
                                  panStack.lastTouched = pan
                                  sheet = .display
                              })
            }
        }
        // Diversity..., from this pan's flag: Pan 1's sheet sits on the screen.
        .modifier(PanDiversitySheet(pan: pan, controls: controls.flagControls, app: app))
    }

    /// The band itself: the trace, waterfall, scales, band-plan strip,
    /// flags, markers, zoom and the tuning dial, if one is chosen. Under a
    /// cover it fades almost away, and a cover coming up closes an open
    /// drop sheet.
    @ViewBuilder
    private func bandLayers(_ controls: PanControls, sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat,
                            menuBinding: Binding<BandMenuAnchor?>? = nil) -> some View {
        let band = controls.band.band
        let slices = controls.band.slices
        let first = controls.pan == .first
        let layers = ZStack(alignment: .topLeading) {
            BandView(model: band)
            // The sound-only marks sit over the band and beneath every
            // control on it, so one scrolling down passes under the dial.
            BandAwayMarks(band: band)
            BandGestureLayer(band: band, slices: slices, settings: app.phoneSettings, tuning: controls.tuning,
                             keyed: transmit.ptt.transmitting, txFilterHz: transmit.txFilterHz,
                             onAirSliceId: transmit.radioOnAirSliceId, txZeroLineSliceId: transmit.txZeroLineSliceId,
                             foreign: main.foreign, sideways: sideways,
                             trailingInset: trailingInset, leadingInset: leadingInset, display: controls.display,
                             spots: app.spots,
                             readout: main.meterReadout,
                             flagLevel: { [weak sMeter = main.sMeter] in sMeter?.flagLevel(for: $0) },
                             meterRxMode: main.meterRxMode,
                             firstKeyRefused: BandSlicesModel.refusedForTransmitChoice(transmit.ptt),
                             flagControls: controls.flagControls,
                             transmitReason: BandGestureLayer.transmitReason(transmit, take: main.take),
                             take: main.take, bandMenu: menuBinding ?? $bandMenu)
            DbmScaleArrows(display: controls.display, band: band, trailingInset: trailingInset)
            BandReadouts(band: band, trailingInset: trailingInset)
            TransmitDisplayBanner(band: band)
            StackNotice(band: band, sideways: sideways)
            PanDisplayRefusalNotice(stack: panStack, band: band, pan: controls.pan, sideways: sideways,
                                    leadingInset: leadingInset, trailingInset: trailingInset)
            if first {
                // Level calibration reads Pan 1's slices.
                LevelCalBandLine(band: band, levelCal: app.levelCal, sideways: sideways)
            }
        }
        let faded = Group {
            if let devices = main.devices {
                layers.modifier(BandUnderReceiverTaken(devices: devices, slices: slices))
            } else {
                layers
            }
        }
        if let flow {
            faded.modifier(BandUnderCover(flow: flow) {
                sheet = nil
                bandMenu = nil
                columnMenu = nil
            })
        } else {
            faded
        }
    }

    /// Closes the sheet and opens a described Setup page; a Display page
    /// with `scope` chosen on its strip when one is given.
    private func openDescribedPage(_ pageId: String, scope: MainScreenModel.DisplayEditScope? = nil) {
        sheet = nil
        openSetup()
        if let scope, app.phoneNavigation?.openDisplayPage(pageId, scope) == true {
            return
        }
        _ = app.phoneNavigation?.openSetupPage(pageId)
    }

    @ViewBuilder
    private func dropped(_ kind: OpenSheet) -> some View {
        switch kind {
        case .pan:
            // The pan last touched's sheet, with Add a pan and Close this pan.
            let controls = main.controls(for: main.toolbarPan)
            PanSheet(model: controls.sheet, sliceColour: main.sliceColour, pan: controls.pan, stack: main.panStack,
                     slices: controls.band.slices)
        case .display:
            if let source = main.displayPillSource {
                DisplayDropSheet(pages: source.pages, source: source, main: main, tab: $displayTab,
                                 device: DisplayDropSheet.deviceWord, openSetup: {
                    sheet = nil
                    openSetup()
                }, onSetupDefaults: { tab, pan in
                    openDescribedPage(tab.setupPageId, scope: .pan(pan))
                }, onOpenPage: { pageId in
                    openDescribedPage(pageId)
                }, onOpen: { pan in
                    app.phoneNavigation?.followDisplayPan(pan)
                })
            } else {
                DisplaySheet(model: main.display, core: main.coreDisplay) {
                    sheet = nil
                    openSetup()
                }
            }
        case .slices:
            SliceListSheet(model: main.sliceList, slices: main.slices) {
                sheet = nil
            }
        }
    }
}

/// The PTT's place on the band (``BandColumnLayout/pttFrame(home:avoid:flags:keyed:maxX:)``):
/// right of the column's frames that reach its row and, with `flags`, of a
/// full flag it would cover, never past `maxX`. While `keyed`, where it was
/// when transmitting began (``PttLatch``, in `hold`), which the one view with
/// `updatesHold` keeps; `content` is also told whether it is held away from
/// DC15's keyed place. Only this view follows the flags as they move, not
/// the whole screen.
private struct PttPlace<Content: View>: View {
    let flags: BandSlicesModel?
    /// Where the flags' pan starts down the band: 0 but for a stacked pan.
    var flagsTop: CGFloat = 0
    let home: CGRect
    let boxes: [CGRect]
    var keyed = false
    var maxX: CGFloat = .infinity
    @ObservedObject var hold: PttHold
    var updatesHold = false
    /// The PTT's frame, and whether it is held away from DC15's keyed place.
    @ViewBuilder let content: (CGRect, Bool) -> Content

    var body: some View {
        if let flags {
            Following(slices: flags, top: flagsTop, home: home, boxes: boxes, maxX: maxX) { receive, keyedRule in
                placed(receive: receive, keyedRule: keyedRule)
            }
        } else {
            placed(receive: BandColumnLayout.pttFrame(home: home, avoid: boxes, maxX: maxX),
                   keyedRule: BandColumnLayout.pttFrame(home: home, avoid: boxes, keyed: true, maxX: maxX))
        }
    }

    @ViewBuilder
    private func placed(receive: CGRect, keyedRule: CGRect) -> some View {
        let inputs = PttLatch.Inputs(keyed: keyed, home: home, maxX: maxX, receive: receive, keyedRule: keyedRule)
        let shown = PttLatch.shown(hold.latch, inputs)
        if updatesHold {
            content(shown, keyed && shown != keyedRule)
                .onChange(of: inputs, initial: true) { _, now in hold.update(now) }
        } else {
            content(shown, keyed && shown != keyedRule)
        }
    }

    private struct Following<Placed: View>: View {
        @ObservedObject var slices: BandSlicesModel
        let top: CGFloat
        let home: CGRect
        let boxes: [CGRect]
        let maxX: CGFloat
        let content: (CGRect, CGRect) -> Placed

        var body: some View {
            let flags = slices.flagRects.map { $0.offsetBy(dx: 0, dy: top) }
            content(BandColumnLayout.pttFrame(home: home, avoid: boxes, flags: flags, maxX: maxX),
                    BandColumnLayout.pttFrame(home: home, avoid: boxes, flags: flags, keyed: true, maxX: maxX))
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
