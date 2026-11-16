// NereusSDR for iOS: where the Setup tab's stack is, so another screen can open one of its pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine

/// The Setup tab's navigation path. Another screen opens a page through it,
/// as the Display sheet's "More display options in Setup" opens Display's
/// page (spec section 5.1 item 15, D79). The one list pushes the path; on a
/// regular-width iPad ``SetupSplitView`` reads the same path as the page it
/// shows beside the list (R-IOS-52, spec section 7.3), so each `open`
/// selects the page there and pushes it on an iPhone, and a page stays open
/// when the window changes width.
@MainActor
final class SetupRouter: ObservableObject {
    @Published var path: [SetupTree.Route] = [] {
        // Leaving Display for the list or another category: the next
        // Display page opened from the list edits All pans.
        didSet {
            if !Self.inDisplay(path), displayScope != .all {
                displayScope = .all
            }
            pendingRestore = nil
            keep?(path)
        }
    }
    /// Told each change of the path, for it to be kept on this phone.
    var keep: (@MainActor ([SetupTree.Route]) -> Void)?
    /// The place kept from last time, until the Core's pages say whether it
    /// exists here, or the operator goes somewhere first.
    @Published private(set) var pendingRestore: [SetupTree.Route]?
    /// The number pad a Setup number row has open, at the foot of the tab.
    let pads = ValuePadHost()
    /// The pans Display's pages edit: All pans from the Setup list, or the
    /// pan a page was opened on. It stays while moving between Display's
    /// pages, as the desktop's strip keeps its pan (spec section 7.3).
    @Published var displayScope: MainScreenModel.DisplayEditScope = .all

    /// Whether `path` is in Setup > Display.
    static func inDisplay(_ path: [SetupTree.Route]) -> Bool {
        path.contains { route in
            switch route {
            case .category(let id): return id == "Display"
            case .described(let category, _): return category == "display"
            case .page(let page): return page == .displayOnThisPhone
            case .cfcBands: return false
            }
        }
    }

    /// A session reached ready: `saved` is shown once ``applyRestore(tree:known:)`` finds it.
    func restore(_ saved: [SetupTree.Route]) {
        pendingRestore = saved.isEmpty ? nil : saved
    }

    /// Shows the kept place once its pages exist. `known` says the tree is the
    /// Core's own: only then does a page missing from it mean the page is
    /// gone, and the operator lands on the last of the place that remains.
    func applyRestore(tree: [SetupTree.Category], known: Bool) {
        guard let saved = pendingRestore else { return }
        let found = Self.remaining(saved, tree: tree)
        guard known || found.count == saved.count else { return }
        path = found
    }

    /// `saved` up to the first page `tree` does not offer.
    static func remaining(_ saved: [SetupTree.Route], tree: [SetupTree.Category]) -> [SetupTree.Route] {
        var found: [SetupTree.Route] = []
        for route in saved {
            let exists: Bool
            switch route {
            case .category(let id):
                exists = tree.contains { $0.id == id }
            case .page(let page):
                exists = tree.contains { category in
                    category.opensPage == page
                        || category.pages.contains { $0.page == page && $0.unavailableReason == nil }
                }
            case .described(let category, let page):
                let destination = SetupTree.Destination.described(category: category, page: page)
                exists = tree.contains { entries in
                    entries.pages.contains { $0.destination == destination && $0.unavailableReason == nil }
                }
            case .cfcBands:
                // Opened from the described page before it.
                if case .described? = found.last { exists = true } else { exists = false }
            }
            guard exists else { break }
            found.append(route)
        }
        return found
    }

    /// Shows `page` through its category, from the top of the tree.
    func open(_ page: SetupTree.Page) {
        path = SetupTree.route(to: page)
    }

    /// Shows the Core-described page `pageId` (`dsp.agcAlc`) through its
    /// category; false when no category of the tree holds that id.
    @discardableResult
    func open(describedPage pageId: String) -> Bool {
        guard let route = SetupTree.route(toDescribed: pageId) else { return false }
        path = route
        return true
    }

    /// Shows the Core-described page `pageId` with `scope` chosen on its
    /// strip, as the Display sheet's Setup link opens its pan's page; false,
    /// leaving the scope as it was, when no category holds that id.
    @discardableResult
    func open(describedPage pageId: String, scope: MainScreenModel.DisplayEditScope) -> Bool {
        guard open(describedPage: pageId) else { return false }
        displayScope = scope
        return true
    }

    /// The Display pill opened on `pan`: an open Display page switches its
    /// strip from All to that pan, so a pill change and the page agree.
    /// Outside Display the scope stays All.
    func followPill(_ pan: PanIdentity) {
        guard Self.inDisplay(path) else { return }
        displayScope = .pan(pan)
    }
}
