// NereusSDR for iOS: the tab bar and the screen each tab opens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The app's root: the connecting screens until a Core is connected (spec
/// section 5.3), then the screen of the chosen tab over NereusSDR's own tab
/// bar (Panadapter, Modes, Tools, Radio, Setup; spec section 5.2). Each
/// tab's screen stays alive while another is shown, so it comes back as it
/// was left. Each screen task draws its tab's content here. The Core's open
/// question about other devices is asked over all of them, from here.
struct RootView: View {
    @EnvironmentObject private var model: AppModel
    @EnvironmentObject private var flow: ConnectionFlow
    @State private var selection: AppTab
    @Environment(\.horizontalSizeClass) private var widthClass
    /// The Setup tab's stack, so the Display sheet can open Display's page.
    @StateObject private var setupRouter = SetupRouter()
    @StateObject private var toolsRouter = ToolsRouter()
    @StateObject private var txSettingsRouter = TxSettingsRouter()

    /// `selection` is the tab shown first, Panadapter unless a test asks otherwise.
    init(selection: AppTab = .panadapter) {
        _selection = State(initialValue: selection)
    }

    var body: some View {
        Group {
            switch flow.screen {
            case .welcome:
                WelcomeScreen(flow: flow)
            case .setUpCore:
                SetUpCoreScreen(flow: flow)
            case .findCore:
                if flow.nearby.isEmpty {
                    FindStationScreen(flow: flow)
                } else {
                    FoundItScreen(flow: flow)
                }
            case .cores:
                YourStationsScreen(flow: flow)
            case .typeAddress:
                TypeAddressScreen(flow: flow)
            case .addresses:
                AddressesScreen(flow: flow)
            case .pairByCode:
                PairByCodeScreen(flow: flow)
            case .microphone:
                MicrophoneQuestionScreen(flow: flow)
            case .band:
                tabs
            }
        }
        .overlay(alignment: .bottom) {
            if let held = flow.heldQuestion {
                ZStack(alignment: .bottom) {
                    Color.black.opacity(0.55).ignoresSafeArea().accessibilityHidden(true)
                    FifthDeviceSheet(flow: flow, question: held)
                }
            }
        }
    }

    private var tabs: some View {
        GeometryReader { proxy in
            let sideways = proxy.size.width > proxy.size.height
            VStack(spacing: 0) {
                ZStack {
                    ForEach(AppTab.allCases) { tab in
                        let shown = tab == selection
                        content(for: tab)
                            .overlay(alignment: .topTrailing) {
                                // While this phone transmits, every other tab's title
                                // bar carries the TX pill; the Panadapter's PTT shows it.
                                if tab != .panadapter {
                                    TxPill(transmit: model.main.transmit)
                                        .padding(.top, 7)
                                        .padding(.trailing, 12)
                                }
                            }
                            .opacity(shown ? 1 : 0)
                            .allowsHitTesting(shown)
                            .accessibilityHidden(!shown)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                TabBar(selection: $selection, sideways: sideways,
                       iPad: IPadLayout.arrangement(horizontal: widthClass, size: proxy.size).isIPad)
                    // The Core's open question, over whichever tab is showing (D85).
                    .background(ConfirmationWindowAnchor(app: model))
            }
            .environment(\.uprightBandWidth, proxy.uprightBandWidth)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .onAppear {
            txSettingsRouter.selectModes = { selection = .modes }
            txSettingsRouter.selectPanadapter = { selection = .panadapter }
        }
        // Keep the screen on counts only while the band itself is showing.
        .onChange(of: selection, initial: true) { _, tab in
            model.longSession.bandShown(tab == .panadapter)
        }
        .onDisappear {
            model.longSession.bandShown(false)
        }
    }

    @ViewBuilder
    private func content(for tab: AppTab) -> some View {
        switch tab {
        case .panadapter:
            // More display options in Setup opens Setup's Display page (D79).
            MainScreen(app: model, main: model.main, flow: flow, openSetup: {
                setupRouter.open(.displayOnThisPhone)
                selection = .setup
            },
                       openRadio: { selection = .radio },
                       openPerformance: {
                           toolsRouter.openPerformance()
                           selection = .tools
                       }, openTransmitSettings: { txSettingsRouter.openTransmit() })
        case .setup:
            SetupTab(app: model, flow: flow, router: setupRouter)
                .onAppear {
                    // Setup's buttons that open another page (V15).
                    // The routers hold nothing of the model's, so no cycle.
                    let setupRouter = setupRouter
                    let toolsRouter = toolsRouter
                    model.phoneNavigation = PhoneNavigation(
                        openSetupPage: { pageId in setupRouter.open(describedPage: pageId) },
                        openTxEqualizer: {
                            toolsRouter.openTxEqualizer()
                            selection = .tools
                        })
                }
        case .tools:
            ToolsTab(app: model, flow: flow, spots: model.spots,
                     router: toolsRouter, isActive: selection == .tools)
        case .radio:
            RadioView(app: model, main: model.main, flow: flow, isActive: selection == .radio)
        case .modes:
            ModesTab(app: model, main: model.main, flow: flow, txRouter: txSettingsRouter)
        }
    }
}

#Preview {
    let model = AppModel()
    RootView()
        .environmentObject(model)
        .environmentObject(ConnectionFlow.live(app: model, kind: .phone))
        .preferredColorScheme(.dark)
}
