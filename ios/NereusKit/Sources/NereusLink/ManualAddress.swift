// NereusSDR for iOS: reads an address the operator types for a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// An address typed by hand. Pasted whole (``parse(_:)``): a hostname, an
/// IPv4 literal or a bracketed IPv6 literal, each with an optional `:port`;
/// without a port the Core's default, 47910, is used. Typed as the address
/// screen asks for it (D69, ``parse(host:port:)``): the address alone, an
/// IPv6 literal with or without brackets, and the port in a field of its
/// own. ``split(_:)`` moves a pasted address's port into that field.
public enum ManualAddress {
    /// The endpoint `text` names, or nil when it is not an address this
    /// accepts. Leading and trailing spaces are ignored; a scheme, a path,
    /// an IPv6 literal without brackets and a port outside 1 to 65535 are not.
    public static func parse(_ text: String) -> StationEndpoint? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty,
              !trimmed.contains(where: { $0.isWhitespace || $0 == "/" || $0 == "@" || $0 == "?" || $0 == "#" }) else {
            return nil
        }

        if trimmed.hasPrefix("[") {
            guard let close = trimmed.firstIndex(of: "]") else {
                return nil
            }
            let literal = String(trimmed[trimmed.index(after: trimmed.startIndex)..<close])
            guard isIPv6(literal) else {
                return nil
            }
            let rest = trimmed[trimmed.index(after: close)...]
            guard let port = port(after: rest) else {
                return nil
            }
            return StationEndpoint(host: literal.lowercased(), port: port)
        }

        let parts = trimmed.split(separator: ":", omittingEmptySubsequences: false)
        guard parts.count <= 2 else {
            // More than one colon: an IPv6 literal must be bracketed.
            return nil
        }
        let host = String(parts[0])
        guard isIPv4(host) || isHostname(host) else {
            return nil
        }
        var port = StationEndpoint.defaultPort
        if parts.count == 2 {
            guard let given = portNumber(String(parts[1])) else {
                return nil
            }
            port = given
        }
        return StationEndpoint(host: host.lowercased(), port: port)
    }

    /// The endpoint the address screen's two fields name (D69), or nil when
    /// either is not one this accepts. `host` is a hostname, an IPv4
    /// literal, or an IPv6 literal with or without brackets; a bare IPv6
    /// literal is always a whole address, never an address and a port.
    /// `port` is a whole number from 1 to 65535. Leading and trailing spaces
    /// are ignored in both.
    public static func parse(host: String, port: String) -> StationEndpoint? {
        guard let number = parsePort(port) else {
            return nil
        }
        var address = host.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !address.isEmpty,
              !address.contains(where: { $0.isWhitespace || $0 == "/" || $0 == "@" || $0 == "?" || $0 == "#" }) else {
            return nil
        }
        if address.hasPrefix("[") {
            guard address.hasSuffix("]"), address.count > 2 else {
                return nil
            }
            address = String(address.dropFirst().dropLast())
            guard isIPv6(address) else {
                return nil
            }
        }
        if address.contains(":") {
            guard isIPv6(address) else {
                return nil
            }
        } else if !(isIPv4(address) || isHostname(address)) {
            return nil
        }
        return StationEndpoint(host: address.lowercased(), port: number)
    }

    /// The port field's value, a whole number from 1 to 65535, or nil.
    /// Leading and trailing spaces are ignored.
    public static func parsePort(_ text: String) -> UInt16? {
        portNumber(text.trimmingCharacters(in: .whitespacesAndNewlines))
    }

    /// A pasted address taken apart by the rules ``parse(_:)`` reads it
    /// with: the address, without the brackets around an IPv6 literal, and
    /// the port when the text carries one (`[2001:db8::10]:50055`,
    /// `192.0.2.10:47910`, `core.example:50055`). A bare IPv6 literal has no
    /// port. Nothing is checked here beyond the shape; ``parse(host:port:)``
    /// checks the parts.
    public static func split(_ text: String) -> (host: String, port: String?) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.hasPrefix("["), let close = trimmed.firstIndex(of: "]") {
            let literal = String(trimmed[trimmed.index(after: trimmed.startIndex)..<close])
            let rest = trimmed[trimmed.index(after: close)...]
            if rest.isEmpty {
                return (literal, nil)
            }
            if rest.first == ":" {
                return (literal, String(rest.dropFirst()))
            }
            return (trimmed, nil)
        }
        let parts = trimmed.split(separator: ":", omittingEmptySubsequences: false)
        guard parts.count == 2 else {
            // No colon, or more than one: a name, an IPv4 literal or a bare
            // IPv6 literal, whole.
            return (trimmed, nil)
        }
        return (String(parts[0]), String(parts[1]))
    }

    /// The port after a bracketed literal: nothing, or `:` and a port.
    private static func port(after rest: Substring) -> UInt16? {
        if rest.isEmpty {
            return StationEndpoint.defaultPort
        }
        guard rest.first == ":" else {
            return nil
        }
        return portNumber(String(rest.dropFirst()))
    }

    private static func portNumber(_ text: String) -> UInt16? {
        guard !text.isEmpty, text.count <= 5, text.allSatisfy({ $0.isASCII && $0.isNumber }),
              let value = Int(text), value >= 1, value <= 65535 else {
            return nil
        }
        return UInt16(value)
    }

    private static func isIPv4(_ text: String) -> Bool {
        let octets = text.split(separator: ".", omittingEmptySubsequences: false)
        guard octets.count == 4 else {
            return false
        }
        return octets.allSatisfy { octet in
            !octet.isEmpty && octet.count <= 3 && octet.allSatisfy({ $0.isASCII && $0.isNumber })
                && (Int(octet) ?? 256) <= 255
        }
    }

    private static func isIPv6(_ text: String) -> Bool {
        // A zone (fe80::1%en0) is allowed; the address before it must parse.
        let address = text.split(separator: "%", maxSplits: 1, omittingEmptySubsequences: false)
        guard let first = address.first, !first.isEmpty, first.contains(":") else {
            return false
        }
        if address.count == 2 && address[1].isEmpty {
            return false
        }
        var storage = in6_addr()
        return String(first).withCString { inet_pton(AF_INET6, $0, &storage) } == 1
    }

    private static func isHostname(_ text: String) -> Bool {
        var name = Substring(text)
        if name.hasSuffix(".") {
            name = name.dropLast()
        }
        guard !name.isEmpty, name.count <= 253 else {
            return false
        }
        let labels = name.split(separator: ".", omittingEmptySubsequences: false)
        let wellFormed = labels.allSatisfy { label in
            !label.isEmpty && label.count <= 63 && label.first != "-" && label.last != "-"
                && label.allSatisfy { $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "-") }
        }
        // All-numeric labels would be a malformed IPv4 literal, not a name.
        let numeric = labels.allSatisfy { $0.allSatisfy { $0.isASCII && $0.isNumber } }
        return wellFormed && !numeric
    }
}
