// NereusSDR for iOS: the Setup tab's tree: Devices first, then the desktop's categories in order, each marked
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror

/// The Setup tab's tree (R-IOS-18, D16, spec section 5.2 items 6 and 7):
/// Devices first, then the desktop's categories in its order and with its
/// names, each marked Core, This phone or Both as the spec's table marks
/// it. A category shows only the pages that exist (D41): the app's own
/// pages, and the pages the Core describes, each in the desktop's place
/// for it, so a page the Core adds appears by itself. A category with
/// neither is left out. The Keyboard page, Skins, Collapsible Display and
/// the desktop's Remote Access page are never on the phone (spec section
/// 5.2 item 7).
enum SetupTree {
    /// One of the app's own pages.
    enum Page: String, Hashable, Sendable, CaseIterable {
        case devices
        case navigation
        case batteryAndSessions
        case audioOnThisPhone
        case displayOnThisPhone
        case pttButtons
        case dataUse
        case coreLogs
        case about
    }

    /// Where a page's row leads.
    enum Destination: Hashable, Sendable {
        case native(Page)
        /// A page the Core describes: its category's ID and its own.
        case described(category: String, page: String)
    }

    /// One page's row in its category: its name there and its mark.
    struct PageEntry: Equatable, Sendable {
        let destination: Destination
        let title: String
        let tag: SetupTag
        /// Why this page cannot open on this Core: listed greyed with the
        /// reason, never left out (the 3D View page on an older Core).
        var unavailableReason: String? = nil

        init(page: Page, title: String, tag: SetupTag) {
            destination = .native(page)
            self.title = title
            self.tag = tag
        }

        init(category: String, page: SetupDescription.Page) {
            destination = .described(category: category, page: page.id)
            title = page.title
            tag = SetupTag(page.whereOwned)
        }

        init(category: String, pageId: String, title: String, tag: SetupTag) {
            destination = .described(category: category, page: pageId)
            self.title = title
            self.tag = tag
        }

        /// The app's own page this row opens, if it is one.
        var page: Page? {
            if case .native(let page) = destination { return page }
            return nil
        }
    }

    /// One category of the tree.
    struct Category: Identifiable, Equatable, Sendable {
        /// The desktop's name for it, also its id.
        let title: String
        let tag: SetupTag
        /// The line under its name.
        let summary: String
        let pages: [PageEntry]
        /// A category that is a page of its own opens it at once (Devices,
        /// and About this app, whose page lists the app's own).
        let opensPage: Page?
        /// Why part of this category from the Core cannot be shown.
        var notice: String? = nil

        var id: String { title }
    }

    /// Where the Setup tab's stack has got to.
    enum Route: Hashable, Sendable {
        case category(String)
        case page(Page)
        case described(category: String, page: String)
        /// The CFC band editor a described row opens.
        case cfcBands(category: String, control: String)
    }

    /// One category of the desktop's tree: its Core ID, name, mark, the
    /// desktop's page names in order, and the app's own pages among them.
    struct Skeleton: Sendable {
        let coreId: String?
        let title: String
        let tag: SetupTag
        let pageOrder: [String]
        let native: [PageEntry]
        let opensPage: Page?
    }

