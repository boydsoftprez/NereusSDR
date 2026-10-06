// NereusSDR for iOS: read-only evidence about a selected network route
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import Network

/// A numeric address only. An IPv4-mapped IPv6 address keeps its IPv6 family.
/// `scope` is the zone of a scoped IPv6 literal when the source provides one.
public struct NumericRouteEndpoint: Sendable, Equatable, Hashable {
    public enum Family: Sendable, Hashable { case ipv4, ipv6 }
    public let address: String
    public let port: UInt16
    public let family: Family
    public let scope: String?
    private let binaryAddress: Data

    public init?(address: String, port: UInt16) {
        guard port > 0, !address.isEmpty, address.utf8.count <= 128 else { return nil }
        let parts = address.split(separator: "%", omittingEmptySubsequences: false)
        guard parts.count <= 2 else { return nil }
        let bare = String(parts[0])
        let zone = parts.count == 2 ? String(parts[1]) : nil
        if let zone {
            guard !zone.isEmpty, zone.utf8.count <= 64,
                  zone.utf8.allSatisfy({ byte in
                      (48...57).contains(byte) || (65...90).contains(byte) ||
                      (97...122).contains(byte) || byte == 45 || byte == 46 || byte == 95
                  }) else { return nil }
        }
        var v4 = in_addr()
        var v6 = in6_addr()
        let isV4 = bare.withCString { inet_pton(AF_INET, $0, &v4) == 1 }
        let isV6 = bare.withCString { inet_pton(AF_INET6, $0, &v6) == 1 }
        guard isV6 || (isV4 && zone == nil) else { return nil }
        self.address = bare
        self.port = port
        family = isV6 ? .ipv6 : .ipv4
        scope = zone
        binaryAddress = isV6 ? Data(bytes: &v6, count: MemoryLayout<in6_addr>.size)
            : Data(bytes: &v4, count: MemoryLayout<in_addr>.size)
    }

    /// Network's path endpoint, never the destination supplied to its dial.
    /// A name remains unknown even if a separate resolver might resolve it.
    public init?(pathEndpoint: NWEndpoint) {
        guard case .hostPort(let host, let nwPort) = pathEndpoint else { return nil }
        let address: String
        let zone: String?
        switch host {
        case .ipv4(let value):
            address = value.debugDescription
            zone = nil
        case .ipv6(let value):
            let rendered = value.debugDescription
            let parts = rendered.split(separator: "%", omittingEmptySubsequences: false)
            guard parts.count <= 2 else { return nil }
            address = String(parts[0])
            zone = value.interface?.name ?? (parts.count == 2 ? String(parts[1]) : nil)
        case .name:
            return nil
        @unknown default:
            return nil
        }
        let literal = zone.map { "\(address)%\($0)" } ?? address
        self.init(address: literal, port: nwPort.rawValue)
    }

    public static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.family == rhs.family && lhs.port == rhs.port && lhs.scope == rhs.scope &&
            lhs.binaryAddress == rhs.binaryAddress
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(family)
        hasher.combine(port)
        hasher.combine(scope)
        hasher.combine(binaryAddress)
    }
}

public enum SelectedRouteTransport: Sendable, Equatable {
    case tlsWebSocket, iceUDP, iceTCP
}

public enum SelectedRouteKind: Sendable, Equatable {
    case unknown, direct, turnRelay, webRelay, wssTunnel, systemProxy
}

/// The address's meaning matters as much as the address. In particular an
/// ICE candidate is not necessarily the phone's physical UDP socket peer.
public enum SelectedEndpointRole: Sendable, Equatable {
    case readyPathRemote
    case frameworkReportedProxiedPath
    case nominatedICEPeer
    case claimedLocalShim
}

public enum SelectedICECandidateType: Sendable, Equatable {
    case host, srflx, prflx, relay
}

public enum SelectedRouteUnavailableReason: Sendable, Equatable {
    case notReady
    case noSelectedPath
    case nonNumericPathEndpoint
    case noSelectedPair
    case malformedSelectedPair
    case retired
    case noCurrentMedia
    case unsupported
}

/// One reading at the point of access, not a saved or advertised address.
/// `coreEndpoint` is populated only when the selected route gives direct
/// evidence of the Core's numeric endpoint. A selected relay allocation,
/// configured proxy path or local carrier shim does not give that proof.
public enum SelectedRouteObservation: Sendable, Equatable {
    public struct Route: Sendable, Equatable {
        public let kind: SelectedRouteKind
        public let transport: SelectedRouteTransport
        public let selectedEndpoint: NumericRouteEndpoint
        public let endpointRole: SelectedEndpointRole
        public let coreEndpoint: NumericRouteEndpoint?
        public let localCandidateType: SelectedICECandidateType?
        public let remoteCandidateType: SelectedICECandidateType?
        /// The far end's peer-reflexive address matched a relay allocation
        /// it offered to this same ICE agent.
        public let remoteMatchedFarEndRelay: Bool

        public init(kind: SelectedRouteKind, transport: SelectedRouteTransport,
                    selectedEndpoint: NumericRouteEndpoint, endpointRole: SelectedEndpointRole,
                    coreEndpoint: NumericRouteEndpoint?, localCandidateType: SelectedICECandidateType? = nil,
                    remoteCandidateType: SelectedICECandidateType? = nil,
                    remoteMatchedFarEndRelay: Bool = false) {
            self.kind = kind
            self.transport = transport
            self.selectedEndpoint = selectedEndpoint
            self.endpointRole = endpointRole
            self.coreEndpoint = coreEndpoint
            self.localCandidateType = localCandidateType
            self.remoteCandidateType = remoteCandidateType
            self.remoteMatchedFarEndRelay = remoteMatchedFarEndRelay
        }

