// NereusSDR for iOS: this phone's own networks, as its interfaces report them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin

/// The addresses of this phone's running interfaces, with their prefixes:
/// what decides whether a Core's address is on this network, and which
/// address families the phone can use to reach a server.
public struct LocalNetworks: Sendable, Equatable {
    /// One address on a running interface.
    public struct Entry: Sendable, Equatable {
        /// 4 bytes for IPv4, 16 for IPv6.
        public var address: [UInt8]
        public var prefixLength: Int
        public var isLoopback: Bool
        /// On a tunnel interface (``LocalNetworks/isTunnelInterface(_:)``):
        /// a VPN or overlay that may route addresses outside its own subnet.
        public var isTunnel: Bool

        public init(address: [UInt8], prefixLength: Int, isLoopback: Bool = false, isTunnel: Bool = false) {
            self.address = address
            self.prefixLength = prefixLength
            self.isLoopback = isLoopback
            self.isTunnel = isTunnel
        }
    }

    public var entries: [Entry]

    public init(entries: [Entry]) {
        self.entries = entries
    }

    /// This phone's interfaces now: every address on an interface that is up
    /// and running.
    public static func current() -> LocalNetworks {
        var list: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&list) == 0, let first = list else {
            return LocalNetworks(entries: [])
        }
        defer { freeifaddrs(list) }
        var entries: [Entry] = []
        var cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let item = cursor {
            defer { cursor = item.pointee.ifa_next }
            let flags = Int32(item.pointee.ifa_flags)
            guard flags & IFF_UP != 0, flags & IFF_RUNNING != 0,
                  let address = item.pointee.ifa_addr, let mask = item.pointee.ifa_netmask else {
                continue
            }
            // A netmask's own family field is not always set, so it is read
            // as the address's family.
            let family = Int32(address.pointee.sa_family)
            guard let bytes = Self.bytes(of: address, family: family),
                  let maskBytes = Self.bytes(of: mask, family: family) else {
                continue
            }
            let prefix = maskBytes.reduce(0) { $0 + $1.nonzeroBitCount }
            let name = String(cString: item.pointee.ifa_name)
            entries.append(Entry(address: bytes, prefixLength: prefix, isLoopback: flags & IFF_LOOPBACK != 0,
                                 isTunnel: Self.isTunnelInterface(name)))
        }
        return LocalNetworks(entries: entries)
    }

    /// The families this phone can reach a server over: an IPv4 address that
    /// is not loopback or link-local (a private one behind NAT counts), or a
    /// global unicast IPv6 address (2000::/3), on an interface that is not
    /// loopback.
    public var usableFamilies: AddressFamilies {
        var families = AddressFamilies.none
        for entry in entries where !entry.isLoopback {
            if entry.address.count == 4 {
                let isLoopback = entry.address[0] == 127
                let isLinkLocal = entry.address[0] == 169 && entry.address[1] == 254
                let isZero = entry.address.allSatisfy { $0 == 0 }
                if !isLoopback && !isLinkLocal && !isZero {
                    families.ipv4 = true
                }
            } else if entry.address.count == 16, entry.address[0] & 0xE0 == 0x20 {
                families.ipv6 = true
            }
        }
        return families
    }

    /// True when `host`, an IP literal, is loopback or inside the subnet of
    /// one of these addresses. False for a name.
    public func contains(_ host: String) -> Bool {
        guard var bytes = Self.literalBytes(host) else {
            return false
        }
        // An IPv4-mapped IPv6 address is its IPv4 address.
        if bytes.count == 16, bytes[0..<10].allSatisfy({ $0 == 0 }), bytes[10] == 0xFF, bytes[11] == 0xFF {
            bytes = Array(bytes[12..<16])
        }
        if bytes.count == 4, bytes[0] == 127 {
            return true
        }
        if bytes.count == 16, bytes[0..<15].allSatisfy({ $0 == 0 }), bytes[15] == 1 {
            return true
        }
        return entries.contains { entry in
            entry.address.count == bytes.count && Self.samePrefix(entry.address, bytes, bits: entry.prefixLength)
        }
    }

    /// Whether a connection from this phone could reach `host`, an address
    /// of a Core's. A private address (RFC 1918, the shared 100.64.0.0/10,
    /// IPv6 unique-local) belongs to one network: it is reached only on that
    /// network, or through a VPN with an address of the same family, which
    /// may route it. A link-local address is reached only on its own link.
    /// Anything else, and a name, may be reached from anywhere. Off its
    /// network a private IPv4 address only ever reaches the carrier's NAT64
    /// translation of it, never the Core (JJ's build 13 on 5G, 2026-09-29).
    public func mayReach(_ host: String) -> Bool {
        guard var bytes = Self.literalBytes(host) else {
            return true
        }
        if bytes.count == 16, bytes[0..<10].allSatisfy({ $0 == 0 }), bytes[10] == 0xFF, bytes[11] == 0xFF {
            bytes = Array(bytes[12..<16])
        }
        switch Self.scope(of: bytes) {
        case .global:
            return true
        case .linkLocal:
            return contains(host)
        case .privateNetwork:
            if contains(host) {
                return true
            }
            return entries.contains { entry in
                entry.isTunnel && entry.address.count == bytes.count
                    && Self.scope(of: entry.address) != .linkLocal
            }
        }
    }

    /// Whether `host` is a global unicast literal, an address a Core can be
    /// dialled at from any network: IPv6 in 2000::/3, or IPv4 outside every
    /// block of RFC 6890's special-purpose registry the Core leaves out of
    /// its own list (``specialIpv4Blocks``): private, shared, loopback,
    /// link-local, protocol assignments, documentation, 6to4 relay,
    /// benchmarking, multicast and reserved. False for a unique local or
    /// link-local address, an IPv4-mapped one, and a name.
    public static func isGlobal(_ host: String) -> Bool {
        guard let bytes = literalBytes(host) else {
            return false
        }
        if bytes.count == 16 {
            return bytes[0] & 0xE0 == 0x20
        }
        guard bytes.count == 4 else {
            return false
        }
        let value = bytes.reduce(UInt32(0)) { $0 << 8 | UInt32($1) }
        return !specialIpv4Blocks.contains { block in
            let mask: UInt32 = block.prefix == 0 ? 0 : ~UInt32(0) << UInt32(32 - block.prefix)
            return value & mask == block.base
        }
    }

    /// RFC 6890's IPv4 special-purpose address registry, the blocks the
    /// public internet does not route to one computer: the Core's own
    /// table, block for block (`kSpecialIpv4`, From
    /// src/core/session/CoreAddresses.cpp:41-60), so the phone keeps and
    /// races exactly the IPv4 addresses the Core would list.
    static let specialIpv4Blocks: [(base: UInt32, prefix: Int)] = [
        (0x0000_0000, 8),  // 0.0.0.0/8, this network
        (0x0A00_0000, 8),  // 10.0.0.0/8, private
        (0x6440_0000, 10), // 100.64.0.0/10, shared (carrier-grade NAT)
        (0x7F00_0000, 8),  // 127.0.0.0/8, loopback
        (0xA9FE_0000, 16), // 169.254.0.0/16, link-local
        (0xAC10_0000, 12), // 172.16.0.0/12, private
        (0xC000_0000, 24), // 192.0.0.0/24, protocol assignments
        (0xC000_0200, 24), // 192.0.2.0/24, documentation
        (0xC058_6300, 24), // 192.88.99.0/24, 6to4 relay anycast
        (0xC0A8_0000, 16), // 192.168.0.0/16, private
        (0xC612_0000, 15), // 198.18.0.0/15, benchmarking
        (0xC633_6400, 24), // 198.51.100.0/24, documentation
        (0xCB00_7100, 24), // 203.0.113.0/24, documentation
        (0xE000_0000, 4),  // 224.0.0.0/4, multicast
        (0xF000_0000, 4),  // 240.0.0.0/4, reserved, and the broadcast address
    ]

    /// `endpoints` in the order a direct dial prefers them, each kind in
    /// the order given: global IPv6 first, then unique local and link-local
    /// IPv6, then global and private IPv4, then names. The race still
    /// ranks each by the network it is on (link document section 21.1);
    /// this only decides which of equal rank starts first.
    public static func directPreference(_ endpoints: [StationEndpoint]) -> [StationEndpoint] {
        func order(_ endpoint: StationEndpoint) -> Int {
            guard let bytes = literalBytes(endpoint.host) else {
                return 6
            }
            let global = isGlobal(endpoint.host)
            if bytes.count == 16 {
                if global {
                    return 0
                }
                return scope(of: bytes) == .linkLocal ? 2 : 1
            }
            if global {
                return 3
            }
            return scope(of: bytes) == .linkLocal ? 5 : 4
        }
        return endpoints.enumerated().sorted { lhs, rhs in
            let left = order(lhs.element)
            let right = order(rhs.element)
            return left == right ? lhs.offset < rhs.offset : left < right
        }.map(\.element)
    }

    /// A VPN's or overlay's interface: `utun` (every VPN made with Network
    /// Extension, WireGuard, Tailscale and ZeroTier among them), `ipsec`,
    /// `ppp`, `tun` and `tap`. iOS keeps a few `utun` interfaces of its own,
    /// with link-local addresses only.
    public static func isTunnelInterface(_ name: String) -> Bool {
        ["utun", "ipsec", "ppp", "tun", "tap"].contains { name.hasPrefix($0) }
    }

    private enum AddressScope {
        case global, privateNetwork, linkLocal
    }

    /// Loopback counts as private: ``contains(_:)`` accepts it.
    private static func scope(of bytes: [UInt8]) -> AddressScope {
        if bytes.count == 4 {
            switch (bytes[0], bytes[1]) {
            case (169, 254):
                return .linkLocal
            case (10, _), (127, _), (192, 168):
                return .privateNetwork
            case (172, 16...31):
                return .privateNetwork
            case (100, 64...127):
                return .privateNetwork
            default:
                return .global
            }
        }
        guard bytes.count == 16 else {
            return .global
        }
        if bytes[0] == 0xFE && bytes[1] & 0xC0 == 0x80 {
            return .linkLocal
        }
        if bytes[0] & 0xFE == 0xFC || bytes.prefix(15).allSatisfy({ $0 == 0 }) {
            return .privateNetwork
        }
        return .global
    }

    // MARK: Helpers

    static func literalBytes(_ host: String) -> [UInt8]? {
        var text = host
        if text.hasPrefix("["), text.hasSuffix("]") {
            text = String(text.dropFirst().dropLast())
        }
        if let zone = text.firstIndex(of: "%") {
            text = String(text[..<zone])
        }
        var v4 = in_addr()
        if inet_pton(AF_INET, text, &v4) == 1 {
            return withUnsafeBytes(of: v4) { Array($0) }
        }
        var v6 = in6_addr()
        if inet_pton(AF_INET6, text, &v6) == 1 {
            return withUnsafeBytes(of: v6) { Array($0) }
        }
        return nil
    }

    private static func samePrefix(_ a: [UInt8], _ b: [UInt8], bits: Int) -> Bool {
        var left = max(0, min(bits, a.count * 8))
        var index = 0
        while left > 0 {
            let take = min(8, left)
            let mask = UInt8(truncatingIfNeeded: 0xFF << (8 - take))
            if a[index] & mask != b[index] & mask {
                return false
            }
            left -= take
            index += 1
        }
        return true
    }

    /// The address bytes of a socket address of `family`, read no further
    /// than its own length says: a netmask's may stop short, its missing
    /// bytes being zero.
    private static func bytes(of address: UnsafeMutablePointer<sockaddr>, family: Int32) -> [UInt8]? {
        let offset: Int
        let count: Int
        switch family {
        case AF_INET:
            offset = MemoryLayout<sockaddr_in>.offset(of: \.sin_addr) ?? 4
            count = 4
        case AF_INET6:
            offset = MemoryLayout<sockaddr_in6>.offset(of: \.sin6_addr) ?? 8
            count = 16
        default:
            return nil
        }
        let length = Int(address.pointee.sa_len)
        let raw = UnsafeRawPointer(address)
        return (0..<count).map { index in
            offset + index < length ? raw.load(fromByteOffset: offset + index, as: UInt8.self) : 0
        }
    }
}
