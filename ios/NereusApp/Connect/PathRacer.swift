// NereusSDR for iOS: choose one verified control connection before signing in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia

/// The initial Task 29a race. The rungs send nothing. The first compatible
/// hello bound to the saved Core identity transfers its live connection to
/// one session. In standby mode, a better verified rung may remain open until
/// the session reaches its snapshot; the racer never authenticates it.
final class PathRacer: @unchecked Sendable {
    /// Optional checkpoints used to make snapshot races reproducible in tests.
    struct TestHooks: Sendable {
        let afterFirstWinnerPublished: (@Sendable () async -> Void)?
        let afterCompletionQueued: (@Sendable (Int) -> Void)?
        let beforeStandbyHealthAvailability: (@Sendable () async -> Void)?
        let afterFinishRequested: (@Sendable () -> Void)?

        init(afterFirstWinnerPublished: (@Sendable () async -> Void)? = nil,
             afterCompletionQueued: (@Sendable (Int) -> Void)? = nil,
             beforeStandbyHealthAvailability: (@Sendable () async -> Void)? = nil,
             afterFinishRequested: (@Sendable () -> Void)? = nil) {
            self.afterFirstWinnerPublished = afterFirstWinnerPublished
            self.afterCompletionQueued = afterCompletionQueued
            self.beforeStandbyHealthAvailability = beforeStandbyHealthAvailability
            self.afterFinishRequested = afterFinishRequested
        }
    }

    enum Rank: Int, Sendable, Comparable {
        case localWebSocket = 0
        case otherWebSocket = 1
        case directIce = 2
        case turn = 3
        case webRelay = 4

        static func < (lhs: Rank, rhs: Rank) -> Bool { lhs.rawValue < rhs.rawValue }
    }

    enum Route: Sendable {
        case address(StationEndpoint)
        case service(any CoreServiceRoute)
    }

    struct Winner: Sendable {
        let transport: PreauthenticatedTransport
        let route: Route
        let path: ConnectionAttempt.Path
        let mediaIce: IceSettings?
        let row: Int
        let rank: Rank
    }

    struct Result: Sendable {
        let winner: Winner?
        let tries: [ConnectionAttempt.Try]
        let routes: [Route]
        let incompatible: LinkMessage.Hello?
        let localNetworkDenied: Bool
    }

    private struct Candidate: Sendable {
        let row: Int
        let route: Route
        let transport: PreauthenticatedTransport
        let initial: ConnectionAttempt.Try
        var isIpv4: Bool {
            guard case .address(let endpoint) = route else { return false }
            let family = AddressFamilies.ofLiteral(endpoint.host)
            return family.known && !family.ipv6
        }
    }

    private struct Completion: Sendable {
        let candidate: Candidate
        let outcome: ConnectionAttempt.Outcome
        let hello: LinkMessage.Hello?
        let path: ConnectionAttempt.Path
        let ice: IceSettings?
        let denied: Bool
        var evidence: IceCandidateEvidence? = nil
    }

    private enum Update: Sendable {
        case completed(Completion)
        case namesResolved([StationEndpoint], preferred: Bool)
        case resolutionTimedOut
        case ipv4StaggerReady
        case standbyDeadline(Int)
        case standbyHealthCheck(Int)
        case finishInitialRace
    }

    private let lock = NSLock()
    private var candidates: [Candidate] = []
    private var cancelled = false
    private var stream: AsyncStream<Update>.Continuation?
    private var standbyTask: Task<Result, Never>?
    private var finishRequested = false
    private var finishClaimed = false
    private var initialWinnerRow: Int?
    private var initialWinnerRank: Rank?
    private var testHooks: TestHooks?
    private var finishedTries: [ConnectionAttempt.Try]?

    /// Every rung's final outcome, once ``finishInitialRace(currentRank:)``
    /// has ended the race; nil before. The first winner's row stays
    /// `.trying`, since the session that adopted it owns its outcome.
    var settledTries: [ConnectionAttempt.Try]? { lock.withLock { finishedTries } }

    func cancel() {
        let (closing, stream) = lock.withLock { () -> ([Candidate], AsyncStream<Update>.Continuation?) in
            cancelled = true
            return (candidates, self.stream)
        }
        for candidate in closing { candidate.transport.close() }
        stream?.finish()
    }

