// NereusSDR for iOS: the Core's messages reach the mirror at once and the command client and media control in order
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// JJ's option 1 for the tune delay (2026-10-27): the app's event pump
/// applies each of the Core's messages to the mirror at once and hands the
/// command client and media control theirs through their own ordered
/// lanes, so a write's answer never waits behind the hops for the readings
/// before it.
@Suite("The Core's messages through the app's lanes", .serialized)
@MainActor
struct SessionEventLanesTests {
    // MARK: One lane

    /// What a lane handed over, and how many hand-overs ran at once.
    final class Handed: @unchecked Sendable {
        private let lock = NSLock()
        private var items: [Int] = []
        private var running = 0
        private var mostAtOnce = 0

        var all: [Int] { lock.withLock { items } }
        var mostRunningAtOnce: Int { lock.withLock { mostAtOnce } }

        func begin() {
            lock.withLock {
                running += 1
                mostAtOnce = max(mostAtOnce, running)
            }
        }

        func end(_ item: Int) {
            lock.withLock {
                running -= 1
                items.append(item)
            }
        }
    }

    @Test("a lane hands a burst over in order, one at a time, and a drain waits for all of it")
    func laneKeepsOrderUnderABurst() async {
        let handed = Handed()
        let lane = CoreEventLane<Int> { item in
            handed.begin()
            // Some hand-overs suspend, as an actor hop does.
            if item % 7 == 0 {
                await Task.yield()
            }
            handed.end(item)
        }
        for item in 0 ..< 5_000 {
            lane.send(item)
        }
        await lane.drain()
        #expect(handed.all == Array(0 ..< 5_000))
        #expect(handed.mostRunningAtOnce == 1)
        await lane.finish()
        #expect(lane.hasEnded)
    }

    @Test("a lane whose session is gone skips what is left, and a finished lane drains at once")
    func stoppedLaneSkipsAndFinishedLaneDrainsAtOnce() async {
        let handed = Handed()
        let lane = CoreEventLane<Int> { item in
            handed.begin()
            handed.end(item)
        }
        lane.send(1)
        await lane.drain()
        lane.stopDelivering()
        lane.send(2)
        await lane.drain()
        #expect(lane.pendingCount == 0)
        #expect(handed.all == [1])
        await lane.finish()
        #expect(lane.hasEnded)
        lane.send(3)
        await lane.drain()
        #expect(lane.pendingCount == 0)
        #expect(handed.all == [1])
    }

    // MARK: Ordered intake

    nonisolated static func inboxEvent(_ sequence: Int) -> StationSession.Event {
        .message(strayAnswer(UInt32(sequence)))
    }

    @Test("intake returns the available FIFO prefix without waiting to fill a batch", .timeLimit(.minutes(1)))
    func inboxReadyPrefixDoesNotWaitToFill() async {
        let inbox = SessionEventInbox()
        defer { inbox.close() }
        #expect(await inbox.offer(Self.inboxEvent(1)))
        #expect(await inbox.nextBatch() == [Self.inboxEvent(1)])
        for sequence in 2 ... 41 {
            #expect(await inbox.offer(Self.inboxEvent(sequence)))
        }
        #expect(await inbox.nextBatch() == (2 ... 33).map(Self.inboxEvent))
        #expect(await inbox.nextBatch() == (34 ... 41).map(Self.inboxEvent))
        inbox.finishInput()
        #expect(await inbox.nextBatch() == nil)
        #expect(!inbox.snapshotForTesting.hasBatchWaiter)
    }

