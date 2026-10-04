// NereusSDR for iOS: the Modes tab: one scrolling page of the active slice's full control set
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Modes tab (R-IOS-18, D15, D17, spec section 5.2 item 1, the board's
/// `tpl-modes` and picture 05): one scrolling page for the active slice,
/// top to bottom: the slice switch, the mode, the filter, the front end,
/// AGC, noise, audio, RIT and XIT, DIG and RTTY, then transmit. A typed
/// value opens the number pad over the page. Each control writes to
/// its owner at the Core (``ModesTabModel``); the RX and TX panels on the
/// band keep a quick subset of the same controls.
struct ModesTab: View {
    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    @ObservedObject var flow: ConnectionFlow
    @ObservedObject private var model: ModesTabModel
    @ObservedObject private var rx: RxPanelModel
    @ObservedObject private var txRouter: TxSettingsRouter

    init(app: AppModel, main: MainScreenModel, flow: ConnectionFlow, txRouter: TxSettingsRouter? = nil) {
        self.app = app
        self.main = main
        self.flow = flow
        model = main.modes
        rx = main.rx
        self.txRouter = txRouter ?? TxSettingsRouter()
    }

    var body: some View {
        let link = LinkState(connection: app.connection, roundTripMs: app.roundTripMs, offline: flow.offline,
                             relayed: app.linkRelayed)
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: AppTab.modes.title,
                                 back: txRouter.returnsToPanel ? "TX panel" : nil,
                                 onBack: { txRouter.backToPanel() }, backIdentifier: "modesBackToTx", backHeight: 44) {
                LinkChip(link: link, core: main.coreName ?? app.coreHost)
                    .accessibilityIdentifier("modesLink")
            }
            ScrollViewReader { proxy in
                ScrollView {
                    ModesPage(model: model, micLevel: main.micLevel)
                }
                .onChange(of: txRouter.focusSerial) { _, _ in
                    proxy.scrollTo("modesTransmitSection", anchor: .top)
                }
            }
            .accessibilityIdentifier("modesPage")
            // A typed value's number pad, over the page.
            .overlay {
                ValuePadLayer(pad: model.pad)
            }
            .overlay(alignment: .bottom) {
                // The Core's words for the last change it refused, wherever the page is scrolled.
                if let note = model.note ?? rx.note {
                    Text(note)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.refusalText)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.horizontal, 12)
                        .padding(.vertical, 8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(ChromeColours.notice)
                        .overlay(alignment: .top) {
                            Rectangle().fill(ChromeColours.barBorder).frame(height: 1)
                        }
                        .accessibilityIdentifier("modesNote")
                }
            }
        }
        .background(ChromeColours.panel.ignoresSafeArea(edges: [.top, .horizontal]))
    }
}

/// The page itself, the sections in the board's order, for the tab and
/// for its pictures.
struct ModesPage: View {
    @ObservedObject var model: ModesTabModel
    /// The mic level meter's own level before keying.
    let micLevel: LiveMicLevel

    var body: some View {
        ModesPageBody(model: model, slices: model.slices, micLevel: micLevel)
    }
}

/// The page's sections under the slice switch. While the active slice is
/// one this phone only listens to (R-IOS-42, JJ's rulings of 2026-09-30),
/// the owner block sits under the switch with the Core's line, Stop
/// listening, Take control and this phone's own volume, and every section
/// that would change the slice is greyed; a tap on one nudges the block.
private struct ModesPageBody: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var slices: BandSlicesModel
    let micLevel: LiveMicLevel

    @State private var nudged = false

    var body: some View {
        let listened = slices.active.flatMap { $0.listening ? $0 : nil }
        let grey = listened.map { _ in true } ?? false
        VStack(alignment: .leading, spacing: 0) {
            SliceSwitch(model: model)
            if let listened {
                ModesOwnerBlock(slices: slices, entry: listened, nudged: nudged)
                    .padding(.top, 8)
            }
            Group {
                ModePicker(model: model)
                FilterSection(model: model, rx: model.rx)
                FrontEndSection(model: model)
                AgcSection(model: model, rx: model.rx)
                NoiseSection(model: model, rx: model.rx)
                AudioSection(model: model, rx: model.rx)
                RitXitSection(model: model)
                DigitalSection(model: model)
            }
            .modifier(ModesGrey(grey: grey, words: listened?.ownerLine) { nudge() })
            TransmitSection(model: model, transmit: model.transmit, micLevel: micLevel)
                .id("modesTransmitSection")
        }
        .padding(.bottom, 8)
    }

    private func nudge() {
        nudged = true
        Task { @MainActor in
            try? await Task.sleep(for: .seconds(0.9))
            nudged = false
        }
    }
}

/// Each greyed section: 0.4, never hidden, its tap nudging the owner block.
private struct ModesGrey: ViewModifier {
    let grey: Bool
    let words: String?
    let nudge: () -> Void

    func body(content: Content) -> some View {
        if grey {
            content
                .allowsHitTesting(false)
                .opacity(VfoFlagView.dimmed)
                .overlay {
                    Color.black.opacity(0.001)
                        .contentShape(Rectangle())
                        .onTapGesture(perform: nudge)
                        .accessibilityHidden(true)
                }
                .accessibilityHint(words ?? "")
        } else {
            content
        }
    }
}

/// A Settings trip focuses Modes' Transmit section and remembers its Back route.
@MainActor
final class TxSettingsRouter: ObservableObject {
    @Published private(set) var focusSerial: UInt64 = 0
    @Published private(set) var returnsToPanel = false
    var selectModes: () -> Void = {}
    var selectPanadapter: () -> Void = {}

    func openTransmit() {
        returnsToPanel = true
        focusSerial &+= 1
        selectModes()
    }
    func backToPanel() {
        guard returnsToPanel else { return }
        returnsToPanel = false
        selectPanadapter()
    }
}
