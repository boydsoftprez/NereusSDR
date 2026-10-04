// NereusSDR for iOS: reaches a paired Core through the remote access service and opens the control connection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// The phone's side of a control connection through the remote access
/// service (iPhone app plan Task 28a, R-IOS-16; link document section 20;
/// the rendezvous document, sections 6.3 and 8). Each dial:
///
/// 1. opens the first service that answers (``RendezvousClient``) and takes
///    its STUN server, chosen by this phone's address families once the
///    names are resolved (``IceSettings``);
/// 2. makes the control peer (``ControlPeer``) and its offer, and
///    introduces this phone to the Core registered under the Core's id,
///    signed with this phone's device key;
/// 3. takes the Core's answer, resolves the relay names in it, chooses the
///    relay host the same way and gathers; candidates go both ways through
///    the service, and an empty one ends this phone's;
/// 4. once the channel opens, leaves the service and hands the channel to
///    the session, with the SHA-256 of the certificate the Core presented in
///    DTLS, which the session checks against the one the Core's identity key
///    binds before it sends anything.
///
/// A dial fails, in plain words (``RendezvousDialError``):
///
/// - at once when no service answers, or when every service that answered
///   says the Core is not there (`offline`, after trying the next one), or
///   refuses the introduction;
/// - at once when the introduction ends (`stationLeft`, `expired`) or this
///   phone's connection to the service drops while the Core's answer or the
///   end of its candidates is still missing. Once both have come this phone
///   holds everything the service would carry from the Core, the checks can
///   finish without the service, and the dial runs on to its deadline;
/// - when the connection fails, or has not opened within ``dialDeadline``.
///
/// Only one dial runs at a time on a session (the session closes one
/// attempt before the next), so the Core counts this phone once against its
/// cap of two connections still connecting from one source. Nothing secret
/// is logged: no id, key, SDP, candidate or credential.
public final class RendezvousDialer: @unchecked Sendable {
    /// The whole attempt: the service's hello, two name lookups, then
    /// gathering and the connectivity checks (the desktop's
    /// `RendezvousDialer::kDialDeadlineMs`, 79 s).
    public static let dialDeadline: Duration = RendezvousClient.helloTimeout + IceSettings.hostLookupTimeout
        + IceSettings.hostLookupTimeout + IceSettings.connectDeadline

    private static let logger = Logger(subsystem: "NereusSDR", category: "rendezvous.dial")

    /// Resolves names to the address families they reach from this phone.
    public typealias Resolver = @Sendable ([String]) async -> [String: AddressFamilies]

    private let servers: [RendezvousServer]
    private let stationId: String
    private let device: DeviceIdentity
    private let relayAllowed: Bool
    private let clock: any LinkClock
    private let deadline: Duration
    private let makeTransport: RendezvousTransportFactory
    private let localFamilies: @Sendable () -> AddressFamilies
    private let resolve: Resolver
    private let makeRelaySocket: RelaySocketFactory?
    private let passesCandidate: @Sendable (String) -> Bool
    private let beforeApplyingAnswer: @Sendable () async -> Void
    private let serviceGoneWeighed: @Sendable () -> Void

    private let lock = NSLock()
    /// What the last connection this dialer opened was when it opened:
    /// whether its path went through a relay, and the media connection's
    /// ICE settings for that path. Read while the peer was open, so a
    /// closed peer (which has no selected pair) never turns a relayed
    /// session into a direct one.
    private var latestRelayed: Bool?
    private var latestRank: Int?
    private weak var latestPeer: ControlPeer?
    private var latestMediaIce: IceSettings?
    private var latestMediaRelay: RelayRouteContext?
    private var latestRelayIssue: String?
    private var latestTry: ConnectionAttempt.Try?
    private var latestError: RendezvousDialError?
    private var coreCandidatesHandler: (@Sendable ([String]) -> Void)?

