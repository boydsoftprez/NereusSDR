// NereusSDR for iOS: the control session to a Core, from dialling to ready and back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The connection a session runs over: the TLS WebSocket to a Core's
/// address (``WebSocketLinkTransport``), or the control data channel to a
/// Core reached through the remote access service
/// (``DataChannelSessionTransport``, link document section 20). Either
/// opens once, reports the SHA-256 of the certificate the Core presented
/// (in TLS, or in DTLS), carries the same session messages and the
/// heartbeat, and closes; the session runs the same code over both.
public typealias SessionTransport = LinkTransport

/// One control session to a Core (link document sections 5, 6.1 and 12):
/// it opens its transport (the TLS WebSocket, or the control data channel
/// through the remote access service), checks the pin or, for a paired Core, its
/// identity key and certificate binding, answers the Core's `hello` with
/// the highest link major both support and signs in, then passes on every
/// message until the session ends. It keeps the heartbeat and the 30 s
/// connect deadline, counted from the dial, and after a lost link or a
/// retryable end it redials on `ReconnectPolicy`'s schedule until
/// `disconnect()`, except while ``holdRetries()`` holds it (the phone has no
/// network); ``redialNow()`` dials at once and starts the schedule over.
///
/// Reconnect attempts retire their previous connection before selecting a
/// new route. A ready identity-trusted session can briefly hold its old and
/// new connections during a `path.switch` handover without signing in again.
///
/// This is the link layer: `send` sends what it is given whenever the link
/// allows it, whatever the Core's mirrored properties say. Deciding what to
/// offer the operator belongs above it.
/// One actor-coherent reading of the selected physical control transport.
/// Attempt and route numbers are meaningful only within this StationSession.
public struct StationDiagnosticsSnapshot: Sendable {
    public struct RoundTrip: Sendable {
        public let attemptGeneration: Int
        public let routeID: Int
        public let duration: Duration
        public let observedAtMilliseconds: Int64
    }

    public let state: StationSession.State
    public let attemptGeneration: Int?
    public let routeID: Int?
    public let selectedRoute: SelectedRouteObservation
    public let serviceRank: Int?
    public let traffic: LinkTrafficObservation?
    public let roundTrip: RoundTrip?

    public init(state: StationSession.State, attemptGeneration: Int?, routeID: Int?,
                selectedRoute: SelectedRouteObservation, serviceRank: Int? = nil,
                traffic: LinkTrafficObservation?, roundTrip: RoundTrip?) {
        self.state = state
        self.attemptGeneration = attemptGeneration
        self.routeID = routeID
        self.selectedRoute = selectedRoute
        self.serviceRank = serviceRank
        self.traffic = traffic
        self.roundTrip = roundTrip
    }
}

