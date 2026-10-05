// NereusSDR for iOS: the ICE settings of a peer connection that came through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import NereusLink

/// The ICE settings of a peer connection that came through the remote
/// access service (iPhone app plan Task 27a, R-IOS-16; the rendezvous
/// document, sections 6.1, 6.3 and 8), the same as the Core's (its
/// `IceConfiguration`, Task 27). A peer that did not come through the
/// service has none and gathers host candidates only.
///
/// Shaped by the app's pinned libdatachannel v0.24.5 over libjuice, the
/// desktop's pins:
///
/// - One STUN server: libdatachannel picks one of its STUN servers at
///   random, so exactly one is given, chosen by this phone's address
///   families (below).
/// - At most two relay servers, UDP only, and libjuice resolves one address
///   per relay host, preferring IPv4, so the service names an IPv4-only and
///   an IPv6-only relay host. The relay is sized by allocations (four for
///   each Core's id, both ends of a session sharing them), so one end
///   relays on one family: the host this phone can reach. With `families`
///   2 both slots carry the two names, the IPv4-only one first.
/// - Which one (the rendezvous document, section 8): the first entry whose
///   name resolves to a family this phone has a usable address in; the
///   service's first entry alone when the phone has both families or
///   cannot tell (no usable address seen, or no name resolved). An
///   IPv4-only phone behind NAT so takes the IPv4-only name whichever the
///   service lists first. (The Core, when it has one family and knows no
///   relay name's, takes both hosts; the phone keeps to the document.)
/// - Credentials are fixed before gathering starts: a media peer is built
///   from these settings only once ``relayKnown`` (the answer's `turn`
///   arrived, or was null), and libdatachannel gathers after the peer
///   exists. The control connection's peer (``ControlPeer``) is made with
///   the STUN server alone before its offer goes out, and gathers with the
///   relay added only once it is known (ios/patches/libdatachannel/0002).
/// - An MTU of 996 bytes, so TURN's 4-byte ChannelData header keeps a
///   relayed datagram at the 1000 bytes media is capped at.
/// - A full relay (TURN 486) is an ordinary outcome: libjuice finishes
///   gathering without it and the connection goes on with the paths it has.
public struct IceSettings: Sendable, Equatable {
    /// libdatachannel's and libjuice's relay limit.
    public static let maxRelayServers = 2
    /// 1000 bytes on the wire less TURN's ChannelData header.
    public static let mtu = 996
    /// libjuice gives up on a STUN or TURN server after 23.5 s, and on the
    /// connectivity checks after 39.5 s; a deadline for the whole connection
    /// covers both, one after the other.
    public static let gatheringDeadline: Duration = .milliseconds(23_500)
    public static let connectivityTimeout: Duration = .milliseconds(39_500)
    public static let connectDeadline: Duration = gatheringDeadline + connectivityTimeout
    /// The port a STUN or TURN URL means when it names none.
    public static let defaultPort: UInt16 = 3478
    /// How long ``resolveHostFamilies(_:timeout:)`` waits for the names.
    public static let hostLookupTimeout: Duration = .seconds(3)

    /// A STUN server, or a relay's address.
    public struct Server: Sendable, Equatable {
        public var host: String
        public var port: UInt16
    }

    /// A relay server with the credentials the service minted (UDP only).
    public struct Relay: Sendable, Equatable {
        public var host: String
        public var port: UInt16
        public var username: String
        public var password: String
    }

    /// The settings are not complete: the relay credentials are not known
    /// yet, so no peer may gather with them.
    public struct NotReady: Error, Equatable {}

    public private(set) var stun: Server?
    public private(set) var relays: [Relay] = []
    /// When the relay credentials arrived (or were known to be absent):
    /// gathering may start.
    public private(set) var relayKnown = false
    public let relayAllowed: Bool
    /// This phone's usable address families.
    public let localFamilies: AddressFamilies
    private var hostFamilies: [String: AddressFamilies] = [:]