    @Test("a full intake suspends its reader and normal input end drains every accepted event", .timeLimit(.minutes(1)))
    func inboxCapacitySuspendsAndResumesLosslessly() async throws {
        let inbox = SessionEventInbox()
        defer { inbox.close() }
        for sequence in 1 ... 64 {
            #expect(await inbox.offer(Self.inboxEvent(sequence)))
        }
        let producer = Task.detached { await inbox.offer(Self.inboxEvent(65)) }
        try #require(await settle { inbox.snapshotForTesting.hasCapacityWaiter })
        #expect(inbox.snapshotForTesting.pendingCount == 64)
        // End accepts no new offers, but retains the offer already waiting.
        inbox.finishInput()
        #expect(!(await inbox.offer(Self.inboxEvent(66))))
        var received = try #require(await inbox.nextBatch())
        #expect(received == (1 ... 32).map(Self.inboxEvent))
        #expect(await producer.value)
        while let batch = await inbox.nextBatch() {
            received.append(contentsOf: batch)
        }
        #expect(received == (1 ... 65).map(Self.inboxEvent))
        let state = inbox.snapshotForTesting
        #expect(state.maximumPendingCount == 64)
        #expect(state.maximumBatchCount == 32)
        #expect(state.pendingCount == 0 && !state.hasCapacityWaiter && !state.hasBatchWaiter)
    }

    @Test("closing a full intake wakes and joins its real stream reader", .timeLimit(.minutes(1)))
    func intakeCancellationWhileFullReleasesAndJoins() async throws {
        let (events, source) = AsyncStream<StationSession.Event>.makeStream()
        let intake = SessionEventIntake(events: events)
        defer { intake.inbox.close(); source.finish() }
        for sequence in 1 ... 65 { source.yield(Self.inboxEvent(sequence)) }
        try #require(await settle { intake.inbox.snapshotForTesting.hasCapacityWaiter })
        #expect(intake.inbox.snapshotForTesting.pendingCount == 64)
        await intake.closeAndJoin()
        #expect(intake.readerHasEndedForTesting)
        let state = intake.inbox.snapshotForTesting
        #expect(state.isClosed && state.pendingCount == 0)
        #expect(!state.hasCapacityWaiter && !state.hasBatchWaiter)
        #expect(!(await intake.inbox.offer(Self.inboxEvent(66))))
        #expect(await intake.nextBatch() == nil)
    }

    @Test("closing an empty intake wakes its consumer and joins a reader on an open stream", .timeLimit(.minutes(1)))
    func intakeCancellationWhileEmptyReleasesAndJoins() async throws {
        let (events, source) = AsyncStream<StationSession.Event>.makeStream()
        let intake = SessionEventIntake(events: events)
        defer { intake.inbox.close(); source.finish() }
        let consumer = Task { await intake.nextBatch() }
        try #require(await settle { intake.inbox.snapshotForTesting.hasBatchWaiter })
        // The source is still open: joining cannot depend on its deallocation.
        await intake.closeAndJoin()
        #expect(await consumer.value == nil)
        #expect(intake.readerHasEndedForTesting)
        let state = intake.inbox.snapshotForTesting
        #expect(state.isClosed && !state.hasBatchWaiter && !state.hasCapacityWaiter)

        let inbox = SessionEventInbox()
        let cancelledConsumer = Task.detached { await inbox.nextBatch() }
        try #require(await settle { inbox.snapshotForTesting.hasBatchWaiter })
        cancelledConsumer.cancel()
        #expect(await cancelledConsumer.value == nil)
        #expect(inbox.snapshotForTesting.isClosed && !inbox.snapshotForTesting.hasBatchWaiter)
    }

