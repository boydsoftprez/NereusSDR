// NereusSDR for iOS: the relay credentials the remote access service mints for one connection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The relay credentials an `answer` carries (the rendezvous document,
/// section 8): the same object reaches both ends, valid for new relay
/// allocations until `expires`. The password is a secret: it goes to the
/// ICE stack and nowhere else, never into a log.
public struct RendezvousTurn: Sendable, Hashable {
    /// `"<expires>:<station id>"`, 1 to 512 bytes.
    public var username: String
    /// Standard base64 of an HMAC-SHA1, 1 to 128 bytes.
    public var password: String
    /// Unix seconds.
    public var expires: Int64
    /// The relay's `turn:` URLs, in the service's order: on the NereusSDR
    /// service an IPv4-only name first, then an IPv6-only one.
    public var urls: [String]

    public init(username: String, password: String, expires: Int64, urls: [String]) {
        self.username = username
        self.password = password
        self.expires = expires
        self.urls = urls
    }
}

extension RendezvousTurn: CustomStringConvertible {
    /// Never the password, and not the username, which names the Core.
    public var description: String {
        "RendezvousTurn(expires: \(expires), urls: \(urls.count))"
    }
}