    /// The spec's table (section 5.2 item 6), in the desktop's order.
    static let skeleton: [Skeleton] = [
        Skeleton(coreId: nil, title: "Devices", tag: .core, pageOrder: ["Devices"],
                 native: [PageEntry(page: .devices, title: "Devices", tag: .core)], opensPage: .devices),
        Skeleton(coreId: "general", title: "General", tag: .both,
                 pageOrder: ["Startup & Preferences", "UI Scale & Theme", "Navigation", "Battery and sessions",
                             "Options"],
                 native: [PageEntry(page: .navigation, title: "Navigation", tag: .thisPhone),
                          PageEntry(page: .batteryAndSessions, title: "Battery and sessions", tag: .thisPhone)],
                 opensPage: nil),
        Skeleton(coreId: "hardware", title: "Hardware", tag: .core, pageOrder: ["Hardware Config", "DDC Routing"],
                 native: [], opensPage: nil),
        Skeleton(coreId: "pa", title: "PA", tag: .core, pageOrder: ["PA Gain", "Watt Meter", "PA Values"],
                 native: [], opensPage: nil),
        Skeleton(coreId: "audio", title: "Audio", tag: .both,
                 pageOrder: ["On this phone", "Devices", "TX Input", "VAX", "TCI", "Advanced", "TX Profile"],
                 native: [PageEntry(page: .audioOnThisPhone, title: "On this phone", tag: .thisPhone)],
                 opensPage: nil),
        Skeleton(coreId: "dsp", title: "DSP", tag: .core,
                 pageOrder: ["AGC/ALC", "NR/ANF", "NB/SNB", "CW", "AM/SAM", "FM", "CFC", "TNF", "Filter Presets",
                             "Options"],
                 native: [], opensPage: nil),
        Skeleton(coreId: "display", title: "Display", tag: .both,
                 pageOrder: ["On this phone", "Spectrum Defaults", "Spectrum Peaks", "Waterfall Defaults",
                             "Grid & Scales", "Multimeter", "TX Display", "3D View"],
                 native: [PageEntry(page: .displayOnThisPhone, title: "On this phone", tag: .both)],
                 opensPage: nil),
        Skeleton(coreId: "transmit", title: "Transmit", tag: .both,
                 pageOrder: ["Power", "TX Profiles", "Speech Processor", "DEXP/VOX", "PTT buttons"],
                 native: [PageEntry(page: .pttButtons, title: "PTT buttons", tag: .thisPhone)], opensPage: nil),
        Skeleton(coreId: "appearance", title: "Appearance", tag: .thisPhone,
                 pageOrder: ["Colors & Theme", "Meter Styles", "Gradients"], native: [], opensPage: nil),
        Skeleton(coreId: "catNetwork", title: "CAT & Network", tag: .both,
                 pageOrder: ["Serial Ports", "TCI Server", "Peripherals", "4O3A", "RF-Kit", "TCP/IP CAT",
                             "MIDI Control", "Data use"],
                 native: [PageEntry(page: .dataUse, title: "Data use", tag: .thisPhone)], opensPage: nil),
        Skeleton(coreId: "test", title: "Test", tag: .core, pageOrder: ["Two-Tone IMD"], native: [], opensPage: nil),
        Skeleton(coreId: "diagnostics", title: "Diagnostics", tag: .both,
                 pageOrder: ["Radio Status", "Connection Quality", "Settings Validation", "Logs", "Export / Import"],
                 native: [PageEntry(page: .coreLogs, title: "Logs", tag: .core)], opensPage: nil),
        Skeleton(coreId: nil, title: "About this app", tag: .thisPhone, pageOrder: ["About this app"],
                 native: [PageEntry(page: .about, title: "About this app", tag: .thisPhone)], opensPage: .about),
    ]

    /// Pages the phone never shows, whatever a description says.
    static let leftOff: Set<String> = ["keyboard", "skins", "collapsible display", "remote access"]

    /// The tree with only the app's own pages, as before any Core describes its own.
    static var categories: [Category] {
        categories(described: [:], order: [], unreadable: [:])
    }

    /// The Display category's 3D View page (Display V12) and its title.
    static let stackPageId = "display.threeD"
    static let stackPageTitle = "3D View"

    /// Display's pages with the 3D View page as this Core allows it: a
    /// Core that describes Display but does not offer the 3D view (below
    /// description 12, or without its gate) lists the page greyed with the
    /// reason (JJ's 3D View board, recommendation 7).
    private static func withStackPage(_ pages: [PageEntry], description: SetupDescription?,
                                      stackOffered: Bool) -> [PageEntry] {
        guard let description, description.category.id == "display" else {
            return pages
        }
        let described = description.pages.contains { $0.id == stackPageId }
        if described && stackOffered {
            return pages
        }
        var greyed = PageEntry(category: "display", pageId: stackPageId, title: stackPageTitle, tag: .thisPhone)
        greyed.unavailableReason = StackedTraceHold.coreOlderText
        if let index = pages.firstIndex(where: { $0.destination == greyed.destination }) {
            var kept = pages
            kept[index] = greyed
            return kept
        }
        return pages + [greyed]
    }

