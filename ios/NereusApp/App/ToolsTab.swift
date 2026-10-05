// NereusSDR for iOS: the Tools tab and the pages it opens, in the desktop's order from the Core's tool list
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

@MainActor
final class ToolsRouter: ObservableObject {
    @Published private(set) var performanceRequest: UInt64 = 0
    func openPerformance() { performanceRequest &+= 1 }
    @Published private(set) var txEqualizerRequest: UInt64 = 0
    /// Opens the TX Equalizer page, as Setup's Speech Processor buttons do.
    func openTxEqualizer() { txEqualizerRequest &+= 1 }
}

/// The Tools tab (spec section 5.2 items 3 to 5, R-IOS-18, D41, D42): the
/// desktop's Tools menu from the Core's own list (``StationToolList``), in
/// its order, each tool marked with where it runs, following the Core's
/// catalogue while the tab is open. Spot Hub opens its page and the pages
/// under it, each with its way back (R-IOS-25, D32); FreeDV Reporter opens
/// its station list (R-IOS-26), and Spot Hub's FreeDV row opens the Core's
/// reporter connection. TX Equalizer, PureSignal, Diversity, TCI Server,
/// VAX Audio and Support Bundle open their own pages. An older Core's
/// notice sits at the top.
struct ToolsTab: View {
    @ObservedObject var app: AppModel
    @ObservedObject var flow: ConnectionFlow
    @ObservedObject var spots: SpotsModel
    @ObservedObject private var freedv: FreeDVReporterModel
    @ObservedObject private var performance: ConnectionPerformanceModel
    @ObservedObject var router: ToolsRouter
    @StateObject private var list: ToolListModel
    var isActive: Bool
    /// The pages open over the tab: Spot Hub, then one under it.
    @State private var route: [Page]
    @State private var askingToClear = false

    enum Page: Hashable {
        case spotHub
        case spotHubPage(SpotHubPage.Route)
        case performance
        case freedvReporter
        case txEqualizer
        case pureSignal
        case diversity
        case tciServer
        case vaxAudio
        case supportBundle

        var title: String {
            switch self {
            case .spotHub: return "Spot Hub"
            case .spotHubPage(let page): return page.title
            case .performance: return "Connection and performance"
            case .freedvReporter: return "FreeDV Reporter"
            case .txEqualizer: return "TX Equalizer"
            case .pureSignal: return "PureSignal"
            case .diversity: return "Diversity"
            case .tciServer: return "TCI Server"
            case .vaxAudio: return "VAX Audio"
            case .supportBundle: return "Support Bundle"
            }
        }

        /// The page a row of the list opens.
        init(_ page: StationToolList.Page) {
            switch page {
            case .spotHub: self = .spotHub
            case .freedvReporter: self = .freedvReporter
            case .txEqualizer: self = .txEqualizer
            case .pureSignal: self = .pureSignal
            case .diversity: self = .diversity
            case .tciServer: self = .tciServer
            case .vaxAudio: self = .vaxAudio
            case .performance: self = .performance
            case .supportBundle: self = .supportBundle
            }
        }
    }

    init(app: AppModel, flow: ConnectionFlow, spots: SpotsModel,
         router: ToolsRouter = ToolsRouter(), isActive: Bool = true, route: [Page] = []) {
        self.app = app
        self.flow = flow
        self.spots = spots
        freedv = app.freedv
        performance = app.connectionPerformance
        self.router = router
        _list = StateObject(wrappedValue: ToolListModel(mirror: app.mirror, catalogFeed: app.main.catalogFeed))
        self.isActive = isActive
        _route = State(initialValue: route)
    }

    var body: some View {
        let link = LinkState(connection: app.connection, roundTripMs: app.roundTripMs, offline: flow.offline,
                             relayed: app.linkRelayed)
        let core = RadioCoreCard.name(coreName: app.main.coreName, coreHost: app.coreHost)
        VStack(spacing: 0) {
            if let page = route.last {
                ConnectChrome.NavBar(title: page.title, back: route.dropLast().last?.title ?? "Tools",
                                     onBack: { route.removeLast() }) {
                    trailing(page, link: link, core: core)
                }
            } else {
                ConnectChrome.NavBar(title: "Tools") {
                    LinkChip(link: link, core: core)
                }
            }
            if route.last == .performance {
                ConnectionPerformanceView(input: performance.input,
                    onSelectRange: { performance.selectRange($0) },
                    onResetSessionStats: { performance.resetSessionStats() })
                    .onAppear { if isActive { performance.open(attempt: flow.attempt) } }
                    .onDisappear { performance.close() }
            } else if route.last == .freedvReporter {
                // The list scrolls under its filters, with your status below it.
                FreeDVReporterPage(freedv: freedv)
            } else if let page = route.last, Self.stationPages.contains(page) {
                stationPage(page)
            } else {
                ScrollView {
                    content
                        .padding(12)
                        .padding(.bottom, 8)
                }
            }
        }
        .background(ChromeColours.page.ignoresSafeArea(edges: [.top, .horizontal]))
        .onChange(of: router.txEqualizerRequest) { _, _ in
            route = [.txEqualizer]
        }
        .onChange(of: router.performanceRequest) { _, _ in
            route = [.performance]
            if isActive { performance.open(attempt: flow.attempt) }
        }
        .onChange(of: isActive) { _, shown in
            if route.last == .performance {
                if shown { performance.open(attempt: flow.attempt) } else { performance.close() }
            }
        }
        .onChange(of: flow.attempt) { _, next in performance.setAttempt(next) }
        .confirmationDialog("Clear all spots?", isPresented: $askingToClear, titleVisibility: .visible) {
            Button("Clear all spots", role: .destructive) {
                spots.clearAll()
            }
        } message: {
            Text("This clears the Core's spots for every device.")
        }
    }

