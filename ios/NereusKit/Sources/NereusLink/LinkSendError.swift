// NereusSDR for iOS: why the session kept a message back rather than send it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A message `StationSession.send` refused locally, sending nothing,
/// because the Core would end the session on it (link document sections
/// 3.6, 5.1, 11, 12.3 and 13). These are for the log, never for the operator.
public enum LinkSendError: Error, Equatable {
    /// No connection is open.
    case notConnected
    /// A kind only the Core sends.
    case notAClientMessage
    /// The session sends the one `hello` itself.
    case secondHello
    /// The session sends the one `auth.request` itself, after its `hello`.
    case authRequestOutOfOrder
    /// Anything else before the Core accepted the sign-in.
    case beforeSignIn
    /// `media.control` before `snapshot.complete`.
    case beforeSnapshotComplete
    /// Over the Core's 1 MiB inbound cap, or `media.control` over 128 KiB.
    case tooLarge(bytes: Int)
    /// A `pair.*` message: pairing runs on a connection of its own
    /// (`PairingClient`), and the Core ends a session that sends one.
    case pairingOnSession
    /// A message the Core's decoder would refuse.
    case unreadable(String)
}
