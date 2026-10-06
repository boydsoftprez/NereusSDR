// NereusSDR for iOS: control handover keeps one authenticated session
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct SessionTransportSwitchTests {
    private final class BinaryInbox: @unchecked Sendable {
        private let lock = NSLock()
        private let waiters = ConditionWaiters()
        private var held: [Data] = []
        func append(_ frame: Data) { lock.withLock { held.append(frame) }; waiters.release() }
        var frames: [Data] { lock.withLock { held } }
        func waitFor(_ count: Int) async {
            _ = await waiters.wait(within: .seconds(5)) { self.frames.count >= count }
        }
    }
    private final class Candidate: LinkTransport, @unchecked Sendable {
        let digest: Data
        let hello: LinkMessage.Hello
        private let lock = NSLock()
        private let waiters = ConditionWaiters()
        private var recipient: (@Sendable (LinkTransportEvent) async -> Void)?
        private var binaryReceiver: (@Sendable (Data) -> Void)?
        private var binarySent: [Data] = []
        private var sent: [String] = []
        private var closed = false
        private var pings = 0

        init(identity: TestStationIdentity, digest: Data = Data(repeating: 9, count: 32),
             major: UInt16 = 1, minor: UInt16 = 11,
             bindingDigest: Data? = nil) throws {
            self.digest = digest
            hello = LinkMessage.Hello(major: major, minor: minor, settingsSchema: 0,
                                      peer: "nereusd", majors: [major],
                                      identity: try identity.claim(certificateSHA256: bindingDigest ?? digest))
        }

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            lock.withLock { recipient = onEvent }
            await onEvent(.text(LinkCodec.encode(.hello(hello))))
            return digest
        }
        @discardableResult func send(_ text: String) -> Bool {
            let admitted = lock.withLock { () -> Bool in
                guard !closed else { return false }
                sent.append(text)
                return true
            }
            if admitted { waiters.release() }
            return admitted
        }
        func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {
            lock.withLock { binaryReceiver = receiver }
        }
        @discardableResult func sendBinary(_ frame: Data) -> Bool {
            lock.withLock {
                guard !closed else { return false }
                binarySent.append(frame)
                return true
            }
        }
        var binaryFrames: [Data] { lock.withLock { binarySent } }
        func deliverBinary(_ frame: Data) { lock.withLock { binaryReceiver }?(frame) }
        func ping() { lock.withLock { pings += 1 } }
        func close() { lock.withLock { closed = true }; waiters.release() }
        var messages: [String] { lock.withLock { sent } }
        var isClosed: Bool { lock.withLock { closed } }
        var pingCount: Int { lock.withLock { pings } }
        func waitForMessages(_ count: Int) async {
            _ = await waiters.wait(within: .seconds(5)) { self.messages.count >= count || self.isClosed }
        }
        func deliver(_ message: LinkMessage) async {
            await lock.withLock { recipient }?(.text(LinkCodec.encode(message)))
        }
        func deliver(_ text: String) async { await lock.withLock { recipient }?(.text(text)) }
        func pong() async { await lock.withLock { recipient }?(.pong) }
        func drop() async { await lock.withLock { recipient }?(.closed) }
    }

    private struct Rig {
        let identity: TestStationIdentity
        let oldDigest = Data(repeating: 4, count: 32)
        let old: ScriptedTransport
        let clock = ManualLinkClock()
        let session: StationSession
        let recorder: EventRecorder

        init(timerClock: (any LinkClock)? = nil, diagnosticEndpointRank: Int? = nil) {
            let identity = TestStationIdentity()
            self.identity = identity
            old = ScriptedTransport(presentedSHA256: oldDigest, openFailure: nil)
            let old = self.old
            session = StationSession(trust: identity.trust,
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: timerClock ?? clock, transport: { old },
                                     diagnosticEndpointRank: diagnosticEndpointRank)
            recorder = EventRecorder(session)
        }

        func ready(capability: LinkMessage.PropertyValue = .i64(1), minor: UInt16 = 11) async throws {
            await session.connect()
            let hello = LinkMessage.Hello(major: 1, minor: minor, settingsSchema: 0,
                                          peer: "nereusd", majors: [1],
                                          identity: try identity.claim(certificateSHA256: oldDigest))
            await old.deliver(.hello(hello))
            await old.deliver(.authResult(.init(accepted: true, reason: "")))
            await old.deliver(.capabilities(.init(properties: [
                .init(name: "controlSwitchVersion", value: capability),
            ])))
            await old.deliver(.snapshotComplete)
            #expect(await session.state == .ready)
        }

        func candidate(identity newIdentity: TestStationIdentity? = nil,
                       minor: UInt16 = 11,
                       availabilityProbe: (@Sendable () async -> Void)? = nil)
            async throws -> (Candidate, PreauthenticatedTransport) {
            let raw = try Candidate(identity: newIdentity ?? identity, minor: minor)
            let lease = PreauthenticatedTransport(raw, availabilityProbe: availabilityProbe)
            _ = try await lease.inspect(clock: clock, deadline: .seconds(30))
            return (raw, lease)
        }
    }

    @Test func manualFactoryRankIsNotInheritedByASwitchedLease() async throws {
        let rig = Rig(diagnosticEndpointRank: 1)
        try await rig.ready()
        #expect(await rig.session.diagnosticsSnapshot().serviceRank == 1)
        let (raw, lease) = try await rig.candidate()
        let move = startMove(rig, lease)
        for _ in 0..<50_000 { if raw.messages.count >= 2 { break }; await Task.yield() }
        _ = try #require(raw.messages.count >= 2)
        await rig.old.deliver(.pathSwitch)
        #expect(await move.value)
        #expect(await rig.session.diagnosticsSnapshot().serviceRank == nil)
        await rig.session.disconnect()
    }

    @Test func retryUsesOnlyTheNewAttemptsAdoptedRank() async throws {
        let identity = TestStationIdentity()
        let clock = ManualLinkClock()
        let firstRaw = try Candidate(identity: identity)
        let secondRaw = try Candidate(identity: identity)
        let first = PreauthenticatedTransport(firstRaw)
        let second = PreauthenticatedTransport(secondRaw)
        _ = try await first.inspect(clock: clock, deadline: .seconds(30))
        _ = try await second.inspect(clock: clock, deadline: .seconds(30))
        first.recordDiagnosticServiceRank(0)
        second.recordDiagnosticServiceRank(4)
        let queue = TransportQueue([first, second])
        let session = StationSession(trust: identity.trust, authenticator: TokenAuthenticator(token: "test-token"),
                                    clock: clock, transport: { queue.next() })
        await session.connect()
        await firstRaw.deliver(.authResult(.init(accepted: true, reason: "")))
        await firstRaw.deliver(.snapshotComplete)
        let before = await session.diagnosticsSnapshot()
        #expect(before.state == .ready && before.serviceRank == 0)
        await firstRaw.drop()
        #expect(await session.diagnosticsSnapshot().serviceRank == nil)
        await clock.advance(by: 1000)
        await secondRaw.deliver(.authResult(.init(accepted: true, reason: "")))
        await secondRaw.deliver(.snapshotComplete)
        let after = await session.diagnosticsSnapshot()
        #expect(after.state == .ready && after.serviceRank == 4)
        #expect(after.attemptGeneration != before.attemptGeneration)
        #expect(first.diagnosticServiceRank == nil)
        await session.disconnect()
    }

    @Test(arguments: [0, 1, 2, 3, 4])
    func diagnosticRankIsHiddenUntilAdoptionAndCannotBeReassigned(rank: Int) async throws {
        let rig = Rig()
        let (_, lease) = try await rig.candidate()
        lease.recordDiagnosticServiceRank(rank)
        #expect(lease.diagnosticServiceRank == nil)
        _ = try await lease.open { _ in }
        #expect(lease.diagnosticServiceRank == rank)
        lease.recordDiagnosticServiceRank((rank + 1) % 5)
        #expect(lease.diagnosticServiceRank == rank)
        lease.close()
        #expect(lease.diagnosticServiceRank == nil)
    }

    @Test func expiredUnadoptedLeaseCannotSupplyDiagnosticRank() async throws {
        let rig = Rig()
        let (_, lease) = try await rig.candidate()
        lease.recordDiagnosticServiceRank(2)
        await rig.clock.advance(by: 30_000)
        #expect(!(await lease.isAvailable()))
        #expect(lease.diagnosticServiceRank == nil)
        lease.recordDiagnosticServiceRank(4)
        #expect(lease.diagnosticServiceRank == nil)
    }

    @Test func diagnosticRankChangesWithActiveRouteBeforeHeldAppCommit() async throws {
        let rig = Rig()
        try await rig.ready()
        let before = await rig.session.diagnosticsSnapshot()
        let (raw, lease) = try await rig.candidate()
        lease.recordDiagnosticServiceRank(1)
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        let during = await rig.session.diagnosticsSnapshot()
        #expect(during.attemptGeneration == before.attemptGeneration)
        #expect(during.routeID != before.routeID)
        #expect(during.serviceRank == 1)
        await gate.resume()
        #expect(await moved.value)
        lease.close()
        #expect(await rig.session.diagnosticsSnapshot().serviceRank == nil)
    }

    private static let ticket = Base64URL.encode(Data(repeating: 0x5a, count: 32))

    @Test func binaryUsesCurrentSendAndCurrentOrDrainingReceiveAcrossBarrier() async throws {
        let rig = Rig()
        try await rig.ready()
        let inbox = BinaryInbox()
        let owner = UUID()
        await rig.session.setMediaTunnelReceiver(owner: owner) { inbox.append($0) }
        let frame = Data([2] + Array(repeating: UInt8(7), count: 17))
        #expect(await rig.session.sendMediaTunnel(frame))
        #expect(rig.old.binaryFrames == [frame])
        rig.old.deliverBinary(frame)
        await inbox.waitFor(1)
        #expect(inbox.frames.count == 1)

        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        #expect(await rig.session.sendMediaTunnel(frame))
        #expect(raw.binaryFrames == [frame])
        #expect(rig.old.binaryFrames == [frame])
        raw.deliverBinary(frame)
        rig.old.deliverBinary(frame)
        await inbox.waitFor(2)
        #expect(inbox.frames.count == 2, "OLD remains an accepted drain")
        await gate.resume()
        #expect(await moved.value)
        await inbox.waitFor(3)
        #expect(inbox.frames.count == 3, "NEW is released only after route callback")
        await rig.clock.advance(by: 5_000)
        rig.old.deliverBinary(frame)
        for _ in 0..<100 { await Task.yield() }
        #expect(inbox.frames.count == 3, "retired OLD is no longer accepted")
    }

    private func startMove(_ rig: Rig, _ lease: PreauthenticatedTransport,
                           safety: @escaping @Sendable () async -> Bool = { true },
                           commit: @escaping @Sendable () async -> Void = {}) -> Task<Bool, Never> {
        Task {
            await rig.session.moveControl(to: lease, safetyCheck: safety,
                                          requestTicket: {
                                              try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                          }, onRouteCommit: commit)
        }
    }

    @Test func ticketValidationAndRedaction() throws {
        let ticket = try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
        #expect(PathTicket(secret: "not-a-ticket", expiresInMs: 10_000) == nil)
        #expect(PathTicket(secret: Self.ticket + "=", expiresInMs: 10_000) == nil)
        #expect(PathTicket(secret: Self.ticket, expiresInMs: 0) == nil)
        #expect(!String(describing: ticket).contains(Self.ticket))
        #expect(!String(reflecting: ticket).contains(Self.ticket))
        #expect(!String(describing: ticket.customMirror).contains(Self.ticket))
    }

    @Test func inspectedCandidateIsAvailableOnlyBeforeConsumption() async throws {
        let rig = Rig()
        let (_, lease) = try await rig.candidate()
        let checked = await lease.inspectedConnection()
        #expect(checked?.1.count == 32)
        lease.close()
        #expect(await lease.inspectedConnection() == nil)
    }

    @Test func oldBarrierCommitsBeforeNewMessagesAndPreservesSession() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true },
                                          requestTicket: { try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000)) },
                                          onRouteCommit: {})
        }
        await raw.waitForMessages(2)
        #expect(try LinkCodec.decode(raw.messages[0]).kind == .hello)
        #expect(try LinkCodec.decode(raw.messages[1]).kind == .pathJoin)
        #expect(!raw.messages.contains { $0.contains("auth.request") })
        await raw.deliver(.capabilities(.init(properties: [])))
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        #expect(await rig.session.state == .ready)
        #expect(try LinkCodec.decode(try #require(rig.old.pending.last)).kind == .pathSwitch)
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 9, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
    }

    @Test func wrongIdentitySendsNoTicketOrJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate(identity: TestStationIdentity())
        let requested = LockFlag()
        let moved = await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                  requestTicket: {
                                                      requested.set()
                                                      return try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                                  }, onRouteCommit: {})
        #expect(!moved)
        #expect(!requested.value)
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test(arguments: [(LinkMessage.PropertyValue.utf8("1"), UInt16(11)),
                      (.enumeration(1), 11), (.i64(0), 11), (.i64(1), 10)])
    func capabilityRequiresTypedI64AndMinor11(value: LinkMessage.PropertyValue,
                                                minor: UInt16) async throws {
        let rig = Rig()
        try await rig.ready(capability: value, minor: minor)
        let (raw, lease) = try await rig.candidate(minor: minor)
        let moved = await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                  requestTicket: {
                                                      try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                                  }, onRouteCommit: {})
        #expect(!moved)
        #expect(raw.messages.isEmpty)
        #expect(await rig.session.state == .ready)
    }

    @Test func candidateMinorMustMatchExistingAgreedMinor() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate(minor: 10)
        #expect(!(await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                requestTicket: {
                                                    try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                                }, onRouteCommit: {})))
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
    }

    @Test func candidateCertificateBindingAndMajorMustMatchExactly() async throws {
        let rig = Rig()
        try await rig.ready()
        for raw in [try Candidate(identity: rig.identity, bindingDigest: Data(repeating: 3, count: 32)),
                    try Candidate(identity: rig.identity, major: 2)] {
            let lease = PreauthenticatedTransport(raw)
            _ = try await lease.inspect(clock: rig.clock, deadline: .seconds(30))
            let moved = await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                      requestTicket: {
                                                          try #require(PathTicket(secret: Self.ticket,
                                                                                  expiresInMs: 10_000))
                                                      }, onRouteCommit: {})
            #expect(!moved)
            #expect(raw.messages.isEmpty)
            #expect(raw.isClosed)
        }
        #expect(await rig.session.state == .ready)
    }

    @Test func unsafeRadioStateSkipsTicketAndJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let requests = LockFlag()
        let moved = await rig.session.moveControl(to: lease, safetyCheck: { false },
                                                  requestTicket: {
                                                      requests.set()
                                                      return try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                                  }, onRouteCommit: {})
        #expect(!moved)
        #expect(!requests.value)
        #expect(raw.messages.isEmpty)
        #expect(await rig.session.state == .ready)
    }

    @Test func withheldBarrierAbandonsOnlyCandidateAtTenSeconds() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.clock.advance(by: 10_000)
        #expect(!(await moved.value))
        #expect(raw.isClosed)
        #expect(await rig.session.state == .ready)
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 42, args: [])))
        #expect(try LinkCodec.decode(try #require(rig.old.pending.last)).kind == .commandInvoke)
    }

    @Test(arguments: [Int64(10_000), 300])
    func startedMoveTimeoutCannotCloseNewAfterBarrier(expiresInMs: Int64) async throws {
        let timerClock = CapturedLinkClock()
        let rig = Rig(timerClock: timerClock)
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true },
                                          requestTicket: {
                                              try #require(PathTicket(secret: Self.ticket,
                                                                      expiresInMs: expiresInMs))
                                          }, onRouteCommit: { await gate.pause() })
        }
        await raw.waitForMessages(2)
        let started = try #require(timerClock.takeStarted(after: expiresInMs))
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        await started()
        #expect(!raw.isClosed)
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 85, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
        await gate.resume()
        #expect(await moved.value)
        #expect(await rig.session.state == .ready)
    }

    @Test func refusedNewJoinAndNewClosePreserveOld() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await raw.deliver(.sessionEnd(.init(reason: "join refused", retryable: false,
                                             code: "protocolError")))
        #expect(!(await moved.value))
        #expect(await rig.session.state == .ready)
        #expect(!rig.old.isClosedByApp)

        let (second, secondLease) = try await rig.candidate()
        let next = startMove(rig, secondLease)
        await second.waitForMessages(2)
        await second.drop()
        #expect(!(await next.value))
        #expect(await rig.session.state == .ready)
    }

    @Test func oldCloseAfterLiveJoinCountsAsBarrier() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.old.dropLink()
        #expect(await moved.value)
        #expect(await rig.session.state == .ready)
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 24, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
    }

    @Test func newCloseAfterCommitStartsNormalRetry() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        await raw.drop()
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        #expect(rig.old.isClosedByApp)
    }

    @Test func successfulMoveClaimsCandidateOriginalDeadline() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        await rig.clock.advance(by: 30_000)
        #expect(!raw.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test func drainingOldPongNeverBecomesCurrentRouteDiagnosticRtt() async throws {
        let rig = Rig()
        try await rig.ready()
        await rig.clock.advance(by: 20_000)
        #expect(rig.old.pingCount == 1)
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        let selected = await rig.session.diagnosticsSnapshot()
        #expect(selected.routeID != nil && selected.routeID != 0)
        #expect(selected.roundTrip == nil)

        await rig.old.answerPings()
        #expect(await rig.session.diagnosticsSnapshot().roundTrip == nil)
        await rig.clock.advance(by: 20_000)
        #expect(raw.pingCount == 1)
        await rig.old.deliverPong()
        #expect(await rig.session.diagnosticsSnapshot().roundTrip == nil)
        await raw.pong()
        let current = await rig.session.diagnosticsSnapshot()
        #expect(current.roundTrip?.routeID == current.routeID)
        #expect(current.roundTrip?.attemptGeneration == current.attemptGeneration)
    }

    @Test func oldPongCannotMakeOverlappingCurrentPingsLookUnambiguous() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)

        await rig.clock.advance(by: 20_000)
        #expect(raw.pingCount == 1)
        await rig.old.deliverPong() // Legacy heartbeat is cleared by the draining route.
        await rig.clock.advance(by: 20_000)
        #expect(raw.pingCount == 2)
        await raw.pong() // Either current ping could have produced this pong.
        #expect(await rig.session.diagnosticsSnapshot().roundTrip == nil)
        await raw.pong()
        #expect(await rig.session.diagnosticsSnapshot().roundTrip == nil)

        await rig.clock.advance(by: 20_000)
        #expect(raw.pingCount == 3)
        await raw.pong()
        let recovered = await rig.session.diagnosticsSnapshot()
        #expect(recovered.roundTrip?.routeID == recovered.routeID)
        #expect(recovered.roundTrip?.attemptGeneration == recovered.attemptGeneration)
    }

    @Test func routeCommitPrecedesHeldNewMessages() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        let delta = LinkMessage.delta(.init(key: "radio", properties: [
            .init(name: "frequency", value: .i64(123)),
        ]))
        await raw.deliver(delta)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        #expect(!rig.recorder.messages.contains(delta))
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 25, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
        await gate.resume()
        #expect(await moved.value)
        #expect(await rig.recorder.handled { events in events.contains(.message(delta)) })
    }

    @Test func safetyIsRecheckedAfterTicketBeforeJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let checks = Counter()
        let moved = await rig.session.moveControl(to: lease,
                                                  safetyCheck: { checks.next() == 1 },
                                                  requestTicket: {
                                                      try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                                  }, onRouteCommit: {})
        #expect(!moved)
        #expect(checks.value == 2)
        #expect(raw.messages.isEmpty)
        #expect(await rig.session.state == .ready)
    }

    @Test func oldCloseBeforeJoinCannotTurnStaleTicketIntoJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = TicketGate()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true },
                                          requestTicket: { await gate.request() }, onRouteCommit: {})
        }
        await gate.waitEntered()
        await rig.old.dropLink()
        #expect(!(await moved.value))
        await gate.release()
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func newSnapshotIsNeverPublishedAndOldSessionSurvives() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await raw.deliver(.snapshotComplete)
        #expect(!(await moved.value))
        #expect(await rig.session.state == .ready)
        #expect(rig.recorder.messages.filter { $0 == .snapshotComplete }.count == 1)
    }

    @Test func futureNewMessageKindDoesNotAbortMove() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await raw.deliver("{\"type\":\"future.session.message\",\"value\":7}")
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        #expect(await rig.session.state == .ready)
    }

    @Test func secondMoveRetiresPriorOldAndDrainsItsOwnOldAfterFiveSeconds() async throws {
        let rig = Rig()
        try await rig.ready()
        let (first, firstLease) = try await rig.candidate()
        let movedFirst = startMove(rig, firstLease)
        await first.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await movedFirst.value)
        let (second, secondLease) = try await rig.candidate()
        let movedSecond = startMove(rig, secondLease)
        await second.waitForMessages(2)
        #expect(rig.old.isClosedByApp)
        await first.deliver(.pathSwitch)
        #expect(await movedSecond.value)
        #expect(!first.isClosed)
        await rig.clock.advance(by: 5_000)
        #expect(first.isClosed)
        #expect(!second.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test func secondMoveAcceptsDrainingRoutePongAndClose() async throws {
        let timerClock = CapturedLinkClock()
        let rig = Rig(timerClock: timerClock)
        try await rig.ready()
        let (first, firstLease) = try await rig.candidate()
        let movedFirst = startMove(rig, firstLease)
        await first.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        #expect(await movedFirst.value)

        let (second, secondLease) = try await rig.candidate()
        let movedSecond = startMove(rig, secondLease)
        await second.waitForMessages(2)
        await rig.session.redialNow()
        #expect(first.pingCount == 1)
        #expect(await rig.session.pingsAwaitingPong == 1)
        await first.deliver(.pathSwitch)
        #expect(await movedSecond.value)
        await first.pong()
        #expect(await rig.session.pingsAwaitingPong == 0)
        #expect(timerClock.activeCount(after: 5_000) == 1)
        await first.drop()
        #expect(timerClock.activeCount(after: 5_000) == 0)
        #expect(await rig.session.state == .ready)
        #expect(!second.isClosed)
    }

    @Test func startedPriorGenerationDrainCannotRetireNewDrain() async throws {
        let timerClock = CapturedLinkClock()
        let identity = TestStationIdentity()
        let digest = Data(repeating: 4, count: 32)
        let firstOld = ScriptedTransport(presentedSHA256: digest, openFailure: nil)
        let secondOld = ScriptedTransport(presentedSHA256: digest, openFailure: nil)
        let transports = TransportSequence([firstOld, secondOld])
        let session = StationSession(trust: identity.trust,
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: timerClock, transport: { transports.next() })

        try await ready(session, on: firstOld, identity: identity, digest: digest)
        let first = try Candidate(identity: identity)
        let firstLease = PreauthenticatedTransport(first)
        _ = try await firstLease.inspect(clock: timerClock, deadline: .seconds(30))
        let firstMove = Task {
            await session.moveControl(to: firstLease, safetyCheck: { true },
                                      requestTicket: {
                                          try #require(PathTicket(secret: Self.ticket,
                                                                  expiresInMs: 10_000))
                                      }, onRouteCommit: {})
        }
        await first.waitForMessages(2)
        await firstOld.deliver(.pathSwitch)
        #expect(await firstMove.value)
        let startedOldDrain = try #require(timerClock.takeStarted(after: 5_000))

        await session.disconnect()
        try await ready(session, on: secondOld, identity: identity, digest: digest)
        let second = try Candidate(identity: identity)
        let secondLease = PreauthenticatedTransport(second)
        _ = try await secondLease.inspect(clock: timerClock, deadline: .seconds(30))
        let secondMove = Task {
            await session.moveControl(to: secondLease, safetyCheck: { true },
                                      requestTicket: {
                                          try #require(PathTicket(secret: Self.ticket,
                                                                  expiresInMs: 10_000))
                                      }, onRouteCommit: {})
        }
        await second.waitForMessages(2)
        await secondOld.deliver(.pathSwitch)
        #expect(await secondMove.value)
        #expect(timerClock.activeCount(after: 5_000) == 1)

        await startedOldDrain()
        #expect(!secondOld.isClosedByApp)
        #expect(timerClock.activeCount(after: 5_000) == 1)
        let currentDrain = try #require(timerClock.takeStarted(after: 5_000))
        await currentDrain()
        #expect(secondOld.isClosedByApp)
        #expect(!second.isClosed)
        #expect(await session.state == .ready)
    }

    @Test func candidatePongCountsWhileOldSendsAndNewSendsAfterBarrier() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let moved = startMove(rig, lease)
        await raw.waitForMessages(2)
        await rig.session.redialNow()
        #expect(rig.old.pingCount == 1)
        // The move emits join before its release task drains candidate
        // events. Releasing explicitly makes pong() wait for the session's
        // receiver, so advancing the manual deadline cannot outrun it.
        #expect(await lease.releaseEvents())
        await raw.pong()
        await rig.clock.advance(by: 5_000)
        #expect(await rig.session.state == .ready)
        try await rig.session.send(.commandInvoke(.init(verb: "tx.keepalive", id: 71, args: [])))
        #expect(try LinkCodec.decode(try #require(rig.old.pending.last)).kind == .commandInvoke)
        await rig.old.deliver(.pathSwitch)
        #expect(await moved.value)
        try await rig.session.send(.commandInvoke(.init(verb: "tx.keepalive", id: 72, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
        await rig.session.setAcceleratedHeartbeat(true)
        await rig.clock.advance(by: 2_000)
        #expect(raw.pingCount == 1)
    }

    @Test func heldNewMessagesBeyondThirtyTwoMiBEndSession() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        let payload = String(repeating: "x", count: 7 * 1024 * 1024)
        let text = "{\"type\":\"capabilities\",\"properties\":[],\"pad\":\"\(payload)\"}"
        for _ in 0..<5 { await raw.deliver(text) }
        await gate.resume()
        #expect(!(await moved.value))
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    @Test func singleNewMessageAboveInboundCapEndsSession() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        let payload = String(repeating: "x", count: 8 * 1024 * 1024)
        await raw.deliver("{\"type\":\"capabilities\",\"properties\":[],\"pad\":\"\(payload)\"}")
        await gate.resume()
        #expect(!(await moved.value))
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
    }

    private final class Counter: @unchecked Sendable {
        private let lock = NSLock()
        private var count = 0
        var value: Int { lock.withLock { count } }
        func next() -> Int { lock.withLock { count += 1; return count } }
    }

    @Test func disconnectReturnsSuspendedTicketMovePromptly() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = TicketGate()
        let done = LockFlag()
        let completion = ConditionWaiters()
        let moved = Task {
            let result = await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                       requestTicket: { await gate.request() },
                                                       onRouteCommit: {})
            done.set()
            completion.release()
            return result
        }
        await gate.waitEntered()
        await rig.session.disconnect()
        let finished = await completion.wait(within: .milliseconds(200)) { done.value }
        await gate.release()
        #expect(finished)
        #expect(!(await moved.value))
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .stopped)
    }

    @Test func cancelingCallerCancelsSuspendedTicketAndClosesCandidate() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = TicketGate()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true },
                                          requestTicket: { await gate.request() }, onRouteCommit: {})
        }
        await gate.waitEntered()
        moved.cancel()
        let cancelled = await gate.waitCancelled()
        await gate.release()
        #expect(cancelled)
        #expect(!(await moved.value))
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test func cancelingCallerCancelsSuspendedSafetyBeforeTicket() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = SafetyGate()
        let requested = LockFlag()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { await gate.check() },
                                          requestTicket: {
                                              requested.set()
                                              return try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                          }, onRouteCommit: {})
        }
        await gate.waitEntered()
        moved.cancel()
        let cancelled = await gate.waitCancelled()
        await gate.release()
        #expect(cancelled)
        #expect(!(await moved.value))
        #expect(!requested.value)
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
    }

    @Test func cancellationDuringAvailabilityDoesNotRequestTicket() async throws {
        let rig = Rig()
        try await rig.ready()
        let gate = AvailabilityGate(pauseAt: 1)
        let (raw, lease) = try await rig.candidate(availabilityProbe: { await gate.check() })
        let requested = LockFlag()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { true },
                                          requestTicket: {
                                              requested.set()
                                              return try #require(PathTicket(secret: Self.ticket,
                                                                              expiresInMs: 10_000))
                                          }, onRouteCommit: {})
        }
        await gate.waitEntered()
        let preparation = try #require(await rig.session.movePreparationForTesting)
        moved.cancel()
        #expect(!(await moved.value))
        await gate.release()
        await preparation.value
        #expect(!requested.value)
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
    }

    @Test func staleAvailabilityCannotReplaceNewMovesExpiry() async throws {
        let timerClock = CapturedLinkClock()
        let rig = Rig(timerClock: timerClock)
        try await rig.ready()
        let gate = AvailabilityGate(pauseAt: 2)
        let (_, firstLease) = try await rig.candidate(availabilityProbe: { await gate.check() })
        let first = Task {
            await rig.session.moveControl(to: firstLease, safetyCheck: { true },
                                          requestTicket: {
                                              try #require(PathTicket(secret: Self.ticket,
                                                                      expiresInMs: 200))
                                          }, onRouteCommit: {})
        }
        await gate.waitEntered()
        let preparation = try #require(await rig.session.movePreparationForTesting)
        first.cancel()
        #expect(!(await first.value))

        let (secondRaw, secondLease) = try await rig.candidate()
        let second = Task {
            await rig.session.moveControl(to: secondLease, safetyCheck: { true },
                                          requestTicket: {
                                              try #require(PathTicket(secret: Self.ticket,
                                                                      expiresInMs: 100))
                                          }, onRouteCommit: {})
        }
        await secondRaw.waitForMessages(2)
        await gate.release()
        await preparation.value
        await rig.old.deliver(.pathSwitch)
        #expect(await second.value)
        #expect(timerClock.activeCount(after: 100) == 0)
        #expect(timerClock.activeCount(after: 200) == 0)
    }

    @Test func alreadyCancelledCallerNeverStartsTicketOrJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let requested = LockFlag()
        let moved = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            return await rig.session.moveControl(to: lease, safetyCheck: { true },
                                                 requestTicket: {
                                                     requested.set()
                                                     return try #require(PathTicket(secret: Self.ticket,
                                                                                     expiresInMs: 10_000))
                                                 }, onRouteCommit: {})
        }
        #expect(!(await moved.value))
        #expect(!requested.value)
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test func callerCancelAtFinalSafetyCheckCannotSendJoin() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = FinalSafetyGate()
        let moved = Task {
            await rig.session.moveControl(to: lease, safetyCheck: { await gate.check() },
                                          requestTicket: {
                                              try #require(PathTicket(secret: Self.ticket, expiresInMs: 10_000))
                                          }, onRouteCommit: {})
        }
        await gate.waitEntered()
        moved.cancel()
        await gate.release()
        #expect(!(await moved.value))
        #expect(raw.messages.isEmpty)
        #expect(raw.isClosed)
        #expect(await rig.session.state == .ready)
    }

    @Test func callerCancelAfterBarrierKeepsCommittedNewRoute() async throws {
        let rig = Rig()
        try await rig.ready()
        let (raw, lease) = try await rig.candidate()
        let gate = CommitGate()
        let moved = startMove(rig, lease, commit: { await gate.pause() })
        await raw.waitForMessages(2)
        await rig.old.deliver(.pathSwitch)
        await gate.waitEntered()
        moved.cancel()
        try await rig.session.send(.commandInvoke(.init(verb: "test.echo", id: 81, args: [])))
        #expect(try LinkCodec.decode(try #require(raw.messages.last)).kind == .commandInvoke)
        await gate.resume()
        #expect(await moved.value)
        #expect(await rig.session.state == .ready)
    }

    @Test func acceleratedHeartbeatUsesTwoSecondsWithoutResettingUnchangedTicks() async throws {
        let rig = Rig()
        try await rig.ready()
        await rig.session.setAcceleratedHeartbeat(true)
        await rig.clock.advance(by: 1_999)
        #expect(rig.old.pingCount == 0)
        await rig.session.setAcceleratedHeartbeat(true)
        await rig.clock.advance(by: 1)
        #expect(rig.old.pingCount == 1)
        await rig.old.answerPings()
        await rig.clock.advance(by: 2_000)
        #expect(rig.old.pingCount == 2)
        await rig.old.answerPings()
        await rig.session.setAcceleratedHeartbeat(false)
        await rig.clock.advance(by: 19_999)
        #expect(rig.old.pingCount == 2)
        await rig.clock.advance(by: 1)
        #expect(rig.old.pingCount == 3)
    }

    @Test func supersededHeartbeatCallbackCannotStartAnotherChain() async throws {
        let timerClock = CapturedLinkClock()
        let rig = Rig(timerClock: timerClock)
        try await rig.ready()
        let started = try #require(timerClock.takeStarted(after: 20_000))
        await rig.session.setAcceleratedHeartbeat(true)
        await started()
        #expect(rig.old.pingCount == 0)
        #expect(timerClock.activeCount(after: 2_000) == 1)
        let next = try #require(timerClock.takeStarted(after: 2_000))
        await next()
        #expect(rig.old.pingCount == 1)
        #expect(timerClock.activeCount(after: 2_000) == 1)
    }

    @Test func disconnectCancelsAsyncSelectionAndClosesLateLease() async {
        let queue = FactoryQueue()
        let clock = ManualLinkClock()
        let pin = Data(repeating: 7, count: 32)
        let session = StationSession(trust: .certificate(pinSHA256: pin),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: clock, asyncTransport: { try await queue.select() })
        let connecting = Task { await session.connect() }
        #expect(await queue.waitForCalls(1))
        await session.disconnect()
        await connecting.value
        let late = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(late)
        for _ in 0..<2_000 where !late.isClosedByApp { await Task.yield() }
        #expect(late.isClosedByApp)
        #expect(await session.state == .stopped)
    }

    @Test func failedAsyncSelectionUsesExistingOneSecondBackoff() async {
        let queue = FactoryQueue()
        let clock = ManualLinkClock()
        let pin = Data(repeating: 8, count: 32)
        let session = StationSession(trust: .certificate(pinSHA256: pin),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: clock, asyncTransport: { try await queue.select() })
        let connecting = Task { await session.connect() }
        #expect(await queue.waitForCalls(1))
        queue.fail(LinkTransportError.failed("no path"))
        await connecting.value
        #expect(await session.state == .waitingToRetry(seconds: 1))
        let retry = Task { await clock.advance(by: 1_000) }
        #expect(await queue.waitForCalls(2))
        let next = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(next)
        await retry.value
        #expect(await session.state == .connecting)
        #expect(!next.isClosedByApp)
        await session.disconnect()
    }

    @Test func networkSupersessionClosesStaleAsyncSelection() async {
        let queue = FactoryQueue()
        let pin = Data(repeating: 6, count: 32)
        let session = StationSession(trust: .certificate(pinSHA256: pin),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: ManualLinkClock(),
                                     asyncTransport: { try await queue.select() })
        let first = Task { await session.connect() }
        #expect(await queue.waitForCalls(1))
        let second = Task { await session.redialNow() }
        #expect(await queue.waitForCalls(2))
        let stale = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(stale)
        let winner = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(winner)
        await first.value
        await second.value
        for _ in 0..<2_000 where !stale.isClosedByApp { await Task.yield() }
        #expect(stale.isClosedByApp)
        #expect(!winner.isClosedByApp)
        await session.disconnect()
    }

    @Test func holdingRetriesCancelsSelectionUntilNetworkReturns() async {
        let queue = FactoryQueue()
        let pin = Data(repeating: 5, count: 32)
        let session = StationSession(trust: .certificate(pinSHA256: pin),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: ManualLinkClock(),
                                     asyncTransport: { try await queue.select() })
        let first = Task { await session.connect() }
        #expect(await queue.waitForCalls(1))
        await session.holdRetries()
        await first.value
        #expect(await session.state == .waitingToRetry(seconds: 0))
        let stale = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(stale)
        for _ in 0..<2_000 where !stale.isClosedByApp { await Task.yield() }
        #expect(stale.isClosedByApp)
        let restarted = Task { await session.redialNow() }
        #expect(await queue.waitForCalls(2))
        let live = ScriptedTransport(presentedSHA256: pin, openFailure: nil)
        queue.release(live)
        await restarted.value
        #expect(await session.state == .connecting)
        await session.disconnect()
    }

    @Test func asyncSelectionCertificateMismatchIsFinalRefusal() async {
        let queue = FactoryQueue()
        let session = StationSession(trust: .certificate(pinSHA256: Data(repeating: 2, count: 32)),
                                     authenticator: TokenAuthenticator(token: "test-token"),
                                     clock: ManualLinkClock(),
                                     asyncTransport: { try await queue.select() })
        let recorder = EventRecorder(session)
        let connecting = Task { await session.connect() }
        #expect(await queue.waitForCalls(1))
        queue.fail(LinkTransportError.certificateMismatch)
        await connecting.value
        #expect(await session.state == .stopped)
        #expect(await recorder.handled { events in events.contains(.refused(
            Refusal(.authentication(StationSession.certificateMismatchText), code: .identityChanged))) })
    }

    private final class FactoryQueue: @unchecked Sendable {
        private let lock = NSLock()
        private let waiters = ConditionWaiters()
        private var count = 0
        private var pending: [CheckedContinuation<any SessionTransport, Error>] = []
        func select() async throws -> any SessionTransport {
            try await withCheckedThrowingContinuation { continuation in
                lock.withLock {
                    count += 1
                    pending.append(continuation)
                }
                waiters.release()
            }
        }
        func waitForCalls(_ expected: Int) async -> Bool {
            await waiters.wait(within: .seconds(5)) { self.lock.withLock { self.count >= expected } }
        }
        func release(_ transport: any SessionTransport) {
            let continuation = lock.withLock { pending.removeFirst() }
            continuation.resume(returning: transport)
        }
        func fail(_ error: Error) {
            let continuation = lock.withLock { pending.removeFirst() }
            continuation.resume(throwing: error)
        }
    }

    private final class TransportQueue: @unchecked Sendable {
        private let lock = NSLock()
        private var transports: [any SessionTransport]
        init(_ transports: [any SessionTransport]) { self.transports = transports }
        func next() -> any SessionTransport { lock.withLock { transports.removeFirst() } }
    }

    private final class TransportSequence: @unchecked Sendable {
        private let lock = NSLock()
        private var transports: [ScriptedTransport]
        init(_ transports: [ScriptedTransport]) { self.transports = transports }
        func next() -> any SessionTransport { lock.withLock { transports.removeFirst() } }
    }

    private func ready(_ session: StationSession, on old: ScriptedTransport,
                       identity: TestStationIdentity, digest: Data) async throws {
        await session.connect()
        let hello = LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0,
                                      peer: "nereusd", majors: [1],
                                      identity: try identity.claim(certificateSHA256: digest))
        await old.deliver(.hello(hello))
        await old.deliver(.authResult(.init(accepted: true, reason: "")))
        await old.deliver(.capabilities(.init(properties: [
            .init(name: "controlSwitchVersion", value: .i64(1)),
        ])))
        await old.deliver(.snapshotComplete)
        #expect(await session.state == .ready)
    }

    private actor TicketGate {
        private var entering: CheckedContinuation<Void, Never>?
        private var leaving: CheckedContinuation<PathTicket, Never>?
        private var entered = false
        private let cancelled = LockFlag()
        private let cancellationWaiters = ConditionWaiters()
        func request() async -> PathTicket {
            entered = true
            entering?.resume()
            entering = nil
            return await withTaskCancellationHandler {
                await withCheckedContinuation { leaving = $0 }
            } onCancel: {
                cancelled.set()
                cancellationWaiters.release()
            }
        }
        func waitEntered() async {
            if entered { return }
            await withCheckedContinuation { entering = $0 }
        }
        func release() {
            leaving?.resume(returning: PathTicket(secret: SessionTransportSwitchTests.ticket,
                                                  expiresInMs: 10_000)!)
            leaving = nil
        }
        func waitCancelled() async -> Bool {
            await cancellationWaiters.wait(within: .milliseconds(200)) { self.cancelled.value }
        }
    }

    private actor SafetyGate {
        private var entering: CheckedContinuation<Void, Never>?
        private var leaving: CheckedContinuation<Bool, Never>?
        private var entered = false
        private let cancelled = LockFlag()
        private let cancellationWaiters = ConditionWaiters()
        func check() async -> Bool {
            entered = true
            entering?.resume()
            entering = nil
            return await withTaskCancellationHandler {
                await withCheckedContinuation { leaving = $0 }
            } onCancel: {
                cancelled.set()
                cancellationWaiters.release()
            }
        }
        func waitEntered() async {
            if entered { return }
            await withCheckedContinuation { entering = $0 }
        }
        func waitCancelled() async -> Bool {
            await cancellationWaiters.wait(within: .milliseconds(200)) { self.cancelled.value }
        }
        func release() { leaving?.resume(returning: true); leaving = nil }
    }

    private actor FinalSafetyGate {
        private var checks = 0
        private var entering: CheckedContinuation<Void, Never>?
        private var leaving: CheckedContinuation<Bool, Never>?
        private var entered = false
        func check() async -> Bool {
            checks += 1
            guard checks == 3 else { return true }
            entered = true
            entering?.resume()
            entering = nil
            return await withCheckedContinuation { leaving = $0 }
        }
        func waitEntered() async {
            if entered { return }
            await withCheckedContinuation { entering = $0 }
        }
        func release() { leaving?.resume(returning: true); leaving = nil }
    }

    private actor CommitGate {
        private var entering: CheckedContinuation<Void, Never>?
        private var leaving: CheckedContinuation<Void, Never>?
        private var entered = false
        func pause() async {
            entered = true
            entering?.resume()
            entering = nil
            await withCheckedContinuation { leaving = $0 }
        }
        func waitEntered() async {
            if entered { return }
            await withCheckedContinuation { entering = $0 }
        }
        func resume() { leaving?.resume(); leaving = nil }
    }

    private actor AvailabilityGate {
        let pauseAt: Int
        private var checks = 0
        private var entering: CheckedContinuation<Void, Never>?
        private var leaving: CheckedContinuation<Void, Never>?
        private var entered = false
        init(pauseAt: Int) { self.pauseAt = pauseAt }
        func check() async {
            checks += 1
            guard checks == pauseAt else { return }
            entered = true
            entering?.resume()
            entering = nil
            await withCheckedContinuation { leaving = $0 }
        }
        func waitEntered() async {
            if entered { return }
            await withCheckedContinuation { entering = $0 }
        }
        func release() { leaving?.resume(); leaving = nil }
    }

    private final class LockFlag: @unchecked Sendable {
        private let lock = NSLock()
        private var held = false
        var value: Bool { lock.withLock { held } }
        func set() { lock.withLock { held = true } }
    }

    private final class CapturedLinkClock: LinkClock, @unchecked Sendable {
        private struct Entry {
            let id: Int
            let after: Int64
            let action: @Sendable () async -> Void
            var cancelled = false
            var started = false
        }
        private struct Timer: LinkTimer {
            let clock: CapturedLinkClock
            let id: Int
            func cancel() { clock.cancel(id) }
        }
        private let lock = NSLock()
        private var entries: [Entry] = []
        private var nextId = 0
        func schedule(after duration: Duration,
                      _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
            let parts = duration.components
            let after = parts.seconds * 1_000 + parts.attoseconds / 1_000_000_000_000_000
            return lock.withLock {
                let id = nextId
                nextId += 1
                entries.append(Entry(id: id, after: after, action: action))
                return Timer(clock: self, id: id)
            }
        }
        private func cancel(_ id: Int) {
            lock.withLock {
                if let index = entries.firstIndex(where: { $0.id == id }) {
                    entries[index].cancelled = true
                }
            }
        }
        func takeStarted(after: Int64) -> (@Sendable () async -> Void)? {
            lock.withLock {
                guard let index = entries.firstIndex(where: {
                    $0.after == after && !$0.cancelled && !$0.started
                }) else { return nil }
                entries[index].started = true
                return entries[index].action
            }
        }
        func activeCount(after: Int64) -> Int {
            lock.withLock { entries.filter { $0.after == after && !$0.cancelled && !$0.started }.count }
        }
    }
}
