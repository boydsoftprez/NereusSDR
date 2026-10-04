// NereusSDR for iOS: hand an already verified connection to one session without redialling
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Owns a single live transport between the pre-auth route race and one
/// `StationSession`. The Core's hello is buffered, never answered here.
public final class PreauthenticatedTransport: LinkTransport, @unchecked Sendable {
    private let inner: any LinkTransport
    private let events = Events()
    private let lock = NSLock()
    private var digest: Data?
    private var inspectedHello: LinkMessage.Hello?
    private var adopted = false
    private var savedDiagnosticServiceRank: Int?
    private var closed = false
    private var expired = false
    private var deadlineTimer: (any LinkTimer)?
    private var deadlineGeneration = 0
    private var deadlineClock: (any LinkClock)?
    private var deadlineDueMs: Int64?
    private var deadlineRemainingMs: Int64?
    private var deadlineHandler: (@Sendable () async -> Void)?
    private let availabilityProbe: (@Sendable () async -> Void)?

    public init(_ inner: any LinkTransport) {
        self.inner = inner
        availabilityProbe = nil
    }

    // Lets the switch tests hold the availability snapshot across actor reentry.
    internal init(_ inner: any LinkTransport,
                  availabilityProbe: (@Sendable () async -> Void)?) {
        self.inner = inner
        self.availabilityProbe = availabilityProbe
    }

