// NereusSDR for iOS: when Setup takes the iPad's split, and how the split reads the Setup tab's path
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
@testable import NereusSDR
import SwiftUI
import Testing

/// R-IOS-52, W8 (spec section 7.3): a regular-width iPad window takes the
/// split and every compact-width one keeps the one list; the split reads the
/// same path the one list pushes, so a page open in one is open in the other.
@Suite("Setup's split on the iPad")
@MainActor
struct SetupSplitTests {
    /// The tree with no Core: the app's own pages only.
    private var tree: [SetupTree.Category] {
        SetupTree.categories(described: [:], order: [], unreadable: [:], stackOffered: false, showPaValues: false)
    }

    @Test("The split is for a regular-width window with an iPad's room, by the size class the window reports")
    func splitFollowsTheSizeClass() {
        let full = CGSize(width: 1210, height: 760)
        #expect(SetupSplit.applies(horizontal: .regular, size: full))
        #expect(SetupSplit.applies(horizontal: .regular, size: CGSize(width: 834, height: 1130)))
        // An iPad window shared narrowly reports compact: the one list.
        #expect(!SetupSplit.applies(horizontal: .compact, size: CGSize(width: 375, height: 760)))
        #expect(!SetupSplit.applies(horizontal: .compact, size: full))
        #expect(!SetupSplit.applies(horizontal: nil, size: full))
        // A large iPhone on its side reports regular, but has no iPad's room.
        #expect(!SetupSplit.applies(horizontal: .regular, size: CGSize(width: 956, height: 400)))
    }

    @Test("The path's category and page are shown on the right; what follows is pushed over the page")
    func selectionReadsThePath() {
        let agc: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.agcAlc")]
        let at = SetupSplitSelection(path: agc, tree: tree)
        #expect(at.openCategory == "DSP")
        #expect(at.shown == .described(category: "dsp", page: "dsp.agcAlc"))
        #expect(at.prefix == agc)
        #expect(at.pushed.isEmpty)

        let bands = agc + [.cfcBands(category: "dsp", control: "dsp.cfc.bands")]
        let deeper = SetupSplitSelection(path: bands, tree: tree)
        #expect(deeper.shown == agc[1])
        #expect(deeper.pushed == [.cfcBands(category: "dsp", control: "dsp.cfc.bands")])

        let devices = SetupSplitSelection(path: [.page(.devices)], tree: tree)
        #expect(devices.openCategory == "Devices")
        #expect(devices.shown == .page(.devices))

        let category = SetupSplitSelection(path: [.category("General")], tree: tree)
        #expect(category.openCategory == "General")
        #expect(category.shown == .category("General"))

        // Nothing chosen yet: the first category's page.
        let start = SetupSplitSelection(path: [], tree: tree)
        #expect(start.shown == .page(.devices))
        #expect(start.prefix == [.page(.devices)])
        #expect(start.openCategory == "Devices")
    }

    @Test("A Setup link from elsewhere opens its page in the split, through the router's own paths")
    func routerPathsOpenInTheSplit() {
        let router = SetupRouter()
        router.open(.displayOnThisPhone)
        let display = SetupSplitSelection(path: router.path, tree: tree)
        #expect(display.openCategory == "Display")
        #expect(display.shown == .page(.displayOnThisPhone))
        #expect(display.pushed.isEmpty)

        #expect(router.open(describedPage: "dsp.agcAlc"))
        let agc = SetupSplitSelection(path: router.path, tree: tree)
        #expect(agc.openCategory == "DSP")
        #expect(agc.shown == .described(category: "dsp", page: "dsp.agcAlc"))
    }

    @Test("Choosing a category opens its first page that can open; the open one stays as it is")
    func choosingACategory() throws {
        let general = try #require(tree.first { $0.id == "General" })
        #expect(SetupSplitSelection.path(choosing: general, openCategory: "Devices", current: [.page(.devices)])
            == [.category("General"), .page(.navigation)])
        let current: [SetupTree.Route] = [.category("General"), .page(.batteryAndSessions)]
        #expect(SetupSplitSelection.path(choosing: general, openCategory: "General", current: current) == current)
        let devices = try #require(tree.first { $0.id == "Devices" })
        #expect(SetupSplitSelection.path(choosing: devices, openCategory: "General", current: current)
            == [.page(.devices)])
        // A category none of whose pages can open shows its list of pages, each greyed with its reason.
        let closed = SetupTree.Category(title: "Closed", tag: .core, summary: "", pages: [], opensPage: nil)
        #expect(SetupSplitSelection.path(choosing: closed, openCategory: nil) == [.category("Closed")])
    }
}
