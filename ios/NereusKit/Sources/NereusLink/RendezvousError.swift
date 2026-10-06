// NereusSDR for iOS: why a request to the remote access service did not go through
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why ``RendezvousClient`` did not introduce this device or open a
/// mailbox. The service's reasons are its own words (the rendezvous
/// document, section 7), shown as sent.
public enum RendezvousError: Error, Sendable, Equatable {
    /// No service in the list answered with its greeting.
    case unreachable
    /// Every service that answered said the Core is not registered with it
    /// (`offline`), with the last one's words.
    case offline(reason: String)
    /// Every service that answered said no Core shows that pairing code's
    /// number (`nameplateUnknown`), with the last one's words.
    case nameplateUnknown(reason: String)
    /// A service refused with another code: its code, its words and its
    /// wait (zero: no advice).
    case refused(code: String, reason: String, retryAfter: Duration)
    /// The introduction ended before the Core answered (`stationLeft`,
    /// `expired`, or a code this app does not know).
    case introductionEnded(code: String)
    /// The Core did not answer the introduction in time. A Core stays
    /// silent towards a device it has not paired, a revoked one and a
    /// signature that does not verify, so this is also how those look.
    case noAnswer
    /// The connection to the service ended before the request finished.
    case connectionLost
    /// A value this app would send is not of its kind (a Core id that is
    /// not one, an offer, body or candidate over its cap), so nothing was
    /// sent.
    case protocolViolation
    /// A request of this kind is already running on this client.
    case busy
}
