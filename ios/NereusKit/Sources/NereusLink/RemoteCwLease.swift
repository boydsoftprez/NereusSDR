// NereusSDR for iOS: CW command and report lifetime, independent of microphone/MOX
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-28: Remote CW v2 lease fencing. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

/// Main-actor admission fences commands synchronously with Stop and session
/// retirement. The existing PTT actor remains the sole heartbeat coordinator.
@MainActor
public final class RemoteCwLease {
    public struct Request: Sendable {
        fileprivate let id: Int
        fileprivate let generation: Int
        public let authority: CommandSendPermit
    }

    public struct Activity: Sendable {
        /// nil ends CW; zero watches a pending request without inventing an epoch.
        public let epoch: Int64?
        public let session: Int
        public let authority: CommandSendPermit
    }

    public var onActivity: ((Activity) -> Void)?
    public private(set) var epoch: Int64?
    public private(set) var releaseAuthority = CommandSendPermit()
    private var session = 0
    private var generation = 0
    private var nextRequest = 0
    private var sendTail: Task<Void, Never>?
    public var hasActivity: Bool { epoch != nil || !pending.isEmpty }

    /// All CWX page instances share admission order; Stop waits for earlier
    /// command copies to be enqueued, never for their delayed Core answers.
    @discardableResult
    public func enqueueSend(_ operation: @escaping @MainActor () async -> Void) -> Task<Void, Never> {
        let previous = sendTail
        let task = Task {
            await previous?.value
            await operation()
        }
        sendTail = task
        return task
    }

    private var pending: Set<Int> = []
    private var requestAuthority = CommandSendPermit()
    private var activityAuthority = CommandSendPermit()
    private var retiredThrough: Int64 = 0
    // A tuple has no request id. After Stop, an unknown old epoch cannot be
    // distinguished from a new request until its command result establishes it.
    private var reportMayEstablishEpoch = true

    public init() {}

    public func reset(session: Int) {
        stop()
        releaseAuthority.revoke()
        releaseAuthority = CommandSendPermit()
        self.session = session
        retiredThrough = 0
        reportMayEstablishEpoch = true
        publish()
    }

    public func begin() -> Request {
        nextRequest += 1
        pending.insert(nextRequest)
        let request = Request(id: nextRequest, generation: generation, authority: requestAuthority)
        publish()
        return request
    }

    public func isCurrent(_ request: Request) -> Bool {
        request.generation == generation && pending.contains(request.id) && !request.authority.isRevoked
    }

    public func complete(_ request: Request, acceptedEpoch: Int64?, active: Bool) {
        guard isCurrent(request) else { return }
        pending.remove(request.id)
        if let acceptedEpoch, Self.valid(acceptedEpoch), acceptedEpoch > retiredThrough {
            if active {
                // An append result from an older epoch cannot replace a newer one.
                if acceptedEpoch >= (epoch ?? 0) { epoch = acceptedEpoch }
            } else {
                retiredThrough = max(retiredThrough, acceptedEpoch)
                if epoch == acceptedEpoch { epoch = nil }
            }
        }
        publish()
    }

    public func stop() {
        generation += 1
        requestAuthority.revoke()
        requestAuthority = CommandSendPermit()
        pending.removeAll()
        if let epoch { retiredThrough = max(retiredThrough, epoch) }
        epoch = nil
        reportMayEstablishEpoch = false
        publish()
    }

    /// Read only a complete tuple from the current session's ordered mirror.
    public func observe(_ tuple: TransmitStateReport.RemoteCwActivity?, thisDeviceId: String?) {
        guard let tuple, let thisDeviceId else { return }
        guard tuple.deviceId == thisDeviceId else {
            // A successor owner retires our content, including its pending
            // results. Its tuple can never become this phone's CW lease.
            // Core epochs are global: even before our first result,
            // another owner's accepted epoch retires any older answer.
            // A completed foreign activity may be the first tuple seen.
            // Keep pending requests eligible for a truly later epoch.
            retiredThrough = max(retiredThrough, tuple.epoch)
            if let epoch, tuple.epoch > epoch { stop() }
            return
        }
        if tuple.active {
            guard tuple.epoch > retiredThrough else { return }
            if epoch == tuple.epoch || (!pending.isEmpty && reportMayEstablishEpoch) {
                if tuple.epoch >= (epoch ?? 0) { epoch = tuple.epoch }
            }
        } else {
            retiredThrough = max(retiredThrough, tuple.epoch)
            if epoch == tuple.epoch { epoch = nil }
        }
        publish()
    }

    public static func valid(_ epoch: Int64) -> Bool { epoch > 0 && epoch <= Int64(UInt32.max) }

    private func publish() {
        activityAuthority.revoke()
        activityAuthority = CommandSendPermit()
        onActivity?(Activity(epoch: epoch ?? (pending.isEmpty ? nil : 0), session: session,
                             authority: activityAuthority))
    }
}
