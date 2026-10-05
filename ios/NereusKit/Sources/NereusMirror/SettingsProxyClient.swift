// NereusSDR for iOS: the settings the Core keeps, cached in the app and written through to it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import os

/// The settings proxy (link document section 8): the Core's station-scoped
/// settings, a flat space of string keys and values, held here as a local
/// cache of its snapshot and written through to it. Only keys the Core
/// keeps are written; each app keeps the others itself.
///
/// A write updates the cache at once and is answered by `settings.value`
/// carrying this app's `origin` (its echo) or by `settings.reject`, which
/// puts back the Core's value (or no value, for a key it does not keep).
/// Only `settings.write` and `settings.value` carry an origin: the Core's
/// own changes and every removal, this app's included, come back with
/// origin "".
///
/// As the desktop's remote window keeps the operator's value until the Core
/// answers (`StationClient.cpp:1040-1068`, `handleDelta` `:4174-4214`), a
/// key this app has written keeps its value over another device's change
/// or a later snapshot until that answer; the Core's value is recorded and
/// shows on a refusal without one or on a lost link (`:2219`). A write
/// with no answer within ``MirrorStore/answerDeadline`` returns to the latest
/// Core value, marked as not confirmed (``unconfirmedKeys``), while its
/// actual pending answer remains observable.
@MainActor
public final class SettingsProxyClient: ObservableObject {
    public typealias Sender = MirrorStore.Sender
    /// A sender captured at admission and checked again at transport handoff.
    public typealias BoundSender = @Sendable (LinkMessage, CommandSendPermit) async throws -> Void
    public typealias CaptureSender = @Sendable () -> BoundSender?

    /// The station-scoped settings as this app last knew them.
    @Published public private(set) var values: [String: String] = [:]
    /// Nil until this authenticated session's first settings snapshot. A
    /// later radio's snapshot merges without changing the identity.
    @Published public private(set) var currentSnapshotIdentity: UInt64?
    /// This app's origin tag in its writes, by which it knows its own echo.
    public let origin: String
    /// Keys whose latest write had no answer within the deadline.
    @Published public private(set) var unconfirmedKeys: Set<String> = []

    /// Core events before cache publication. Closed multi-key editors distinguish
    /// their echo from outside changes without reading an optimistic value.
    struct CoreChange {
        let key: String
        let value: String?
        let ownEcho: Bool
    }
    let coreChanges = PassthroughSubject<CoreChange, Never>()

    var hasPendingFilterPresets: Bool { pending.keys.contains { $0.hasPrefix("filters/") } }

    func confirmedValue(_ key: String) -> String? {
        if let held = coreValues[key] { return held }
        return values[key]
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.settings")

    private struct Operation {
        let token: UInt64
        let isRemoval: Bool
        let onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)?
        /// Legacy removals return after sending; bound removals await an
        /// echo. Nil once handed back as not confirmed.
        var resolve: CheckedContinuation<SettingsWriteOutcome, Never>?
        var deadline: (any LinkTimer)?
        var expired = false
    }

    private let send: Sender
    private let captureSender: CaptureSender?
    private let clock: any LinkClock
    /// The Core's last value of each key this app has a write pending on.
    private var coreValues: [String: String?] = [:]
    /// This app's writes and removals the Core has not answered, per key, in order.
    private var pending: [String: [Operation]] = [:]
    private var sendTails: [String: Task<Void, Never>] = [:]
    /// The next `settings.snapshot` is the session's first, which replaces
    /// the cache; later ones (a radio that arrived late) merge into it.
    private var nextSnapshotReplaces = false
    private var linkUp = false
    private var nextToken: UInt64 = 0
    private var latestTokens: [String: UInt64] = [:]
    private var nextSnapshotIdentity: UInt64 = 0
    private var sessionPermit = CommandSendPermit()

    /// A plain sender is useful for in-memory clients. It cannot guarantee a
    /// revoked permit is checked at final transport handoff after suspension.
    public init(origin: String = UUID().uuidString, send: @escaping Sender,
                captureSender: CaptureSender? = nil, clock: any LinkClock = SystemLinkClock()) {
        self.origin = origin
        self.send = send
        self.captureSender = captureSender
        self.clock = clock
    }

    /// A proxy that sends through `session`. Feed it the session's events
    /// with `handle(_:)`.
    public convenience init(session: StationSession, origin: String = UUID().uuidString) {
        self.init(origin: origin, send: { message in try await session.send(message) },
                  captureSender: { { message, permit in try await session.send(message, permit: permit) } })
    }

    // MARK: Reading

    /// A setting's value as the cache holds it, or nil when the Core has none.
    public func value(_ key: String) -> String? {
        values[key]
    }