    private var isCancelled: Bool { lock.withLock { cancelled } }
    private var isFinishing: Bool { lock.withLock { finishRequested } }

    private func add(_ candidate: Candidate) -> Bool {
        lock.withLock {
            guard !cancelled, !finishRequested else { return false }
            candidates.append(candidate)
            return true
        }
    }

    /// The caller invokes this after StationSession adopts the first winner.
    /// Before that transfer, cancel still owns and closes its lease. Finishing
    /// the initial race never closes the first winner by itself.
    @discardableResult
    func transferInitialWinner() -> Bool {
        lock.withLock {
            guard !cancelled, let row = initialWinnerRow else { return false }
            candidates.removeAll { $0.row == row }
            initialWinnerRow = nil
            return true
        }
    }

    /// Called at snapshot.complete. The caller supplies the live session rank
    /// when ICE may have changed its selected pair since the first hello.
    /// A returned standby is the original verified lease, removed from racer
    /// ownership. Calling this again returns nil.
    func finishInitialRace(currentRank: Rank? = nil) async -> Winner? {
        let (task, sink, hook) = lock.withLock { () ->
            (Task<Result, Never>?, AsyncStream<Update>.Continuation?, (@Sendable () -> Void)?) in
            guard !finishClaimed, let standbyTask else { return (nil, nil, nil) }
            finishClaimed = true
            finishRequested = true
            return (standbyTask, stream, testHooks?.afterFinishRequested)
        }
        guard let task else { return nil }
        sink?.yield(.finishInitialRace)
        hook?()
        let finished = await task.value
        let selected = finished.winner
        lock.withLock {
            standbyTask = nil
            finishedTries = finished.tries
        }
        guard let selected else { return nil }
        let current = currentRank ?? lock.withLock { initialWinnerRank }
        let acceptable = current.map { selected.rank < $0 } ?? false
        let available = await selected.transport.isAvailable()
        let transferred = lock.withLock { () -> Bool in
            guard acceptable, available, !cancelled,
                  candidates.contains(where: { $0.row == selected.row }) else { return false }
            candidates.removeAll { $0.row == selected.row }
            return true
        }
        if !transferred {
            selected.transport.close()
            lock.withLock {
                if finishedTries?.indices.contains(selected.row) == true {
                    finishedTries?[selected.row].outcome = .cancelled
                }
            }
            return nil
        }
        return selected
    }

    private func retireLosers(except row: Int?) {
        let closing = lock.withLock { candidates.filter { $0.row != row } }
        for candidate in closing { candidate.transport.close() }
    }

    /// Service runs while direct hostnames resolve. Numeric direct addresses
    /// enter the two-slot scheduler immediately; resolved names join it once
    /// their actual IP, port and IPv6 scope can be deduplicated.
    func run(direct: [StationEndpoint], service: (any CoreServiceRoute)?, trust: StationTrust,
             transportFactory: @escaping LinkTransportFactory, clock: any LinkClock,
             serviceAddress: String, preferredFirst: Bool = false,
             resolveNames: @escaping @Sendable ([StationEndpoint]) -> [StationEndpoint] = DirectRouteResolver.resolve,
             retainBetterStandby: Bool = false,
             betterThan maximumRank: Rank? = nil,
             networks: @escaping @Sendable () -> LocalNetworks = LocalNetworks.current,
             serviceRank: @escaping @Sendable (any CoreServiceRoute, ConnectionAttempt.Path) -> Rank =
                { route, path in
                    if let value = route.selectedPathRank, let rank = Rank(rawValue: value) { return rank }
                    return path == .relay ? .turn : .directIce
                },
             testHooks: TestHooks? = nil)
        async -> Result {
        lock.withLock { self.testHooks = testHooks }
        if !retainBetterStandby {
            return await performRace(direct: direct, service: service, trust: trust,
                                     transportFactory: transportFactory, clock: clock,
                                     serviceAddress: serviceAddress, preferredFirst: preferredFirst,
                                     resolveNames: resolveNames, maximumRank: maximumRank, serviceRank: serviceRank,
                                     networks: networks, initialSink: nil, testHooks: testHooks)
        }
        let (firstResults, firstSink) = AsyncStream.makeStream(of: Result.self)
        let worker = Task {
            await self.performRace(direct: direct, service: service, trust: trust,
                                   transportFactory: transportFactory, clock: clock,
                                   serviceAddress: serviceAddress, preferredFirst: preferredFirst,
                                   resolveNames: resolveNames, maximumRank: maximumRank, serviceRank: serviceRank,
                                   networks: networks, initialSink: firstSink, testHooks: testHooks)
        }
        lock.withLock { standbyTask = worker }
        for await result in firstResults {
            if result.winner == nil {
                _ = await worker.value
                lock.withLock { standbyTask = nil }
            }
            if isCancelled {
                return Result(winner: nil, tries: result.tries, routes: result.routes,
                              incompatible: result.incompatible,
                              localNetworkDenied: result.localNetworkDenied)
            }
            return result
        }
        let result = await worker.value
        lock.withLock { standbyTask = nil }
        return result
    }

