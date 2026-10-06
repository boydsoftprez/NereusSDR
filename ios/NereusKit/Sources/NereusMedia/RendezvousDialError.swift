// NereusSDR for iOS: why a connection through the remote access service did not open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Why ``RendezvousDialer`` did not open a control connection.
public enum RendezvousDialError: Error, Sendable, Equatable {
    /// The service did not introduce this phone: no service answered, the
    /// Core is not there, the service refused, the introduction ended before
    /// the Core's answer, or the Core stayed silent.
    case service(RendezvousError)
    /// After the answer, the introduction ended or the service connection
    /// dropped while the Core's candidates were still to come.
    case serviceGone
    /// The Core's answer is not a control connection's.
    case unusableAnswer
    /// The connection failed before its channel opened.
    case notConnected
    /// The connection did not open within ``RendezvousDialer/dialDeadline``.
    case notOpenedInTime
    /// The channel opened but the Core's DTLS certificate could not be read.
    case noCertificate
    /// All available ICE paths failed after the web relay reported why its
    /// own leg ended. Direct and TURN paths remained eligible meanwhile.
    case webRelay(String)
    /// The same relay failure, with the whole dial deadline exhausted.
    case webRelayTimedOut(String)

    /// Plain words for the operator: the service's own words where it gave
    /// them, as sent.
    public var operatorText: String {
        switch self {
        case .service(.offline(let reason)), .service(.refused(_, let reason, _)):
            return reason
        case .service(.unreachable):
            return Self.unreachableServiceText
        case .webRelay(let words), .webRelayTimedOut(let words):
            return words
        default:
            return Self.notReachedText
        }
    }

    /// The app's words when no remote access service answered.
    public static let unreachableServiceText =
        "The remote access service could not be reached. Check this phone's internet connection and try again."
    /// The app's words when the connection through the service did not open.
    public static let notReachedText = "The Core could not be reached from here."
}
