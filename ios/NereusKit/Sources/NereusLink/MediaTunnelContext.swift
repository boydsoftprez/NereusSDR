// NereusSDR for iOS: shared loopback carrier for encrypted media over direct WSS
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
//
// Ported from NereusSDR-original src/core/session/MediaTunnel.cpp at
// 0b41e58113616b404f8ab0e7e257d1a744e4d3ec (2026-09-27).
// Modification history (NereusSDR): 2026-09-27, Swift implementation by
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Darwin
import Dispatch
import Foundation

public enum MediaTunnelError: Error, Sendable {
    case unavailable, alreadyClaimed, routeLimit
}

public struct MediaTunnelClaim: Sendable {
    public let connectionId: UUID
    public let candidate: String
    fileprivate let token: UUID
    public let port: UInt16
    fileprivate let ownership: BinaryMediaOwnership
}

/// A synchronous permit closes the gap between the context actor's liveness
/// check and installation on the session actor.
final class MediaTunnelRegistrationPermit: @unchecked Sendable {
    private let lock = NSLock()
    private var live = true
    var isLive: Bool { lock.withLock { live } }
    func retire() { lock.withLock { live = false } }
}

/// One context per logical StationSession. Peers from current, replacement,
/// and draining media share its three UUID routes. The context retains the
/// session; the session's callback captures this context weakly.
public actor MediaTunnelContext {
    private struct Route {
        let claim: MediaTunnelClaim
        let socket: RelayUDPSocket
        var sender: sockaddr_in?
    }

    private let session: StationSession
    private let owner = UUID()
    private let registration = MediaTunnelRegistrationPermit()
    private let queue = DispatchQueue(label: "NereusSDR.media.tunnel.udp")
    private let inbound = MediaTunnelInbound()
    private var routes: [UUID: Route] = [:]
    private var pendingRoutes: [UUID: UUID] = [:]
    private var retiringRoutes: Set<UUID> = []
    private let beforeRegistration: (@Sendable () async -> Void)?
    private struct Outbound {
        let frame: Data
        let connectionId: UUID
        let ownership: BinaryMediaOwnership
    }
    private var outbound: [Outbound] = []
    private var outboundBytes = 0
    private var flushing = false
    private var closed = false

    public init(session: StationSession) {
        self.session = session
        self.beforeRegistration = nil
    }

    init(session: StationSession, beforeRegistration: (@Sendable () async -> Void)?) {
        self.session = session
        self.beforeRegistration = beforeRegistration
    }

    deinit {
        registration.retire()
        let session = session
        let owner = owner
        Task { await session.setMediaTunnelReceiver(owner: owner, receiver: nil) }
        for route in routes.values { route.socket.close() }
    }

    public func claimMedia(connectionId: UUID) async throws -> MediaTunnelClaim {
        guard !closed else { throw MediaTunnelError.unavailable }
        guard routes[connectionId] == nil, pendingRoutes[connectionId] == nil,
              !retiringRoutes.contains(connectionId) else { throw MediaTunnelError.alreadyClaimed }
        guard routes.count + pendingRoutes.count + retiringRoutes.count < 3 else {
            throw MediaTunnelError.routeLimit
        }
        let token = UUID()
        pendingRoutes[connectionId] = token
        inbound.setHandler { [weak self] frame, ownership in
            await self?.deliver(frame, ownership: ownership)
        }
        await beforeRegistration?()
        guard !closed, !Task.isCancelled, pendingRoutes[connectionId] == token else {
            if pendingRoutes[connectionId] == token { pendingRoutes.removeValue(forKey: connectionId) }
            throw Task.isCancelled ? CancellationError() : MediaTunnelError.unavailable
        }
        let ingress = inbound
        await session.setMediaTunnelReceiver(owner: owner, registration: registration) {
            frame in ingress.offer(frame)
        }
        guard !closed, !Task.isCancelled, pendingRoutes[connectionId] == token else {
            if pendingRoutes[connectionId] == token { pendingRoutes.removeValue(forKey: connectionId) }
            if closed { await session.setMediaTunnelReceiver(owner: owner, receiver: nil) }
            throw Task.isCancelled ? CancellationError() : MediaTunnelError.unavailable
        }
        let socket: RelayUDPSocket
        do { socket = try RelayUDPSocket() }
        catch {
            pendingRoutes.removeValue(forKey: connectionId)
            throw error
        }
        let ownership = BinaryMediaOwnership()
        let claim = MediaTunnelClaim(connectionId: connectionId,
                                     candidate: "candidate:wsrelay2 1 UDP 1 127.0.0.1 \(socket.port) typ host",
                                     token: token, port: socket.port, ownership: ownership)
        pendingRoutes.removeValue(forKey: connectionId)
        routes[connectionId] = Route(claim: claim, socket: socket)
        inbound.register(connectionId, ownership: ownership)
        socket.activate(on: queue) { [weak self, weak socket] in
            guard let self, let socket else { return }
            Task { await self.drain(socket, claim: claim) }
        }
        return claim
    }

    public func releaseMedia(_ claim: MediaTunnelClaim) async {
        guard let route = routes[claim.connectionId], route.claim.token == claim.token else { return }
        routes.removeValue(forKey: claim.connectionId)
        retiringRoutes.insert(claim.connectionId)
        claim.ownership.retire()
        inbound.unregister(claim.connectionId, ownership: claim.ownership)
        route.socket.close()
        outbound.removeAll { $0.ownership === claim.ownership }
        outboundBytes = outbound.reduce(0) { $0 + $1.frame.count }
        await session.discardMediaTunnel(ownership: claim.ownership)
        retiringRoutes.remove(claim.connectionId)
    }

    /// End the logical session's carrier and unregister its callback. Claimed
    /// sockets remain owned by their peers until each native ICE barrier ends.
    public func close() async {
        guard !closed else { return }
        closed = true
        registration.retire()
        pendingRoutes.removeAll()
        outbound.removeAll()
        outboundBytes = 0
        await session.setMediaTunnelReceiver(owner: owner, receiver: nil)
    }

    private func drain(_ socket: RelayUDPSocket, claim: MediaTunnelClaim) async {
        defer { socket.finishedDrain() }
        guard !closed else { return }
        for _ in 0..<128 {
            guard let packet = socket.read() else { break }
            guard !closed, var route = routes[claim.connectionId], route.claim.token == claim.token,
                  packet.sender.sin_family == AF_INET,
                  UInt32(bigEndian: packet.sender.sin_addr.s_addr) >> 24 == 127,
                  !packet.bytes.isEmpty, packet.bytes.count <= 1484 else { continue }
            if let known = route.sender {
                guard known.sin_addr.s_addr == packet.sender.sin_addr.s_addr,
                      known.sin_port == packet.sender.sin_port else { continue }
            } else {
                route.sender = packet.sender
                routes[claim.connectionId] = route
            }
            var frame = Data([2])
            frame.append(Self.uuidBytes(claim.connectionId))
            frame.append(packet.bytes)
            outbound.append(Outbound(frame: frame, connectionId: claim.connectionId,
                                     ownership: claim.ownership))
            outboundBytes += frame.count
            while outbound.count > 64 || outboundBytes > 24_576 {
                outboundBytes -= outbound.removeFirst().frame.count
            }
        }
        await flush()
    }

    private func flush() async {
        guard !flushing else { return }
        guard !closed else { outbound.removeAll(); outboundBytes = 0; return }
        flushing = true
        while !outbound.isEmpty {
            let item = outbound.removeFirst()
            outboundBytes -= item.frame.count
            _ = await session.sendMediaTunnel(item.frame, ownership: item.ownership)
        }
        flushing = false
    }

    private func deliver(_ frame: Data, ownership: BinaryMediaOwnership) {
        guard !closed, (18...1501).contains(frame.count), frame.first == 2 else { return }
        let bytes = Array(frame[1..<17])
        let id = UUID(uuid: uuid_t(bytes[0], bytes[1], bytes[2], bytes[3],
                                   bytes[4], bytes[5], bytes[6], bytes[7],
                                   bytes[8], bytes[9], bytes[10], bytes[11],
                                   bytes[12], bytes[13], bytes[14], bytes[15]))
        guard let route = routes[id], route.claim.ownership === ownership,
              let sender = route.sender else { return }
        route.socket.send(Data(frame.dropFirst(17)), to: sender)
    }

    private static func uuidBytes(_ id: UUID) -> Data {
        var uuid = id.uuid
        return withUnsafeBytes(of: &uuid) { Data($0) }
    }
}

