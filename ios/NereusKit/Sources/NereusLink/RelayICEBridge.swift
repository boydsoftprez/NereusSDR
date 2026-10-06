// NereusSDR for iOS: loopback ICE sockets for the web relay leg
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
//
// Ported from NereusSDR-original src/core/session/RelayLeg.cpp at
// 0b41e58113616b404f8ab0e7e257d1a744e4d3ec (2026-09-27).
// The candidate, lane claims, UUID framing, and first-sender rule follow
// that implementation. Modification history (NereusSDR): 2026-09-27,
// Swift implementation by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Darwin
import Dispatch
import Foundation

public enum RelayICEError: Error, Sendable, Equatable {
    case socketUnavailable, alreadyClaimed, routeLimit, mediaModeConflict, invalidConnectionId
}

/// An ICE peer adds `candidate` to its existing ICE agent. Retain the claim
/// until that agent is torn down, then pass it to `release(_:)`.
public struct RelayICEClaim: Sendable, Equatable {
    public let candidate: String
    public let port: UInt16
    public let lane: RelayLane
    public let connectionId: UUID?
    fileprivate let token: UUID

    fileprivate init(lane: RelayLane, port: UInt16, connectionId: UUID? = nil,
                     token: UUID = UUID()) {
        self.lane = lane
        self.port = port
        self.connectionId = connectionId
        self.token = token
        self.candidate = "candidate:wsrelay\(lane.rawValue) 1 UDP 1 127.0.0.1 \(port) typ host"
    }
}

struct RelayUDPPacket: Sendable {
    let bytes: Data
    let sender: sockaddr_in
}

/// One nonblocking IPv4 socket. The read source schedules at most one actor
/// drain at a time; the kernel receive buffer is bounded and no per-packet
/// task is created. Close invalidates callbacks before the descriptor is reused.
final class RelayUDPSocket: @unchecked Sendable {
    let port: UInt16
    private let fd: Int32
    private let lock = NSLock()
    private var closed = false
    private var scheduled = false
    private var source: DispatchSourceRead?

