// NereusSDR for iOS: selected route evidence and strict candidate parsing tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Network
import Testing
@testable import NereusLink

struct SelectedRouteObservationTests {
    private let host = "candidate:one 1 UDP 2130706431 192.0.2.8 49000 typ host"

    @Test func numericAddressRetainsFamilyScopeAndMappedForm() {
        #expect(NumericRouteEndpoint(address: "192.0.2.8", port: 49000)?.family == .ipv4)
        #expect(NumericRouteEndpoint(address: "2001:db8::8%en0", port: 49000)?.scope == "en0")
        #expect(NumericRouteEndpoint(address: "2001:db8::8%en0", port: 49000)?.family == .ipv6)
        #expect(NumericRouteEndpoint(address: "::ffff:192.0.2.8", port: 49000)?.family == .ipv6)
        #expect(NumericRouteEndpoint(address: "2001:0db8:0:0:0:0:0:8", port: 49000)
                == NumericRouteEndpoint(address: "2001:db8::8", port: 49000))
        #expect(NumericRouteEndpoint(address: "core.example", port: 49000) == nil)
        #expect(NumericRouteEndpoint(address: "192.0.2.8", port: 0) == nil)
    }

    @Test func nominatedDirectAndReflexiveCandidatesPreserveEvidence() {
        let local = "candidate:local 1 UDP 2130706431 2001:db8::2%en0 49001 typ host"
        for type in ["host", "srflx", "prflx"] {
            let remote = "candidate:remote 1 UDP 12345 2001:db8::8 49000 typ \(type)"
            let result = SelectedRouteObservation.fromSelectedICEPair(local: local, remote: remote)
            guard case .available(let route) = result else { Issue.record("selected pair was discarded"); return }
            #expect(route.kind == .direct)
            #expect(route.endpointRole == .nominatedICEPeer)
            #expect(route.selectedEndpoint.family == .ipv6)
            #expect(route.coreEndpoint == route.selectedEndpoint)
            #expect(route.localCandidateType == .host)
        }
    }

    @Test func turnAndKnownFarEndRelayNeverClaimCoreEndpoint() {
        let relay = "candidate:relay 1 UDP 12345 198.51.100.7 52000 typ relay"
        let selected = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: relay)
        guard case .available(let route) = selected else { Issue.record("TURN pair unavailable"); return }
        #expect(route.kind == .turnRelay)
        #expect(route.selectedEndpoint.address == "198.51.100.7")
        #expect(route.coreEndpoint == nil)
        let learned = "candidate:learned 1 UDP 12345 198.51.100.7 52000 typ prflx"
        let endpoint = NumericRouteEndpoint(address: "198.51.100.7", port: 52000)!
        let prflx = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: learned,
                                                                  knownFarEndRelays: [endpoint])
        guard case .available(let prflxRoute) = prflx else { Issue.record("learned pair unavailable"); return }
        #expect(prflxRoute.kind == .turnRelay)
        #expect(prflxRoute.coreEndpoint == nil)
        let ipv6Relay = "candidate:relay 1 UDP 12345 2001:0db8:0:0:0:0:0:7 52000 typ relay"
        let parsedRelay = SelectedRouteObservation.farEndRelayEndpoint(from: ipv6Relay)!
        let ipv6Learned = "candidate:learned 1 UDP 12345 2001:db8::7 52000 typ prflx"
        let equivalent = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: ipv6Learned,
                                                                      knownFarEndRelays: [parsedRelay])
        guard case .available(let equivalentRoute) = equivalent else { Issue.record("IPv6 pair unavailable"); return }
        #expect(equivalentRoute.kind == .turnRelay)
    }

    @Test func claimedShimMustMatchRemoteLoopbackAndPort() {
        let loopback = "candidate:shim 1 UDP 12345 127.0.0.1 53000 typ host"
        let ordinary = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: loopback)
        guard case .available(let direct) = ordinary else { Issue.record("loopback pair unavailable"); return }
        #expect(direct.kind == .direct)
        let claimed = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: loopback,
                                                                    carrierClaim: .webRelay(port: 53000))
        guard case .available(let relayed) = claimed else { Issue.record("claimed pair unavailable"); return }
        #expect(relayed.kind == .webRelay)
        #expect(relayed.endpointRole == .claimedLocalShim)
        #expect(relayed.coreEndpoint == nil)
        let mismatched = SelectedRouteObservation.fromSelectedICEPair(local: host, remote: loopback,
                                                                       carrierClaim: .wssTunnel(port: 53001))
        guard case .available(let other) = mismatched else { Issue.record("mismatched pair unavailable"); return }
        #expect(other.kind == .direct)
    }

    @Test func malformedOrUnselectedPairIsUnavailable() {
        #expect(SelectedRouteObservation.fromSelectedICEPair(local: nil, remote: host) == .unavailable(.noSelectedPair))
        for bad in [
            "candidate:short 1 UDP 1 192.0.2.8 49000 typ",
            "candidate:bad 1 UDP 1 core.example 49000 typ host",
            "candidate:bad 1 UDP 1 192.0.2.8 0 typ host",
            "candidate:bad 1 UDP 1 192.0.2.8 49000 typ bogus",
            "candidate:bad 1 UDP 1 192.0.2.8 49000 host host",
            "candidate:bad 1 UDP 1 192.0.2.8 49000 typ host raddr",
        ] {
            #expect(SelectedRouteObservation.fromSelectedICEPair(local: host, remote: bad)
                    == .unavailable(.malformedSelectedPair))
        }
    }

    @Test func pathEndpointNeedsNumericHostPort() {
        let numeric = NWEndpoint.hostPort(host: NWEndpoint.Host("192.0.2.8"), port: 49000)
        #expect(NumericRouteEndpoint(pathEndpoint: numeric)?.family == .ipv4)
        let named = NWEndpoint.hostPort(host: .name("core.example", nil), port: 49000)
        #expect(NumericRouteEndpoint(pathEndpoint: named) == nil)
        let scoped = NWEndpoint.hostPort(host: .ipv6(IPv6Address("fe80::1%lo0")!), port: 49000)
        #expect(NumericRouteEndpoint(pathEndpoint: scoped)?.address == "fe80::1")
        #expect(NumericRouteEndpoint(pathEndpoint: scoped)?.scope == "lo0")
        let direct = SelectedRouteObservation.fromReadyWebSocket(pathEndpoint: numeric, usesSystemProxy: false)
        guard case .available(let route) = direct else { Issue.record("ready path unavailable"); return }
        #expect(route.kind == .direct)
        #expect(route.coreEndpoint == route.selectedEndpoint)
        let proxy = SelectedRouteObservation.fromReadyWebSocket(pathEndpoint: numeric, usesSystemProxy: true)
        guard case .available(let proxied) = proxy else { Issue.record("proxy path unavailable"); return }
        #expect(proxied.kind == .systemProxy)
        #expect(proxied.endpointRole == .frameworkReportedProxiedPath)
        #expect(proxied.coreEndpoint == nil)
        #expect(SelectedRouteObservation.fromReadyWebSocket(pathEndpoint: named, usesSystemProxy: true)
                == .unavailable(.nonNumericPathEndpoint, kind: .systemProxy, transport: .tlsWebSocket))
    }

    // The lines libdatachannel's rtcGetSelectedCandidatePair writes: its
    // Candidate string form puts "a=" before libjuice's generated line, and
    // libjuice adds "raddr 0.0.0.0 rport 0" to reflexive and relay lines
    // and "tcptype active" to a TCP one.
    private let wireHost = "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host"

    @Test func libdatachannelDirectPairReadsAsDirect() {
        let remote = "a=candidate:2 1 UDP 2122317823 198.51.100.7 50055 typ host"
        guard case .available(let route) = SelectedRouteObservation.fromSelectedICEPair(
            local: wireHost, remote: remote) else { Issue.record("direct pair unavailable"); return }
        #expect(route.kind == .direct)
        #expect(route.transport == .iceUDP)
        #expect(route.selectedEndpoint == NumericRouteEndpoint(address: "198.51.100.7", port: 50055))
        #expect(route.coreEndpoint == route.selectedEndpoint)
        let v6Local = "a=candidate:1 1 UDP 2122317823 2001:db8::2 50000 typ host"
        let v6Remote = "a=candidate:2 1 UDP 1686052607 2001:db8::7 50055 typ srflx raddr 0.0.0.0 rport 0"
        guard case .available(let v6) = SelectedRouteObservation.fromSelectedICEPair(
            local: v6Local, remote: v6Remote) else { Issue.record("IPv6 pair unavailable"); return }
        #expect(v6.selectedEndpoint.family == .ipv6)
        #expect(v6.remoteCandidateType == .srflx)
    }

    @Test func libdatachannelRelayedPairReadsAsRelayed() {
        let theirRelay = "a=candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0"
        guard case .available(let far) = SelectedRouteObservation.fromSelectedICEPair(
            local: wireHost, remote: theirRelay) else { Issue.record("far relay pair unavailable"); return }
        #expect(far.kind == .turnRelay)
        #expect(far.remoteCandidateType == .relay)
        #expect(far.coreEndpoint == nil)
        let ourRelay = "a=candidate:4 1 UDP 16777215 198.51.100.23 62000 typ relay raddr 0.0.0.0 rport 0"
        let theirReflexive = "a=candidate:5 1 UDP 1686052607 198.51.100.7 50055 typ srflx raddr 0.0.0.0 rport 0"
        guard case .available(let near) = SelectedRouteObservation.fromSelectedICEPair(
            local: ourRelay, remote: theirReflexive) else { Issue.record("near relay pair unavailable"); return }
        #expect(near.kind == .turnRelay)
        #expect(near.localCandidateType == .relay)
        #expect(near.coreEndpoint == nil)
    }

    @Test func libdatachannelPeerReflexivePairUsesTheAdmittedRelay() {
        // The Core's relay line arrives through signalling without "a=";
        // the selected pair then reports it as peer-reflexive with "a=".
        let admitted = "candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0"
        let learned = "a=candidate:6 1 UDP 1845501695 198.51.100.22 61000 typ prflx"
        let relay = SelectedRouteObservation.farEndRelayEndpoint(from: admitted)
        #expect(relay != nil)
        #expect(SelectedRouteObservation.farEndRelayEndpoint(from: "a=" + admitted) == relay)
        guard case .available(let route) = SelectedRouteObservation.fromSelectedICEPair(
            local: wireHost, remote: learned, knownFarEndRelays: Set([relay].compactMap { $0 }))
        else { Issue.record("peer-reflexive pair unavailable"); return }
        #expect(route.kind == .turnRelay)
        #expect(route.remoteCandidateType == .prflx)
        guard case .available(let unknown) = SelectedRouteObservation.fromSelectedICEPair(
            local: wireHost, remote: learned) else { Issue.record("peer-reflexive pair unavailable"); return }
        #expect(unknown.kind == .direct)
    }

    @Test func libdatachannelTcpPairKeepsItsTransport() {
        let local = "a=candidate:1 1 TCP 1015022079 192.0.2.10 9 typ host tcptype active"
        let remote = "a=candidate:2 1 TCP 1015022079 198.51.100.7 50056 typ host tcptype active"
        guard case .available(let route) = SelectedRouteObservation.fromSelectedICEPair(
            local: local, remote: remote) else { Issue.record("TCP pair unavailable"); return }
        #expect(route.transport == .iceTCP)
        #expect(SelectedRouteObservation.fromSelectedICEPair(local: wireHost, remote: remote)
                == .unavailable(.malformedSelectedPair))
    }

    @Test func incompleteLibdatachannelLinesAreStillRefused() {
        for bad in [
            "a=",
            "a=candidate:",
            "a=candidate: 1 UDP 1 192.0.2.8 49000 typ host",
            "a=a=candidate:1 1 UDP 1 192.0.2.8 49000 typ host",
            "a=mid:0",
            "a=candidate:1 1 UDP 1 192.0.2.8 49000 typ relay raddr 0.0.0.0 rport",
            "a=candidate:1 1 UDP 1 core.local 49000 typ host",
            " a=candidate:1 1 UDP 1 192.0.2.8 49000 typ host",
        ] {
            #expect(SelectedRouteObservation.fromSelectedICEPair(local: wireHost, remote: bad)
                    == .unavailable(.malformedSelectedPair), "\(bad)")
        }
    }

    @Test func onlyARelayAllocationIsCalledTheRelaysAddress() {
        let farRelay = "a=candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0"
        guard case .available(let far) = SelectedRouteObservation.fromSelectedICEPair(
            local: wireHost, remote: farRelay) else { Issue.record("far relay pair unavailable"); return }
        #expect(far.selectedEndpointIsRelay)

        // Our relay, their reflexive address: the address shown is the Core's.
        let ourRelay = "a=candidate:4 1 UDP 16777215 198.51.100.23 62000 typ relay raddr 0.0.0.0 rport 0"
        let theirReflexive = "a=candidate:5 1 UDP 1686052607 198.51.100.7 50055 typ srflx raddr 0.0.0.0 rport 0"
        guard case .available(let near) = SelectedRouteObservation.fromSelectedICEPair(
            local: ourRelay, remote: theirReflexive) else { Issue.record("near relay pair unavailable"); return }
        #expect(near.kind == .turnRelay)
        #expect(!near.selectedEndpointIsRelay)

        // Their relay learned as peer-reflexive counts only when admitted.
        let learned = "a=candidate:6 1 UDP 1845501695 198.51.100.22 61000 typ prflx"
        let admitted = NumericRouteEndpoint(address: "198.51.100.22", port: 61000)!
        guard case .available(let matched) = SelectedRouteObservation.fromSelectedICEPair(
            local: ourRelay, remote: learned, knownFarEndRelays: [admitted])
        else { Issue.record("matched pair unavailable"); return }
        #expect(matched.selectedEndpointIsRelay)
        guard case .available(let unmatched) = SelectedRouteObservation.fromSelectedICEPair(
            local: ourRelay, remote: learned) else { Issue.record("unmatched pair unavailable"); return }
        #expect(unmatched.kind == .turnRelay)
        #expect(!unmatched.selectedEndpointIsRelay)
    }
}
