// NereusSDR for iOS: the app pairs with a real nereusd by its code over TLS, then signs in with its key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process exists only on macOS; on the iOS simulator this
// test is not built.
#if os(macOS)

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-08, R-IOS-16: the whole path against a real Core, `nereusd`, with a
/// scratch profile and config (``NereusdProcess``): the code read from
/// `nereusd pairing show`, pairing over the real TLS WebSocket, then a new
/// connection that signs in with the device key, the Core checking the
/// signature, and a key it has not paired refused. This is the
/// cross-implementation proof that the Core accepts the app's device
/// signature and the app accepts the Core's certificate binding. Run by
/// `ios/scripts/interop-test.sh`, which builds `nereusd` and sets
/// `NEREUS_NEREUSD`; without it the test is skipped.
@Suite(.serialized, .enabled(if: NereusdProcess.path != nil, "set NEREUS_NEREUSD (ios/scripts/interop-test.sh)"))
struct CorePairingSignInTests {
    /// Signs in by `device`'s key to `paired` and returns the Core's answer,
    /// or the refusal that ended the session first.
    private static func signIn(_ device: DeviceIdentity, to paired: PairedStation,
                               at endpoint: StationEndpoint) async throws
        -> (result: LinkMessage.AuthResult?, refusal: Refusal?) {
        let session = StationSession(endpoint: endpoint, trust: paired.trust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Interop iPhone",
                                                                               kind: .phone))
        let answer = Task { () -> (LinkMessage.AuthResult?, Refusal?) in
            var result: LinkMessage.AuthResult?
            for await event in session.events {
                switch event {
                case .message(.authResult(let received)):
                    result = received
                    if received.accepted {
                        return (received, nil)
                    }
                case .refused(let refusal):
                    return (result, refusal)
                default:
                    continue
                }
            }
            return (result, nil)
        }
        await session.connect()
        // Above the session's own 30 s connect deadline, as the pairing's is.
        let timeout = Task {
            try await Task.sleep(for: backstop)
            await session.disconnect()
        }
        let outcome = await answer.value
        timeout.cancel()
        await session.disconnect()
        return outcome
    }

    /// Over IPv4 loopback, and over IPv6 loopback reached as `[::1]`: the
    /// Core answers an IPv6 opening only when its Host header names it in
    /// brackets, so this is the proof the app's opening does.
    /// Longer than the product's own connect deadline
    /// (``PairingClient/connectDeadline``, 30 s), so it only ever catches a
    /// pairing that ignores that deadline, never one still inside it.
    static let backstop: Duration = .seconds(45)

    @Test(arguments: ["127.0.0.1", "::1"])
    func pairsByCodeOverTLSThenSignsInByKey(host: String) async throws {
        let core = try NereusdProcess(host: host)
        defer { core.stop() }
        let started = ContinuousClock.now
        /// Records `what` failed, when, and what the Core wrote, before the
        /// scratch directory goes.
        func evidence(_ what: String) -> Comment {
            Comment(rawValue: "\(what) after \(ContinuousClock.now - started) on \(host) port \(core.endpoint.port);"
                    + " nereusd's output:\n\(core.log)")
        }
        let code: String
        do {
            code = try await core.pairingCode()
            // The console answering is not proof the Core's port takes a connection.
            try await core.waitUntilListening()
        } catch {
            Issue.record(error, evidence("the Core was not ready"))
            return
        }
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let client = try PairingClient(identity: device, name: "Interop iPhone", kind: .phone)
        let dialled = ContinuousClock.now
        let pairing = Task { try await client.pair(code: code, via: .direct(core.endpoint)) }
        let backstop = Task {
            try await Task.sleep(for: Self.backstop)
            pairing.cancel()
        }
        defer { backstop.cancel() }
        let paired: PairedStation
        do {
            paired = try await pairing.value
        } catch {
            Issue.record(error, evidence("pairing failed \(ContinuousClock.now - dialled) after the dial, "
                                         + "\(error)"))
            return
        }
        #expect(paired.endpoints == [core.endpoint])
        #expect(P256Wire.isCanonicalKey(paired.identityKey))

        let (status, text) = try await core.console(["status"])
        #expect(status == 0)
        #expect(text.contains("Paired devices: 1"))

        // A new connection, trusting the Core by the key it paired with.
        let signedIn = try await Self.signIn(device, to: paired, at: core.endpoint)
        #expect(signedIn.result?.accepted == true, evidence("signing in by key"))
        #expect(signedIn.refusal == nil, evidence("signing in by key"))

        // A key the Core has not paired proves itself but is not admitted.
        let stranger = try DeviceIdentity.load(store: InMemoryKeyStore())
        let refused = try await Self.signIn(stranger, to: paired, at: core.endpoint)
        #expect(refused.result?.accepted == false, evidence("a stranger signing in"))
        #expect(refused.refusal?.code == .deviceNotPaired, evidence("a stranger signing in"))
    }
}

#endif