    /// A dialer for the Core whose rendezvous id is `stationId`
    /// (``PairedStation/rendezvousId``), as `device`.
    ///
    /// - Parameters:
    ///   - servers: the services, in the order to try them.
    ///   - relayAllowed: with false, no relay candidate is gathered or used.
    public convenience init(servers: [RendezvousServer] = RendezvousServer.defaults, stationId: String,
                            device: DeviceIdentity, relayAllowed: Bool = true,
                            clock: any LinkClock = SystemLinkClock(),
                            deadline: Duration = RendezvousDialer.dialDeadline,
                            transportFactory: @escaping RendezvousTransportFactory = RendezvousWebSocket.factory,
                            relaySocketFactory: RelaySocketFactory? = nil,
                            localFamilies: @escaping @Sendable () -> AddressFamilies = {
                                LocalNetworks.current().usableFamilies
                            },
                            resolve: @escaping Resolver = { await IceSettings.resolveHostFamilies($0) }) {
        self.init(servers: servers, stationId: stationId, device: device, relayAllowed: relayAllowed, clock: clock,
                  deadline: deadline, transportFactory: transportFactory, relaySocketFactory: relaySocketFactory,
                  localFamilies: localFamilies,
                  resolve: resolve, passesCandidate: { _ in true })
    }

    /// As the public initialiser, with `passesCandidate`, a test's filter on
    /// the candidates either way (a test that must go through the relay
    /// passes relay candidates only, so neither end learns a direct path
    /// from the other), and two hooks a test orders the dial by: one run
    /// after the Core's answer has come and before it is applied, one when
    /// the service's going has been weighed. Only this package's tests
    /// reach it; the app cannot.
    package init(servers: [RendezvousServer] = RendezvousServer.defaults, stationId: String, device: DeviceIdentity,
                 relayAllowed: Bool = true, clock: any LinkClock = SystemLinkClock(),
                 deadline: Duration = RendezvousDialer.dialDeadline,
                 transportFactory: @escaping RendezvousTransportFactory = RendezvousWebSocket.factory,
                 relaySocketFactory: RelaySocketFactory? = nil,
                 localFamilies: @escaping @Sendable () -> AddressFamilies = { LocalNetworks.current().usableFamilies },
                 resolve: @escaping Resolver = { await IceSettings.resolveHostFamilies($0) },
                 passesCandidate: @escaping @Sendable (String) -> Bool,
                 beforeApplyingAnswer: @escaping @Sendable () async -> Void = {},
                 serviceGoneWeighed: @escaping @Sendable () -> Void = {}) {
        self.servers = servers
        self.stationId = stationId
        self.device = device
        self.relayAllowed = relayAllowed
        self.clock = clock
        self.deadline = deadline
        makeTransport = transportFactory
        makeRelaySocket = relaySocketFactory
        self.localFamilies = localFamilies
        self.resolve = resolve
        self.passesCandidate = passesCandidate
        self.beforeApplyingAnswer = beforeApplyingAnswer
        self.serviceGoneWeighed = serviceGoneWeighed
    }

