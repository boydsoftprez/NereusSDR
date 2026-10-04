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
/// On an iPad (spec section 5.6, D30, D31) the RX and TX panels don't slide
/// in: on its side they sit in the applet column on the band's right, under
/// the analog S-meter, and upright in three columns below the band
/// (``IPadLayout``). The toolbar, flags, band plan, PTT and tabs are the
/// phone's.
struct MainScreen: View {
    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    /// Transmit: the PTT, the TX panel and the keyed view.
    @ObservedObject private var transmit: TransmitModel
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
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.horizontalSizeClass) private var widthClass

    /// `rxPanelOpen` and `txPanelOpen` open the screen with that panel out,
    /// `sheet` with that sheet open.
    init(app: AppModel, main: MainScreenModel, flow: ConnectionFlow? = nil, rxPanelOpen: Bool = false,
         txPanelOpen: Bool = false, sheet: OpenSheet? = nil, columnShown: Bool = true,
         openSetup: @escaping () -> Void = {}, openRadio: @escaping () -> Void = {},
         openPerformance: @escaping () -> Void = {}, openTransmitSettings: @escaping () -> Void = {}) {
        self.app = app
        self.main = main
        transmit = main.transmit
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
                             trailingInset: proxy.safeAreaInsets.trailing)
                    }
                case .column:
                    // The iPad on its side (D30): the band on the left, the applet column on the right.
                    VStack(spacing: 0) {
                        toolbar(sideways: true, layout: layout)
                        ConnectionSummaryRow(app: app)
                        HStack(spacing: 0) {
                            band(sideways: true, leadingInset: 0, trailingInset: 0)
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
                        band(sideways: false, leadingInset: 0, trailingInset: 0)
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
    private func keyedOverlay(sideways: Bool) -> some View {
        let overlay = KeyedOverlay(transmit: transmit, accessories: main.accessories, keyedMeters: main.keyedMeters,
                                   take: main.take, meters: main.band.catalog?.meters,
                                   showsStrip: main.band.settings.bandPlanStrip,
                                   spectrumShare: main.band.shownSpectrumShare, sideways: sideways)
        if let flow {
            overlay.modifier(BandUnderCover(flow: flow) {})
        } else {
            overlay
        }
    }

    private func band(sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat) -> some View {
        ZStack(alignment: .topLeading) {
            bandLayers(sideways: sideways, leadingInset: sideways ? leadingInset : 0,
                       trailingInset: sideways ? trailingInset : 0)
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
            PttButton(transmit: transmit, linkUp: app.connection == .connected)
                .padding(.leading, PttButton.inset + (sideways ? leadingInset : 0))
                .padding(.bottom, PttButton.bottomInset)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottomLeading)
            // A spot's details or a badge's list, over PTT and under the covers.
            SpotLayer.Popups(spots: app.spots, sideways: sideways)
                .padding(.trailing, sideways ? trailingInset : 0)
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
    }

    /// The band itself: the trace, waterfall, scales, band-plan strip,
    /// flags, markers, zoom and the tuning dial, if one is chosen. Under a
    /// cover it fades almost away, and a cover coming up closes an open
    /// drop sheet.
    @ViewBuilder
    private func bandLayers(sideways: Bool, leadingInset: CGFloat, trailingInset: CGFloat) -> some View {
        let layers = ZStack(alignment: .topLeading) {
            BandView(model: main.band)
            // The sound-only marks sit over the band and beneath every
            // control on it, so one scrolling down passes under the dial.
            BandAwayMarks(band: main.band)
            BandGestureLayer(band: main.band, slices: main.slices, settings: app.phoneSettings, tuning: main.tuning,
                             keyed: transmit.ptt.transmitting, txFilterHz: transmit.txFilterHz,
                             onAirSliceId: transmit.radioOnAirSliceId, txZeroLineSliceId: transmit.txZeroLineSliceId,
                             foreign: main.foreign, sideways: sideways,
                             trailingInset: trailingInset, leadingInset: leadingInset, display: main.display,
                             spots: app.spots,
                             readout: main.meterReadout,
                             firstKeyRefused: BandSlicesModel.refusedForTransmitChoice(transmit.ptt),
                             flagControls: main.flagControls,
                             transmitReason: BandGestureLayer.transmitReason(transmit, take: main.take),
                             take: main.take)
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
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
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