    init() throws {
        let descriptor = Darwin.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard descriptor >= 0 else { throw RelayICEError.socketUnavailable }
        fd = descriptor
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_port = 0
        address.sin_addr = in_addr(s_addr: UInt32(0x7f000001).bigEndian)
        let bound = withUnsafePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.bind(descriptor, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard bound == 0 else {
            Darwin.close(descriptor)
            throw RelayICEError.socketUnavailable
        }
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        let named = withUnsafeMutablePointer(to: &address) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.getsockname(descriptor, $0, &length)
            }
        }
        guard named == 0 else {
            Darwin.close(descriptor)
            throw RelayICEError.socketUnavailable
        }
        port = UInt16(bigEndian: address.sin_port)
        _ = fcntl(descriptor, F_SETFL, fcntl(descriptor, F_GETFL) | O_NONBLOCK)
        var receiveBytes: Int32 = 64 * 1501
        _ = setsockopt(descriptor, SOL_SOCKET, SO_RCVBUF, &receiveBytes,
                       socklen_t(MemoryLayout<Int32>.size))
    }

    func activate(on queue: DispatchQueue, _ schedule: @escaping @Sendable () -> Void) {
        let reader = DispatchSource.makeReadSource(fileDescriptor: fd, queue: queue)
        reader.setEventHandler { [weak self] in
            guard let self else { return }
            let shouldSchedule = self.lock.withLock { () -> Bool in
                guard !self.closed, !self.scheduled else { return false }
                self.scheduled = true
                return true
            }
            if shouldSchedule { schedule() }
        }
        lock.withLock { source = reader }
        reader.resume()
    }

    func read() -> RelayUDPPacket? {
        guard !lock.withLock({ closed }) else { return nil }
        var sender = sockaddr_in()
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        var buffer = [UInt8](repeating: 0, count: RelayFrame.maximumDatagramBytes + 1)
        let capacity = buffer.count
        let count = withUnsafeMutablePointer(to: &sender) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { address in
                buffer.withUnsafeMutableBytes {
                    Darwin.recvfrom(fd, $0.baseAddress, capacity, 0, address, &length)
                }
            }
        }
        guard count >= 0 else { return nil }
        return RelayUDPPacket(bytes: Data(buffer.prefix(count)), sender: sender)
    }

    func finishedDrain() { lock.withLock { scheduled = false } }

    func discardPendingBatch() -> Bool {
        // A small datagram can fit hundreds of times in SO_RCVBUF. Return
        // only after recvfrom reports EAGAIN; callers keep ownership closed
        // between bounded batches.
        for _ in 0..<128 {
            guard read() != nil else { return true }
        }
        return false
    }

    func send(_ bytes: Data, to peer: sockaddr_in) {
        guard !lock.withLock({ closed }) else { return }
        var destination = peer
        _ = withUnsafePointer(to: &destination) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { address in
                bytes.withUnsafeBytes {
                    Darwin.sendto(fd, $0.baseAddress, bytes.count, 0, address,
                                  socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
    }

    func close() {
        let (shouldClose, old) = lock.withLock { () -> (Bool, DispatchSourceRead?) in
            guard !closed else { return (false, nil) }
            closed = true
            let old = source
            source = nil
            return (true, old)
        }
        guard shouldClose else { return }
        old?.cancel()
        Darwin.close(fd)
    }

    deinit { close() }
}

/// Routes already encrypted ICE, DTLS and SRTP datagrams between one
/// `RelayLeg` and existing ICE agents. WSS rejoins leave its sockets and
/// candidate ports intact. A media leg selects raw or UUID routing once.
public actor RelayICEBridge {
    private struct Owner {
        let claim: RelayICEClaim
        let socket: RelayUDPSocket
        let minimumSequence: UInt64
        var peer: sockaddr_in?
    }

    private let leg: RelayLeg
    private let controlSocket: RelayUDPSocket
    private let rawMediaSocket: RelayUDPSocket
    private let readQueue: DispatchQueue
    private let autoStartInboundConsumers: Bool
    private let afterWatermark: (@Sendable (RelayLane, UUID?) async -> Void)?
    private var control: Owner?
    private var rawMedia: Owner?
    private var routes: [UUID: Owner] = [:]
    private var pendingControl: UUID?
    private var pendingRawMedia: UUID?
    private var pendingRoutes: [UUID: UUID] = [:]
    private var releasingControl = false
    private var releasingRawMedia = false
    private var releasingRoutes: Set<UUID> = []
    private var mediaMode: Bool? // false: raw, true: UUID routed
    private var listening = false
    private var inboundStarted = false
    private var closed = false
    private var streamTasks: [Task<Void, Never>] = []

    public init(leg: RelayLeg) throws {
        try self.init(leg: leg, readQueue: .global(qos: .userInitiated),
                      startInboundConsumers: true, afterWatermark: nil)
    }

    init(leg: RelayLeg, readQueue: DispatchQueue,
         startInboundConsumers: Bool,
         afterWatermark: (@Sendable (RelayLane, UUID?) async -> Void)? = nil) throws {
        let control = try RelayUDPSocket()
        do { rawMediaSocket = try RelayUDPSocket() }
        catch { control.close(); throw error }
        controlSocket = control
        self.leg = leg
        self.readQueue = readQueue
        self.autoStartInboundConsumers = startInboundConsumers
        self.afterWatermark = afterWatermark
    }

    deinit {
        streamTasks.forEach { $0.cancel() }
        for (_, owner) in routes { owner.socket.close() }
        controlSocket.close()
        rawMediaSocket.close()
    }

    public func claimControl() async throws -> RelayICEClaim {
        guard !closed else { throw RelayICEError.socketUnavailable }
        guard control == nil, pendingControl == nil, !releasingControl else {
            throw RelayICEError.alreadyClaimed
        }
        let token = UUID()
        pendingControl = token
        let sequence = await leg.inboundSequence(on: .control)
        await afterWatermark?(.control, nil)
        guard !closed, !Task.isCancelled, pendingControl == token else {
            if pendingControl == token { pendingControl = nil }
            throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
        }
        pendingControl = nil
        beginListening()
        let claim = RelayICEClaim(lane: .control, port: controlSocket.port, token: token)
        control = Owner(claim: claim, socket: controlSocket, minimumSequence: sequence)
        let registered = await leg.claimOutgoing(claim.token, on: .control, route: nil)
        guard registered, !closed, !Task.isCancelled, control?.claim.token == claim.token else {
            if control?.claim.token == claim.token {
                control = nil
                releasingControl = registered
            }
            if registered { await leg.retireOutgoing(claim.token, on: .control, route: nil) }
            releasingControl = false
            throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
        }
        return claim
    }

    public func claimMedia(connectionId: UUID? = nil) async throws -> RelayICEClaim {
        guard !closed else { throw RelayICEError.socketUnavailable }
        let routed = connectionId != nil
        guard mediaMode == nil || mediaMode == routed else { throw RelayICEError.mediaModeConflict }
        if let connectionId {
            guard connectionId != UUID(uuid: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)) else {
                throw RelayICEError.invalidConnectionId
            }
            guard pendingRawMedia == nil else { throw RelayICEError.mediaModeConflict }
            guard routes[connectionId] == nil, pendingRoutes[connectionId] == nil,
                  !releasingRoutes.contains(connectionId) else {
                throw RelayICEError.alreadyClaimed
            }
            guard routes.count + pendingRoutes.count + releasingRoutes.count < 3 else {
                throw RelayICEError.routeLimit
            }
            let token = UUID()
            pendingRoutes[connectionId] = token
            let sequence = await leg.inboundSequence(on: .media)
            await afterWatermark?(.media, connectionId)
            guard !closed, !Task.isCancelled, pendingRoutes[connectionId] == token else {
                if pendingRoutes[connectionId] == token { pendingRoutes.removeValue(forKey: connectionId) }
                throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
            }
            let socket: RelayUDPSocket
            do { socket = try RelayUDPSocket() }
            catch {
                pendingRoutes.removeValue(forKey: connectionId)
                throw error
            }
            pendingRoutes.removeValue(forKey: connectionId)
            beginListening()
            let claim = RelayICEClaim(lane: .media, port: socket.port,
                                      connectionId: connectionId, token: token)
            routes[connectionId] = Owner(claim: claim, socket: socket, minimumSequence: sequence)
            socket.activate(on: readQueue) { [weak self, weak socket] in
                guard let self, let socket else { return }
                Task { await self.drain(socket, claim: claim) }
            }
            mediaMode = true
            let registered = await leg.claimOutgoing(claim.token, on: .media, route: connectionId)
            guard registered, !closed, !Task.isCancelled,
                  routes[connectionId]?.claim.token == claim.token else {
                if routes[connectionId]?.claim.token == claim.token {
                    routes.removeValue(forKey: connectionId)
                    if registered { releasingRoutes.insert(connectionId) }
                }
                socket.close()
                if registered { await leg.retireOutgoing(claim.token, on: .media, route: connectionId) }
                releasingRoutes.remove(connectionId)
                throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
            }
            return claim
        }
        guard pendingRoutes.isEmpty else { throw RelayICEError.mediaModeConflict }
        guard rawMedia == nil, pendingRawMedia == nil, !releasingRawMedia else {
            throw RelayICEError.alreadyClaimed
        }
        let token = UUID()
        pendingRawMedia = token
        let sequence = await leg.inboundSequence(on: .media)
        await afterWatermark?(.media, nil)
        guard !closed, !Task.isCancelled, pendingRawMedia == token else {
            if pendingRawMedia == token { pendingRawMedia = nil }
            throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
        }
        pendingRawMedia = nil
        beginListening()
        let claim = RelayICEClaim(lane: .media, port: rawMediaSocket.port, token: token)
        rawMedia = Owner(claim: claim, socket: rawMediaSocket, minimumSequence: sequence)
        mediaMode = false
        let registered = await leg.claimOutgoing(claim.token, on: .media, route: nil)
        guard registered, !closed, !Task.isCancelled, rawMedia?.claim.token == claim.token else {
            if rawMedia?.claim.token == claim.token {
                rawMedia = nil
                releasingRawMedia = registered
            }
            if registered { await leg.retireOutgoing(claim.token, on: .media, route: nil) }
            releasingRawMedia = false
            throw Task.isCancelled ? CancellationError() : RelayICEError.socketUnavailable
        }
        return claim
    }

    public func release(_ claim: RelayICEClaim) async {
        switch claim.lane {
        case .control:
            guard control?.claim.token == claim.token else { return }
            control = nil
            releasingControl = true
            await leg.retireOutgoing(claim.token, on: .control, route: nil)
            await discardPending(from: controlSocket)
            releasingControl = false
        case .media:
            if let id = claim.connectionId {
                guard let owner = routes[id], owner.claim.token == claim.token else { return }
                routes.removeValue(forKey: id)
                releasingRoutes.insert(id)
                owner.socket.close()
                await leg.retireOutgoing(claim.token, on: .media, route: id)
                releasingRoutes.remove(id)
            } else {
                guard rawMedia?.claim.token == claim.token else { return }
                rawMedia = nil
                releasingRawMedia = true
                await leg.retireOutgoing(claim.token, on: .media, route: nil)
                await discardPending(from: rawMediaSocket)
                releasingRawMedia = false
            }
        }
    }

    private func discardPending(from socket: RelayUDPSocket) async {
        while !socket.discardPendingBatch() {
            await Task.yield()
        }
    }

    public func close() async {
        guard !closed else { return }
        closed = true
        let oldControl = control
        let oldRawMedia = rawMedia
        control = nil
        rawMedia = nil
        pendingControl = nil
        pendingRawMedia = nil
        pendingRoutes.removeAll()
        let released = routes
        for (_, owner) in released { owner.socket.close() }
        routes.removeAll()
        controlSocket.close()
        rawMediaSocket.close()
        streamTasks.forEach { $0.cancel() }
        streamTasks.removeAll()
        if let oldControl { await leg.retireOutgoing(oldControl.claim.token, on: .control, route: nil) }
        if let oldRawMedia { await leg.retireOutgoing(oldRawMedia.claim.token, on: .media, route: nil) }
        await leg.discardQueuedLane(.control)
        if mediaMode == false { await leg.discardQueuedLane(.media) }
        for (id, owner) in released {
            await leg.retireOutgoing(owner.claim.token, on: .media, route: id)
        }
    }

    private func beginListening() {
        guard !listening else { return }
        listening = true
        controlSocket.activate(on: readQueue) { [weak self] in
            guard let self else { return }
            Task { await self.drain(self.controlSocket, claim: nil) }
        }
        rawMediaSocket.activate(on: readQueue) { [weak self] in
            guard let self else { return }
            Task { await self.drain(self.rawMediaSocket, claim: nil) }
        }
        if autoStartInboundConsumers { startInboundConsumers() }
    }

    func startInboundConsumers() {
        guard !inboundStarted, !closed else { return }
        inboundStarted = true
        streamTasks.append(Task { [weak self, leg] in
            for await packet in leg.controlPackets { await self?.deliverControl(packet) }
        })
        streamTasks.append(Task { [weak self, leg] in
            for await packet in leg.mediaPackets { await self?.deliverMedia(packet) }
        })
    }

    private func drain(_ socket: RelayUDPSocket, claim: RelayICEClaim?) async {
        defer { socket.finishedDrain() }
        guard !closed else { return }
        for _ in 0..<128 {
            guard let packet = socket.read() else { return }
            guard packet.sender.sin_family == AF_INET,
                  UInt32(bigEndian: packet.sender.sin_addr.s_addr) >> 24 == 127,
                  !packet.bytes.isEmpty else { continue }
            if let claim {
                guard let id = claim.connectionId, var owner = routes[id],
                      owner.claim.token == claim.token else { continue }
                guard packet.bytes.count <= 1484, accepts(packet.sender, peer: &owner.peer) else { continue }
                routes[id] = owner
                let routed = try? RelayFrame.routedMedia(id, packet.bytes)
                if let routed {
                    _ = await leg.sendOwned(Data(routed.dropFirst()), on: .media,
                                            route: id, token: claim.token)
                }
            } else if socket === controlSocket {
                guard var owner = control, packet.bytes.count <= 1500,
                      accepts(packet.sender, peer: &owner.peer) else { continue }
                control = owner
                _ = await leg.sendOwned(packet.bytes, on: .control, route: nil,
                                        token: owner.claim.token)
            } else {
                guard var owner = rawMedia, packet.bytes.count <= 1500,
                      accepts(packet.sender, peer: &owner.peer) else { continue }
                rawMedia = owner
                _ = await leg.sendOwned(packet.bytes, on: .media, route: nil,
                                        token: owner.claim.token)
            }
        }
    }

    private func accepts(_ sender: sockaddr_in, peer: inout sockaddr_in?) -> Bool {
        if let known = peer {
            return known.sin_addr.s_addr == sender.sin_addr.s_addr && known.sin_port == sender.sin_port
        }
        peer = sender
        return true
    }

    private func deliverControl(_ packet: RelayInboundDatagram) async {
        guard !closed, let before = control, packet.sequence > before.minimumSequence,
              !packet.bytes.isEmpty, packet.bytes.count <= 1500 else { return }
        let terminal = await leg.isTerminal
        guard !terminal, !closed, let current = control,
              current.claim.token == before.claim.token,
              packet.sequence > current.minimumSequence, let peer = current.peer else { return }
        current.socket.send(packet.bytes, to: peer)
    }

    private func deliverMedia(_ packet: RelayInboundDatagram) async {
        guard !closed, !packet.bytes.isEmpty else { return }
        if mediaMode == true {
            guard let routed = try? RelayFrame.unrouteMedia(packet.bytes),
                  let before = routes[routed.connectionId],
                  packet.sequence > before.minimumSequence else { return }
            let terminal = await leg.isTerminal
            guard !terminal, !closed, let current = routes[routed.connectionId],
                  current.claim.token == before.claim.token,
                  packet.sequence > current.minimumSequence, let peer = current.peer else { return }
            current.socket.send(routed.datagram, to: peer)
        } else {
            guard let before = rawMedia, packet.bytes.count <= 1500,
                  packet.sequence > before.minimumSequence else { return }
            let terminal = await leg.isTerminal
            guard !terminal, !closed, let current = rawMedia,
                  current.claim.token == before.claim.token,
                  packet.sequence > current.minimumSequence, let peer = current.peer else { return }
            current.socket.send(packet.bytes, to: peer)
        }
    }
}