public actor StationSession {
    public enum State: Sendable, Equatable {
        case idle
        case connecting
        case authenticating
        case receivingSnapshot
        case ready
        case waitingToRetry(seconds: Int)
        case stopped
    }

    public enum Event: Sendable, Equatable {
        case stateChanged(State)
        case message(LinkMessage)
        case refused(Refusal)
    }

    /// The name the app gives itself in its `hello`.
    public static let peerName = "NereusSDR iPhone"
    /// The settings schema the app reports in its `hello`; the app keeps no
    /// settings file of the desktop's shape.
    public static let settingsSchema: Int32 = 0
    /// The Core's inbound cap (link document section 12.3).
    public static let maxOutboundMessageBytes = 1_048_576
    /// The whole connect sequence, opening included, must finish this long
    /// after the app dials: one bound from the dial, as the desktop's
    /// StationClient arms kStationHandshakeDeadlineMs (30000) before the
    /// WebSocket upgrade (src/core/session/StationClient.cpp:1011-1021).
    public static let connectDeadline: Duration = .milliseconds(30_000)
    public static let takeoverAnswerDeadline: Duration = .milliseconds(60_000)
    /// A ping goes out this often (section 12.1).
    public static let heartbeatInterval: Duration = .milliseconds(20_000)
    /// The Core's accelerated cadence for a relay path or a direct media
    /// tunnel. The liveness threshold remains two missed pongs.
    public static let acceleratedHeartbeatInterval: Duration = .milliseconds(2_000)
    /// At a tick with this many pings unanswered, the link is lost.
    public static let maxMissedPongs = 2
    /// After the phone's network changes under a ready session, a ping goes
    /// out at once; with no pong this long after it, the link is lost. A
    /// working path answers in well under a second, even over cellular
    /// with a lost segment resent (TCP's first resend is at 1 s), so the
    /// app need not wait out two 20 s heartbeat ticks to notice a path
    /// that went with the old network.
    public static let linkCheckDeadline: Duration = .milliseconds(5_000)

    // Words the app shows when the app itself decided; the Core's own
    // reasons are passed on as sent.
    static let certificateMismatchText =
        "This Core is not the one this app paired with. Pair with it again."
    static let signInFailedText = "This app could not sign in to the Core. Pair with it again."
    static let endedBeforeCheckText = "The Core closed the connection before this app could check it."

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.session")

    /// Everything the session reports, in order.
    public nonisolated let events: AsyncStream<Event>
    private nonisolated let eventSink: AsyncStream<Event>.Continuation
    /// The link's round-trip time, each time a heartbeat ping is answered
    /// (link document section 12.1: recorded for the operator's eye only,
    /// never for liveness). Only a ping sent with no other one unanswered
    /// is timed, so each time belongs to the ping it measures.
    public nonisolated let roundTrips: AsyncStream<Duration>
    private nonisolated let roundTripSink: AsyncStream<Duration>.Continuation

    public private(set) var state: State = .idle
    /// The link major agreed with the Core in the current or last session.
    public private(set) var agreedMajor: UInt16?
    /// The minor agreed with the Core: the lower of the two.
    public private(set) var agreedMinor: UInt16?
    /// The Core's `hello` in the current or last session.
    public private(set) var stationHello: LinkMessage.Hello?
    public var heldQuestion: LinkMessage.SessionHeld? { attempt?.heldQuestion }

    private let trust: StationTrust
    private let authenticator: any StationAuthenticator
    /// True when this session signs in with this device's paired key, not
    /// a Core's older token.
    public nonisolated var signsWithDeviceKey: Bool { authenticator.signsWithDeviceKey }
    private let clock: any LinkClock
    /// Makes the connection for one attempt.
    private let makeTransport: @Sendable () async throws -> any SessionTransport
    /// The `features` object of the app's `hello` (section 6.1).
    private let features: [String: Int]

    private var policy = ReconnectPolicy()
    /// Whether a session that uses media counts as up for the redial
    /// schedule only once its media connection is up.
    private var waitsForMedia = false
    private var acceleratedHeartbeat = false
    private var generation = 0
    /// Each native connect timer arm has its own identity. Cancellation does
    /// not stop an action that already left the timer queue for this actor.
    private var connectDeadlineArm = 0
    private var attempt: Attempt?
    /// An explicit fixed-endpoint factory rank, used only for its route 0.
    private let diagnosticEndpointRank: Int?
    private struct DiagnosticPing {
        let attemptGeneration: Int
        let routeID: Int
        let sentAt: ContinuousClock.Instant
    }
    private var diagnosticPing: DiagnosticPing?
    /// Counts unanswered pings on the selected physical route independently
    /// of legacy heartbeat accounting, which also accepts OLD drain pongs.
    private var diagnosticPingsAwaitingPong = 0
    private var diagnosticRoundTrip: StationDiagnosticsSnapshot.RoundTrip?
    private var selection: TransportSelection?
    private var selectionTask: Task<Void, Never>?
    private var retryTimer: (any LinkTimer)?
    private var retryToken = 0
    /// While the phone has no network, a lost link or a failed attempt
    /// waits without a timer: tries then cannot succeed.
    private var retriesHeld = false
    private var moveSerial = 0
    private var move: ControlMove?
    private var oldDrain: OldDrain?
    private let binaryIngress = SessionBinaryIngress()
    private var mediaTunnelOwner: UUID?
    private var mediaTunnelReceiver: (@Sendable (Data) -> Void)?
    var mediaTunnelReceiverOwner: UUID? { mediaTunnelOwner }
    #if DEBUG
    private var afterCommandHandoffForTesting: (@Sendable (LinkMessage) async -> Void)?

    /// Holds after synchronous transport enqueue but before this actor returns.
    public func holdAfterCommandHandoffForTesting(_ hook: (@Sendable (LinkMessage) async -> Void)?) {
        afterCommandHandoffForTesting = hook
    }
    #endif

    /// Where one connection has got to.
    private enum Phase: Equatable {
        case opening
        case awaitingHello
        case authenticating
        case receivingSnapshot
        case ready
    }

    /// One connection, from dialling to its end.
    private struct Attempt {
        let generation: Int
        var transport: any LinkTransport
        var routeId = 0
        var phase: Phase = .opening
        var certificateSHA256 = Data()
        var helloSent = false
        var authSent = false
        var pingsAwaitingPong = 0
        /// When the one unanswered ping went out, for its round trip.
        var pingSentAt: ContinuousClock.Instant?
        var deadline: (any LinkTimer)?
        var deadlineDueMs: Int64?
        var deadlineRemainingMs: Int64?
        var heldDeadline: (any LinkTimer)?
        var heldQuestion: LinkMessage.SessionHeld?
        var heartbeat: (any LinkTimer)?
        var heartbeatToken = 0
        /// The check after a network change, until a pong answers it.
        var linkCheck: (any LinkTimer)?
        var inbound: [String] = []
        var pumping = false
        /// The Core's latest capabilities in this connection offer media
        /// (`remoteMediaVersion` 1 or more at agreed minor 1 or more).
        var mediaOffered = false
        var controlSwitchOffered = false
        /// The Core has shown it is the one the app trusts: at once under a
        /// pinned certificate (checked in the handshake), and under an
        /// identity trust only once its `hello` has passed the check.
        /// Until then nothing it sends is passed on.
        var identityVerified = false
        /// Ready, and the redial schedule waits for the media connection
        /// before it starts over.
        var resetAwaitsMedia = false
    }

    private enum MovePhase: Equatable { case requesting, joined, committing }

    /// Caller cancellation can run off the session actor. Its synchronous
    /// flag arbitrates cancellation against the final hello/join send, so
    /// an actor hop to clean up cannot let a canceled ticket slip through.
    private final class MoveCancellation: @unchecked Sendable {
        private let lock = NSLock()
        private var canceled = false
        var isCanceled: Bool { lock.withLock { canceled } }
        func cancel() { lock.withLock { canceled = true } }
        func sendJoin(_ action: () -> Bool) -> Bool {
            lock.withLock {
                guard !canceled else { return false }
                return action()
            }
        }
    }

    private struct ControlMove {
        let id: Int
        let generation: Int
        let candidate: PreauthenticatedTransport
        let hello: LinkMessage.Hello
        let cancellation: MoveCancellation
        let onRouteCommit: @Sendable () async -> Void
        var phase: MovePhase = .requesting
        var timeout: (any LinkTimer)?
        var ticketExpiry: (any LinkTimer)?
        var held: [String] = []
        var heldBytes = 0
        var heldBinary: [Data] = []
        var heldBinaryBytes = 0
        var sawHello = false
        var completion: CheckedContinuation<Bool, Never>?
        var preparation: Task<Void, Never>?
        var routeCommit: Task<Void, Never>?
    }

    private struct OldDrain {
        let generation: Int
        let routeId: Int
        let transport: any LinkTransport
        let timer: any LinkTimer
    }

    /// `features` is what the app's `hello` declares: the app's own
    /// (``LinkFeatures/app``), or, for a conformance fixture whose client
    /// declares fewer of them by name, that fixture's.
    public init(endpoint: StationEndpoint, trust: StationTrust, authenticator: any StationAuthenticator,
                clock: any LinkClock = SystemLinkClock(),
                transportFactory: @escaping LinkTransportFactory = WebSocketLinkTransport.factory,
                features: [String: Int] = LinkFeatures.app,
                diagnosticEndpointRank: Int? = nil) {
        self.init(trust: trust, authenticator: authenticator, clock: clock,
                  transport: { transportFactory(endpoint, trust) }, features: features,
                  diagnosticEndpointRank: diagnosticEndpointRank)
    }

    /// A session over transports `transport` makes, one for each attempt:
    /// ``DataChannelSessionTransport/factory(_:)`` for a Core reached
    /// through the remote access service, which has no address of its own
    /// here. Everything else is as the other initialiser.
    public init(trust: StationTrust, authenticator: any StationAuthenticator,
                clock: any LinkClock = SystemLinkClock(),
                transport: @escaping @Sendable () -> any SessionTransport,
                features: [String: Int] = LinkFeatures.app,
                diagnosticEndpointRank: Int? = nil) {
        self.init(trust: trust, authenticator: authenticator, clock: clock,
                  asyncTransport: { transport() }, features: features,
                  diagnosticEndpointRank: diagnosticEndpointRank)
    }

    /// Selects a fresh route for every attempt, including retries. The
    /// app's route racer can return a previously inspected live lease.
    public init(trust: StationTrust, authenticator: any StationAuthenticator,
                clock: any LinkClock = SystemLinkClock(),
                asyncTransport: @escaping @Sendable () async throws -> any SessionTransport,
                features: [String: Int] = LinkFeatures.app,
                diagnosticEndpointRank: Int? = nil) {
        self.diagnosticEndpointRank = diagnosticEndpointRank.flatMap { (0...1).contains($0) ? $0 : nil }
        self.features = features
        self.trust = trust
        self.authenticator = authenticator
        self.clock = clock
        self.makeTransport = asyncTransport
        (events, eventSink) = AsyncStream.makeStream(of: Event.self)
        (roundTrips, roundTripSink) = AsyncStream.makeStream(of: Duration.self, bufferingPolicy: .bufferingNewest(1))
        binaryIngress.setHandler { [weak self] frame, generation, routeId in
            await self?.handleBinary(frame, generation: generation, routeId: routeId)
        }
    }

    deinit {
        eventSink.finish()
        roundTripSink.finish()
    }

    /// Pings sent in this connection that no pong has answered yet.
    var pingsAwaitingPong: Int { attempt?.pingsAwaitingPong ?? 0 }

    /// Does not suspend between selecting the transport and reading its route
    /// and traffic. A previous route's receipt is never current after a move.
    public func diagnosticsSnapshot() -> StationDiagnosticsSnapshot {
        guard state == .ready, let current = attempt, current.phase == .ready else {
            return StationDiagnosticsSnapshot(state: state, attemptGeneration: nil, routeID: nil,
                                              selectedRoute: .unavailable(.notReady), serviceRank: nil, traffic: nil,
                                              roundTrip: nil)
        }
        let selectedRoute = current.transport.selectedRouteObservation
        let capturedRank = current.transport.diagnosticServiceRank
            ?? (current.routeId == 0 ? diagnosticEndpointRank : nil)
        let serviceRank = Self.diagnosticRank(selectedRoute, capturedRank: capturedRank)
        let traffic = current.transport.trafficObservation
        let matching = diagnosticRoundTrip.flatMap { receipt in
            receipt.attemptGeneration == current.generation && receipt.routeID == current.routeId
                ? receipt : nil
        }
        return StationDiagnosticsSnapshot(state: state, attemptGeneration: current.generation,
                                          routeID: current.routeId, selectedRoute: selectedRoute, serviceRank: serviceRank,
                                          traffic: traffic?.active == true ? traffic : nil,
                                          roundTrip: matching)
    }

    private static func diagnosticRank(_ observation: SelectedRouteObservation, capturedRank: Int?) -> Int? {
        switch observation {
        case .available(let route):
            if route.transport == .tlsWebSocket { return capturedRank }
            switch route.kind {
            case .direct: return 2
            case .turnRelay: return 3
            case .webRelay: return 4
            case .unknown, .wssTunnel, .systemProxy: return nil
            }
        case .unavailable(let reason, _, _):
            switch reason {
            case .retired, .notReady, .noCurrentMedia: return nil
            default: return capturedRank
            }
        }
    }

    /// Lets race tests await a canceled preparation after its external gate opens.
    var movePreparationForTesting: Task<Void, Never>? { move?.preparation }

    // MARK: Connecting and stopping

    /// Dials the Core, unless a connection is already under way. Returns
    /// once the connection has opened or failed to.
    public func connect() async {
        switch state {
        case .idle, .stopped, .waitingToRetry:
            break
        default:
            return
        }
        retryTimer?.cancel()
        retryTimer = nil
        retriesHeld = false
        policy.reset()
        await startAttempt()
    }

    /// The phone has no network: no redial until ``redialNow()``. A redial
    /// already scheduled is called off; an attempt under way carries on
    /// and, if it fails, waits.
    public func holdRetries() {
        retriesHeld = true
        retryToken += 1
        retryTimer?.cancel()
        retryTimer = nil
        if selection != nil {
            generation += 1
            cancelSelection()
            setState(.waitingToRetry(seconds: 0))
        }
    }

    /// The phone's network came back or changed: lifts a hold and, when the
    /// session is waiting to retry or still opening a connection (which may
    /// be on a network that is gone), dials again at once with the redial
    /// schedule started over. A ready session checks its link at once: a
    /// ping now, and the link is lost if no pong answers within
    /// ``linkCheckDeadline``. A session that is signing in or stopped is
    /// left alone. Returns once the new connection has opened or failed to.
    public func redialNow() async {
        retriesHeld = false
        switch state {
        case .waitingToRetry:
            break
        case .ready:
            checkLinkNow()
            return
        case .connecting:
            if selection != nil {
                generation += 1
                cancelSelection()
            } else {
                guard let current = attempt, current.phase == .opening else {
                    return
                }
                // One connection at a time: the old one closes before the next dials.
                generation += 1
                connectDeadlineArm &+= 1
                current.deadline?.cancel()
                current.heldDeadline?.cancel()
                current.heartbeat?.cancel()
                current.linkCheck?.cancel()
                current.transport.close()
                attempt = nil
            }
        default:
            return
        }
        retryToken += 1
        retryTimer?.cancel()
        retryTimer = nil
        policy.reset()
        await startAttempt()
    }

    /// Closes the session and stops: no further attempt, not even one
    /// already scheduled.
    public func disconnect() {
        diagnosticPing = nil
        diagnosticPingsAwaitingPong = 0
        diagnosticRoundTrip = nil
        policy.cancel()
        retriesHeld = false
        retryTimer?.cancel()
        retryTimer = nil
        generation += 1
        cancelSelection()
        abandonMove()
        retireOld()
        if let current = attempt {
            connectDeadlineArm &+= 1
            current.deadline?.cancel()
            current.heldDeadline?.cancel()
            current.heartbeat?.cancel()
            current.linkCheck?.cancel()
            current.transport.close()
            attempt = nil
        }
        setState(.stopped)
    }

    // MARK: Media

    /// With `true`, a session whose Core offers media counts as up for the
    /// redial schedule only when ``mediaConnectionUp()`` says its media
    /// connection is up, not already at `snapshot.complete`: a Core that
    /// signs in but never carries audio or display keeps backing off.
    /// Set it before ``connect()``.
    public func setWaitsForMedia(_ waits: Bool) {
        waitsForMedia = waits
    }

    /// The current session's media connection is up: the redial schedule
    /// starts over, if it was waiting for this.
    public func mediaConnectionUp() {
        guard attempt?.phase == .ready, attempt?.resetAwaitsMedia == true else {
            return
        }
        attempt?.resetAwaitsMedia = false
        policy.reset()
    }

    /// The app enables this for a selected relay path (rank 3 or 4) or a
    /// direct media tunnel. Repeating the current value preserves the next
    /// scheduled tick; a changed value starts that cadence immediately.
    public func setAcceleratedHeartbeat(_ enabled: Bool) {
        guard acceleratedHeartbeat != enabled else { return }
        acceleratedHeartbeat = enabled
        guard var current = attempt, current.phase != .opening else { return }
        current.heartbeat?.cancel()
        current.heartbeatToken += 1
        current.heartbeat = scheduleHeartbeat(current.generation, token: current.heartbeatToken)
        attempt = current
    }

    // MARK: Moving a ready control session

    /// Moves one ready identity-trusted session to an inspected live lease.
    /// The app keeps ownership of the `session.pathTicket` command through
    /// `requestTicket`, including its id and result matching. `safetyCheck`
    /// reads live key, VOX and MOX-transition state before the request and
    /// again before the join. `onRouteCommit` updates media context before
    /// any held message from the new connection reaches `events`. Once the
    /// OLD barrier commits, canceling the caller cannot restore OLD: this
    /// call still awaits route commit and returns `true` while NEW lives.
    /// A `false` result means NEW never became the active live route, or
    /// the session ended during route commit.
    @discardableResult
    public func moveControl(to candidate: PreauthenticatedTransport,
                            safetyCheck: @escaping @Sendable () async -> Bool,
                            requestTicket: @escaping @Sendable () async throws -> PathTicket,
                            onRouteCommit: @escaping @Sendable () async -> Void) async -> Bool {
        guard !Task.isCancelled, case .identity(let publicKey) = trust,
              let current = attempt, current.phase == .ready,
              current.controlSwitchOffered, move == nil,
              let major = agreedMajor, let minor = agreedMinor,
              let checked = await candidate.inspectedConnection(),
              StationTrust.identityVerifies(checked.0.identity, publicKey: publicKey,
                                            certificateSHA256: checked.1),
              LinkVersionPolicy.agree(ours: LinkVersionPolicy.supportedMajors,
                                      theirs: checked.0.supportedMajors) == major,
              min(LinkVersionPolicy.minor, checked.0.minor) == minor else {
            candidate.close()
            return false
        }
        // Another suspension above may have retired the ready session.
        guard !Task.isCancelled, attempt?.generation == current.generation, attempt?.phase == .ready,
              move == nil else {
            candidate.close()
            return false
        }
        retireOld()
        moveSerial += 1
        let id = moveSerial
        let gen = current.generation
        let cancellation = MoveCancellation()
        move = ControlMove(id: id, generation: gen, candidate: candidate,
                           hello: checked.0, cancellation: cancellation,
                           onRouteCommit: onRouteCommit)
        // The request, join and barrier share the Core's 10 s ticket
        // window; a slow request must not buy a fresh 10 s after it returns.
        move?.timeout = clock.schedule(after: .seconds(10)) { [weak self] in
            await self?.moveTimedOut(id, generation: gen)
        }
        return await withTaskCancellationHandler {
            await withCheckedContinuation { completion in
                guard !Task.isCancelled, isMove(id, gen, phase: .requesting) else {
                    abandonMove(id)
                    completion.resume(returning: false)
                    return
                }
                move?.completion = completion
                move?.preparation = Task { [weak self] in
                    await self?.prepareMove(id: id, generation: gen, candidate: candidate,
                                            digest: checked.1, major: major,
                                            cancellation: cancellation,
                                            safetyCheck: safetyCheck, requestTicket: requestTicket)
                }
            }
        } onCancel: { [weak self] in
            cancellation.cancel()
            Task { [weak self] in await self?.cancelCallerMove(id) }
        }
    }

    private func cancelCallerMove(_ id: Int) {
        guard move?.id == id, move?.phase != .committing else { return }
        abandonMove(id)
    }

    private func prepareMove(id: Int, generation gen: Int, candidate: PreauthenticatedTransport,
                             digest: Data, major: UInt16, cancellation: MoveCancellation,
                             safetyCheck: @escaping @Sendable () async -> Bool,
                             requestTicket: @escaping @Sendable () async throws -> PathTicket) async {
        guard !Task.isCancelled, !cancellation.isCanceled,
              await safetyCheck(), !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting),
              await candidate.isAvailable() else {
            abandonMove(id)
            return
        }
        guard !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting) else {
            abandonMove(id)
            return
        }
        let ticket: PathTicket
        do {
            ticket = try await requestTicket()
        } catch {
            abandonMove(id)
            return
        }
        guard !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting), await safetyCheck(),
              !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting),
              await candidate.isAvailable() else {
            abandonMove(id)
            return
        }
        guard !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting) else {
            abandonMove(id)
            return
        }
        if ticket.expiresInMs < 10_000 {
            move?.ticketExpiry = clock.schedule(after: .milliseconds(ticket.expiresInMs)) { [weak self] in
                await self?.moveTimedOut(id, generation: gen)
            }
        }
        do {
            installBinary(on: candidate, generation: gen, routeId: id)
            let opened = try await candidate.open { [weak self] event in
                await self?.handleCandidate(event, generation: gen, moveId: id)
            }
            guard !Task.isCancelled, !cancellation.isCanceled,
                  opened == digest, isMove(id, gen, phase: .requesting),
                  await safetyCheck(), !Task.isCancelled, !cancellation.isCanceled,
                  isMove(id, gen, phase: .requesting),
                  await candidate.isAvailable() else {
                abandonMove(id)
                return
            }
        } catch {
            abandonMove(id)
            return
        }
        guard !Task.isCancelled, !cancellation.isCanceled,
              isMove(id, gen, phase: .requesting) else {
            abandonMove(id)
            return
        }
        // Same hello as an initial sign-in, then a join in place of
        // auth.request. The secret is never reported as an event.
        let ownHello = LinkMessage.Hello(major: major, minor: LinkVersionPolicy.minor,
                                         settingsSchema: Self.settingsSchema, peer: Self.peerName,
                                         majors: LinkVersionPolicy.supportedMajors, features: features)
        guard cancellation.sendJoin({
            guard candidate.send(LinkCodec.encode(.hello(ownHello))) else { return false }
            return candidate.send(LinkCodec.encode(.pathJoin(.init(ticket: ticket.secret))))
        }) else {
            abandonMove(id)
            return
        }
        move?.phase = .joined
        Task { [weak self] in
            if !(await candidate.releaseEvents()) {
                await self?.rejectCandidate(id, generation: gen)
            }
        }
    }

    private func isMove(_ id: Int, _ gen: Int, phase: MovePhase) -> Bool {
        move?.id == id && move?.generation == gen && move?.phase == phase &&
            attempt?.generation == gen && attempt?.phase == .ready
    }

    private func moveTimedOut(_ id: Int, generation gen: Int) {
        guard move?.id == id, move?.generation == gen,
              move?.phase != .committing else { return }
        abandonMove(id)
    }

    private func handleCandidate(_ event: LinkTransportEvent, generation gen: Int,
                                 moveId id: Int) async {
        if attempt?.generation == gen,
           (oldDrain?.routeId == id || (attempt?.routeId == id && move?.id != id)) {
            await handle(event, generation: gen, routeId: id)
            return
        }
        guard move?.id == id, move?.generation == gen else { return }
        switch event {
        case .pong:
            receivedPong(generation: gen, routeID: id)
        case .closed:
            if move?.phase == .committing {
                finish(gen, refusal: nil, retry: true)
            } else {
                abandonMove(id)
            }
        case .text(let text):
            let bytes = text.utf8.count
            guard bytes <= WebSocketLinkTransport.maxInboundMessageBytes,
                  (move?.heldBytes ?? 0) + bytes <= 4 * WebSocketLinkTransport.maxInboundMessageBytes else {
                finish(gen, refusal: nil, retry: true)
                return
            }
            let message = try? LinkCodec.decode(text)
            if move?.sawHello == false {
                guard case .hello(let hello)? = message, hello == move?.hello else {
                    rejectCandidate(id, generation: gen)
                    return
                }
                move?.sawHello = true
                return
            }
            guard let message else {
                // A future Core message follows the same extension rule as
                // on the original route: preserve ordering, then let the
                // normal receiver ignore a kind this build cannot read.
                move?.held.append(text)
                move?.heldBytes += bytes
                return
            }
            switch message {
            case .hello, .authResult, .snapshotComplete, .pathJoin, .pathSwitch:
                rejectCandidate(id, generation: gen)
            case .sessionEnd:
                if move?.phase == .committing {
                    finish(gen, refusal: nil, retry: true)
                } else {
                    abandonMove(id)
                }
            default:
                move?.held.append(text)
                move?.heldBytes += bytes
            }
        }
    }

    private func rejectCandidate(_ id: Int, generation gen: Int) {
        if move?.id == id, move?.phase == .committing {
            finish(gen, refusal: nil, retry: true)
        } else {
            abandonMove(id)
        }
    }

    private func commitMove(oldClosed: Bool = false) {
        guard let moving = move, moving.phase == .joined,
              let current = attempt, current.generation == moving.generation else { return }
        // The inspected lease's original 30 s budget ends only when Core
        // actually moved this session. A late barrier cannot revive it.
        guard moving.candidate.sessionReady() else {
            if oldClosed {
                finish(current.generation, refusal: nil, retry: true)
            } else {
                abandonMove(moving.id)
            }
            return
        }
        // This is the last outbound text on OLD. No actor suspension occurs
        // between it and changing the route used by all future sends.
        // OLD can already be closed when the joined candidate takes over.
        _ = current.transport.send(LinkCodec.encode(.pathSwitch))
        attempt?.transport = moving.candidate
        attempt?.routeId = moving.id
        diagnosticPing = nil
        diagnosticPingsAwaitingPong = 0
        diagnosticRoundTrip = nil
        attempt?.inbound.removeAll()
        move?.phase = .committing
        move?.timeout?.cancel()
        move?.timeout = nil
        move?.ticketExpiry?.cancel()
        move?.ticketExpiry = nil
        let old = current.transport
        let gen = current.generation
        let routeId = current.routeId
        let timer = clock.schedule(after: .seconds(5)) { [weak self] in
            await self?.oldDrainDue(routeId, generation: gen)
        }
        oldDrain = OldDrain(generation: gen, routeId: routeId, transport: old, timer: timer)
        let commitTask = Task { [weak self] in
            await moving.onRouteCommit()
            await self?.routeCommitFinished(moving.id, generation: moving.generation)
        }
        move?.routeCommit = commitTask
    }

    private func routeCommitFinished(_ id: Int, generation gen: Int) async {
        guard isMove(id, gen, phase: .committing), let held = move?.held,
              let heldBinary = move?.heldBinary else { return }
        move?.completion?.resume(returning: true)
        move = nil
        attempt?.inbound.append(contentsOf: held)
        await pump(gen)
        for frame in heldBinary { handleBinary(frame, generation: gen, routeId: id) }
    }

    private func abandonMove(_ id: Int? = nil) {
        guard let moving = move, id == nil || moving.id == id else { return }
        move = nil
        moving.cancellation.cancel()
        moving.timeout?.cancel()
        moving.ticketExpiry?.cancel()
        moving.preparation?.cancel()
        moving.routeCommit?.cancel()
        moving.candidate.close()
        moving.completion?.resume(returning: false)
    }

    private func oldDrainDue(_ routeId: Int, generation gen: Int) {
        guard attempt?.generation == gen, oldDrain?.generation == gen,
              oldDrain?.routeId == routeId else { return }
        retireOld()
    }

    private func retireOld() {
        guard let old = oldDrain else { return }
        oldDrain = nil
        old.timer.cancel()
        old.transport.close()
    }

    // MARK: Sending

    /// One shared tunnel context owns this receiver. A retired context may
    /// clear only its own registration, never a later owner's.
    public func setMediaTunnelReceiver(owner: UUID,
                                       receiver: (@Sendable (Data) -> Void)?) {
        if let receiver {
            mediaTunnelOwner = owner
            mediaTunnelReceiver = receiver
        } else if mediaTunnelOwner == owner {
            mediaTunnelOwner = nil
            mediaTunnelReceiver = nil
        }
    }

    /// A context that closed while its registration awaited this actor must
    /// not install over a newer owner's receiver.
    func setMediaTunnelReceiver(owner: UUID, registration: MediaTunnelRegistrationPermit,
                                receiver: @escaping @Sendable (Data) -> Void) {
        guard registration.isLive else { return }
        setMediaTunnelReceiver(owner: owner, receiver: receiver)
    }

    /// Send on the current physical route, including during OLD's drain.
    @discardableResult public func sendMediaTunnel(_ frame: Data) -> Bool {
        guard state == .ready, attempt?.phase == .ready,
              (18...1501).contains(frame.count), frame.first == 2,
              let transport = attempt?.transport else { return false }
        return transport.sendBinary(frame)
    }

    @discardableResult func sendMediaTunnel(_ frame: Data,
                                           ownership: BinaryMediaOwnership) -> Bool {
        guard state == .ready, attempt?.phase == .ready,
              (18...1501).contains(frame.count), frame.first == 2,
              let transport = attempt?.transport else { return false }
        return transport.sendBinary(frame, ownership: ownership)
    }

    func discardMediaTunnel(ownership: BinaryMediaOwnership) {
        attempt?.transport.discardBinary(ownership: ownership)
        oldDrain?.transport.discardBinary(ownership: ownership)
    }

    private func installBinary(on transport: any LinkTransport, generation: Int, routeId: Int) {
        let ingress = binaryIngress
        transport.setBinaryReceiver { frame in
            ingress.offer(frame, generation: generation, routeId: routeId)
        }
    }

    private func handleBinary(_ frame: Data, generation gen: Int, routeId: Int) {
        guard state == .ready, let current = attempt,
              current.generation == gen, current.phase == .ready,
              (18...1501).contains(frame.count), frame.first == 2 else { return }
        if routeId == current.routeId {
            if move?.phase == .committing { holdBinary(frame) }
            else { mediaTunnelReceiver?(frame) }
        } else if oldDrain?.generation == gen, oldDrain?.routeId == routeId {
            mediaTunnelReceiver?(frame)
        } else if move?.generation == gen, move?.id == routeId,
                  move?.phase == .joined || move?.phase == .committing {
            holdBinary(frame)
        }
    }

    private func holdBinary(_ frame: Data) {
        guard move != nil else { return }
        move?.heldBinary.append(frame)
        move?.heldBinaryBytes += frame.count
        while (move?.heldBinary.count ?? 0) > 64 || (move?.heldBinaryBytes ?? 0) > 24_576 {
            if let removed = move?.heldBinary.removeFirst() {
                move?.heldBinaryBytes -= removed.count
            }
        }
    }

    /// Sends one message. Refuses locally, sending nothing, whatever the
    /// Core would end the session for: a `hello` or `auth.request` (the
    /// session sends its own, once), anything before the Core accepted the
    /// sign-in, `media.control` before `snapshot.complete`, a `pair.*`
    /// message (pairing has a connection of its own), a message over the
    /// Core's 1 MiB cap, and a message the Core could not read.
    public func send(_ message: LinkMessage) async throws {
        switch message {
        case .hello:
            throw LinkSendError.secondHello
        case .authRequest:
            throw LinkSendError.authRequestOutOfOrder
        case .pairStart, .pairSpake, .pairConfirm, .pairFail, .pairAccept:
            throw LinkSendError.pairingOnSession
        case .sessionTakeover(let answer):
            guard let held = attempt?.heldQuestion, held.revision == answer.revision,
                  answer.deviceId.isEmpty || held.devices.contains(where: {
                      $0.deviceId == answer.deviceId && $0.replaceable
                  }) else { throw LinkSendError.notConnected }
            guard try transmit(message) else { throw LinkSendError.notConnected }
        default:
            guard try transmit(message) else { throw LinkSendError.notConnected }
        }
    }

    /// Check cancellation and admitting authority with no suspension between
    /// authorization and the transport's synchronous enqueue.
    public func send(_ message: LinkMessage, permit: CommandSendPermit) async throws {
        switch message {
        case .commandInvoke, .propertyWrite, .settingsWrite, .settingsRemove:
            break
        default:
            throw LinkSendError.notAClientMessage
        }
        guard try permit.handoff({ try transmit(message) }) else {
            throw LinkSendError.notConnected
        }
        #if DEBUG
        await afterCommandHandoffForTesting?(message)
        #endif
    }

    /// The command client uses this for a heartbeat. The gate is checked
    /// during the synchronous transport handoff, after actor admission.
    public func send(_ message: LinkMessage, while gate: TransmitHeartbeatGate) async throws {
        guard case .commandInvoke(let invoke) = message, invoke.verb == "tx.keepalive" else {
            throw LinkSendError.notAClientMessage
        }
        guard try gate.handoff({ try transmit(message) }) else {
            throw LinkSendError.notConnected
        }
    }

    @discardableResult
    private func transmit(_ message: LinkMessage) throws -> Bool {
        guard var current = attempt, current.phase != .opening else {
            throw LinkSendError.notConnected
        }
        guard message.kind.sentByClient else {
            throw LinkSendError.notAClientMessage
        }
        switch message {
        case .hello:
            guard !current.helloSent else {
                throw LinkSendError.secondHello
            }
        case .authRequest:
            guard current.helloSent, !current.authSent else {
                throw LinkSendError.authRequestOutOfOrder
            }
        case .mediaControl:
            guard current.phase == .ready else {
                throw current.phase == .receivingSnapshot ? LinkSendError.beforeSnapshotComplete
                    : LinkSendError.beforeSignIn
            }
        default:
            guard current.phase == .receivingSnapshot || current.phase == .ready else {
                throw LinkSendError.beforeSignIn
            }
        }
        let text = LinkCodec.encode(message)
        let bytes = text.utf8.count
        guard bytes <= Self.maxOutboundMessageBytes else {
            throw LinkSendError.tooLarge(bytes: bytes)
        }
        // The Core ends the session on a message it cannot decode, so one
        // its decoder would refuse (an id of 0 in a numbered family, an
        // oversized media.control, a value out of range) never leaves.
        do {
            _ = try LinkCodec.decode(text)
        } catch LinkCodecError.overCap {
            throw LinkSendError.tooLarge(bytes: bytes)
        } catch {
            throw LinkSendError.unreadable(String(describing: error))
        }
        guard current.transport.send(text) else { return false }
        switch message {
        case .hello:
            current.helloSent = true
        case .authRequest:
            current.authSent = true
        default:
            break
        }
        attempt = current
        return true
    }

    // MARK: One attempt

    private func startAttempt() async {
        generation += 1
        let gen = generation
        if case .pairing = trust {
            // A pairing connection is not a session: it signs nothing in
            // and ends once the pairing does (section 3.6). Only a
            // programming mistake reaches this.
            assertionFailure("a StationSession was given a pairing trust")
            Self.logger.warning("A session was given a pairing trust; nothing was dialled")
            eventSink.yield(.refused(Refusal(.authentication(Self.signInFailedText))))
            setState(.stopped)
            return
        }
        if authenticator.signsWithDeviceKey, !trust.isIdentity {
            // The device key signs only for a Core whose key and binding
            // the session checks first (section 3.4).
            Self.logger.warning("A device key sign-in needs an identity trust; nothing was dialled")
            eventSink.yield(.refused(Refusal(.authentication(Self.signInFailedText))))
            setState(.stopped)
            return
        }
        Self.logger.notice("Dialling the Core")
        setState(.connecting)
        let gate = TransportSelection()
        selection = gate
        selectionTask = Task { [makeTransport] in
            do {
                gate.complete(try await makeTransport())
            } catch {
                gate.fail(error)
            }
        }
        let transport: any SessionTransport
        do {
            transport = try await gate.value()
        } catch {
            guard gen == generation else { return }
            selection = nil
            selectionTask = nil
            if let failure = error as? LinkTransportError, failure == .certificateMismatch {
                eventSink.yield(.refused(Self.identityChangedRefusal))
                finishSelectionFailure(retry: false)
            } else {
                finishSelectionFailure(retry: true)
            }
            return
        }
        guard gen == generation, selection === gate else {
            transport.close()
            return
        }
        selection = nil
        selectionTask = nil
        attempt = Attempt(generation: gen, transport: transport)
        diagnosticPing = nil
        diagnosticPingsAwaitingPong = 0
        diagnosticRoundTrip = nil
        installBinary(on: transport, generation: gen, routeId: 0)
        // One clock from the dial: a Core slow to open leaves the connect
        // sequence only what is left of the 30 s, never another 30 s. A
        // transport that bounds its own opening (the control channel's dial
        // through the remote access service) starts it once open instead.
        if !transport.boundsItsOwnOpening && !(transport is PreauthenticatedTransport) {
            armConnectDeadline(gen, after: Self.connectDeadline)
        }
        do {
            let digest = try await transport.open { [weak self] event in
                await self?.handle(event, generation: gen, routeId: 0)
            }
            guard isCurrent(gen) else {
                transport.close()
                return
            }
            await opened(gen, certificateSHA256: digest)
            if let preauthenticated = transport as? PreauthenticatedTransport,
               !(await preauthenticated.releaseEvents()), isCurrent(gen) {
                finish(gen, refusal: nil, retry: true)
            }
        } catch {
            guard isCurrent(gen) else {
                return
            }
            if let failure = error as? LinkTransportError, failure == .certificateMismatch {
                finish(gen, refusal: Self.identityChangedRefusal, retry: false)
            } else {
                Self.logger.info("Could not reach the Core: \(String(describing: error), privacy: .public)")
                finish(gen, refusal: nil, retry: true)
            }
        }
    }

    private func cancelSelection() {
        selection?.cancel()
        selectionTask?.cancel()
        selection = nil
        selectionTask = nil
    }

    private func finishSelectionFailure(retry: Bool) {
        guard retry, let delay = policy.nextDelay() else {
            setState(.stopped)
            return
        }
        retryToken += 1
        let token = retryToken
        setState(.waitingToRetry(seconds: delay))
        guard !retriesHeld else { return }
        retryTimer = clock.schedule(after: .seconds(delay)) { [weak self] in
            await self?.retryDue(token)
        }
    }

    private func isCurrent(_ gen: Int) -> Bool {
        attempt?.generation == gen
    }

    /// The Core is not the one this app paired with: the app's own end
    /// (section 12.4), final.
    private static let identityChangedRefusal =
        Refusal(.authentication(certificateMismatchText), code: .identityChanged)

    /// The WebSocket is open: check the pin again (under an identity trust,
    /// the Core's `hello` is checked instead), start the heartbeat (the
    /// deadline has run since the dial), and read whatever arrived while
    /// opening.
    private func opened(_ gen: Int, certificateSHA256 digest: Data) async {
        guard var current = attempt, current.generation == gen, current.phase == .opening else {
            return
        }
        if case .certificate(let pin) = trust, !CertificatePin.matches(digest, pin: pin) {
            finish(gen, refusal: Self.identityChangedRefusal, retry: false)
            return
        }
        current.certificateSHA256 = digest
        if case .certificate = trust {
            current.identityVerified = true
        }
        if current.deadline == nil && !(current.transport is PreauthenticatedTransport) {
            connectDeadlineArm &+= 1
            current.deadlineDueMs = clock.nowMilliseconds + 30_000
            current.deadline = scheduleDeadline(gen, arm: connectDeadlineArm, after: Self.connectDeadline)
        }
        current.phase = .awaitingHello
        current.heartbeatToken += 1
        current.heartbeat = scheduleHeartbeat(gen, token: current.heartbeatToken)
        attempt = current
        if let preauthenticated = current.transport as? PreauthenticatedTransport {
            preauthenticated.onDeadline { [weak self] in await self?.leaseDeadlinePassed(gen) }
        }
        await pump(gen)
    }

    private func handle(_ event: LinkTransportEvent, generation gen: Int, routeId: Int) async {
        guard let current = attempt, current.generation == gen else {
            return
        }
        if routeId != current.routeId {
            if oldDrain?.routeId == routeId {
                switch event {
                case .pong: receivedPong(generation: gen, routeID: routeId)
                case .closed: retireOld()
                case .text: break
                }
            }
            return
        }
        switch event {
        case .text(let text):
            attempt?.inbound.append(text)
            if current.phase != .opening {
                await pump(gen)
            }
        case .pong:
            receivedPong(generation: gen, routeID: routeId)
        case .closed:
            if move?.generation == gen, move?.phase == .joined {
                commitMove(oldClosed: true)
                return
            }
            // While opening, the open's own result decides what happens.
            if current.phase != .opening {
                Self.logger.info("The link to the Core was lost")
                finish(gen, refusal: nil, retry: true)
            }
        }
    }

    private func receivedPong(generation gen: Int, routeID: Int) {
        if attempt?.generation == gen, attempt?.routeId == routeID,
           diagnosticPingsAwaitingPong > 0 {
            if diagnosticPingsAwaitingPong == 1, let sent = diagnosticPing,
               sent.attemptGeneration == gen, sent.routeID == routeID {
                diagnosticRoundTrip = StationDiagnosticsSnapshot.RoundTrip(
                    attemptGeneration: gen, routeID: routeID,
                    duration: ContinuousClock.now - sent.sentAt,
                    observedAtMilliseconds: clock.nowMilliseconds)
            }
            diagnosticPing = nil
            diagnosticPingsAwaitingPong -= 1
        }
        if let sent = attempt?.pingSentAt {
            roundTripSink.yield(ContinuousClock.now - sent)
        }
        attempt?.pingsAwaitingPong = 0
        attempt?.pingSentAt = nil
        attempt?.linkCheck?.cancel()
        attempt?.linkCheck = nil
    }

    /// Reads inbound messages one at a time, in arrival order, even when
    /// reading one waits on the authenticator.
    private func pump(_ gen: Int) async {
        guard let current = attempt, current.generation == gen, !current.pumping else {
            return
        }
        attempt?.pumping = true
        while let next = attempt, next.generation == gen, let text = next.inbound.first {
            attempt?.inbound.removeFirst()
            await receive(text, gen)
        }
        if isCurrent(gen) {
            attempt?.pumping = false
        }
    }

    private func receive(_ text: String, _ gen: Int) async {
        let message: LinkMessage
        do {
            message = try LinkCodec.decode(text)
        } catch {
            // A newer Core's new kind costs this app nothing (section 13).
            Self.logger.info("Ignoring a message from the Core the app cannot read: \(String(describing: error), privacy: .private)")
            return
        }
        guard attempt?.identityVerified == true || message.kind == .hello || message.kind == .sessionEnd else {
            // Before a paired Core has proved itself, only its hello or an
            // end is read; it sends nothing else before auth.result (5.1).
            Self.logger.info("Ignoring a \(message.kind.rawValue, privacy: .public) message from a Core not yet checked")
            return
        }
        guard message.kind.sentByStation else {
            Self.logger.info("Ignoring a \(message.kind.rawValue, privacy: .public) message, which only an app sends")
            return
        }
        switch message {
        case .pathSwitch:
            if move?.generation == gen, move?.phase == .joined {
                commitMove()
            }
        case .hello(let hello):
            await stationHello(hello, gen)
        case .authResult(let result):
            authResult(result, gen)
        case .sessionEnd(let end) where attempt?.identityVerified != true:
            // Words from a Core not yet checked are not shown: honour only
            // whether to try again (the connection cap), as a lost link.
            Self.logger.info("A Core not yet checked ended the connection")
            let retryable = end.retryable ?? false
            finish(gen, refusal: Refusal(.ended(Self.endedBeforeCheckText, retryable: retryable)), retry: retryable)
        case .sessionEnd(let end):
            eventSink.yield(.message(message))
            let retryable = end.retryable ?? false
            Self.logger.info("The Core ended the session (\(end.code ?? "no code", privacy: .public)): \(end.reason, privacy: .private)")
            let code = end.code.map(Refusal.Code.init(stationWireName:))
            finish(gen, refusal: Refusal(.ended(end.reason, retryable: retryable), code: code), retry: retryable)
        case .sessionHeld(let held):
            guard attempt?.phase == .receivingSnapshot, (agreedMinor ?? 0) >= 11,
                  authenticator.signsWithDeviceKey, features["sessionHolder"] == 1,
                  features["deviceAuth"] == 1, stationHello?.features?["sessionHolder"] == 1,
                  stationHello?.features?["deviceAuth"] == 1 else {
                finish(gen, refusal: Refusal(.ended("The Core sent a fifth-device question out of turn.", retryable: false),
                                            code: .protocolError), retry: false)
                return
            }
            if attempt?.heldQuestion == nil {
                pauseConnectDeadline()
                attempt?.heldDeadline = clock.schedule(after: Self.takeoverAnswerDeadline) { [weak self] in
                    await self?.heldDeadlinePassed(gen)
                }
            }
            attempt?.heldQuestion = held
            eventSink.yield(.message(message))
        case .capabilities(let capabilities):
            if attempt?.heldQuestion != nil {
                attempt?.heldDeadline?.cancel()
                attempt?.heldDeadline = nil
                attempt?.heldQuestion = nil
                resumeConnectDeadline(gen)
            }
            attempt?.mediaOffered = Self.offersMedia(capabilities, agreedMinor: agreedMinor ?? 0)
            attempt?.controlSwitchOffered = Self.offersControlSwitch(capabilities, agreedMinor: agreedMinor ?? 0)
            eventSink.yield(.message(message))
            // Media withdrawn while the schedule waited for it: the session
            // no longer uses media, so it is up now.
            if attempt?.resetAwaitsMedia == true, attempt?.mediaOffered == false {
                attempt?.resetAwaitsMedia = false
                policy.reset()
            }
        case .snapshotComplete:
            if attempt?.phase == .receivingSnapshot {
                // The inspected lease owns the original opening-through-
                // snapshot deadline. It must claim readiness under that
                // lease's lock before this session publishes .ready.
                if let lease = attempt?.transport as? PreauthenticatedTransport,
                   !lease.sessionReady() {
                    finish(gen, refusal: nil, retry: true)
                    return
                }
                eventSink.yield(.message(message))
                attempt?.phase = .ready
                connectDeadlineArm &+= 1
                attempt?.deadline?.cancel()
                attempt?.deadline = nil
                attempt?.deadlineDueMs = nil
                if waitsForMedia && attempt?.mediaOffered == true {
                    attempt?.resetAwaitsMedia = true
                } else {
                    policy.reset()
                }
                setState(.ready)
            }
        default:
            eventSink.yield(.message(message))
        }
    }

    private static func offersControlSwitch(_ capabilities: LinkMessage.Capabilities,
                                            agreedMinor: UInt16) -> Bool {
        guard agreedMinor >= 11 else { return false }
        return capabilities.properties.contains { entry in
            guard entry.name == "controlSwitchVersion", case .i64(let version) = entry.value else {
                return false
            }
            return version >= 1
        }
    }

    /// The Core's `hello`: agree a major or leave having sent nothing, then
    /// send the app's `hello` and sign in.
    private func stationHello(_ hello: LinkMessage.Hello, _ gen: Int) async {
        guard let current = attempt, current.generation == gen, current.phase == .awaitingHello else {
            Self.logger.info("Ignoring a second hello from the Core")
            return
        }
        // A paired Core proves it is the same one before anything is sent
        // or passed on: its hello names the key the app paired with, and
        // that key has bound the certificate this connection presented
        // (section 3.4).
        if case .identity(let publicKey) = trust {
            guard StationTrust.identityVerifies(hello.identity, publicKey: publicKey,
                                                certificateSHA256: current.certificateSHA256) else {
                Self.logger.warning("The Core's identity is not the one this app paired with; nothing was sent")
                finish(gen, refusal: Self.identityChangedRefusal, retry: false)
                return
            }
            attempt?.identityVerified = true
        }
        eventSink.yield(.message(.hello(hello)))
        let ours = LinkVersionPolicy.supportedMajors
        let theirs = hello.supportedMajors
        guard let agreed = LinkVersionPolicy.agree(ours: ours, theirs: theirs) else {
            Self.logger.warning("No link version shared with the Core (it supports \(theirs, privacy: .public), this app \(ours, privacy: .public))")
            finish(gen, refusal: Refusal(LinkVersionPolicy.refusal(station: theirs, app: ours)), retry: false)
            return
        }
        agreedMajor = agreed
        agreedMinor = min(LinkVersionPolicy.minor, hello.minor)
        stationHello = hello
        if hello.settingsSchema != Self.settingsSchema {
            Self.logger.info("The Core's settings schema is \(hello.settingsSchema), the app's \(Self.settingsSchema)")
        }

        let request: LinkMessage.AuthRequest
        do {
            request = try await authenticator.authRequest(stationHello: hello,
                                                          certificateSHA256: current.certificateSHA256)
        } catch {
            guard isCurrent(gen) else {
                return
            }
            let reason = (error as? StationAuthenticationError)?.reason ?? Self.signInFailedText
            finish(gen, refusal: Refusal(.authentication(reason)), retry: false)
            return
        }
        guard let now = attempt, now.generation == gen, now.phase == .awaitingHello else {
            return
        }
        if let preauthenticated = now.transport as? PreauthenticatedTransport,
           !(await preauthenticated.isAvailable()) {
            finish(gen, refusal: nil, retry: false)
            return
        }
        guard isCurrent(gen), attempt?.phase == .awaitingHello else { return }
        let ownHello = LinkMessage.Hello(major: agreed, minor: LinkVersionPolicy.minor,
                                         settingsSchema: Self.settingsSchema, peer: Self.peerName,
                                         majors: ours, features: features)
        do {
            guard try transmit(.hello(ownHello)), try transmit(.authRequest(request)) else {
                throw LinkSendError.notConnected
            }
        } catch {
            Self.logger.warning("Could not send the sign-in: \(String(describing: error), privacy: .public)")
            finish(gen, refusal: Refusal(.authentication(Self.signInFailedText)), retry: false)
            return
        }
        attempt?.phase = .authenticating
        setState(.authenticating)
    }

    private func authResult(_ result: LinkMessage.AuthResult, _ gen: Int) {
        guard attempt?.phase == .authenticating else {
            Self.logger.info("Ignoring a sign-in answer the app did not wait for")
            return
        }
        eventSink.yield(.message(.authResult(result)))
        if result.accepted {
            Self.logger.notice("Signed in to the Core")
            attempt?.phase = .receivingSnapshot
            setState(.receivingSnapshot)
            return
        }
        // A wrong credential is final; a lockout says it is retryable, so a
        // stranger's failed attempts never lock the operator out for good.
        let retryable = result.retryable ?? false
        Self.logger.warning("The Core refused the sign-in (\(result.code ?? "no code", privacy: .public)): \(result.reason, privacy: .private)")
        let code = result.code.map(Refusal.Code.init(stationWireName:))
        finish(gen, refusal: Refusal(.authentication(result.reason), code: code), retry: retryable)
    }

    /// Whether a capabilities set offers media at this agreed minor
    /// (link document section 6.2; the media control document).
    private static func offersMedia(_ capabilities: LinkMessage.Capabilities, agreedMinor: UInt16) -> Bool {
        guard agreedMinor >= 1 else {
            return false
        }
        return capabilities.properties.contains { entry in
            guard entry.name == "remoteMediaVersion" else {
                return false
            }
            switch entry.value {
            case .i64(let version), .enumeration(let version):
                return version >= 1
            default:
                return false
            }
        }
    }

    // MARK: Timers

    private func scheduleDeadline(_ gen: Int, arm: Int, after delay: Duration) -> any LinkTimer {
        clock.schedule(after: delay) { [weak self] in
            await self?.deadlinePassed(gen, arm: arm)
        }
    }

    private func armConnectDeadline(_ gen: Int, after delay: Duration) {
        connectDeadlineArm &+= 1
        let parts = delay.components
        let ms = parts.seconds * 1_000 + parts.attoseconds / 1_000_000_000_000_000
        attempt?.deadlineDueMs = clock.nowMilliseconds + ms
        attempt?.deadline = scheduleDeadline(gen, arm: connectDeadlineArm, after: delay)
    }

    private func pauseConnectDeadline() {
        if let due = attempt?.deadlineDueMs {
            attempt?.deadlineRemainingMs = max(0, due - clock.nowMilliseconds)
            attempt?.deadlineDueMs = nil
            connectDeadlineArm &+= 1
            attempt?.deadline?.cancel()
            attempt?.deadline = nil
        }
        (attempt?.transport as? PreauthenticatedTransport)?.pauseDeadline()
    }

    private func resumeConnectDeadline(_ gen: Int) {
        if let remaining = attempt?.deadlineRemainingMs {
            attempt?.deadlineRemainingMs = nil
            armConnectDeadline(gen, after: .milliseconds(remaining))
        }
        (attempt?.transport as? PreauthenticatedTransport)?.resumeDeadline()
    }

    private func heldDeadlinePassed(_ gen: Int) {
        guard attempt?.generation == gen, attempt?.heldQuestion != nil else { return }
        finish(gen, refusal: Refusal(.ended("The Core already has four devices connected.", retryable: false),
                                    code: .other("coreFull")), retry: false)
    }

    private func deadlinePassed(_ gen: Int, arm: Int) {
        guard let current = attempt, current.generation == gen, current.phase != .ready,
              connectDeadlineArm == arm, current.deadline != nil, current.deadlineDueMs != nil else {
            return
        }
        expireConnectDeadline(gen)
    }

    /// The inspected lease owns its own timer and invalidates callbacks
    /// under its lock, so it does not use the native session arm identity.
    private func leaseDeadlinePassed(_ gen: Int) {
        guard let current = attempt, current.generation == gen, current.phase != .ready else { return }
        expireConnectDeadline(gen)
    }

    private func expireConnectDeadline(_ gen: Int) {
        Self.logger.warning("The Core did not finish connecting in time")
        finish(gen, refusal: nil, retry: true)
    }

    private func scheduleHeartbeat(_ gen: Int, token: Int) -> any LinkTimer {
        clock.schedule(after: acceleratedHeartbeat ? Self.acceleratedHeartbeatInterval : Self.heartbeatInterval) {
            [weak self] in
            await self?.heartbeatDue(gen, token: token)
        }
    }

    private func heartbeatDue(_ gen: Int, token: Int) {
        guard var current = attempt, current.generation == gen,
              current.heartbeatToken == token else {
            return
        }
        if current.pingsAwaitingPong >= Self.maxMissedPongs {
            Self.logger.warning("The Core stopped answering pings")
            finish(gen, refusal: nil, retry: true)
            return
        }
        // A pong cannot say which ping it answers, so only a lone one is timed.
        current.pingSentAt = current.pingsAwaitingPong == 0 ? ContinuousClock.now : nil
        diagnosticPing = diagnosticPingsAwaitingPong == 0
            ? DiagnosticPing(attemptGeneration: gen, routeID: current.routeId, sentAt: ContinuousClock.now) : nil
        diagnosticPingsAwaitingPong += 1
        current.pingsAwaitingPong += 1
        current.heartbeatToken += 1
        current.heartbeat = scheduleHeartbeat(gen, token: current.heartbeatToken)
        attempt = current
        current.transport.ping()
    }

    /// A ping at once, outside the heartbeat's own ticks, with its own
    /// deadline: the phone's network changed under a ready session.
    private func checkLinkNow() {
        guard var current = attempt, current.phase == .ready else {
            return
        }
        let gen = current.generation
        Self.logger.notice("The network changed; checking the link to the Core")
        current.pingSentAt = current.pingsAwaitingPong == 0 ? ContinuousClock.now : nil
        diagnosticPing = diagnosticPingsAwaitingPong == 0
            ? DiagnosticPing(attemptGeneration: gen, routeID: current.routeId, sentAt: ContinuousClock.now) : nil
        diagnosticPingsAwaitingPong += 1
        current.pingsAwaitingPong += 1
        current.linkCheck?.cancel()
        current.linkCheck = clock.schedule(after: Self.linkCheckDeadline) { [weak self] in
            await self?.linkCheckDue(gen)
        }
        attempt = current
        current.transport.ping()
    }

    private func linkCheckDue(_ gen: Int) {
        guard let current = attempt, current.generation == gen, current.linkCheck != nil else {
            return
        }
        attempt?.linkCheck = nil
        guard current.pingsAwaitingPong > 0 else {
            return
        }
        Self.logger.warning("The Core did not answer a ping after the network changed")
        finish(gen, refusal: nil, retry: true)
    }

    // MARK: Ending

    /// Ends the attempt: closes the connection, reports the refusal, and
    /// either waits to redial or stops.
    private func finish(_ gen: Int, refusal: Refusal?, retry: Bool) {
        guard let current = attempt, current.generation == gen else {
            return
        }
        abandonMove()
        retireOld()
        connectDeadlineArm &+= 1
        current.deadline?.cancel()
        current.heldDeadline?.cancel()
        current.heartbeat?.cancel()
        current.linkCheck?.cancel()
        current.transport.close()
        attempt = nil
        diagnosticPing = nil
        diagnosticPingsAwaitingPong = 0
        diagnosticRoundTrip = nil
        if let refusal {
            eventSink.yield(.refused(refusal))
        }
        guard retry, let delay = policy.nextDelay() else {
            setState(.stopped)
            return
        }
        retryToken += 1
        let token = retryToken
        setState(.waitingToRetry(seconds: delay))
        guard !retriesHeld else {
            return
        }
        retryTimer = clock.schedule(after: .seconds(delay)) { [weak self] in
            await self?.retryDue(token)
        }
    }

    private func retryDue(_ token: Int) async {
        guard token == retryToken, case .waitingToRetry = state else {
            return
        }
        retryTimer = nil
        await startAttempt()
    }

    private func setState(_ next: State) {
        guard next != state else {
            return
        }
        state = next
        eventSink.yield(.stateChanged(next))
    }
}

