// NereusSDR for iOS: the dial through the remote access service, against a scripted service and a Core's end in this process
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// iPhone app plan Task 28a (R-IOS-16): the phone's dial, as the desktop's
/// `RendezvousDialer` behaves (tst_rendezvous_client): the service plays by
/// script, and the Core's end is a real libdatachannel answerer on
/// 127.0.0.1, so the control channel really opens. Serialized, since the
/// peers share libdatachannel's process-wide state.
@Suite(.serialized) struct RendezvousDialerTests {
    static let serverA = RendezvousServer(host: "rv.conformance.invalid")
    static let serverB = RendezvousServer(host: "rv2.conformance.invalid")

    /// A dialer on the scripted service, to a Core id made for the run.
    struct Rig {
        let service = RendezvousTestService()
        let stationId: String
        let device: DeviceIdentity
        let dialer: RendezvousDialer
        let clock: (any LinkClock)?

        /// With `answerAfterServiceGone`, the dial applies the Core's
        /// answer only once it has weighed the service going away.
        init(servers: [RendezvousServer] = [RendezvousDialerTests.serverA], clock: (any LinkClock)? = nil,
             deadline: Duration = RendezvousDialer.dialDeadline,
             answerAfterServiceGone gate: ServiceGoneGate? = nil,
             relaySocketFactory: RelaySocketFactory? = nil,
             relayAllowed: Bool = true) throws {
            stationId = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
            device = try DeviceIdentity.load(store: InMemoryKeyStore())
            self.clock = clock
            if let gate {
                dialer = RendezvousDialer(servers: servers, stationId: stationId, device: device,
                                          relayAllowed: relayAllowed, clock: clock ?? SystemLinkClock(),
                                          deadline: deadline, transportFactory: service.factory,
                                          relaySocketFactory: relaySocketFactory, localFamilies: { .ipv4Only },
                                          resolve: { _ in [:] }, passesCandidate: { _ in true },
                                          beforeApplyingAnswer: { await gate.waitUntilWeighed() },
                                          serviceGoneWeighed: { gate.weighed() })
            } else {
                dialer = RendezvousDialer(servers: servers, stationId: stationId, device: device,
                                          relayAllowed: relayAllowed, clock: clock ?? SystemLinkClock(),
                                          deadline: deadline, transportFactory: service.factory,
                                          relaySocketFactory: relaySocketFactory, localFamilies: { .ipv4Only },
                                          resolve: { _ in [:] })
            }
        }
    }

    /// Opens once the dial has weighed the service going away: the order a
    /// slow phone sees, where the service's news arrives before the phone
    /// has applied the Core's answer.
    final class ServiceGoneGate: @unchecked Sendable {
        private let lock = NSLock()
        private var open = false
        private var waiters: [CheckedContinuation<Void, Never>] = []

        func weighed() {
            let resumed = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
                open = true
                defer { waiters = [] }
                return waiters
            }
            for waiter in resumed {
                waiter.resume()
            }
        }

