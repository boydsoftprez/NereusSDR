// NereusSDR for iOS: the ICE settings of a peer that came through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-16 (iPhone app plan Task 27a): ``IceSettings`` and the peer
/// configuration the bridge is made with, as the Core's own (Task 27): one
/// STUN server, at most two relay servers carrying the IPv4-only and
/// IPv6-only relay names, credentials fixed before gathering, MTU 996.
@Suite struct IceSettingsTests {
    /// The NereusSDR service's lists (the rendezvous document, section 8),
    /// IPv4-only name first.
    static let stun = ["stun:rv4.nereussdr.com:3478", "stun:rv6.nereussdr.com:3478"]
    static let turn = RendezvousTurn(
        username: "1800086400:lwhu2kyjnrfdwvpxvdckao3lb7", password: "pHOMyrrf/XfuT+cSEc=", expires: 1_800_086_400,
        urls: ["turn:rv4.nereussdr.com:3478?transport=udp", "turn:rv4.nereussdr.com:443?transport=udp",
               "turn:rv6.nereussdr.com:3478?transport=udp", "turn:rv6.nereussdr.com:443?transport=udp"])
    static let resolved: [String: AddressFamilies] = ["rv4.nereussdr.com": .ipv4Only, "rv6.nereussdr.com": .ipv6Only]

    private static func counts(_ servers: [String]) -> (stun: Int, turn: Int) {
        (servers.filter { $0.hasPrefix("stun:") }.count, servers.filter { $0.hasPrefix("turn:") }.count)
    }