    private func performRace(direct: [StationEndpoint], service: (any CoreServiceRoute)?, trust: StationTrust,
                             transportFactory: @escaping LinkTransportFactory, clock: any LinkClock,
                             serviceAddress: String, preferredFirst: Bool,
                             resolveNames: @escaping @Sendable ([StationEndpoint]) -> [StationEndpoint],
                             maximumRank: Rank?,
                             serviceRank: @escaping @Sendable (any CoreServiceRoute, ConnectionAttempt.Path) -> Rank,
                             networks currentNetworks: @escaping @Sendable () -> LocalNetworks,
                             initialSink: AsyncStream<Result>.Continuation?,
                             testHooks: TestHooks?) async -> Result {
        guard case .identity(let key) = trust else {
            let result = Result(winner: nil, tries: [], routes: [], incompatible: nil, localNetworkDenied: false)
            initialSink?.yield(result)
            initialSink?.finish()
            return result
        }
        let (updates, sink) = AsyncStream.makeStream(of: Update.self)
        let retired = lock.withLock { () -> Bool in
            stream = sink
            return cancelled
        }
        if retired { sink.finish() }
        defer {
            sink.finish()
            initialSink?.finish()
            lock.withLock { stream = nil }
        }
        var tries: [ConnectionAttempt.Try] = []
        var routes: [Route] = []
        var seen = Set<StationEndpoint>()
        var pendingDirect: [Candidate] = []
        var activeDirectRows = Set<Int>()
        var serviceActive = false
        var resolvingNames = false
        var resolvingPreferred = false
        var firstIpv6Launched = false
        var ipv4GateOpen = true
        var ipv4StaggerTimer: (any LinkTimer)?
        var winner: Winner?
        var standby: Winner?
        var standbyHealthTimer: (any LinkTimer)?
        var firstPublished = false
        var incompatible: LinkMessage.Hello?
        var denied = false
        func rank(_ candidate: Candidate, path: ConnectionAttempt.Path? = nil) -> Rank {
            switch candidate.route {
            case .address:
                return candidate.initial.path == .thisNetwork ? .localWebSocket : .otherWebSocket
            case .service(let service):
                return serviceRank(service, path ?? .direct)
            }
        }
        func close(_ candidate: Candidate) {
            activeDirectRows.remove(candidate.row)
            candidate.transport.close()
        }
        func watchStandby(_ row: Int) {
            standbyHealthTimer?.cancel()
            standbyHealthTimer = clock.schedule(after: .seconds(1)) {
                sink.yield(.standbyHealthCheck(row))
            }
        }
        func prunePending() {
            guard let winner else { return }
            let bestRank = standby?.rank ?? winner.rank
            pendingDirect.removeAll { candidate in
                guard rank(candidate) >= bestRank else { return false }
                close(candidate)
                tries[candidate.row].outcome = .cancelled
                return true
            }
        }
        // A private address belongs to one network: off it, and with no
        // VPN that may carry it, it can only reach someone else's network
        // or the carrier's NAT64 translation of it, never the Core. An
        // address the operator chose for this connect is dialled anyway.
        let chosen = preferredFirst ? direct.first?.canonical : nil
        let racedFrom = currentNetworks()
        func reachable(_ endpoint: StationEndpoint) -> Bool {
            endpoint.canonical == chosen || racedFrom.mayReach(endpoint.canonical.host)
        }
        func append(_ route: Route, endpoint: StationEndpoint?) {
            let path = endpoint.map { ConnectionAttempt.path(for: $0, networks: currentNetworks()) } ?? .direct
            let address = endpoint.map(ConnectionFlow.addressText) ?? serviceAddress
            let row = tries.count
            let inner: any LinkTransport
            if let endpoint { inner = transportFactory(endpoint, trust) }
            else { inner = service!.makeTransport() }
            let candidate = Candidate(row: row, route: route,
                                      transport: PreauthenticatedTransport(inner),
                                      initial: .init(path: path, address: address, throughService: endpoint == nil))
            guard add(candidate) else {
                candidate.transport.close()
                return
            }
            tries.append(candidate.initial)
            routes.append(route)
            if endpoint == nil {
                serviceActive = true
                Task {
                    sink.yield(.completed(await Self.inspect(candidate, key: key, clock: clock)))
                    testHooks?.afterCompletionQueued?(candidate.row)
                }
            } else {
                pendingDirect.append(candidate)
            }
        }
        func scheduleDirect() {
            // A chosen manual hostname owns the first direct dial. Let its
            // lookup complete before stale saved literals consume either
            // socket; the service has already started independently.
            guard !resolvingPreferred else { return }
            while activeDirectRows.count < 2 && !pendingDirect.isEmpty && !isCancelled && !isFinishing {
                prunePending()
                if pendingDirect.isEmpty { break }
                let index: Int
                if activeDirectRows.count == 1 {
                    if let ipv4 = pendingDirect.firstIndex(where: \.isIpv4) {
                        // Keep the second source-handshake slot for the
                        // IPv4 peer of Happy Eyeballs, even with two v6s.
                        if !ipv4GateOpen { break }
                        index = ipv4
                    } else if resolvingNames || resolvingPreferred {
                        break
                    } else {
                        index = 0
                    }
                } else {
                    if !ipv4GateOpen, pendingDirect[0].isIpv4 {
                        guard let ipv6 = pendingDirect.firstIndex(where: { candidate in
                            guard case .address(let endpoint) = candidate.route else { return false }
                            return AddressFamilies.ofLiteral(endpoint.host).ipv6
                        }) else { break }
                        index = ipv6
                    } else {
                        index = 0
                    }
                }
                let candidate = pendingDirect.remove(at: index)
                // Finalization and slot admission share one lock. Once the
                // finish marker is requested, no pending rung can start.
                let admitted = lock.withLock { () -> Bool in
                    guard !cancelled, !finishRequested else { return false }
                    activeDirectRows.insert(candidate.row)
                    return true
                }
                if !admitted {
                    pendingDirect.insert(candidate, at: index)
                    break
                }
                if !candidate.isIpv4, case .address(let endpoint) = candidate.route,
                   AddressFamilies.ofLiteral(endpoint.host).ipv6, !firstIpv6Launched {
                    firstIpv6Launched = true
                    ipv4GateOpen = false
                    ipv4StaggerTimer = clock.schedule(after: .milliseconds(250)) {
                        sink.yield(.ipv4StaggerReady)
                    }
                }
                Task {
                    guard !self.isFinishing, !self.isCancelled else {
                        candidate.transport.close()
                        return
                    }
                    sink.yield(.completed(await Self.inspect(candidate, key: key, clock: clock)))
                    testHooks?.afterCompletionQueued?(candidate.row)
                }
            }
        }
        if let service, !preferredFirst {
            append(.service(service), endpoint: nil)
        }
        let selectedName = preferredFirst && direct.first.map { !AddressFamilies.ofLiteral($0.host).known } == true
            ? direct.first : nil
        let literals = direct.filter { AddressFamilies.ofLiteral($0.host).known }
        let names = direct.filter { !AddressFamilies.ofLiteral($0.host).known && $0 != selectedName }
        resolvingPreferred = selectedName != nil
        resolvingNames = !names.isEmpty
        var numeric = DirectRouteResolver.resolve(literals)
        if !preferredFirst { numeric = Self.ipv6First(numeric) }
        for endpoint in numeric where reachable(endpoint) && seen.insert(endpoint.canonical).inserted {
            append(.address(endpoint), endpoint: endpoint)
        }
        scheduleDirect()
        // An explicitly entered address is launched as the selected direct
        // target; service still starts alongside it as a live backup.
        if let service, preferredFirst {
            append(.service(service), endpoint: nil)
        }
        if let selectedName {
            DispatchQueue.global(qos: .userInitiated).async {
                let resolved = resolveNames([selectedName])
                sink.yield(.namesResolved(resolved, preferred: true))
            }
        }
        if !names.isEmpty {
            DispatchQueue.global(qos: .userInitiated).async {
                let resolved = resolveNames(names)
                sink.yield(.namesResolved(resolved, preferred: false))
            }
        }
        let resolutionTimer: (any LinkTimer)? = (resolvingNames || resolvingPreferred)
            ? clock.schedule(after: StationSession.connectDeadline) { sink.yield(.resolutionTimedOut) } : nil
        defer {
            resolutionTimer?.cancel()
            ipv4StaggerTimer?.cancel()
            standbyHealthTimer?.cancel()
        }
        if !serviceActive && activeDirectRows.isEmpty && !resolvingNames && !resolvingPreferred {
            let result = Result(winner: nil, tries: tries, routes: routes, incompatible: nil,
                                localNetworkDenied: false)
            initialSink?.yield(result)
            return result
        }
        race: for await update in updates {
            if isCancelled { break }
            switch update {
            case .finishInitialRace:
                // The first lease already belongs to the signing-in session.
                // Only the verified standby may transfer here; pending and
                // late completed rungs are closed without affecting it.
                let closing = lock.withLock { candidates }
                for candidate in closing where candidate.row != winner?.row && candidate.row != standby?.row {
                    close(candidate)
                    if tries.indices.contains(candidate.row) { tries[candidate.row].outcome = .cancelled }
                }
                if let standby, await standby.transport.isAvailable() {
                    return Result(winner: standby, tries: tries, routes: routes,
                                  incompatible: nil, localNetworkDenied: denied)
                }
                standby?.transport.close()
                if let standby { tries[standby.row].outcome = .cancelled }
                return Result(winner: nil, tries: tries, routes: routes,
                              incompatible: nil, localNetworkDenied: denied)
            case .namesResolved(let resolved, let preferred):
                guard preferred ? resolvingPreferred : resolvingNames else { continue }
                if preferred { resolvingPreferred = false }
                else { resolvingNames = false }
                var addresses = resolved
                if !preferredFirst || preferred {
                    addresses = Self.ipv6First(addresses)
                }
                for endpoint in addresses where (preferred || reachable(endpoint))
                    && seen.insert(endpoint.canonical).inserted {
                    append(.address(endpoint), endpoint: endpoint)
                }
                if preferred {
                    // The entered hostname can resolve to a saved literal.
                    // Promote that existing candidate instead of creating a
                    // duplicate socket or losing the user's selected target.
                    var rank: [StationEndpoint: Int] = [:]
                    for (index, endpoint) in addresses.enumerated() {
                        if rank[endpoint.canonical] == nil { rank[endpoint.canonical] = index }
                    }
                    let selected = pendingDirect.filter { candidate in
                        guard case .address(let endpoint) = candidate.route else { return false }
                        return rank[endpoint.canonical] != nil
                    }.sorted { lhs, rhs in
                        guard case .address(let left) = lhs.route,
                              case .address(let right) = rhs.route else { return false }
                        return (rank[left.canonical] ?? .max) < (rank[right.canonical] ?? .max)
                    }
                    let others = pendingDirect.filter { candidate in
                        guard case .address(let endpoint) = candidate.route else { return true }
                        return rank[endpoint.canonical] == nil
                    }
                    pendingDirect = selected + others
                }
                scheduleDirect()
            case .resolutionTimedOut:
                if resolvingPreferred, let selectedName {
                    tries.append(.init(path: ConnectionAttempt.path(for: selectedName,
                                                                    networks: currentNetworks()),
                                       address: ConnectionFlow.addressText(selectedName), outcome: .timedOut))
                    routes.append(.address(selectedName))
                }
                if resolvingNames {
                    for endpoint in names {
                        tries.append(.init(path: ConnectionAttempt.path(for: endpoint,
                                                                        networks: currentNetworks()),
                                           address: ConnectionFlow.addressText(endpoint), outcome: .timedOut))
                        routes.append(.address(endpoint))
                    }
                }
                resolvingPreferred = false
                resolvingNames = false
                scheduleDirect()
            case .ipv4StaggerReady:
                ipv4GateOpen = true
                scheduleDirect()
            case .standbyDeadline(let row):
                guard standby?.row == row else { continue }
                standbyHealthTimer?.cancel()
                standbyHealthTimer = nil
                if let expired = lock.withLock({ candidates.first { $0.row == row } }) {
                    close(expired)
                }
                standby = nil
                tries[row].outcome = .timedOut
                scheduleDirect()
            case .standbyHealthCheck(let row):
                guard standby?.row == row else { continue }
                await testHooks?.beforeStandbyHealthAvailability?()
                if let held = standby, await held.transport.isAvailable() {
                    watchStandby(row)
                } else {
                    standbyHealthTimer = nil
                    if let ended = lock.withLock({ candidates.first { $0.row == row } }) {
                        close(ended)
                    }
                    standby = nil
                    tries[row].outcome = .noAnswer
                    scheduleDirect()
                }
            case .completed(let completed):
                let row = completed.candidate.row
                if tries[row].outcome == .cancelled { continue }
                if case .service = completed.candidate.route { serviceActive = false }
                denied = denied || completed.denied
                tries[row].path = completed.path
                tries[row].outcome = completed.outcome
                if let evidence = completed.evidence { tries[row].ice = evidence }
                if let hello = completed.hello {
                    if LinkVersionPolicy.agree(ours: LinkVersionPolicy.supportedMajors,
                                               theirs: hello.supportedMajors) == nil {
                        close(completed.candidate)
                        if winner == nil {
                            incompatible = hello
                            retireLosers(except: nil)
                            break race
                        }
                        scheduleDirect()
                        continue
                    }
                    let ready = Winner(transport: completed.candidate.transport, route: completed.candidate.route,
                                       path: completed.path, mediaIce: completed.ice,
                                       row: row, rank: rank(completed.candidate, path: completed.path))
                    ready.transport.recordDiagnosticServiceRank(ready.rank.rawValue)
                    if let maximumRank, ready.rank >= maximumRank {
                        close(completed.candidate)
                        tries[row].outcome = .cancelled
                        scheduleDirect()
                        if winner == nil && !resolvingNames && !resolvingPreferred && !serviceActive &&
                            activeDirectRows.isEmpty && pendingDirect.isEmpty {
                            break race
                        }
                        continue
                    }
                    if let winner {
                        if let held = standby, !(await held.transport.isAvailable()) {
                            if let prior = lock.withLock({ candidates.first { $0.row == held.row } }) {
                                close(prior)
                            }
                            tries[held.row].outcome = .noAnswer
                            standby = nil
                        }
                        if ready.rank < winner.rank && (standby == nil || ready.rank < standby!.rank) {
                            if let standby,
                               let prior = lock.withLock({ candidates.first { $0.row == standby.row } }) {
                                close(prior)
                            }
                            standby = ready
                            tries[row].outcome = .trying
                            ready.transport.onDeadline { sink.yield(.standbyDeadline(row)) }
                            watchStandby(row)
                            prunePending()
                            let others = lock.withLock { candidates }
                            for candidate in others where candidate.row != winner.row && candidate.row != row {
                                guard rank(candidate) >= ready.rank else { continue }
                                close(candidate)
                                if tries[candidate.row].outcome == .trying {
                                    tries[candidate.row].outcome = .cancelled
                                }
                                if case .service = candidate.route { serviceActive = false }
                            }
                        } else {
                            close(completed.candidate)
                            tries[row].outcome = .cancelled
                        }
                        scheduleDirect()
                        continue
                    }
                    winner = ready
                    lock.withLock {
                        initialWinnerRow = row
                        initialWinnerRank = ready.rank
                    }
                    tries[row].outcome = .trying
                    if initialSink == nil {
                        retireLosers(except: row)
                        break race
                    }
                    // Keep only rungs capable of beating the first winner.
                    // The direct slot includes this winner through sign-in.
                    prunePending()
                    let others = lock.withLock { candidates }
                    for candidate in others where candidate.row != row {
                        let canBeat = rank(candidate) < ready.rank
                        if !canBeat {
                            close(candidate)
                            if tries.indices.contains(candidate.row) { tries[candidate.row].outcome = .cancelled }
                            if case .service = candidate.route { serviceActive = false }
                        }
                    }
                    initialSink?.yield(Result(winner: ready, tries: tries, routes: routes,
                                              incompatible: nil, localNetworkDenied: denied))
                    initialSink?.finish()
                    firstPublished = true
                    scheduleDirect()
                    await testHooks?.afterFirstWinnerPublished?()
                    continue
                }
                close(completed.candidate)
                scheduleDirect()
            }
            if winner == nil && !resolvingNames && !resolvingPreferred && !serviceActive &&
                activeDirectRows.isEmpty && pendingDirect.isEmpty {
                break
            }
        }
        for index in tries.indices where tries[index].outcome == .trying && index != winner?.row {
            tries[index].outcome = .cancelled
        }
        if isCancelled {
            // cancel() already closed every lease still owned by the racer.
            // The first winner may have moved to StationSession meanwhile.
            winner = nil
        }
        let result = Result(winner: winner, tries: tries, routes: routes, incompatible: incompatible,
                            localNetworkDenied: denied)
        if !firstPublished { initialSink?.yield(result) }
        return result
    }