    /// Introduced data channels bound their own ICE dial. Their hello
    /// deadline starts only after that dial opens, as in StationSession.
    public var boundsItsOwnOpening: Bool { inner.boundsItsOwnOpening }
    /// Captured from the inspected winner before adoption. Metadata cannot
    /// affect trust, lease admission or path selection.
    public func recordDiagnosticServiceRank(_ rank: Int) {
        lock.withLock {
            guard !closed, !adopted, (0...4).contains(rank) else { return }
            savedDiagnosticServiceRank = rank
        }
    }
    public var diagnosticServiceRank: Int? {
        lock.withLock { !closed && adopted ? savedDiagnosticServiceRank : nil }
    }
    public var selectedRouteObservation: SelectedRouteObservation {
        let available = lock.withLock { !closed && adopted }
        guard available else { return .unavailable(lock.withLock { closed ? .retired : .notReady }) }
        let reading = inner.selectedRouteObservation
        return lock.withLock { !closed && adopted } ? reading : .unavailable(.retired)
    }
    public var trafficObservation: LinkTrafficObservation? {
        guard lock.withLock({ !closed && adopted }) else { return nil }
        let reading = inner.trafficObservation
        return lock.withLock { !closed && adopted } ? reading : nil
    }
    public func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {
        inner.setBinaryReceiver(receiver)
    }
    @discardableResult public func sendBinary(_ frame: Data) -> Bool {
        guard lock.withLock({ !closed && adopted }) else { return false }
        return inner.sendBinary(frame)
    }
    @discardableResult public func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        guard lock.withLock({ !closed && adopted }) else { return false }
        return inner.sendBinary(frame, ownership: ownership)
    }
    public func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }

    /// Opens once and returns the first Core hello and this connection's
    /// certificate digest. The caller must verify both before adopting.
    public func inspect(clock: any LinkClock, deadline: Duration) async throws -> (LinkMessage.Hello, Data) {
        guard lock.withLock({ !closed }) else {
            throw LinkTransportError.failed("the connection was cancelled before opening")
        }
        if !inner.boundsItsOwnOpening {
            installDeadline(clock: clock, after: deadline)
        }
        do {
            let opened = try await inner.open { [weak self, events] event in
                if event == .closed { self?.markClosed() }
                if !(await events.receive(event)) { self?.close() }
            }
            if inner.boundsItsOwnOpening {
                installDeadline(clock: clock, after: deadline)
            }
            let available = lock.withLock { () -> Bool in
                guard !closed else { return false }
                digest = opened
                return true
            }
            guard available, let first = await events.firstText(),
                  case .hello(let hello) = try LinkCodec.decode(first) else {
                close()
                throw LinkTransportError.failed("the Core did not send its hello")
            }
            lock.withLock { inspectedHello = hello }
            return (hello, opened)
        } catch {
            close()
            throw error
        }
    }

    /// `StationSession` calls this only on the winner. Buffered events are
    /// delivered in arrival order before any newly arriving event.
    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        guard let ready = lock.withLock({ () -> Data? in
            guard !closed, !adopted, let digest else { return nil }
            adopted = true
            return digest
        }) else {
            throw LinkTransportError.failed("the inspected connection is no longer available")
        }
        await events.prepare(onEvent)
        return ready
    }

    /// Called by the session only after it installed its certificate digest
    /// and entered the hello phase. A connection that already closed cannot
    /// be used to authenticate, even when its hello was buffered first.
    public func releaseEvents() async -> Bool {
        await events.release()
    }

    /// Includes both a local cancellation and a remote close. Call this
    /// after each suspension before adopting or sending authentication.
    public func isAvailable() async -> Bool {
        let local = lock.withLock { !closed }
        guard local else { return false }
        let remotelyClosed = await events.hasClosed()
        if let availabilityProbe { await availabilityProbe() }
        return !remotelyClosed
    }

    /// The exact hello and certificate digest captured by `inspect`, while
    /// this lease is still live and unclaimed. A session rechecks this pair
    /// before requesting a one-use control-move ticket.
    public func inspectedConnection() async -> (LinkMessage.Hello, Data)? {
        let checked = lock.withLock { () -> (LinkMessage.Hello, Data)? in
            guard !closed, !adopted, let inspectedHello, let digest else { return nil }
            return (inspectedHello, digest)
        }
        guard let checked, !(await events.hasClosed()) else { return nil }
        return checked
    }

    /// True once the connect deadline passed before this lease was
    /// adopted: the attempt record's "no answer in time".
    public var passedItsDeadline: Bool { lock.withLock { expired } }

    /// StationSession adopts the original deadline rather than starting a
    /// second 30-second budget after the race has already inspected hello.
    public func onDeadline(_ handler: @escaping @Sendable () async -> Void) {
        let alreadyExpired = lock.withLock { () -> Bool in
            deadlineHandler = handler
            return expired
        }
        if alreadyExpired { Task { await handler() } }
    }

    /// The Core's fifth-device question suspends this lease's original
    /// opening-to-snapshot budget; admission resumes only the time left.
    public func pauseDeadline() {
        let timer = lock.withLock { () -> (any LinkTimer)? in
            guard !closed, let clock = deadlineClock, let due = deadlineDueMs else { return nil }
            deadlineRemainingMs = max(0, due - clock.nowMilliseconds)
            deadlineDueMs = nil
            deadlineGeneration += 1
            let taken = deadlineTimer
            deadlineTimer = nil
            return taken
        }
        timer?.cancel()
    }

    public func resumeDeadline() {
        let remainder = lock.withLock { () -> (any LinkClock, Int64)? in
            guard !closed, let clock = deadlineClock, let ms = deadlineRemainingMs else { return nil }
            deadlineRemainingMs = nil
            return (clock, ms)
        }
        if let (clock, ms) = remainder { installDeadline(clock: clock, after: .milliseconds(ms)) }
    }

    /// Snapshot readiness ends this attempt's single connection budget.
    @discardableResult
    public func sessionReady() -> Bool {
        let (ready, timer) = lock.withLock { () -> (Bool, (any LinkTimer)?) in
            guard !closed else { return (false, nil) }
            let taken = deadlineTimer
            deadlineTimer = nil
            return (true, taken)
        }
        timer?.cancel()
        return ready
    }

    @discardableResult public func send(_ text: String) -> Bool {
        guard lock.withLock({ !closed && adopted }) else { return false }
        return inner.send(text)
    }
    public func ping() {
        if lock.withLock({ !closed && adopted }) { inner.ping() }
    }
    public func close() {
        let (shouldClose, timer) = lock.withLock { () -> (Bool, (any LinkTimer)?) in
            guard !closed else { return (false, nil) }
            closed = true
            let taken = deadlineTimer
            deadlineTimer = nil
            return (true, taken)
        }
        guard shouldClose else { return }
        timer?.cancel()
        inner.close()
        Task { await events.abort() }
    }

    private func markClosed() {
        let timer = lock.withLock { () -> (any LinkTimer)? in
            closed = true
            let taken = deadlineTimer
            deadlineTimer = nil
            return taken
        }
        timer?.cancel()
    }

    private func installDeadline(clock: any LinkClock, after duration: Duration) {
        let generation = lock.withLock { () -> Int in
            deadlineGeneration += 1
            return deadlineGeneration
        }
        let timer = clock.schedule(after: duration) { [weak self] in self?.deadlinePassed(generation) }
        let installed = lock.withLock { () -> Bool in
            guard !closed, deadlineGeneration == generation else { return false }
            let parts = duration.components
            let ms = parts.seconds * 1_000 + parts.attoseconds / 1_000_000_000_000_000
            deadlineClock = clock
            deadlineDueMs = clock.nowMilliseconds + ms
            deadlineTimer = timer
            return true
        }
        if !installed { timer.cancel() }
        else if duration <= .zero { deadlinePassed(generation) }
    }

    private func deadlinePassed(_ generation: Int) {
        let (didExpire, handler) = lock.withLock { () -> (Bool, (@Sendable () async -> Void)?) in
            guard !closed, deadlineGeneration == generation, deadlineTimer != nil else { return (false, nil) }
            closed = true
            expired = true
            deadlineTimer = nil
            return (true, deadlineHandler)
        }
        guard didExpire else { return }
        inner.close()
        Task { await events.abort() }
        if let handler { Task { await handler() } }
    }

    private actor Events {
        private struct BufferedEvent {
            let event: LinkTransportEvent
            let completion: CheckedContinuation<Bool, Never>?
        }

        private var buffered: [BufferedEvent] = []
        private var first: String??
        private var firstWaiter: CheckedContinuation<String?, Never>?
        private var recipient: (@Sendable (LinkTransportEvent) async -> Void)?
        private var inFlightCompletion: CheckedContinuation<Bool, Never>?
        private var pumping = false
        private var closed = false
        private var aborted = false
        private var released = false
        private var bufferedBytes = 0
        private let maxBufferedBytes = WebSocketLinkTransport.maxInboundMessageBytes
        private let maxBufferedEvents = 16

        func receive(_ event: LinkTransportEvent) async -> Bool {
            guard !aborted else { return false }
            if first == nil {
                switch event {
                case .text(let text):
                    first = .some(text)
                    firstWaiter?.resume(returning: text)
                    firstWaiter = nil
                case .closed:
                    closed = true
                    first = .some(nil)
                    firstWaiter?.resume(returning: nil)
                    firstWaiter = nil
                case .pong: break
                }
            }
            if event == .closed { closed = true }
            if !released {
                if case .text(let text) = event { bufferedBytes += text.utf8.count }
                if buffered.count >= maxBufferedEvents || bufferedBytes > maxBufferedBytes {
                    closed = true
                    firstWaiter?.resume(returning: nil)
                    firstWaiter = nil
                    return false
                }
                buffered.append(BufferedEvent(event: event, completion: nil))
                return true
            }
            // Once adopted, preserve the underlying transport's serial
            // callback backpressure. A released snapshot cannot outrun the
            // session while its buffered hello is being handled.
            return await withCheckedContinuation { completion in
                buffered.append(BufferedEvent(event: event, completion: completion))
                Task { await self.drain() }
            }
        }

        func firstText() async -> String? {
            if let first { return first }
            return await withCheckedContinuation { firstWaiter = $0 }
        }

        func hasClosed() -> Bool { closed || aborted }

        func abort() {
            aborted = true
            closed = true
            if first == nil { first = .some(nil) }
            firstWaiter?.resume(returning: nil)
            firstWaiter = nil
            inFlightCompletion?.resume(returning: false)
            inFlightCompletion = nil
            for item in buffered { item.completion?.resume(returning: false) }
            buffered.removeAll()
            bufferedBytes = 0
        }

        func prepare(_ onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) {
            recipient = onEvent
        }

        func release() async -> Bool {
            guard !closed, !aborted else { return false }
            released = true
            await drain()
            return !aborted
        }

        private func drain() async {
            guard released, let recipient, !pumping, !aborted else { return }
            pumping = true
            while !buffered.isEmpty && !aborted {
                let item = buffered.removeFirst()
                if case .text(let text) = item.event { bufferedBytes -= text.utf8.count }
                inFlightCompletion = item.completion
                await recipient(item.event)
                inFlightCompletion?.resume(returning: !aborted)
                inFlightCompletion = nil
            }
            pumping = false
        }
    }
}