    /// The tree: the app's own pages and the Core's described ones, each in
    /// its place. A category the desktop does not have (a newer Core's)
    /// follows Diagnostics with the Core's name and mark.
    static func categories(described: [String: SetupDescription], order: [String],
                           unreadable: [String: String], stackOffered: Bool = false) -> [Category] {
        var result: [Category] = []
        for skeleton in skeleton {
            if skeleton.title == "About this app" {
                result.append(contentsOf: extraCategories(described: described, order: order, unreadable: unreadable))
            }
            let description = skeleton.coreId.flatMap { described[$0] }
            let pages = withStackPage(merged(skeleton, description: description), description: description,
                                      stackOffered: stackOffered)
            let notice = skeleton.coreId.flatMap { unreadable[$0] }
            guard !pages.isEmpty || notice != nil else {
                continue
            }
            result.append(Category(title: skeleton.title, tag: skeleton.tag,
                                   summary: summary(pages, notice: notice, opensPage: skeleton.opensPage),
                                   pages: pages, opensPage: skeleton.opensPage, notice: notice))
        }
        return result
    }

    /// Categories the Core describes that the desktop's tree above lacks.
    private static func extraCategories(described: [String: SetupDescription], order: [String],
                                        unreadable: [String: String]) -> [Category] {
        let known = Set(skeleton.compactMap(\.coreId))
        return order.filter { !known.contains($0) }.compactMap { id in
            let description = described[id]
            let pages = (description?.pages ?? []).filter { !leftOff.contains($0.title.lowercased()) }
                .map { PageEntry(category: id, page: $0) }
            let notice = unreadable[id]
            guard !pages.isEmpty || notice != nil else { return nil }
            let tag = description.map { SetupTag($0.category.whereOwned) } ?? .core
            return Category(title: description?.category.title ?? id, tag: tag,
                            summary: summary(pages, notice: notice, opensPage: nil), pages: pages, opensPage: nil,
                            notice: notice)
        }
    }

    /// The app's own pages and the described ones in the desktop's order.
    /// A described page the desktop's list lacks follows the page before it.
    private static func merged(_ skeleton: Skeleton, description: SetupDescription?) -> [PageEntry] {
        func rank(_ title: String) -> Double? {
            skeleton.pageOrder.firstIndex { $0.caseInsensitiveCompare(title) == .orderedSame }.map(Double.init)
        }
        var ranked: [(rank: Double, sequence: Int, entry: PageEntry)] = []
        for (index, entry) in skeleton.native.enumerated() {
            ranked.append((rank(entry.title) ?? Double(skeleton.pageOrder.count), index, entry))
        }
        var previous = -1.0
        var step = 0
        let described = (description?.pages ?? []).filter { page in
            !leftOff.contains(page.title.lowercased())
                && !skeleton.native.contains { $0.title.caseInsensitiveCompare(page.title) == .orderedSame }
        }
        for page in described {
            let entry = PageEntry(category: skeleton.coreId ?? "", page: page)
            if let known = rank(page.title) {
                previous = known
                step = 0
                ranked.append((known, 100 + ranked.count, entry))
            } else {
                step += 1
                ranked.append((previous + Double(step) / 1000, 100 + ranked.count, entry))
            }
        }
        return ranked.sorted { ($0.rank, $0.sequence) < ($1.rank, $1.sequence) }.map(\.entry)
    }

    private static func summary(_ pages: [PageEntry], notice: String?, opensPage: Page?) -> String {
        if opensPage == .devices {
            return "Paired phones and computers \u{00B7} Add a device"
        }
        if opensPage == .about {
            return "Build · Credits · Licenses"
        }
        if pages.isEmpty, let notice {
            return notice
        }
        return pages.map(\.title).joined(separator: " \u{00B7} ")
    }

    /// The category a page sits in.
    static func category(of page: Page) -> Category? {
        categories.first { $0.pages.contains { $0.page == page } }
    }

    /// The route that shows the Core-described page `pageId`, through the
    /// category its id begins with (`dsp` for `dsp.agcAlc`).
    static func route(toDescribed pageId: String) -> [Route]? {
        guard let dot = pageId.firstIndex(of: "."), dot != pageId.startIndex else { return nil }
        let coreId = String(pageId[..<dot])
        guard let skeleton = skeleton.first(where: { $0.coreId == coreId }) else { return nil }
        return [.category(skeleton.title), .described(category: coreId, page: pageId)]
    }

    /// The route that shows `page`, through its category.
    static func route(to page: Page) -> [Route] {
        guard let category = category(of: page) else {
            return []
        }
        if category.opensPage == page {
            return [.page(page)]
        }
        return [.category(category.id), .page(page)]
    }
}

extension SetupTag {
    /// A described page's mark from where the Core says its settings live.
    init(_ owner: SetupDescription.Owner) {
        switch owner {
        case .station:
            self = .core
        case .phone:
            self = .thisPhone
        case .mixed:
            self = .both
        }
    }
}
