// NereusSDR for iOS: record stream authority before admission and transport handoff
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct RecordStreamAdmissionTests {
    private final class User {}

    private actor Barrier {
        private var entered = false
        private var opened = false
        private var observers: [CheckedContinuation<Void, Never>] = []
        private var release: CheckedContinuation<Void, Never>?

        func pause() async {
            guard !opened else { return }
            entered = true
            let ready = observers
            observers = []
            for observer in ready { observer.resume() }
            await withCheckedContinuation { release = $0 }
        }

        func waitUntilEntered() async {
            if entered { return }
            await withCheckedContinuation { observers.append($0) }
        }

        func open() {
            opened = true
            release?.resume()
            release = nil
        }
    }

    /// Mirrors StationSession.send(_:permit:): suspend before the final
    /// synchronous handoff, check the complete permit chain inside handoff,
    /// then answer outside that fence. The fake Core tracks actual subscriptions.
    private actor Sender {
        private let firstHandoff: Barrier?
        private var heldFirst = false
        private weak var commands: CommandClient?
        private(set) var sent: [LinkMessage.CommandInvoke] = []
        private(set) var active: Set<String> = []

        init(firstHandoff: Barrier?) { self.firstHandoff = firstHandoff }
        func bind(_ commands: CommandClient) { self.commands = commands }

        /// The fake Core delivers fresh records only while its stream is active.
        func emit(_ id: String, to client: RecordStreamClient) async {
            guard active.contains("spots") else { return }
            await client.apply(.init(stream: "spots", generation: 1, reset: false,
                                     upserts: [.init(id: id, fields: [:])], removes: []))
        }

        func send(_ message: LinkMessage, permit: CommandSendPermit) async throws {
            guard case .commandInvoke(let invoke) = message else { return }
            if invoke.verb == "records.subscribe", !heldFirst, let firstHandoff {
                heldFirst = true
                await firstHandoff.pause()
            }
            guard permit.handoff({
                sent.append(invoke)
                if let argument = invoke.args.first(where: { $0.name == "stream" }),
                   case .utf8(let stream) = argument.value {
                    if invoke.verb == "records.subscribe" { active.insert(stream) }
                    if invoke.verb == "records.unsubscribe" { active.remove(stream) }
                }
                return true
            }) else { throw LinkSendError.notConnected }
            await commands?.receive(.commandResult(.init(
                verb: invoke.verb, id: invoke.id, accepted: true, reason: "", affected: [], values: nil)))
        }
    }

    private struct Rig {
        let client: RecordStreamClient
        let commands: CommandClient
        let clock: ManualLinkClock
        let sender: Sender
    }

    private func rig(firstHandoff: Barrier? = nil) async -> Rig {
        let sender = Sender(firstHandoff: firstHandoff)
        let captured: CommandClient.CommandSender = { message, permit in
            try await sender.send(message, permit: permit)
        }
        let clock = ManualLinkClock()
        // The guarded capture is the production StationSession constructor's
        // path. The unguarded fallback must not silently make this test pass.
        let commands = CommandClient(clock: clock, send: { _ in throw LinkSendError.notConnected },
                                     captureSender: { captured })
        await sender.bind(commands)
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(FixtureReplay.stationHello())
        mirror.apply(FixtureReplay.capabilities([RecordStreamClient.capabilityName: .i64(1)]))
        return Rig(client: RecordStreamClient(mirror: mirror, commands: commands),
                   commands: commands, clock: clock, sender: sender)
    }

    private func ready(_ rig: Rig) async {
        await rig.commands.handle(.stateChanged(.receivingSnapshot))
        rig.client.handle(.stateChanged(.receivingSnapshot))
        await rig.commands.handle(.stateChanged(.ready))
        rig.client.handle(.stateChanged(.ready))
    }

    private func lost(_ rig: Rig) async {
        await rig.commands.handle(.stateChanged(.waitingToRetry(seconds: 5)))
        rig.client.handle(.stateChanged(.waitingToRetry(seconds: 5)))
    }

    private func task(_ rig: Rig) throws -> Task<Void, Never> {
        try #require(rig.client.recordCommandTask)
    }

    private func expectSettled(_ rig: Rig) async {
        #expect(await rig.commands.waitingCount == 0)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        #expect(rig.clock.now == 0)
    }

    #if DEBUG
    // These two cases need the nil-by-default DEBUG admission barrier. The
    // other four exercise the existing guarded sender with no source hook.
    @Test func anUnsubscribeHeldBeforeAdmissionCannotStopTheReconnectedStream() async throws {
        let rig = await rig()
        await ready(rig)
        rig.client.want("spots", backlog: 20)
        await rig.client.recordCommandTask?.value
        let admission = Barrier()
        rig.client.beforeRecordUnsubscribeAdmissionForTesting = { await admission.pause() }
        rig.client.unwant("spots")
        let oldStop = try task(rig)
        await admission.waitUntilEntered()
        #expect(await rig.commands.waitingCount == 0)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        rig.client.beforeRecordUnsubscribeAdmissionForTesting = nil
        await lost(rig)
        rig.client.want("spots", backlog: 200)
        await ready(rig)
        await rig.client.recordCommandTask?.value
        await admission.open()
        await oldStop.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == ["records.subscribe", "records.subscribe"])
        #expect(await rig.sender.active == ["spots"])
        #expect(rig.client.isWanted("spots") && rig.client.available)
        await rig.sender.emit("new-owner-record", to: rig.client)
        #expect(rig.client.records("spots").map(\.id) == ["new-owner-record"])
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        #expect(await rig.sender.sent.map(\.verb) == ["records.subscribe", "records.subscribe", "records.unsubscribe"])
        #expect(await rig.sender.active.isEmpty)
        await expectSettled(rig)
    }

    @Test func anUnsubscribeHeldBeforeAdmissionCannotStopANewWantOnTheSameLink() async throws {
        let rig = await rig()
        await ready(rig)
        rig.client.want("spots", backlog: 20)
        await rig.client.recordCommandTask?.value
        let admission = Barrier()
        rig.client.beforeRecordUnsubscribeAdmissionForTesting = { await admission.pause() }
        rig.client.unwant("spots")
        let oldStop = try task(rig)
        await admission.waitUntilEntered()
        rig.client.beforeRecordUnsubscribeAdmissionForTesting = nil
        rig.client.want("spots", backlog: 200)
        await rig.client.recordCommandTask?.value
        await admission.open()
        await oldStop.value
        #expect(await rig.sender.sent.map(\.verb) == ["records.subscribe", "records.subscribe"])
        #expect(await rig.sender.active == ["spots"])
        await rig.sender.emit("new-want-record", to: rig.client)
        #expect(rig.client.records("spots").map(\.id) == ["new-want-record"])
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        #expect(await rig.sender.sent.map(\.verb) == ["records.subscribe", "records.subscribe", "records.unsubscribe"])
        #expect(await rig.sender.active.isEmpty)
        await expectSettled(rig)
    }

    @Test func aStillCurrentHeldUnsubscribeStopsExactlyOnce() async throws {
        let rig = await rig()
        await ready(rig)
        rig.client.want("spots", backlog: 20)
        await rig.client.recordCommandTask?.value
        let admission = Barrier()
        rig.client.beforeRecordUnsubscribeAdmissionForTesting = { await admission.pause() }
        rig.client.unwant("spots")
        let stop = try task(rig)
        await admission.waitUntilEntered()
        #expect(await rig.sender.sent.map(\.verb) == ["records.subscribe"])
        #expect(await rig.sender.active == ["spots"])
        await admission.open()
        await stop.value
        #expect(await rig.sender.sent.map(\.verb) == ["records.subscribe", "records.unsubscribe"])
        #expect(await rig.sender.active.isEmpty)
        #expect(!rig.client.isWanted("spots"))
        await expectSettled(rig)
    }

    @Test func aSubscribeHeldBeforeAdmissionCannotRunAfterItsLastUserLeaves() async throws {
        let rig = await rig()
        await ready(rig)
        let admission = Barrier()
        rig.client.beforeRecordSubscribeAdmissionForTesting = { await admission.pause() }
        rig.client.want("spots", backlog: 500)
        let oldSubscribe = try task(rig)
        await admission.waitUntilEntered()
        #expect(await rig.commands.waitingCount == 0)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        rig.client.beforeRecordSubscribeAdmissionForTesting = nil
        rig.client.unwant("spots")
        let unsubscribe = try task(rig)
        await unsubscribe.value
        let beforeRelease = await rig.sender.sent
        #expect(beforeRelease.map(\.verb) == ["records.unsubscribe"])
        await admission.open()
        await oldSubscribe.value
        let sent = await rig.sender.sent
        let active = await rig.sender.active
        #expect(sent.map(\.verb) == ["records.unsubscribe"])
        #expect(sent.first?.args == [.init(name: "stream", value: .utf8("spots"))])
        #expect(active.isEmpty)
        #expect(!rig.client.isWanted("spots"))
        #expect(rig.client.refusals["spots"] == nil)
        await expectSettled(rig)
    }

    @Test func aSubscribeHeldBeforeAdmissionCannotEnterTheReconnectedLifetime() async throws {
        let rig = await rig()
        await ready(rig)
        let admission = Barrier()
        rig.client.beforeRecordSubscribeAdmissionForTesting = { await admission.pause() }
        rig.client.want("spots", backlog: 20)
        let oldSubscribe = try task(rig)
        await admission.waitUntilEntered()
        #expect(await rig.commands.waitingCount == 0)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        rig.client.beforeRecordSubscribeAdmissionForTesting = nil
        await lost(rig)
        #expect(!rig.client.available && rig.client.streams.isEmpty)
        rig.client.want("spots", backlog: 200)
        await ready(rig)
        let currentSubscribe = try task(rig)
        await currentSubscribe.value
        await admission.open()
        await oldSubscribe.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == ["records.subscribe"])
        #expect(sent.map(\.args) == [[.init(name: "stream", value: .utf8("spots")),
                                    .init(name: "backlog", value: .i64(200))]])
        #expect(rig.client.isWanted("spots") && rig.client.available)
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let final = await rig.sender.sent
        let active = await rig.sender.active
        #expect(final.map(\.verb) == ["records.subscribe", "records.unsubscribe"])
        #expect(active.isEmpty)
        await expectSettled(rig)
    }
    #endif

    @Test func anAdmittedSubscribeCannotHandoffAfterItsLastUserLeaves() async throws {
        let handoff = Barrier()
        let rig = await rig(firstHandoff: handoff)
        await ready(rig)
        rig.client.want("spots", backlog: 500)
        let subscribe = try task(rig)
        await handoff.waitUntilEntered()
        #expect(await rig.commands.waitingCount == 1)
        #expect(rig.clock.pendingDueTimes == [5_000])
        rig.client.unwant("spots")
        let unsubscribe = try task(rig)
        await handoff.open()
        await subscribe.value
        await unsubscribe.value
        let sent = await rig.sender.sent
        let active = await rig.sender.active
        #expect(sent.map(\.verb) == ["records.unsubscribe"])
        #expect(active.isEmpty)
        #expect(!rig.client.isWanted("spots"))
        await expectSettled(rig)
    }

    @Test func anAdmittedSubscribeAlreadyLosesItsSessionAuthorityOnReconnect() async throws {
        let handoff = Barrier()
        let rig = await rig(firstHandoff: handoff)
        await ready(rig)
        rig.client.want("spots", backlog: 20)
        let oldSubscribe = try task(rig)
        await handoff.waitUntilEntered()
        await lost(rig)
        #expect(await rig.commands.waitingCount == 0)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        rig.client.want("spots", backlog: 200)
        await ready(rig)
        let currentSubscribe = try task(rig)
        await handoff.open()
        await oldSubscribe.value
        await currentSubscribe.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == ["records.subscribe"])
        #expect(sent.first?.args == [.init(name: "stream", value: .utf8("spots")),
                                    .init(name: "backlog", value: .i64(200))])
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let active = await rig.sender.active
        #expect(active.isEmpty)
        await expectSettled(rig)
    }

    @Test func oneUserLeavingCannotRevokeTheOtherUsersPendingSubscribe() async throws {
        let handoff = Barrier()
        let rig = await rig(firstHandoff: handoff)
        await ready(rig)
        let first = User()
        let last = User()
        rig.client.want("spots", backlog: 500, by: first)
        rig.client.want("spots", backlog: 500, by: last)
        let subscribe = try task(rig)
        await handoff.waitUntilEntered()
        rig.client.unwant("spots", by: first)
        #expect(rig.client.isWanted("spots", by: last))
        #expect(!rig.client.isWanted("spots", by: first))
        await handoff.open()
        await subscribe.value
        let beforeLast = await rig.sender.sent
        let activeBeforeLast = await rig.sender.active
        #expect(beforeLast.map(\.verb) == ["records.subscribe"])
        #expect(activeBeforeLast == ["spots"])
        rig.client.unwant("spots", by: last)
        await rig.client.recordCommandTask?.value
        let sent = await rig.sender.sent
        let active = await rig.sender.active
        #expect(sent.map(\.verb) == ["records.subscribe", "records.unsubscribe"])
        #expect(active.isEmpty)
        await expectSettled(rig)
    }

    @Test func aBacklogRefreshKeepsAuthorityWhileTheStreamIsStillWanted() async throws {
        let handoff = Barrier()
        let rig = await rig(firstHandoff: handoff)
        await ready(rig)
        rig.client.want("spots", backlog: 20)
        let firstSubscribe = try task(rig)
        await handoff.waitUntilEntered()
        rig.client.want("spots", backlog: 200)
        let refresh = try task(rig)
        await handoff.open()
        await firstSubscribe.value
        await refresh.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == ["records.subscribe", "records.subscribe"])
        #expect(sent.map(\.args) == [
            [.init(name: "stream", value: .utf8("spots")), .init(name: "backlog", value: .i64(20))],
            [.init(name: "stream", value: .utf8("spots")), .init(name: "backlog", value: .i64(200))],
        ])
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let final = await rig.sender.sent
        let active = await rig.sender.active
        #expect(final.map(\.verb) == ["records.subscribe", "records.subscribe", "records.unsubscribe"])
        #expect(active.isEmpty)
        await expectSettled(rig)
    }
}
