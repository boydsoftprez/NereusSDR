// NereusSDR for iOS: where the Setup tab's stack is, so another screen can open one of its pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine

/// The Setup tab's navigation path. Another screen opens a page through it,
/// as the Display sheet's "More display options in Setup" opens Display's
/// page (spec section 5.1 item 15, D79).
@MainActor
final class SetupRouter: ObservableObject {
    @Published var path: [SetupTree.Route] = []
    /// The number pad a Setup number row has open, at the foot of the tab.
    let pads = ValuePadHost()

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
}
