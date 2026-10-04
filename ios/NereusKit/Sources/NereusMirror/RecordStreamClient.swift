// NereusSDR for iOS: the Core's record streams (its spots, its sources' consoles), subscribed and kept in step
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import os

/// The Core's record streams (link document section 7.7): lists of records
/// that come and go, such as its spots and each spot source's console. The
/// app says which streams it wants with ``want(_:backlog:)``; once the
/// snapshot is complete, on a Core that advertises `recordStreamVersion` 1,
/// each is asked for with `records.subscribe`, and again after every
/// reconnect. The Core answers with a `record.batch` that resets the copy
/// here, then sends only upserts and removes.
///
/// A copy holds each stream's records oldest first, as the Core keeps them,
/// bounded by the stream's capacity. A reset replaces the copy; an upsert
/// of a record the copy holds changes it in place, and a new one goes last;
/// a remove of a record it does not hold is ignored. Every copy is dropped
/// when the link goes down, as a window drops the Core's spots when its
/// session ends, so nothing old shows while the Core is out of reach.
@MainActor
public final class RecordStreamClient: ObservableObject {
    /// One stream as this app holds it.
    public struct Stream: Equatable, Sendable {
        /// Rises when the Core clears the stream.
        public var generation: Int64
        /// Oldest first.
        public var records: [LinkMessage.RecordBatch.Record]

        public init(generation: Int64, records: [LinkMessage.RecordBatch.Record]) {
            self.generation = generation
            self.records = records
        }
    }

    /// The capability that gates the record streams and the spot verbs.
    public static let capabilityName = "recordStreamVersion"
    /// The first `recordStreamVersion` whose `spots` records may carry
    /// `resolvedMode`: at this version an absent field means the Core has
    /// no mode for that spot; below it the Core cannot say.
    public static let resolvedModeVersion: Int64 = 2
    public static let subscribeVerb = "records.subscribe"
    public static let unsubscribeVerb = "records.unsubscribe"
    /// The Core's spots.
    public static let spotsStream = "spots"
    /// Each station spot source's console: `spotConsole:dxCluster`.
    public static func consoleStream(_ source: String) -> String {
        "spotConsole:\(source)"
    }

    /// The most a stream holds (link 7.7); an unknown stream keeps the spots' bound.
    public static func capacity(of stream: String) -> Int {
        if stream == FreeDVStation.streamName {
            return FreeDVStation.capacity
        }
        if stream == StationTciClient.streamName {
            return StationTciClient.capacity
        }
        if stream == StationRadio.streamName {
            return StationRadio.capacity
        }
        if stream == CoreLogLine.streamName {
            return CoreLogLine.capacity
        }
        if stream == AmModulationReading.txStream || stream == AmModulationReading.feedbackStream {
            return AmModulationReading.capacity
        }
        if stream == StationVax.levelsStream {
            return StationVax.levelsCapacity
        }
        return stream.hasPrefix("spotConsole:") ? 200 : 500
    }

    /// Each stream this app holds, by name.
    @Published public private(set) var streams: [String: Stream] = [:]
    /// The Core's words for a stream it would not send, by name.
    @Published public private(set) var refusals: [String: String] = [:]
    /// The Core sends record streams to this app: the snapshot is complete
    /// and it advertises `recordStreamVersion` 1.
    @Published public private(set) var available = false
    /// The Core's `recordStreamVersion` while the link is up and the
    /// snapshot complete; 0 otherwise.
    @Published public private(set) var version: Int64 = 0

    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.records")

    private let mirror: MirrorStore
    private let commands: CommandClient
    private let timeout: Duration
    /// The streams this app wants, with the backlog asked for: the most any
    /// of the stream's users asks for.
    private var wanted: [String: Int] = [:]
    /// Who in this app wants each stream, with the backlog each asks for.
    /// A stream is asked for once however many use it, and stopped only
    /// when the last of them goes.
    private var users: [String: [ObjectIdentifier: Int]] = [:]
    /// One stream's requests on the current link. A timed-out request may
    /// have started it; a refused refresh cannot stop an earlier subscription.
    private struct Subscription {
        let authority = CommandSendPermit()
        var latestAttempt: UUID?
        var pending: Set<UUID> = []
        var mayBeActive = false
    }
    private var subscriptions: [String: Subscription] = [:]
    /// Last-user cleanup remains owned after its subscription is removed.
    /// A new want or a retired link revokes it before actor admission or handoff.
    private var unsubscribeAuthorities: [String: CommandSendPermit] = [:]
    private var linkUp = false

    /// The latest launched record command, through its local outcome handler.
    /// Its task records operation settlement separately from transport handoff.
    private(set) var recordCommandTask: Task<Void, Never>?

    #if DEBUG
    /// A nil-by-default barrier before command actor admission.
    var beforeRecordSubscribeAdmissionForTesting: (@Sendable () async -> Void)?
    /// A nil-by-default barrier before unsubscribe command actor admission.
    var beforeRecordUnsubscribeAdmissionForTesting: (@Sendable () async -> Void)?
    #endif

