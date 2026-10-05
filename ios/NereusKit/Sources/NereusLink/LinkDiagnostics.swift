// NereusSDR for iOS: fixed-field local link diagnostics for the phone log
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// Formatting and sinks receive fixed enums and integers only.
public final class LinkDiagnostics: @unchecked Sendable {
    public enum Connection: String, Sendable, CaseIterable { case control, media }
    public enum Scene: String, Sendable, CaseIterable { case foreground, background, inactive }
    public enum RouteKind: String, Sendable, CaseIterable {
        case host
        case serverReflexive = "server-reflexive"
        case peerReflexive = "peer-reflexive"
        case relay
        case webRelay = "web-relay"
        case wssTunnel = "wss-tunnel"
        case unknown
    }
    public enum RouteRank: Int, Sendable, CaseIterable {
        case unknown = -1, localWebSocket = 0, otherWebSocket = 1, serviceDirect = 2, serviceRelayed = 3, webRelay = 4
    }
    public enum Event: Sendable, Equatable {
        case observedRoute(connection: Connection, kind: RouteKind, rank: RouteRank, milliseconds: Int64)
        case scene(Scene, milliseconds: Int64)
        case keepaliveGap(milliseconds: Int64, scene: Scene, suppressed: UInt64, observedAt: Int64)

        public var line: String {
            switch self {
            case .observedRoute(let connection, let kind, let rank, let milliseconds):
                return "observed-route connection=\(connection.rawValue) kind=\(kind.rawValue) rank=\(rank.rawValue) mono_ms=\(milliseconds)"
            case .scene(let scene, let milliseconds):
                return "scene state=\(scene.rawValue) mono_ms=\(milliseconds)"
            case .keepaliveGap(let milliseconds, let scene, let suppressed, let observedAt):
                return "keepalive-gap gap_ms=\(milliseconds) scene=\(scene.rawValue) suppressed=\(suppressed) mono_ms=\(observedAt)"
            }
        }
    }

    public static let shared = LinkDiagnostics()
    private static let logger = Logger(subsystem: "NereusSDR", category: "link.diagnostics")
    private let lock = NSLock()
    private let clock: any LinkClock
    private let sink: @Sendable (Event) -> Void
    private var scene: Scene = .inactive
    private var loggedScene: Scene?
    private var lastHandoff: Int64?
    private var lastActivity: TransmitHeartbeatGate?
    private var lastGapLine: Int64?
    private var suppressedGaps: UInt64 = 0

    public init(clock: any LinkClock = SystemLinkClock(), sink: (@Sendable (Event) -> Void)? = nil) {
        self.clock = clock
        self.sink = sink ?? { event in
            Self.logger.notice("\(event.line, privacy: .public)")
        }
    }

    public func observedRoute(connection: Connection, kind: RouteKind, rank: RouteRank) {
        let event = lock.withLock {
            Event.observedRoute(connection: connection, kind: kind, rank: rank,
                                milliseconds: clock.nowMilliseconds)
        }
        sink(event)
    }

    public func sceneChanged(to next: Scene) {
        let event: Event? = lock.withLock {
            scene = next
            guard loggedScene != next else { return nil }
            loggedScene = next
            return .scene(next, milliseconds: clock.nowMilliseconds)
        }
        if let event { sink(event) }
    }

    /// Call only after the media sender returned true or an awaited post succeeded.
    public func successfulKeepaliveHandoff(activity: TransmitHeartbeatGate) {
        let event: Event? = lock.withLock {
            let now = clock.nowMilliseconds
            defer { lastHandoff = now }
            // Strong identity prevents reuse and excludes intentional idle intervals.
            // The global limiter and suppressed count survive activity changes.
            guard lastActivity === activity else {
                lastActivity = activity
                return nil
            }
            guard let previous = lastHandoff else { return nil }
            let gap = now - previous
            guard gap > 150 else { return nil }
            if let lastGapLine, now - lastGapLine < 1000 {
                suppressedGaps &+= 1
                return nil
            }
            let event = Event.keepaliveGap(milliseconds: gap, scene: scene,
                                          suppressed: suppressedGaps, observedAt: now)
            lastGapLine = now
            suppressedGaps = 0
            return event
        }
        if let event { sink(event) }
    }
}

/// Private route identities stay with their owner; the logger receives no identity.
public struct ObservedRouteDiagnosticLedger<Identity: Equatable> {
    private var previous: Identity?
    public init() {}
    public mutating func changed(_ current: Identity) -> Bool {
        guard previous != current else { return false }
        previous = current
        return true
    }
    public mutating func reset() { previous = nil }
}

/// Classification uses only the existing guarded observation. No raw pair is read.
public struct ObservedRouteDiagnosticSummary: Sendable {
    public let kind: LinkDiagnostics.RouteKind
    public let rank: LinkDiagnostics.RouteRank

    public init?(_ observation: SelectedRouteObservation, carrierRank: Int? = nil) {
        guard case .available(let route) = observation, route.transport != .tlsWebSocket else { return nil }
        switch route.kind {
        case .webRelay:
            kind = .webRelay; rank = .webRelay
        case .turnRelay:
            kind = .relay; rank = .serviceRelayed
        case .direct:
            rank = .serviceDirect
            switch route.remoteCandidateType {
            case .host: kind = .host
            case .srflx: kind = .serverReflexive
            case .prflx: kind = .peerReflexive
            case .relay: kind = .relay
            case nil: kind = .unknown
            }
        case .wssTunnel:
            guard let carrierRank, let known = LinkDiagnostics.RouteRank(rawValue: carrierRank),
                  known != .unknown else { return nil }
            kind = .wssTunnel; rank = known
        case .unknown, .systemProxy:
            return nil
        }
    }
}
