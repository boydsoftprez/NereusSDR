// NereusSDR for iOS: the command client's and media control's own ordered lanes for one session's events
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia
import NereusMirror

/// One consumer's share of the Core's messages, in the order they came,
/// handed over by a task of its own, off the main thread. The app's event
/// pump puts an item in and goes straight on to the next message; the
/// lane's task hands each item to its consumer and waits for it before the
/// next, so the consumer sees its items in arrival order and one at a time,
/// as it did when the pump waited on it. Nothing put in is dropped while
/// the lane delivers: it holds what its consumer has not taken yet, and
/// only the messages that consumer reads ever go in, never the Core's
/// readings, so a flood of readings never grows it.
final class CoreEventLane<Item: Sendable>: Sendable {
    private enum Entry: Sendable {
        case item(Item)
        /// Resumed once every item put in before it has been handed over.
        case fence(CheckedContinuation<Void, Never>)
    }

    /// Whether items still go to the consumer, and whether the task ended.
    private final class State: @unchecked Sendable {
        private let lock = NSLock()
        private var delivering = true
        private var ended = false
        private var pending = 0
        #if DEBUG
        private var maximumPending = 0
        #endif

        var isDelivering: Bool { lock.withLock { delivering } }
        var hasEnded: Bool { lock.withLock { ended } }
        var pendingCount: Int { lock.withLock { pending } }
        #if DEBUG
        var maximumPendingCount: Int { lock.withLock { maximumPending } }
        #endif
        func addItem() {
            lock.withLock {
                pending += 1
                #if DEBUG
                maximumPending = max(maximumPending, pending)
                #endif
            }
        }
        func completeItem() { lock.withLock { pending -= 1 } }
        func stopDelivering() { lock.withLock { delivering = false } }
        func end() { lock.withLock { ended = true } }
    }

    private let entries: AsyncStream<Entry>.Continuation
    private let consumer: Task<Void, Never>
    private let state: State

    /// Starts the lane's task, at the priority of the task that makes it.
    init(deliver: @escaping @Sendable (Item) async -> Void) {
        let (stream, entries) = AsyncStream<Entry>.makeStream()
        self.entries = entries
        let state = State()
        self.state = state
        consumer = Task {
            for await entry in stream {
                switch entry {
                case .item(let item):
                    if state.isDelivering {
                        await deliver(item)
                    }
                    state.completeItem()
                case .fence(let done):
                    done.resume()
                }
            }
            state.end()
        }
    }

    deinit {
        entries.finish()
    }

    /// Puts `item` in, after everything put in before it.
    func send(_ item: Item) {
        state.addItem()
        if case .terminated = entries.yield(.item(item)) {
            state.completeItem()
        }
    }

    /// Returns once everything put in so far has been handed over, or
    /// skipped once the lane stopped delivering, or at once when the lane
    /// has been finished.
    func drain() async {
        await withCheckedContinuation { (done: CheckedContinuation<Void, Never>) in
            if case .terminated = entries.yield(.fence(done)) {
                done.resume()
            }
        }
    }

    var pendingCount: Int { state.pendingCount }
    #if DEBUG
    var maximumPendingCount: Int { state.maximumPendingCount }
    #endif

    /// Whether items still go to the consumer.
    var isDelivering: Bool { state.isDelivering }

    /// The session the items came from is no longer the app's: what is
    /// still in the lane is skipped, as the pump skipped a stale session's
    /// messages.
    func stopDelivering() {
        state.stopDelivering()
    }

    /// Whether the lane's task has ended.
    var hasEnded: Bool { state.hasEnded }

    /// Ends the lane: what is in it is still handed over (or skipped once
    /// stopped), then its task ends. Returns once it has.
    func finish() async {
        entries.finish()
        await consumer.value
    }
}

/// One session reader's lossless FIFO before the main actor takes ready
/// prefixes. Full admission waits; a ready prefix never waits to fill.
/// These are internal scheduling bounds, not wire or operator settings.
final class SessionEventInbox: @unchecked Sendable {
    static let capacity = 64
    static let maximumBatchSize = 32

    private let lock = NSLock()
    private var pending: [StationSession.Event] = []
    private var inputFinished = false
    private var closed = false
    private var waitingOffer: (event: StationSession.Event, done: CheckedContinuation<Bool, Never>)?
    private var waitingBatch: CheckedContinuation<[StationSession.Event]?, Never>?
    #if DEBUG
    private var maximumPending = 0
    private var maximumBatch = 0
    private var taken = 0

    struct Snapshot: Sendable {
        let pendingCount: Int
        let maximumPendingCount: Int
        let maximumBatchCount: Int
        let takenCount: Int
        let hasCapacityWaiter: Bool
        let hasBatchWaiter: Bool
        let isClosed: Bool
    }