    public init(mirror: MirrorStore, commands: CommandClient, timeout: Duration = .seconds(5)) {
        self.mirror = mirror
        self.commands = commands
        self.timeout = timeout
    }

    /// The records of `stream` this app holds, oldest first.
    public func records(_ stream: String) -> [LinkMessage.RecordBatch.Record] {
        streams[stream]?.records ?? []
    }

    // MARK: Wanting

    /// Stands for every ask made without naming who asks: all such asks
    /// count as one user.
    private final class Unnamed {}
    private static let unnamed = Unnamed()

    /// Whether this app asks for `stream` now, for anyone.
    public func isWanted(_ stream: String) -> Bool {
        wanted[stream] != nil
    }

    /// Whether `user` asks for `stream` now.
    public func isWanted(_ stream: String, by user: AnyObject) -> Bool {
        users[stream]?[ObjectIdentifier(user)] != nil
    }

    /// ``want(_:backlog:by:)`` for an ask that names no user.
    public func want(_ stream: String, backlog: Int) {
        want(stream, backlog: backlog, by: Self.unnamed)
    }

    /// Asks for `stream` on behalf of `user`, carrying the newest `backlog`
    /// records, now if the Core can send it and after every reconnect. The
    /// stream is asked for once however many users want it, with the most
    /// backlog any of them asks for, and asked for again when that most
    /// changes. Asking again with the same backlog changes nothing.
    public func want(_ stream: String, backlog: Int, by user: AnyObject) {
        let backlog = max(0, min(backlog, Self.capacity(of: stream)))
        unsubscribeAuthorities.removeValue(forKey: stream)?.revoke()
        users[stream, default: [:]][ObjectIdentifier(user)] = backlog
        let most = users[stream]?.values.max() ?? backlog
        guard wanted[stream] != most else {
            return
        }
        wanted[stream] = most
        subscriptions[stream]?.latestAttempt = nil
        subscribeWanted()
    }

    /// Asks for a wanted `stream` again without stopping it first, as a
    /// window's Refresh does (link 7.7): the Core answers with a fresh reset
    /// of its backlog. Nothing happens for a stream this app does not want.
    public func resubscribe(_ stream: String) {
        guard wanted[stream] != nil else {
            return
        }
        subscriptions[stream]?.latestAttempt = nil
        subscribeWanted()
    }

    /// ``unwant(_:by:)`` for an ask that named no user.
    public func unwant(_ stream: String) {
        unwant(stream, by: Self.unnamed)
    }

    /// `user` no longer wants `stream`. When no other user wants it, the
    /// stream is stopped and this app's copy of it dropped; while another
    /// does, it runs on untouched. Nothing happens for a user that did not
    /// ask for it.
    public func unwant(_ stream: String, by user: AnyObject) {
        guard users[stream]?.removeValue(forKey: ObjectIdentifier(user)) != nil else {
            return
        }
        if let left = users[stream]?.values.max() {
            // Another user still reads it: it runs on as the Core sends it.
            wanted[stream] = left
            return
        }
        users[stream] = nil
        guard wanted.removeValue(forKey: stream) != nil else {
            return
        }
        streams[stream] = nil
        refusals[stream] = nil
        guard let subscription = subscriptions.removeValue(forKey: stream) else {
            return
        }
        subscription.authority.revoke()
        guard subscription.mayBeActive || !subscription.pending.isEmpty else {
            return
        }
        let authority = CommandSendPermit()
        unsubscribeAuthorities[stream] = authority
        let commands = commands
        let timeout = timeout
        #if DEBUG
        let beforeAdmission = beforeRecordUnsubscribeAdmissionForTesting
        #endif
        recordCommandTask = Task { [weak self] in
            defer { self?.unsubscribeAnswered(stream, authority: authority) }
            #if DEBUG
            await beforeAdmission?()
            #endif
            do {
                _ = try await commands.invoke(Self.unsubscribeVerb,
                                              arguments: [CommandArgument(name: "stream", value: .text(stream))],
                                              timeout: timeout, authority: authority)
            } catch {
                Self.logger.info("A record stream was not stopped: \(String(describing: error), privacy: .public)")
            }
        }
    }

    // MARK: Feeding

