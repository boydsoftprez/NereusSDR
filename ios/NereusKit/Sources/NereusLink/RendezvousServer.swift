// NereusSDR for iOS: a remote access service the app can reach a Core through
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One remote access service (the rendezvous document,
/// `docs/architecture/2026-09-23-rendezvous-v1.md`, section 2): a WebSocket
/// over TLS at `wss://host:port/`. The app keeps an ordered list of them,
/// the operator's own first and the NereusSDR one behind it (the pairing
/// design, section 5.3), and uses the first that answers.
public struct RendezvousServer: Hashable, Codable, Sendable {
    /// The port the service listens on unless its address names another.
    public static let defaultPort: UInt16 = 443

    /// The NereusSDR service, `rv.nereussdr.com` (JJ's choice, 2026-09-26).
    public static let nereus = RendezvousServer(host: "rv.nereussdr.com")

    /// The list the app uses when the operator has named none.
    public static let defaults: [RendezvousServer] = [nereus]

    /// A host name, or an IP literal without brackets.
    public var host: String
    public var port: UInt16

    public init(host: String, port: UInt16 = RendezvousServer.defaultPort) {
        self.host = host
        self.port = port
    }

    /// The service as an endpoint, for the WebSocket's URL and Host header.
    public var endpoint: StationEndpoint {
        StationEndpoint(host: host, port: port)
    }
}
