// NereusSDR for iOS: the fake Core pairs by code through a stand-in for the remote access service's mailbox
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusKitTesting
import NereusLink
import Testing

/// The app's pairing client, through ``FakeStation/rendezvousTransportFactory``:
/// the mailbox on the code's number carries the whole code exchange to the
/// fake's Core side, with no `hello` either way, and gives back the Core's
/// key from its sealed box and no address; a number the fake does not show
/// is refused in the service's words. Codes are the fake's, made at run time.
@Suite("FakeStation, pairing through the service's mailbox")
struct FakeRendezvousMailboxTests {
    /// A clock that never moves, for the pairing clients: the connect
    /// deadline is not what these tests check, and both sides hash the code
    /// with Argon2id (64 MiB), which on a busy Mac can outlast the 30 second
    /// wall-clock deadline and end a pairing that would have finished.
    private static var noDeadline: ManualLinkClock { ManualLinkClock() }

    private func client(_ station: FakeStation) throws -> PairingClient {
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        return try PairingClient(identity: device, name: "Test iPhone", kind: .phone, clock: Self.noDeadline,
                                 transportFactory: { _, _ in
                                     Issue.record("a pairing through the mailbox dialled the Core straight")
                                     return FailingTransport()
                                 },
                                 rendezvousTransportFactory: station.rendezvousTransportFactory)
    }

    @Test func aCodePairsThroughTheMailbox() async throws {
        let station = try FakeStation(requiresPairing: true)
        let code = station.pairingCode
        let nameplate = try #require(PairingClient.nameplate(ofNormalised: code))
        let paired = try await client(station).pair(code: code, via: .rendezvous(server: RendezvousServer.defaults,
                                                                               nameplate: nameplate))
        #expect(paired.identityKey == station.identity.publicKey)
        #expect(paired.endpoints.isEmpty)
        #expect(station.pairedDeviceKeys.count == 1)
        #expect(station.pairingConnectionCount == 1)
    }

    /// The number is one no Core shows and no other pairing in the process
    /// holds: a number derived from the fake's code could be the one
    /// ``aCodePairsThroughTheMailbox()`` holds while both run, and the
    /// client's one-pairing-per-number gate would refuse it as `alreadyPairing`.
    @Test func aNumberTheCoreDoesNotShowIsRefusedInTheServicesWords() async throws {
        let station = try FakeStation(requiresPairing: true)
        let parts = station.pairingCode.split(separator: "-")
        let number = MailboxNameplates.unshown()
        let other = "\(number)-\(parts[1])-\(parts[2])"
        await #expect(throws: PairingError.refused(
            reason: "No Core is showing that pairing code right now. Check the code and try again.",
            retryAfter: .zero)) {
            _ = try await client(station).pair(code: other, via: .rendezvous(server: RendezvousServer.defaults,
                                                                           nameplate: number))
        }
        #expect(station.pairedDeviceKeys.isEmpty)
        #expect(station.pairingConnectionCount == 0)
    }

    /// A connection that never opens.
    private struct FailingTransport: LinkTransport {
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            throw LinkTransportError.failed("not here")
        }

        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }
}
