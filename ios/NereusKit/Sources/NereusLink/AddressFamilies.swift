// NereusSDR for iOS: which address families something has, IPv4, IPv6, both or cannot tell
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin

/// Which address families something has: this phone's usable addresses, or
/// what a server's name resolved to. Neither set: cannot tell.
public struct AddressFamilies: Sendable, Hashable {
    public var ipv4: Bool
    public var ipv6: Bool

    public init(ipv4: Bool = false, ipv6: Bool = false) {
        self.ipv4 = ipv4
        self.ipv6 = ipv6
    }

    public static let none = AddressFamilies()
    public static let ipv4Only = AddressFamilies(ipv4: true)
    public static let ipv6Only = AddressFamilies(ipv6: true)
    public static let both = AddressFamilies(ipv4: true, ipv6: true)

    public var known: Bool { ipv4 || ipv6 }
    public var isBoth: Bool { ipv4 && ipv6 }

    /// True when the two have a family in common.
    public func shares(_ other: AddressFamilies) -> Bool {
        (ipv4 && other.ipv4) || (ipv6 && other.ipv6)
    }

    /// The family of an IP literal (an IPv6 one with or without brackets;
    /// an IPv4-mapped IPv6 address counts as IPv4); none for a name.
    public static func ofLiteral(_ host: String) -> AddressFamilies {
        var text = host
        if text.hasPrefix("["), text.hasSuffix("]") {
            text = String(text.dropFirst().dropLast())
        }
        if let zone = text.firstIndex(of: "%") {
            text = String(text[..<zone])
        }
        var v4 = in_addr()
        if inet_pton(AF_INET, text, &v4) == 1 {
            return .ipv4Only
        }
        var v6 = in6_addr()
        if inet_pton(AF_INET6, text, &v6) == 1 {
            let bytes = withUnsafeBytes(of: v6) { Array($0) }
            let mapped = bytes[0..<10].allSatisfy { $0 == 0 } && bytes[10] == 0xFF && bytes[11] == 0xFF
            return mapped ? .ipv4Only : .ipv6Only
        }
        return .none
    }
}
