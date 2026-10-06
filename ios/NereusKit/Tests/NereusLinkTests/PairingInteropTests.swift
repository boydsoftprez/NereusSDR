// NereusSDR for iOS: the app pairs with the station's own pairing code, across two processes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-08, R-IOS-16, D37: ``PairingClient`` (spake2-ee and libsodium in
/// Swift) against the Core's own `StationServer` pairing (the same pins in
/// C++), run by `nereus_pairing_peer` with a scratch Core for each run. Run
/// by `ios/scripts/interop-test.sh`, which builds the helper and sets
/// `NEREUS_PAIRING_PEER`; without it these tests are skipped. The scratch
/// code is used, never printed.
@Suite(.serialized, .enabled(if: PairingPeerProcess.path != nil,
                             "set NEREUS_PAIRING_PEER (ios/scripts/interop-test.sh)"))
struct PairingInteropTests {
    /// Where the client thinks it dials; the helper is the connection.
    private static let endpoint = StationEndpoint(host: "127.0.0.1")
    /// The Core's wait before the next code after one wrong code
    /// (PairingWindow::kFirstRetryMs, src/core/security/PairingWindow.h:103).
    private static let firstPairingGapMs = 5000

    private static func client(_ peer: PairingPeerProcess) throws -> PairingClient {
        try PairingClient(identity: try DeviceIdentity.load(store: InMemoryKeyStore()), name: "Interop iPhone",
                          kind: .phone, transportFactory: { _, trust in
                              precondition(trust == .pairing, "a pairing connection is dialled under the pairing trust")
                              return peer
                          })
    }

    /// The error a pairing ended with, or nil when it paired.
    private static func pairingError(_ pairing: () async throws -> PairedStation) async -> PairingError? {
        do {
            _ = try await pairing()
            return nil
        } catch {
            return error as? PairingError ?? .ended(reason: "not a pairing error")
        }
    }

    /// Checks that the pairing's Core is the one its hello named, bound to
    /// the certificate `peer.ready` reported.
    private static func checkIdentity(_ paired: PairedStation, _ peer: PairingPeerProcess,
                                      _ ready: PairingPeerProcess.Ready) {
        guard case .hello(let hello)? = peer.stationMessages.first, let claim = hello.identity else {
            Issue.record("the Core's first message is not a hello with its identity")
            return
        }
        #expect(Base64URL.decode(claim.publicKey) == paired.identityKey)
        #expect(StationTrust.identityVerifies(claim, publicKey: paired.identityKey,
                                              certificateSHA256: ready.certificateSHA256))
        #expect(paired.endpoints == [Self.endpoint])
    }

    /// `code` with its first word swapped for the next word in the list.
    private static func withFirstWordSwapped(_ code: String) -> String {
        let parts = code.split(separator: "-").map(String.init)
        let words = PairingCodeText.words
        guard parts.count == 3, let index = words.firstIndex(of: parts[1]) else {
            return ""
        }
        return "\(parts[0])-\(words[(index + 1) % words.count])-\(parts[2])"
    }

    @Test func theCodePairsWithTheCoreItsHelloNames() async throws {
        let peer = try PairingPeerProcess()
        defer { peer.stop() }
        let ready = try await peer.ready()
        let paired = try await Self.client(peer).pair(code: ready.code, via: .direct(Self.endpoint))
        Self.checkIdentity(paired, peer, ready)
        #expect(try await peer.done() == PairingPeerProcess.Done(paired: true, devices: 1))
    }

    @Test func aWrongCodeIsBurnedAndTheCoreAsksForItsWait() async throws {
        let peer = try PairingPeerProcess()
        defer { peer.stop() }
        let ready = try await peer.ready()
        let wrong = Self.withFirstWordSwapped(ready.code)
        let client = try Self.client(peer)
        let failure = await Self.pairingError { try await client.pair(code: wrong, via: .direct(Self.endpoint)) }
        guard case .wrongCode(let retryAfter, let reason)? = failure else {
            Issue.record("the pairing did not end as a wrong code: \(String(describing: failure))")
            return
        }
        // The Core reports what is left of its gap before the next code, the
        // first gap being PairingWindow::kFirstRetryMs, 5000 ms
        // (src/core/security/PairingWindow.h:103, counted down in
        // PairingWindow::retryAfterMs, PairingWindow.cpp:264-273), so a
        // loaded machine can read 4999: a wait there, and no longer.
        #expect(retryAfter > .zero, "the Core sent no wait")
        #expect(retryAfter <= .milliseconds(Self.firstPairingGapMs), "the Core asked for more than its first gap")
        #expect(reason == "The pairing code was not right. A new code will appear on the Core.")
        #expect(try await peer.done() == PairingPeerProcess.Done(paired: false, devices: 0))
    }

    @Test func oneTapPairs() async throws {
        let peer = try PairingPeerProcess()
        defer { peer.stop() }
        let ready = try await peer.ready()
        let paired = try await Self.client(peer).pairOnThisNetwork(endpoint: Self.endpoint)
        Self.checkIdentity(paired, peer, ready)
        #expect(try await peer.done() == PairingPeerProcess.Done(paired: true, devices: 1))
    }

    @Test(arguments: [
        (["--claimed"], "One tap pairs only a Core with no paired devices. Use the pairing code the Core shows.", 1),
        (["--lan-deny"], "This Core pairs only with its code. Use the pairing code the Core shows.", 0),
        (["--address", "192.0.2.7"],
         "One tap works only on the Core's own network. Use the pairing code the Core shows.", 0),
    ])
    func oneTapIsRefused(_ arguments: [String], _ reason: String, _ devices: Int) async throws {
        let peer = try PairingPeerProcess(arguments: arguments)
        defer { peer.stop() }
        _ = try await peer.ready()
        let client = try Self.client(peer)
        let failure = await Self.pairingError { try await client.pairOnThisNetwork(endpoint: Self.endpoint) }
        #expect(failure == .refused(reason: reason, retryAfter: .zero))
        #expect(try await peer.done() == PairingPeerProcess.Done(paired: false, devices: devices))
    }

    @Test func aClaimedCoreReopenedPairsOneMoreByCode() async throws {
        let peer = try PairingPeerProcess(arguments: ["--claimed"])
        defer { peer.stop() }
        let ready = try await peer.ready()
        let paired = try await Self.client(peer).pair(code: ready.code, via: .direct(Self.endpoint))
        Self.checkIdentity(paired, peer, ready)
        #expect(try await peer.done() == PairingPeerProcess.Done(paired: true, devices: 2))
    }
}

#endif
