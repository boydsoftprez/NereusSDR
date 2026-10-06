// NereusSDR for iOS: an open control data channel and the certificate the Core presented on it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What a ``ControlChannelConnector`` returns once the channel is open.
public struct ControlChannelConnection: Sendable {
    /// The open channel.
    public var channel: any ControlChannel
    /// SHA-256 of the certificate the Core presented in the DTLS handshake,
    /// read from the handshake itself, never from the fingerprint its SDP
    /// claims (link document section 20): the session checks the Core's
    /// `hello` binding against it, as it checks a WebSocket's TLS
    /// certificate.
    public var certificateSHA256: Data

    public init(channel: any ControlChannel, certificateSHA256: Data) {
        self.channel = channel
        self.certificateSHA256 = certificateSHA256
    }
}
