// NereusSDR for iOS: where a Core listens, its host and control port
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The address of a Core's control connection. `host` is a hostname, an
/// IPv4 literal or an IPv6 literal without brackets.
public struct StationEndpoint: Hashable, Codable, Sendable {
    /// The port a Core listens on unless it is told otherwise.
    public static let defaultPort: UInt16 = 47910

    public var host: String
    public var port: UInt16

    public init(host: String, port: UInt16 = StationEndpoint.defaultPort) {
        self.host = host
        self.port = port
    }

    /// The endpoint as one Core is known by, however it was spelled: the
    /// host lowercased, without a trailing dot or the brackets around an
    /// IPv6 literal, and the port.
    public var canonical: StationEndpoint {
        var name = host.lowercased()
        if name.hasPrefix("["), name.hasSuffix("]") {
            name = String(name.dropFirst().dropLast())
        }
        while name.hasSuffix(".") {
            name = String(name.dropLast())
        }
        return StationEndpoint(host: name, port: port)
    }
}