    public func isCurrent(_ expectedSnapshotIdentity: UInt64) -> Bool {
        currentSnapshotIdentity == expectedSnapshotIdentity
    }

    /// True while the latest write of `key` has had no answer within the deadline.
    public func isUnconfirmed(_ key: String) -> Bool {
        unconfirmedKeys.contains(key)
    }

    // MARK: Feeding

    /// One session event.
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(let message):
            apply(message)
        case .stateChanged(let state):
            let up = state == .receivingSnapshot || state == .ready
            if linkUp && !up {
                linkLost()
            }
            linkUp = up
        case .refused:
            break
        }
    }

    /// One message from the Core.
    public func apply(_ message: LinkMessage) {
        switch message {
        case .authResult(let result):
            retireSession()
            if result.accepted {
                nextSnapshotIdentity &+= 1
                nextSnapshotReplaces = true
            } else {
                nextSnapshotReplaces = false
            }
        case .settingsSnapshot(let snapshot):
            let firstOfSession = nextSnapshotReplaces
            var next = firstOfSession ? [:] : values
            nextSnapshotReplaces = false
            for entry in snapshot.properties {
                if case .utf8(let text) = entry.value {
                    coreChanges.send(CoreChange(key: entry.name, value: text, ownEcho: false))
                    if !firstOfSession, pending[entry.name] != nil {
                        // A key this app wrote keeps its value until the answer.
                        coreValues[entry.name] = text
                        if holdsValue(entry.name) { continue }
                    }
                    next[entry.name] = text
                }
            }
            set(next)
            if firstOfSession && linkUp {
                currentSnapshotIdentity = nextSnapshotIdentity
            }
        case .settingsValue(let change):
            applyValue(change)
        case .settingsReject(let reject):
            applyReject(reject)
        default:
            break
        }
    }

    // MARK: Writing

    /// Writes one setting the Core keeps: the cache takes the value at once,
    /// and the answer is the Core's echo or its refusal. A key each app keeps
    /// itself is not sent.
    public func write(_ key: String, _ value: String,
                      onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)? = nil) async -> SettingsWriteOutcome {
        await writePrepared(key, value, expectedSnapshotIdentity: currentSnapshotIdentity, authority: nil, onLateOutcome: onLateOutcome)
    }

    /// Admit a described control only against its captured settings snapshot.
    /// `authority` belongs to the caller and may be revoked when that gesture
    /// or description expires. The sender must honor the permit at handoff.
    public func writeBound(_ key: String, _ value: String, expectedSnapshotIdentity: UInt64,
                           authority: CommandSendPermit, capturedSender: BoundSender? = nil,
                           onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)? = nil) async -> SettingsWriteOutcome {
        await writePrepared(key, value, expectedSnapshotIdentity: expectedSnapshotIdentity, authority: authority, capturedSender: capturedSender, onLateOutcome: onLateOutcome)
    }

    private func writePrepared(_ key: String, _ value: String, expectedSnapshotIdentity: UInt64?,
                               authority: CommandSendPermit?, capturedSender: BoundSender? = nil,
                               onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)?) async -> SettingsWriteOutcome {
        guard SettingsScope.of(key) == .station else {
            return .keptOnThisDevice
        }
        guard let expectedSnapshotIdentity, isCurrent(expectedSnapshotIdentity),
              linkUp, authority?.isRevoked != true else { return .notSent }
        let message = LinkMessage.settingsWrite(LinkMessage.SettingsWrite(
            key: key, origin: origin,
            properties: [LinkMessage.PropertyEntry(name: key, value: .utf8(value))]))
        let previous = values[key]
        guard let sender = capturedSender ?? admittedSender() else { return .notSent }
        let permit = CommandSendPermit(parents: [sessionPermit] + [authority].compactMap { $0 })
        return await withCheckedContinuation { continuation in
            _ = start(key: key, message: message, value: value, previous: previous,
                      isRemoval: false, resolve: continuation, sender: sender, permit: permit, onLateOutcome: onLateOutcome)
        }
    }

    /// Removes one setting the Core keeps: the cache drops it at once. A
    /// key each app keeps itself is not sent.
    public func remove(_ key: String) async {
        guard SettingsScope.of(key) == .station else {
            return
        }
        guard currentSnapshotIdentity != nil, linkUp else { return }
        guard let sender = admittedSender() else { return }
        let permit = CommandSendPermit(parents: [sessionPermit])
        let sending = startRemoval(key, resolve: nil, sender: sender, permit: permit)
        await sending.value
    }

    /// A bound removal resolves on its echo or refusal, with the same outcome
    /// rules as a bound write. Stale admission has no cache effect.
    public func removeBound(_ key: String, expectedSnapshotIdentity: UInt64,
                            authority: CommandSendPermit, capturedSender: BoundSender? = nil,
                            onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)? = nil) async -> SettingsWriteOutcome {
        guard SettingsScope.of(key) == .station else { return .keptOnThisDevice }
        guard isCurrent(expectedSnapshotIdentity), linkUp, !authority.isRevoked else { return .notSent }
        guard let sender = capturedSender ?? admittedSender() else { return .notSent }
        let permit = CommandSendPermit(parents: [sessionPermit, authority])
        return await withCheckedContinuation { continuation in
            _ = startRemoval(key, resolve: continuation, sender: sender, permit: permit, onLateOutcome: onLateOutcome)
        }
    }

    private func startRemoval(_ key: String, resolve: CheckedContinuation<SettingsWriteOutcome, Never>?,
                              sender: @escaping BoundSender, permit: CommandSendPermit,
                              onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)? = nil) -> Task<Void, Never> {
        let message = LinkMessage.settingsRemove(LinkMessage.SettingsRemove(key: key))
        let previous = values[key]
        return start(key: key, message: message, value: nil, previous: previous,
                     isRemoval: true, resolve: resolve, sender: sender, permit: permit, onLateOutcome: onLateOutcome)
    }

    // MARK: Inside

    func admittedSender() -> BoundSender? {
        if let captureSender { return captureSender() }
        let send = send
        return { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            try await send(message)
        }
    }

    private func start(key: String, message: LinkMessage, value: String?, previous: String?,
                       isRemoval: Bool, resolve: CheckedContinuation<SettingsWriteOutcome, Never>?,
                       sender: @escaping BoundSender, permit: CommandSendPermit,
                       onLateOutcome: (@MainActor (SettingsWriteOutcome) -> Void)? = nil) -> Task<Void, Never> {
        nextToken &+= 1
        let token = nextToken
        latestTokens[key] = token
        if pending[key] == nil {
            coreValues[key] = .some(previous)
        }
        pending[key, default: []].append(Operation(token: token, isRemoval: isRemoval, onLateOutcome: onLateOutcome, resolve: resolve))
        if resolve != nil {
            let deadline = clock.schedule(after: MirrorStore.answerDeadline) { [weak self] in
                await self?.deadlinePassed(key: key, token: token)
            }
            if let index = pending[key]?.firstIndex(where: { $0.token == token }) {
                pending[key]?[index].deadline = deadline
            }
        }
        setUnconfirmed(key, false)
        setValue(value, for: key)
        let previousSend = sendTails[key]
        let sending = Task { @MainActor in
            await previousSend?.value
            guard self.pending[key]?.contains(where: { $0.token == token }) == true else { return }
            guard !permit.isRevoked else {
                self.unsent(key: key, token: token, previous: previous)?.resolve?.resume(returning: .notSent)
                return
            }
            do {
                try await sender(message, permit)
                // A successful bound return means the transport accepted its
                // handoff. Later revocation must not discard its pending echo.
            } catch {
                Self.logger.info("A setting was not sent: \(String(describing: error), privacy: .public)")
                self.unsent(key: key, token: token, previous: previous)?.resolve?.resume(returning: .notSent)
            }
        }
        sendTails[key] = sending
        return sending
    }

    /// The Core's `settings.value`: this app's own echo (its origin, or an
    /// empty origin and no value answering its oldest removal) settles that
    /// operation; anything else is another device's or the Core's change.
    private func applyValue(_ change: LinkMessage.SettingsValue) {
        let carried = Self.text(change.properties)
        var queue = pending[change.key] ?? []
        let ownEcho: Bool
        if let oldest = queue.first {
            ownEcho = oldest.isRemoval ? change.origin.isEmpty && change.properties.isEmpty
                : change.origin == origin
        } else {
            ownEcho = false
        }
        coreChanges.send(CoreChange(key: change.key, value: carried, ownEcho: ownEcho))
        if ownEcho {
            let settled = queue.removeFirst()
            pending[change.key] = queue.isEmpty ? nil : queue
            receiveCore(carried, key: change.key, token: settled.token)
            settle(settled, .accepted, key: change.key)
            return
        }
        if !queue.isEmpty { coreValues[change.key] = .some(carried) }
        if !holdsValue(change.key) { setValue(carried, for: change.key) }
    }

    /// Only the current operation can replace an expired current display.
    /// Older FIFO answers still update Core behind a newer active hold.
    private func receiveCore(_ value: String?, key: String, token: UInt64) {
        let latest = latestTokens[key] == token
        guard latest || holdsValue(key) else { return }
        if pending[key] != nil { coreValues[key] = .some(value) }
        else { coreValues[key] = nil }
        if latest || !holdsValue(key) { setValue(value, for: key) }
    }

    private func holdsValue(_ key: String) -> Bool {
        pending[key]?.contains { $0.token == latestTokens[key] && !$0.expired } == true
    }

    private func applyReject(_ reject: LinkMessage.SettingsReject) {
        coreChanges.send(CoreChange(key: reject.key, value: Self.text(reject.properties), ownEcho: true))
        var queue = pending[reject.key] ?? []
        guard !queue.isEmpty else {
            setValue(Self.text(reject.properties), for: reject.key)
            return
        }
        let settled = queue.removeFirst()
        pending[reject.key] = queue.isEmpty ? nil : queue
        receiveCore(Self.text(reject.properties), key: reject.key, token: settled.token)
        settle(settled, .rejected(reason: reject.reason ?? ""), key: reject.key)
    }

    /// An operation that never left: take it back, and the cache with it
    /// when nothing later of this app's is waiting on the key. Returns the
    /// operation, or nil when a lost link already settled it.
    @discardableResult
    private func unsent(key: String, token: UInt64, previous: String?) -> Operation? {
        var queue = pending[key] ?? []
        guard let index = queue.firstIndex(where: { $0.token == token }) else {
            return nil
        }
        let operation = queue.remove(at: index)
        operation.deadline?.cancel()
        pending[key] = queue.isEmpty ? nil : queue
        if queue.isEmpty {
            setUnconfirmed(key, false)
            // The Core's value, as recorded while this write waited.
            let core: String? = coreValues.removeValue(forKey: key) ?? previous
            setValue(core, for: key)
        } else if index == queue.count {
            setValue(previous, for: key)
        }
        return operation
    }

    private func linkLost() {
        retireSession()
        nextSnapshotReplaces = false
    }

    private func retireSession() {
        currentSnapshotIdentity = nil
        sessionPermit.revoke()
        sessionPermit = CommandSendPermit()
        sendTails = [:]
        let waiting = pending
        pending = [:]
        // The Core's values come back for what this app had waiting
        // (StationClient.cpp:2219); nothing is sent again.
        let core = coreValues
        coreValues = [:]
        for (key, queue) in waiting {
            for operation in queue {
                operation.deadline?.cancel()
                if let resolve = operation.resolve {
                    resolve.resume(returning: .linkLost)
                } else if latestTokens[key] == operation.token {
                    operation.onLateOutcome?(.linkLost)
                }
            }
            if let value = core[key] {
                setValue(value, for: key)
            }
        }
        if !unconfirmedKeys.isEmpty {
            unconfirmedKeys = []
        }
    }

    /// An answer settles an operation: its waiter hears it, or, when it
    /// was already handed back as not confirmed, the mark goes.
    private func settle(_ operation: Operation, _ outcome: SettingsWriteOutcome, key: String) {
        operation.deadline?.cancel()
        if let resolve = operation.resolve {
            resolve.resume(returning: outcome)
        } else if latestTokens[key] == operation.token {
            operation.onLateOutcome?(outcome)
        }
        if latestTokens[key] == operation.token { setUnconfirmed(key, false) }
    }

    /// No answer within the deadline: the latest Core value shows, the waiter hears
    /// it was not confirmed, and the key is marked when this was its
    /// latest write.
    private func deadlinePassed(key: String, token: UInt64) {
        guard var queue = pending[key], let index = queue.firstIndex(where: { $0.token == token }),
              let resolve = queue[index].resolve else {
            return
        }
        queue[index].resolve = nil
        queue[index].deadline = nil
        queue[index].expired = true
        pending[key] = queue
        Self.logger.info("A setting had no answer from the Core within 5 s; the latest Core value shows, not confirmed")
        if latestTokens[key] == token {
            setValue(coreValues[key] ?? nil, for: key)
            setUnconfirmed(key, true)
        }
        resolve.resume(returning: .notConfirmed)
    }

    private func setUnconfirmed(_ key: String, _ on: Bool) {
        if on != unconfirmedKeys.contains(key) {
            if on {
                unconfirmedKeys.insert(key)
            } else {
                unconfirmedKeys.remove(key)
            }
        }
    }

    private func setValue(_ value: String?, for key: String) {
        guard values[key] != value else {
            return
        }
        values[key] = value
    }

    private func set(_ next: [String: String]) {
        if next != values {
            values = next
        }
    }

    /// A setting's value from its entry, or nil when there is none.
    private static func text(_ entries: [LinkMessage.PropertyEntry]) -> String? {
        guard let entry = entries.first, case .utf8(let text) = entry.value else {
            return nil
        }
        return text
    }
}