/// A bounded ingress between synchronous Network callbacks and the session
/// actor. One drain task handles a burst, so packet rate cannot create an
/// unbounded number of tasks or enter the control text stream.
final class SessionBinaryIngress: @unchecked Sendable {
    private struct Packet {
        let frame: Data
        let generation: Int
        let routeId: Int
    }
    private let lock = NSLock()
    private var queue: [Packet] = []
    private var bytes = 0
    private var draining = false
    private var handler: (@Sendable (Data, Int, Int) async -> Void)?
    var queued: (count: Int, bytes: Int) { lock.withLock { (queue.count, bytes) } }

    func setHandler(_ handler: @escaping @Sendable (Data, Int, Int) async -> Void) {
        lock.withLock { self.handler = handler }
    }

    func offer(_ frame: Data, generation: Int, routeId: Int) {
        guard (18...1501).contains(frame.count), frame.first == 2 else { return }
        let shouldStart = lock.withLock { () -> Bool in
            queue.append(Packet(frame: frame, generation: generation, routeId: routeId))
            bytes += frame.count
            while queue.count > 64 || bytes > 24_576 {
                bytes -= queue.removeFirst().frame.count
            }
            guard !draining else { return false }
            draining = true
            return true
        }
        if shouldStart { Task { await drain() } }
    }

    private func drain() async {
        while true {
            let next = lock.withLock { () -> (Packet, (@Sendable (Data, Int, Int) async -> Void)?)? in
                guard !queue.isEmpty else { draining = false; return nil }
                let packet = queue.removeFirst()
                bytes -= packet.frame.count
                return (packet, handler)
            }
            guard let (packet, handler) = next else { return }
            await handler?(packet.frame, packet.generation, packet.routeId)
        }
    }
}