    @ViewBuilder
    private var content: some View {
        switch route.last {
        case nil:
            VStack(alignment: .leading, spacing: 10) {
                if let older = flow.olderCore {
                    // An older Core (D23): what it can't do is greyed where it sits.
                    OlderCoreNotice(core: older)
                }
                SpotHubPage.Card {
                    ForEach(Array(list.entries.enumerated()), id: \.element.id) { index, entry in
                        if index > 0 {
                            SpotHubPage.Line()
                        }
                        row(entry)
                    }
                }
            }
        case .spotHub?:
            SpotHubPage(spots: spots) { route.append(.spotHubPage($0)) }
        case .spotHubPage(.list)?:
            SpotListPage(spots: spots)
        case .spotHubPage(.display)?:
            SpotDisplayPage(spots: spots)
        case .spotHubPage(.source(.freeDv))?:
            FreeDVSourcePage(freedv: freedv, spots: spots)
        case .spotHubPage(.source(let source))?:
            SpotSourcePage(spots: spots, source: source)
        case .spotHubPage(.identity)?:
            SpotIdentityPage(spots: spots)
        case .performance?, .freedvReporter?, .txEqualizer?, .pureSignal?, .diversity?, .tciServer?, .vaxAudio?,
             .supportBundle?:
            EmptyView()
        }
    }

    /// The pages of the Core's tools, each with its own scrolling and number pad.
    private static let stationPages: Set<Page> = [.txEqualizer, .pureSignal, .diversity, .tciServer, .vaxAudio,
                                                  .supportBundle]

    @ViewBuilder
    private func stationPage(_ page: Page) -> some View {
        switch page {
        case .txEqualizer:
            TxEqualizerScreen(model: TxEqualizerModel(mirror: app.mirror, transmit: app.main.transmit,
                                                       commands: app.commands))
        case .pureSignal:
            ToolScreen(model: PureSignalModel(mirror: app.mirror, transmit: app.main.transmit,
                                              catalogFeed: app.main.catalogFeed)) {
                PureSignalPage(model: $0)
            }
        case .diversity:
            ToolScreen(model: app.diversity) {
                DiversityPage(model: $0)
            }
        case .tciServer:
            ToolScreen(model: TciServerModel(mirror: app.mirror, commands: app.commands, records: app.records),
                       pad: { $0.pad }) {
                TciServerPage(model: $0)
            }
        case .vaxAudio:
            ToolScreen(model: VaxAudioModel(mirror: app.mirror, records: app.records)) {
                VaxAudioPage(model: $0)
            }
        case .supportBundle:
            ToolScreen(model: app.supportBundle) {
                SupportBundlePage(model: $0, log: app.coreLog)
            }
        default:
            EmptyView()
        }
    }

    /// One tool on the list: its line, where it runs, and why it is greyed.
    private func row(_ entry: StationToolList.Entry) -> some View {
        var detail = entry.detail
        var enabled = entry.enabled && entry.page != nil
        if entry.page == .freedvReporter, entry.enabled {
            // FreeDV Reporter opens once the Core runs it.
            detail = freedvDetail
            enabled = freedv.runsReporter
        }
        let identifier = entry.page == .performance ? "tools.connectionPerformance" : "tools.\(entry.id)"
        return SpotHubPage.Row(title: entry.title, detail: detail, tag: entry.tag, enabled: enabled,
                               identifier: identifier) {
            if let page = entry.page {
                route.append(Page(page))
            }
        }
    }

    /// FreeDV Reporter's line on Tools: what it lists, or why it can't open.
    private var freedvDetail: String {
        freedv.runsReporter ? "Stations on the air, as the Core hears them" : FreeDVReporterModel.olderCoreReason
    }

    @ViewBuilder
    private func trailing(_ page: Page, link: LinkState, core: String) -> some View {
        switch page {
        case .spotHubPage(.list):
            // Clear all spots, as the Display page's Clear, asked first.
            Button("Clear") {
                askingToClear = true
            }
            .font(.system(size: 15, weight: .semibold))
            .foregroundStyle(spots.clearReason == nil ? ChromeColours.accent : ChromeColours.buttonOffText)
            .disabled(spots.clearReason != nil)
            .accessibilityLabel("Clear all spots")
            .accessibilityIdentifier("spotList.clear")
        case .spotHubPage(.source), .spotHubPage(.identity):
            SetupTagBadge(tag: .core)
        case .freedvReporter:
            // The desktop's Open Website, in Safari.
            Link("Website", destination: FreeDVReporterModel.website)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(ChromeColours.accent)
                .accessibilityIdentifier("freedv.website")
        case .spotHub, .spotHubPage(.display), .performance, .txEqualizer, .pureSignal, .diversity, .tciServer,
             .vaxAudio, .supportBundle:
            LinkChip(link: link, core: core)
        }
    }
}
