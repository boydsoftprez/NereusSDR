// NereusSDR for iOS: the way to a paired Core through the remote access service, for one connect
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMedia

/// One paired Core reached through the remote access service (R-IOS-16,
/// D22; link document section 20): each of the session's attempts dials
/// the Core through the service and runs the session over the control
/// connection that opens, and the session's media connection uses that
/// connection's ICE settings. ``ConnectionFlow`` makes one for each connect
/// that gets this far, after the Core's own addresses and this network.
protocol CoreServiceRoute: AnyObject, Sendable {
    /// A new session transport, with a connection of its own, for each attempt.
    func makeTransport() -> any SessionTransport
    /// The media connection's ICE settings for the control connection last
    /// opened; nil before one opened.
    func mediaIceSettings() -> IceSettings?
    /// The selected ICE pair, refreshed while its peer is still live.
    /// Direct ICE is 2, TURN is 3, and the web relay is 4.
    var selectedPathRank: Int? { get }
    /// A new service introduction that gathers no relay candidates. Nil
    /// where the route cannot prove that restriction.
    func directOnlyRoute() -> (any CoreServiceRoute)?
    /// Retains the selected web relay while a media generation still uses it.
    func mediaRelayContext() -> RelayRouteContext?
    /// The last dial as a try of the attempt record: relay when the
    /// connection went through the relay, direct otherwise.
    var lastTry: ConnectionAttempt.Try? { get }
    /// Why the last dial failed; nil when it opened or none has run.
    var lastError: RendezvousDialError? { get }
    /// Hands `handler`, on every dial this route and its direct-only route
    /// make, each candidate the Core has offered so far (its answer's SDP
    /// first, then each it trickled), from the answer on and whether or not
    /// the connection then opens (R-IOS-16). Never logged.
    func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void)
}

extension CoreServiceRoute {
    var selectedPathRank: Int? { nil }
    func directOnlyRoute() -> (any CoreServiceRoute)? { nil }
    func mediaRelayContext() -> RelayRouteContext? { nil }
    func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void) {}
}