    @Test("cancellation before waiter registration cannot leave a full or empty intake suspended",
          .timeLimit(.minutes(1)), arguments: [false, true])
    func cancellationBeforeWaiterRegistrationCannotLoseWake(full: Bool) async throws {
        let inbox = SessionEventInbox()
        defer { inbox.close() }
        if full {
            for sequence in 1 ... 64 { #expect(await inbox.offer(Self.inboxEvent(sequence))) }
        }
        let gate = DeliveryGate()
        let operation = Task.detached {
            await gate.wait()
            if full { return await inbox.offer(Self.inboxEvent(65)) }
            return await inbox.nextBatch() != nil
        }
        try #require(await gate.waitUntilEntered())
        operation.cancel()
        await gate.release()
        #expect(!(await operation.value))
        let state = inbox.snapshotForTesting
        #expect(state.isClosed && state.pendingCount == 0)
        #expect(!state.hasCapacityWaiter && !state.hasBatchWaiter)
    }

    @Test("the real batched pump preserves every reading and relevant event through a full intake",
          .timeLimit(.minutes(2)))
    func actualPumpPreservesEveryReadingAndRelevantEventAcrossFullInbox() async throws {
        let station = try FakeStation(additions: FakeStation.Additions.all.union(.remoteTx))
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try await station.deliverSetupPanels()
        try #require(await settle { model.mirror.object("txState") != nil })
        let slice = try #require(model.mirror.object("slice:1"))
        let lanes = try #require(model.eventLanesForTesting)
        let intake = try #require(model.eventIntakeForTesting)
        let expected = (0 ..< 160).map { -180.0 + Double($0) / 1_000 }
        var readings: [Double] = []
        let observation = slice.$values.sink { values in
            MainActor.assumeIsolated {
                if case .double(let value)? = values["signalStrengthDbm"], expected.contains(value) {
                    readings.append(value)
                }
            }
        }
        defer { observation.cancel() }
        let gate = DeliveryGate()
        lanes.hooksForTesting.setCommands { await gate.wait() }
        defer { Task { await gate.release() } }
        await station.deliver(Self.strayAnswer(6_999_999))
        try #require(await gate.waitUntilEntered())
        // This direct barrier must stop the real pump before every reading below.
        await station.setSetupKeyed(true)
        for sequence in 0 ..< 160 {
            await station.deliver(.delta(.init(key: "slice:1", properties: [
                .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(expected[sequence])),
            ])))
            if sequence % 4 == 0 { await station.deliver(Self.strayAnswer(UInt32(7_000_000 + sequence / 4))) }
            if sequence % 8 == 0 { await station.deliver(Self.mediaOperation(10_000 + sequence / 8)) }
        }
        try #require(await settle { intake.inbox.snapshotForTesting.hasCapacityWaiter })
        #expect(intake.inbox.snapshotForTesting.pendingCount == 64)
        #expect(readings.isEmpty)
        await gate.release()
        let resultIds = { () -> [UInt32] in
            lanes.handedForTesting.all.compactMap { entry in
                if case .commands(.message(.commandResult(let result))) = entry,
                   result.verb == "test.lane", result.id >= 7_000_000 { return result.id }
                return nil
            }
        }
        let operations = { () -> [Int] in
            lanes.handedForTesting.all.compactMap { entry in
                if case .media(.event(.message(.mediaControl(let control)))) = entry,
                   case .number(let sequence)? = control.payload["sequence"], sequence >= 10_000 {
                    return Int(sequence)
                }
                return nil
            }
        }
        try #require(await settle { readings.count == 160 && resultIds().count == 40 && operations().count == 20 })
        #expect(readings == expected)
        #expect(resultIds() == (0 ..< 40).map { UInt32(7_000_000 + $0) })
        #expect(operations() == Array(10_000 ..< 10_020))
        #expect(intake.inbox.snapshotForTesting.maximumPendingCount == 64)
        #expect(intake.inbox.snapshotForTesting.maximumBatchCount == 32)
        #expect(lanes.commands.maximumPendingCount <= 64 && lanes.media.maximumPendingCount <= 65)
        await model.disconnect()
        #expect(intake.readerHasEndedForTesting && lanes.hasEnded)
        let ended = intake.inbox.snapshotForTesting
        #expect(ended.pendingCount == 0 && !ended.hasCapacityWaiter && !ended.hasBatchWaiter)
    }

