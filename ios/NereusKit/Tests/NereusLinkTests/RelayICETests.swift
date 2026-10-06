// NereusSDR for iOS: relay ICE loopback socket behavior
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin
import Testing
import LinkSessionTestSupport
@testable import NereusLink

private actor ICEFakePool {
    private var sockets: [ICEFakeSocket]
    init(_ sockets: ICEFakeSocket...) { self.sockets = sockets }
    func next() throws -> any RelayBinarySocket {
        guard !sockets.isEmpty else { throw RelayICEError.socketUnavailable }
        return sockets.removeFirst()
    }
}

private actor ICEWatermarkGate {
    nonisolated let entered: AsyncStream<Void>
    private let enteredContinuation: AsyncStream<Void>.Continuation
    private var armed = false
    private var waiters: [CheckedContinuation<Void, Never>] = []

    init() { (entered, enteredContinuation) = AsyncStream.makeStream(of: Void.self) }
    func arm() { armed = true }
    func pause() async {
        guard armed else { return }
        enteredContinuation.yield(())
        await withCheckedContinuation { waiters.append($0) }
    }
    func resume() {
        armed = false
        for waiter in waiters { waiter.resume() }
        waiters.removeAll()
    }
}

private final class ICEFakeSocket: RelayBinarySocket, @unchecked Sendable {
    let sent: AsyncStream<Data>
    private let sentContinuation: AsyncStream<Data>.Continuation
    private let received: AsyncStream<RelaySocketEvent>
    private let receiveContinuation: AsyncStream<RelaySocketEvent>.Continuation
    private let lock = NSLock()
    private var messages: [Data] = []
    var snapshot: [Data] { lock.withLock { messages } }

    init() {
        (sent, sentContinuation) = AsyncStream.makeStream(of: Data.self)
        (received, receiveContinuation) = AsyncStream.makeStream(of: RelaySocketEvent.self)
    }
    func send(_ message: Data) async throws {
        lock.withLock { messages.append(message) }
        sentContinuation.yield(message)
    }
    func nextEvent() async -> RelaySocketEvent? {
        var iterator = received.makeAsyncIterator()
        return await iterator.next()
    }
    func receive(_ event: RelaySocketEvent) { receiveContinuation.yield(event) }
    func close() async { receiveContinuation.finish() }
}