    /// The settings for a connection through the service: `stunUrls` as its
    /// hello listed them, `local` this phone's usable families
    /// (``LocalNetworks/usableFamilies``) and `hosts` the families the
    /// lists' names resolved to on this phone (an empty map when nothing is
    /// known). No relay until ``setRelay(_:families:)``.
    public init(stunUrls: [String], relayAllowed: Bool = true, local: AddressFamilies,
                hosts: [String: AddressFamilies] = [:]) {
        self.relayAllowed = relayAllowed
        localFamilies = local
        addHostFamilies(hosts)
        let usable = stunUrls.compactMap(Self.parseStunUrl)
        if let chosen = chooseByFamily(usable.map(\.host)) {
            stun = usable[chosen]
        }
    }

    /// Adds resolved names (the relay's, which arrive after the hello).
    public mutating func addHostFamilies(_ hosts: [String: AddressFamilies]) {
        for (name, families) in hosts {
            hostFamilies[name.lowercased()] = families
        }
    }

    /// The relay credentials the answer brought (nil when it sent null).
    /// `families` is how many address families to relay on: 1, the host
    /// chosen by this phone's families, or 2, the first two the service
    /// lists. Returns how many relay servers the settings now hold.
    @discardableResult
    public mutating func setRelay(_ turn: RendezvousTurn?, families: Int) -> Int {
        let wanted = min(max(families, 1), Self.maxRelayServers)
        relayKnown = true
        relays = []
        guard relayAllowed, let turn else {
            return 0
        }
        // The first usable URL of each host, in the service's order.
        var hosts: [Relay] = []
        for url in turn.urls {
            guard let address = Self.parseTurnUrl(url),
                  !hosts.contains(where: { $0.host.lowercased() == address.host.lowercased() }) else {
                continue
            }
            hosts.append(Relay(host: address.host, port: address.port, username: turn.username,
                               password: turn.password))
        }
        if wanted == 1 {
            if let chosen = chooseByFamily(hosts.map(\.host)) {
                relays = [hosts[chosen]]
            }
        } else {
            relays = Array(hosts.prefix(wanted))
        }
        return relays.count
    }

    /// The ICE servers as libdatachannel reads them: `stun:host:port`, then
    /// `turn:user:password@host:port?transport=udp` for each relay, with
    /// the credentials percent-encoded (a username holds a colon) and an
    /// IPv6 literal in brackets.
    public var libdatachannelServers: [String] {
        var servers: [String] = []
        if let stun {
            servers.append("stun:\(Self.hostText(stun.host)):\(stun.port)")
        }
        for relay in relays {
            servers.append("turn:\(Self.percentEncoded(relay.username)):\(Self.percentEncoded(relay.password))@"
                           + "\(Self.hostText(relay.host)):\(relay.port)?transport=udp")
        }
        return servers
    }

    /// The `stun:` URL alone: what a control connection is made with, its
    /// relay servers added when gathering starts (``relayServerUrls``).
    public var stunServerUrls: [String] {
        libdatachannelServers.filter { $0.hasPrefix("stun:") }
    }

    /// The `turn:` URLs alone, credentials in them.
    public var relayServerUrls: [String] {
        libdatachannelServers.filter { $0.hasPrefix("turn:") }
    }

    /// The settings a session's media connection uses (link document
    /// section 20): the control connection's STUN server, and this end's
    /// relay only when the control connection's selected path goes through
    /// a relay (`controlPathRelayed` true, or nil while no path is known),
    /// so a direct session takes no relay allocation for its media. The
    /// Core's relay candidates are still accepted while the relay is
    /// allowed.
    public func forMedia(controlPathRelayed: Bool?) -> IceSettings {
        guard controlPathRelayed == false else {
            return self
        }
        var media = self
        media.relays = []
        media.relayKnown = true
        return media
    }

    /// Whether a candidate the Core sent may be used: a relay one only when
    /// the relay is allowed.
    public func acceptsRemoteCandidate(_ candidate: String) -> Bool {
        guard candidate.hasPrefix("candidate:") else {
            return false
        }
        return relayAllowed || Self.candidateType(candidate) != "relay"
    }

    // MARK: Choosing by family

