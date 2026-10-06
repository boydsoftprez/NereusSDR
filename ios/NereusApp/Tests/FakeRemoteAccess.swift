// NereusSDR for iOS: a stand-in for the remote access service in the app's tests, reaching a fake Core or not
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMedia
@testable import NereusSDR

/// Stands in for the remote access service (R-IOS-16): each route
/// ``ConnectionFlow`` makes through it dials the fake Core it reaches, or
/// fails as the service would (the Core not on it, the service not
/// answering), and reports the dial as ``RendezvousDialer`` does: the try's
/// path (relay when ``relayed``), its outcome and the error. The media
/// connection's ICE settings it gives are ``ice``. Nothing reaches a network.
final class FakeRemoteAccess: @unchecked Sendable {
    /// What a dial through the service does.
    enum Way {
        /// It opens a connection to this fake Core.
        case reaches(FakeStation)
        /// The service says the Core is not on it, in its words.
        case offline
        /// No service answers.
        case unreachable
        /// The Core answers, offering these candidates (its answer's SDP
        /// or candidate lines), and then the connection does not open, as
        /// when the introduction's checks fail before sign-in.
        case answersThenFails([String])
    }

    /// The service's words for a Core not on it.
    static let offlineReason =
        "The Core is not reachable right now. Check that it is running and connected to the internet."
    /// The host the attempt record names for a dial through the service.
    static let host = "rv.nereussdr.com"

    private let lock = NSLock()
    private var current: Way
    private var relayedPath: Bool
    private var offered: IceCandidateEvidence?
    private var made: [(stationId: String, route: Route)] = []

    /// The media connection's ICE settings a route gives once it opened:
    /// one STUN server and no relay.
    let ice: IceSettings

    init(_ way: Way, relayed: Bool = false) {
        current = way
        relayedPath = relayed
        var settings = IceSettings(stunUrls: ["stun:stun.invalid:3478"], local: .ipv4Only)
        settings.setRelay(nil, families: 1)
        ice = settings
    }

    var way: Way {
        get { lock.withLock { current } }
        set { lock.withLock { current = newValue } }
    }

    var relayed: Bool {
        get { lock.withLock { relayedPath } }
        set { lock.withLock { relayedPath = newValue } }
    }

    /// What each end offered, which a dial reports once it ends, as
    /// ``RendezvousDialer`` does; nil reports none.
    var evidence: IceCandidateEvidence? {
        get { lock.withLock { offered } }
        set { lock.withLock { offered = newValue } }
    }

    /// The rendezvous ids of the Cores routes were made for, in order.
    var stationIds: [String] { lock.withLock { made.map(\.stationId) } }

    /// Each independently made dialer, including those used by session retries.
    var routes: [Route] { lock.withLock { made.map(\.route) } }

    /// How many connections were dialled through the service.
    var dials: Int { lock.withLock { made.reduce(0) { $0 + $1.route.dials } } }

    /// ``ConnectionFlow/Dependencies/serviceRoute``.
    var maker: @Sendable (PairedStation, DeviceIdentity) -> any CoreServiceRoute {
        { [self] station, _ in
            let route = Route(service: self)
            lock.withLock { made.append((station.rendezvousId, route)) }
            return route
        }
    }

    /// One route: each transport it makes is one dial through the service.
    final class Route: CoreServiceRoute, @unchecked Sendable {
        private let service: FakeRemoteAccess
        private let lock = NSLock()
        private var tried: ConnectionAttempt.Try?
        private var error: RendezvousDialError?
        private var opened = false
        private var count = 0
        private var coreCandidates: (@Sendable ([String]) -> Void)?

        init(service: FakeRemoteAccess) {
            self.service = service
        }

        var dials: Int { lock.withLock { count } }

        func makeTransport() -> any SessionTransport {
            lock.withLock { count += 1 }
            let inner: (any LinkTransport)?
            if case .reaches(let station) = service.way {
                inner = station.transportFactory(station.endpoint, .identity(publicKey: station.identity.publicKey))
            } else {
                inner = nil
            }
            return Transport(route: self, inner: inner)
        }

        func mediaIceSettings() -> IceSettings? {
            lock.withLock { opened } ? service.ice : nil
        }

        var lastTry: ConnectionAttempt.Try? { lock.withLock { tried } }
        var lastError: RendezvousDialError? { lock.withLock { error } }

        func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void) {
            lock.withLock { coreCandidates = handler }
        }

        /// The candidates the Core offers on this dial, handed to the
        /// observer as ``RendezvousDialer`` does; nil when it never answers.
        fileprivate func answer() -> Bool {
            guard case .answersThenFails(let candidates) = service.way else {
                return false
            }
            lock.withLock { coreCandidates }?(candidates)
            return true
        }

        /// The service itself does not answer.
        fileprivate var serviceUnreachable: Bool {
            if case .unreachable = service.way {
                return true
            }
            return false
        }

        fileprivate func begin() {
            lock.withLock {
                tried = ConnectionAttempt.Try(path: .direct, address: FakeRemoteAccess.host, throughService: true)
                error = nil
            }
        }

        fileprivate func finish(_ failure: RendezvousDialError?) {
            let relayed = service.relayed
            let evidence = service.evidence
            lock.withLock {
                error = failure
                tried?.ice = evidence
                if failure == nil {
                    opened = true
                    tried?.outcome = .connected
                    if relayed {
                        tried?.path = .relay
                    }
                } else {
                    tried?.outcome = .noAnswer
                }
            }
        }
    }

    /// One dial, then the fake Core's connection when it reaches it.
    private struct Transport: LinkTransport {
        let route: Route
        let inner: (any LinkTransport)?

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            route.begin()
            if route.answer() {
                route.finish(.notConnected)
                throw RendezvousDialError.notConnected
            }
            guard let inner else {
                let failure: RendezvousDialError = route.serviceUnreachable
                    ? .service(.unreachable) : .service(.offline(reason: FakeRemoteAccess.offlineReason))
                route.finish(failure)
                throw failure
            }
            let digest = try await inner.open(onEvent: onEvent)
            route.finish(nil)
            return digest
        }

        @discardableResult func send(_ text: String) -> Bool {
            inner?.send(text) ?? false
        }

        func ping() {
            inner?.ping()
        }

        func close() {
            inner?.close()
        }
    }
}
