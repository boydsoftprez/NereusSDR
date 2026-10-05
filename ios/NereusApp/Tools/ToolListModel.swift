// NereusSDR for iOS: the Tools tab's list as it stands now, following the Core's catalogue while the tab is open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// Keeps the Tools tab's list (``StationToolList``) current: it reads the
/// Core's catalogue again whenever a new one arrives, the Core connects or
/// goes away, or a Core without a catalogue connects, and publishes the
/// list only when it changed.
@MainActor
final class ToolListModel: ObservableObject {
    @Published private(set) var entries: [StationToolList.Entry] = []

    private let mirror: MirrorStore
    private let catalogFeed: CatalogFeed
    private var watch: ToolMirrorWatch?

    init(mirror: MirrorStore, catalogFeed: CatalogFeed) {
        self.mirror = mirror
        self.catalogFeed = catalogFeed
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        watch.watch(catalogFeed.$catalog)
        watch.watch(catalogFeed.$needsNewerCore)
        self.watch = watch
        refresh()
    }

    func refresh() {
        var next = StationToolList.entries(tools: catalogFeed.catalog?.tools,
                                           connected: mirror.isSnapshotComplete && !mirror.isStale,
                                           olderCore: catalogFeed.needsNewerCore)
        // A complete negotiated summary supplies this page before a delayed catalogue arrives.
        // A catalogue that explicitly lacks the radio hardware keeps its existing omission.
        if catalogFeed.catalog == nil, mirror.isSnapshotComplete, !mirror.isStale,
           DiversityState.available(in: mirror),
           let json = ToolValue.text(watch?.object("radio")?[DiversityState.propertyName]),
           DiversityState(json: json) != nil,
           let index = next.firstIndex(where: { $0.id == "diversity" }) {
            let row = next[index]
            next[index] = StationToolList.Entry(id: row.id, title: row.title,
                detail: "Two-receiver diversity and phasing", tag: row.tag, page: row.page, reason: nil)
        }
        if next != entries {
            entries = next
        }
    }
}
