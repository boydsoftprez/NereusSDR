// NereusSDR for iOS: Setup on a full-width iPad: the list of categories on the left and the open page beside it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Setup in a regular-width iPad window (R-IOS-52, W8, spec section 7.3):
/// the categories on the left, the open category's pages under its name and
/// the open page marked in the glass, and the open page on the right, its
/// content held to a readable width, as the desktop's Setup window has its
/// tree beside the page. An iPhone, and an iPad window shared narrowly,
/// keep ``SetupTab``'s one list with pages that slide in.
///
/// Both layouts read the same ``SetupRouter/path``, so a page opened in one
/// is still open after the window changes to the other, and a Setup link
/// from elsewhere opens its page in either. Here the path's category and
/// page are the selection shown on the right; anything past them (the CFC
/// band editor) is pushed over the page on the right.
struct SetupSplitView<Detail: View, Footer: View>: View {
    @ObservedObject var router: SetupRouter
    let tree: [SetupTree.Category]
    /// A route's page, as the one list shows it.
    let destination: (SetupTree.Route) -> Detail
    /// The note on where settings live, and a test build's name, under the list.
    let footer: Footer

    init(router: SetupRouter, tree: [SetupTree.Category],
         @ViewBuilder destination: @escaping (SetupTree.Route) -> Detail,
         @ViewBuilder footer: () -> Footer) {
        self.router = router
        self.tree = tree
        self.destination = destination
        self.footer = footer()
    }

    var body: some View {
        let selection = SetupSplitSelection(path: router.path, tree: tree)
        HStack(spacing: 0) {
            NavigationStack {
                list(selection)
                    .navigationTitle("Setup")
            }
            .frame(width: SetupSplit.listWidth)
            .overlay(alignment: .trailing) {
                Rectangle()
                    .fill(WorkspacePalette.borderSubtle)
                    .frame(width: 1)
                    .ignoresSafeArea()
                    .accessibilityHidden(true)
            }
            NavigationStack(path: pushed(over: selection)) {
                destination(selection.shown)
                    // The chosen page heads its column with its own title.
                    .environment(\.setupInlineTitles, true)
                    .navigationDestination(for: SetupTree.Route.self) { route in
                        // A page pushed over it keeps the bar's title and its way back.
                        destination(route)
                            .environment(\.setupInlineTitles, false)
                    }
            }
            // A newly chosen page starts at its own top, with nothing pushed over it.
            .id(selection.shown)
            .environment(\.setupReadableWidth, SetupSplit.readableWidth)
            .frame(maxWidth: .infinity)
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("setupSplitPage")
        }
    }

    // MARK: The list

    private func list(_ selection: SetupSplitSelection) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 2) {
                ForEach(tree) { category in
                    categoryRow(category, selection: selection)
                    if selection.openCategory == category.id, category.opensPage == nil {
                        ForEach(category.pages, id: \.destination) { entry in
                            pageRow(entry, in: category, selection: selection)
                        }
                    }
                }
                footer
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textTertiary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 12)
                    .padding(.top, 14)
            }
            .padding(.horizontal, 8)
            .padding(.top, 8)
            .padding(.bottom, 24)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .scrollContentBackground(.hidden)
        .background(WorkspacePalette.panelBg.ignoresSafeArea())
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("setupSplitList")
    }

    private func categoryRow(_ category: SetupTree.Category, selection: SetupSplitSelection) -> some View {
        let open = selection.openCategory == category.id
        let here = category.opensPage.map { selection.shown == .page($0) } ?? false
        return Button {
            router.path = SetupSplitSelection.path(choosing: category, openCategory: selection.openCategory,
                                                   current: router.path)
        } label: {
            HStack(spacing: 10) {
                Text(category.title)
                    .font(.body.weight(.semibold))
                    .frame(maxWidth: .infinity, alignment: .leading)
                SetupTagBadge(tag: category.tag)
                Image(systemName: open && category.opensPage == nil ? "chevron.down" : "chevron.right")
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(WorkspacePalette.textScale)
                    .accessibilityHidden(true)
            }
            .modifier(SetupSplitRow(here: here, indent: 0))
        }
        .buttonStyle(SetupLinkStyle())
        .accessibilityIdentifier(category.title)
        .accessibilityAddTraits(here ? .isSelected : [])
    }

    @ViewBuilder
    private func pageRow(_ entry: SetupTree.PageEntry, in category: SetupTree.Category,
                         selection: SetupSplitSelection) -> some View {
        if let reason = entry.unavailableReason {
            // Listed greyed with its reason, never left out.
            VStack(alignment: .leading, spacing: 2) {
                Text(entry.title)
                    .font(.subheadline)
                    .foregroundStyle(WorkspacePalette.textInactive)
                Text(reason)
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textScale)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .modifier(SetupSplitRow(here: false, indent: SetupSplitRow.pageIndent))
            .accessibilityElement(children: .combine)
            .accessibilityAddTraits(.isStaticText)
            .accessibilityIdentifier(entry.title)
        } else {
            let route = SetupSplitSelection.route(entry.destination)
            let here = selection.shown == route
            Button {
                router.path = [.category(category.id), route]
            } label: {
                Text(entry.title)
                    .font(.subheadline)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .modifier(SetupSplitRow(here: here, indent: SetupSplitRow.pageIndent))
            }
            .buttonStyle(SetupLinkStyle())
            .accessibilityIdentifier(entry.title)
            .accessibilityAddTraits(here ? .isSelected : [])
        }
    }

    // MARK: The page

    /// What is pushed over the shown page on the right; a push or a back
    /// there changes the router's path past the selection.
    private func pushed(over selection: SetupSplitSelection) -> Binding<[SetupTree.Route]> {
        Binding {
            selection.pushed
        } set: { routes in
            router.path = selection.prefix + routes
        }
    }
}

