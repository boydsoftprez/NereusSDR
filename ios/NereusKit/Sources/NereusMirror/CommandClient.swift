// NereusSDR for iOS: asks the Core to act and pairs each answer with its request
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// Sends `command.invoke` and waits for its `command.result` (link document
/// section 9), pairing answers by command id. Ids start at 1 and run to
/// 4294967295, then wrap, skipping any still waiting. An answer for an id
/// it is not waiting for is ignored. A PureSignal action answers more than
/// once on one id; `invoke` resolves on the final answer and hands each
/// interim one to `onPhase`.
///
/// Like the session under it, it sends what it is asked whatever the
/// mirrored properties say; deciding what to offer the operator belongs
/// above it.
public actor CommandClient {
    public typealias Sender = @Sendable (LinkMessage) async throws -> Void
    /// Captures the logical session at admission. The returned sender must
    /// address that captured session, even if the app publishes a new one.
    public typealias CommandSender = @Sendable (LinkMessage, CommandSendPermit) async throws -> Void
    public typealias CaptureSender = @Sendable () -> CommandSender?
    public typealias GuardedSender = @Sendable (LinkMessage, TransmitHeartbeatGate) async throws -> Void
    public typealias CaptureGuardedSender = @Sendable () -> GuardedSender?

    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.commands")

    private struct Pending {
        let verb: String
        let answers: AsyncStream<Answer>.Continuation
        let onPhase: (@Sendable (CommandResult) -> Void)?
        var timer: (any LinkTimer)?
        let permit: CommandSendPermit
        let onLateOutcome: (@Sendable (Result<CommandResult, CommandError>) async -> Void)?
        var timedOut = false
    }

    private typealias Answer = Result<CommandResult, CommandError>
    private struct BoundSend: Sendable {
        let sender: CommandSender
        let stillAllowed: @Sendable () async -> Bool
        let gate: CommandSendPermit
    }

    private let send: Sender
    private let captureSender: CaptureSender?
    private let captureGuardedSender: CaptureGuardedSender?
    private let clock: any LinkClock
    private var nextId: UInt32 = 1
    private var pending: [UInt32: Pending] = [:]
    /// Answers still due to copies of a command already answered
    /// (``invoke(_:arguments:copies:timeout:)``), by id; each is dropped
    /// quietly as it comes.
    private var copiesDue: [UInt32: Int] = [:]
    private var linkUp = false
    private var linkGeneration: UInt64 = 0
    private var sessionPermit = CommandSendPermit()
    /// The last command's sending, which the next one's waits for.
    private var sendTail: Task<Void, Never>?

    public init(clock: any LinkClock = SystemLinkClock(), send: @escaping Sender,
                captureSender: CaptureSender? = nil,
                captureGuardedSender: CaptureGuardedSender? = nil) {
        self.clock = clock
        self.send = send
        self.captureSender = captureSender
        self.captureGuardedSender = captureGuardedSender
    }

    /// A client that sends through `session`. Feed it the session's events
    /// with `handle(_:)`.
    public init(session: StationSession, clock: any LinkClock = SystemLinkClock()) {
        self.init(clock: clock, send: { message in try await session.send(message) },
                  captureSender: { { message, permit in try await session.send(message, permit: permit) } },
                  captureGuardedSender: { { message, gate in try await session.send(message, while: gate) } })
    }

    /// Commands sent and not yet finally answered.
    public var waitingCount: Int { pending.count }

    // MARK: Feeding

    /// One session event: a `command.result`, or a lost link, which ends
    /// every command still waiting.
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(let message):
            receive(message)
        case .stateChanged(let state):
            let up = state == .receivingSnapshot || state == .ready
            if linkUp && !up {
                sessionPermit.revoke()
                sessionPermit = CommandSendPermit()
                linkGeneration &+= 1
                for id in Array(pending.keys) {
                    finish(id, with: .failure(.linkLost))
                }
                copiesDue = [:]
            }
            linkUp = up
        case .refused:
            break
        }
    }

    /// One message from the Core; only `command.result` matters here.
    public func receive(_ message: LinkMessage) {
        guard case .commandResult(let wire) = message else {
            return
        }
        if pending[wire.id] == nil, let due = copiesDue[wire.id] {
            // An answer to a copy of a command already answered.
            copiesDue[wire.id] = due > 1 ? due - 1 : nil
            return
        }
        guard let waiting = pending[wire.id], waiting.verb == wire.verb else {
            Self.logger.info("Ignoring an answer to a \(wire.verb, privacy: .public) request this app is not waiting for")
            return
        }
        let result = CommandResult(wire)
        if result.isFinal {
            finish(wire.id, with: .success(result))
        } else {
            waiting.onPhase?(result)
        }
    }

    // MARK: Invoking

    /// Asks the Core to act and returns its final answer, accepted or not.
    /// Throws `CommandError` when nothing was sent, the link was lost or no
    /// final answer came within `timeout`; nothing is left waiting.
    public func invoke(_ verb: String, arguments: [CommandArgument], timeout: Duration,
                       onPhase: (@Sendable (CommandResult) -> Void)? = nil) async throws -> CommandResult {
        try await invoke(verb, arguments: arguments, copies: 1, timeout: timeout, onPhase: onPhase)
    }

    /// Sends the same command, one id, `copies` times in a row, and returns
    /// the Core's first final answer: the keying verbs' rule (link document
    /// section 18.6), where the Core acts on the first copy and answers
    /// every copy the same way, so a lost frame never loses a key or an
    /// unkey. The answers to the other copies are dropped quietly.
    public func invoke(_ verb: String, arguments: [CommandArgument], copies: Int, timeout: Duration,
                       onPhase: (@Sendable (CommandResult) -> Void)? = nil) async throws -> CommandResult {
        let started = start(verb, arguments: arguments, copies: copies, timeout: timeout, onPhase: onPhase)
        return try await withTaskCancellationHandler {
            await started.sent()
            return try await started.result()
        } onCancel: {
            started.gate.revoke()
            Task { await self.abandon(started.id) }
        }
    }

    /// One ordinary command owned by a caller lifetime. Its authority is
    /// captured before actor admission and reaches the existing sender unchanged.
    func invoke(_ verb: String, arguments: [CommandArgument], timeout: Duration,
                authority: CommandSendPermit) async throws -> CommandResult {
        let started = start(verb, arguments: arguments, copies: 1, timeout: timeout, authority: authority)
        return try await withTaskCancellationHandler {
            await started.sent()
            return try await started.result()
        } onCancel: {
            started.gate.revoke()
            Task { await self.abandon(started.id) }
        }
    }

    /// For a non-keying control whose shown value remains unresolved after
    /// timeout. Its final answer (or session loss) still reaches that owner.
    /// Ordered keying, copies and authority-bound commands use the existing APIs.
    public func invokeHeld(_ verb: String, arguments: [CommandArgument], timeout: Duration,
                           onLateOutcome: @escaping @Sendable (Result<CommandResult, CommandError>) async -> Void)
        async throws -> CommandResult {
        let started = begin(verb, arguments: arguments, copies: 1, timeout: timeout,
                            onPhase: nil, bound: nil, onLateOutcome: onLateOutcome)
        return try await withTaskCancellationHandler {
            await started.sent()
            return try await started.result()
        } onCancel: {
            started.gate.revoke()
            Task { await self.abandon(started.id) }
        }
    }

    /// Uses this same client's ID, queue and result matching, but binds one
    /// sensitive command to the captured session transport. An earlier
    /// queued command cannot make it resolve a later mutable app route.
    /// Optional caller authority survives actor admission and the send queue.
    public func invokeBound(_ verb: String, arguments: [CommandArgument], timeout: Duration,
                            sender: @escaping CommandSender,
                            stillAllowed: @escaping @Sendable () async -> Bool,
                            authority: CommandSendPermit? = nil,
                            handoffReceipt: CommandHandoffReceipt? = nil,
                            onLateOutcome: (@Sendable (Result<CommandResult, CommandError>) async -> Void)? = nil) async throws -> CommandResult {
        let gate = CommandSendPermit(handoffReceipt: handoffReceipt)
        let started = begin(verb, arguments: arguments, copies: 1, timeout: timeout,
                            onPhase: nil, bound: BoundSend(sender: sender, stillAllowed: stillAllowed, gate: gate),
                            authority: authority, onLateOutcome: onLateOutcome)
        return try await withTaskCancellationHandler {
            return try await started.result()
        } onCancel: {
            gate.revoke()
            Task { await self.abandon(started.id) }
        }
    }

    /// A command on its way: ``sent()`` returns once its copies have gone
    /// (or failed to), ``result()`` once the Core has finally answered.
    public struct Started: Sendable {
        fileprivate let id: UInt32
        fileprivate let sending: Task<Void, Never>
        fileprivate let answers: AsyncStream<Result<CommandResult, CommandError>>
        fileprivate let client: CommandClient
        fileprivate let gate: CommandSendPermit

        /// The command's id on the link, which a question the Core raises
        /// for it names (`confirm.request`'s `forCommandId`).
        public var commandId: UInt32 { id }

        /// Returns once every copy has been handed to the link, in order.
        public func sent() async {
            await sending.value
        }

        /// The Core's final answer. Throws `CommandError` when nothing was
        /// sent, the link was lost or no answer came in time.
        public func result() async throws -> CommandResult {
            try await withTaskCancellationHandler {
                for await answer in answers {
                    return try answer.get()
                }
                await client.abandon(id)
                throw CancellationError()
            } onCancel: {
                gate.revoke()
                Task { await client.abandon(id) }
            }
        }
    }

    /// Sends the same command, one id, `copies` times in a row, and hands
    /// back its progress: the keying verbs' rule (link document section
    /// 18.6), where the Core acts on the first copy and answers every copy
    /// the same way, so a lost frame never loses a key or an unkey. The
    /// answers to the other copies are dropped quietly. Commands started
    /// one after another from the actor's own turn are sent in that order.
    public func start(_ verb: String, arguments: [CommandArgument], copies: Int, timeout: Duration,
                      onPhase: (@Sendable (CommandResult) -> Void)? = nil,
                      authority: CommandSendPermit? = nil) -> Started {
        begin(verb, arguments: arguments, copies: copies, timeout: timeout, onPhase: onPhase, bound: nil,
              authority: authority)
    }

    /// Retains the command ID while binding admission and handoff to the intent's sender.
    public func startBound(_ verb: String, arguments: [CommandArgument], copies: Int, timeout: Duration,
                           sender: @escaping CommandSender,
                           stillAllowed: @escaping @Sendable () async -> Bool,
                           authority: CommandSendPermit) -> Started {
        begin(verb, arguments: arguments, copies: copies, timeout: timeout, onPhase: nil,
              bound: BoundSend(sender: sender, stillAllowed: stillAllowed, gate: CommandSendPermit()),
              authority: authority)
    }

    private func begin(_ verb: String, arguments: [CommandArgument], copies: Int, timeout: Duration,
                       onPhase: (@Sendable (CommandResult) -> Void)?, bound: BoundSend?,
                       authority: CommandSendPermit? = nil,
                       onLateOutcome: (@Sendable (Result<CommandResult, CommandError>) async -> Void)? = nil) -> Started {
        let id = allocateId()
        let (answers, continuation) = AsyncStream.makeStream(of: Result<CommandResult, CommandError>.self,
                                                             bufferingPolicy: .unbounded)
        let gate = CommandSendPermit(parents: [sessionPermit] + [authority, bound?.gate].compactMap { $0 })
        guard !gate.isRevoked else {
            continuation.yield(.failure(.notSent))
            continuation.finish()
            return Started(id: id, sending: Task {}, answers: answers, client: self, gate: gate)
        }
        pending[id] = Pending(verb: verb, answers: continuation, onPhase: onPhase, timer: nil, permit: gate, onLateOutcome: onLateOutcome)
        pending[id]?.timer = clock.schedule(after: timeout) { [weak self] in
            await self?.finish(id, with: .failure(.timedOut))
        }
        let args = arguments.map { LinkMessage.PropertyEntry(name: $0.name, value: $0.value.wireValue) }
        let message = LinkMessage.commandInvoke(LinkMessage.CommandInvoke(verb: verb, id: id, args: args))
        let copies = max(1, copies)
        if copies > 1 {
            copiesDue[id] = copies - 1
        }
        // Each command's copies go after the one before's, so two commands
        // started in order reach the Core in order.
        let previous = sendTail
        let admittedGeneration = linkGeneration
        let admittedSender = bound?.sender ?? admittedSender()
        let sending = Task { [weak self] in
            await previous?.value
            await self?.sendCopies(message, id: id, verb: verb, copies: copies, bound: bound,
                                  sender: admittedSender, generation: admittedGeneration, gate: gate)
        }
        sendTail = sending
        return Started(id: id, sending: sending, answers: answers, client: self, gate: gate)
    }

    private func sendCopies(_ message: LinkMessage, id: UInt32, verb: String, copies: Int,
                            bound: BoundSend?, sender: @escaping CommandSender,
                            generation: UInt64, gate: CommandSendPermit) async {
        var sent = 0
        do {
            for _ in 0..<copies {
                guard !gate.isRevoked else {
                    finish(id, with: .failure(.notSent))
                    return
                }
                guard sent > 0 || pending[id] != nil else { return }
                guard linkUp else {
                    finish(id, with: .failure(.notSent))
                    return
                }
                guard generation == linkGeneration else {
                    finish(id, with: .failure(.linkLost))
                    return
                }
                if let bound {
                    guard pending[id] != nil, !bound.gate.isRevoked,
                          await bound.stillAllowed(), pending[id] != nil,
                          !bound.gate.isRevoked else {
                        finish(id, with: .failure(.notSent))
                        return
                    }
                    try await sender(message, gate)
                } else {
                    try await sender(message, gate)
                }
                sent += 1
            }
        } catch {
            Self.logger.info("A \(verb, privacy: .public) request was not sent: \(String(describing: error), privacy: .public)")
            copiesDue[id] = sent > 1 ? sent - 1 : nil
            if sent == 0 {
                finish(id, with: .failure(.notSent))
            }
        }
    }

    /// A waiting command whose caller went away.
    fileprivate func abandon(_ id: UInt32) {
        finish(id, with: nil)
    }

    /// Sends a command once and returns as soon as it has gone, without
    /// waiting for its answer, which is dropped when it comes: for
    /// `tx.keepalive` (link document section 18.7), sent every 100 ms and
    /// answered `accepted` whether or not it counted, where a lost one is
    /// overtaken by the next. Throws `CommandError.notSent` when nothing
    /// was sent.
    public func post(_ verb: String, arguments: [CommandArgument],
                     onSuccessfulHandoff: (@Sendable () -> Void)? = nil) async throws {
        try await post(verb, arguments: arguments, gate: nil, stillAllowed: { true },
                       onSuccessfulHandoff: onSuccessfulHandoff)
    }

    public func post(_ verb: String, arguments: [CommandArgument],
                     gate: TransmitHeartbeatGate?,
                     stillAllowed: @escaping @Sendable () async -> Bool,
                     onSuccessfulHandoff: (@Sendable () -> Void)? = nil) async throws {
        let id = allocateId()
        let args = arguments.map { LinkMessage.PropertyEntry(name: $0.name, value: $0.value.wireValue) }
        // Held until the answer comes, so the id is not used again meanwhile.
        copiesDue[id] = 1
        let sender = admittedSender()
        let postPermit = CommandSendPermit(parents: [sessionPermit])
        let guardedSender = captureGuardedSender?()
        let generation = linkGeneration
        let previous = sendTail
        let sending = Task { [weak self] () -> Bool in
            await previous?.value
            guard let self, await self.mayPost(generation), await stillAllowed(),
                  gate?.isOpen ?? true else { return false }
            do {
                let message = LinkMessage.commandInvoke(LinkMessage.CommandInvoke(verb: verb, id: id, args: args))
                if let gate, let guardedSender {
                    try await guardedSender(message, gate)
                } else {
                    try await sender(message, postPermit)
                }
                // Diagnostic only, inside the successful send task. An awaiting
                // caller's later actor resumption cannot inflate this timestamp.
                onSuccessfulHandoff?()
                return true
            } catch {
                return false
            }
        }
        sendTail = Task { _ = await sending.value }
        guard await sending.value else {
            copiesDue[id] = nil
            throw CommandError.notSent
        }
    }

    private func mayPost(_ generation: UInt64) -> Bool {
        linkUp && generation == linkGeneration
    }

    private func admittedSender() -> CommandSender {
        guard let captureSender else {
            let send = send
            return { message, permit in
                guard !permit.isRevoked else { throw LinkSendError.notConnected }
                try await send(message)
            }
        }
        return captureSender() ?? { _, _ in throw LinkSendError.notConnected }
    }

    // MARK: Inside

    /// Where the tests make the next id search start, to reach the wrap.
    func setNextIdForTesting(_ id: UInt32) {
        nextId = id == 0 ? 1 : id
    }

    private func allocateId() -> UInt32 {
        while pending[nextId] != nil || copiesDue[nextId] != nil {
            nextId = nextId == UInt32.max ? 1 : nextId + 1
        }
        let id = nextId
        nextId = nextId == UInt32.max ? 1 : nextId + 1
        return id
    }

    /// Ends the command with `id`: forgets it, stops its timer and hands the
    /// answer to `invoke`.
    private func finish(_ id: UInt32, with answer: Answer?) {
        guard var waiting = pending[id] else { return }
        if case .failure(.timedOut)? = answer, waiting.onLateOutcome != nil, !waiting.timedOut {
            waiting.permit.revoke()
            waiting.timer?.cancel()
            waiting.timer = nil
            waiting.timedOut = true
            pending[id] = waiting
            waiting.answers.yield(.failure(.timedOut))
            waiting.answers.finish()
            return
        }
        pending[id] = nil
        if waiting.timedOut {
            if let answer, let onLateOutcome = waiting.onLateOutcome {
                Task { await onLateOutcome(answer) }
            }
            return
        }
        // A successful/refused result still allows the already admitted repeated
        // copies. Timeout, cancellation and link loss retire all remaining sends.
        if case .success? = answer {} else { waiting.permit.revoke() }
        waiting.timer?.cancel()
        if let answer {
            waiting.answers.yield(answer)
        }
        waiting.answers.finish()
    }
}
