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
    @StateObject private var setupRouter: SetupRouter
    @StateObject private var toolsRouter = ToolsRouter()
    @StateObject private var txSettingsRouter = TxSettingsRouter()

    /// `selection` is the tab shown first, Panadapter unless a test asks
    /// otherwise; `setupRouter` stands in for the Setup tab's stack (the tests').
    init(selection: AppTab = .panadapter, setupRouter: SetupRouter? = nil) {
        _selection = State(initialValue: selection)
        _setupRouter = StateObject(wrappedValue: setupRouter ?? SetupRouter())
    }

    /// The page "More display options in Setup" opens: on a Core whose
    /// Display description reaches version 29, its first page, Spectrum
    /// (spec section 7.1); on an older Core, "On this phone".
    static func openDisplaySetup(_ router: SetupRouter, app: AppModel) {
        if (app.setupPages.categories["display"]?.version ?? 0) >= SetupTree.displayPagesVersion {
            router.open(describedPage: DisplayPillTab.spectrum.setupPageId)
        } else {
            router.open(.displayOnThisPhone)
        }
    }

    /// The Setup and Tools pages other screens open, installed as the root
    /// appears so a link works before the Setup tab has ever been shown.
    /// The routers hold nothing of the model's, so no cycle.
    static func navigation(setupRouter: SetupRouter, toolsRouter: ToolsRouter,
                           selectSetup: @escaping () -> Void, selectTools: @escaping () -> Void) -> PhoneNavigation {
        var navigation = PhoneNavigation(
            openSetupPage: { pageId in setupRouter.open(describedPage: pageId) },
            openTxEqualizer: {
                toolsRouter.openTxEqualizer()
                selectTools()
            })
        navigation.openDisplayPage = { pageId, scope in
            guard setupRouter.open(describedPage: pageId, scope: scope) else { return false }
            selectSetup()
            return true
        }
        return navigation
    }

    private func installNavigation() {
        model.phoneNavigation = Self.navigation(setupRouter: setupRouter, toolsRouter: toolsRouter,
                                                selectSetup: { selection = .setup },
                                                selectTools: { selection = .tools })
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
        // Setup's links from other screens work from the start (V15).
        .onAppear { installNavigation() }
        // Every switch in the app draws in the desktop's workspace look (W7),
        // under the workspace accent (W6); each presentation repeats it.
        .workspacePresentationRoot()
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
            // Back on Setup's Display pages, their rows edit the pans the
            // strip shows again, whatever a pill chose meanwhile.
            if tab == .setup, SetupRouter.inDisplay(setupRouter.path),
               (model.setupPages.categories["display"]?.version ?? 0) >= SetupTree.displayPagesVersion,
               model.main.displayEditScope != setupRouter.displayScope {
                model.main.displayEditScope = setupRouter.displayScope
            }
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
                Self.openDisplaySetup(setupRouter, app: model)
                selection = .setup
            },
                       openRadio: { selection = .radio },
                       openPerformance: {
                           toolsRouter.openPerformance()
                           selection = .tools
                       }, openTransmitSettings: { txSettingsRouter.openTransmit() })
        case .setup:
            SetupTab(app: model, flow: flow, router: setupRouter)
                // Setup's buttons that open another page (V15), as the root installs them.
                .onAppear { installNavigation() }
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
