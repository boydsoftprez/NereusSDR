// NereusSDR for iOS: commands to the Core, their ids, answers, phases and timeouts
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@Suite struct CommandClientTests {
    private let sent = SentMessages()
    private let clock = ManualLinkClock()

    private func client() async -> CommandClient {
        let client = CommandClient(clock: clock, send: sent.sender)
        await client.handle(.stateChanged(.ready))
        return client
    }

    /// Starts an invoke and waits for its `command.invoke` to leave.
    private func start(_ client: CommandClient, _ verb: String, _ arguments: [CommandArgument] = [],
                       timeout: Duration = .seconds(10),
                       onPhase: (@Sendable (CommandResult) -> Void)? = nil)
        async throws -> (Task<CommandResult, any Error>, LinkMessage.CommandInvoke) {
        let before = sent.count
        let task = Task { try await client.invoke(verb, arguments: arguments, timeout: timeout, onPhase: onPhase) }
        #expect(await sent.settle(untilCount: before + 1))
        guard case .commandInvoke(let invoke)? = sent.messages.last else {
            Issue.record("no command.invoke was sent")
            throw CancellationError()
        }
        return (task, invoke)
    }

    private static func result(_ verb: String, _ id: UInt32, accepted: Bool = true, reason: String = "",
                               affected: [String] = [], values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
        .commandResult(LinkMessage.CommandResult(verb: verb, id: id, accepted: accepted, reason: reason,
                                                 affected: affected, values: values))
    }

    /// The commands sent so far, in order.
    private static func invokes(_ sent: SentMessages) -> [LinkMessage.CommandInvoke] {
        sent.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message {
                return invoke
            }
            return nil
        }
    }

    /// The first command sent that `pick` takes; the test fails if none was.
    private static func firstInvoke(_ sent: SentMessages,
                                    _ pick: (LinkMessage.CommandInvoke) -> Bool) throws -> LinkMessage.CommandInvoke {
        let found = invokes(sent).first(where: pick)
        return try #require(found)
    }

    /// The last command sent that `pick` takes; the test fails if none was.
    private static func lastInvoke(_ sent: SentMessages,
                                   _ pick: (LinkMessage.CommandInvoke) -> Bool) throws -> LinkMessage.CommandInvoke {
        let found = invokes(sent).last(where: pick)
        return try #require(found)
    }

    /// A key: `tx.key`, or tune, two-tone or the tuner's tune turned on.
    private static func isOn(_ invoke: LinkMessage.CommandInvoke) -> Bool {
        invoke.verb == "tx.key" ||
            ((invoke.verb == "tx.tune" || invoke.verb == "tx.twoTone" || invoke.verb == "tx.tunerTune") &&
             invoke.args.first?.value == .bool(true))
    }

    /// A release: `tx.unkey`, or tune, two-tone or the tuner's tune turned off.
    private static func isOff(_ invoke: LinkMessage.CommandInvoke) -> Bool {
        invoke.verb == "tx.unkey" ||
            ((invoke.verb == "tx.tune" || invoke.verb == "tx.twoTone" || invoke.verb == "tx.tunerTune") &&
             invoke.args.first?.value == .bool(false))
    }

    private static func phase(_ text: String) -> [LinkMessage.PropertyEntry] {
        [LinkMessage.PropertyEntry(name: "phase", value: .utf8(text))]
    }

    @Test func argumentsTravelInOrderAndResultsPairById() async throws {
        let client = await client()
        let arguments = [
            CommandArgument(name: "mode", value: .int(2)),
            CommandArgument(name: "graceMs", value: .int(500)),
            CommandArgument(name: "swrGateEnabled", value: .bool(true)),
            CommandArgument(name: "swrGateMax", value: .double(2.5)),
        ]
        let (first, invoke) = try await start(client, "setTxInterlockPolicy", arguments)
        #expect(invoke.id == 1)
        #expect(invoke.args == [
            LinkMessage.PropertyEntry(name: "mode", value: .i64(2)),
            LinkMessage.PropertyEntry(name: "graceMs", value: .i64(500)),
            LinkMessage.PropertyEntry(name: "swrGateEnabled", value: .bool(true)),
            LinkMessage.PropertyEntry(name: "swrGateMax", value: .f64(2.5)),
        ])
        let (second, other) = try await start(client, "dspAssets.list")
        #expect(other.id == 2)

        // Answers arrive out of order; each finds its own request. One for
        // an id nobody is waiting for, or with another verb, is ignored.
        await client.receive(Self.result("dspAssets.list", 1_102))
        await client.receive(Self.result("setTxInterlockPolicy", 2))
        await client.receive(Self.result("dspAssets.list", 2, values: [
            LinkMessage.PropertyEntry(name: "revision", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "status", value: .utf8("NNR model selections are available.")),
        ]))
        await client.receive(Self.result("setTxInterlockPolicy", 1, affected: ["accessoryData"]))
        let listed = try await second.value
        #expect(listed.accepted)
        #expect(listed.values == ["revision": .int(1), "status": .text("NNR model selections are available.")])
        #expect(listed.phase == nil)
        let policy = try await first.value
        #expect(policy == CommandResult(accepted: true, reason: "", affectedKeys: ["accessoryData"], values: [:],
                                        phase: nil))
        #expect(await client.waitingCount == 0)
    }

    @Test func aPureSignalActionResolvesOnItsFinalAnswer() async throws {
        let client = await client()
        let phases = PhaseLog()
        let (task, invoke) = try await start(client, "ps3.off", onPhase: { phases.append($0) })
        await client.receive(Self.result("ps3.off", invoke.id, values: Self.phase("accepted")))
        #expect(phases.results.map(\.phase) == ["accepted"])
        await client.receive(Self.result("ps3.off", invoke.id, affected: ["pureSignal"],
                                         values: Self.phase("completed")))
        let final = try await task.value
        #expect(final.accepted)
        #expect(final.phase == "completed")
        #expect(final.affectedKeys == ["pureSignal"])
        #expect(phases.results.count == 1)

        // A final answer that is not accepted is still the answer.
        let (save, saveInvoke) = try await start(client, "ps3.saveCorrection",
                                                 [CommandArgument(name: "label", value: .text("Conformance"))],
                                                 onPhase: { phases.append($0) })
        await client.receive(Self.result("ps3.saveCorrection", saveInvoke.id, values: Self.phase("accepted")))
        await client.receive(Self.result("ps3.saveCorrection", saveInvoke.id, values: Self.phase("pending")))
        await client.receive(Self.result("ps3.saveCorrection", saveInvoke.id, accepted: false,
                                         reason: "PureSignal is unavailable until the radio is ready.",
                                         affected: ["pureSignal"], values: Self.phase("failed")))
        let failed = try await save.value
        #expect(!failed.accepted)
        #expect(failed.phase == "failed")
        #expect(failed.reason == "PureSignal is unavailable until the radio is ready.")
        #expect(phases.results.map(\.phase) == ["accepted", "accepted", "pending"])

        // Refused before the transmit gate: one answer, no phase.
        let (single, singleInvoke) = try await start(client, "ps3.single")
        await client.receive(Self.result("ps3.single", singleInvoke.id, accepted: false,
                                         reason: "PureSignal cannot be run from a remote window."))
        #expect(try await single.value.reason == "PureSignal cannot be run from a remote window.")
    }

    @Test func aVerbTheCoreDoesNotKnowComesBackRefusedWithItsReason() async throws {
        let client = await client()
        let (task, invoke) = try await start(client, "conformanceUnknownVerb")
        let reason = "The Core does not know this request. Updating the Core may help."
        await client.receive(Self.result("conformanceUnknownVerb", invoke.id, accepted: false, reason: reason))
        let result = try await task.value
        #expect(!result.accepted)
        #expect(result.reason == reason)
    }

    @Test func aTimeoutThrowsAndLeavesNothingWaiting() async throws {
        let client = await client()
        let (task, invoke) = try await start(client, "requestIoBoardProbe", timeout: .seconds(5))
        await clock.advance(by: 4_999)
        #expect(await client.waitingCount == 1)
        await clock.advance(by: 1)
        await #expect(throws: CommandError.timedOut) { try await task.value }
        #expect(await client.waitingCount == 0)
        #expect(clock.pendingDueTimes.isEmpty)
        // A late answer changes nothing.
        await client.receive(Self.result("requestIoBoardProbe", invoke.id))
        #expect(await client.waitingCount == 0)
    }

    @Test func aLostLinkOrAnUnsentRequestThrows() async throws {
        let client = await client()
        let (task, _) = try await start(client, "addSlice", [CommandArgument(name: "initialPanId", value: .text(""))])
        await client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await #expect(throws: CommandError.linkLost) { try await task.value }
        #expect(await client.waitingCount == 0)
        #expect(clock.pendingDueTimes.isEmpty)

        sent.refuseAll()
        await #expect(throws: CommandError.notSent) {
            try await client.invoke("addSlice", arguments: [], timeout: .seconds(1))
        }
        #expect(await client.waitingCount == 0)
    }

    @Test func aQueuedTicketCannotFollowAReplacedSession() async throws {
        let hold = QueuedSendHold()
        let old = SentMessages()
        let newer = SentMessages()
        let route = ChangingSendRoute(initial: old)
        let allowed = BoundSendPermission()
        let client = CommandClient(clock: clock, send: { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "held.command" {
                await hold.enter()
            }
            try await route.send(message)
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(10))
        await hold.waitUntilEntered()
        let ticket = Task {
            try await client.invokeBound("session.pathTicket", arguments: [], timeout: .seconds(10),
                                         sender: { message, _ in try await old.sender(message) }, stillAllowed: { allowed.value })
        }
        #expect(await waitForWaitingCount(client, 2))
        await route.replace(with: newer)
        allowed.revoke()
        await hold.release()
        await earlier.sent()
        await #expect(throws: CommandError.notSent) { try await ticket.value }
        #expect(old.messages.isEmpty)
        #expect(newer.messages.count == 1)
        #expect(newer.messages.allSatisfy { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb != "session.pathTicket" }
            return true
        })
    }

    @Test func aRevokedBoundLeaveCannotAcquireAuthorityAtCommandAdmission() async throws {
        let client = await client()
        let expiry = CommandSendPermit()
        expiry.revoke()
        await #expect(throws: CommandError.notSent) {
            try await client.invokeBound("session.leave", arguments: [], timeout: .seconds(10),
                                         sender: { [sent] message, _ in try await sent.sender(message) },
                                         stillAllowed: { true }, authority: expiry)
        }
        #expect(sent.messages.isEmpty)
        #expect(await client.waitingCount == 0)
    }

    @Test func boundStartedRetainsIdQueueAndCoreResultMatching() async throws {
        let ordinary = SentMessages()
        let bound = SentMessages()
        let client = CommandClient(clock: clock, send: ordinary.sender)
        await client.handle(.stateChanged(.ready))
        let authority = CommandSendPermit()
        let started = await client.startBound("tx.take", arguments: [], copies: 1, timeout: .seconds(10),
                                              sender: { message, permit in
            guard permit.handoff({ true }) else { throw LinkSendError.notConnected }
            try await bound.sender(message)
        }, stillAllowed: { true }, authority: authority)
        await started.sent()
        let invoke = try Self.firstInvoke(bound) { $0.verb == "tx.take" }
        #expect(invoke.id == started.commandId)
        #expect(ordinary.messages.isEmpty)
        await client.receive(Self.result("tx.take", started.commandId))
        #expect(try await started.result().accepted)
        #expect(await client.waitingCount == 0)
    }

    @Test func revokedBoundStartedIsRejectedAtActorAdmission() async throws {
        let client = await client()
        let authority = CommandSendPermit()
        authority.revoke()
        let started = await client.startBound("tx.take", arguments: [], copies: 1, timeout: .seconds(10),
                                              sender: { [sent] message, permit in
            guard permit.handoff({ true }) else { throw LinkSendError.notConnected }
            try await sent.sender(message)
        }, stillAllowed: { true }, authority: authority)
        #expect(await client.waitingCount == 0)
        #expect(clock.pendingDueTimes.isEmpty)
        await started.sent()
        await #expect(throws: CommandError.notSent) { try await started.result() }
        #expect(sent.messages.isEmpty)
    }

    @Test func boundStartedRetirementClosesFinalSynchronousHandoff() async throws {
        let hold = QueuedSendHold()
        let receipt = CommandHandoffReceipt()
        let authority = CommandSendPermit(handoffReceipt: receipt)
        let client = await client()
        let started = await client.startBound("tx.take", arguments: [], copies: 1, timeout: .seconds(10),
                                              sender: { [sent] message, permit in
            await hold.enter()
            guard permit.handoff({ true }) else { throw LinkSendError.notConnected }
            try await sent.sender(message)
        }, stillAllowed: { true }, authority: authority)
        await hold.waitUntilEntered()
        authority.revoke()
        await hold.release()
        await started.sent()
        await #expect(throws: CommandError.notSent) { try await started.result() }
        #expect(!receipt.wasSent)
        #expect(sent.messages.isEmpty)
        #expect(await client.waitingCount == 0)
    }

    @Test func aHandoffReceiptMarksOnlySuccessfulSynchronousEnqueue() {
        enum Refused: Error { case transport }
        let receipt = CommandHandoffReceipt()
        let permit = CommandSendPermit(handoffReceipt: receipt)
        #expect(!receipt.wasSent)
        #expect(permit.handoff { false } == false)
        #expect(!receipt.wasSent)
        #expect(throws: Refused.self) { try permit.handoff { throw Refused.transport } }
        #expect(!receipt.wasSent)
        permit.revoke()
        #expect(permit.handoff { true } == false)
        #expect(!receipt.wasSent)

        let accepted = CommandSendPermit(handoffReceipt: receipt)
        #expect(accepted.handoff { true })
        #expect(receipt.wasSent)
    }

    @Test func canceledQueuedTicketFinishesBeforeEarlierSendIsReleased() async throws {
        let hold = QueuedSendHold()
        let old = SentMessages()
        let client = CommandClient(clock: clock, send: { message in
            await hold.enter()
            try await old.sender(message)
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(10))
        await hold.waitUntilEntered()
        let ticket = Task {
            try await client.invokeBound("session.pathTicket", arguments: [], timeout: .seconds(10),
                                         sender: { message, _ in try await old.sender(message) }, stillAllowed: { true })
        }
        #expect(await waitForWaitingCount(client, 2))
        ticket.cancel()
        await #expect(throws: CancellationError.self) { try await ticket.value }
        #expect(await client.waitingCount == 1)
        #expect(old.messages.isEmpty)
        await hold.release()
        await earlier.sent()
        #expect(old.messages.count == 1)
    }

    /// An ordinary TX command started on OLD must not be sent through NEW
    /// after the old link ends while an earlier send owns the command queue.
    @Test func queuedOldTransmitCommandCannotReachReplacementRoute() async {
        let hold = QueuedSendHold()
        let old = SentMessages()
        let newer = SentMessages()
        let route = ChangingSendRoute(initial: old)
        let client = CommandClient(clock: clock, send: { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "held.command" {
                await hold.enter()
                try await old.sender(message)
            } else {
                try await route.send(message)
            }
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(10))
        await hold.waitUntilEntered()
        let oldUnkey = await client.start("tx.unkey", arguments: [
            CommandArgument(name: "epoch", value: .int(7)),
        ], copies: 3, timeout: .seconds(5))
        #expect(await waitForWaitingCount(client, 2))
        await client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await route.replace(with: newer)
        await client.handle(.stateChanged(.ready))
        await hold.release()
        await earlier.sent()
        await oldUnkey.sent()
        #expect(newer.messages.count == 0)
    }

    @Test func canceledOrdinaryCommandCannotLeaveTheQueue() async {
        let hold = QueuedSendHold()
        let sent = SentMessages()
        let client = CommandClient(clock: clock, send: { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "held.command" {
                await hold.enter()
            }
            try await sent.sender(message)
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(10))
        await hold.waitUntilEntered()
        let canceled = Task {
            try await client.invoke("tx.key", arguments: [], copies: 3, timeout: .seconds(10))
        }
        #expect(await waitForWaitingCount(client, 2))
        canceled.cancel()
        await hold.release()
        await earlier.sent()
        _ = try? await canceled.value
        #expect(sent.messages.compactMap { message -> String? in
            if case .commandInvoke(let invoke) = message { return invoke.verb }
            return nil
        } == ["held.command"])
    }

    /// If media is unavailable, a posted fallback heartbeat must not pass
    /// an off still waiting behind an earlier command on the primary queue.
    @Test func primaryFallbackHeartbeatCannotOvertakeQueuedOff() async throws {
        let hold = QueuedSendHold()
        let sent = SentMessages()
        let client = CommandClient(clock: clock, send: { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == "held.command" {
                await hold.enter()
            }
            try await sent.sender(message)
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(10))
        await hold.waitUntilEntered()
        let off = await client.start("tx.unkey", arguments: [
            CommandArgument(name: "epoch", value: .int(7)),
        ], copies: 3, timeout: .seconds(5))
        #expect(await waitForWaitingCount(client, 2))
        let transmit = TransmitCommandClient(commands: client)
        let heartbeat = Task { await transmit.sendKeepalive(sequence: 1, epoch: PttController.unansweredEpoch) }
        let verbs = sent.messages.compactMap { message -> String? in
            if case .commandInvoke(let invoke) = message { return invoke.verb }
            return nil
        }
        #expect(verbs.isEmpty)
        await hold.release()
        await earlier.sent()
        await off.sent()
        await heartbeat.value
    }

    @Test func queuedTicketTimesOutWithoutWaitingForEarlierSend() async throws {
        let hold = QueuedSendHold()
        let sent = SentMessages()
        let client = CommandClient(clock: clock, send: { message in
            await hold.enter()
            try await sent.sender(message)
        })
        await client.handle(.stateChanged(.ready))
        let earlier = await client.start("held.command", arguments: [], copies: 1, timeout: .seconds(30))
        await hold.waitUntilEntered()
        let ticket = Task {
            try await client.invokeBound("session.pathTicket", arguments: [], timeout: .seconds(10),
                                         sender: { message, _ in try await sent.sender(message) }, stillAllowed: { true })
        }
        #expect(await waitForWaitingCount(client, 2))
        await clock.advance(by: 10_000)
        await #expect(throws: CommandError.timedOut) { try await ticket.value }
        #expect(await client.waitingCount == 1)
        #expect(sent.messages.isEmpty)
        await hold.release()
        await earlier.sent()
        #expect(sent.messages.count == 1)
    }

    @Test func idsWrapWithinTheirRangeAndSkipOnesStillWaiting() async throws {
        let client = await client()
        let (held, heldInvoke) = try await start(client, "dspAssets.list")
        #expect(heldInvoke.id == 1)
        await client.setNextIdForTesting(UInt32.max)
        let (last, lastInvoke) = try await start(client, "dspAssets.list")
        #expect(lastInvoke.id == UInt32.max)
        let (wrapped, wrappedInvoke) = try await start(client, "dspAssets.list")
        // 0 is never used, and 1 is still waiting.
        #expect(wrappedInvoke.id == 2)
        for id in [heldInvoke.id, lastInvoke.id, wrappedInvoke.id] {
            await client.receive(Self.result("dspAssets.list", id))
        }
        for task in [held, last, wrapped] {
            #expect(try await task.value.accepted)
        }
    }

    private func waitForWaitingCount(_ client: CommandClient, _ count: Int) async -> Bool {
        let deadline = ContinuousClock.now.advanced(by: .seconds(2))
        while ContinuousClock.now < deadline {
            if await client.waitingCount == count { return true }
            await Task.yield()
        }
        return await client.waitingCount == count
    }

    // MARK: The keying verbs' copies and the keepalive (Task 54)

    @Test func copiesGoUnderOneIdAndTheirAnswersAreDroppedQuietly() async throws {
        let client = await client()
        let task = Task {
            try await client.invoke("tx.key", arguments: [CommandArgument(name: "trigger", value: .text("screen"))],
                                    copies: 3, timeout: .seconds(5))
        }
        #expect(await sent.settle(untilCount: 3))
        let ids = sent.messages.compactMap { message -> UInt32? in
            if case .commandInvoke(let invoke) = message {
                return invoke.id
            }
            return nil
        }
        #expect(ids == [1, 1, 1])
        let epoch = [LinkMessage.PropertyEntry(name: "epoch", value: .i64(1))]
        for _ in 0..<3 {
            await client.receive(Self.result("tx.key", 1, values: epoch))
        }
        let answer = try await task.value
        #expect(answer.values["epoch"] == .int(1))
        #expect(await client.waitingCount == 0)
        // The id is free again once every copy is answered.
        let (_, next) = try await start(client, "dspAssets.list")
        #expect(next.id == 2)
    }

    @Test func aPostedKeepaliveReturnsWithoutWaiting() async throws {
        let client = await client()
        try await client.post("tx.keepalive", arguments: [CommandArgument(name: "sequence", value: .int(1)),
                                                          CommandArgument(name: "epoch", value: .int(4_294_967_295))])
        guard case .commandInvoke(let invoke)? = sent.messages.last else {
            Issue.record("no command.invoke was sent")
            return
        }
        #expect(invoke.verb == "tx.keepalive")
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "sequence", value: .i64(1)),
                                LinkMessage.PropertyEntry(name: "epoch", value: .i64(4_294_967_295))])
        #expect(await client.waitingCount == 0)
        // Its id is not used again until its answer comes.
        let (_, other) = try await start(client, "dspAssets.list")
        #expect(other.id != invoke.id)
        await client.receive(Self.result("tx.keepalive", invoke.id))
    }

    /// Task 55: while the media connection's "tx" channel takes the
    /// keepalive, none goes on the session; when it does not, the session
    /// carries it. Never both.
    @Test func theKeepaliveGoesOnTheTxChannelWhenItIsOpenElseOnTheSession() async throws {
        let client = await client()
        let open = OpenFlag()
        let transmit = TransmitCommandClient(commands: client, keepaliveOnMedia: { sequence, epoch in
            open.take(sequence, epoch)
        })
        open.set(true)
        await transmit.sendKeepalive(sequence: 1, epoch: 4_294_967_295)
        #expect(sent.count == 0)
        #expect(open.taken.count == 1)
        open.set(false)
        await transmit.sendKeepalive(sequence: 2, epoch: 4_294_967_295)
        #expect(await sent.settle(untilCount: 1))
        guard case .commandInvoke(let invoke)? = sent.messages.last else {
            Issue.record("no command.invoke was sent")
            return
        }
        #expect(invoke.verb == "tx.keepalive")
        #expect(invoke.args.first == LinkMessage.PropertyEntry(name: "sequence", value: .i64(2)))
        await client.receive(Self.result("tx.keepalive", invoke.id))
    }

    /// The primary release is held before the Core acts on it. VOX remains
    /// armed, so any media heartbeat during this interval can renew the old key.
    @Test(arguments: ["ptt", "tune", "twoTone", "tunerTune"], [true, false])
    func releaseWaitsForOffWithoutHeartbeatOnEitherPath(kind: String, mediaAvailable: Bool) async throws {
        let holdOff = QueuedSendHold()
        let sent = SentMessages()
        let media = OpenFlag()
        let command = CommandClient(clock: clock, send: { message in
            if case .commandInvoke(let invoke) = message {
                let isOff = invoke.verb == "tx.unkey" ||
                    ((invoke.verb == "tx.tune" || invoke.verb == "tx.twoTone" || invoke.verb == "tx.tunerTune") &&
                     invoke.args.first?.value == .bool(false))
                if isOff { await holdOff.enter() }
            }
            try await sent.sender(message)
        })
        await command.handle(.stateChanged(.ready))
        let transmit = TransmitCommandClient(commands: command, keepaliveOnMedia: { sequence, epoch in
            media.take(sequence, epoch)
        })
        let ptt = PttController(commands: transmit, clock: clock)
        await ptt.linkChanged(up: true)
        await ptt.update(TransmitStateReport())
        switch kind {
        case "tune": await ptt.setTune(true)
        case "twoTone": await ptt.setTwoTone(true)
        case "tunerTune": await ptt.toggleTunerTune()
        default: await ptt.tap()
        }
        #expect(await sent.settle(untilCount: 3))
        guard case .commandInvoke(let on)? = sent.messages.first else {
            Issue.record("key command did not reach the sender")
            return
        }
        await command.receive(Self.result(on.verb, on.id, values: [
            LinkMessage.PropertyEntry(name: "epoch", value: .i64(7)),
        ]))
        await ptt.settle()
        #expect(await ptt.state.isKeyed)
        media.set(mediaAvailable)
        await ptt.setVoxArmed(true)
        switch kind {
        case "tune": await ptt.setTune(false)
        case "twoTone": await ptt.setTwoTone(false)
        case "tunerTune": await ptt.toggleTunerTune()
        default: await ptt.tap()
        }
        await holdOff.waitUntilEntered()
        #expect(await !ptt.snapshot.microphoneWanted)
        await ptt.tap()
        await ptt.setTune(true)
        await clock.advance(by: 600)
        #expect(media.taken.map { "\($0.0):\($0.1)" } == [])
        #expect(!sent.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "tx.keepalive" }
            return false
        })
        // Core fd5ae5fd4 checks silence > 400 ms at receipt and timer fire,
        // then accepts either path only for a rising sequence/new enough epoch.
        var watchdog = SourceCheckedWatchdog(epoch: 7, lastMs: 0)
        for (index, heartbeat) in media.taken.enumerated() {
            _ = watchdog.keepalive(sequence: heartbeat.0, epoch: heartbeat.1,
                                   atMs: (index + 1) * 100)
        }
        #expect(watchdog.stopsWhenTimerFires(atMs: 601))
        await holdOff.release()
        #expect(await sent.settle(untilCount: 6))
        let off = try Self.lastInvoke(sent, Self.isOff)
        await command.receive(Self.result("different.off", off.id))
        await clock.advance(by: 100)
        #expect(media.taken.isEmpty)
        await command.receive(Self.result(off.verb, off.id))
        await command.receive(Self.result(off.verb, off.id))
        await ptt.settle()
        #expect(await ptt.snapshot.microphoneWanted)
        await clock.advance(by: 100)
        if mediaAvailable {
            #expect(media.taken.count == 1)
        } else {
            #expect(await sent.settle(untilCount: 7))
            #expect(sent.messages.contains { message in
                if case .commandInvoke(let invoke) = message { return invoke.verb == "tx.keepalive" }
                return false
            })
        }
    }

    /// A failed primary off has no delivery barrier. VOX heartbeats must not
    /// keep the prior key alive while the controller retries its release.
    @Test func failedPrimaryOffDoesNotContinueMediaHeartbeat() async {
        let sent = SentMessages()
        let media = OpenFlag()
        let command = CommandClient(clock: clock, send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let transmit = TransmitCommandClient(commands: command, keepaliveOnMedia: { sequence, epoch in
            media.take(sequence, epoch)
        })
        let ptt = PttController(commands: transmit, clock: clock)
        await ptt.linkChanged(up: true)
        await ptt.update(TransmitStateReport())
        await ptt.tap()
        #expect(await sent.settle(untilCount: 3))
        guard case .commandInvoke(let on)? = sent.messages.first else {
            Issue.record("key command did not reach the sender")
            return
        }
        await command.receive(Self.result(on.verb, on.id, values: [
            LinkMessage.PropertyEntry(name: "epoch", value: .i64(7)),
        ]))
        await ptt.settle()
        await ptt.setVoxArmed(true)
        media.set(true)
        sent.refuseAll()
        await ptt.tap()
        await ptt.settle()
        #expect(await !ptt.snapshot.microphoneWanted)
        await clock.advance(by: 600)
        #expect(media.taken.map { "\($0.0):\($0.1)" } == [])
        let before = sent.count
        await ptt.tap()
        await ptt.setTune(true)
        await ptt.setTwoTone(true)
        await ptt.dismissNotice()
        var olderState = TransmitStateReport()
        olderState.held = true
        olderState.holderSource = "device"
        await ptt.update(olderState)
        await ptt.update(TransmitStateReport())
        #expect(sent.count == before)
        if case .refused(let refusal) = await ptt.state {
            #expect(refusal.reason.contains(PttController.unresolvedOffText))
        } else {
            Issue.record("Unconfirmed off must keep its reconnect reason visible")
        }
    }

    @Test(arguments: ["refused", "missing"])
    func unconfirmedOffStaysFencedThroughUnrelatedResultAndSameSessionReconnect(outcome: String) async throws {
        let sent = SentMessages()
        let media = OpenFlag()
        let command = CommandClient(clock: clock, send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let transmit = TransmitCommandClient(commands: command, keepaliveOnMedia: { sequence, epoch in
            media.take(sequence, epoch)
        })
        let ptt = PttController(commands: transmit, clock: clock)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.update(TransmitStateReport())
        await ptt.tap()
        #expect(await sent.settle(untilCount: 3))
        let on = try Self.firstInvoke(sent, Self.isOn)
        await command.receive(Self.result(on.verb, on.id, values: [
            .init(name: "epoch", value: .i64(7)),
        ]))
        await ptt.settle()
        media.set(true)
        await ptt.setVoxArmed(true)
        await ptt.tap()
        #expect(await sent.settle(untilCount: 6))
        let off = try Self.lastInvoke(sent, Self.isOff)
        let unrelated = await command.start("tx.tune", arguments: [
            CommandArgument(name: "on", value: .bool(false)),
        ], copies: 1, timeout: .seconds(5))
        await unrelated.sent()
        let different = try Self.lastInvoke(sent) { $0.verb == "tx.tune" }
        await command.receive(Self.result(different.verb, different.id))
        #expect((try? await unrelated.result().accepted) == true)
        if outcome == "refused" {
            await command.receive(Self.result(off.verb, off.id, accepted: false,
                                              reason: "The Core refused the release."))
        } else {
            await clock.advance(by: 5_000)
        }
        await ptt.settle()
        await clock.advance(by: 600)
        #expect(media.taken.isEmpty)
        let before = sent.count
        await ptt.tap()
        await ptt.setTune(true)
        #expect(sent.count == before)
        await ptt.linkChanged(up: false)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        #expect(sent.count == before)
        await clock.advance(by: 300)
        #expect(media.taken.isEmpty)
        await ptt.logicalSessionChanged(2)
        await ptt.linkChanged(up: true)
        await ptt.tap()
        #expect(await sent.settle(untilCount: before + 3))
    }

    @Test func olderAcceptedOffCannotRetireNewerCompensationOff() async throws {
        let sent = SentMessages()
        let media = OpenFlag()
        let command = CommandClient(clock: clock, send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let transmit = TransmitCommandClient(commands: command, keepaliveOnMedia: { sequence, epoch in
            media.take(sequence, epoch)
        })
        let ptt = PttController(commands: transmit, clock: clock)
        await ptt.linkChanged(up: true)
        await ptt.update(TransmitStateReport())
        await ptt.tap()
        #expect(await sent.settle(untilCount: 3))
        let key = try Self.firstInvoke(sent, Self.isOn)
        await ptt.setVoxArmed(true)
        media.set(true)
        await ptt.tap()
        #expect(await sent.settle(untilCount: 6))
        let firstOff = try Self.lastInvoke(sent, Self.isOff)
        await command.receive(Self.result(key.verb, key.id, values: [
            .init(name: "epoch", value: .i64(7)),
        ]))
        #expect(await sent.settle(untilCount: 9))
        let compensation = try Self.lastInvoke(sent) { Self.isOff($0) && $0.id != firstOff.id }
        #expect(compensation.id != firstOff.id)
        await command.receive(Self.result(firstOff.verb, firstOff.id))
        await command.receive(Self.result(firstOff.verb, firstOff.id))
        await clock.advance(by: 300)
        #expect(media.taken.isEmpty)
        await ptt.tap()
        #expect(sent.count == 9)
        await command.receive(Self.result(compensation.verb, compensation.id))
        await ptt.settle()
        #expect(await ptt.snapshot.microphoneWanted)
        await clock.advance(by: 100)
        #expect(media.taken.count == 1)
    }

    @Test func acceptedCompensationPublishesVoxMicrophoneWithoutCoreUpdate() async throws {
        let sent = SentMessages()
        let command = CommandClient(clock: clock, send: sent.sender)
        await command.handle(.stateChanged(.ready))
        let ptt = PttController(commands: TransmitCommandClient(commands: command), clock: clock)
        await ptt.linkChanged(up: true)
        await ptt.update(TransmitStateReport())
        await ptt.tap()
        #expect(await sent.settle(untilCount: 3))
        let key = try Self.firstInvoke(sent, Self.isOn)
        await ptt.setVoxArmed(true)
        await ptt.tap()
        #expect(await sent.settle(untilCount: 6))
        let firstOff = try Self.lastInvoke(sent, Self.isOff)
        await command.receive(Self.result(firstOff.verb, firstOff.id))
        for _ in 0..<50_000 {
            if await ptt.state == .idle { break }
            await Task.yield()
        }
        #expect(await ptt.state == .idle)
        await command.receive(Self.result(key.verb, key.id, values: [
            .init(name: "epoch", value: .i64(7)),
        ]))
        #expect(await sent.settle(untilCount: 9))
        let compensation = try Self.lastInvoke(sent) { Self.isOff($0) && $0.id != firstOff.id }
        #expect(await !ptt.snapshot.microphoneWanted)
        await command.receive(Self.result(compensation.verb, compensation.id))
        await ptt.settle()
        #expect(await ptt.snapshot.microphoneWanted)
    }

    @Test(arguments: [true, false])
    func oldHeartbeatCannotUseReopenedNewSession(mediaPath: Bool) async {
        let media = OpenFlag()
        media.set(mediaPath)
        let hold = QueuedSendHold()
        let primary = OpenFlag()
        primary.set(true)
        let command = CommandClient(clock: clock, send: sent.sender, captureGuardedSender: {
            { _, gate in
                guard gate.handoff({ primary.take(1, 0) }) else { throw LinkSendError.notConnected }
            }
        })
        await command.handle(.stateChanged(.ready))
        let adapter = HeldHeartbeatTransmit(base: TransmitCommandClient(commands: command,
            keepaliveOnMedia: { media.take($0, $1) }), hold: hold, checks: HeartbeatCheckCounter(), holdAt: mediaPath ? 1 : 3)
        let ptt = PttController(commands: adapter, clock: clock)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.setVoxArmed(true)
        let oldTick = Task { await clock.advance(by: 100) }
        await hold.waitUntilEntered()
        await ptt.logicalSessionChanged(2)
        await ptt.linkChanged(up: true)
        await ptt.setVoxArmed(true)
        await hold.release()
        await oldTick.value
        #expect(media.taken.isEmpty)
        #expect(primary.taken.isEmpty)
        #expect(sent.messages.isEmpty)
        await clock.advance(by: 100)
        #expect(mediaPath ? media.taken.count == 1 : primary.taken.count == 1)
    }

    @Test func oldVoxCompletionCannotChangeReplacementOwner() async {
        let ptt = PttController(commands: TransmitCommandClient(commands: await client()), clock: clock)
        await ptt.logicalSessionChanged(1)
        await ptt.linkChanged(up: true)
        await ptt.logicalSessionChanged(2)
        await ptt.linkChanged(up: true)
        await ptt.setVoxArmed(true, owner: 1)
        #expect(await !ptt.snapshot.voxArmed)
        await ptt.setVoxArmed(true, owner: 2)
        await ptt.setVoxArmed(false, owner: 1)
        #expect(await ptt.snapshot.voxArmed)
        let canceled = CommandSendPermit()
        canceled.revoke()
        await ptt.setVoxArmed(false, owner: 2, authority: canceled)
        #expect(await ptt.snapshot.voxArmed)
    }

    @Test func theTransmitClientReadsEpochsAndRefusals() {
        let accepted = CommandResult(accepted: true, reason: "", affectedKeys: [], values: ["epoch": .int(7)],
                                     phase: nil)
        #expect(TransmitCommandClient.answer(accepted) == .accepted(epoch: 7))
        let released = CommandResult(accepted: true, reason: "", affectedKeys: [], values: [:], phase: nil)
        #expect(TransmitCommandClient.answer(released) == .accepted(epoch: nil))
        let refused = CommandResult(accepted: false,
                                    reason: "The amplifier is in standby. Operate it, or change the interlock in Setup.",
                                    affectedKeys: [],
                                    values: ["refusalCode": .text("ampStandby"), "refusalFix": .text("operateAmp")],
                                    phase: nil)
        #expect(TransmitCommandClient.answer(refused) == .refused(TxRefusalInfo(
            reason: "The amplifier is in standby. Operate it, or change the interlock in Setup.",
            code: "ampStandby", fix: "operateAmp")))
    }
}