    /// Hands `handler`, on each dial from the Core's answer on and whether
    /// or not the channel then opens, every candidate the Core has offered
    /// so far (its answer's SDP first, then each it trickled) each time
    /// that grows. The phone keeps the Core's global host addresses from
    /// them (R-IOS-16). Runs on the dial's own executor; never logged.
    public func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void) {
        lock.withLock { coreCandidatesHandler = handler }
    }

    /// The connector a ``DataChannelSessionTransport`` dials with.
    public var connector: ControlChannelConnector {
        { [self] onEvent in
            try await dial(onEvent: onEvent)
        }
    }

    /// The ICE settings the session's media connection uses for the last
    /// successful peer. While it is open, the selected pair is refreshed
    /// before deciding whether this end's TURN relay is needed.
    public func mediaIceSettings() -> IceSettings? {
        let (peer, fallback, relayed) = lock.withLock { (latestPeer, latestMediaIce, latestRelayed) }
        if let peer { return peer.mediaIceSettings(controlPathRelayed: peer.selectedPathIsRelayed ?? relayed) }
        return fallback
    }

    /// The retained route context for this dialer's latest successful ICE
    /// connection. Media must retain it for its own lifetime, and claim a
    /// UUID-routed or raw media candidate only after mode negotiation.
    public func mediaRelayContext() -> RelayRouteContext? {
        lock.withLock { latestMediaRelay }
    }

    /// The last web relay terminal issue, if one occurred. Ordinary ICE
    /// paths can still succeed when this is set.
    public var lastRelayIssue: String? {
        lock.withLock { latestRelayIssue }
    }

    /// Live selected ICE rank for the most recent successful peer, or its
    /// last observed rank after closure. Direct ICE is 2, TURN is 3, and
    /// the claimed web relay is 4.
    public var lastPathRank: Int? {
        let (peer, fallback) = lock.withLock { (latestPeer, latestRank) }
        guard let rank = peer?.selectedPathRank else { return fallback }
        lock.withLock {
            if latestPeer === peer {
                latestRank = rank
                latestRelayed = rank >= 3
            }
        }
        return rank
    }

    /// Whether the selected pair of the last successful peer currently uses
    /// TURN or the web relay; the last observation remains after it closes.
    public var lastPathRelayed: Bool? {
        if let rank = lastPathRank { return rank >= 3 }
        return lock.withLock { latestRelayed }
    }

    /// Why the last dial failed, nil when it opened or none has run: the
    /// trouble screen's words for the path through the service.
    public var lastError: RendezvousDialError? {
        lock.withLock { latestError }
    }

    /// The last dial as a path of the attempt record (the desktop's words):
    /// through the first service's host, relay once the connection showed
    /// it went through the relay when it opened, direct otherwise, marked
    /// as through the service and with what each end offered once the dial
    /// ended. Nil before a dial.
    public var attemptTry: ConnectionAttempt.Try? {
        let tried = lock.withLock { latestTry }
        guard var tried else {
            return nil
        }
        if tried.outcome == .connected, lastPathRelayed == true {
            tried.path = .relay
        }
        return tried
    }

    /// Dials once. `onEvent` hears what the channel receives from the moment
    /// the peer exists. Returns once the channel is open.
    public func dial(onEvent: @escaping @Sendable (ControlChannelEvent) -> Void) async throws -> ControlChannelConnection {
        let host = servers.first?.host ?? ""
        lock.withLock {
            latestTry = ConnectionAttempt.Try(path: .direct, address: host, throughService: true)
            latestRelayed = nil
            latestRank = nil
            latestPeer = nil
            latestMediaRelay = nil
            latestRelayIssue = nil
            latestError = nil
        }
        let attempt = ControlDial(settings: ControlDial.Settings(
            servers: servers, stationId: stationId, device: device, relayAllowed: relayAllowed, clock: clock,
            deadline: deadline, makeTransport: makeTransport, localFamilies: localFamilies, resolve: resolve,
            makeRelaySocket: makeRelaySocket,
            passesCandidate: passesCandidate,
            coreCandidatesSeen: { [weak self] lines in
                self?.lock.withLock { self?.coreCandidatesHandler }?(lines)
            },
            beforeApplyingAnswer: beforeApplyingAnswer,
            serviceGoneWeighed: serviceGoneWeighed), onEvent: onEvent)
        do {
            let opened = try await attempt.run()
            let relayIssue = await attempt.lastRelayIssue
            let evidence = await attempt.iceEvidence
            lock.withLock {
                latestTry?.ice = evidence
                latestRelayed = opened.relayed
                latestRank = opened.rank
                latestPeer = opened.peer
                latestMediaIce = opened.mediaIce
                latestMediaRelay = opened.mediaRelay
                latestRelayIssue = relayIssue
                latestTry?.outcome = .connected
            }
            return opened.connection
        } catch {
            let relayIssue = await attempt.lastRelayIssue
            let evidence = await attempt.iceEvidence
            let outcome: ConnectionAttempt.Outcome
            switch error as? RendezvousDialError {
            case .notOpenedInTime?, .webRelayTimedOut(_)?:
                outcome = .timedOut
            case .service(.unreachable)?, .service(.offline)?, .service(.noAnswer)?:
                outcome = .noAnswer
            default:
                outcome = .failed
            }
            lock.withLock {
                latestTry?.outcome = outcome
                latestTry?.ice = evidence
                latestError = error as? RendezvousDialError
                latestRelayIssue = relayIssue
            }
            Self.logger.info("The connection through the remote access service did not open: \(String(describing: error), privacy: .public)")
            throw error
        }
    }
}