    @Test("a transmit barrier inside a ready prefix finishes before its following mirror reading",
          .timeLimit(.minutes(2)))
    func barrierInsideReadyPrefixPreservesTransmitOrdering() async throws {
        let station = try FakeStation(additions: FakeStation.Additions.all.union(.remoteTx))
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try await station.deliverSetupPanels()
        try #require(await settle { model.mirror.object("txState") != nil })
        let slice = try #require(model.mirror.object("slice:1"))
        let lanes = try #require(model.eventLanesForTesting)
        let intake = try #require(model.eventIntakeForTesting)
        try await LinkBarrier.roundTrip(model.commands)
        await lanes.drain()
        try #require(await settle { intake.inbox.snapshotForTesting.hasBatchWaiter })
        let gate = DeliveryGate()
        lanes.hooksForTesting.setCommands { await gate.wait() }
        defer { Task { await gate.release() } }
        await station.deliver(Self.strayAnswer(7_999_999))
        try #require(await gate.waitUntilEntered())
        let takenBefore = intake.inbox.snapshotForTesting.takenCount
        await station.setSetupKeyed(true)
        // The fake has no spontaneous event publisher here. Once this first
        // barrier has been taken, the next prefix waits behind its drain.
        try #require(await settle { intake.inbox.snapshotForTesting.takenCount > takenBefore })
        let silences = { () -> [Bool] in
            var values: [Bool] = []
            for entry in lanes.handedForTesting.all {
                if case .media(.transmitSilence(let expected)) = entry, values.last != expected { values.append(expected) }
            }
            return values
        }
        var markers: [Double] = []
        var heldWhenShown: Bool?
        let observation = slice.$values.sink { values in
            MainActor.assumeIsolated {
                if case .double(let value)? = values["signalStrengthDbm"], value == -31 || value == -32 {
                    markers.append(value)
                    if value == -32 { heldWhenShown = silences().last == false }
                }
            }
        }
        defer { observation.cancel() }
        await station.deliver(.delta(.init(key: "slice:1", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-31)),
        ])))
        await station.setSetupKeyed(false)
        await station.deliver(.delta(.init(key: "slice:1", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-32)),
        ])))
        await Self.readings(station, rounds: 40)
        try #require(await settle { intake.inbox.snapshotForTesting.hasCapacityWaiter })
        #expect(markers.isEmpty)
        await gate.release()
        try #require(await settle { heldWhenShown != nil })
        #expect(markers == [-31, -32])
        #expect(heldWhenShown == true)
        #expect(silences() == [false, true, false])
        #expect(intake.inbox.snapshotForTesting.maximumBatchCount == 32)
        await model.disconnect()
        #expect(intake.readerHasEndedForTesting && lanes.hasEnded)
    }

    // MARK: The app's pump

    /// The Core's readings for both slices, `count` rounds of them.
    nonisolated static func readings(_ station: FakeStation, rounds count: Int, from level: Double = -110) async {
        for round in 0 ..< count {
            for slice in 0 ..< 2 {
                await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(slice)", properties: [
                    .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(level + Double(round % 50))),
                ])))
            }
        }
    }

    /// A media operation media control has no connection for: it reads it
    /// and does nothing, so the lane's order shows in what it handed over.
    nonisolated static func mediaOperation(_ sequence: Int) -> LinkMessage {
        .mediaControl(LinkMessage.MediaControl(payload: [
            "connectionId": .string("lane-test"), "op": .string("noise-floor"), "sequence": .number(Double(sequence)),
        ]))
    }

    /// An answer to a command nobody waits for: the command client reads it
    /// and lets it go, so the lane's order shows in what it handed over.
    nonisolated static func strayAnswer(_ id: UInt32) -> LinkMessage {
        .commandResult(LinkMessage.CommandResult(verb: "test.lane", id: id, accepted: true, reason: "",
                                                 affected: [], values: nil))
    }

    func connected(_ station: FakeStation) async throws -> AppModel {
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try #require(await settle { model.connection == .connected })
        return model
    }

    @Test("while readings flood, the command client and media control each get theirs in arrival order and no reading")
    func eachConsumerKeepsArrivalOrderUnderABurst() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try #require(await settle { model.mirror.object("slice:1") != nil })
        let lanes = try #require(model.eventLanesForTesting)
        let rounds = 40
        // Commands the fake answers at once, sent while the burst runs.
        let commands = model.commands
        let invoked = Task.detached {
            var accepted = 0
            for _ in 0 ..< 10 {
                if (try? await commands.invoke(FakeStation.barrierVerb, arguments: [], timeout: .seconds(30)))?
                    .accepted == true {
                    accepted += 1
                }
            }
            return accepted
        }
        let burst = Task.detached {
            for round in 0 ..< rounds {
                await Self.readings(station, rounds: 10)
                await station.deliver(Self.strayAnswer(UInt32(1_000_000 + round)))
                await Self.readings(station, rounds: 3)
                await station.deliver(Self.mediaOperation(round))
            }
        }
        await burst.value
        #expect(await invoked.value == 10)
        let strays = { () -> [UInt32] in
            lanes.handedForTesting.all.compactMap { entry in
                if case .commands(.message(.commandResult(let result))) = entry, result.verb == "test.lane" {
                    return result.id
                }
                return nil
            }
        }
        let operations = { () -> [Int] in
            lanes.handedForTesting.all.compactMap { entry in
                if case .media(.event(.message(.mediaControl(let control)))) = entry,
                   case .number(let sequence)? = control.payload["sequence"] {
                    return Int(sequence)
                }
                return nil
            }
        }
        #expect(await settle { strays().count == rounds && operations().count == rounds })
        #expect(strays() == (0 ..< rounds).map { UInt32(1_000_000 + $0) })
        #expect(operations() == Array(0 ..< rounds))
        // No reading went into either lane.
        #expect(!lanes.handedForTesting.all.contains { entry in
            switch entry {
            case .commands(.message(.delta)), .media(.event(.message(.delta))):
                return true
            default:
                return false
            }
        })
        // The mirror took every reading: the last round's value is shown.
        let last = try #require(model.mirror.object("slice:1")?["signalStrengthDbm"])
        #expect(last == .double(-110 + 2))
        await model.disconnect()
    }

    @Test("while readings flood and the main thread is held 15 ms a frame, a write's answer is applied at once",
          .timeLimit(.minutes(2)))
    func writeAnswerIsAppliedAtOnceWhileReadingsFlood() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try #require(await settle { model.mirror.object("slice:0")?["frequency"] != nil })
        let holder = TuneUnderLoadTests.MainHolder()
        holder.start(holdingMs: 15)
        defer { holder.stop() }
        let flooding = Flag()
        // Ten times the Core's own rate: a reading for each slice every 5 ms.
        let flood = Task.detached {
            while flooding.isSet {
                await Self.readings(station, rounds: 1)
                try? await Task.sleep(for: .milliseconds(5))
            }
        }
        defer { flooding.clear() }
        try await Task.sleep(for: .seconds(1))
        var waits: [Double] = []
        for step in 1 ... 10 {
            let hz = 7_236_400.0 + Double(step) * 100
            let before = station.messages.count
            let answered = Answered()
            let core = Task.detached {
                let written = { @Sendable () -> LinkMessage.PropertyWrite? in
                    station.messages.dropFirst(before).lazy.compactMap { message -> LinkMessage.PropertyWrite? in
                        if case .propertyWrite(let write) = message, write.key == "slice:0" { return write }
                        return nil
                    }.first
                }
                _ = await station.waitUntil(within: .seconds(10)) { written() != nil }
                guard let write = written(), let writeId = write.writeId, let entry = write.properties.first else {
                    return
                }
                // Noted as it leaves the fake: the session's own read is in the wait.
                answered.note()
                await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId,
                                                                                 results: [
                    .init(property: entry.name, accepted: true, reason: "", value: entry),
                ])))
            }
            let outcome = await model.mirror.write("slice:0", property: "frequency", value: .double(hz))
            let applied = ProcessInfo.processInfo.systemUptime
            await core.value
            #expect(outcome.accepted && outcome.answeredByCore, "write \(step): \(outcome.reason)" as Comment)
            if let at = answered.at {
                waits.append(max(0, applied - at) * 1_000)
            }
        }
        let sorted = waits.sorted()
        print("SessionEventLanes write answer to applied ms (main held 15 ms a frame, readings every 5 ms): "
              + "median \(Int(sorted[sorted.count / 2])) max \(Int(sorted.last ?? 0)) of \(sorted.count)")
        #expect(waits.count == 10)
        // A main-queue turn waits at most a frame (16.7 ms) under the hold;
        // the pump used to need three hops for each reading before the answer.
        #expect((sorted.last ?? .infinity) < 100, "an answer waited \(Int(sorted.last ?? 0)) ms")
        flooding.clear()
        await flood.value
        holder.stop()
        await model.disconnect()
    }

    @Test("relevant events backpressure the real pump and resume in order")
    func relevantEventsBackpressurePump() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try #require(await settle { model.mirror.object("slice:1") != nil })
        let lanes = try #require(model.eventLanesForTesting)
        let gate = DeliveryGate()
        lanes.hooksForTesting.setCommands { await gate.wait() }
        defer { Task { await gate.release() } }
        for id in 0 ..< 100 {
            await station.deliver(Self.strayAnswer(UInt32(4_000_000 + id)))
        }
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-42)),
        ])))
        #expect(await settle { lanes.commands.pendingCount >= 64 })
        // A bounded observation also gives a broken pump a chance to expose its overrun.
        let deadline = ContinuousClock.now + .seconds(2)
        while ContinuousClock.now < deadline && lanes.commands.maximumPendingCount <= 64
                && model.mirror.object("slice:1")?["signalStrengthDbm"] != .double(-42) {
            await Task.yield()
        }
        #expect(lanes.commands.maximumPendingCount <= 64)
        #expect(model.mirror.object("slice:1")?["signalStrengthDbm"] != .double(-42))
        await gate.release()
        #expect(await settle {
            lanes.handedForTesting.all.filter {
                if case .commands(.message(.commandResult(let result))) = $0 {
                    return result.verb == "test.lane" && result.id >= 4_000_000
                }
                return false
            }.count == 100 && model.mirror.object("slice:1")?["signalStrengthDbm"] == .double(-42)
        })
        let ids = lanes.handedForTesting.all.compactMap { entry -> UInt32? in
            if case .commands(.message(.commandResult(let result))) = entry,
               result.verb == "test.lane", result.id >= 4_000_000 { return result.id }
            return nil
        }
        #expect(ids == (0 ..< 100).map { UInt32(4_000_000 + $0) })
        #expect(await settle { lanes.commands.pendingCount == 0 })
        await model.disconnect()
        #expect(lanes.hasEnded)
    }

    @Test("media events backpressure the real pump without dropping operations")
    func mediaEventsBackpressurePump() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try #require(await settle { model.mirror.object("slice:1") != nil })
        let lanes = try #require(model.eventLanesForTesting)
        let gate = DeliveryGate()
        lanes.hooksForTesting.setMedia { await gate.wait() }
        defer { Task { await gate.release() } }
        for sequence in 0 ..< 100 { await station.deliver(Self.mediaOperation(sequence)) }
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-43)),
        ])))
        #expect(await settle { lanes.media.pendingCount >= 64 })
        let deadline = ContinuousClock.now + .seconds(2)
        while ContinuousClock.now < deadline && lanes.media.maximumPendingCount <= 65
                && model.mirror.object("slice:1")?["signalStrengthDbm"] != .double(-43) {
            await Task.yield()
        }
        #expect(lanes.media.maximumPendingCount <= 65)
        #expect(model.mirror.object("slice:1")?["signalStrengthDbm"] != .double(-43))
        await gate.release()
        let operations = { () -> [Int] in
            lanes.handedForTesting.all.compactMap { entry in
                if case .media(.event(.message(.mediaControl(let control)))) = entry,
                   case .number(let sequence)? = control.payload["sequence"] { return Int(sequence) }
                return nil
            }
        }
        #expect(await settle { operations().count == 100
            && model.mirror.object("slice:1")?["signalStrengthDbm"] == .double(-43) })
        #expect(operations() == Array(0 ..< 100))
        #expect(await settle { lanes.media.pendingCount == 0 })
        await model.disconnect()
        #expect(lanes.hasEnded)
    }

    @Test("meter readings bypass a held command lane below its high-water mark")
    func meterFloodBypassesHeldLane() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try #require(await settle { model.mirror.object("slice:1") != nil })
        let lanes = try #require(model.eventLanesForTesting)
        let gate = DeliveryGate()
        lanes.hooksForTesting.setCommands { await gate.wait() }
        defer { Task { await gate.release() } }
        await station.deliver(Self.strayAnswer(5_000_000))
        #expect(await settle { lanes.commands.pendingCount == 1 })
        await Self.readings(station, rounds: 100)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-41)),
        ])))
        #expect(await settle { model.mirror.object("slice:1")?["signalStrengthDbm"] == .double(-41) })
        #expect(lanes.commands.pendingCount == 1)
        #expect(lanes.media.pendingCount == 0)
        await gate.release()
        await model.disconnect()
    }

    @Test("a held old lane fences publication of a new session", arguments: [false, true])
    func heldOldLaneFencesReconnect(media: Bool) async throws {
        let oldStation = try FakeStation(additions: .all)
        let newStation = try FakeStation(additions: .all)
        let model = try await connected(oldStation)
        let old = try #require(model.eventLanesForTesting)
        let oldSession = try #require(model.session)
        let oldIntake = try #require(model.eventIntakeForTesting)
        let gate = DeliveryGate()
        if media {
            old.hooksForTesting.setMedia { await gate.wait() }
            await oldStation.deliver(Self.mediaOperation(9_000))
        } else {
            old.hooksForTesting.setCommands { await gate.wait() }
            await oldStation.deliver(Self.strayAnswer(6_000_000))
        }
        defer { Task { await gate.release() } }
        #expect(await gate.waitUntilEntered())
        if !media {
            // Hold the real pump at a direct barrier, then fill its intake.
            await oldStation.setSetupKeyed(true)
            await Self.readings(oldStation, rounds: 50)
            try #require(await settle { oldIntake.inbox.snapshotForTesting.hasCapacityWaiter })
        }
        var stoppedSession = false
        model.disconnectAfterSessionStopForTesting = { stoppedSession = true }
        let reconnect = Task {
            await model.connect(to: newStation.endpoint, trust: newStation.trust,
                                authenticator: newStation.authenticator,
                                transportFactory: newStation.transportFactory, clock: HeartbeatSoonLinkClock())
        }
        #expect(await settle { stoppedSession })
        #expect(model.session === oldSession)
        #expect(model.eventLanesForTesting === old)
        #expect(!old.hasEnded)
        #expect(model.eventIntakeForTesting === oldIntake)
        if !media { #expect(!oldIntake.readerHasEndedForTesting) }
        await gate.release()
        await reconnect.value
        model.disconnectAfterSessionStopForTesting = nil
        #expect(old.hasEnded)
        #expect(oldIntake.readerHasEndedForTesting)
        let oldIntakeState = oldIntake.inbox.snapshotForTesting
        #expect(oldIntakeState.pendingCount == 0 && !oldIntakeState.hasCapacityWaiter && !oldIntakeState.hasBatchWaiter)
        try #require(await settle { model.connection == .connected })
        let fresh = try #require(model.eventLanesForTesting)
        #expect(fresh !== old)
        #expect(model.session !== oldSession)
        let oldCount = old.handedForTesting.all.count
        let answer = try await model.commands.invoke(FakeStation.barrierVerb, arguments: [], timeout: .seconds(30))
        #expect(answer.accepted)
        #expect(old.handedForTesting.all.count == oldCount)
        await model.disconnect()
    }

    @Test("on disconnect the lanes end with the session; a new connection gets lanes of its own")
    func lanesEndWithTheirSession() async throws {
        let station = try FakeStation(additions: .all)
        let model = try await connected(station)
        let first = try #require(model.eventLanesForTesting)
        let burst = Task.detached {
            for round in 0 ..< 20 {
                await Self.readings(station, rounds: 5)
                await station.deliver(Self.strayAnswer(UInt32(2_000_000 + round)))
                await station.deliver(Self.mediaOperation(round))
            }
        }
        await burst.value
        await model.disconnect()
        #expect(first.hasEnded)
        #expect(!first.commands.isDelivering && !first.media.isDelivering)
        #expect(model.eventLanesForTesting == nil)
        // Everything the burst put in was handed over before the lanes ended.
        #expect(first.handedForTesting.all.filter { entry in
            if case .commands(.message(.commandResult(let result))) = entry { return result.verb == "test.lane" }
            return false
        }.count == 20)

        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        try #require(await settle { model.connection == .connected })
        let second = try #require(model.eventLanesForTesting)
        #expect(second !== first)
        #expect(second.commands.isDelivering && second.media.isDelivering)
        let answer = try await model.commands.invoke(FakeStation.barrierVerb, arguments: [], timeout: .seconds(30))
        #expect(answer.accepted)
        #expect(second.handedForTesting.all.contains { entry in
            if case .commands(.message(.commandResult(let result))) = entry {
                return result.verb == FakeStation.barrierVerb
            }
            return false
        })
        // Nothing reached the ended lanes since.
        let handedBefore = first.handedForTesting.all.count
        await station.deliver(Self.strayAnswer(3_000_000))
        try await LinkBarrier.roundTrip(model.commands)
        #expect(first.handedForTesting.all.count == handedBefore)
        await model.disconnect()
        #expect(second.hasEnded)
    }

    @Test("media control holds the transmit silence before the mirror takes the next message, in the mirror's order")
    func transmitSilenceKeepsItsSequence() async throws {
        let station = try FakeStation(additions: FakeStation.Additions.all.union(.remoteTx))
        let model = try await connected(station)
        try await MainScreenShotTests.fill(station)
        try await station.deliverSetupPanels()
        try #require(await settle { model.mirror.object("txState") != nil })
        let slice = try #require(model.mirror.object("slice:1"))
        let lanes = try #require(model.eventLanesForTesting)
        let silences = { () -> [Bool] in
            var values: [Bool] = []
            for entry in lanes.handedForTesting.all {
                if case .media(.transmitSilence(let expected)) = entry, values.last != expected {
                    values.append(expected)
                }
            }
            return values
        }
        for (keyed, marker) in [(true, -21.0), (false, -22.0)] {
            // At the marker's synchronous publication, media control must
            // already hold its silence, even if the next reading follows now.
            var heldWhenShown: Bool?
            let observation = slice.$values.sink { values in
                MainActor.assumeIsolated {
                    if heldWhenShown == nil, values["signalStrengthDbm"] == .double(marker) {
                        heldWhenShown = silences().last == keyed
                    }
                }
            }
            // The key change and a reading right behind it, while readings flow.
            let sent = Task.detached {
                await Self.readings(station, rounds: 20)
                await station.setSetupKeyed(keyed)
                await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
                    .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(marker)),
                ])))
                await Self.readings(station, rounds: 20, from: -100)
            }
            let giveUp = ContinuousClock.now + .seconds(10)
            while heldWhenShown == nil && ContinuousClock.now < giveUp {
                await Task.yield()
            }
            await sent.value
            observation.cancel()
            #expect(heldWhenShown == true, "keyed \(keyed): media control's silence when the next reading showed")
            #expect(model.expectedTransmitSilence() == keyed)
        }
        #expect(silences() == [false, true, false])
        await model.disconnect()
    }

    // MARK: Helpers

    actor DeliveryGate {
        private var open = false
        private var entered = false
        private var waiters: [CheckedContinuation<Void, Never>] = []
        func wait() async {
            entered = true
            if open { return }
            await withCheckedContinuation { waiters.append($0) }
        }
        func waitUntilEntered() async -> Bool {
            let deadline = ContinuousClock.now + .seconds(20)
            while !entered && ContinuousClock.now < deadline { await Task.yield() }
            return entered
        }
        func release() {
            open = true
            let pending = waiters
            waiters.removeAll()
            for waiter in pending { waiter.resume() }
        }
    }

    final class Flag: @unchecked Sendable {
        private let lock = NSLock()
        private var set = true
        var isSet: Bool { lock.withLock { set } }
        func clear() { lock.withLock { set = false } }
    }

    final class Answered: @unchecked Sendable {
        private let lock = NSLock()
        private var time: Double?
        var at: Double? { lock.withLock { time } }
        func note() { lock.withLock { time = ProcessInfo.processInfo.systemUptime } }
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        let giveUp = ContinuousClock.now + .seconds(20)
        while ContinuousClock.now < giveUp {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(10))
        }
        return condition()
    }
}
