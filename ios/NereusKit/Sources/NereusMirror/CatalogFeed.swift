// NereusSDR for iOS: the latest catalogue the Core sent, read from the mirror's catalog object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusModels

/// The Core's catalogue as the screens read it (link document section 7.4,
/// R-IOS-06, R-IOS-27): the latest ``StationCatalog`` from the mirror's
/// `catalog` object (class `StationCatalog`, properties `json` and
/// `revision`).
///
/// `revision` travels as an i64 holding a u32 that wraps. Within a session a
/// new value replaces the held catalogue only when its revision is newer by
/// serial arithmetic (RFC 1982); a new session's first value is always taken,
/// since a restarted Core counts again from its start. An empty `json` is no
/// catalogue yet.
///
/// A Core at a minor below 11, or with `stationCatalogVersion` 0, has no
/// catalogue: ``needsNewerCore`` is then true and the screens that need one
/// say ``needsNewerCoreText`` (D23).
@MainActor
public final class CatalogFeed: ObservableObject {
    /// The agreed minor the catalogue arrived in (`kStationCatalogSessionProtocolMinor`).
    public static let catalogMinor: UInt16 = 11
    /// The mirrored object's key and class.
    public static let objectKey = "catalog"
    public static let className = "StationCatalog"
    /// What a screen that needs the catalogue shows on a Core without one.
    public static let needsNewerCoreText = "Needs a newer Core"

    /// The latest catalogue, or nil when there is none yet.
    @Published public private(set) var catalog: StationCatalog?
    /// The revision of the value last taken.
    @Published public private(set) var revision: UInt32?
    /// True when the connected Core sends no catalogue at all.
    @Published public private(set) var needsNewerCore = false

    private let store: MirrorStore
    private var subscriptions: Set<AnyCancellable> = []
    private var watched: MirrorObject?
    private var watchedValues: AnyCancellable?
    /// A new snapshot began since the last value was taken.
    private var newSession = true
    private var refreshQueued = false

    public init(store: MirrorStore) {
        self.store = store
        store.$isSnapshotComplete.sink { [weak self] complete in
            if !complete {
                self?.newSession = true
            }
            self?.queueRefresh()
        }.store(in: &subscriptions)
        store.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &subscriptions)
        store.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &subscriptions)
    }

    /// Serial-number order over 32 bits (RFC 1982).
    static func isNewer(_ value: UInt32, than previous: UInt32) -> Bool {
        value != previous && value &- previous < 0x8000_0000
    }

    // MARK: Inside

    /// The store publishes before it changes, so the feed reads it once the
    /// change is done, on the main actor's next turn.
    private func queueRefresh() {
        guard !refreshQueued else {
            return
        }
        refreshQueued = true
        Task { @MainActor [weak self] in
            self?.refresh()
        }
    }

    private func refresh() {
        refreshQueued = false
        let offered = (store.agreedMinor ?? 0) >= Self.catalogMinor
            && store.capabilityVersion("stationCatalogVersion") >= 1
        let older = !store.capabilities.isEmpty && !offered
        if needsNewerCore != older {
            needsNewerCore = older
        }
        guard !older, let object = store.object(Self.objectKey), object.className == Self.className else {
            watch(nil)
            // A snapshot that completed without the object, or a Core without
            // one, leaves no catalogue; one still arriving keeps the last.
            if older || store.isSnapshotComplete {
                take(nil, revision: nil)
            }
            return
        }
        watch(object)
        guard case .int(let raw)? = object["revision"], (0...Int64(UInt32.max)).contains(raw),
              case .text(let json)? = object["json"] else {
            return
        }
        let next = UInt32(raw)
        if !newSession, let held = revision, !Self.isNewer(next, than: held) {
            return
        }
        let parsed = StationCatalog.parse(json: json)
        // An unreadable catalogue changes nothing; an empty one is none yet.
        if parsed == nil && !json.isEmpty {
            return
        }
        // Until the new snapshot completes, every value is the new session's
        // own, whatever its revision.
        if store.isSnapshotComplete {
            newSession = false
        }
        take(parsed, revision: next)
    }

    private func take(_ next: StationCatalog?, revision nextRevision: UInt32?) {
        if catalog != next {
            catalog = next
        }
        if revision != nextRevision {
            revision = nextRevision
        }
    }

    private func watch(_ object: MirrorObject?) {
        guard object !== watched else {
            return
        }
        watched = object
        watchedValues = object?.$values.sink { [weak self] _ in self?.queueRefresh() }
    }
}