    private func chooseByFamily(_ hosts: [String]) -> Int? {
        guard !hosts.isEmpty else {
            return nil
        }
        guard localFamilies.known, !localFamilies.isBoth else {
            return 0
        }
        return hosts.firstIndex { familiesOf(host: $0).shares(localFamilies) } ?? 0
    }

    private func familiesOf(host: String) -> AddressFamilies {
        let literal = AddressFamilies.ofLiteral(host)
        return literal.known ? literal : hostFamilies[host.lowercased()] ?? .none
    }

    // MARK: URLs (RFC 7064, RFC 7065)

    /// `stun:host[:port]`, an IPv6 literal in brackets.
    public static func parseStunUrl(_ url: String) -> Server? {
        guard url.hasPrefix("stun:"), !url.contains("?") else {
            return nil
        }
        return parseHostPort(String(url.dropFirst(5)))
    }

    /// `turn:host[:port][?transport=udp]`; nil for `turns:` and any other
    /// transport, which libjuice cannot use.
    public static func parseTurnUrl(_ url: String) -> Server? {
        guard url.hasPrefix("turn:") else {
            return nil
        }
        var rest = String(url.dropFirst(5))
        if let query = rest.firstIndex(of: "?") {
            guard rest[rest.index(after: query)...].lowercased() == "transport=udp" else {
                return nil
            }
            rest = String(rest[..<query])
        }
        return parseHostPort(rest)
    }

    /// The candidate's type (`host`, `srflx`, `prflx`, `relay`), empty when
    /// it has none.
    public static func candidateType(_ candidate: String) -> String {
        let fields = candidate.split(separator: " ")
        guard let index = fields.firstIndex(of: "typ"), index + 1 < fields.count else {
            return ""
        }
        return String(fields[index + 1])
    }

    /// The names (not IP literals) the STUN and TURN URLs use, each once, in
    /// lower case: what to resolve before choosing.
    public static func hostNames(_ urls: [String]) -> [String] {
        var names: [String] = []
        for url in urls {
            guard let address = parseStunUrl(url) ?? parseTurnUrl(url),
                  !AddressFamilies.ofLiteral(address.host).known else {
                continue
            }
            let name = address.host.lowercased()
            if !names.contains(name) {
                names.append(name)
            }
        }
        return names
    }