private actor QueuedSendHold {
    private var entered = false
    private var enteredWaiter: CheckedContinuation<Void, Never>?
    private var releaseWaiter: CheckedContinuation<Void, Never>?
    private var released = false

    func enter() async {
        entered = true
        enteredWaiter?.resume()
        enteredWaiter = nil
        if !released {
            await withCheckedContinuation { releaseWaiter = $0 }
        }
    }

    func waitUntilEntered() async {
        if !entered { await withCheckedContinuation { enteredWaiter = $0 } }
    }

    func release() {
        released = true
        releaseWaiter?.resume()
        releaseWaiter = nil
    }
}

private actor ChangingSendRoute {
    private var sink: SentMessages
    init(initial: SentMessages) { sink = initial }
    func replace(with sink: SentMessages) { self.sink = sink }
    func send(_ message: LinkMessage) async throws { try await sink.sender(message) }
}

private final class BoundSendPermission: @unchecked Sendable {
    private let lock = NSLock()
    private var allowed = true
    var value: Bool { lock.withLock { allowed } }
    func revoke() { lock.withLock { allowed = false } }
}

/// Collects interim answers from `onPhase`.
private final class PhaseLog: @unchecked Sendable {
    private let lock = NSLock()
    private var logged: [CommandResult] = []

    var results: [CommandResult] { lock.withLock { logged } }