    private static func inspect(_ candidate: Candidate, key: Data, clock: any LinkClock) async -> Completion {
        guard !Task.isCancelled else {
            return Completion(candidate: candidate, outcome: .cancelled, hello: nil,
                              path: candidate.initial.path, ice: nil, denied: false)
        }
        do {
            let (hello, digest) = try await candidate.transport.inspect(clock: clock,
                                                                         deadline: StationSession.connectDeadline)
            guard StationTrust.identityVerifies(hello.identity, publicKey: key,
                                                certificateSHA256: digest) else {
                candidate.transport.close()
                return Completion(candidate: candidate, outcome: .notThisCore, hello: nil,
                                  path: candidate.initial.path, ice: nil, denied: false)
            }
            guard await candidate.transport.isAvailable() else {
                return Completion(candidate: candidate, outcome: .noAnswer, hello: nil,
                                  path: candidate.initial.path, ice: nil, denied: false)
            }
            switch candidate.route {
            case .address:
                return Completion(candidate: candidate, outcome: .connected, hello: hello,
                                  path: candidate.initial.path, ice: nil, denied: false)
            case .service(let service):
                // Snapshot this dial's selected ICE pair and media settings
                // before another generation can change the route's getters.
                let tried = service.lastTry
                return Completion(candidate: candidate, outcome: .connected, hello: hello,
                                  path: tried?.path ?? .direct,
                                  ice: service.mediaIceSettings(), denied: false, evidence: tried?.ice)
            }
        } catch {
            let failure = error as? LinkTransportError
            let denied = failure == .localNetworkDenied
            let expired = candidate.transport.passedItsDeadline
            candidate.transport.close()
            var evidence: IceCandidateEvidence?
            var outcome = Self.outcome(of: failure, deadlinePassed: expired)
            if case .service(let service) = candidate.route, let tried = service.lastTry {
                evidence = tried.ice
                if tried.outcome != .trying && tried.outcome != .connected { outcome = tried.outcome }
            }
            return Completion(candidate: candidate, outcome: outcome, hello: nil,
                              path: candidate.initial.path, ice: nil, denied: denied, evidence: evidence)
        }
    }

    /// How a rung that failed to open ended, in the attempt record's words.
    static func outcome(of failure: LinkTransportError?, deadlinePassed: Bool) -> ConnectionAttempt.Outcome {
        if deadlinePassed || failure?.isNoReply == true { return .timedOut }
        switch failure {
        case .refused?: return .refused
        case .unreachable?: return .unreachable
        default: return .noAnswer
        }
    }

    private static func ipv6First(_ endpoints: [StationEndpoint]) -> [StationEndpoint] {
        endpoints.enumerated().sorted { lhs, rhs in
            let left = AddressFamilies.ofLiteral(lhs.element.host).ipv6
            let right = AddressFamilies.ofLiteral(rhs.element.host).ipv6
            return left == right ? lhs.offset < rhs.offset : left
        }.map(\.element)
    }
}
