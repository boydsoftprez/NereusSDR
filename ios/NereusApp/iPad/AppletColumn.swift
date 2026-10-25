// NereusSDR for iOS: the iPad's applet column on its side: the analog S-meter at its head, then RX, then TX with the amp and tuner
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 19, the Roundtable applet second after
// the S-Meter (movable), the column one scroll with the room's box pinned
// at its foot while the applet is in view, and widened to the room list
// plus the room. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// The applet column (spec section 5.6 item 1, D30, picture 18): with the
/// iPad on its side, one column on the band's right like the desktop's main
/// window. The analog S-meter heads it; under it Roundtable (spec 11.4,
/// second unless moved), then RX and then TX (with the amp and tuner), all
/// in one scroll. RX and TX are the phone's panels, laid in the column
/// instead of sliding over the band. While the Roundtable applet is in
/// view its box is pinned at the column's foot. Widened, the column is
/// Roundtable's room list beside the room. The toolbar's corner button
/// hides the column and brings it back.
struct AppletColumn: View {
    @ObservedObject var main: MainScreenModel
    @ObservedObject var rx: RxPanelModel
    @ObservedObject var transmit: TransmitModel
    /// Opens the Radio tab, where an amplifier or tuner is set up.
    var openRadio: () -> Void = {}
    var openSettings: () -> Void = {}
    /// Roundtable's applet; nil leaves it out.
    let roundtable: RoundtableAppletConfig?

    @State private var roundtablePosition: Int
    @StateObject private var roomScroll = RoundtableRoomScroll()
    @State private var viewportHeight: CGFloat = 0
    @State private var appletFrame: CGRect = .zero
    @State private var footHeight: CGFloat = 0

    init(main: MainScreenModel, openRadio: @escaping () -> Void = {}, openSettings: @escaping () -> Void = {},
         roundtable: RoundtableAppletConfig? = nil) {
        self.main = main
        rx = main.rx
        transmit = main.transmit
        self.openRadio = openRadio
        self.openSettings = openSettings
        self.roundtable = roundtable
        _roundtablePosition = State(initialValue: roundtable?.settings.roundtableAppletPosition
                                    ?? AppletOrder.roundtableDefault)
    }

    static let width = IPadLayout.columnWidth
    /// Widened: the room list and the room (the board's).
    static let wideWidth: CGFloat = 640
    /// The column's scroll, for where the applet and its messages are.
    nonisolated static let space = "appletColumn"

    private var wide: Bool { roundtable?.wide.wrappedValue ?? false }

    private var order: [AppletKind] {
        let kinds = AppletOrder.order(roundtableAt: roundtablePosition)
        return roundtable == nil ? kinds.filter { $0 != .roundtable } : kinds
    }

    private var pinned: Bool {
        guard let roundtable, roundtable.roundtable.room != nil, !roundtable.roundtable.askingFirstRun,
              roundtable.roundtable.coreReason == nil else {
            return false
        }
        return RoundtableAppletFoot.boxPinned(applet: appletFrame, viewportHeight: viewportHeight)
    }

    var body: some View {
        Group {
            if wide, let roundtable {
                RoundtableWideApplet(roundtable: roundtable.roundtable, transmit: transmit, slices: main.slices,
                                     stationCallsign: roundtable.stationCallsign,
                                     toggleWide: { roundtable.wide.wrappedValue = false },
                                     firstRunNoteSeen: roundtable.firstRunNoteSeen,
                                     askFirstRun: roundtable.askFirstRun)
            } else {
                column
            }
        }
        .frame(width: wide ? Self.wideWidth : Self.width)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(ChromeColours.panel)
        .overlay(alignment: .leading) {
            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
        }
        .onAppear { syncSlice() }
        .onChange(of: main.slices.activeSliceId) { syncSlice() }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Applets")
        .accessibilityIdentifier("appletColumn")
    }

    private var column: some View {
        ScrollViewReader { reader in
            ScrollView {
                VStack(spacing: 0) {
                    ForEach(order, id: \.self) { kind in
                        applet(kind, reader: reader)
                    }
                }
            }
            .coordinateSpace(name: Self.space)
            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { viewportHeight = $0 }
            .safeAreaInset(edge: .bottom, spacing: 0) {
                if pinned, let roundtable {
                    RoundtableAppletFoot(roundtable: roundtable.roundtable, transmit: transmit, scroll: roomScroll,
                                         stationCallsign: roundtable.stationCallsign,
                                         jump: { id in withAnimation { reader.scrollTo(id, anchor: .center) } },
                                         toEnd: {
                                             withAnimation {
                                                 reader.scrollTo(RoundtableRoomView.endId, anchor: .bottom)
                                             }
                                         })
                        .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { footHeight = $0 }
                        .onDisappear { footHeight = 0 }
                }
            }
            .overlay {
                // The column scrolls both panels, so it shows their number pad.
                ValuePadLayer(pad: rx.pad ?? transmit.pad)
            }
            .onChange(of: roundtable?.scrollRequest ?? 0) {
                withAnimation { reader.scrollTo(RoundtableApplet.id, anchor: .top) }
            }
        }
    }

    @ViewBuilder
    private func applet(_ kind: AppletKind, reader: ScrollViewProxy) -> some View {
        switch kind {
        case .sMeter:
            AnalogSMeter(model: main.sMeter)
        case .roundtable:
            if let roundtable {
                let position = order.firstIndex(of: .roundtable) ?? AppletOrder.roundtableDefault
                RoundtableApplet(roundtable: roundtable.roundtable, transmit: transmit, slices: main.slices,
                                 stationCallsign: roundtable.stationCallsign, wide: false,
                                 toggleWide: { roundtable.wide.wrappedValue = true },
                                 moveUp: position > 0 ? { move(to: position - 1) } : nil,
                                 moveDown: position < AppletOrder.lastPosition ? { move(to: position + 1) } : nil,
                                 embedding: RoundtableRoomEmbedding(reader: reader, scroll: roomScroll, space: Self.space,
                                                                    visibleHeight: viewportHeight
                                                                        - (pinned ? footHeight : 0)),
                                 firstRunNoteSeen: roundtable.firstRunNoteSeen, askFirstRun: roundtable.askFirstRun)
                    .id(RoundtableApplet.id)
                    .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(Self.space)) } action: { appletFrame = $0 }
            }
        case .rx:
            RxPanel(model: rx, sliceColour: main.sliceColour, width: nil, scrolls: false)
        case .tx:
            TxPanel(transmit: transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes,
                    meters: main.band.catalog?.meters, openRadio: openRadio, width: nil, scrolls: false,
                    modMonitor: main.modMonitor, take: main.take, openSettings: openSettings)
        }
    }

    private func move(to position: Int) {
        let clamped = AppletOrder.clamped(position)
        roundtablePosition = clamped
        roundtable?.settings.roundtableAppletPosition = clamped
    }

    private func syncSlice() {
        if let roundtable {
            roundtable.roundtable.activeSliceId = main.slices.activeSliceId.map(Int64.init)
        }
    }
}