    func append(_ result: CommandResult) {
        lock.withLock { logged.append(result) }
    }
}

/// The "tx" channel as a keepalive test sees it: open or not, and what it took.
private final class OpenFlag: @unchecked Sendable {
    private let lock = NSLock()
    private var open = false
    private var took: [(Int64, Int64)] = []

    var taken: [(Int64, Int64)] { lock.withLock { took } }

    func set(_ value: Bool) {
        lock.withLock { open = value }
    }

    func take(_ sequence: Int64, _ epoch: Int64) -> Bool {
        lock.withLock {
            guard open else {
                return false
            }
            took.append((sequence, epoch))
            return true
        }
    }
}

/// Test oracle for the current RemoteTxWatchdog.cpp keepalive/onTimer rules.
/// The production clock and timer are injected in the Core tests; this
/// checks the phone's emitted heartbeat trace against those exact rules.
private struct SourceCheckedWatchdog {
    let epoch: Int64
    var lastMs: Int
    var lastSequence: Int64 = 0
    var stopped = false

    mutating func keepalive(sequence: Int64, epoch receivedEpoch: Int64, atMs: Int) -> Bool {
        guard !stopped else { return false }
        if atMs - lastMs > 400 {
            stopped = true
            return false
        }
        guard sequence > lastSequence, receivedEpoch >= epoch else { return false }
        lastSequence = sequence
        lastMs = atMs
        return true
    }