    private static func parseHostPort(_ text: String) -> Server? {
        guard !text.isEmpty else {
            return nil
        }
        var host: String
        var portText: Substring?
        if text.hasPrefix("[") {
            guard let close = text.firstIndex(of: "]") else {
                return nil
            }
            host = String(text[text.index(after: text.startIndex)..<close])
            let rest = text[text.index(after: close)...]
            if !rest.isEmpty {
                guard rest.hasPrefix(":") else {
                    return nil
                }
                portText = rest.dropFirst()
            }
            guard AddressFamilies.ofLiteral(host) == .ipv6Only else {
                return nil
            }
        } else {
            // A bare IPv6 literal is not allowed: RFC 7064 writes it in brackets.
            let colons = text.filter { $0 == ":" }.count
            guard colons <= 1 else {
                return nil
            }
            if let colon = text.firstIndex(of: ":") {
                host = String(text[..<colon])
                portText = text[text.index(after: colon)...]
            } else {
                host = text
            }
            let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: ".-"))
            guard !host.isEmpty, host.unicodeScalars.allSatisfy({ allowed.contains($0) && $0.isASCII }) else {
                return nil
            }
        }
        var port = defaultPort
        if let portText {
            guard let value = UInt16(portText), value >= 1 else {
                return nil
            }
            port = value
        }
        return Server(host: host, port: port)
    }

    private static func hostText(_ host: String) -> String {
        host.contains(":") ? "[\(host)]" : host
    }

    /// RFC 3986's unreserved characters kept, everything else as `%XX`.
    private static func percentEncoded(_ text: String) -> String {
        var out = ""
        for byte in text.utf8 {
            let unreserved = (0x41...0x5A).contains(byte) || (0x61...0x7A).contains(byte)
                || (0x30...0x39).contains(byte) || [0x2D, 0x2E, 0x5F, 0x7E].contains(byte)
            if unreserved {
                out.unicodeScalars.append(Unicode.Scalar(byte))
            } else {
                out += String(format: "%%%02X", byte)
            }
        }
        return out
    }

    // MARK: Resolving the servers' names

    /// Resolves `names` and reports the families each resolved to, within
    /// `timeout`: a name not back by then cannot be told and is left out,
    /// and the service's first entry is kept. An IP literal needs no lookup.
    public static func resolveHostFamilies(_ names: [String],
                                           timeout: Duration = hostLookupTimeout) async -> [String: AddressFamilies] {
        let wanted = Array(Set(names.map { $0.lowercased() }.filter { !$0.isEmpty && !AddressFamilies.ofLiteral($0).known }))
        guard !wanted.isEmpty else {
            return [:]
        }
        let lookup = HostLookup(pending: wanted.count)
        return await withCheckedContinuation { continuation in
            lookup.start(continuation)
            for name in wanted {
                DispatchQueue.global(qos: .userInitiated).async {
                    lookup.resolved(name, families: familiesByLookup(name))
                }
            }
            let (seconds, attoseconds) = timeout.components
            DispatchQueue.global().asyncAfter(deadline: .now() + .nanoseconds(Int(seconds) * 1_000_000_000
                                                                              + Int(attoseconds / 1_000_000_000))) {
                lookup.finish()
            }
        }
    }

    private static func familiesByLookup(_ name: String) -> AddressFamilies {
        var hints = addrinfo()
        hints.ai_family = AF_UNSPEC
        hints.ai_socktype = SOCK_DGRAM
        var result: UnsafeMutablePointer<addrinfo>?
        guard getaddrinfo(name, nil, &hints, &result) == 0, let first = result else {
            return .none
        }
        defer { freeaddrinfo(result) }
        var families = AddressFamilies.none
        var cursor: UnsafeMutablePointer<addrinfo>? = first
        while let item = cursor {
            if item.pointee.ai_family == AF_INET {
                families.ipv4 = true
            } else if item.pointee.ai_family == AF_INET6, let address = item.pointee.ai_addr {
                let mapped = address.withMemoryRebound(to: sockaddr_in6.self, capacity: 1) { pointer -> Bool in
                    let bytes = withUnsafeBytes(of: pointer.pointee.sin6_addr) { Array($0) }
                    return bytes[0..<10].allSatisfy { $0 == 0 } && bytes[10] == 0xFF && bytes[11] == 0xFF
                }
                if mapped {
                    families.ipv4 = true
                } else {
                    families.ipv6 = true
                }
            }
            cursor = item.pointee.ai_next
        }
        return families
    }

    /// The lookups of one call, finished once: when every name is back or
    /// at the timeout, whichever comes first.
    private final class HostLookup: @unchecked Sendable {
        private let lock = NSLock()
        private var pending: Int
        private var result: [String: AddressFamilies] = [:]
        private var continuation: CheckedContinuation<[String: AddressFamilies], Never>?

        init(pending: Int) {
            self.pending = pending
        }

        func start(_ continuation: CheckedContinuation<[String: AddressFamilies], Never>) {
            lock.withLock { self.continuation = continuation }
        }

        func resolved(_ name: String, families: AddressFamilies) {
            let done: Bool = lock.withLock {
                if families.known {
                    result[name] = families
                }
                pending -= 1
                return pending == 0
            }
            if done {
                finish()
            }
        }

        func finish() {
            let (continuation, result) = lock.withLock { () -> (CheckedContinuation<[String: AddressFamilies], Never>?, [String: AddressFamilies]) in
                let taken = self.continuation
                self.continuation = nil
                return (taken, self.result)
            }
            continuation?.resume(returning: result)
        }
    }
}

extension IceSettings: CustomStringConvertible {
    /// Never the relay credentials.
    public var description: String {
        let stunText = stun.map { "\($0.host):\($0.port)" } ?? "none"
        return "IceSettings(stun: \(stunText), relays: \(relays.map(\.description)), relayKnown: \(relayKnown), "
            + "relayAllowed: \(relayAllowed))"
    }
}

extension IceSettings.Relay: CustomStringConvertible {
    /// The relay's address only: never its username or password.
    public var description: String {
        "\(host):\(port)"
    }
}
