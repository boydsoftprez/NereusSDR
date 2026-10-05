// NereusSDR for iOS: current-session Setup descriptions from the mirror
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine

/// A current snapshot's Setup categories. Each category is independently
/// unpublished, usable, or unavailable with a plain reason. Callers must
/// recheck `description(for:)` and `generation` before using a gesture's data.
@MainActor
public final class SetupDescriptionFeed: ObservableObject {
    public enum Status: Equatable, Sendable {
        case unpublished
        case available(SetupDescription)
        case unavailable(String)
    }
    public static let minimumMinor: UInt16 = 11
    public static let objectKey = "setup"
    public static let className = "SetupDescription"

    @Published public private(set) var generation: UInt64 = 0
    @Published public private(set) var categoryOrder: [String] = []
    @Published public private(set) var revision: UInt32?
    @Published public private(set) var statuses: [String: Status] = [:]

    private let store: MirrorStore
    private var subscriptions: Set<AnyCancellable> = []
    private var watched: MirrorObject?
    private var watchedValues: AnyCancellable?
    private var publishedValues: [String: MirrorValue] = [:]
    private var publishedSchemaIdentity: UInt64?
    private var refreshQueued = false

    public init(store: MirrorStore) {
        self.store = store
        store.$isSnapshotComplete.sink { [weak self] complete in
            if !complete { self?.invalidate() }
            self?.queueRefresh()
        }.store(in: &subscriptions)
        store.$isStale.sink { [weak self] stale in
            if stale { self?.invalidate() }
            self?.queueRefresh()
        }.store(in: &subscriptions)
        store.$capabilities.sink { [weak self] capabilities in
            guard let self else { return }
            if !Self.supports(capabilities["setupDescriptionVersion"]) { self.invalidate() }
            self.queueRefresh()
        }.store(in: &subscriptions)
        store.$objectKeys.sink { [weak self] keys in
            if !keys.contains(Self.objectKey) { self?.invalidate() }
            self?.queueRefresh()
        }.store(in: &subscriptions)
        store.$schemaRevision.sink { [weak self] _ in self?.queueRefresh() }.store(in: &subscriptions)
    }

    /// Reads usable metadata only from the currently completed snapshot.
    /// The guards invalidate synchronously, even before Combine's queued
    /// parse follows a delta or session event.
    public func description(for category: String) -> SetupDescription? {
        guard case .available(let description) = status(for: category) else { return nil }
        return description
    }

    /// A gesture captures `generation` when it starts and calls this before
    /// dispatch. The status check catches a delta even before the feed's
    /// queued refresh advances the published generation.
    public func isCurrent(_ capturedGeneration: UInt64, category: String) -> Bool {
        capturedGeneration == generation && description(for: category) != nil
    }

    public func status(for category: String) -> Status {
        guard store.isSnapshotComplete, !store.isStale else {
            return .unavailable("The Core's Setup description is not current.")
        }
        guard (store.agreedMinor ?? 0) >= Self.minimumMinor else {
            return .unavailable("This Core does not provide Setup descriptions.")
        }
        guard Self.supports(store.capabilities["setupDescriptionVersion"]) else {
            return .unavailable("Setup description version is unavailable.")
        }
        guard let schemaIdentity = store.currentSessionSchemaIdentity(ofClass: Self.className) else {
            return .unavailable("The Core has not sent a current Setup schema.")
        }
        guard schemaIdentity == publishedSchemaIdentity else {
            return .unavailable("The Core's Setup schema is changing.")
        }
        guard let object = store.object(Self.objectKey), object.className == Self.className,
              object === watched else {
            return .unavailable("The Core has not sent Setup descriptions.")
        }
        guard object.values == publishedValues else {
            return .unavailable("The Core's Setup description is changing.")
        }
        let result = statuses[category] ?? .unavailable("The Core has not described this Setup category.")
        if case .available(let description) = result,
           Self.projectedVersion(maximum: store.capabilityVersion("setupDescriptionVersion"),
                                 category: category) != description.version {
            return .unavailable("Setup description version is changing.")
        }
        return result
    }

    private static func supports(_ value: MirrorValue?) -> Bool {
        switch value {
        case .int(let version)?, .enumeration(let version)?:
            return (1...Int64(SetupDescription.highestVersion)).contains(version)
        default: return false
        }
    }