/// When Setup takes the split, and its measures.
enum SetupSplit {
    /// The list's width (setup-board.html's `.tree`).
    static let listWidth: CGFloat = 300
    /// The page's content width at most (setup-board.html's `.wide`).
    static let readableWidth: CGFloat = 720

    /// Whether Setup takes the split: the iPad layouts' own test, a regular
    /// width and an iPad's room (``IPadLayout``), so every iPhone and an
    /// iPad window shared narrowly keep the one list.
    static func applies(horizontal: UserInterfaceSizeClass?, size: CGSize) -> Bool {
        IPadLayout.arrangement(horizontal: horizontal, size: size).isIPad
    }
}

/// How the split reads the router's path: which category is open, which
/// page is shown on the right, and what is pushed over it.
struct SetupSplitSelection: Equatable {
    /// The page shown when nothing is chosen yet: the first category's.
    static let start = SetupTree.Route.page(.devices)

    /// The category open in the list, by its id.
    let openCategory: String?
    /// The route shown at the right-hand side's root.
    let shown: SetupTree.Route
    /// The path up to and including ``shown``.
    let prefix: [SetupTree.Route]
    /// The routes pushed over ``shown``.
    let pushed: [SetupTree.Route]

    init(path: [SetupTree.Route], tree: [SetupTree.Category]) {
        let depth: Int
        if case .category = path.first, path.count >= 2 {
            depth = 2
        } else {
            depth = min(path.count, 1)
        }
        prefix = depth == 0 ? [Self.start] : Array(path.prefix(depth))
        pushed = Array(path.dropFirst(depth))
        shown = prefix[prefix.count - 1]
        openCategory = Self.category(of: prefix[0], tree: tree)
    }

    /// The category a path's first step belongs to.
    private static func category(of route: SetupTree.Route, tree: [SetupTree.Category]) -> String? {
        switch route {
        case .category(let id):
            return id
        case .page(let page):
            return tree.first { $0.opensPage == page || $0.pages.contains { $0.page == page } }?.id
        case .described(let category, _), .cfcBands(let category, _):
            return tree.first { skeletonId($0) == category }?.id
        }
    }

    private static func skeletonId(_ category: SetupTree.Category) -> String? {
        SetupTree.skeleton.first { $0.title == category.title }?.coreId
    }

    /// The path a tap on `category` in the list opens: a category that is a
    /// page opens it; another opens its first page that can open, as the
    /// desktop's tree does; the category already open stays as it is.
    static func path(choosing category: SetupTree.Category, openCategory: String?,
                     current: [SetupTree.Route]? = nil) -> [SetupTree.Route] {
        if let page = category.opensPage {
            return [.page(page)]
        }
        if openCategory == category.id, let current {
            return current
        }
        guard let first = category.pages.first(where: { $0.unavailableReason == nil }) else {
            return [.category(category.id)]
        }
        return [.category(category.id), route(first.destination)]
    }

    /// The route a page row opens.
    static func route(_ destination: SetupTree.Destination) -> SetupTree.Route {
        switch destination {
        case .native(let page):
            return .page(page)
        case .described(let category, let page):
            return .described(category: category, page: page)
        }
    }
}

/// A row of the split's list: its padding, at least a finger's height, and
/// the glass under the open page.
private struct SetupSplitRow: ViewModifier {
    /// A page row's indent under its category (setup-board.html's `.sub`).
    static let pageIndent: CGFloat = 18

    let here: Bool
    let indent: CGFloat

    func body(content: Content) -> some View {
        content
            .foregroundStyle(here ? WorkspacePalette.glassSelText : WorkspacePalette.textPrimary)
            .padding(.leading, 12 + indent)
            .padding(.trailing, 12)
            .padding(.vertical, 9)
            .frame(maxWidth: .infinity, minHeight: SetupRowMetrics.minHeight, alignment: .leading)
            .background {
                if here {
                    WorkspaceFace(.glass, radius: WorkspacePalette.chipRadius)
                }
            }
            .contentShape(Rectangle())
    }
}