    @Test func bothSlotsCarryTheIPv4OnlyAndIPv6OnlyRelayNamesInOrder() throws {
        var ice = IceSettings(stunUrls: Self.stun, local: .both, hosts: Self.resolved)
        #expect(ice.setRelay(Self.turn, families: 2) == 2)
        #expect(ice.relays.map(\.host) == ["rv4.nereussdr.com", "rv6.nereussdr.com"])
        #expect(ice.relays.map(\.port) == [3478, 3478])
        let configuration = try MediaPeer.Configuration.throughRendezvous(ice)
        let bridged = RtcBridge.iceServers(configuration.iceServers)
        #expect(Self.counts(bridged) == (1, 2))
        #expect(bridged == [
            "stun:rv4.nereussdr.com:3478",
            "turn:1800086400%3Alwhu2kyjnrfdwvpxvdckao3lb7:pHOMyrrf%2FXfuT%2BcSEc%3D@rv4.nereussdr.com:3478?transport=udp",
            "turn:1800086400%3Alwhu2kyjnrfdwvpxvdckao3lb7:pHOMyrrf%2FXfuT%2BcSEc%3D@rv6.nereussdr.com:3478?transport=udp",
        ])
        #expect(configuration.mtu == 996)
        #expect(!configuration.hostCandidatesOnly)
    }

    @Test func oneFamilyTakesTheRelayHostItCanReachWhateverTheOrder() {
        // An IPv6-only phone (say on cellular with NAT64) behind the service's IPv4-first list.
        var ipv6 = IceSettings(stunUrls: Self.stun, local: .ipv6Only, hosts: Self.resolved)
        #expect(ipv6.stun == IceSettings.Server(host: "rv6.nereussdr.com", port: 3478))
        #expect(ipv6.setRelay(Self.turn, families: 1) == 1)
        #expect(ipv6.relays.map(\.host) == ["rv6.nereussdr.com"])
        // An IPv4-only phone behind NAT, with the list reversed.
        var ipv4 = IceSettings(stunUrls: Self.stun.reversed(), local: .ipv4Only, hosts: Self.resolved)
        #expect(ipv4.stun?.host == "rv4.nereussdr.com")
        var reversed = Self.turn
        reversed.urls.reverse()
        ipv4.setRelay(reversed, families: 1)
        #expect(ipv4.relays.map(\.host) == ["rv4.nereussdr.com"])
        // Both families: the service's first entry alone.
        var both = IceSettings(stunUrls: Self.stun, local: .both, hosts: Self.resolved)
        both.setRelay(Self.turn, families: 1)
        #expect(both.stun?.host == "rv4.nereussdr.com")
        #expect(both.relays.map(\.host) == ["rv4.nereussdr.com"])
    }

    /// The rendezvous document, section 8: when the phone cannot tell (its
    /// families unknown, or no relay name resolved), the first entry alone.
    @Test func aPhoneThatCannotTellTakesTheFirstRelayEntryOnly() {
        var unresolved = IceSettings(stunUrls: Self.stun, local: .ipv6Only)
        #expect(unresolved.setRelay(Self.turn, families: 1) == 1)
        #expect(unresolved.relays.map(\.host) == ["rv4.nereussdr.com"])
        #expect(Self.counts(unresolved.libdatachannelServers) == (1, 1))
        var noAddress = IceSettings(stunUrls: Self.stun, local: .none, hosts: Self.resolved)
        noAddress.setRelay(Self.turn, families: 1)
        #expect(noAddress.stun?.host == "rv4.nereussdr.com")
        #expect(noAddress.relays.map(\.host) == ["rv4.nereussdr.com"])
    }

    @Test func noDescriptionShowsTheRelayCredentials() throws {
        var ice = IceSettings(stunUrls: Self.stun, local: .both, hosts: Self.resolved)
        ice.setRelay(Self.turn, families: 2)
        let configuration = try MediaPeer.Configuration.throughRendezvous(ice)
        let username = Self.turn.username
        let stationId = String(username.split(separator: ":")[1])
        for text in [String(describing: ice), String(describing: ice.relays), "\(ice.relays[0])",
                     String(describing: configuration), String(describing: Self.turn)] {
            #expect(!text.contains("pHOMyrrf"), "\(text)")
            #expect(!text.contains(stationId), "\(text)")
        }
        // A library line quoting a relay URL loses its user and password.
        let refused = "Invalid ICE server URL: " + configuration.iceServers[1]
        #expect(RtcBridge.withoutCredentials(refused)
                == "Invalid ICE server URL: turn:<removed>@rv4.nereussdr.com:3478?transport=udp")
        #expect(RtcBridge.withoutCredentials("stun:rv4.nereussdr.com:3478") == "stun:rv4.nereussdr.com:3478")
    }

    @Test func gatheringWaitsForTheCredentials() throws {
        var ice = IceSettings(stunUrls: Self.stun, local: .both)
        #expect(!ice.relayKnown)
        // No peer can be made to gather before the answer's turn is known.
        #expect(throws: IceSettings.NotReady()) { try MediaPeer.Configuration.throughRendezvous(ice) }
        ice.setRelay(nil, families: 1)
        #expect(ice.relayKnown)
        let configuration = try MediaPeer.Configuration.throughRendezvous(ice)
        #expect(configuration.iceServers == ["stun:rv4.nereussdr.com:3478"])
        // The peer takes its servers when it is made; libdatachannel reads the URLs.
        let peer = MediaPeer(configuration: configuration)
        #expect(peer.configuration.iceServers == configuration.iceServers)
        peer.close()
    }

    @Test func aRelayedPeerIsMadeWithTheCredentials() throws {
        var ice = IceSettings(stunUrls: Self.stun, local: .both, hosts: Self.resolved)
        ice.setRelay(Self.turn, families: 2)
        let peer = MediaPeer(configuration: try .throughRendezvous(ice))
        #expect(peer.configuration.mtu == 996)
        #expect(Self.counts(peer.configuration.iceServers) == (1, 2))
        peer.close()
    }

    @Test func theBridgeHoldsTheLibrarysLimits() {
        let many = ["stun:a.example:3478", "stun:b.example:3478", "turn:u:p@c.example:3478?transport=udp",
                    "turn:u:p@d.example:3478?transport=udp", "turn:u:p@e.example:3478?transport=udp", "turns:f.example"]
        #expect(RtcBridge.iceServers(many) == ["stun:a.example:3478", "turn:u:p@c.example:3478?transport=udp",
                                               "turn:u:p@d.example:3478?transport=udp"])
        #expect(RtcBridge.iceServers([]) == [])
    }

    @Test func aDeniedRelayGivesNoRelayAndRefusesRelayCandidates() {
        var ice = IceSettings(stunUrls: Self.stun, relayAllowed: false, local: .both)
        #expect(ice.setRelay(Self.turn, families: 2) == 0)
        #expect(ice.relayKnown)
        #expect(!ice.acceptsRemoteCandidate("candidate:1 1 UDP 1 198.51.100.4 3478 typ relay raddr 0.0.0.0 rport 0"))
        #expect(ice.acceptsRemoteCandidate("candidate:1 1 UDP 2122317823 192.0.2.7 50123 typ host"))
        let allowed = IceSettings(stunUrls: Self.stun, local: .both)
        #expect(allowed.acceptsRemoteCandidate("candidate:1 1 UDP 1 198.51.100.4 3478 typ relay raddr 0.0.0.0 rport 0"))
    }

    @Test func urlsAreReadAsRFC7064And7065WriteThem() {
        #expect(IceSettings.parseStunUrl("stun:rv4.nereussdr.com") == IceSettings.Server(host: "rv4.nereussdr.com", port: 3478))
        #expect(IceSettings.parseStunUrl("stun:[2001:db8::1]:19302") == IceSettings.Server(host: "2001:db8::1", port: 19302))
        #expect(IceSettings.parseStunUrl("stun:2001:db8::1") == nil)
        #expect(IceSettings.parseTurnUrl("turns:rv4.nereussdr.com:443") == nil)
        #expect(IceSettings.parseTurnUrl("turn:rv4.nereussdr.com:443?transport=tcp") == nil)
        #expect(IceSettings.parseTurnUrl("turn:rv6.nereussdr.com:443?transport=udp")?.port == 443)
        #expect(IceSettings.hostNames(Self.stun + Self.turn.urls + ["stun:192.0.2.1:3478"])
                == ["rv4.nereussdr.com", "rv6.nereussdr.com"])
        #expect(IceSettings.candidateType("candidate:1 1 UDP 1 192.0.2.7 1 typ srflx raddr 0.0.0.0 rport 0") == "srflx")
    }

    @Test func anIPv6LiteralRelayIsBracketed() {
        var ice = IceSettings(stunUrls: ["stun:[2001:db8::2]:3478"], local: .ipv6Only)
        ice.setRelay(RendezvousTurn(username: "u", password: "p", expires: 0, urls: ["turn:[2001:db8::2]:3478"]),
                     families: 1)
        #expect(ice.libdatachannelServers == ["stun:[2001:db8::2]:3478", "turn:u:p@[2001:db8::2]:3478?transport=udp"])
    }

    @Test func theConnectDeadlineCoversGatheringAndTheChecks() {
        #expect(IceSettings.gatheringDeadline == .milliseconds(23_500))
        #expect(IceSettings.connectivityTimeout == .milliseconds(39_500))
        #expect(IceSettings.connectDeadline == .milliseconds(63_000))
    }

    @Test func aLiteralNeedsNoLookupAndLocalhostResolvesHere() async {
        #expect(await IceSettings.resolveHostFamilies(["192.0.2.1", "[2001:db8::1]"]).isEmpty)
        let local = await IceSettings.resolveHostFamilies(["localhost"])
        #expect(local["localhost"]?.known == true)
    }
}