/// Bounded, single-drain ingress from the session's synchronous callback.
final class MediaTunnelInbound: @unchecked Sendable {
    private struct Packet {
        let frame: Data
        let ownership: BinaryMediaOwnership
    }
    private let lock = NSLock()
    private var queue: [Packet] = []
    private var owners: [UUID: BinaryMediaOwnership] = [:]
    private var bytes = 0
    private var draining = false
    private var handler: (@Sendable (Data, BinaryMediaOwnership) async -> Void)?
    var queuedCount: Int { lock.withLock { queue.count } }

    func setHandler(_ handler: @escaping @Sendable (Data, BinaryMediaOwnership) async -> Void) {
        lock.withLock { self.handler = handler }
    }

    func register(_ id: UUID, ownership: BinaryMediaOwnership) {
        lock.withLock { owners[id] = ownership }
    }

    func unregister(_ id: UUID, ownership: BinaryMediaOwnership) {
        lock.withLock {
            if owners[id] === ownership { owners.removeValue(forKey: id) }
            queue.removeAll { $0.ownership === ownership }
            bytes = queue.reduce(0) { $0 + $1.frame.count }
        }
    }

    func offer(_ frame: Data) {
        guard (18...1501).contains(frame.count), frame.first == 2 else { return }
        let uuid = Array(frame[1..<17])
        let id = UUID(uuid: uuid_t(uuid[0], uuid[1], uuid[2], uuid[3],
                                   uuid[4], uuid[5], uuid[6], uuid[7],
                                   uuid[8], uuid[9], uuid[10], uuid[11],
                                   uuid[12], uuid[13], uuid[14], uuid[15]))
        let start = lock.withLock { () -> Bool in
            guard let ownership = owners[id] else { return false }
            queue.append(Packet(frame: frame, ownership: ownership))
            bytes += frame.count
            while queue.count > 64 || bytes > 24_576 { bytes -= queue.removeFirst().frame.count }
            guard !draining else { return false }
            draining = true
            return true
        }
        if start { Task { await drain() } }
    }

    private func drain() async {
        while true {
            let next = lock.withLock { () -> (Packet, (@Sendable (Data, BinaryMediaOwnership) async -> Void)?)? in
                guard !queue.isEmpty else { draining = false; return nil }
                let packet = queue.removeFirst()
                bytes -= packet.frame.count
                return (packet, handler)
            }
            guard let (packet, handler) = next else { return }
            await handler?(packet.frame, packet.ownership)
        }
    }
}
