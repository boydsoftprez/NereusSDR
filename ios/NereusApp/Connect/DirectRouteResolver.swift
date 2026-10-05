// NereusSDR for iOS: resolve direct control addresses before scheduling bounded TLS dials
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import NereusLink

/// Turns names and alternate numeric spellings into actual IP, port and
/// scope keys. The rendezvous rung starts independently of these lookups.
enum DirectRouteResolver {
    static func resolve(_ endpoints: [StationEndpoint]) -> [StationEndpoint] {
        resolve(endpoints, addresses: systemAddresses)
    }

    /// `addresses` looks a host up to numeric addresses (IPv6 with its
    /// scope as `%index`), nil when the lookup fails.
    static func resolve(_ endpoints: [StationEndpoint],
                        addresses: (String) -> [String]?) -> [StationEndpoint] {
        var seen = Set<StationEndpoint>()
        var resolved: [StationEndpoint] = []
        for endpoint in endpoints {
            for address in lookup(endpoint, addresses: addresses) where seen.insert(address.canonical).inserted {
                resolved.append(address)
            }
        }
        return resolved
    }

    /// A failed lookup retains the named endpoint so Network framework may
    /// still resolve it through a system path that getaddrinfo lacks.
    ///
    /// An IPv4 literal is kept as it is. On an IPv6-only network with
    /// NAT64, getaddrinfo answers an IPv4 literal with the carrier's IPv6
    /// synthesis of it; for a private address that reaches the carrier's
    /// side, never the Core, and it would read as a direct IPv6 attempt
    /// (JJ's build 13 on 5G, 2026-09-29). Network framework still
    /// translates a public IPv4 literal itself when the network needs it.
    static func lookup(_ endpoint: StationEndpoint,
                       addresses: (String) -> [String]? = systemAddresses) -> [StationEndpoint] {
        let canonical = endpoint.canonical
        var v4 = in_addr()
        if inet_pton(AF_INET, canonical.host, &v4) == 1 {
            return [canonical]
        }
        guard let found = addresses(canonical.host), !found.isEmpty else {
            return [canonical]
        }
        return found.map { StationEndpoint(host: $0, port: endpoint.port).canonical }
    }

    /// getaddrinfo's numeric answers for `host`: an IPv4-mapped IPv6
    /// address as its IPv4 address, a scoped IPv6 address with its zone.
    static func systemAddresses(_ host: String) -> [String]? {
        var hints = addrinfo()
        hints.ai_family = AF_UNSPEC
        hints.ai_socktype = SOCK_STREAM
        var result: UnsafeMutablePointer<addrinfo>?
        guard getaddrinfo(host, nil, &hints, &result) == 0, let first = result else {
            return nil
        }
        defer { freeaddrinfo(result) }
        var addresses: [String] = []
        var cursor: UnsafeMutablePointer<addrinfo>? = first
        while let item = cursor {
            defer { cursor = item.pointee.ai_next }
            guard let address = item.pointee.ai_addr,
                  item.pointee.ai_family == AF_INET || item.pointee.ai_family == AF_INET6 else { continue }
            var buffer = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            guard getnameinfo(address, item.pointee.ai_addrlen, &buffer, socklen_t(buffer.count), nil, 0,
                              NI_NUMERICHOST) == 0 else { continue }
            var name = String(cString: buffer)
            if item.pointee.ai_family == AF_INET6 {
                let v6 = address.withMemoryRebound(to: sockaddr_in6.self, capacity: 1) { $0.pointee }
                let bytes = withUnsafeBytes(of: v6.sin6_addr) { Array($0) }
                if bytes[0..<10].allSatisfy({ $0 == 0 }) && bytes[10] == 0xff && bytes[11] == 0xff {
                    name = bytes[12..<16].map(String.init).joined(separator: ".")
                } else if v6.sin6_scope_id != 0 && !name.contains("%") {
                    name += "%\(v6.sin6_scope_id)"
                }
            }
            addresses.append(name)
        }
        return addresses
    }
}