    /// One session event. Feed it after the mirror has handled it, so the
    /// Core's capabilities are known when the snapshot completes.
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(.recordBatch(let batch)):
            apply(batch)
        case .message:
            break
        case .stateChanged(let state):
            let up = state == .receivingSnapshot || state == .ready
            if linkUp && !up {
                linkLost()
            }
            linkUp = up
            if state == .ready {
                version = mirror.capabilityVersion(Self.capabilityName)
                available = version >= 1
                subscribeWanted()
            }
        case .refused:
            break
        }
    }

    /// One `record.batch`. A batch for a stream this app does not want (one
    /// just stopped) is dropped.
    public func apply(_ batch: LinkMessage.RecordBatch) {
        guard wanted[batch.stream] != nil else {
            return
        }
        var stream = streams[batch.stream] ?? Stream(generation: batch.generation, records: [])
        if batch.reset {
            stream = Stream(generation: batch.generation, records: [])
        } else if batch.generation > stream.generation {
            // The Core cleared the stream; its reset follows, and meanwhile nothing old stays.
            stream = Stream(generation: batch.generation, records: [])
        }
        var index: [String: Int] = [:]
        for (position, record) in stream.records.enumerated() {
            index[record.id] = position
        }
        for record in batch.upserts {
            if let position = index[record.id] {
                stream.records[position] = record
            } else {
                index[record.id] = stream.records.count
                stream.records.append(record)
            }
        }
        if !batch.removes.isEmpty {
            let removed = Set(batch.removes)
            stream.records.removeAll { removed.contains($0.id) }
        }
        let capacity = Self.capacity(of: batch.stream)
        if stream.records.count > capacity {
            stream.records.removeFirst(stream.records.count - capacity)
        }
        streams[batch.stream] = stream
    }

    // MARK: Spots

    /// The name of the `spots` field that carries the slice mode the Core
    /// worked out for the spot (the slice's `dspMode` wire value).
    public static let resolvedModeField = "resolvedMode"

    /// The slice mode the Core worked out for a `spots` record, or nil when
    /// the record carries none (or carries something that is not a whole,
    /// non-negative number). Read it only from a Core at
    /// ``resolvedModeVersion`` or later.
    public static func resolvedMode(of record: LinkMessage.RecordBatch.Record) -> Int? {
        guard case .number(let value)? = record.fields[resolvedModeField], value.isFinite, value >= 0,
              value <= 65_535, value == value.rounded() else {
            return nil
        }
        return Int(value)
    }

    // MARK: Inside

    private func subscribeWanted() {
        guard linkUp, available else {
            return
        }
        for (stream, backlog) in wanted.sorted(by: { $0.key < $1.key }) where subscriptions[stream]?.latestAttempt == nil {
            subscribe(stream, backlog: backlog)
        }
    }

    private func subscribe(_ stream: String, backlog: Int) {
        let attempt = UUID()
        var subscription = subscriptions[stream] ?? Subscription()
        subscription.latestAttempt = attempt
        subscription.pending.insert(attempt)
        subscriptions[stream] = subscription
        let authority = subscription.authority
        let commands = commands
        let timeout = timeout
        #if DEBUG
        let beforeAdmission = beforeRecordSubscribeAdmissionForTesting
        #endif
        recordCommandTask = Task { [weak self] in
            #if DEBUG
            await beforeAdmission?()
            #endif
            do {
                let result = try await commands.invoke(Self.subscribeVerb, arguments: [
                    CommandArgument(name: "stream", value: .text(stream)),
                    CommandArgument(name: "backlog", value: .int(Int64(backlog))),
                ], timeout: timeout, authority: authority)
                self?.answered(stream, attempt: attempt, result)
            } catch {
                Self.logger.info("A record stream was not asked for: \(String(describing: error), privacy: .public)")
                self?.failed(stream, attempt: attempt, error)
            }
        }
    }

    private func answered(_ stream: String, attempt: UUID, _ result: CommandResult) {
        guard var subscription = subscriptions[stream],
              subscription.pending.remove(attempt) != nil else {
            return
        }
        if result.accepted {
            subscription.mayBeActive = true
        }
        subscriptions[stream] = subscription
        guard subscription.latestAttempt == attempt, wanted[stream] != nil else {
            return
        }
        refusals[stream] = result.accepted ? nil : result.reason
    }

    private func failed(_ stream: String, attempt: UUID, _ error: Error) {
        guard var subscription = subscriptions[stream],
              subscription.pending.remove(attempt) != nil else {
            return
        }
        // Only notSent establishes no handoff. Link loss ends that session;
        // timeout (or cancellation after handoff) leaves the remote state uncertain.
        let failure = error as? CommandError
        if failure != .notSent && failure != .linkLost {
            subscription.mayBeActive = true
        }
        if subscription.latestAttempt == attempt {
            // Asked again on the next reconnect, or the next want.
            subscription.latestAttempt = nil
        }
        subscriptions[stream] = subscription
    }

    private func unsubscribeAnswered(_ stream: String, authority: CommandSendPermit) {
        guard unsubscribeAuthorities[stream] === authority else { return }
        unsubscribeAuthorities[stream] = nil
    }

    private func linkLost() {
        for authority in unsubscribeAuthorities.values {
            authority.revoke()
        }
        unsubscribeAuthorities = [:]
        for subscription in subscriptions.values {
            subscription.authority.revoke()
        }
        streams = [:]
        subscriptions = [:]
        available = false
        version = 0
    }
}
