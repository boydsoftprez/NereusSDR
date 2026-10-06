// NereusSDR for iOS: an opt-in check of the app's client against the live remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import Testing
@testable import NereusLink

/// Off unless `NEREUS_LIVE_RENDEZVOUS=1` is set: the default suite never
/// reaches the internet. Run only by the controller or JJ, as
/// `NEREUS_LIVE_RENDEZVOUS=1 ios/scripts/swift-test.sh --filter RendezvousLiveTests`.
///
/// It opens the real `wss://rv.nereussdr.com/` exactly as the phone does
/// (Network framework's WebSocket, `Host: rv.nereussdr.com:443`), takes the
/// hello, and introduces a device key made for this run to a Core id made
/// for this run, which no Core holds, so the service answers `offline` at
/// once. Nothing reaches any Core, nothing is paired, no mailbox is opened
/// (a guessed number could use up a real Core's code), and nothing is kept.
@Suite(.enabled(if: ProcessInfo.processInfo.environment["NEREUS_LIVE_RENDEZVOUS"] == "1"))
struct RendezvousLiveTests {
    @Test func theLiveServiceGreetsAndSaysAnUnknownCoreIsOffline() async throws {
        let client = RendezvousClient()
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let nobody = RendezvousIdentity.stationId(spki: P256.Signing.PrivateKey().publicKey.derRepresentation)
        do {
            _ = try await client.introduce(stationId: nobody, device: device, offer: "v=0\r\n")
            Issue.record("the service answered for a Core that does not exist")
        } catch let error as RendezvousError {
            #expect(error == .offline(
                reason: "The Core is not reachable right now. Check that it is running and connected to the internet."))
        }
        // The IPv4-only STUN name first (the rendezvous document, section 8).
        let stun = await client.stunUrls
        #expect(stun.first?.hasPrefix("stun:rv4.") == true)
        await client.close()
    }
}