    var snapshotForTesting: Snapshot {
        lock.withLock {
            Snapshot(pendingCount: pending.count, maximumPendingCount: maximumPending,
                     maximumBatchCount: maximumBatch, takenCount: taken,
                     hasCapacityWaiter: waitingOffer != nil, hasBatchWaiter: waitingBatch != nil,
                     isClosed: closed)
        }
    }
    #endif

    /// Only the session's reader offers events. The one offer waiting at
    /// capacity is held separately until a prefix makes room for it.
    func offer(_ event: StationSession.Event) async -> Bool {
        await withTaskCancellationHandler {
            if Task.isCancelled {
                close()
                return false
            }
            return await withCheckedContinuation { done in
                var accepted: Bool?
                var consumer: CheckedContinuation<[StationSession.Event]?, Never>?
                lock.withLock {
                    guard !closed, !inputFinished, !Task.isCancelled else {
                        accepted = false
                        return
                    }
                    if let waiting = waitingBatch {
                        waitingBatch = nil
                        consumer = waiting
                        accepted = true
                        #if DEBUG
                        maximumBatch = max(maximumBatch, 1)
                        taken += 1
                        #endif
                    } else if pending.count < Self.capacity {
                        pending.append(event)
                        accepted = true
                        #if DEBUG
                        maximumPending = max(maximumPending, pending.count)
                        #endif
                    } else {
                        precondition(waitingOffer == nil, "One session reader owns intake admission")
                        waitingOffer = (event, done)
                    }
                }
                // Resuming a consumer may run model code: never under lock.
                consumer?.resume(returning: [event])
                if let accepted { done.resume(returning: accepted) }
            }
        } onCancel: {
            self.close()
        }
    }

    /// Only the app's event pump takes prefixes. Removing a prefix also
    /// admits the offer already waiting at capacity, in the same FIFO.
    func nextBatch() async -> [StationSession.Event]? {
        await withTaskCancellationHandler {
            if Task.isCancelled {
                close()
                return nil
            }
            return await withCheckedContinuation { done in
                var batch: [StationSession.Event]?
                var ready = false
                var producer: CheckedContinuation<Bool, Never>?
                lock.withLock {
                    if closed || Task.isCancelled {
                        ready = true
                    } else if !pending.isEmpty {
                        let count = min(pending.count, Self.maximumBatchSize)
                        batch = Array(pending.prefix(count))
                        pending.removeFirst(count)
                        if let waiting = waitingOffer {
                            pending.append(waiting.event)
                            waitingOffer = nil
                            producer = waiting.done
                        }
                        ready = true
                        #if DEBUG
                        maximumBatch = max(maximumBatch, count)
                        taken += count
                        #endif
                    } else if inputFinished {
                        ready = true
                    } else {
                        precondition(waitingBatch == nil, "One app pump owns intake consumption")
                        waitingBatch = done
                    }
                }
                producer?.resume(returning: true)
                if ready { done.resume(returning: batch) }
            }
        } onCancel: {
            self.close()
        }
    }

    /// Normal input completion keeps accepted FIFO work, including an
    /// offer already waiting for room. The last drained prefix precedes nil.
    func finishInput() {
        let consumer = lock.withLock { () -> CheckedContinuation<[StationSession.Event]?, Never>? in
            inputFinished = true
            guard pending.isEmpty, waitingOffer == nil else { return nil }
            let waiting = waitingBatch
            waitingBatch = nil
            return waiting
        }
        consumer?.resume(returning: nil)
    }

    /// Abort an abandoned owner. Closing before or after continuation
    /// registration has the same wakeup: every stored waiter is taken once.
    func close() {
        let waiters = lock.withLock { () -> (CheckedContinuation<Bool, Never>?,
                                           CheckedContinuation<[StationSession.Event]?, Never>?) in
            closed = true
            pending.removeAll()
            let producer = waitingOffer?.done
            let consumer = waitingBatch
            waitingOffer = nil
            waitingBatch = nil
            return (producer, consumer)
        }
        waiters.0?.resume(returning: false)
        waiters.1?.resume(returning: nil)
    }
}

/// Exactly one reader consumes the session stream off the main actor.
/// The main actor owns all handling; this task only admits FIFO payloads.
/// The pump closes and joins it before finishing its command/media lanes.
final class SessionEventIntake: Sendable {
    private final class ReaderState: @unchecked Sendable {
        private let lock = NSLock()
        private var ended = false
        func end() { lock.withLock { ended = true } }
        var hasEnded: Bool { lock.withLock { ended } }
    }

    let inbox: SessionEventInbox
    private let reader: Task<Void, Never>
    private let state: ReaderState

