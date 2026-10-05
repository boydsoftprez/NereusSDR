// NereusSDR for iOS: one grant-bound web relay leg for encrypted datagrams
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

public enum RelaySocketEvent: Sendable {
    case binary(Data)
    case text
    case closed
}

/// A binary WebSocket. Implementations must deliver complete messages in order.
public protocol RelayBinarySocket: Sendable {
    func nextEvent() async -> RelaySocketEvent?
    func send(_ message: Data) async throws
    func close() async
}

public typealias RelaySocketFactory = @Sendable (URL) async throws -> any RelayBinarySocket

public enum RelayLegEvent: Sendable, Equatable {
    case ready(peerPresent: Bool)
    case peer(present: Bool)
    case ended(RelayEndCode)
    case reconnectTimedOut
}

/// Arrival order at the leg, before the shim's independently scheduled
/// consumer. A new ICE claim accepts only arrivals newer than its watermark.
public struct RelayInboundDatagram: Sendable {
    public let sequence: UInt64
    public let bytes: Data
}

/// Holds one relay grant and its control/media datagram lanes. The caller
/// owns ICE loopback sockets; this leg only accepts and emits already
/// encrypted agent datagrams, so the same sockets survive WSS rejoins.
public actor RelayLeg {
    private struct Outbox {
        var messages: [Data] = []
        var bytes = 0

        mutating func append(_ message: Data) {
            messages.append(message)
            bytes += message.count
            while messages.count > 64 || bytes > 24_576 {
                bytes -= messages.removeFirst().count
            }
        }

        mutating func pop() -> Data? {
            guard !messages.isEmpty else { return nil }
            let message = messages.removeFirst()
            bytes -= message.count
            return message
        }

        mutating func clear() {
            messages.removeAll()
            bytes = 0
        }
    }

    /// Best-effort encrypted datagrams to the two ICE agent shims. Each
    /// receive lane retains at most its newest 64 datagrams for a slow shim.
    public nonisolated let controlDatagrams: AsyncStream<Data>
    public nonisolated let mediaDatagrams: AsyncStream<Data>
    public nonisolated let controlPackets: AsyncStream<RelayInboundDatagram>
    public nonisolated let mediaPackets: AsyncStream<RelayInboundDatagram>
    private let controlContinuation: AsyncStream<Data>.Continuation
    private let mediaContinuation: AsyncStream<Data>.Continuation
    private let controlPacketContinuation: AsyncStream<RelayInboundDatagram>.Continuation
    private let mediaPacketContinuation: AsyncStream<RelayInboundDatagram>.Continuation
    private let onLifecycle: @Sendable (RelayLegEvent) async -> Void
    private let grant: RelayGrant
    private let factory: RelaySocketFactory
    private let clock: any LinkClock
    private var generation = 0
    private var started = false
    private var terminal = false
    private var socket: (any RelayBinarySocket)?
    private var ready = false
    private var peerPresent = false
    private var reconnectTimer: (any LinkTimer)?
    private var retryTimer: (any LinkTimer)?
    private var control = Outbox()
    private var media = Outbox()
    private var nextLane = RelayLane.control
    private var writerGeneration: Int?
    private var controlReceiveSequence: UInt64 = 0
    private var mediaReceiveSequence: UInt64 = 0
    private var controlOwner: UUID?
    private var rawMediaOwner: UUID?
    private var routedMediaOwners: [UUID: UUID] = [:]

    /// Finished streams may still contain buffered datagrams. Consumers use
    /// this state to discard them after the leg ends.
    public var isTerminal: Bool { terminal }

    public init(grant: RelayGrant, factory: RelaySocketFactory? = nil,
                clock: any LinkClock = SystemLinkClock(),
                onLifecycle: @escaping @Sendable (RelayLegEvent) async -> Void) {
        self.grant = grant
        self.factory = factory ?? { url in try await NetworkRelaySocket.connect(to: url) }
        self.clock = clock
        self.onLifecycle = onLifecycle
        let (control, controlContinuation) = AsyncStream.makeStream(
            of: Data.self, bufferingPolicy: .bufferingNewest(64))
        let (media, mediaContinuation) = AsyncStream.makeStream(
            of: Data.self, bufferingPolicy: .bufferingNewest(64))
        let (controlPackets, controlPacketContinuation) = AsyncStream.makeStream(
            of: RelayInboundDatagram.self, bufferingPolicy: .bufferingNewest(64))
        let (mediaPackets, mediaPacketContinuation) = AsyncStream.makeStream(
            of: RelayInboundDatagram.self, bufferingPolicy: .bufferingNewest(64))
        self.controlDatagrams = control
        self.mediaDatagrams = media
        self.controlPackets = controlPackets
        self.mediaPackets = mediaPackets
        self.controlContinuation = controlContinuation
        self.mediaContinuation = mediaContinuation
        self.controlPacketContinuation = controlPacketContinuation
        self.mediaPacketContinuation = mediaPacketContinuation
    }

    public func start() {
        guard !started, !terminal else { return }
        started = true
        Task { await connect() }
    }

    /// Returns false when the agent datagram is empty or over the wire cap.
    /// An absent peer drops immediately. During a broken WebSocket, bounded
    /// queues keep the newest datagrams until the same grant rejoins.
    @discardableResult
    public func send(_ datagram: Data, on lane: RelayLane) -> Bool {
        guard !terminal, started, !datagram.isEmpty,
              datagram.count <= RelayFrame.maximumDatagramBytes else { return false }
        guard peerPresent else { return false }
        let message = try! RelayFrame.datagram(lane, datagram)
        switch lane {
        case .control: control.append(message)
        case .media: media.append(message)
        }
        startWriter()
        return true
    }

    public func cancel() async {
        guard !terminal else { return }
        terminal = true
        generation += 1
        reconnectTimer?.cancel()
        retryTimer?.cancel()
        reconnectTimer = nil
        retryTimer = nil
        control.clear()
        media.clear()
        let old = socket
        socket = nil
        controlContinuation.finish()
        mediaContinuation.finish()
        controlPacketContinuation.finish()
        mediaPacketContinuation.finish()
        await old?.close()
    }

    public func inboundSequence(on lane: RelayLane) -> UInt64 {
        lane == .control ? controlReceiveSequence : mediaReceiveSequence
    }

    /// Release of the sole control or raw-media owner retires that lane's
    /// unsent datagrams. Routed media uses `discardQueuedMedia(for:)`.
    public func discardQueuedLane(_ lane: RelayLane) {
        switch lane {
        case .control: control.clear()
        case .media: media.clear()
        }
    }

    /// Ownership is checked inside the leg actor, so a packet suspended on
    /// the bridge-to-leg hop cannot enter an outbox after retirement.
    func claimOutgoing(_ token: UUID, on lane: RelayLane, route: UUID?) -> Bool {
        guard !terminal else { return false }
        switch lane {
        case .control:
            guard route == nil, controlOwner == nil else { return false }
            controlOwner = token
        case .media:
            if let route {
                guard routedMediaOwners[route] == nil else { return false }
                routedMediaOwners[route] = token
            } else {
                guard rawMediaOwner == nil else { return false }
                rawMediaOwner = token
            }
        }
        return true
    }

    func sendOwned(_ datagram: Data, on lane: RelayLane, route: UUID?, token: UUID) -> Bool {
        let current: UUID?
        switch lane {
        case .control: current = route == nil ? controlOwner : nil
        case .media:
            if let route { current = routedMediaOwners[route] }
            else { current = rawMediaOwner }
        }
        guard current == token else { return false }
        return send(datagram, on: lane)
    }

    func retireOutgoing(_ token: UUID, on lane: RelayLane, route: UUID?) {
        switch lane {
        case .control:
            guard route == nil, controlOwner == token else { return }
            controlOwner = nil
            control.clear()
        case .media:
            if let route {
                guard routedMediaOwners[route] == token else { return }
                routedMediaOwners.removeValue(forKey: route)
                discardQueuedMedia(for: route)
            } else {
                guard rawMediaOwner == token else { return }
                rawMediaOwner = nil
                media.clear()
            }
        }
    }

    /// A routed ICE owner has ended. Remove only its UUID's frames from the
    /// media outbox so a later WSS rejoin cannot transmit its old datagrams.
    public func discardQueuedMedia(for connectionId: UUID) {
        var uuid = connectionId.uuid
        let prefix = withUnsafeBytes(of: &uuid) { Data($0) }
        media.messages.removeAll { frame in
            frame.count >= 18 && frame.dropFirst().prefix(16).elementsEqual(prefix)
        }
        media.bytes = media.messages.reduce(0) { $0 + $1.count }
    }

    private func scheduleConnect(after delay: Duration) {
        guard !terminal else { return }
        retryTimer?.cancel()
        retryTimer = clock.schedule(after: delay) { [weak self] in
            Task { await self?.connect() }
        }
    }

    private func connect() async {
        guard !terminal, socket == nil else { return }
        generation += 1
        let mine = generation
        do {
            let candidate = try await factory(grant.url)
            guard !terminal, generation == mine else {
                await candidate.close()
                return
            }
            socket = candidate
            try await candidate.send(RelayFrame.join(grant))
            guard !terminal, generation == mine else {
                await candidate.close()
                return
            }
            while let event = await candidate.nextEvent() {
                guard !terminal, generation == mine else { break }
                switch event {
                case .binary(let bytes): await receive(bytes, generation: mine)
                case .text: await finish(.protocolError)
                case .closed: break
                }
                if case .closed = event { break }
                if terminal || generation != mine { break }
            }
            await candidate.close()
            if !terminal, generation == mine { connectionLost() }
        } catch {
            if !terminal, generation == mine { connectionLost(after: .seconds(1)) }
        }
    }

    private func receive(_ bytes: Data, generation: Int) async {
        do {
            switch try RelayFrame.read(bytes) {
            case .ready(let present):
                ready = true
                peerPresent = present
                if !present { control.clear(); media.clear() }
                reconnectTimer?.cancel()
                reconnectTimer = nil
                await onLifecycle(.ready(peerPresent: present))
                if present { startWriter() }
            case .peer(let present):
                peerPresent = present
                if !present { control.clear(); media.clear() }
                await onLifecycle(.peer(present: present))
                if present { startWriter() }
            case .datagram(let lane, let datagram):
                guard ready else { return }
                switch lane {
                case .control:
                    controlReceiveSequence &+= 1
                    controlContinuation.yield(datagram)
                    controlPacketContinuation.yield(RelayInboundDatagram(
                        sequence: controlReceiveSequence, bytes: datagram))
                case .media:
                    mediaReceiveSequence &+= 1
                    mediaContinuation.yield(datagram)
                    mediaPacketContinuation.yield(RelayInboundDatagram(
                        sequence: mediaReceiveSequence, bytes: datagram))
                }
            case .end(let code):
                if code.permitsRejoin {
                    let old = socket
                    socket = nil
                    ready = false
                    peerPresent = false
                    retryAfterEnd(after: code.retryDelay)
                    await old?.close()
                } else {
                    await finish(code)
                }
            case .ignored: break
            }
        } catch {
            await finish(.protocolError)
        }
    }

    private func finish(_ code: RelayEndCode) async {
        guard !terminal else { return }
        terminal = true
        generation += 1
        reconnectTimer?.cancel()
        retryTimer?.cancel()
        control.clear()
        media.clear()
        let old = socket
        socket = nil
        controlContinuation.finish()
        mediaContinuation.finish()
        controlPacketContinuation.finish()
        mediaPacketContinuation.finish()
        Task { await old?.close() }
        await onLifecycle(.ended(code))
    }

    private func connectionLost(after delay: Duration = .zero) {
        socket = nil
        ready = false
        // Keep the last peer state for bounded traffic during a reset.
        generationChanged(after: delay)
    }

    private func retryAfterEnd(after delay: Duration) {
        generation += 1
        reconnectTimer?.cancel()
        reconnectTimer = nil
        scheduleConnect(after: delay)
    }

    private func generationChanged(after delay: Duration) {
        generation += 1
        if reconnectTimer == nil {
            reconnectTimer = clock.schedule(after: .seconds(30)) { [weak self] in
                await self?.reconnectExpired()
            }
        }
        scheduleConnect(after: delay)
    }

    private func reconnectExpired() async {
        guard !terminal else { return }
        terminal = true
        generation += 1
        retryTimer?.cancel()
        control.clear()
        media.clear()
        let old = socket
        socket = nil
        controlContinuation.finish()
        mediaContinuation.finish()
        controlPacketContinuation.finish()
        mediaPacketContinuation.finish()
        Task { await old?.close() }
        await onLifecycle(.reconnectTimedOut)
    }

    private func startWriter() {
        guard writerGeneration != generation, ready, peerPresent, socket != nil, !terminal,
              !control.messages.isEmpty || !media.messages.isEmpty else { return }
        let mine = generation
        writerGeneration = mine
        Task { await writeQueued(generation: mine) }
    }

    private func writeQueued(generation mine: Int) async {
        defer {
            if writerGeneration == mine {
                writerGeneration = nil
                startWriter()
            }
        }
        while generation == mine, ready, peerPresent, !terminal, let current = socket {
            let selected: (RelayLane, Data)?
            if nextLane == .control {
                if let message = control.pop() { selected = (.control, message) }
                else if let message = media.pop() { selected = (.media, message) }
                else { selected = nil }
            } else {
                if let message = media.pop() { selected = (.media, message) }
                else if let message = control.pop() { selected = (.control, message) }
                else { selected = nil }
            }
            guard let (lane, message) = selected else { return }
            nextLane = lane == .control ? .media : .control
            do {
                try await current.send(message)
            } catch {
                if !terminal, generation == mine {
                    connectionLost()
                    await current.close()
                }
                return
            }
        }
    }
}
