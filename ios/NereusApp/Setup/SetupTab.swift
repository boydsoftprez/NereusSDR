// NereusSDR for iOS: the Setup tab: Devices first, then the desktop's categories, each marked, and their pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Setup tab (R-IOS-18, D16; spec section 5.2 items 6 to 8, picture
/// 05): the tree ``SetupTree`` lays out, each category with the line of its
/// pages and its mark, then the note on where settings live, then a test
/// build's name. A category opens its list of pages, the app's own and the
/// ones the Core describes (``DescribedPage``); Devices and About this app
/// open their page at once.
struct SetupTab: View {
    @ObservedObject var app: AppModel
    @ObservedObject var main: MainScreenModel
    @ObservedObject var flow: ConnectionFlow
    @ObservedObject var router: SetupRouter
    /// The pages the Core describes, and their controls' owners.
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var controls: SetupControlDispatcher
    /// The band, for whether the Core offers the 3D view.
    @ObservedObject var band: BandModel
    /// The panels for the closed controls a generic row cannot draw.
    var specialized: SetupSpecializedPanels
    @StateObject private var devices: DevicesModel
    @StateObject private var timeOut: TransmitTimeOutModel
    /// A test build's name, shown small at the foot of the list.
    var buildTag: String?

    /// `devices` stands in for the Devices page's own model (the tests'
    /// pictures); `specialized` for the app's closed-control panels.
    init(app: AppModel, flow: ConnectionFlow, router: SetupRouter, buildTag: String? = BuildTag.current,
         devices: DevicesModel? = nil, specialized: SetupSpecializedPanels? = nil) {
        self.app = app
        main = app.main
        self.flow = flow
        self.router = router
        pages = app.setupPages
        controls = app.setupControls
        band = app.main.band
        self.specialized = specialized ?? .live(app)
        self.buildTag = buildTag
        let transmit = app.main.transmit
        _devices = StateObject(wrappedValue: devices ?? DevicesModel(mirror: app.mirror, commands: app.commands,
                                                          catalogFeed: app.main.catalogFeed,
                                                          thisDeviceId: { [weak transmit] in transmit?.thisDeviceId }))
        _timeOut = StateObject(wrappedValue: TransmitTimeOutModel(settings: app.settings))
    }

    /// The note under the tree, naming the Core.
    static func note(coreName: String?) -> String {
        let core = coreName.map { "the Core, \($0)" } ?? "the Core"
        return "Core settings are shared by every device paired with \(core). Settings marked This phone stay on this phone."
    }

    var body: some View {
        NavigationStack(path: $router.path) {
            List {
                Section {
                    ForEach(tree) { category in
                        NavigationLink(value: category.opensPage.map(SetupTree.Route.page)
                                       ?? SetupTree.Route.category(category.id)) {
                            SetupRow(title: category.title, detail: category.summary, tag: category.tag)
                        }
                        .accessibilityIdentifier(category.title)
                    }
                } footer: {
                    Text(Self.note(coreName: main.coreName))
                        .accessibilityIdentifier("setupNote")
                }
                if let buildTag {
                    Section {
                        Text(BuildTag.label(for: buildTag))
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .accessibilityIdentifier("SetupBuildTag")
                    }
                }
            }
            .navigationTitle("Setup")
            .navigationDestination(for: SetupTree.Route.self) { route in
                destination(route)
            }
        }
        // Another page opening closes an open pad, sending nothing.
        .onChange(of: router.path) { _, _ in router.pads.close() }
        // A number row's typed value: its pad at the foot of the tab.
        .valuePads(router.pads)
    }

    /// The tree now: the app's own pages and the ones the Core describes.
    private var tree: [SetupTree.Category] {
        SetupTree.categories(described: pages.categories, order: pages.order, unreadable: pages.unreadable,
                             stackOffered: band.stackOffered, showPaValues: app.paValues.showPage)
    }

    @ViewBuilder
    private func destination(_ route: SetupTree.Route) -> some View {
        switch route {
        case .category(let id):
            if let category = tree.first(where: { $0.id == id }) {
                List {
                    Section {
                        ForEach(category.pages, id: \.destination) { entry in
                            if let reason = entry.unavailableReason {
                                // Listed greyed with its reason, never left out.
                                SetupRow(title: entry.title, detail: reason, tag: entry.tag, unavailable: true)
                                    .accessibilityIdentifier(entry.title)
                            } else {
                                NavigationLink(value: Self.route(entry.destination)) {
                                    SetupRow(title: entry.title, detail: nil, tag: entry.tag)
                                }
                                .accessibilityIdentifier(entry.title)
                            }
                        }
                    } footer: {
                        if let notice = category.notice {
                            Text(notice)
                                .accessibilityIdentifier("setupCategoryNotice")
                        }
                    }
                }
                .navigationTitle(category.title)
                .toolbar {
                    ToolbarItem(placement: .topBarTrailing) {
                        SetupTagBadge(tag: category.tag)
                    }
                }
            }
        case .page(let page):
            self.page(page)
        case .described(let category, let page):
            DescribedPage(pages: pages, dispatcher: controls, category: category, pageId: page,
                          specialized: specialized,
                          levelCal: page == LevelCalSection.pageId ? app.levelCal : nil)
        case .cfcBands(let category, let control):
            CfcBandsEditor(pages: pages, dispatcher: controls, category: category, controlId: control)
        }
    }

    private static func route(_ destination: SetupTree.Destination) -> SetupTree.Route {
        switch destination {
        case .native(let page):
            return .page(page)
        case .described(let category, let page):
            return .described(category: category, page: page)
        }
    }

    @ViewBuilder
    private func page(_ page: SetupTree.Page) -> some View {
        switch page {
        case .devices:
            DevicesPage(model: devices, main: main, flow: flow)
        case .navigation:
            NavigationPage(settings: app.phoneSettings)
        case .batteryAndSessions:
            BatteryAndSessionsPage(settings: app.phoneSettings, sleepTimer: app.longSession.sleepTimer)
        case .audioOnThisPhone:
            AudioOnThisPhonePage(settings: app.phoneSettings, audio: app.audio, quality: app.audioQuality)
        case .displayOnThisPhone:
            DisplayOnThisPhonePage(main: main)
        case .pttButtons:
            PttButtonsPage(timeOut: timeOut)
        case .dataUse:
            DataUsePage(settings: app.phoneSettings, meter: app.longSession.meter)
        case .coreLogs:
            CoreLogsPage(log: app.coreLog)
        case .about:
            AboutAppPage(buildTag: buildTag)
        }
    }
}