private final class ICEPeer: @unchecked Sendable {
    private let fd: Int32
    init() throws {
        fd = Darwin.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard fd >= 0 else { throw RelayICEError.socketUnavailable }
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr = in_addr(s_addr: UInt32(0x7f000001).bigEndian)
        let result = withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.bind(fd, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard result == 0 else { Darwin.close(fd); throw RelayICEError.socketUnavailable }
        // Every wait on this socket expects a datagram, and returns as soon
        // as it lands; 30 s only bounds a real failure. The old 2 s timed out
        // under a load average near 80 before the bridge's consumer ran.
        var timeout = timeval(tv_sec: 30, tv_usec: 0)
        _ = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, socklen_t(MemoryLayout<timeval>.size))
    }
    deinit { Darwin.close(fd) }
    func send(_ bytes: Data, to port: UInt16) {
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_port = port.bigEndian
        address.sin_addr = in_addr(s_addr: UInt32(0x7f000001).bigEndian)
        _ = withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { target in
                bytes.withUnsafeBytes {
                    Darwin.sendto(fd, $0.baseAddress, bytes.count, 0, target,
                                  socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
    }
    /// The first datagram before the deadline, or nil, retrying each recv
    /// that times out. Blocks its thread.
    func receive(within limit: Duration = testDeadline) -> Data? {
        let deadline = ContinuousClock.now + limit
        var bytes = [UInt8](repeating: 0, count: 1600)
        let capacity = bytes.count
        repeat {
            let count = bytes.withUnsafeMutableBytes { Darwin.recv(fd, $0.baseAddress, capacity, 0) }
            if count > 0 { return Data(bytes.prefix(count)) }
        } while ContinuousClock.now < deadline
        return nil
    }
    /// Receives on this peer's own thread. A blocking recv on Swift's
    /// cooperative pool holds a thread the bridge needs to forward the very
    /// datagram being waited for, which starves it on a busy machine.
    func receiveAsync() async -> Data? {
        await withCheckedContinuation { continuation in
            receiveQueue.async { continuation.resume(returning: self.receive()) }
        }
    }
    private let receiveQueue = DispatchQueue(label: "relay-ice-test-peer")
}

private func startedLeg() async throws -> (RelayLeg, ICEFakeSocket) {
    let socket = ICEFakeSocket()
    let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!, token: "opaque", expires: 1)
    let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
    let leg = RelayLeg(grant: grant, factory: { _ in socket }, onLifecycle: { event in
        continuation.yield(event)
    })
    var sent = socket.sent.makeAsyncIterator()
    var received = events.makeAsyncIterator()
    await leg.start()
    _ = await sent.next()
    socket.receive(.binary(Data([0x81, 0x01, 0x01])))
    _ = await received.next()
    return (leg, socket)
}

/// How long a test waits for something that should happen: generous, so a
/// busy machine cannot turn a slow delivery into a failure. A test that
/// passes never waits this long.
private let testDeadline: Duration = .seconds(30)

/// Polls until the predicate holds or the deadline passes, sleeping between
/// polls so the waiting test leaves the cooperative pool to the bridge.
private func eventually(_ predicate: @escaping () -> Bool) async -> Bool {
    let deadline = ContinuousClock.now + testDeadline
    while !predicate() && ContinuousClock.now < deadline { try? await Task.sleep(for: .milliseconds(5)) }
    return predicate()
}

struct RelayICETests {
    @Test func controlClaimOffersLowestPriorityCandidate() async throws {
        let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                   token: "opaque", expires: 1)
        let leg = RelayLeg(grant: grant, onLifecycle: { _ in })
        let bridge = try RelayICEBridge(leg: leg)
        let claim = try await bridge.claimControl()
        #expect(claim.candidate == "candidate:wsrelay1 1 UDP 1 127.0.0.1 \(claim.port) typ host")
        #expect(claim.port != 0)
        await bridge.close()
        await leg.cancel()
    }

    @Test func realUDPControlAndRawMediaRoundTrip() async throws {
        let (leg, web) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg)
        let control = try await bridge.claimControl()
        let media = try await bridge.claimMedia()
        let controlPeer = try ICEPeer(), mediaPeer = try ICEPeer()
        controlPeer.send(Data([0x11, 0x22]), to: control.port)
        mediaPeer.send(Data([0x33]), to: media.port)
        #expect(await eventually { web.snapshot.contains(Data([1, 0x11, 0x22])) })
        #expect(await eventually { web.snapshot.contains(Data([2, 0x33])) })
        let beforeOversize = web.snapshot.count
        controlPeer.send(Data(repeating: 0x44, count: 1501), to: control.port)
        try await Task.sleep(for: .milliseconds(100))
        #expect(web.snapshot.count == beforeOversize)
        web.receive(.binary(Data([1, 0xaa])))
        web.receive(.binary(Data([2, 0xbb])))
        #expect(await controlPeer.receiveAsync() == Data([0xaa]))
        #expect(await mediaPeer.receiveAsync() == Data([0xbb]))
        await bridge.close()
        await leg.cancel()
    }

