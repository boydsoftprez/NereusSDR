// NereusSDR for iOS: the connection under a session, a TLS WebSocket or a test's stand-in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One media route's admission ticket. Retiring it and handing a frame to
/// NWConnection are serialized under this lock; a retired generation cannot
/// enter a physical transport queue or survive its queue drain.
public final class BinaryMediaOwnership: @unchecked Sendable {
    private let lock = NSLock()
    private var active = true
    public init() {}
    public func retire() { lock.withLock { active = false } }
    public func withActive<T>(_ body: () -> T) -> T? {
        lock.withLock { active ? body() : nil }
    }
}

/// What the connection reports to its session, in order.
public enum LinkTransportEvent: Sendable, Equatable {
    /// One text frame.
    case text(String)
    /// The answer to the session's ping.
    case pong
    /// The connection is gone.
    case closed
}

/// Why a connection did not open.
public enum LinkTransportError: Error, Equatable {
    /// The Core presented a certificate other than the pinned one; nothing
    /// was read from it.
    case certificateMismatch
    /// iOS did not let the app reach devices on this network (the Local
    /// Network permission is off for it), so the Core was never tried.
    case localNetworkDenied
    /// The connection could not be made or was lost while opening.
    case failed(String)
    /// The computer at the address turned the connection away (TCP reset
    /// to the opening): nothing listens on that port, or its firewall
    /// rejects it.
    case refused
    /// This device has no route to the address from the network it is on.
    case unreachable

    /// Nothing answered before the connection's deadline.
    public static let noReply = LinkTransportError.failed("no reply")

    /// True for ``noReply`` and for the system proxy's own deadline passing.
    public var isNoReply: Bool {
        self == .noReply || self == .failed(SystemProxyError.timedOut.openingFailureText)
    }
}

/// One control connection. The session opens it once, sends text frames
/// and pings on it, and closes it; the heartbeat is WebSocket ping and pong
/// frames, or the control data channel's 5-byte pings and pongs, never a
/// message (link document sections 12.1 and 20). A session names it
/// ``SessionTransport``.
public protocol LinkTransport: Sendable {
    /// Opens the connection, checking the Core's certificate against the
    /// trust before anything is read, and returns the SHA-256 of the
    /// certificate it presented. Events arrive through `onEvent`, one at a
    /// time and in order.
    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data
    /// Submits one complete text frame to this physical connection. True
    /// means local admission only; a later transport completion can still
    /// fail. A closed or refusing transport returns false.
    @discardableResult func send(_ text: String) -> Bool
    /// Sends a ping; its pong arrives as `.pong`.
    func ping()
    /// Closes with a normal close; no further events matter.
    func close()
    /// Install a synchronous binary ingress. The receiver must return promptly.
    /// Binary media never enters the ordered control-event stream.
    func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?)
    /// Queue one bounded binary frame on this physical connection.
    @discardableResult func sendBinary(_ frame: Data) -> Bool
    @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool
    /// Drop unsent frames owned by exactly this retired generation.
    func discardBinary(ownership: BinaryMediaOwnership)
    /// Whether opening is a dial with its own bound (the control data
    /// channel's, through the remote access service), so the session's
    /// connect deadline counts from the moment it opens rather than from
    /// the dial. False for the WebSocket, whose opening is part of what the
    /// deadline covers.
    var boundsItsOwnOpening: Bool { get }
    /// Read-only evidence from this selected, ready physical connection.
    var selectedRouteObservation: SelectedRouteObservation { get }
    /// Diagnostic metadata of this selected transport, never a dial decision.
    var diagnosticServiceRank: Int? { get }
    /// Application text payload on this one transport, if observable.
    var trafficObservation: LinkTrafficObservation? { get }
}

extension LinkTransport {
    public var boundsItsOwnOpening: Bool { false }
    public var selectedRouteObservation: SelectedRouteObservation { .unavailable(.unsupported) }
    public var diagnosticServiceRank: Int? { nil }
    public var trafficObservation: LinkTrafficObservation? { nil }
    public func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) {}
    @discardableResult public func sendBinary(_ frame: Data) -> Bool { false }
    @discardableResult public func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        ownership.withActive { sendBinary(frame) } ?? false
    }
    public func discardBinary(ownership: BinaryMediaOwnership) {}
}

/// Makes the connection for one attempt.
public typealias LinkTransportFactory = @Sendable (StationEndpoint, StationTrust) -> any LinkTransport
