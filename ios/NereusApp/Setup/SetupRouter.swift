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
        }
    }
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
}