        func waitUntilWeighed() async {
            await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
                let already = lock.withLock { () -> Bool in
                    if open {
                        return true
                    }
                    waiters.append(continuation)
                    return false
                }
                if already {
                    continuation.resume()
                }
            }
        }
    }

    /// The channel's events, for a test to read.
    final class ChannelEvents: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [ControlChannelEvent] = []

        func append(_ event: ControlChannelEvent) {
            lock.withLock { stored.append(event) }
        }

        var events: [ControlChannelEvent] { lock.withLock { stored } }
    }

    final class RelayOpenRecord: @unchecked Sendable {
        private let lock = NSLock()
        private var count = 0
        private var closedCount = 0
        func opened() { lock.withLock { count += 1 } }
        func closed() { lock.withLock { closedCount += 1 } }
        var openings: Int { lock.withLock { count } }
        var closings: Int { lock.withLock { closedCount } }
    }

    actor QuietRelaySocket: RelayBinarySocket {
        private let events: AsyncStream<RelaySocketEvent>
        private let sink: AsyncStream<RelaySocketEvent>.Continuation
        private let record: RelayOpenRecord?
        init(record: RelayOpenRecord? = nil) {
            self.record = record
            (events, sink) = AsyncStream.makeStream(of: RelaySocketEvent.self)
        }
        func nextEvent() async -> RelaySocketEvent? {
            var iterator = events.makeAsyncIterator()
            return await iterator.next()
        }
        func send(_ message: Data) async throws { _ = message }
        func emit(_ data: Data) { sink.yield(.binary(data)) }
        func close() async { record?.closed(); sink.finish() }
    }

    /// Plays the service and the Core through the answer. Returns the
    /// service connection and the introduction the phone sent.
    private func answer(_ rig: Rig, core: TestControlAnswerer, connection index: Int = 0)
        async throws -> (RendezvousTestService.Connection, RendezvousMessage.Introduce) {
        let connection = try #require(await rig.service.connection(index))
        connection.greet()
        guard case .introduce(let introduce)? = await connection.nextMessage() else {
            throw TestControlAnswerer.TimedOut(description: "the introduction")
        }
        let answer = try await core.answer(introduce.offer)
        connection.deliver(.answer(sdp: answer, turn: nil))
        return (connection, introduce)
    }

    /// Relays the phone's candidates to the Core's end until the phone ends
    /// them (an empty candidate), or until cancelled.
    private func relayPhoneCandidates(_ connection: RendezvousTestService.Connection,
                                      to core: TestControlAnswerer) -> Task<[String], Never> {
        Task {
            var seen: [String] = []
            while !Task.isCancelled {
                guard let message = await connection.nextMessage(within: .seconds(1)) else {
                    continue
                }
                if case .candidate(let candidate) = message {
                    seen.append(candidate)
                    if candidate.isEmpty {
                        break
                    }
                    core.add(candidate)
                }
            }
            return seen
        }
    }

    /// The Core's candidates, then the end of them.
    private func sendCoreCandidates(_ connection: RendezvousTestService.Connection,
                                    from core: TestControlAnswerer) async throws {
        for candidate in try await core.firstCandidates() {
            connection.deliver(.candidate(candidate))
        }
        connection.deliver(.candidate(""))
    }

    // MARK: The dial

    /// The whole dial: the service's hello, a signed introduction carrying
    /// an offer of one application line and no candidate, the answer, the
    /// candidates both ways, then the channel opens with the certificate
    /// the Core presented in DTLS and the phone leaves the service. Opening
    /// can retire the introduction before gathering ends.
    @Test func aDialOpensTheControlChannelAndLeavesTheService() async throws {
        let rig = try Rig()
        let core = try TestControlAnswerer()
        defer { core.close() }
        let events = ChannelEvents()
        let dialling = Task { try await rig.dialer.dial { events.append($0) } }

        let (connection, introduce) = try await answer(rig, core: core)
        #expect(introduce.id == rig.stationId)
        #expect(introduce.device == rig.device.id)
        #expect(ControlPeer.isControlDescription(introduce.offer))
        #expect(!introduce.offer.contains("a=candidate"))
        let relay = relayPhoneCandidates(connection, to: core)
        try await sendCoreCandidates(connection, from: core)

        let opened = try await dialling.value
        #expect(opened.certificateSHA256 == core.certificateSHA256)
        #expect(opened.certificateSHA256.count == 32)
        #expect(await connection.waitUntilClosed(within: .seconds(10)), "the phone left the service")
        relay.cancel()
        _ = await relay.value
        let phoneCandidates = connection.allSent.compactMap { text -> String? in
            guard case .candidate(let candidate)? = try? RendezvousMessage.decode(text, direction: .toService)
            else { return nil }
            return candidate
        }
        let offeredCandidates = phoneCandidates.filter { !$0.isEmpty }
        #expect(!offeredCandidates.isEmpty, "the phone offered candidates before the channel opened")
        #expect(offeredCandidates.allSatisfy { $0.hasPrefix("candidate:") })

        // The channel carries the session's frames both ways.
        #expect(opened.channel.send(ControlChannelFraming.ping(id: 1)))
        for _ in 0..<400 where core.messages.isEmpty {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(core.messages == [ControlChannelFraming.ping(id: 1)])
        #expect(core.channels.count == 1)
        #expect(core.channels.first.flatMap { core.bridge.label(ofChannel: $0) } == "control")
        let tried = try #require(rig.dialer.attemptTry)
        #expect(tried.path == .direct && tried.address == Self.serverA.host && tried.outcome == .connected)
        // It went through the service, and says what each end offered and
        // which pair carried it (R-IOS-16, 2026-09-29).
        #expect(tried.throughService)
        let ice = try #require(tried.ice)
        #expect(!ice.phone.isEmpty && !ice.core.isEmpty)
        #expect(ice.pairs.filter { $0.outcome == .used }.count == 1)
        #expect(ice.pairs.allSatisfy { $0.outcome == .used || $0.outcome == .notUsed })
        let media = try #require(rig.dialer.mediaIceSettings())
        #expect(media.relays.isEmpty && media.relayKnown)
        // The pair libdatachannel itself selected reads as this open
        // channel's route, in the exact lines the library writes.
        var observation = opened.channel.selectedRouteObservation
        for _ in 0..<400 where observation == .unavailable(.notReady) {
            try await Task.sleep(for: .milliseconds(5))
            observation = opened.channel.selectedRouteObservation
        }
        guard case .available(let route) = observation else {
            Issue.record("the open control channel's selected pair read as \(observation)")
            opened.channel.close()
            return
        }
        #expect(route.kind == .direct)
        #expect(route.transport == .iceUDP)
        #expect(route.coreEndpoint == route.selectedEndpoint)
        opened.channel.close()
    }

    /// When gathering finishes while the introduction is still live, the
    /// phone sends the empty end-of-candidates message. Keep the Core from
    /// learning a phone address until that message arrives, so opening
    /// cannot retire the introduction first.
    @Test func gatheringEndsBeforeOpeningSendsTheTerminalCandidate() async throws {
        let rig = try Rig()
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        defer { dialling.cancel() }
        let (connection, _) = try await answer(rig, core: core)

        var phoneCandidates: [String] = []
        for _ in 0..<65 {
            let message = try #require(await connection.nextMessage(within: .seconds(5)))
            guard case .candidate(let candidate) = message else {
                Issue.record("the phone sent a message other than an ICE candidate")
                return
            }
            phoneCandidates.append(candidate)
            if candidate.isEmpty { break }
        }
        guard phoneCandidates.last == "" else {
            Issue.record("the phone did not end its candidates while the introduction stayed live")
            return
        }
        let offeredCandidates = phoneCandidates.filter { !$0.isEmpty }
        #expect(!offeredCandidates.isEmpty)
        #expect(offeredCandidates.allSatisfy { $0.hasPrefix("candidate:") })
        for candidate in offeredCandidates { core.add(candidate) }
        try await sendCoreCandidates(connection, from: core)

        let opened = try await dialling.value
        #expect(opened.certificateSHA256 == core.certificateSHA256)
        #expect(await connection.waitUntilClosed(within: .seconds(10)))
        opened.channel.close()
    }

    /// A valid web grant is independent of TURN. WSS opening starts even
    /// while answer application is held, and the direct ICE path remains
    /// usable when that relay socket never reaches READY.
    @Test func turnNullWebGrantOpensBeforeAnswerApplication() async throws {
        let gate = ServiceGoneGate()
        let opens = RelayOpenRecord()
        let rig = try Rig(answerAfterServiceGone: gate,
                          relaySocketFactory: { _ in
                              opens.opened()
                              return QuietRelaySocket(record: opens)
                          })
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "validGrantToken", expires: 2_000_000_000)
        connection.deliver(.relayGrant(grant))
        for _ in 0..<400 where opens.openings == 0 {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(opens.openings == 1, "the relay opened while the answer was held")
        gate.weighed()
        let relay = relayPhoneCandidates(connection, to: core)
        try await sendCoreCandidates(connection, from: core)
        let opened = try await dialling.value
        #expect(opened.certificateSHA256 == core.certificateSHA256)
        #expect(await connection.waitUntilClosed(within: .seconds(10)))
        #expect(opens.closings == 0, "the grant-bound leg outlives the introduction")
        let context = try #require(rig.dialer.mediaRelayContext())
        let peer = try #require(opened.channel as? ControlPeer)
        #expect(peer.admittedRelayCandidateCount == 1)
        #expect(rig.dialer.lastPathRank == 2)
        #expect(rig.dialer.lastPathRelayed == false)
        await peer.closeWhenDeleted()
        #expect(opens.closings == 0, "control retirement leaves the route context alive")
        let mediaClaim = try await context.claimMedia(connectionId: UUID())
        #expect(mediaClaim.lane == .media)
        await context.releaseMedia(mediaClaim)
        relay.cancel()
        _ = await relay.value
    }

    @Test func deniedGrantDoesNotOpenWebRelay() async throws {
        let opens = RelayOpenRecord()
        let rig = try Rig(relaySocketFactory: { _ in opens.opened(); return QuietRelaySocket() },
                          relayAllowed: false)
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "deniedGrantToken", expires: 2_000_000_000)
        connection.deliver(.relayGrant(grant))
        let relay = relayPhoneCandidates(connection, to: core)
        try await sendCoreCandidates(connection, from: core)
        let opened = try await dialling.value
        #expect(opens.openings == 0)
        #expect(rig.dialer.mediaRelayContext() == nil)
        opened.channel.close()
        relay.cancel()
        _ = await relay.value
    }

    @Test func terminalWebRelayWordsSurviveDirectIceSuccess() async throws {
        let opens = RelayOpenRecord()
        let socket = QuietRelaySocket(record: opens)
        let rig = try Rig(relaySocketFactory: { _ in opens.opened(); return socket })
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "badGrantToken", expires: 2_000_000_000)
        connection.deliver(.relayGrant(grant))
        for _ in 0..<400 where opens.openings == 0 {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(opens.openings == 1)
        await socket.emit(Data([0x83]) + Data("badToken".utf8))
        let relay = relayPhoneCandidates(connection, to: core)
        try await sendCoreCandidates(connection, from: core)
        let opened = try await dialling.value
        #expect(rig.dialer.lastRelayIssue == "The web relay did not accept this connection. Try connecting again.")
        #expect(rig.dialer.lastError == nil)
        #expect(rig.dialer.lastPathRank == 2)
        opened.channel.close()
        relay.cancel()
        _ = await relay.value
    }

    @Test func cancelledGrantClosesTheLegBeforeAnswerApplication() async throws {
        let gate = ServiceGoneGate()
        let opens = RelayOpenRecord()
        let rig = try Rig(answerAfterServiceGone: gate,
                          relaySocketFactory: { _ in
                              opens.opened()
                              return QuietRelaySocket(record: opens)
                          })
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        let grant = try RelayGrant(urlString: "wss://rv.conformance.invalid/v1/relay",
                                   token: "cancelGrantToken", expires: 2_000_000_000)
        connection.deliver(.relayGrant(grant))
        for _ in 0..<400 where opens.openings == 0 {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(opens.openings == 1)
        dialling.cancel()
        gate.weighed()
        await #expect(throws: CancellationError.self) { try await dialling.value }
        for _ in 0..<400 where opens.closings == 0 {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(opens.closings >= 1)
        #expect(rig.dialer.mediaRelayContext() == nil)
        connection.deliver(.relayGrant(grant))
        #expect(opens.openings == 1, "a late grant cannot reopen a cancelled generation")
    }

    /// Task 28 tail, the four rows of the desktop's
    /// aDialAfterTheCoresAnswerAndCandidatesOutlivesTheService: once the
    /// Core's answer and the end of its candidates have come, the service
    /// going away (the Core left it, or this phone's connection dropped)
    /// leaves the dial running, and the channel opens from the checks alone.
    @Test(arguments: [true, false])
    func aDialAfterTheAnswerAndCandidatesOutlivesTheService(coreLeft: Bool) async throws {
        let rig = try Rig()
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        try await sendCoreCandidates(connection, from: core)
        // None of the phone's candidates is ever relayed: the Core learns
        // the phone's address from its connectivity checks.
        if coreLeft {
            connection.deliver(.introductionEnd(code: "stationLeft"))
        } else {
            connection.drop()
        }
        let opened = try await dialling.value
        #expect(opened.certificateSHA256 == core.certificateSHA256)
        opened.channel.close()
    }

    /// The rule keys on the answer and the Core's candidates having come,
    /// not on this phone having applied the answer (Task 56, a load
    /// finding: a slow phone had not yet set the answer on its peer when the
    /// service left, and failed a dial the desktop keeps). Here the dial is
    /// held from applying the answer until it has weighed the service going
    /// away, the order a slow phone sees; the channel still opens.
    @Test(arguments: [true, false])
    func aDialOutlivesTheServiceBeforeItHasAppliedTheAnswer(coreLeft: Bool) async throws {
        let rig = try Rig(answerAfterServiceGone: ServiceGoneGate())
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let connection = try #require(await rig.service.connection(0))
        connection.greet()
        guard case .introduce(let introduce)? = await connection.nextMessage() else {
            throw TestControlAnswerer.TimedOut(description: "the introduction")
        }
        let answer = try await core.answer(introduce.offer)
        let candidates = try await core.firstCandidates()
        connection.deliver(.answer(sdp: answer, turn: nil))
        for candidate in candidates {
            connection.deliver(.candidate(candidate))
        }
        connection.deliver(.candidate(""))
        if coreLeft {
            connection.deliver(.introductionEnd(code: "stationLeft"))
        } else {
            connection.drop()
        }
        let opened = try await dialling.value
        #expect(opened.certificateSHA256 == core.certificateSHA256)
        opened.channel.close()
    }

    /// With only the answer come, the same leaves nothing the connection
    /// can finish with: the dial fails at once, not at its deadline.
    @Test(arguments: [true, false])
    func aDialBeforeTheCoresCandidatesFailsAtOnceWhenTheServiceGoes(coreLeft: Bool) async throws {
        let rig = try Rig()
        let core = try TestControlAnswerer()
        defer { core.close() }
        let started = ContinuousClock.now
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        if coreLeft {
            connection.deliver(.introductionEnd(code: "stationLeft"))
        } else {
            connection.drop()
        }
        await #expect(throws: RendezvousDialError.serviceGone) { try await dialling.value }
        #expect(ContinuousClock.now - started < .seconds(20))
        #expect(rig.dialer.attemptTry?.outcome == .failed)
    }

    /// Every candidate the Core offered, its answer first, reaches the
    /// observer from the answer on, even when the dial then fails before
    /// the channel opens (R-IOS-16): the phone keeps the Core's global host
    /// addresses from them. A documentation-range address stands in for
    /// the Core's global IPv6 one.
    @Test func theCoresCandidatesReachTheObserverEvenWhenTheDialFails() async throws {
        let rig = try Rig()
        let core = try TestControlAnswerer()
        defer { core.close() }
        let seen = SeenCandidates()
        rig.dialer.observeCoreCandidates { seen.set($0) }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let connection = try #require(await rig.service.connection(0))
        connection.greet()
        guard case .introduce(let introduce)? = await connection.nextMessage() else {
            throw TestControlAnswerer.TimedOut(description: "the introduction")
        }
        let answer = try await core.answer(introduce.offer)
        connection.deliver(.answer(sdp: answer, turn: nil))
        for _ in 0..<400 where seen.lines.count < 1 {
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(seen.lines == [answer])
        let trickled = "candidate:1 1 UDP 2122317823 2001:db8:7::15f2 50001 typ host"
        connection.deliver(.candidate(trickled))
        for _ in 0..<400 where seen.lines.count < 2 {
            try await Task.sleep(for: .milliseconds(5))
        }
        connection.drop()
        await #expect(throws: RendezvousDialError.serviceGone) { try await dialling.value }
        #expect(seen.lines == [answer, trickled])
        #expect(CoreAddressList.introducedAddresses(seen.lines, port: 50055)
                .contains(StationEndpoint(host: "2001:db8:7::15f2", port: 50055)))
    }

    final class SeenCandidates: @unchecked Sendable {
        private let lock = NSLock()
        private var latest: [String] = []
        var lines: [String] { lock.withLock { latest } }
        func set(_ lines: [String]) { lock.withLock { latest = lines } }
    }

    /// A Core that is not registered: the next service is tried, then the
    /// dial fails at once with the service's own words.
    @Test func offlineTriesTheNextServiceThenFailsAtOnceInTheServicesWords() async throws {
        let rig = try Rig(servers: [Self.serverA, Self.serverB])
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let words = "That Core is not connected to this service."
        for index in 0..<2 {
            let connection = try #require(await rig.service.connection(index))
            connection.greet()
            guard case .introduce? = await connection.nextMessage() else {
                Issue.record("no introduction to service \(index)")
                return
            }
            connection.deliver(.error(RendezvousMessage.ServiceError(code: "offline", reason: words,
                                                                      retryAfterMs: 0)))
        }
        await #expect(throws: RendezvousDialError.service(.offline(reason: words))) { try await dialling.value }
        #expect(rig.service.connections.map(\.server) == [Self.serverA, Self.serverB])
        #expect(RendezvousDialError.service(.offline(reason: words)).operatorText == words)
        #expect(rig.dialer.attemptTry?.outcome == .noAnswer)
        // Kept for the trouble screen's words (Task 56).
        #expect(rig.dialer.lastError == .service(.offline(reason: words)))
    }

    /// No service answering is the app's own words.
    @Test func noServiceAnsweringIsUnreachable() async throws {
        let rig = try Rig()
        rig.service.refuse(Self.serverA)
        await #expect(throws: RendezvousDialError.service(.unreachable)) { try await rig.dialer.dial { _ in } }
        #expect(RendezvousDialError.service(.unreachable).operatorText
                == RendezvousDialError.unreachableServiceText)
    }

    /// An answer that is not one control connection's (a media line) is
    /// refused.
    @Test func anAnswerThatIsNotAControlConnectionsIsRefused() async throws {
        let rig = try Rig()
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let connection = try #require(await rig.service.connection(0))
        connection.greet()
        guard case .introduce? = await connection.nextMessage() else {
            Issue.record("no introduction")
            return
        }
        let audio = "v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
            + "c=IN IP4 0.0.0.0\r\na=mid:0\r\n"
        connection.deliver(.answer(sdp: audio, turn: nil))
        await #expect(throws: RendezvousDialError.unusableAnswer) { try await dialling.value }
    }

    /// An answer that comes, lets the dial outlive the service, and then
    /// cannot be applied still ends the dial at once, in its own words.
    @Test func anUnusableAnswerAppliedAfterTheServiceWentStillEndsTheDial() async throws {
        let rig = try Rig(answerAfterServiceGone: ServiceGoneGate())
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let connection = try #require(await rig.service.connection(0))
        connection.greet()
        guard case .introduce? = await connection.nextMessage() else {
            Issue.record("no introduction")
            return
        }
        let audio = "v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
            + "c=IN IP4 0.0.0.0\r\na=mid:0\r\n"
        connection.deliver(.answer(sdp: audio, turn: nil))
        connection.deliver(.candidate(""))
        connection.deliver(.introductionEnd(code: "stationLeft"))
        let started = ContinuousClock.now
        await #expect(throws: RendezvousDialError.unusableAnswer) { try await dialling.value }
        #expect(ContinuousClock.now - started < .seconds(20))
    }

    /// A connection that has not opened by the deadline fails as not
    /// opened in time, and the phone's service connection is closed.
    @Test func aDialThatDoesNotOpenInTimeFails() async throws {
        let clock = ManualLinkClock()
        let rig = try Rig(clock: clock)
        let core = try TestControlAnswerer()
        defer { core.close() }
        let dialling = Task { try await rig.dialer.dial { _ in } }
        let (connection, _) = try await answer(rig, core: core)
        // Nothing more from the Core: its candidates never come.
        await clock.advance(by: Int64(RendezvousDialer.dialDeadline.components.seconds * 1000))
        await #expect(throws: RendezvousDialError.notOpenedInTime) { try await dialling.value }
        #expect(await connection.waitUntilClosed(within: .seconds(10)))
        #expect(rig.dialer.attemptTry?.outcome == .timedOut)
        // The record says the Core offered nothing (the deadline can pass
        // before this phone's own gathering reports).
        let ice = try #require(rig.dialer.attemptTry?.ice)
        #expect(ice.core.isEmpty && ice.pairs.isEmpty)
        #expect(ice.summary.hasSuffix("The Core offered nothing."))
    }

    /// The desktop's bound: the service's hello, two lookups and the
    /// connect deadline.
    @Test func theDialDeadlineIsSeventyNineSeconds() {
        #expect(RendezvousDialer.dialDeadline == .milliseconds(79_000))
    }

    /// A session whose transport is closed while it still dials calls the
    /// dial off: the service connection closes.
    @Test func closingTheTransportWhileDiallingCallsTheDialOff() async throws {
        let rig = try Rig()
        let transport = DataChannelSessionTransport(connect: rig.dialer.connector)
        let opening = Task { try await transport.open { _ in } }
        let connection = try #require(await rig.service.connection(0))
        connection.greet()
        guard case .introduce? = await connection.nextMessage() else {
            Issue.record("no introduction")
            return
        }
        transport.close()
        #expect(await connection.waitUntilClosed(within: .seconds(10)))
        await #expect(throws: (any Error).self) { try await opening.value }
    }
}
