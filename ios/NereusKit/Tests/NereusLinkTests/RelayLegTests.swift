// NereusSDR for iOS: deterministic web relay leg behavior
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
import LinkSessionTestSupport
@testable import NereusLink

private actor RelayTestSink {
    let events: AsyncStream<RelayLegEvent>
    private let continuation: AsyncStream<RelayLegEvent>.Continuation
    init() { (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self) }
    func record(_ event: RelayLegEvent) { continuation.yield(event) }
}

private actor RelayLifecycleGate {
    private var waiter: CheckedContinuation<Void, Never>?
    private var open = false
    func wait() async {
        if open { return }
        await withCheckedContinuation { waiter = $0 }
    }
    func release() {
        open = true
        waiter?.resume()
        waiter = nil
    }
}

private final class RelayTestSocket: RelayBinarySocket, @unchecked Sendable {
    private let incomingStream: AsyncStream<RelaySocketEvent>
    let sent: AsyncStream<Data>
    private let incoming: AsyncStream<RelaySocketEvent>.Continuation
    private let outgoing: AsyncStream<Data>.Continuation
    private let lock = NSLock()
    private var stopped = false
    private var blocked: CheckedContinuation<Void, Error>?
    private var messages: [Data] = []
    private var reads = 0
    var readCount: Int { lock.withLock { reads } }
    var blockData = false
    var holdBlockedOnClose = false
    var isBlocked: Bool { lock.withLock { blocked != nil } }
    var sentMessages: [Data] { lock.withLock { messages } }

    init() {
        (incomingStream, incoming) = AsyncStream.makeStream(of: RelaySocketEvent.self)
        (sent, outgoing) = AsyncStream.makeStream(of: Data.self)
    }

    func send(_ message: Data) async throws {
        if message.first != 0x80 && lock.withLock({ blockData }) {
            try await withCheckedThrowingContinuation { (waiter: CheckedContinuation<Void, Error>) in
                lock.withLock { blocked = waiter }
            }
        }
        guard !lock.withLock({ stopped }) else { throw RelaySocketError.closed }
        lock.withLock { messages.append(message) }
        outgoing.yield(message)
    }

    func release() {
        let waiter = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            let old = blocked; blocked = nil; blockData = false; return old
        }
        waiter?.resume()
    }

    func nextEvent() async -> RelaySocketEvent? {
        lock.withLock { reads += 1 }
        var iterator = incomingStream.makeAsyncIterator()
        return await iterator.next()
    }

    func receive(_ event: RelaySocketEvent) { incoming.yield(event) }
    func close() async {
        let waiter = lock.withLock { () -> CheckedContinuation<Void, Error>? in
            stopped = true
            if holdBlockedOnClose { return nil }
            let old = blocked; blocked = nil; return old
        }
        waiter?.resume(throwing: RelaySocketError.closed)
        incoming.yield(.closed)
        incoming.finish()
    }
}

private actor RelaySocketPool {
    let sockets: [RelayTestSocket]
    var count = 0
    init(_ sockets: RelayTestSocket...) { self.sockets = sockets }
    func next(_ url: URL) throws -> any RelayBinarySocket {
        guard count < sockets.count else { throw RelaySocketError.closed }
        let next = sockets[count]
        count += 1
        return next
    }
}

private func relayGrant() throws -> RelayGrant {
    try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!, token: "opaqueToken_1", expires: 1)
}

private func eventually(_ condition: @escaping () async -> Bool) async {
    let deadline = ContinuousClock.now + .seconds(5)
    while !(await condition()) && ContinuousClock.now < deadline { await Task.yield() }
    #expect(await condition())
}