    @Test func routedMediaCarriesUUIDAndRejectsWrongSenderAndOversize() async throws {
        let (leg, web) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg)
        let id = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        let route = try await bridge.claimMedia(connectionId: id)
        let owner = try ICEPeer(), outsider = try ICEPeer()
        owner.send(Data([0x55]), to: route.port)
        let routedSeen = await eventually { web.snapshot.count > 1 }
        #expect(routedSeen)
        #expect(web.snapshot.contains(Data([2, 0, 17, 34, 51, 68, 85, 102, 119,
                                            136, 153, 170, 187, 204, 221, 238, 255, 0x55])))
        let before = web.snapshot.count
        outsider.send(Data([0x66]), to: route.port)
        owner.send(Data(repeating: 0x77, count: 1485), to: route.port)
        try await Task.sleep(for: .milliseconds(100))
        #expect(web.snapshot.count == before)
        web.receive(.binary(Data([2, 0, 17, 34, 51, 68, 85, 102, 119,
                                  136, 153, 170, 187, 204, 221, 238, 255, 0xcc])))
        #expect(await owner.receiveAsync() == Data([0xcc]))
        await bridge.close()
        await leg.cancel()
    }

    @Test func routeLimitModeAndLateReleaseOwnership() async throws {
        let (leg, _) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg)
        let old = try await bridge.claimControl()
        await bridge.release(old)
        let current = try await bridge.claimControl()
        await bridge.release(old)
        await #expect(throws: RelayICEError.alreadyClaimed) { _ = try await bridge.claimControl() }
        let ids = (1...4).map { UUID(uuidString: "00000000-0000-0000-0000-00000000000\($0)")! }
        var routes: [RelayICEClaim] = []
        for id in ids.prefix(3) { routes.append(try await bridge.claimMedia(connectionId: id)) }
        await #expect(throws: RelayICEError.routeLimit) { _ = try await bridge.claimMedia(connectionId: ids[3]) }
        await #expect(throws: RelayICEError.mediaModeConflict) { _ = try await bridge.claimMedia() }
        let zero = UUID(uuid: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        await #expect(throws: RelayICEError.invalidConnectionId) {
            _ = try await bridge.claimMedia(connectionId: zero)
        }
        await bridge.release(routes[0])
        let reclaimed = try await bridge.claimMedia(connectionId: ids[0])
        await bridge.release(routes[0])
        #expect(reclaimed.connectionId == ids[0])
        await bridge.release(reclaimed)
        _ = try await bridge.claimMedia(connectionId: ids[3])
        #expect(current.port == old.port)
        await bridge.close()
        await leg.cancel()
    }

    @Test func closingSocketTwiceDoesNotCloseReusedDescriptor() throws {
        let old = try RelayUDPSocket()
        old.close()
        let current = try RelayUDPSocket()
        old.close()
        let peer = try ICEPeer()
        peer.send(Data([0x41]), to: current.port)
        var observed: Data?
        for _ in 0..<1000 {
            observed = current.read()?.bytes
            if observed != nil { break }
            usleep(1000)
        }
        #expect(observed == Data([0x41]))
        current.close()
    }

    @Test func routeReleasePurgesQueuedUUIDAndRejoinKeepsControlSocket() async throws {
        let first = ICEFakeSocket(), second = ICEFakeSocket()
        let pool = ICEFakePool(first, second)
        let clock = ManualLinkClock()
        let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                   token: "opaque", expires: 1)
        let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
        let leg = RelayLeg(grant: grant, factory: { _ in try await pool.next() }, clock: clock,
                           onLifecycle: { continuation.yield($0) })
        let bridge = try RelayICEBridge(leg: leg)
        let control = try await bridge.claimControl()
        let peer = try ICEPeer()
        var status = events.makeAsyncIterator()
        await leg.start()
        #expect(await eventually { first.snapshot.count == 1 })
        first.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        peer.send(Data([0x11]), to: control.port)
        #expect(await eventually { first.snapshot.contains(Data([1, 0x11])) })
        first.receive(.closed)
        _ = await eventually { clock.pendingDueTimes.contains(0) }
        await clock.advance(by: 0)
        #expect(await eventually { second.snapshot.count == 1 })

        let oldID = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        let liveID = UUID(uuidString: "10213243-5465-7687-98a9-bacbdcedfe0f")!
        let oldRoute = try await bridge.claimMedia(connectionId: oldID)
        let liveRoute = try await bridge.claimMedia(connectionId: liveID)
        let oldPayload = Data(try RelayFrame.routedMedia(oldID, Data([0x44])).dropFirst())
        let livePayload = Data(try RelayFrame.routedMedia(liveID, Data([0x55])).dropFirst())
        #expect(await leg.send(oldPayload, on: .media))
        #expect(await leg.send(livePayload, on: .media))
        await bridge.release(oldRoute)
        second.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        #expect(await eventually { second.snapshot.contains(Data([2]) + livePayload) })
        #expect(!second.snapshot.contains(Data([2]) + oldPayload))
        peer.send(Data([0x22]), to: control.port)
        #expect(await eventually { second.snapshot.contains(Data([1, 0x22])) })
        await bridge.release(liveRoute)
        await bridge.close()
        await leg.cancel()
    }

    @Test func releaseFullyDrainsSmallOldUDPPacketsBeforeReclaim() async throws {
        let queue = DispatchQueue(label: "relay-ice-paused-reads")
        queue.suspend()
        var paused = true
        defer { if paused { queue.resume() } }
        let (leg, web) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg, readQueue: queue,
                                        startInboundConsumers: true)
        let old = try await bridge.claimControl()
        let oldPeer = try ICEPeer(), newPeer = try ICEPeer()
        for _ in 0..<400 { oldPeer.send(Data([0x61]), to: old.port) }
        await bridge.release(old)
        let fresh = try await bridge.claimControl()
        queue.resume()
        paused = false
        newPeer.send(Data([0x72]), to: fresh.port)
        #expect(await eventually { web.snapshot.contains(Data([1, 0x72])) })
        #expect(!web.snapshot.contains(Data([1, 0x61])))
        await bridge.close()
        await leg.cancel()
    }

    @Test func repeatedRoutedUUIDReleaseAllowsReclaimWithoutGrowingHistory() async throws {
        let (leg, _) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg)
        let id = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        for _ in 0..<100 {
            let route = try await bridge.claimMedia(connectionId: id)
            await bridge.release(route)
        }
        let active = try await bridge.claimMedia(connectionId: id)
        #expect(active.connectionId == id)
        await bridge.close()
        await leg.cancel()
    }

    @Test func releasingRawOwnersPurgesTheirQueuedFramesAcrossRejoin() async throws {
        let first = ICEFakeSocket(), second = ICEFakeSocket()
        let pool = ICEFakePool(first, second)
        let clock = ManualLinkClock()
        let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                   token: "opaque", expires: 1)
        let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
        let leg = RelayLeg(grant: grant, factory: { _ in try await pool.next() }, clock: clock,
                           onLifecycle: { continuation.yield($0) })
        let bridge = try RelayICEBridge(leg: leg)
        let oldControl = try await bridge.claimControl()
        let oldMedia = try await bridge.claimMedia()
        var status = events.makeAsyncIterator()
        await leg.start()
        #expect(await eventually { first.snapshot.count == 1 })
        first.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        first.receive(.closed)
        _ = await eventually { clock.pendingDueTimes.contains(0) }
        await clock.advance(by: 0)
        #expect(await eventually { second.snapshot.count == 1 })
        #expect(await leg.send(Data([0x11]), on: .control))
        #expect(await leg.send(Data([0x22]), on: .media))
        await bridge.release(oldControl)
        await bridge.release(oldMedia)
        _ = try await bridge.claimControl()
        _ = try await bridge.claimMedia()
        #expect(await leg.send(Data([0x33]), on: .control))
        #expect(await leg.send(Data([0x44]), on: .media))
        second.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        #expect(await eventually { second.snapshot.contains(Data([1, 0x33])) &&
                                   second.snapshot.contains(Data([2, 0x44])) })
        #expect(!second.snapshot.contains(Data([1, 0x11])))
        #expect(!second.snapshot.contains(Data([2, 0x22])))
        await bridge.close()
        await leg.cancel()
    }

    @Test func bufferedInboundRawFramesStayWithTheirOriginalClaims() async throws {
        let web = ICEFakeSocket()
        let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                   token: "opaque", expires: 1)
        let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
        let leg = RelayLeg(grant: grant, factory: { _ in web },
                           onLifecycle: { continuation.yield($0) })
        let bridge = try RelayICEBridge(leg: leg,
                                        readQueue: .global(qos: .userInitiated),
                                        startInboundConsumers: false)
        let oldControl = try await bridge.claimControl()
        let oldMedia = try await bridge.claimMedia()
        let oldControlPeer = try ICEPeer(), oldMediaPeer = try ICEPeer()
        var status = events.makeAsyncIterator()
        await leg.start()
        #expect(await eventually { web.snapshot.count == 1 })
        web.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        oldControlPeer.send(Data([0x11]), to: oldControl.port)
        oldMediaPeer.send(Data([0x22]), to: oldMedia.port)
        #expect(await eventually { web.snapshot.contains(Data([1, 0x11])) &&
                                   web.snapshot.contains(Data([2, 0x22])) })
        web.receive(.binary(Data([1, 0xaa])))
        web.receive(.binary(Data([2, 0xcc])))
        web.receive(.binary(Data([0x82, 1])))
        _ = await status.next() // Both old inbound frames have reached the leg's streams.
        await bridge.release(oldControl)
        await bridge.release(oldMedia)
        let freshControl = try await bridge.claimControl()
        let freshMedia = try await bridge.claimMedia()
        let freshControlPeer = try ICEPeer(), freshMediaPeer = try ICEPeer()
        freshControlPeer.send(Data([0x33]), to: freshControl.port)
        freshMediaPeer.send(Data([0x44]), to: freshMedia.port)
        #expect(await eventually { web.snapshot.contains(Data([1, 0x33])) &&
                                   web.snapshot.contains(Data([2, 0x44])) })
        await bridge.startInboundConsumers()
        web.receive(.binary(Data([1, 0xbb])))
        web.receive(.binary(Data([2, 0xdd])))
        #expect(await freshControlPeer.receiveAsync() == Data([0xbb]))
        #expect(await freshMediaPeer.receiveAsync() == Data([0xdd]))
        await bridge.close()
        await leg.cancel()
    }

    @Test func retiredOwnerCannotEnqueueAfterNewOwnerClaimsLane() async throws {
        let (leg, web) = try await startedLeg()
        let old = UUID(), current = UUID()
        #expect(await leg.claimOutgoing(old, on: .control, route: nil))
        await leg.retireOutgoing(old, on: .control, route: nil)
        #expect(await leg.claimOutgoing(current, on: .control, route: nil))
        await leg.retireOutgoing(old, on: .control, route: nil)
        #expect(await leg.sendOwned(Data([0x11]), on: .control, route: nil, token: old) == false)
        #expect(await leg.sendOwned(Data([0x22]), on: .control, route: nil, token: current))
        #expect(await eventually { web.snapshot.contains(Data([1, 0x22])) })
        #expect(!web.snapshot.contains(Data([1, 0x11])))
        await leg.cancel()
    }

    @Test func bufferedRoutedFrameDoesNotReachReclaimedUUID() async throws {
        let web = ICEFakeSocket()
        let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                   token: "opaque", expires: 1)
        let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
        let leg = RelayLeg(grant: grant, factory: { _ in web },
                           onLifecycle: { continuation.yield($0) })
        let bridge = try RelayICEBridge(leg: leg,
                                        readQueue: .global(qos: .userInitiated),
                                        startInboundConsumers: false)
        let id = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        let old = try await bridge.claimMedia(connectionId: id)
        let oldPeer = try ICEPeer()
        var status = events.makeAsyncIterator()
        await leg.start()
        #expect(await eventually { web.snapshot.count == 1 })
        web.receive(.binary(Data([0x81, 0x01, 0x01])))
        _ = await status.next()
        oldPeer.send(Data([0x11]), to: old.port)
        #expect(await eventually { web.snapshot.count == 2 })
        web.receive(.binary(try RelayFrame.routedMedia(id, Data([0xaa]))))
        web.receive(.binary(Data([0x82, 1])))
        _ = await status.next()
        await bridge.release(old)
        let fresh = try await bridge.claimMedia(connectionId: id)
        let freshPeer = try ICEPeer()
        freshPeer.send(Data([0x22]), to: fresh.port)
        #expect(await eventually { web.snapshot.count == 3 })
        await bridge.startInboundConsumers()
        web.receive(.binary(try RelayFrame.routedMedia(id, Data([0xbb]))))
        #expect(await freshPeer.receiveAsync() == Data([0xbb]))
        await bridge.close()
        await leg.cancel()
    }

    @Test func pendingClaimReservesEveryLaneBeforeWatermarkHop() async throws {
        let id = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        for kind in 0..<3 {
            let gate = ICEWatermarkGate()
            let web = ICEFakeSocket()
            let grant = try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!,
                                       token: "opaque", expires: 1)
            let (events, continuation) = AsyncStream.makeStream(of: RelayLegEvent.self)
            let leg = RelayLeg(grant: grant, factory: { _ in web },
                               onLifecycle: { continuation.yield($0) })
            let bridge = try RelayICEBridge(leg: leg,
                                            readQueue: .global(qos: .userInitiated),
                                            startInboundConsumers: false,
                                            afterWatermark: { _, _ in await gate.pause() })
            func claim() async throws -> RelayICEClaim {
                switch kind {
                case 0: try await bridge.claimControl()
                case 1: try await bridge.claimMedia()
                default: try await bridge.claimMedia(connectionId: id)
                }
            }
            let old = try await claim()
            let oldPeer = try ICEPeer()
            var status = events.makeAsyncIterator()
            await leg.start()
            #expect(await eventually { web.snapshot.count == 1 })
            web.receive(.binary(Data([0x81, 0x01, 0x01])))
            _ = await status.next()
            oldPeer.send(Data([0x11]), to: old.port)
            #expect(await eventually { web.snapshot.count == 2 })
            let oldFrame = kind == 2 ? try RelayFrame.routedMedia(id, Data([0xaa]))
                                     : Data([kind == 0 ? 1 : 2, 0xaa])
            web.receive(.binary(oldFrame))
            web.receive(.binary(Data([0x82, 1])))
            _ = await status.next()
            await bridge.release(old)

            await gate.arm()
            let pending = Task { try await claim() }
            var entered = gate.entered.makeAsyncIterator()
            _ = await entered.next()
            await #expect(throws: RelayICEError.alreadyClaimed) { _ = try await claim() }
            await gate.resume()
            let fresh = try await pending.value
            let freshPeer = try ICEPeer()
            freshPeer.send(Data([0x22]), to: fresh.port)
            #expect(await eventually { web.snapshot.count == 3 })
            await bridge.startInboundConsumers()
            let freshFrame = kind == 2 ? try RelayFrame.routedMedia(id, Data([0xbb]))
                                       : Data([kind == 0 ? 1 : 2, 0xbb])
            web.receive(.binary(freshFrame))
            #expect(await freshPeer.receiveAsync() == Data([0xbb]))
            await bridge.close()
            await leg.cancel()
        }
    }

    @Test func pendingRoutedClaimsCountTowardThreeSocketLimit() async throws {
        let gate = ICEWatermarkGate()
        let (leg, _) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg,
                                        readQueue: .global(qos: .userInitiated),
                                        startInboundConsumers: true,
                                        afterWatermark: { _, _ in await gate.pause() })
        await gate.arm()
        let ids = (1...4).map { UUID(uuidString: "00000000-0000-0000-0000-00000000000\($0)")! }
        let pending = ids.prefix(3).map { id in
            Task { try await bridge.claimMedia(connectionId: id) }
        }
        var entered = gate.entered.makeAsyncIterator()
        for _ in 0..<3 { _ = await entered.next() }
        await #expect(throws: RelayICEError.routeLimit) {
            _ = try await bridge.claimMedia(connectionId: ids[3])
        }
        await gate.resume()
        for task in pending {
            let route = try await task.value
            await bridge.release(route)
        }
        await bridge.close()
        await leg.cancel()
    }

    @Test func cancelledPendingClaimReleasesItsReservation() async throws {
        let gate = ICEWatermarkGate()
        let (leg, _) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg,
                                        readQueue: .global(qos: .userInitiated),
                                        startInboundConsumers: true,
                                        afterWatermark: { _, _ in await gate.pause() })
        await gate.arm()
        let pending = Task { try await bridge.claimControl() }
        var entered = gate.entered.makeAsyncIterator()
        _ = await entered.next()
        pending.cancel()
        await gate.resume()
        await #expect(throws: CancellationError.self) { _ = try await pending.value }
        let replacement = try await bridge.claimControl()
        #expect(replacement.port != 0)
        await bridge.close()
        await leg.cancel()
    }

    @Test func closeInvalidatesHeldRoutedReservation() async throws {
        let gate = ICEWatermarkGate()
        let (leg, _) = try await startedLeg()
        let bridge = try RelayICEBridge(leg: leg,
                                        readQueue: .global(qos: .userInitiated),
                                        startInboundConsumers: true,
                                        afterWatermark: { _, _ in await gate.pause() })
        let id = UUID(uuidString: "00112233-4455-6677-8899-aabbccddeeff")!
        await gate.arm()
        let pending = Task { try await bridge.claimMedia(connectionId: id) }
        var entered = gate.entered.makeAsyncIterator()
        _ = await entered.next()
        await bridge.close()
        await gate.resume()
        await #expect(throws: RelayICEError.socketUnavailable) { _ = try await pending.value }
        await leg.cancel()
    }
}
