// NereusSDR for iOS: the tab bar and the screen each tab opens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the UI tests' record of the hold
// PTT. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 18, the swipe up from the tab bar, and
// the swipes' pages on the Tools tab sliding in from the swipe's side.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the swipe up from the tab bar on an
// iPad too. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import NereusBand
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
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    /// A drag on the tab bar is the swipe up, or nothing of the swipes'.
    @State private var tabBarSwiping: Bool?
    @GestureState private var tabBarDragging = false
    /// The Tools tab sliding in from a swipe's side.
    @State private var toolsSlide = CGSize.zero

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
    /// On an iPhone a link never chooses a pan the strip greys there (it
    /// shows one pan): Pan 1 is chosen in its place.
    static func navigation(setupRouter: SetupRouter, toolsRouter: ToolsRouter,
                           selectSetup: @escaping () -> Void, selectTools: @escaping () -> Void,
                           isPhone: Bool = UIDevice.current.userInterfaceIdiom == .phone) -> PhoneNavigation {
        func shown(_ pan: PanIdentity) -> PanIdentity {
            DisplaySettingsStrip.greyed(pan, isPhone: isPhone) ? .first : pan
        }
        var navigation = PhoneNavigation(
            openSetupPage: { pageId in setupRouter.open(describedPage: pageId) },
            openTxEqualizer: {
                toolsRouter.openTxEqualizer()
                selectTools()
            })
        navigation.openDisplayPage = { pageId, scope in
            var chosen = scope
            if case .pan(let pan) = scope {
                chosen = .pan(shown(pan))
            }
            guard setupRouter.open(describedPage: pageId, scope: chosen) else { return false }
            selectSetup()
            return true
        }
        navigation.followDisplayPan = { pan in setupRouter.followPill(shown(pan)) }
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
            let iPad = IPadLayout.arrangement(horizontal: widthClass, size: proxy.size).isIPad
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
                            .offset(tab == .tools ? toolsSlide : .zero)
                            .opacity(shown ? 1 : 0)
                            .allowsHitTesting(shown)
                            .accessibilityHidden(!shown)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                TabBar(selection: $selection, sideways: sideways, iPad: iPad)
                    // The Core's open question, over whichever tab is showing (D85).
                    .background(ConfirmationWindowAnchor(app: model))
                    // Up from the tab bar, on the Panadapter tab (spec 11.3, S1); on an iPad
                    // it widens the Roundtable applet (spec 11.4, S2).
                    .simultaneousGesture(tabBarSwipe(on: selection == .panadapter))
                    .onChange(of: tabBarDragging) { _, now in
                        // A swipe cut short (the touch taken away) springs back.
                        if !now, tabBarSwiping != nil {
                            if tabBarSwiping == true {
                                model.swipes.cancel(reduceMotion: reduceMotion)
                            }
                            tabBarSwiping = nil
                        }
                    }
            }
            .environment(\.uprightBandWidth, proxy.uprightBandWidth)
            .onReceive(model.swipes.$opened) { opened in
                if let opened, opened.target.opensTools {
                    openTools(opened, size: proxy.size)
                }
            }
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
            // The swipes work on the Panadapter tab only.
            model.swipes.panadapterShown = tab == .panadapter
            if tab != .panadapter {
                model.swipes.cancel(reduceMotion: true)
            }
        }
        .onDisappear {
            model.longSession.bandShown(false)
        }
    }

    /// The swipe up from the tab bar: an upward drag more up than across
    /// begins it, and it follows the finger until it lifts.
    private func tabBarSwipe(on: Bool) -> some Gesture {
        DragGesture(minimumDistance: TuneGestures.dragThresholdPoints, coordinateSpace: .global)
            .updating($tabBarDragging) { _, state, _ in
                state = true
            }
            .onChanged { value in
                let swipes = model.swipes
                if tabBarSwiping == nil {
                    tabBarSwiping = on && SwipeShortcuts.claims(.tabBar, translation: value.translation)
                        && swipes.begin(.tabBar)
                }
                if tabBarSwiping == true {
                    swipes.move(SwipeShortcuts.inward(.tabBar, value.translation))
                }
            }
            .onEnded { value in
                let swiping = tabBarSwiping == true
                tabBarSwiping = nil
                if swiping {
                    model.swipes.end(distance: SwipeShortcuts.inward(.tabBar, value.translation),
                                     predicted: SwipeShortcuts.inward(.tabBar, value.predictedEndTranslation),
                                     reduceMotion: reduceMotion)
                }
            }
    }

    /// A swipe opened a page on the Tools tab: the tab shows it, sliding in
    /// from the side the swipe came from.
    private func openTools(_ opened: SwipeShortcutModel.Opened, size: CGSize) {
        switch opened.target {
        case .roundtableAll:
            toolsRouter.openRoundtable()
        case .spotHub:
            toolsRouter.openSpotHub()
        case .tools:
            toolsRouter.openList()
        case .off, .rxPanel, .txPanel, .roundtableMine:
            return
        }
        model.swipes.finish()
        guard !reduceMotion else {
            selection = .tools
            return
        }
        switch opened.origin.edge {
        case .leading:
            toolsSlide = CGSize(width: -size.width, height: 0)
        case .trailing:
            toolsSlide = CGSize(width: size.width, height: 0)
        default:
            toolsSlide = CGSize(width: 0, height: size.height)
        }
        selection = .tools
        withAnimation(.easeOut(duration: 0.22)) {
            toolsSlide = .zero
        }
    }

    /// The UI tests' record of the hold PTT, in a debug build that asks for it.
    @MainActor
    private static var holdObserver: ((PttHold.Action) -> Void)? {
        #if DEBUG
        guard let log = UITestBand.holdLog else {
            return nil
        }
        return { action in log.record(action) }
        #else
        return nil
        #endif
    }

    @ViewBuilder
    private func content(for tab: AppTab) -> some View {
        switch tab {
        case .panadapter:
            // More display options in Setup opens Setup's Display page (D79).
            MainScreen(app: model, main: model.main, flow: flow, holdObserver: Self.holdObserver,
                       swipes: model.swipes, openSetup: {
                Self.openDisplaySetup(setupRouter, app: model)
                selection = .setup
            },
                       openRadio: { selection = .radio },
                       openPerformance: {
                           toolsRouter.openPerformance()
                           selection = .tools
                       }, openTransmitSettings: { txSettingsRouter.openTransmit() })
            #if DEBUG
            .overlay(alignment: .topLeading) {
                if let log = UITestBand.holdLog {
                    UITestHoldLog.Label(log: log)
                }
                if let log = UITestBand.keyLog {
                    UITestKeyLog.Label(log: log)
                        .offset(x: 2)
                }
            }
            #endif
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