    init(events: AsyncStream<StationSession.Event>) {
        let inbox = SessionEventInbox()
        let state = ReaderState()
        self.inbox = inbox
        self.state = state
        reader = Task.detached(priority: Task.currentPriority) {
            for await event in events {
                guard await inbox.offer(event) else { break }
                // The stop itself is queued before normal input completion.
                if case .stateChanged(.stopped) = event { break }
            }
            inbox.finishInput()
            state.end()
        }
    }

    deinit {
        inbox.close()
        reader.cancel()
    }

    func nextBatch() async -> [StationSession.Event]? { await inbox.nextBatch() }

    /// Wake a reader held at capacity before joining; cancellation also
    /// releases a reader awaiting an otherwise still-open session stream.
    func closeAndJoin() async {
        inbox.close()
        reader.cancel()
        await reader.value
    }

    #if DEBUG
    var readerHasEndedForTesting: Bool { state.hasEnded }
    #endif
}

/// What media control's lane carries: the Core's messages it reads, and
/// whether the app expects the band's audio to fall silent while the Core
/// transmits, each time the mirror's transmit state changes that.
enum MediaLaneItem: Sendable, Equatable {
    case event(StationSession.Event)
    case transmitSilence(Bool)
}

/// One session's lanes to the command client and media control (JJ's
/// option 1 for the tune delay, 2026-10-27). The app's event pump applies
/// each of the Core's messages to the mirror, the settings and the records
/// at once and puts what the command client and media control read in
/// their lanes; for the messages that move the session or the transmit
/// state it drains both lanes and then waits on every consumer in turn, as
/// it always did (``AppModel``). The lanes end with the pump, so no
/// consumer outlives its session.
@MainActor
final class SessionEventLanes {
    let session: StationSession
    let owner: UInt64
    let commands: CoreEventLane<StationSession.Event>
    let media: CoreEventLane<MediaLaneItem>
    /// The transmit silence media control was last handed, by either path.
    var sentTransmitSilence: Bool?

    #if DEBUG
    /// What each lane handed over, in order, for the tests.
    let handedForTesting = LaneHandedLog()
    let hooksForTesting = LaneDeliveryHooks()
    #endif

    init(session: StationSession, owner: UInt64, commands client: CommandClient, media control: MediaControlClient) {
        self.session = session
        self.owner = owner
        #if DEBUG
        let log = handedForTesting
        let hooks = hooksForTesting
        #endif
        commands = CoreEventLane { event in
            #if DEBUG
            await hooks.beforeCommands()
            #endif
            await client.handle(event)
            #if DEBUG
            log.note(.commands(event))
            #endif
        }
        media = CoreEventLane { item in
            #if DEBUG
            await hooks.beforeMedia()
            #endif
            switch item {
            case .event(let event):
                await control.handle(event, owner: owner)
            case .transmitSilence(let expected):
                _ = await control.setExpectedAudioSilenceDuringTransmit(expected, owner: owner)
            }
            #if DEBUG
            log.note(.media(item))
            #endif
        }
    }

    /// Returns once both consumers have taken everything put in so far.
    func drain() async {
        await commands.drain()
        await media.drain()
    }

    /// The session is no longer the app's: queued items not yet admitted
    /// are skipped. An already admitted delivery still finishes.
    func stopDelivering() {
        commands.stopDelivering()
        media.stopDelivering()
    }

    /// Ends both lanes; returns once both tasks have ended.
    func finish() async {
        await commands.finish()
        await media.finish()
    }

    /// Whether both lanes' tasks have ended.
    var hasEnded: Bool { commands.hasEnded && media.hasEnded }
}

#if DEBUG
final class LaneDeliveryHooks: @unchecked Sendable {
    typealias Hook = @Sendable () async -> Void
    private let lock = NSLock()
    private var commandHook: Hook?
    private var mediaHook: Hook?

    func setCommands(_ hook: Hook?) { lock.withLock { commandHook = hook } }
    func setMedia(_ hook: Hook?) { lock.withLock { mediaHook = hook } }
    func beforeCommands() async {
        let hook = lock.withLock { commandHook }
        await hook?()
    }
    func beforeMedia() async {
        let hook = lock.withLock { mediaHook }
        await hook?()
    }
}

/// The lanes' hand-overs, in the order they finished, for the tests.
final class LaneHandedLog: @unchecked Sendable {
    enum Entry: Sendable, Equatable {
        case commands(StationSession.Event)
        case media(MediaLaneItem)
    }

    private let lock = NSLock()
    private var entries: [Entry] = []

    var all: [Entry] { lock.withLock { entries } }
    func note(_ entry: Entry) { lock.withLock { entries.append(entry) } }
}
#endif
