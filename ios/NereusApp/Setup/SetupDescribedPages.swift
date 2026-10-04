// NereusSDR for iOS: the Setup pages the Core describes, kept for the tree while the Core is away
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusLink
import NereusMirror

/// The Core's Setup categories as the tree shows them. While the Core's
/// descriptions are current they come straight from the feed; when it is
/// away, the last ones this phone saw from the same Core stay on screen so
/// their pages keep their places and show their last values, disabled.
/// A different Core's pages are never shown.
@MainActor
final class SetupDescribedPages: ObservableObject {
    /// Why a category the Core sent cannot be shown, in plain words.
    static let unreadablePageReason = "This part of Setup from the Core could not be read."

    /// Described rows this phone does not draw. Empty: every row up to
    /// description 16 is drawn (the 3D View page, the Multimeter rows and the
    /// V13 to V16 rows, greyed with a reason where the phone cannot run
    /// them), and the phone asks for 16 (`LinkFeatures`' `setupDescription`).
    static let notDrawnYet: Set<String> = []

    /// Each described category, current or last seen, by its ID.
    @Published private(set) var categories: [String: SetupDescription] = [:]
    /// The Core's category order, as its schema lists them.
    @Published private(set) var order: [String] = []
    /// Categories the Core sent that could not be read, with the reason.
    @Published private(set) var unreadable: [String: String] = [:]
    /// True while what is shown is the Core's current description.
    @Published private(set) var isCurrent = false

    let feed: SetupDescriptionFeed
    private let store: MirrorStore
    private var coreIdentity: String?
    private var watches: Set<AnyCancellable> = []
    private var refreshQueued = false

    init(feed: SetupDescriptionFeed, store: MirrorStore, hello: AnyPublisher<LinkMessage.Hello?, Never>) {
        self.feed = feed
        self.store = store
        feed.$generation.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        store.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        store.$isStale.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        hello.sink { [weak self] hello in
            guard let self, let hello else { return }
            let identity = hello.identity?.publicKey ?? hello.peer
            if identity != self.coreIdentity {
                // Another Core: nothing of the last one's stays.
                self.coreIdentity = identity
                self.categories = [:]
                self.order = []
                self.unreadable = [:]
            }
        }.store(in: &watches)
    }

    /// The page `pageId` of `category`, current or last seen.
    func page(_ pageId: String, in category: String) -> SetupDescription.Page? {
        categories[category]?.pages.first { $0.id == pageId }
    }

    private func queueRefresh() {
        guard !refreshQueued else { return }
        refreshQueued = true
        Task { @MainActor [weak self] in self?.refresh() }
    }

    func refresh() {
        refreshQueued = false
        let current = store.isSnapshotComplete && !store.isStale && !feed.categoryOrder.isEmpty
        if current {
            var next: [String: SetupDescription] = [:]
            var problems: [String: String] = [:]
            for id in feed.categoryOrder {
                switch feed.status(for: id) {
                case .available(let description):
                    next[id] = description.leavingOut(Self.notDrawnYet)
                case .unpublished:
                    break
                case .unavailable:
                    problems[id] = Self.unreadablePageReason
                }
            }
            set(\.categories, next)
            set(\.order, feed.categoryOrder)
            set(\.unreadable, problems)
        }
        set(\.isCurrent, current && categories.keys.allSatisfy { feed.description(for: $0) != nil })
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<SetupDescribedPages, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