        /// True when the selected address is a relay's allocation rather
        /// than the Core's own address. On a path relayed at this end only,
        /// the far end's candidate is the Core's (reflexive) address.
        public var selectedEndpointIsRelay: Bool {
            kind == .turnRelay && (remoteCandidateType == .relay || remoteMatchedFarEndRelay)
        }
    }

    case available(Route)
    case unavailable(SelectedRouteUnavailableReason, kind: SelectedRouteKind = .unknown,
                     transport: SelectedRouteTransport? = nil)

    public enum CarrierClaim: Sendable, Equatable {
        case webRelay(port: UInt16)
        case wssTunnel(port: UInt16)
    }

    /// Only an admitted, syntactically complete far-end relay candidate can
    /// identify a later peer-reflexive candidate as the same relay address.
    public static func farEndRelayEndpoint(from candidate: String) -> NumericRouteEndpoint? {
        guard let parsed = SelectedICECandidate(candidate), parsed.type == .relay else { return nil }
        return parsed.endpoint
    }

    /// Classifies only the pair nominated by ICE. Both candidate lines must
    /// be complete and numeric. The caller supplies far-end relay addresses
    /// actually admitted to this same ICE agent, and its held carrier claim.
    public static func fromSelectedICEPair(local: String?, remote: String?,
                                           knownFarEndRelays: Set<NumericRouteEndpoint> = [],
                                           carrierClaim: CarrierClaim? = nil) -> Self {
        guard let local, let remote else { return .unavailable(.noSelectedPair) }
        guard let ours = SelectedICECandidate(local), let theirs = SelectedICECandidate(remote),
              ours.transport == theirs.transport else { return .unavailable(.malformedSelectedPair) }
        let claimed: SelectedRouteKind?
        if theirs.type == .host, theirs.endpoint.family == .ipv4,
           theirs.endpoint.address == "127.0.0.1" {
            switch carrierClaim {
            case .webRelay(let port) where port == theirs.endpoint.port: claimed = .webRelay
            case .wssTunnel(let port) where port == theirs.endpoint.port: claimed = .wssTunnel
            default: claimed = nil
            }
        } else {
            claimed = nil
        }
        let farEndRelay = theirs.type == .prflx && knownFarEndRelays.contains(theirs.endpoint)
        let relayed = ours.type == .relay || theirs.type == .relay || farEndRelay
        let kind = claimed ?? (relayed ? .turnRelay : .direct)
        let role: SelectedEndpointRole = claimed == nil ? .nominatedICEPeer : .claimedLocalShim
        let core: NumericRouteEndpoint? = kind == .direct ? theirs.endpoint : nil
        return .available(Route(kind: kind, transport: ours.transport,
                                selectedEndpoint: theirs.endpoint, endpointRole: role,
                                coreEndpoint: core, localCandidateType: ours.type,
                                remoteCandidateType: theirs.type, remoteMatchedFarEndRelay: farEndRelay))
    }

    /// `NWPath.remoteEndpoint` on a ready connection. A configured system
    /// proxy makes the path endpoint framework-reported, not proof of either
    /// the proxy server's IP or the ultimate Core's IP.
    public static func fromReadyWebSocket(pathEndpoint: NWEndpoint?, usesSystemProxy: Bool) -> Self {
        let kind: SelectedRouteKind = usesSystemProxy ? .systemProxy : .direct
        guard let pathEndpoint else {
            return .unavailable(.noSelectedPath, kind: kind, transport: .tlsWebSocket)
        }
        guard let endpoint = NumericRouteEndpoint(pathEndpoint: pathEndpoint) else {
            return .unavailable(.nonNumericPathEndpoint, kind: kind, transport: .tlsWebSocket)
        }
        return .available(Route(kind: kind, transport: .tlsWebSocket,
                                selectedEndpoint: endpoint,
                                endpointRole: usesSystemProxy ? .frameworkReportedProxiedPath : .readyPathRemote,
                                coreEndpoint: usesSystemProxy ? nil : endpoint))
    }
}

private struct SelectedICECandidate {
    let endpoint: NumericRouteEndpoint
    let type: SelectedICECandidateType
    let transport: SelectedRouteTransport

    /// Takes a line as signalling carries it (`candidate:...`) and as
    /// libdatachannel's rtcGetSelectedCandidatePair writes it, which is the
    /// same line behind one SDP attribute prefix (`a=candidate:...`).
    init?(_ line: String) {
        guard line.utf8.count <= 512 else { return nil }
        let body = line.hasPrefix("a=") ? line.dropFirst(2) : Substring(line)
        let fields = body.split(whereSeparator: \.isWhitespace)
        guard fields.count >= 8, fields.count.isMultiple(of: 2),
              fields[0].hasPrefix("candidate:"), fields[0].count > "candidate:".count,
              let component = UInt16(fields[1]), component > 0,
              UInt32(fields[3]) != nil,
              let port = UInt16(fields[5]), port > 0,
              fields[6] == "typ",
              let endpoint = NumericRouteEndpoint(address: String(fields[4]), port: port) else { return nil }
        switch fields[2].uppercased() {
        case "UDP": transport = .iceUDP
        case "TCP": transport = .iceTCP
        default: return nil
        }
        switch fields[7] {
        case "host": type = .host
        case "srflx": type = .srflx
        case "prflx": type = .prflx
        case "relay": type = .relay
        default: return nil
        }
        for index in stride(from: 8, to: fields.count, by: 2) {
            guard !fields[index].isEmpty, !fields[index + 1].isEmpty else { return nil }
        }
        self.endpoint = endpoint
    }
}
