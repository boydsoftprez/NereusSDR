// NereusSDR for iOS: one dial of a control connection through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// One attempt of ``RendezvousDialer``, from the service's hello to the
/// channel opening or the attempt failing. It ends once: every way out
/// (the result, a failure, the deadline, the caller's cancellation) goes
/// through ``finish(_:)``.
actor ControlDial {
    struct Settings: Sendable {
        let servers: [RendezvousServer]
        let stationId: String
        let device: DeviceIdentity
        let relayAllowed: Bool
        let clock: any LinkClock
        let deadline: Duration
        let makeTransport: RendezvousTransportFactory
        let localFamilies: @Sendable () -> AddressFamilies
        let resolve: RendezvousDialer.Resolver
        let makeRelaySocket: RelaySocketFactory?
        let passesCandidate: @Sendable (String) -> Bool
        /// Given every candidate the Core has offered so far, its answer's
        /// SDP first and then each it trickled, each time that grows, from
        /// the answer on and whether or not the channel then opens: the
        /// phone keeps the Core's global host addresses from them
        /// (R-IOS-16). Never logged.
        var coreCandidatesSeen: @Sendable ([String]) -> Void = { _ in }
        /// Tests only: runs after the Core's answer has come and before it
        /// is set on the peer, and when the service's going has been
        /// weighed; no-ops otherwise.
        var beforeApplyingAnswer: @Sendable () async -> Void = {}
        var serviceGoneWeighed: @Sendable () -> Void = {}
    }

    /// The open channel with the Core's DTLS certificate, the peer that
    /// carries it, and what the connection was when it opened: whether its
    /// selected path went through a relay (nil when no pair was selected)
    /// and the media connection's ICE settings for that path. Both are read
    /// while the peer is open, since a closed peer has no selected pair.
    typealias Outcome = (connection: ControlChannelConnection, peer: ControlPeer, relayed: Bool?,
                         rank: Int?, mediaIce: IceSettings, mediaRelay: RelayRouteContext?)

    private static let logger = Logger(subsystem: "NereusSDR", category: "rendezvous.dial")

    private let settings: Settings
    private let onEvent: @Sendable (ControlChannelEvent) -> Void

    private var result: CheckedContinuation<Outcome, Error>?
    private var done = false
    private var cancelled = false
    private var deadlineTimer: (any LinkTimer)?
    private var client: RendezvousClient?
    private var peer: ControlPeer?
    private var relayContext: RelayRouteContext?
    private var controlClaim: RelayICEClaim?
    private var relayIssue: String?
    private var relayTerminated = false
    private var tasks: [Task<Void, Never>] = []
    private var offer: String?
    private var offerWaiter: CheckedContinuation<String?, Never>?
    private var peerFailed = false
    /// The Core's answer has been set on the peer.
    private var answered = false
    /// The Core's answer has come, whether or not it has been set on the
    /// peer yet: the desktop's rule for outliving the service keys on the
    /// answer and candidates having come, not on this phone having applied
    /// them, so a slow phone keeps a dial the service leaves after both.
    private var answerReceived = false
    /// The Core's end of candidates (an empty candidate) came.
    private var coreCandidatesEnded = false
    /// The Core's candidates that came before its answer was set.
    private var earlyCandidates: [String] = []
    /// This phone's candidates, held until the introduction is live.
    private var ownCandidates: [String] = []
    private var ownCandidatesEnded = false
    /// Every candidate this phone gathered and every one the Core sent, as
    /// lines, and the pair chosen when the channel opened: the attempt
    /// record's evidence (``IceCandidateEvidence``). Never logged.
    private var phoneCandidateLines: [String] = []
    private var coreCandidateLines: [String] = []
    /// The Core's answer SDP, once it has come.
    private var answerDescription: String?
    private var chosenPair: (local: String, remote: String)?
    private var evidence: IceCandidateEvidence?

    var lastRelayIssue: String? { relayIssue }
    /// What each end offered and how each pair went, once the dial ended.
    var iceEvidence: IceCandidateEvidence? { evidence }

    init(settings: Settings, onEvent: @escaping @Sendable (ControlChannelEvent) -> Void) {
        self.settings = settings
        self.onEvent = onEvent
    }

    /// Runs the attempt; returns the open channel with the Core's DTLS
    /// certificate, and the peer that carries it.
    func run() async throws -> Outcome {
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Outcome, Error>) in
                begin(continuation)
            }
        } onCancel: {
            Task { await self.cancel() }
        }
    }

    private func begin(_ continuation: CheckedContinuation<Outcome, Error>) {
        result = continuation
        guard !cancelled else {
            finish(.failure(CancellationError()))
            return
        }
        deadlineTimer = settings.clock.schedule(after: settings.deadline) { [weak self] in
            await self?.deadlinePassed()
        }
        tasks.append(Task { await self.steps() })
    }

    private func cancel() {
        cancelled = true
        finish(.failure(CancellationError()))
    }

    private func deadlinePassed() {
        guard !done else {
            return
        }
        Self.logger.info("The connection through the remote access service did not open in time")
        finish(.failure(RendezvousDialError.notOpenedInTime))
    }

    // MARK: The steps

    private func steps() async {
        do {
            let client = RendezvousClient(servers: settings.servers, clock: settings.clock,
                                          transportFactory: settings.makeTransport)
            self.client = client
            guard !done else {
                await client.close()
                return
            }
            // 1. The service's hello and its STUN server, by this phone's families.
            let stun = try await client.connect()
            guard !done else {
                return
            }
            let stunFamilies = await settings.resolve(IceSettings.hostNames(stun))
            var ice = IceSettings(stunUrls: stun, relayAllowed: settings.relayAllowed,
                                  local: settings.localFamilies(), hosts: stunFamilies)
            guard !done else {
                return
            }

            // 2. The control peer and its offer, then the introduction.
            let onEvent = self.onEvent
            let peer = try ControlPeer(ice: ice, onChannel: onEvent)
            self.peer = peer
            tasks.append(Task { [weak self] in
                for await signal in peer.signals {
                    await self?.peerSignal(signal)
                }
            })
            tasks.append(Task { [weak self] in
                for await event in client.events {
                    await self?.serviceEvent(event)
                }
            })
            guard let offer = await waitForOffer(), !done else {
                if !done {
                    finish(.failure(RendezvousDialError.notConnected))
                }
                return
            }
            let answer = try await client.introduce(stationId: settings.stationId, device: settings.device,
                                                    offer: offer)
            answerReceived = true
            answerDescription = answer.sdp
            reportCoreCandidates()
            await settings.beforeApplyingAnswer()
            guard !done else {
                return
            }

            // 3. The answer, the relay by this phone's families, and gathering.
            do {
                try peer.acceptAnswer(answer.sdp)
            } catch ControlPeer.PeerError.notAControlDescription {
                Self.logger.info("The Core's answer is not a control connection's")
                finish(.failure(RendezvousDialError.unusableAnswer))
                return
            }
            answered = true
            for candidate in earlyCandidates {
                addCoreCandidate(candidate)
            }
            earlyCandidates = []
            if let claim = controlClaim { _ = peer.addRemoteCandidate(claim.candidate) }
            let relayFamilies = await settings.resolve(IceSettings.hostNames(answer.turn?.urls ?? []))
            guard !done else {
                return
            }
            ice.addHostFamilies(relayFamilies)
            ice.setRelay(answer.turn, families: 1)
            try peer.gather(with: ice)
            // Candidates reported before the introduction was live go now.
            await flushOwnCandidates()
        } catch let error as RendezvousError {
            finish(.failure(RendezvousDialError.service(error)))
        } catch is CancellationError {
            finish(.failure(CancellationError()))
        } catch {
            Self.logger.info("The control connection could not be made: \(String(describing: error), privacy: .public)")
            finish(.failure(RendezvousDialError.notConnected))
        }
    }

    private func waitForOffer() async -> String? {
        if let offer {
            return offer
        }
        if peerFailed || done {
            return nil
        }
        return await withCheckedContinuation { (continuation: CheckedContinuation<String?, Never>) in
            offerWaiter = continuation
        }
    }

    // MARK: What the peer and the service report

    private func peerSignal(_ signal: ControlPeer.Signal) async {
        guard !done else {
            return
        }
        switch signal {
        case .offer(let sdp):
            offer = sdp
            offerWaiter?.resume(returning: sdp)
            offerWaiter = nil
        case .candidate(let candidate):
            phoneCandidateLines.append(candidate)
            ownCandidates.append(candidate)
            await flushOwnCandidates()
        case .gatheringComplete:
            ownCandidatesEnded = true
            await flushOwnCandidates()
        case .opened:
            opened()
        case .failed:
            peerFailed = true
            offerWaiter?.resume(returning: nil)
            offerWaiter = nil
            if offer != nil {
                Self.logger.info("The control connection failed before it opened")
                finish(.failure(RendezvousDialError.notConnected))
            }
        }
    }

    /// Sends this phone's candidates, and then its end of candidates, once
    /// the introduction is live (after the Core's answer).
    private func flushOwnCandidates() async {
        guard answered, let client else {
            return
        }
        let candidates = ownCandidates.filter(settings.passesCandidate)
        ownCandidates = []
        for candidate in candidates {
            await client.sendCandidate(candidate)
        }
        if ownCandidatesEnded {
            ownCandidatesEnded = false
            await client.sendCandidate("")
        }
    }

    private func serviceEvent(_ event: RendezvousClient.Event) async {
        guard !done else {
            return
        }
        switch event {
        case .candidate, .introductionEnded, .connectionLost:
            // The client passes these on only once the Core's answer has
            // come (an introduction ended or lost before it fails the
            // introduce itself), so the answer has arrived even when this
            // task has not yet seen introduce return.
            answerReceived = true
        case .relayGrant, .serviceError, .mailbox, .mailboxClosed:
            break
        }
        switch event {
        case .candidate(let candidate):
            if !candidate.isEmpty {
                coreCandidateLines.append(candidate)
                reportCoreCandidates()
            }
            if candidate.isEmpty {
                coreCandidatesEnded = true
            } else if answered {
                addCoreCandidate(candidate)
            } else {
                earlyCandidates.append(candidate)
            }
        case .introductionEnded(let code):
            serviceGone("the introduction ended (\(code))")
        case .connectionLost:
            serviceGone("the connection to the remote access service ended")
        case .serviceError(let error):
            Self.logger.info("The remote access service said \(error.code, privacy: .public)")
        case .relayGrant(let grant):
            await receiveGrant(grant)
        case .mailbox, .mailboxClosed:
            break
        }
    }

    /// Open WSS immediately on the grant. Claiming the loopback candidate
    /// and admitting it to ICE may complete after answer application.
    private func receiveGrant(_ grant: RelayGrant) async {
        guard settings.relayAllowed, relayContext == nil, !done else { return }
        let context: RelayRouteContext
        do {
            context = try RelayRouteContext(grant: grant, socketFactory: settings.makeRelaySocket,
                                             clock: settings.clock) { [weak self] event in
                await self?.relayEvent(event)
            }
        } catch {
            relayIssue = "The web relay could not open its local socket."
            return
        }
        relayContext = context
        await context.start()
        do {
            let claim = try await context.claimControl()
            guard !done, let peer, peer.attachRelay(context, claim: claim) else {
                await context.releaseControl(claim)
                await context.cancel()
                return
            }
            controlClaim = claim
            if answered { _ = peer.addRemoteCandidate(claim.candidate) }
        } catch {
            if !done, relayIssue == nil {
                relayIssue = "The web relay could not offer a control candidate."
            }
            await context.cancel()
            if relayContext === context { relayContext = nil }
        }
    }

    private func relayEvent(_ event: RelayLegEvent) {
        guard !done else { return }
        switch event {
        case .ended(let code):
            relayTerminated = true
            relayIssue = Self.words(for: code)
        case .reconnectTimedOut:
            relayTerminated = true
            relayIssue = "The connection to the web relay was lost."
        case .ready, .peer:
            break
        }
    }

    private static func words(for code: RelayEndCode) -> String? {
        // Plain operator text matches frozen Core RelayLeg.cpp:600-644
        // [@0b41e581]; no token, grant URL, or candidate enters a log.
        switch code {
        case .protocolError: "The connection through the web relay failed. Updating the app or the Core may help."
        case .timeout: "The web relay did not answer in time. Trying again."
        case .badToken: "The web relay did not accept this connection. Try connecting again."
        case .expired: "The web relay's permission ran out. Try connecting again."
        case .ended, .unknown: "That relayed connection has ended. Try connecting again."
        case .full: "The web relay is busy. Trying again shortly."
        case .tooManyConnections: "Too many connections from this network to the web relay. Trying again shortly."
        case .tooManySessions: "This Core already has as many connections through the web relay as it can. Try again shortly."
        case .peerGone: "The other end left the web relay."
        case .shuttingDown: "The web relay is restarting. Trying again."
        case .replaced, .idle: nil
        }
    }

    /// Hands every candidate the Core has offered so far to
    /// ``Settings/coreCandidatesSeen``, the answer's SDP first.
    private func reportCoreCandidates() {
        settings.coreCandidatesSeen((answerDescription.map { [$0] } ?? []) + coreCandidateLines)
    }

    private func addCoreCandidate(_ candidate: String) {
        guard settings.passesCandidate(candidate) else {
            return
        }
        peer?.addRemoteCandidate(candidate)
    }

    /// The introduction or the connection to the service ended. Once the
    /// Core's answer and the end of its candidates have both come (the
    /// answer need not be applied yet: applying it, or failing to, ends the
    /// dial on its own in ``steps()``), this
    /// phone holds everything the service would carry from the Core and the
    /// connectivity checks finish without it, so the dial runs on to its
    /// deadline; before that, what the connection still needs cannot come,
    /// and the dial ends now so the next goes through the service again.
    private func serviceGone(_ why: String) {
        defer { settings.serviceGoneWeighed() }
        if answerReceived && coreCandidatesEnded {
            Self.logger.info("After the Core's answer and candidates, \(why, privacy: .public); the connection can still open")
            return
        }
        Self.logger.info("Before the Core's answer and candidates came, \(why, privacy: .public)")
        finish(.failure(RendezvousDialError.serviceGone))
    }

    private func opened() {
        guard let peer else {
            return
        }
        guard let certificate = peer.remoteCertificateSHA256 else {
            Self.logger.warning("The control channel opened without a DTLS certificate this phone could read")
            finish(.failure(RendezvousDialError.noCertificate))
            return
        }
        chosenPair = peer.selectedCandidateLines
        let relayed = peer.selectedPathIsRelayed
        let rank = peer.selectedPathRank
        Self.logger.notice("The control connection through the remote access service opened (relayed: \(relayed == true, privacy: .public))")
        finish(.success((connection: ControlChannelConnection(channel: peer, certificateSHA256: certificate),
                         peer: peer, relayed: relayed, rank: rank,
                         mediaIce: peer.mediaIceSettings(controlPathRelayed: relayed),
                         mediaRelay: controlClaim == nil || relayTerminated ? nil : relayContext)))
    }

    // MARK: Ending

    /// Ends the attempt once. On success the service is left (the session
    /// runs over the connection, never through the service); on failure
    /// the peer is closed too.
    private func finish(_ outcome: Result<Outcome, Error>) {
        guard !done else {
            return
        }
        done = true
        let ending: IceCandidateEvidence.Ending
        switch outcome {
        case .success:
            ending = .connected
        case .failure(let error):
            ending = error is CancellationError ? .stopped : .failed
        }
        evidence = IceCandidateEvidence(phone: phoneCandidateLines, core: coreCandidateLines,
                                        chosen: chosenPair ?? peer?.selectedCandidateLines, ending: ending)
        deadlineTimer?.cancel()
        deadlineTimer = nil
        offerWaiter?.resume(returning: nil)
        offerWaiter = nil
        for task in tasks {
            task.cancel()
        }
        tasks = []
        let client = self.client
        Task { await client?.close() }
        let oldPeer = peer
        let oldContext = relayContext
        let oldClaim = controlClaim
        peer = nil
        relayContext = nil
        controlClaim = nil
        let continuation = result
        result = nil
        let finalOutcome: Result<Outcome, Error>
        if case .failure(let error) = outcome, let relayIssue,
           let dialError = error as? RendezvousDialError,
           dialError == .notConnected || dialError == .notOpenedInTime {
            finalOutcome = .failure(dialError == .notOpenedInTime
                                    ? RendezvousDialError.webRelayTimedOut(relayIssue)
                                    : RendezvousDialError.webRelay(relayIssue))
        } else {
            finalOutcome = outcome
        }
        if case .failure = finalOutcome {
            Task {
                if let oldPeer { await oldPeer.closeWhenDeleted() }
                if let oldContext {
                    if let oldClaim { await oldContext.releaseControl(oldClaim) }
                    await oldContext.cancel()
                }
                continuation?.resume(with: finalOutcome)
            }
        } else {
            if oldClaim == nil, let oldContext { Task { await oldContext.cancel() } }
            continuation?.resume(with: finalOutcome)
        }
    }
}
