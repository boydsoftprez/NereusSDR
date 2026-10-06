// NereusSDR for iOS: the addresses each end offered through the service, and how each pair of them went
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

/// R-IOS-16 (2026-09-29): a connection through the remote access service
/// keeps what each end offered and which pair carried it, so a path that
/// never went over IPv6 shows why from the phone's side.
@Suite struct IceCandidateEvidenceTests {
    private static let phoneV6 = "candidate:1 1 UDP 2122317823 2001:db8:7700:48::5 61000 typ host"
    private static let phoneV4 = "a=candidate:2 1 UDP 1686052607 198.51.100.2 4000 typ srflx raddr 192.0.0.2 rport 4000"
    private static let coreV6 = "candidate:1 1 UDP 2122317823 2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2 50000 typ host"
    private static let coreLan = "candidate:2 1 UDP 2122252543 192.168.109.106 50001 typ host"
    private static let coreV4 = "candidate:3 1 UDP 1686052607 203.0.113.67 50001 typ srflx raddr 192.168.109.106 rport 50001"
    private static let coreRelay = "candidate:4 1 UDP 16777215 203.0.113.9 3478 typ relay raddr 0.0.0.0 rport 0"

    @Test func aCandidateLineReadsAsItsAddressPortAndKind() throws {
        let host = try #require(IceCandidateEvidence.Candidate(line: Self.coreV6))
        #expect(host.address == "2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2")
        #expect(host.port == 50000)
        #expect(host.kind == .host)
        #expect(host.isIPv6)
        let reflexive = try #require(IceCandidateEvidence.Candidate(line: Self.phoneV4))
        #expect(reflexive.kind == .reflexive && !reflexive.isIPv6 && reflexive.address == "198.51.100.2")
        #expect(IceCandidateEvidence.Candidate(line: "candidate:4 1 UDP 1 203.0.113.9 3478 typ relay")?.kind == .relay)
        #expect(IceCandidateEvidence.Candidate(line: "") == nil)
        #expect(IceCandidateEvidence.Candidate(line: "candidate:1 1 TCP 1 10.0.0.1 9 typ host tcptype active") == nil)
    }

    @Test func eachPairOfOneFamilyHasAnOutcome() throws {
        let evidence = IceCandidateEvidence(phone: [Self.phoneV6, Self.phoneV4],
                                            core: [Self.coreV6, Self.coreLan, Self.coreV4, ""],
                                            chosen: (local: Self.phoneV4, remote: Self.coreV4),
                                            ending: .connected)
        #expect(evidence.phone.count == 2)
        #expect(evidence.core.count == 3)
        #expect(evidence.pairs.map(\.outcome) == [.notUsed, .notUsed, .used])
        #expect(evidence.pairs.map { $0.phone.isIPv6 } == [true, false, false])
        #expect(evidence.summary == "This phone offered [2001:db8:7700:48::5]:61000 (its own), "
                + "198.51.100.2:4000 (as the internet sees it). "
                + "The Core offered [2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50000 (its own), "
                + "192.168.109.106:50001 (its own), 203.0.113.67:50001 (as the internet sees it). "
                + "[2001:db8:7700:48::5]:61000 to [2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50000: not used; "
                + "198.51.100.2:4000 to 192.168.109.106:50001: not used; "
                + "198.51.100.2:4000 to 203.0.113.67:50001: used. "
                + "A pair not used either failed its check or was passed over; this phone cannot tell which.")
    }

    @Test func whenNothingConnectedEveryPairFailed() {
        let evidence = IceCandidateEvidence(phone: [Self.phoneV6], core: [Self.coreV6, Self.coreRelay],
                                            chosen: nil, ending: .failed)
        #expect(evidence.pairs.map(\.outcome) == [.failed])
        #expect(evidence.summary.hasSuffix("[2001:db8:7700:48::5]:61000 to [2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50000: did not connect."))
        #expect(evidence.summary.contains("203.0.113.9:3478 (a relay)"))
    }

    @Test func aDialStoppedEarlyLeavesItsPairsUnfinished() {
        let evidence = IceCandidateEvidence(phone: [Self.phoneV4], core: [Self.coreV4], chosen: nil, ending: .stopped)
        #expect(evidence.pairs.map(\.outcome) == [.unfinished])
        #expect(evidence.summary.hasSuffix("198.51.100.2:4000 to 203.0.113.67:50001: not finished."))
    }

    @Test func aSideThatOfferedNothingSaysSo() {
        let evidence = IceCandidateEvidence(phone: [], core: [Self.coreV4], chosen: nil, ending: .failed)
        #expect(evidence.summary.hasPrefix("This phone offered nothing. The Core offered 203.0.113.67:50001"))
        #expect(evidence.pairs.isEmpty)
    }

    @Test func theServiceTryCarriesItsEvidence() {
        var tried = ConnectionAttempt.Try(path: .direct, address: "rv.nereussdr.com", throughService: true)
        #expect(tried.ice == nil)
        tried.ice = IceCandidateEvidence(phone: [Self.phoneV4], core: [Self.coreV4], chosen: nil, ending: .failed)
        #expect(tried.ice?.pairs.count == 1)
    }
}