    static func projectedVersion(maximum: Int64?, category: String) -> Int? {
        guard let maximum, (1...Int64(SetupDescription.highestVersion)).contains(maximum) else { return nil }
        // The version the Core sends for each category, as it fits a
        // category to the version this phone asked for: Hardware changed
        // at 6, 13, 16, 17 (the Alex-1 low-pass rows), 18 (the HL2 clock
        // rows) and 23 (Calibration's Rx1 6m LNA row), PA at 5, 13, 14 and 20 (profiles live on air),
        // Transmit at 13 and 15, DSP at 15, 19 (the CFC bands) and 22
        // (RX buffers on air), CAT and Network at 15 and 21 (the TCI
        // Forget row's dependency), Audio at 15 and 24 (Line In Gain's
        // 1.5 dB steps, the Saturn G2's Mic Tip-Ring) and Diagnostics at 15.
        let cap: Int64
        switch category {
        case "hardware": cap = maximum < 13 ? 6 : maximum < 16 ? 13 : maximum < 17 ? 16 : maximum < 18 ? 17 : maximum < 23 ? 18 : 23
        case "pa": cap = maximum < 13 ? 5 : maximum < 20 ? 14 : 20
        case "transmit": cap = maximum < 13 ? 3 : maximum < 15 ? 13 : 15
        case "dsp": cap = maximum < 15 ? 3 : maximum < 19 ? 15 : maximum < 22 ? 19 : 22
        case "catNetwork": cap = maximum < 15 ? 3 : maximum < 21 ? 15 : 21
        case "audio": cap = maximum < 15 ? 3 : maximum < 24 ? 15 : 24
        case "diagnostics": cap = maximum < 15 ? 3 : 15
        case "appearance": cap = maximum < 7 ? 4 : maximum < 12 ? 7 : 12
        case "display": cap = maximum < 8 ? 4 : maximum < 12 ? 11 : 12
        default: cap = 3
        }
        return Int(min(maximum, cap))
    }

    private func invalidate() {
        let hadValue = !statuses.isEmpty || revision != nil || !categoryOrder.isEmpty
        statuses = [:]
        revision = nil
        categoryOrder = []
        publishedValues = [:]
        publishedSchemaIdentity = nil
        watch(nil)
        if hadValue { generation &+= 1 }
    }

    private func queueRefresh() {
        guard !refreshQueued else { return }
        refreshQueued = true
        // MirrorStore's publishers fire in willSet; read after the mutation.
        Task { @MainActor [weak self] in self?.refresh() }
    }

    private func refresh() {
        refreshQueued = false
        guard store.isSnapshotComplete, !store.isStale,
              (store.agreedMinor ?? 0) >= Self.minimumMinor,
              Self.supports(store.capabilities["setupDescriptionVersion"]),
              let schemaIdentity = store.currentSessionSchemaIdentity(ofClass: Self.className),
              let object = store.object(Self.objectKey), object.className == Self.className else {
            invalidate()
            return
        }
        watch(object)
        let names = store.currentSessionPropertyNames(ofClass: Self.className) ?? []
        let ordered = names.filter { $0 != "revision" }
        guard !ordered.isEmpty,
              case .int(let rawRevision)? = object["revision"],
              (0...Int64(UInt32.max)).contains(rawRevision) else {
            let failure = Dictionary(uniqueKeysWithValues: ordered.map { ($0, Status.unavailable("Setup schema or revision is invalid.")) })
            publish(failure, order: ordered, revision: nil, values: object.values,
                    schemaIdentity: schemaIdentity)
            return
        }
        let version: Int
        switch store.capabilities["setupDescriptionVersion"] {
        case .int(let value)?, .enumeration(let value)?: version = Int(value)
        default: version = 0
        }
        var next: [String: Status] = [:]
        for name in ordered {
            guard case .text(let json)? = object[name] else {
                next[name] = .unavailable("Setup category has the wrong value type.")
                continue
            }
            if json.isEmpty {
                next[name] = .unpublished
                continue
            }
            do {
                let parsed = try SetupDescription.parse(json: json)
                if parsed.version == Self.projectedVersion(maximum: Int64(version), category: name)
                    && parsed.category.id == name {
                    next[name] = .available(parsed)
                } else {
                    next[name] = .unavailable("Setup category version or identity does not match this session.")
                }
            } catch let error as SetupDescription.ParseError {
                next[name] = .unavailable(error.reason)
            } catch {
                next[name] = .unavailable("Setup category is invalid.")
            }
        }
        publish(next, order: ordered, revision: UInt32(rawRevision), values: object.values,
                schemaIdentity: schemaIdentity)
    }

    private func publish(_ next: [String: Status], order: [String], revision nextRevision: UInt32?,
                         values: [String: MirrorValue], schemaIdentity: UInt64) {
        if next != statuses || order != categoryOrder || nextRevision != revision
            || values != publishedValues || schemaIdentity != publishedSchemaIdentity {
            statuses = next
            categoryOrder = order
            revision = nextRevision
            publishedValues = values
            publishedSchemaIdentity = schemaIdentity
            generation &+= 1
        }
    }

    private func watch(_ object: MirrorObject?) {
        guard object !== watched else { return }
        watched = object
        watchedValues = object?.$values.sink { [weak self] _ in self?.queueRefresh() }
    }
}