struct RelayLegTests {
    @Test func joinsForwardsBothLanesAndDropsWithoutPeer() async throws {
        let socket = RelayTestSocket()
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, onLifecycle: { await sink.record($0) })
        var sent = socket.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        #expect(await sent.next() == Data([0x80] + Array("opaqueToken_1".utf8)))
        #expect(await leg.send(Data([0x11]), on: .control) == false)
        socket.receive(.binary(Data([0x81, 0x02, 0x00, 0xff])))
        #expect(await received.next() == .ready(peerPresent: false))
        socket.receive(.binary(Data([0x82, 0x01, 0xff])))
        #expect(await received.next() == .peer(present: true))
        #expect(await leg.send(Data([0x17, 0xfe, 0xfd]), on: .media))
        #expect(await sent.next() == Data([0x02, 0x17, 0xfe, 0xfd]))
        socket.receive(.binary(Data([0x01, 0xaa])))
        var control = leg.controlDatagrams.makeAsyncIterator()
        #expect(await control.next() == Data([0xaa]))
        await leg.cancel()
    }

    @Test func closeWithoutEndRejoinsSameGrantWithinThirtySeconds() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent1 = first.sent.makeAsyncIterator(), sent2 = second.sent.makeAsyncIterator()
        await leg.start()
        let firstJoin = await sent1.next()
        first.receive(.binary(Data([0x81, 0x01, 0x01])))
        var received = sink.events.makeAsyncIterator()
        #expect(await received.next() == .ready(peerPresent: true))
        first.receive(.closed)
        await eventually { clock.pendingDueTimes.contains(0) }
        await clock.advance(by: 0)
        #expect(await sent2.next() == firstJoin)
        second.receive(.binary(Data([0x81, 0x01, 0x01])))
        #expect(await received.next() == .ready(peerPresent: true))
        await clock.advance(by: 30_000)
        #expect(await leg.send(Data([0xbb]), on: .control))
        #expect(await sent2.next() == Data([0x01, 0xbb]))
        await leg.cancel()
    }

    @Test func aBusyEndRetriesWithoutTheNoEndThirtySecondWindow() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent = first.sent.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        first.receive(.binary(Data([0x83] + Array("full".utf8))))
        await eventually { clock.pendingDueTimes.contains(1_000) }
        #expect(clock.pendingDueTimes.contains(30_000) == false)
        await leg.cancel()
    }

    @Test func endCodesHaveDistinctRetryAndTerminalMeaning() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent1 = first.sent.makeAsyncIterator(), sent2 = second.sent.makeAsyncIterator()
        await leg.start()
        let join = await sent1.next()
        first.receive(.binary(Data([0x83] + Array("full".utf8))))
        await eventually { clock.pendingDueTimes.contains(1_000) }
        await clock.advance(by: 999)
        #expect(await pool.count == 1)
        await clock.advance(by: 1)
        #expect(await sent2.next() == join)
        second.receive(.binary(Data([0x83] + Array("expired".utf8))))
        var received = sink.events.makeAsyncIterator()
        #expect(await received.next() == .ended(.expired))
        await clock.advance(by: 30_000)
        #expect(await pool.count == 2)
    }

    @Test func slowLifecycleConsumerPreservesStateAndSeparateNewestDatagrams() async throws {
        let socket = RelayTestSocket()
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let gate = RelayLifecycleGate()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) },
                           onLifecycle: { event in
            await sink.record(event)
            if case .ready = event { await gate.wait() }
        })
        var sent = socket.sent.makeAsyncIterator()
        var status = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        socket.receive(.binary(Data([0x81, 0x01, 0x01])))
        #expect(await status.next() == .ready(peerPresent: true))
        for id in 0..<100 {
            socket.receive(.binary(Data([0x01, UInt8(id)])))
            socket.receive(.binary(Data([0x02, UInt8(id)])))
        }
        socket.receive(.binary(Data([0x83] + Array("idle".utf8))))
        #expect(socket.readCount == 1)
        await gate.release()
        #expect(await status.next() == .ended(.idle))
        var control = leg.controlDatagrams.makeAsyncIterator()
        var media = leg.mediaDatagrams.makeAsyncIterator()
        for id in 36..<100 {
            #expect(await control.next() == Data([UInt8(id)]))
            #expect(await media.next() == Data([UInt8(id)]))
        }
    }

    @Test func fallbackSendYieldsToTheNewlyNonemptyLane() async throws {
        let socket = RelayTestSocket()
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) },
                           onLifecycle: { await sink.record($0) })
        var sent = socket.sent.makeAsyncIterator()
        var status = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        socket.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        #expect(await leg.send(Data([0]), on: .control))
        #expect(await sent.next() == Data([1, 0]))
        socket.blockData = true
        #expect(await leg.send(Data([1]), on: .control))
        await eventually { socket.isBlocked }
        #expect(await leg.send(Data([2]), on: .control))
        #expect(await leg.send(Data([3]), on: .media))
        socket.release()
        #expect(await sent.next() == Data([1, 1]))
        #expect(await sent.next() == Data([2, 3]))
        #expect(await sent.next() == Data([1, 2]))
        await leg.cancel()
    }

    @Test func mediaFallbackYieldsToNewControl() async throws {
        let socket = RelayTestSocket()
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) },
                           onLifecycle: { await sink.record($0) })
        var sent = socket.sent.makeAsyncIterator()
        var status = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        socket.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        #expect(await leg.send(Data([0]), on: .media))
        #expect(await sent.next() == Data([2, 0]))
        socket.blockData = true
        #expect(await leg.send(Data([1]), on: .media))
        await eventually { socket.isBlocked }
        #expect(await leg.send(Data([2]), on: .media))
        #expect(await leg.send(Data([3]), on: .control))
        socket.release()
        #expect(await sent.next() == Data([2, 1]))
        #expect(await sent.next() == Data([1, 3]))
        #expect(await sent.next() == Data([2, 2]))
        await leg.cancel()
    }

    @Test func queuesDropOldestIndependentlyAndAlternateLanes() async throws {
        let socket = RelayTestSocket()
        socket.blockData = true
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, onLifecycle: { await sink.record($0) })
        var sent = socket.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        socket.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await received.next()
        #expect(await leg.send(Data(repeating: 0, count: 500), on: .control))
        await eventually { socket.isBlocked }
        for id in 0..<80 {
            var control = Data(repeating: 1, count: 500)
            control[0] = UInt8(id)
            var media = Data(repeating: 2, count: 500)
            media[0] = UInt8(id)
            #expect(await leg.send(control, on: .control))
            #expect(await leg.send(media, on: .media))
        }
        socket.release()
        await eventually { socket.sentMessages.count == 100 }
        let messages = socket.sentMessages.dropFirst() // JOIN
        #expect(messages.count == 99) // one in flight, 49 newest per lane
        #expect(messages.first == Data([1] + Array(repeating: 0, count: 500)))
        let control = messages.dropFirst().filter { $0.first == 1 }
        let media = messages.dropFirst().filter { $0.first == 2 }
        #expect(control.count == 49)
        #expect(media.count == 49)
        #expect(control.first?[1] == 31)
        #expect(media.first?[1] == 31)
        #expect(control.last?[1] == 79)
        #expect(media.last?[1] == 79)
        let afterInFlight = Array(messages.dropFirst())
        #expect(zip(afterInFlight, afterInFlight.dropFirst()).allSatisfy { $0.first != $1.first })
        await leg.cancel()
    }

    @Test func oldStalledWriterCannotStrandNewLegsQueue() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        first.blockData = true
        first.holdBlockedOnClose = true
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent1 = first.sent.makeAsyncIterator(), sent2 = second.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent1.next()
        first.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await received.next()
        #expect(await leg.send(Data([0x11]), on: .control))
        await eventually { first.isBlocked }
        first.receive(.closed)
        await eventually { clock.pendingDueTimes.contains(0) }
        await clock.advance(by: 0)
        _ = await sent2.next()
        second.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await received.next()
        #expect(await leg.send(Data([0x22]), on: .media))
        #expect(await sent2.next() == Data([0x02, 0x22]))
        #expect(first.isBlocked)
        first.release()
        await leg.cancel()
    }

    @Test func rejoinWindowEndsThirtySecondsAfterFirstBreak() async throws {
        let first = RelayTestSocket()
        let pool = RelaySocketPool(first)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent = first.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        first.receive(.closed)
        await eventually { clock.pendingDueTimes.contains(30_000) }
        await clock.advance(by: 29_999)
        #expect(await pool.count >= 1)
        await clock.advance(by: 1)
        #expect(await received.next() == .reconnectTimedOut)
        #expect(await leg.send(Data([1]), on: .control) == false)
    }

    @Test func unknownEndIsTerminal() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent = first.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        first.receive(.binary(Data([0x83] + Array("futureCode".utf8))))
        #expect(await received.next() == .ended(.unknown))
        await clock.advance(by: 30_000)
        #expect(await pool.count == 1)
    }

    @Test func malformedRelayControlEndsLegBeforeAnyDatagram() async throws {
        let socket = RelayTestSocket()
        let pool = RelaySocketPool(socket)
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, onLifecycle: { await sink.record($0) })
        var sent = socket.sent.makeAsyncIterator()
        var received = sink.events.makeAsyncIterator()
        await leg.start()
        _ = await sent.next()
        socket.receive(.binary(Data([0x81, 0x01])))
        socket.receive(.binary(Data([0x02, 0x17])))
        #expect(await received.next() == .ended(.protocolError))
    }

    @Test func cancellationPreventsLateReconnect() async throws {
        let first = RelayTestSocket(), second = RelayTestSocket()
        let pool = RelaySocketPool(first, second)
        let clock = ManualLinkClock()
        let sink = RelayTestSink()
        let leg = RelayLeg(grant: try relayGrant(), factory: { try await pool.next($0) }, clock: clock, onLifecycle: { await sink.record($0) })
        var sent1 = first.sent.makeAsyncIterator()
        await leg.start()
        _ = await sent1.next()
        first.receive(.closed)
        await eventually { clock.pendingDueTimes.contains(0) }
        await leg.cancel()
        await clock.advance(by: 30_000)
        #expect(await pool.count == 1)
    }
}
