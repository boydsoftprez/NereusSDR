// NereusSDR for iOS: the Radio tab: the Core and link, the radio at a glance, the accessories and the rest of the Radio menu
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Radio tab (spec section 5.2 item 5, picture 05, the board's
/// `tpl-radio`). It opens with the Core and link: the Core's name (its
/// address when it has none), how it is reached and the round-trip time,
/// and Disconnect, which ends the session and shows Your Cores. Under it
/// the radio at a glance (``RadioAtAGlanceSection``), then the Core's
/// accessories, each with a one-line status that opens its page (Task 60),
/// then the rest of the desktop's Radio menu as the Core offers it
/// (``RadioMoreSection``): Antenna Setup opens the Core's own antenna page
/// from its Setup description, Manage Radios the radios the Core can see,
/// Protocol Info the Core's radio's details. Each page has its way back.
struct RadioView: View {
    /// A page open over the tab.
    enum Route: Hashable {
        case accessory(AccessoriesSection.Route)
        case antennaSetup
        case manageRadios
        case protocolInfo

        /// The page's title on the bar, and the next page's way back.
        var title: String {
            switch self {
            case .accessory(let page): return page.title
            case .antennaSetup: return "Antenna Setup"
            case .manageRadios: return "Manage Radios"
            case .protocolInfo: return "Protocol Info"
            }
        }

        init(_ page: RadioMenu.Page) {
            switch page {
            case .antennaSetup: self = .antennaSetup
            case .manageRadios: self = .manageRadios
            case .protocolInfo: self = .protocolInfo
            }
        }
    }

    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    @ObservedObject var flow: ConnectionFlow
    /// Disconnect is not offered while this phone transmits.
    @ObservedObject private var transmit: TransmitModel
    @StateObject private var radio: RadioTabModel
    @StateObject private var radios: ManageRadiosModel
    /// The number pad Antenna Setup's number rows open.
    @StateObject private var pads = ValuePadHost()
    /// The pages open over the tab, newest last.
    @State private var route: [Route]
    /// The tab is the one showing (the root keeps every tab alive, hidden).
    var isActive: Bool
    @Environment(\.scenePhase) private var scenePhase

    init(app: AppModel, main: MainScreenModel, flow: ConnectionFlow,
         accessoryRoute: [AccessoriesSection.Route] = [], route: [Route] = [], isActive: Bool = true) {
        self.app = app
        self.main = main
        self.flow = flow
        self.isActive = isActive
        transmit = main.transmit
        _radio = StateObject(wrappedValue: RadioTabModel(app: app))
        _radios = StateObject(wrappedValue: ManageRadiosModel(app: app))
        _route = State(initialValue: accessoryRoute.map(Route.accessory) + route)
    }

    var body: some View {
        page
            .onChange(of: onScreen, initial: true) { _, _ in syncLifecycle() }
            // Another page opening closes an open pad, sending nothing.
            .onChange(of: route) { _, _ in pads.close() }
            .onDisappear {
                radio.hide()
                radios.close()
            }
    }

    /// What is on screen now: the tab in front with the app active, and which page.
    private var onScreen: [Route]? {
        isActive && scenePhase == .active ? route : nil
    }

    /// Readings tick only while the tab's own page is on screen; Manage
    /// Radios asks for its list only while it is on screen.
    static func lifecycle(onScreen: [Route]?) -> (readings: Bool, radioList: Bool) {
        (onScreen?.isEmpty == true, onScreen?.last == .manageRadios)
    }

    private func syncLifecycle() {
        let wanted = Self.lifecycle(onScreen: onScreen)
        if wanted.readings { radio.show() } else { radio.hide() }
        if wanted.radioList { radios.open() } else { radios.close() }
    }

    @ViewBuilder
    private var page: some View {
        let link = LinkState(connection: app.connection, roundTripMs: app.roundTripMs, offline: flow.offline,
                             relayed: app.linkRelayed)
        let core = RadioCoreCard.name(coreName: main.coreName, coreHost: app.coreHost)
        let back = route.dropLast().last?.title ?? "Radio"
        switch route.last {
        case .accessory(let page)?:
            AccessoryScreen(model: main.accessories, transmit: main.transmit, route: page, backTitle: back,
                            coreName: core,
                            back: { route.removeLast() }, open: { route.append(.accessory($0)) }) {
                LinkChip(link: link, core: core)
            }
        case .antennaSetup?:
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: Route.antennaSetup.title, back: back, onBack: { route.removeLast() }) {
                    SetupTagBadge(tag: .core)
                }
                DescribedPage(pages: app.setupPages, dispatcher: app.setupControls,
                              category: RadioTabModel.antennaCategory, pageId: RadioTabModel.antennaPage,
                              specialized: .live(app))
            }
            .background(ChromeColours.page.ignoresSafeArea(edges: [.top, .horizontal]))
            // A number row's typed value: its pad at the foot of the page.
            .valuePads(pads)
        case .manageRadios?, .protocolInfo?:
            let page = route.last ?? .protocolInfo
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: page.title, back: back, onBack: { route.removeLast() }) {
                    LinkChip(link: link, core: core)
                }
                ScrollView {
                    Group {
                        if page == .manageRadios {
                            ManageRadiosPage(model: radios)
                        } else {
                            ProtocolInfoPage(model: radio, now: { app.mirrorClock.nowMilliseconds })
                        }
                    }
                    .padding(12)
                    .padding(.bottom, 8)
                }
            }
            .background(ChromeColours.page.ignoresSafeArea(edges: [.top, .horizontal]))
        case nil:
            tab(link: link, core: core)
        }
    }

    private func tab(link: LinkState, core: String) -> some View {
        let selectedID = flow.selectedCoreIdentity
        return VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Radio") {
                LinkChip(link: link, core: core)
                    .accessibilityIdentifier("radioLink")
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    RadioCoreCard(name: core, status: RadioCoreCard.status(link),
                                  canDisconnect: !flow.transmitting && !flow.disconnecting,
                                  removeReason: flow.radioRemoveReason,
                                  disconnect: { Task { await flow.disconnect() } },
                                  remove: {
                                      if let selectedID { flow.requestRemove(identityKey: selectedID) }
                                  })
                    RadioAtAGlanceSection(model: radio, now: { app.mirrorClock.nowMilliseconds })
                    AccessoriesSection(model: main.accessories) { route.append(.accessory($0)) }
                    RadioMoreSection(entries: radio.menu) { route.append(Route($0)) }
                }
                .padding(12)
                .padding(.bottom, 8)
            }
        }
        .background(ChromeColours.page.ignoresSafeArea(edges: [.top, .horizontal]))
        .alert("Remove Core?", isPresented: Binding(
            get: { flow.removeCandidate != nil },
            set: { if !$0 { flow.cancelRemove() } }
        )) {
            Button("Remove Core", role: .destructive) { flow.confirmRemove() }
            Button("Cancel", role: .cancel) { flow.cancelRemove() }
        } message: {
            Text(ConnectionFlow.removeConfirmationText)
        }
    }
}
