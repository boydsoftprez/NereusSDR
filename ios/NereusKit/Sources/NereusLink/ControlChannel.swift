// NereusSDR for iOS: the control data channel under a session, as its transport sees it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The reliable, ordered data channel labelled `control` on a peer
/// connection of its own, that a session through the remote access service
/// runs over (link document section 20). The media library's peer
/// (`ControlPeer`, NereusMedia) is the real one; a test supplies its own.
/// ``DataChannelSessionTransport`` frames the session on it.
public protocol ControlChannel: AnyObject, Sendable {
    /// Sends one binary data-channel message. False when it could not be
    /// sent (the channel is closed or failing).
    func send(_ frame: Data) -> Bool
    /// Closes the channel, then its connection. Nothing more is reported.
    func close()
    /// The open peer's nominated ICE pair, if one can be read.
    var selectedRouteObservation: SelectedRouteObservation { get }
}

public extension ControlChannel {
    var selectedRouteObservation: SelectedRouteObservation { .unavailable(.unsupported) }
}

/// Makes a control connection: dials, and returns once the channel is open,
/// with the certificate the Core presented in DTLS. What the channel
/// receives reaches `onEvent` in order from the moment the connector is
/// called, so nothing the Core sends as its end opens is lost.
public typealias ControlChannelConnector = @Sendable (
    _ onEvent: @escaping @Sendable (ControlChannelEvent) -> Void
) async throws -> ControlChannelConnection
