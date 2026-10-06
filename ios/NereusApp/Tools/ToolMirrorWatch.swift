// NereusSDR for iOS: the Tools pages' watch on the Core's objects, telling a page to read them again
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// Tells a Tools page's model to read the Core's objects again when one of
/// them changes, when an object comes or goes, when the capabilities change
/// or when the snapshot starts, completes or goes stale. The reads come
/// once per turn of the main queue, after the change has landed.
@MainActor
final class ToolMirrorWatch {
    private let mirror: MirrorStore
    private let changed: @MainActor () -> Void
    private var watched: [String: ObjectIdentifier] = [:]
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watches: Set<AnyCancellable> = []
    private var queued = false

    init(mirror: MirrorStore, changed: @escaping @MainActor () -> Void) {
        self.mirror = mirror
        self.changed = changed
        mirror.$unconfirmedWrites.sink { [weak self] _ in self?.queue() }.store(in: &watches)
        mirror.$objectKeys.sink { [weak self] _ in self?.queue() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queue() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queue() }.store(in: &watches)
        mirror.$isStale.sink { [weak self] _ in self?.queue() }.store(in: &watches)
    }

    /// The object at `key`, watched from now on.
    func object(_ key: String) -> MirrorObject? {
        let object = mirror.object(key)
        let id = object.map(ObjectIdentifier.init)
        if watched[key] != id {
            watched[key] = id
            objectWatches[key] = object?.$values.dropFirst().sink { [weak self] _ in self?.queue() }
        }
        return object
    }

    /// Another source of change the page reads, watched from now on.
    func watch<Output>(_ publisher: some Publisher<Output, Never>) {
        publisher.sink { [weak self] _ in self?.queue() }.store(in: &watches)
    }

    /// Reads again on the next turn of the main queue.
    func queue() {
        guard !queued else {
            return
        }
        queued = true
        Task { @MainActor [weak self] in
            guard let self else {
                return
            }
            self.queued = false
            self.changed()
        }
    }
}
