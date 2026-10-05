// NereusSDR for iOS: first verified route wins before one device sign-in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin
import NereusKitTesting
import NereusLink
import NereusMedia
@testable import NereusSDR
import Testing

@Suite("Initial route race", .serialized)
struct PathRacerTests {
    @Test(arguments: [PathRacer.Rank.localWebSocket, .directIce, .turn, .webRelay])
    func winnerDiagnosticRankComesFromAdoptedInitialLease(rank: PathRacer.Rank) async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label, endpoints: [])
        let service = FakeRemoteAccess(.reaches(core), relayed: rank >= .turn)
        let clock = TestLinkClock()
        let racer = PathRacer()
        // FakeStation's generated hostname describes no local network. A
        // typed loopback endpoint makes this row local without socket I/O.
        var loopback = in6addr_loopback
        var text = [CChar](repeating: 0, count: Int(INET6_ADDRSTRLEN))
        let capacity = socklen_t(text.count)
        _ = try #require(withUnsafePointer(to: &loopback) { inet_ntop(AF_INET6, $0, &text, capacity) } != nil)
        let local = StationEndpoint(host: String(cString: text))
        let result = await racer.run(direct: rank == .localWebSocket ? [local] : [],
            service: rank == .localWebSocket ? nil : service.maker(paired, device),
            trust: paired.trust, transportFactory: core.transportFactory, clock: clock,
            serviceAddress: FakeRemoteAccess.host, networks: { LocalNetworks(entries: []) },
            serviceRank: { _, _ in rank })
        let winner = try #require(result.winner)
        defer { racer.cancel() }
        #expect(winner.rank == rank)
        #expect(winner.transport.diagnosticServiceRank == nil)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: paired.trust, authenticator: auth, clock: clock,
                                    transport: { winner.transport })
        await session.connect()
        for _ in 0..<50_000 {
            if await session.state == .ready { break }
            await Task.yield()
        }
        #expect(await session.state == .ready)
        racer.transferInitialWinner()
        let snapshot = await session.diagnosticsSnapshot()
        #expect(snapshot.routeID == 0)
        #expect(snapshot.serviceRank == rank.rawValue)
        await session.disconnect()
        #expect(winner.transport.diagnosticServiceRank == nil)
    }

    private final class ResolverGate: @unchecked Sendable {
        private let lock = NSLock()
        private let semaphore = DispatchSemaphore(value: 0)
        private var done = false
        private var started = false
        var finished: Bool { lock.withLock { done } }
        var hasStarted: Bool { lock.withLock { started } }
        func resolve(_ names: [StationEndpoint]) -> [StationEndpoint] {
            lock.withLock { started = true }
            _ = semaphore.wait(timeout: .now() + 10)
            lock.withLock { done = true }
            return names
        }
        func release() { semaphore.signal() }
    }

    private final class SplitResolver: @unchecked Sendable {
        private let lock = NSLock()
        private let semaphore = DispatchSemaphore(value: 0)
        private var resolvedBackup = false
        var backupFinished: Bool { lock.withLock { resolvedBackup } }
        func resolve(_ names: [StationEndpoint]) -> [StationEndpoint] {
            if names.first?.host == "manual.invalid" {
                return [StationEndpoint(host: "2001:db8::1")]
            }
            _ = semaphore.wait(timeout: .now() + 2)
            lock.withLock { resolvedBackup = true }
            return [StationEndpoint(host: "127.0.0.1")]
        }
        func releaseBackup() { semaphore.signal() }
    }

    private final class Hanging: LinkTransport, @unchecked Sendable {
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Data, Error>?
        private var closed = false
        private var opened = false
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await withCheckedThrowingContinuation { continuation in
                let already = lock.withLock { () -> Bool in
                    opened = true
                    if closed { return true }
                    waiting = continuation
                    return false
                }
                if already { continuation.resume(throwing: LinkTransportError.failed("closed")) }
            }
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {
            let taken = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                closed = true
                let taken = waiting
                waiting = nil
                return taken
            }
            taken?.resume(throwing: LinkTransportError.failed("closed"))
        }
        var isClosed: Bool { lock.withLock { closed } }
        var hasOpened: Bool { lock.withLock { opened } }
    }

    private final class OpenFlag: LinkTransport, @unchecked Sendable {
        let inner: any LinkTransport
        private let lock = NSLock()
        private var opened = false
        init(_ inner: any LinkTransport) { self.inner = inner }
        var hasOpened: Bool { lock.withLock { opened } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            lock.withLock { opened = true }
            return try await inner.open(onEvent: onEvent)
        }
        @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() { inner.close() }
    }

    private final class OpeningGate: LinkTransport, @unchecked Sendable {
        private let inner: any LinkTransport
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Void, Never>?
        private var onEvent: (@Sendable (LinkTransportEvent) async -> Void)?
        private var opened = false
        private var completedOpen = false
        private var closed = false
        private var released = false
        init(_ inner: any LinkTransport) { self.inner = inner }
        var hasOpened: Bool { lock.withLock { opened } }
        var hasCompletedOpen: Bool { lock.withLock { completedOpen } }
        var isClosed: Bool { lock.withLock { closed } }
        func release() {
            let taken = lock.withLock { () -> CheckedContinuation<Void, Never>? in
                released = true
                let taken = waiting
                waiting = nil
                return taken
            }
            taken?.resume()
        }
        func endRemotely() async {
            let callback = lock.withLock { onEvent }
            await callback?(.closed)
            close()
        }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            await withCheckedContinuation { continuation in
                let resume = lock.withLock { () -> Bool in
                    opened = true
                    self.onEvent = onEvent
                    if closed || released { return true }
                    waiting = continuation
                    return false
                }
                if resume { continuation.resume() }
            }
            if isClosed { throw LinkTransportError.failed("closed") }
            let digest = try await inner.open(onEvent: onEvent)
            lock.withLock { completedOpen = true }
            return digest
        }
        @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() {
            lock.withLock { closed = true }
            release()
            inner.close()
        }
    }

    private final class IncompatibleHello: LinkTransport, @unchecked Sendable {
        private let inner: any LinkTransport
        init(_ inner: any LinkTransport) { self.inner = inner }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await inner.open { event in
                if case .text(let text) = event, text.contains("\"type\":\"hello\"") {
                    await onEvent(.text(text.replacingOccurrences(of: "\"majors\":[1]",
                                                              with: "\"majors\":[99]")))
                } else {
                    await onEvent(event)
                }
            }
        }
        @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() { inner.close() }
    }

    private final class RaceCheckpoint: @unchecked Sendable {
        private let lock = NSLock()
        private var arrived = false
        private var released = false
        private var arrivalWaiter: CheckedContinuation<Void, Never>?
        private var releaseWaiter: CheckedContinuation<Void, Never>?

        func markArrived() {
            let waiter = lock.withLock { () -> CheckedContinuation<Void, Never>? in
                arrived = true
                let waiter = arrivalWaiter
                arrivalWaiter = nil
                return waiter
            }
            waiter?.resume()
        }

        func waitForArrival() async {
            await withCheckedContinuation { continuation in
                let ready = lock.withLock { () -> Bool in
                    if arrived { return true }
                    arrivalWaiter = continuation
                    return false
                }
                if ready { continuation.resume() }
            }
        }

        func pause() async {
            markArrived()
            await withCheckedContinuation { continuation in
                let ready = lock.withLock { () -> Bool in
                    if released { return true }
                    releaseWaiter = continuation
                    return false
                }
                if ready { continuation.resume() }
            }
        }

        func release() {
            let waiter = lock.withLock { () -> CheckedContinuation<Void, Never>? in
                released = true
                let waiter = releaseWaiter
                releaseWaiter = nil
                return waiter
            }
            waiter?.resume()
        }
    }

    @Test func verifiedCompletionQueuedBeforeFinishMarkerBecomesStandby() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "127.0.0.1")])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let better = OpeningGate(core.transportFactory(paired.endpoints[0], paired.trust))
        let workerPaused = RaceCheckpoint()
        let completionQueued = RaceCheckpoint()
        let finishMarked = RaceCheckpoint()
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: { _, _ in better },
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     retainBetterStandby: true,
                                     testHooks: .init(afterFirstWinnerPublished: { await workerPaused.pause() },
                                                      afterCompletionQueued: { row in
                                                          if row == 1 { completionQueued.markArrived() }
                                                      },
                                                      afterFinishRequested: { finishMarked.markArrived() }))
        #expect(result.winner?.rank == .turn)
        await workerPaused.waitForArrival()
        better.release()
        await completionQueued.waitForArrival()
        let finishing = Task { await racer.finishInitialRace(currentRank: .turn) }
        await finishMarked.waitForArrival()
        workerPaused.release()
        let standby = await finishing.value
        #expect(standby?.rank == .localWebSocket)
        #expect(standby?.row == 1)
        #expect(!better.isClosed)
        #expect(core.messages.isEmpty)
        racer.cancel()
        #expect(!better.isClosed)
        standby?.transport.close()
    }

    @Test func finishRequestedDuringSuspendedHealthCheckNeverStartsPendingDirect() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "198.51.100.7"),
                                               StationEndpoint(host: "127.0.0.1"),
                                               StationEndpoint(host: "127.0.0.2")])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let clock = TestLinkClock()
        let remote = OpeningGate(core.transportFactory(paired.endpoints[0], paired.trust))
        let occupied = OpeningGate(core.transportFactory(paired.endpoints[1], paired.trust))
        let pending = OpenFlag(core.transportFactory(paired.endpoints[2], paired.trust))
        let healthPaused = RaceCheckpoint()
        let finishMarked = RaceCheckpoint()
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: { endpoint, _ in
                                         switch endpoint.host {
                                         case "198.51.100.7": remote
                                         case "127.0.0.1": occupied
                                         default: pending
                                         }
                                     }, clock: clock, serviceAddress: FakeRemoteAccess.host,
                                     retainBetterStandby: true,
                                     testHooks: .init(beforeStandbyHealthAvailability: { await healthPaused.pause() },
                                                      afterFinishRequested: { finishMarked.markArrived() }))
        #expect(result.winner?.rank == .turn)
        remote.release()
        let watching = Date().addingTimeInterval(2)
        while !clock.pendingDueTimes.contains(1_000) && Date() < watching { await Task.yield() }
        #expect(clock.pendingDueTimes.contains(1_000))
        await remote.endRemotely()
        await clock.advance(by: 1_000)
        await healthPaused.waitForArrival()
        #expect(!pending.hasOpened)
        let finishing = Task { await racer.finishInitialRace(currentRank: .turn) }
        await finishMarked.waitForArrival()
        healthPaused.release()
        let standby = await finishing.value
        #expect(standby == nil)
        #expect(!pending.hasOpened)
        #expect(occupied.isClosed)
        racer.cancel()
    }

    @Test func aBetterDirectRungOpeningDuringSignInIsTransferredAtSnapshot() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "198.51.100.7"),
                                               StationEndpoint(host: "127.0.0.1")])
        let better = OpeningGate(core.transportFactory(paired.endpoints[1], paired.trust))
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: nil, trust: paired.trust,
                                     transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1" ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        let first = try #require(result.winner)
        #expect(first.rank == .otherWebSocket)
        #expect(!better.isClosed)
        let wait = Date().addingTimeInterval(2)
        while !better.hasOpened && Date() < wait { await Task.yield() }
        #expect(better.hasOpened)
        better.release()
        let verified = Date().addingTimeInterval(2)
        while !better.hasCompletedOpen && Date() < verified { await Task.yield() }
        #expect(better.hasCompletedOpen)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: paired.trust, authenticator: auth,
                                     transport: { first.transport })
        await session.connect()
        racer.transferInitialWinner()
        let standby = await racer.finishInitialRace(currentRank: first.rank)
        #expect(standby?.rank == .localWebSocket)
        #expect(standby?.row != first.row)
        #expect(!better.isClosed)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        racer.cancel()
        #expect(!better.isClosed)
        await session.disconnect()
        standby?.transport.close()
    }

    @Test func aWorseRungClosesWhenTheFirstWinnerIsAlreadyBest() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let worse = OpeningGate(core.transportFactory(StationEndpoint(host: "198.51.100.8"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "127.0.0.1"),
                                              StationEndpoint(host: "198.51.100.8")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? core.transportFactory(endpoint, trust) : worse
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .localWebSocket)
        #expect(worse.isClosed)
        #expect(await racer.finishInitialRace() == nil)
        #expect(await racer.finishInitialRace() == nil)
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test func snapshotBeforeBetterRungOpensClosesThatRungWithoutAuthentication() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .otherWebSocket)
        #expect(await racer.finishInitialRace() == nil)
        #expect(better.isClosed)
        better.release()
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test func finishingTheRaceReportsWhatBecameOfEveryRung() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        let winnerRow = try #require(result.winner?.row)
        let stillOpening = try #require(result.tries.indices.first { $0 != winnerRow })
        #expect(result.tries[stillOpening].outcome == .trying)
        #expect(racer.settledTries == nil)
        #expect(await racer.finishInitialRace() == nil)
        let settled = try #require(racer.settledTries)
        #expect(settled.count == result.tries.count)
        #expect(settled[stillOpening].outcome == .cancelled)
        better.release()
        racer.cancel()
    }

    @Test func twoDirectLeasesRemainTheOpeningCapUntilSnapshot() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let third = OpenFlag(core.transportFactory(StationEndpoint(host: "127.0.0.2"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1"),
                                              StationEndpoint(host: "127.0.0.2")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         switch endpoint.host {
                                         case "127.0.0.1": better
                                         case "127.0.0.2": third
                                         default: core.transportFactory(endpoint, trust)
                                         }
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .otherWebSocket)
        let wait = Date().addingTimeInterval(2)
        while !better.hasOpened && Date() < wait { await Task.yield() }
        #expect(better.hasOpened)
        #expect(!third.hasOpened)
        #expect(await racer.finishInitialRace() == nil)
        #expect(!third.hasOpened)
        racer.cancel()
    }

    @Test func aWrongIdentityBetterRungCannotBecomeStandby() async throws {
        let correct = try FakeStation(fixture: "session-device-sign-in")
        let wrong = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: correct.identity.publicKey)
        let better = OpeningGate(wrong.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : correct.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .otherWebSocket)
        better.release()
        let deadline = Date().addingTimeInterval(2)
        while !better.isClosed && Date() < deadline { await Task.yield() }
        #expect(better.isClosed)
        #expect(await racer.finishInitialRace() == nil)
        #expect(wrong.messages.isEmpty)
        racer.cancel()
    }

    @Test func serviceRankCallbackCanClassifyAWebRelay() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let racer = PathRacer()
        let result = await racer.run(direct: [], service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     retainBetterStandby: true,
                                     serviceRank: { _, _ in .webRelay })
        #expect(result.winner?.rank == .webRelay)
        #expect(await racer.finishInitialRace() == nil)
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test func betterOnlyLookRejectsAnEqualRankBeforeReturningTheLocalLease() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                               StationEndpoint(host: "127.0.0.1")],
                                     service: nil, trust: trust,
                                     transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: "",
                                     betterThan: .otherWebSocket)
        #expect(result.winner?.rank == .localWebSocket)
        #expect(core.messages.isEmpty)
        result.winner?.transport.close()
        racer.cancel()
    }

    @Test func betterOnlyLookNeverReturnsATurnOrWebRelayLease() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label, endpoints: [])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let racer = PathRacer()
        let result = await racer.run(direct: [], service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     betterThan: .turn)
        #expect(result.winner == nil)
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test func incompatibleLateHelloDoesNotEndTheSigningInWinner() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(IncompatibleHello(core.transportFactory(
            StationEndpoint(host: "127.0.0.1"), trust)))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        let first = try #require(result.winner)
        better.release()
        let deadline = Date().addingTimeInterval(2)
        while !better.isClosed && Date() < deadline { await Task.yield() }
        #expect(better.isClosed)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: trust, authenticator: auth,
                                     transport: { first.transport })
        await session.connect()
        racer.transferInitialWinner()
        #expect(await racer.finishInitialRace() == nil)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        await session.disconnect()
        racer.cancel()
    }

    @Test func expiredVerifiedStandbyIsNeverTransferred() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let clock = TestLinkClock()
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: clock, serviceAddress: "",
                                     retainBetterStandby: true)
        let first = try #require(result.winner)
        better.release()
        let opened = Date().addingTimeInterval(2)
        while !better.hasCompletedOpen && Date() < opened { await Task.yield() }
        #expect(better.hasCompletedOpen)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: trust, authenticator: auth,
                                     transport: { first.transport })
        await session.connect()
        racer.transferInitialWinner()
        await clock.advance(by: 30_000)
        #expect(better.isClosed)
        #expect(await racer.finishInitialRace() == nil)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        await session.disconnect()
        racer.cancel()
    }

    @Test func cancellationAfterFirstWinnerClosesBothOwnedLeases() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        let first = try #require(result.winner)
        racer.cancel()
        #expect(!(await first.transport.isAvailable()))
        #expect(better.isClosed)
        #expect(await racer.finishInitialRace() == nil)
        better.release()
        #expect(core.messages.isEmpty)
    }

    @Test func cancellationAfterFirstWinnerTransferPreservesTheAdoptedSession() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let trust = StationTrust.identity(publicKey: core.identity.publicKey)
        let better = OpeningGate(core.transportFactory(StationEndpoint(host: "127.0.0.1"), trust))
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "198.51.100.7"),
                                              StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: trust, transportFactory: { endpoint, trust in
                                         endpoint.host == "127.0.0.1"
                                             ? better : core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     retainBetterStandby: true)
        let first = try #require(result.winner)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: trust, authenticator: auth,
                                     transport: { first.transport })
        await session.connect()
        racer.transferInitialWinner()
        racer.cancel()
        #expect(await racer.finishInitialRace() == nil)
        #expect(await first.transport.isAvailable())
        #expect(better.isClosed)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        await session.disconnect()
    }

    @Test func aStillBetterVerifiedRungReplacesThePriorStandby() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "198.51.100.7"),
                                               StationEndpoint(host: "127.0.0.1")])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let remote = OpeningGate(core.transportFactory(paired.endpoints[0], paired.trust))
        let local = OpeningGate(core.transportFactory(paired.endpoints[1], paired.trust))
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: { endpoint, _ in
                                         endpoint.host == "127.0.0.1" ? local : remote
                                     }, clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .turn)
        remote.release()
        let opened = Date().addingTimeInterval(2)
        while !remote.hasCompletedOpen && Date() < opened { await Task.yield() }
        #expect(remote.hasCompletedOpen)
        local.release()
        let replaced = Date().addingTimeInterval(2)
        while !remote.isClosed && Date() < replaced { await Task.yield() }
        #expect(remote.isClosed)
        let standby = await racer.finishInitialRace()
        #expect(standby?.rank == .localWebSocket)
        #expect(!local.isClosed)
        #expect(core.messages.isEmpty)
        racer.cancel()
        #expect(!local.isClosed)
        standby?.transport.close()
    }

    @Test func aClosedStandbyReleasesItsDirectOpeningSlot() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "198.51.100.7"),
                                               StationEndpoint(host: "127.0.0.1"),
                                               StationEndpoint(host: "127.0.0.2")])
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let clock = TestLinkClock()
        let remote = OpeningGate(core.transportFactory(paired.endpoints[0], paired.trust))
        let occupied = OpeningGate(core.transportFactory(paired.endpoints[1], paired.trust))
        let waiting = OpenFlag(core.transportFactory(paired.endpoints[2], paired.trust))
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: { endpoint, _ in
                                         switch endpoint.host {
                                         case "198.51.100.7": remote
                                         case "127.0.0.1": occupied
                                         default: waiting
                                         }
                                     }, clock: clock, serviceAddress: FakeRemoteAccess.host,
                                     retainBetterStandby: true)
        #expect(result.winner?.rank == .turn)
        remote.release()
        let watching = Date().addingTimeInterval(2)
        while !clock.pendingDueTimes.contains(1_000) && Date() < watching { await Task.yield() }
        #expect(clock.pendingDueTimes.contains(1_000))
        #expect(!waiting.hasOpened)
        await remote.endRemotely()
        await clock.advance(by: 1_000)
        let launched = Date().addingTimeInterval(2)
        while !waiting.hasOpened && Date() < launched { await Task.yield() }
        #expect(waiting.hasOpened)
        #expect(occupied.hasOpened)
        let standby = await racer.finishInitialRace()
        racer.cancel()
        standby?.transport.close()
    }

    @Test func stalledPrivateAddressDoesNotDelayServiceAndKeepsItsIceContext() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.reaches(core), relayed: true)
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "192.0.2.10")])
        let route = service.maker(paired, device)
        let stalled = Hanging()
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: route,
                                     trust: paired.trust, transportFactory: { _, _ in stalled },
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host)
        let winner = try #require(result.winner)
        guard case .service = winner.route else { Issue.record("service did not win"); return }
        #expect(winner.path == .relay)
        #expect(winner.mediaIce == service.ice)
        #expect(stalled.isClosed)
        #expect(core.messages.isEmpty)
        #expect(result.tries[winner.row].outcome == .trying)
        #expect(result.tries.contains { $0.outcome == .cancelled })
        racer.cancel()
    }

    @Test func simultaneousVerifiedPathsSendOneAuthenticationOnlyAfterSelection() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [StationEndpoint(host: "127.0.0.1"),
                                               StationEndpoint(host: "127.0.0.2")])
        let service = FakeRemoteAccess(.reaches(core))
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host)
        let winner = try #require(result.winner)
        #expect(core.messages.isEmpty)
        let auth = try DeviceKeyAuthenticator(identity: device, name: "Phone", kind: .phone)
        let session = StationSession(trust: paired.trust, authenticator: auth,
                                     transport: { winner.transport })
        await session.connect()
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        #expect(result.tries.filter { $0.outcome == .cancelled }.count >= 1)
        await session.disconnect()
        racer.cancel()
    }

    @Test func wrongCoreDirectGetsNoCredentialWhileCorrectServiceCanWin() async throws {
        let correct = try FakeStation(fixture: "session-device-sign-in")
        let wrong = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: correct.identity.publicKey, label: correct.label,
                                   endpoints: [StationEndpoint(host: "127.0.0.1")])
        let service = FakeRemoteAccess(.reaches(correct))
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: wrong.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host)
        let winner = try #require(result.winner)
        guard case .service = winner.route else { Issue.record("correct service did not win"); return }
        #expect(wrong.messages.isEmpty)
        #expect(result.tries.contains { $0.outcome == .notThisCore || $0.outcome == .cancelled })
        racer.cancel()
    }

    @Test func wrongCoreWithoutBackupIsRejectedBeforeAuthentication() async throws {
        let correct = try FakeStation(fixture: "session-device-sign-in")
        let wrong = try FakeStation(fixture: "session-device-sign-in")
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: .identity(publicKey: correct.identity.publicKey),
                                     transportFactory: wrong.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: "")
        #expect(result.winner == nil)
        #expect(result.tries.map(\.outcome) == [.notThisCore])
        #expect(wrong.messages.isEmpty)
    }

    @Test func cancellationClosesAnOpeningDirectRung() async {
        let core = try? FakeStation(fixture: "session-device-sign-in")
        guard let core else { Issue.record("fixture missing"); return }
        let stalled = Hanging()
        let racer = PathRacer()
        let running = Task {
            await racer.run(direct: [StationEndpoint(host: "127.0.0.1")], service: nil,
                            trust: .identity(publicKey: core.identity.publicKey),
                            transportFactory: { _, _ in stalled }, clock: TestLinkClock(), serviceAddress: "")
        }
        let deadline = Date().addingTimeInterval(2)
        while !stalled.hasOpened && Date() < deadline {
            await Task.yield()
        }
        #expect(stalled.hasOpened)
        racer.cancel()
        let result = await running.value
        #expect(result.winner == nil)
        #expect(stalled.isClosed)
    }

    @Test func cancellationBeforeRaceStartsNeverOpensOrAuthenticates() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let stalled = Hanging()
        let racer = PathRacer()
        racer.cancel()
        let result = await racer.run(direct: [StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: { _, _ in stalled }, clock: TestLinkClock(), serviceAddress: "")
        #expect(result.winner == nil)
        #expect(!stalled.hasOpened)
        #expect(core.messages.isEmpty)
    }

    @Test func namesAndScopedAddressesDeduplicateByResolvedSocket() {
        let aliases = DirectRouteResolver.resolve([StationEndpoint(host: "localhost"),
                                                   StationEndpoint(host: "LOCALHOST."),
                                                   StationEndpoint(host: "127.0.0.1")])
        #expect(Set(aliases.map(\.canonical)).count == aliases.count)
        #expect(aliases.filter { $0.host == "127.0.0.1" }.count == 1)
        let scoped = DirectRouteResolver.resolve([StationEndpoint(host: "fe80::1%1"),
                                                  StationEndpoint(host: "fe80::1%2")])
        #expect(scoped.count == 2)
    }

    @Test func aFakeSavedHostnameDialsWithItsInjectedResolver() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let racer = PathRacer()
        let result = await racer.run(direct: [core.endpoint], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: "",
                                     resolveNames: { $0 })
        let winner = try #require(result.winner)
        guard case .address(let endpoint) = winner.route else {
            Issue.record("saved hostname did not connect")
            return
        }
        #expect(endpoint == core.endpoint)
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test func aBlockedNameResolverCannotDelayTheService() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let saved = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                  endpoints: [core.endpoint])
        let service = FakeRemoteAccess(.reaches(core))
        let gate = ResolverGate()
        let racer = PathRacer()
        let result = await racer.run(direct: saved.endpoints, service: service.maker(saved, device),
                                     trust: saved.trust, transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     resolveNames: { gate.resolve($0) })
        guard case .service? = result.winner?.route else {
            gate.release()
            Issue.record("service waited for hostname resolution")
            return
        }
        #expect(!gate.finished)
        gate.release()
        racer.cancel()
    }

    @Test func anExplicitHostnameUsesAReservedDirectSlotAheadOfStaleSavedAddresses() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let stale = Hanging()
        let chosen = core.endpoint
        let racer = PathRacer()
        let result = await racer.run(direct: [chosen, StationEndpoint(host: "192.0.2.10"),
                                              StationEndpoint(host: "192.0.2.11")], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: { endpoint, trust in
                                         endpoint == chosen ? core.transportFactory(endpoint, trust) : stale
                                     }, clock: TestLinkClock(), serviceAddress: "", preferredFirst: true,
                                     resolveNames: { $0 })
        guard case .address(let endpoint)? = result.winner?.route else {
            Issue.record("explicit hostname did not win")
            return
        }
        #expect(endpoint == chosen)
        #expect(core.messages.isEmpty)
        #expect(stale.isClosed)
        racer.cancel()
    }

    @Test func ipv4StartsAfterStaggerDespiteTwoStalledIpv6Addresses() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let firstV6 = Hanging()
        let secondV6 = Hanging()
        let racer = PathRacer()
        let clock = TestLinkClock()
        let ipv4 = OpenFlag(core.transportFactory(StationEndpoint(host: "127.0.0.1"),
                                                  .identity(publicKey: core.identity.publicKey)))
        let running = Task {
            await racer.run(direct: [StationEndpoint(host: "2001:db8::1"),
                                     StationEndpoint(host: "2001:db8::2"),
                                     StationEndpoint(host: "127.0.0.1")], service: nil,
                            trust: .identity(publicKey: core.identity.publicKey),
                            transportFactory: { endpoint, trust in
                                switch endpoint.host {
                                case "2001:db8::1": firstV6
                                case "2001:db8::2": secondV6
                                default: ipv4
                                }
                            }, clock: clock, serviceAddress: "")
        }
        let launched = Date().addingTimeInterval(2)
        while !firstV6.hasOpened && Date() < launched { await Task.yield() }
        #expect(firstV6.hasOpened)
        #expect(!ipv4.hasOpened)
        await clock.advance(by: 249)
        #expect(!ipv4.hasOpened)
        await clock.advance(by: 1)
        let deadline = Date().addingTimeInterval(2)
        while !ipv4.hasOpened && Date() < deadline { await Task.yield() }
        let ipv4StartedPromptly = ipv4.hasOpened
        if !ipv4StartedPromptly { racer.cancel() }
        let result = await running.value
        #expect(ipv4StartedPromptly)
        #expect(result.winner != nil)
        #expect(firstV6.hasOpened)
        #expect(!secondV6.hasOpened)
        #expect(firstV6.isClosed)
        racer.cancel()
    }

    @Test func selectedHostnameAliasPromotesItsSavedSocketWithoutADuplicateOpen() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let staleV6 = Hanging()
        let staleV4 = Hanging()
        let chosen = StationEndpoint(host: "127.0.0.1")
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "manual.invalid"),
                                              StationEndpoint(host: "2001:db8::1"), chosen,
                                              StationEndpoint(host: "192.0.2.10")], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: { endpoint, trust in
                                         switch endpoint.host {
                                         case chosen.host: core.transportFactory(endpoint, trust)
                                         case "2001:db8::1": staleV6
                                         default: staleV4
                                         }
                                     }, clock: TestLinkClock(), serviceAddress: "", preferredFirst: true,
                                     resolveNames: { _ in [chosen] })
        guard case .address(let endpoint)? = result.winner?.route else {
            Issue.record("selected alias did not win")
            return
        }
        #expect(endpoint == chosen)
        #expect(!staleV6.hasOpened)
        #expect(result.routes.compactMap { route -> StationEndpoint? in
            if case .address(let endpoint) = route { return endpoint }
            return nil
        }.filter { $0 == chosen }.count == 1)
        racer.cancel()
    }

    @Test func splitDnsIpv4UsesTheFirstIpv6LaunchGate() async throws {
        for backupResolvesLate in [false, true] {
            let core = try FakeStation(fixture: "session-device-sign-in")
            let ipv6 = Hanging()
            let resolver = SplitResolver()
            let clock = TestLinkClock()
            let racer = PathRacer()
            let ipv4 = OpenFlag(core.transportFactory(StationEndpoint(host: "127.0.0.1"),
                                                      .identity(publicKey: core.identity.publicKey)))
            let running = Task {
                await racer.run(direct: [StationEndpoint(host: "manual.invalid"),
                                         StationEndpoint(host: "backup.invalid")], service: nil,
                                trust: .identity(publicKey: core.identity.publicKey),
                                transportFactory: { endpoint, trust in
                                    if endpoint.host == "127.0.0.1" { ipv4 } else { ipv6 }
                                }, clock: clock, serviceAddress: "", preferredFirst: true,
                                resolveNames: { resolver.resolve($0) })
            }
            let deadline = Date().addingTimeInterval(2)
            while !ipv6.hasOpened && Date() < deadline { await Task.yield() }
            #expect(ipv6.hasOpened)
            if backupResolvesLate {
                await clock.advance(by: 250)
                resolver.releaseBackup()
            } else {
                resolver.releaseBackup()
                let resolved = Date().addingTimeInterval(2)
                while !resolver.backupFinished && Date() < resolved { await Task.yield() }
                #expect(resolver.backupFinished)
                await clock.advance(by: 249)
                #expect(!ipv4.hasOpened)
                await clock.advance(by: 1)
            }
            let result = await running.value
            #expect(result.winner != nil)
            #expect(ipv4.hasOpened)
            racer.cancel()
        }
    }

    @Test func aBlockedResolverWithoutServiceEndsAtTheLogicalDnsDeadline() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let saved = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                  endpoints: [core.endpoint])
        let gate = ResolverGate()
        let clock = TestLinkClock()
        let racer = PathRacer()
        let running = Task {
            await racer.run(direct: saved.endpoints, service: nil,
                            trust: saved.trust, transportFactory: core.transportFactory,
                            clock: clock, serviceAddress: FakeRemoteAccess.host,
                            resolveNames: { gate.resolve($0) })
        }
        let wait = Date().addingTimeInterval(2)
        while (!gate.hasStarted || clock.pendingDueTimes.isEmpty) && Date() < wait { await Task.yield() }
        #expect(gate.hasStarted)
        #expect(!clock.pendingDueTimes.isEmpty)
        await clock.advance(by: 30_000)
        let result = await running.value
        #expect(result.winner == nil)
        #expect(result.tries.contains { $0.address == ConnectionFlow.addressText(core.endpoint) &&
            $0.outcome == .timedOut })
        #expect(!gate.finished)
        gate.release()
    }

    // MARK: The network the phone is on (R-IOS-16, JJ's build 13 on 5G, 2026-09-29)

    private final class Failing: LinkTransport, @unchecked Sendable {
        let error: LinkTransportError
        init(_ error: LinkTransportError) { self.error = error }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            throw error
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    /// The rows whose completion the racer has queued.
    private final class CompletedRows: @unchecked Sendable {
        private let lock = NSLock()
        private var queued: Set<Int> = []
        func add(_ row: Int) { lock.withLock { _ = queued.insert(row) } }
        var rows: Set<Int> { lock.withLock { queued } }
    }

    private final class Dialled: @unchecked Sendable {
        private let lock = NSLock()
        private var list: [String] = []
        func add(_ host: String) { lock.withLock { list.append(host) } }
        var hosts: [String] { lock.withLock { list } }
    }

    private static func v6(_ groups: [UInt16]) -> [UInt8] {
        groups.flatMap { [UInt8($0 >> 8), UInt8($0 & 0xFF)] }
    }

    /// Cellular behind NAT64: the carrier's IPv6 prefix and the CLAT address.
    private static let cellular = LocalNetworks(entries: [
        LocalNetworks.Entry(address: v6([0x2001, 0xdb8, 0x7700, 0x48, 0, 0, 0, 5]), prefixLength: 64),
        LocalNetworks.Entry(address: [192, 0, 0, 2], prefixLength: 32),
    ])
    /// The Core's home Wi-Fi.
    private static let home = LocalNetworks(entries: [
        LocalNetworks.Entry(address: [192, 168, 109, 40], prefixLength: 24),
        LocalNetworks.Entry(address: v6([0x2001, 0xdb8, 0x467f, 0x66e7, 0, 0, 0, 0x40]), prefixLength: 64),
    ])
    private static let lanAddress = StationEndpoint(host: "192.168.109.106", port: 50055)
    private static let globalAddress = StationEndpoint(host: "2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2", port: 50055)

    @Test func anIPv4LiteralIsNeverReplacedByItsNat64Synthesis() {
        let synthesised: @Sendable (String) -> [String]? = { host in
            host == "192.168.109.106" ? ["2001:db8:7700:48::c0a8:6d6a"] : nil
        }
        #expect(DirectRouteResolver.resolve([Self.lanAddress], addresses: synthesised) == [Self.lanAddress])
        let named = StationEndpoint(host: "rock.example.net", port: 50055)
        let dns64: @Sendable (String) -> [String]? = { _ in ["2001:db8:7700:48::cb00:7143"] }
        #expect(DirectRouteResolver.resolve([named], addresses: dns64)
                == [StationEndpoint(host: "2001:db8:7700:48::cb00:7143", port: 50055)])
    }

    @Test func offTheCoresNetworkItsPrivateAddressIsNotRacedAndItsGlobalIPv6Is() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let saved = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                  endpoints: [Self.lanAddress, Self.globalAddress])
        let dialled = Dialled()
        let racer = PathRacer()
        let result = await racer.run(direct: saved.dialOrder, service: nil, trust: saved.trust,
                                     transportFactory: { endpoint, trust in
                                         dialled.add(endpoint.host)
                                         return core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     networks: { Self.cellular })
        #expect(dialled.hosts == [Self.globalAddress.host])
        #expect(result.tries.map(\.address) == [ConnectionFlow.addressText(Self.globalAddress)])
        #expect(result.winner?.rank == .otherWebSocket)
        #expect(result.tries.first?.path == .direct)
        racer.cancel()
    }

    @Test func onTheCoresNetworkItsPrivateAddressIsRacedAsThisNetwork() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let dialled = Dialled()
        let racer = PathRacer()
        let result = await racer.run(direct: [Self.lanAddress], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: { endpoint, trust in
                                         dialled.add(endpoint.host)
                                         return core.transportFactory(endpoint, trust)
                                     }, clock: TestLinkClock(), serviceAddress: "",
                                     networks: { Self.home })
        #expect(dialled.hosts == [Self.lanAddress.host])
        #expect(result.tries.first?.path == .thisNetwork)
        #expect(result.winner?.rank == .localWebSocket)
        racer.cancel()
    }

    @Test func anOperatorsChosenPrivateAddressIsStillDialled() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let dialled = Dialled()
        let racer = PathRacer()
        _ = await racer.run(direct: [Self.lanAddress], service: nil,
                            trust: .identity(publicKey: core.identity.publicKey),
                            transportFactory: { endpoint, trust in
                                dialled.add(endpoint.host)
                                return core.transportFactory(endpoint, trust)
                            }, clock: TestLinkClock(), serviceAddress: "", preferredFirst: true,
                            networks: { Self.cellular })
        #expect(dialled.hosts == [Self.lanAddress.host])
        racer.cancel()
    }

    @Test func aFailedDirectPathSaysWhy() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let refused = StationEndpoint(host: "2001:db8::1", port: 50055)
        let unreachable = StationEndpoint(host: "2001:db8::2", port: 50055)
        let silent = StationEndpoint(host: "203.0.113.5", port: 50055)
        let quiet = Hanging()
        let clock = TestLinkClock()
        let racer = PathRacer()
        let finished = CompletedRows()
        let running = Task {
            await racer.run(direct: [refused, unreachable, silent], service: nil,
                            trust: .identity(publicKey: core.identity.publicKey),
                            transportFactory: { endpoint, _ in
                                switch endpoint.host {
                                case refused.host: Failing(.refused)
                                case unreachable.host: Failing(.unreachable)
                                default: quiet
                                }
                            }, clock: clock, serviceAddress: "", networks: { Self.cellular },
                            testHooks: .init(afterCompletionQueued: { row in finished.add(row) }))
        }
        // Time moves only to the IPv4 stagger (250 ms), never toward a
        // rung's 30 s connect deadline while the rungs still run in real
        // time: under load, a rung that has armed its deadline but not yet
        // reported its failure would otherwise be answered by the deadline.
        // The refused and unreachable rungs (rows 0 and 1) report first.
        let wait = Date().addingTimeInterval(5)
        while !(quiet.hasOpened && finished.rows.isSuperset(of: [0, 1])) && Date() < wait {
            if let next = clock.pendingDueTimes.first, next < 5_000 {
                await clock.advance(by: max(0, next - clock.now))
            }
            await Task.yield()
        }
        #expect(quiet.hasOpened)
        #expect(finished.rows.isSuperset(of: [0, 1]), "finished rows: \(finished.rows.sorted())")
        await clock.advance(by: 31_000)
        let result = await running.value
        #expect(result.winner == nil)
        let outcomes = Dictionary(uniqueKeysWithValues: result.tries.map { ($0.address, $0.outcome) })
        #expect(outcomes[ConnectionFlow.addressText(refused)] == .refused)
        #expect(outcomes[ConnectionFlow.addressText(unreachable)] == .unreachable)
        #expect(outcomes[ConnectionFlow.addressText(silent)] == .timedOut)
    }

    @Test func theServicePathIsMarkedAndKeepsWhatEachEndOffered() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: core.identity.publicKey, label: core.label,
                                   endpoints: [Self.lanAddress])
        let service = FakeRemoteAccess(.reaches(core))
        service.evidence = IceCandidateEvidence(
            phone: ["candidate:1 1 UDP 1686052607 198.51.100.2 4000 typ srflx raddr 192.0.0.2 rport 4000"],
            core: ["candidate:3 1 UDP 1686052607 203.0.113.67 50001 typ srflx raddr 192.168.109.106 rport 50001"],
            chosen: nil, ending: .connected)
        let racer = PathRacer()
        let result = await racer.run(direct: paired.endpoints, service: service.maker(paired, device),
                                     trust: paired.trust, transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: FakeRemoteAccess.host,
                                     networks: { Self.cellular })
        let row = try #require(result.tries.first)
        #expect(result.tries.count == 1)
        #expect(row.throughService)
        // The winner's row stays trying: the session that adopts it owns it.
        #expect(row.outcome == .trying)
        #expect(row.ice == service.evidence)
        var finished = ConnectionAttempt(tries: result.tries)
        finished.end(0, as: .connected)
        #expect(finished.summary == "Tried through the internet service (rv.nereussdr.com): connected.")
        racer.cancel()
    }
}
