// NereusSDR for iOS: how the PTT reaches the Core: the keying verbs and the keepalive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A one-command record that its transport accepted the synchronous enqueue.
/// A delayed actor return or Core result cannot move this boundary backward.
public final class CommandHandoffReceipt: @unchecked Sendable {
    private let lock = NSLock()
    private var sent = false

    public init() {}
    public var wasSent: Bool { lock.withLock { sent } }
    fileprivate func markSent() { lock.withLock { sent = true } }
}

/// Irrevocable authority carried from admission through synchronous transport
/// handoff. Parents only point toward older authorities (intent, then session).
/// Locks are acquired child to parent and released before any actor suspension.
/// Handoff closures must not reenter a permit; production closures only enqueue
/// onto the transport and never call client/controller code synchronously.
public final class CommandSendPermit: @unchecked Sendable {
    private let lock = NSLock()
    private var revoked = false
    private let parents: [CommandSendPermit]
    private let handoffReceipt: CommandHandoffReceipt?

    public init(parents: [CommandSendPermit] = [], handoffReceipt: CommandHandoffReceipt? = nil) {
        self.parents = parents
        self.handoffReceipt = handoffReceipt
    }
    public func revoke() { lock.withLock { revoked = true } }
    public var isRevoked: Bool {
        lock.withLock { revoked || parents.contains { $0.isRevoked } }
    }
    public func handoff(_ send: () throws -> Bool) rethrows -> Bool {
        try lock.withLock {
            guard !revoked else { return false }
            let sent = try handoffParent(0, send)
            if sent { handoffReceipt?.markSent() }
            return sent
        }
    }
    private func handoffParent(_ index: Int, _ send: () throws -> Bool) rethrows -> Bool {
        guard index < parents.count else { return try send() }
        return try parents[index].handoff { try handoffParent(index + 1, send) }
    }
}

/// A synchronous handoff fence shared by the PTT, media sender and primary
/// session. Closing it waits for any heartbeat already entering its transport.
public final class TransmitHeartbeatGate: @unchecked Sendable {
    private let lock = NSLock()
    private var open = false
    private var retired = false

    public init() {}

    public func setOpen(_ value: Bool) { lock.withLock { open = value && !retired } }
    public func retire() { lock.withLock { retired = true; open = false } }
    public var isOpen: Bool { lock.withLock { open } }

    public func handoff(_ send: () throws -> Bool) rethrows -> Bool {
        try lock.withLock {
            guard open else { return false }
            return try send()
        }
    }
}

/// The Core's transmit commands as ``PttController`` sends them. The app's
/// is the command client's; the tests' plays the Core's keying.
public protocol TransmitCommandSending: Sendable {
    /// Sends `verb` `copies` times as one command (one id) and returns once
    /// the copies have gone, after every verb sent before it, with the
    /// answer to wait for. A key and its release therefore reach the Core
    /// in the order they were sent.
    func send(_ verb: TransmitVerb, copies: Int) async -> TransmitPending
    /// The intent's authority is captured before crossing this async boundary.
    func send(_ verb: TransmitVerb, copies: Int, authority: CommandSendPermit) async -> TransmitPending
    /// Sends `tx.keepalive {sequence, epoch}` once, without waiting for its
    /// answer (section 18.7).
    func sendKeepalive(sequence: Int64, epoch: Int64) async
    /// Rechecks the controller's fence after any suspension before handoff.
    func sendKeepalive(sequence: Int64, epoch: Int64,
                       gate: TransmitHeartbeatGate,
                       stillAllowed: @escaping @Sendable () async -> Bool) async
}

public extension TransmitCommandSending {
    /// Compatibility for in-memory conformers. A transport implementation must
    /// carry authority to its final synchronous handoff, as TransmitCommandClient does.
    func send(_ verb: TransmitVerb, copies: Int, authority: CommandSendPermit) async -> TransmitPending {
        guard !authority.isRevoked else { return TransmitPending { .noAnswer } }
        return await send(verb, copies: copies)
    }

    func sendKeepalive(sequence: Int64, epoch: Int64,
                       gate: TransmitHeartbeatGate,
                       stillAllowed: @escaping @Sendable () async -> Bool) async {
        guard await stillAllowed(), gate.isOpen else { return }
        await sendKeepalive(sequence: sequence, epoch: epoch)
    }
}