    func stopsWhenTimerFires(atMs: Int) -> Bool { stopped || atMs - lastMs > 400 }
}

private struct HeldHeartbeatTransmit: TransmitCommandSending {
    let base: TransmitCommandClient
    let hold: QueuedSendHold
    let checks: HeartbeatCheckCounter
    let holdAt: Int
    func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending {
        await base.send(verb, copies: copies)
    }
    func sendKeepalive(sequence: Int64, epoch: Int64) async {}
    func sendKeepalive(sequence: Int64, epoch: Int64, gate: TransmitHeartbeatGate,
                       stillAllowed: @escaping @Sendable () async -> Bool) async {
        await base.sendKeepalive(sequence: sequence, epoch: epoch, gate: gate) {
            let allowed = await stillAllowed()
            if await checks.next() == holdAt { await hold.enter() }
            return allowed
        }
    }
}

private actor HeartbeatCheckCounter {
    private var count = 0
    func next() -> Int { count += 1; return count }
}

extension CommandClientTests {
    /// Task 9: a refused unknown verb finishes its invoke without ending the
    /// session. The next command and a later message use the same connection.
    @Test func unknownVerbRefusalLeavesTheSameSessionReadyForAnotherCommand() async throws {
        let station = ScriptedStation(certificateSHA256: Data(repeating: 0x5A, count: 32))
        let sessionClock = ManualLinkClock()
        let commandClock = ManualLinkClock()
        let session = StationSession(endpoint: StationEndpoint(host: "127.0.0.1"),
                                     trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "conformance-token"),
                                     clock: sessionClock, transportFactory: station.factory)
        let commands = CommandClient(session: session, clock: commandClock)
        // The recorder's barrier observes an event only after the actual client
        // has handled it; nothing injects answers directly into CommandClient.
        // Each handled wait bounds event observation at 10 seconds; the session
        // and command policies still use their separate manual clocks.
        let recorder = EventRecorder(session, forward: { await commands.handle($0) })
        do {
            await session.connect()
            let transport = try #require(station.latest)
            await transport.deliver(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                 peer: "nereusd", majors: [1], features: [:])))
            guard case .hello = try LinkCodec.decode(try #require(transport.takeSent())),
                  case .authRequest(let auth) = try LinkCodec.decode(try #require(transport.takeSent())) else {
                Issue.record("expected hello followed by auth.request")
                throw CancellationError()
            }
            #expect(auth.token == "conformance-token")
            await transport.deliver(.authResult(.init(accepted: true, reason: "", retryable: false)))
            await transport.deliver(.capabilities(.init(properties: [])))
            await transport.deliver(.snapshotComplete)
            try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.stateChanged(.ready)) })
            #expect(await session.state == .ready)
            let original = await session.diagnosticsSnapshot()
            let generation = try #require(original.attemptGeneration)
            let route = try #require(original.routeID)
            #expect(station.dialCount == 1)
            #expect(transport.pending.isEmpty)
            #expect(sessionClock.pendingDueTimes == [20_000])

            let unknown = Task {
                try await commands.invoke("conformanceUnknownVerb", arguments: [], timeout: .seconds(5))
            }
            defer { unknown.cancel() }
            let invoke = try await Self.takeSessionInvoke(transport)
            #expect(invoke.verb == "conformanceUnknownVerb")
            #expect(invoke.id == 1)
            #expect(invoke.args.isEmpty)
            #expect(await commands.waitingCount == 1)
            #expect(commandClock.pendingDueTimes == [5_000])

            // Neither a matching verb with another ID nor another verb with the
            // right ID may complete this invoke. Observe both through the session.
            let wrongID = Self.result("conformanceUnknownVerb", invoke.id + 100, accepted: false,
                                      reason: "This answer belongs to another request.")
            let wrongVerb = Self.result("dspAssets.list", invoke.id, accepted: false,
                                        reason: "This answer belongs to another verb.")
            for unrelated in [wrongID, wrongVerb] {
                await transport.deliver(unrelated)
                try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.message(unrelated)) })
                #expect(await commands.waitingCount == 1)
                #expect(commandClock.pendingDueTimes == [5_000])
            }
            // Exact reason and verb from tests/data/link/v1/sessions/unknown-verb.json;
            // the ID comes from the real invoke, rather than the fixture's scripted ID.
            let reason = "The Core does not know this request. Updating the Core may help."
            let refused = Self.result("conformanceUnknownVerb", invoke.id, accepted: false, reason: reason)
            await transport.deliver(refused)
            try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.message(refused)) })
            #expect(await commands.waitingCount == 0)
            #expect(commandClock.pendingDueTimes.isEmpty)
            // A broken result handler times out deterministically instead of
            // leaving this test's task suspended on a manual clock forever.
            await commandClock.advance(by: 5_000)
            #expect(try await unknown.value == CommandResult(accepted: false, reason: reason,
                                                            affectedKeys: [], values: [:], phase: nil))
            #expect(await session.state == .ready)
            #expect(transport.isOpen)
            #expect(transport.pending.isEmpty)
            #expect(sessionClock.pendingDueTimes == [20_000])

            let ordinary = Task {
                try await commands.invoke("dspAssets.list", arguments: [], timeout: .seconds(5))
            }
            defer { ordinary.cancel() }
            let next = try await Self.takeSessionInvoke(transport)
            #expect(next.verb == "dspAssets.list")
            #expect(next.id == 2)
            #expect(next.id != invoke.id)
            #expect(next.args.isEmpty)
            // A duplicate refused result cannot settle the later command.
            await transport.deliver(refused)
            try #require(await recorder.handled(within: .seconds(10)) { events in
                events.filter { $0 == .message(refused) }.count == 2
            })
            #expect(await commands.waitingCount == 1)
            #expect(commandClock.pendingDueTimes == [10_000])
            // Non-keying accepted command and values from verbs-dsp-assets.json.
            let accepted = Self.result("dspAssets.list", next.id, values: [
                .init(name: "revision", value: .i64(1)),
                .init(name: "status", value: .utf8("NNR model selections are available.")),
            ])
            await transport.deliver(accepted)
            try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.message(accepted)) })
            #expect(await commands.waitingCount == 0)
            #expect(commandClock.pendingDueTimes.isEmpty)
            await commandClock.advance(by: 5_000)
            #expect(try await ordinary.value == CommandResult(accepted: true, reason: "", affectedKeys: [],
                values: ["revision": .int(1), "status": .text("NNR model selections are available.")], phase: nil,
                valueEntries: [
                    .init(name: "revision", value: .i64(1)),
                    .init(name: "status", value: .utf8("NNR model selections are available.")),
                ]))

            let continued = LinkMessage.settingsSnapshot(.init(properties: [
                .init(name: "StationName", value: .utf8("Still connected")),
            ]))
            await transport.deliver(continued)
            try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.message(continued)) })
            let after = await session.diagnosticsSnapshot()
            #expect(after.state == .ready)
            #expect(after.attemptGeneration == generation)
            #expect(after.routeID == route)
            #expect(station.latest === transport)
            #expect(station.dialCount == 1)
            #expect(station.openConnections == 1)
            #expect(recorder.states == [.connecting, .authenticating, .receivingSnapshot, .ready])
            #expect(recorder.refusals.isEmpty)
            #expect(transport.pending.isEmpty)
            #expect(await commands.waitingCount == 0)
            #expect(commandClock.pendingDueTimes.isEmpty)
            #expect(sessionClock.pendingDueTimes == [20_000])
            await session.disconnect()
            try #require(await recorder.handled(within: .seconds(10)) { $0.contains(.stateChanged(.stopped)) })
            #expect(sessionClock.pendingDueTimes.isEmpty)
            #expect(station.openConnections == 0)
        } catch {
            await session.disconnect()
            await recorder.handled(within: .seconds(10)) { $0.contains(.stateChanged(.stopped)) }
            throw error
        }
        // EventRecorder cancels its single stream reader when this scope ends.
    }

    /// Uses the scripted transport's real outgoing frames; yield only as the
    /// existing settle helpers do, with no sleeps or added wall-clock deadline.
    private static func takeSessionInvoke(_ transport: ScriptedTransport) async throws -> LinkMessage.CommandInvoke {
        for _ in 0..<2_000 {
            if !transport.pending.isEmpty { break }
            await Task.yield()
        }
        let message = try LinkCodec.decode(try #require(transport.takeSent()))
        guard case .commandInvoke(let invoke) = message else {
            Issue.record("expected command.invoke, got \(message)")
            throw CancellationError()
        }
        return invoke
    }
}
