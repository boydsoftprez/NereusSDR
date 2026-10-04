// NereusSDR for iOS: controlled CONNECT proof for service WebSockets.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CFNetwork
import Foundation
import Testing
@testable import NereusLink

private struct ServiceProxySettings: @unchecked Sendable { let value: NSDictionary }

extension PACEvaluatingSuites {
    @Suite struct ProxyTransportIntegrationTests {
        @Test func rendezvousAndRelayBothUseConfiguredConnectProxy() async throws {
            let server = try SilentTLSListener(address: "127.0.0.1")
            let serverPort = try await server.start()
            defer { server.stop() }
            let proxy = try LocalConnectProxy(upstreamPort: serverPort, targetHost: "rv.invalid")
            let proxyPort = try await proxy.start()
            defer { proxy.stop() }
            let settings = ServiceProxySettings(value: NSDictionary(dictionary: [
                "HTTPSEnable": 1, "HTTPSProxy": "127.0.0.1", "HTTPSPort": Int(proxyPort),
            ]))
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())

            // The local service has a self-signed test certificate. Reaching it
            // through CONNECT must still fail ordinary system trust.
            let rendezvous = RendezvousWebSocket(
                server: RendezvousServer(host: "rv.invalid", port: serverPort),
                openDeadline: .seconds(2), plainOnLoopback: false, proxyResolver: resolver)
            do {
                _ = try await rendezvous.open { _ in }
                Issue.record("an untrusted rendezvous certificate was accepted")
            } catch let error as LinkTransportError {
                if case .failed(let words) = error { #expect(words.contains("TLS connection failed")) }
                else { Issue.record("unexpected rendezvous refusal: \(error)") }
            }
            #expect(proxy.connectRequests.count == 1)

            let relayURL = URL(string: "wss://rv.invalid:\(serverPort)/v1/relay")!
            do {
                _ = try await NetworkRelaySocket.connect(to: relayURL, resolver: resolver)
                Issue.record("an untrusted relay certificate was accepted")
            } catch let error as RelaySocketError {
                if case .network(let words) = error { #expect(words.contains("TLS connection failed")) }
                else { Issue.record("unexpected relay refusal: \(error)") }
            }
            #expect(proxy.connectRequests.count == 2)
            #expect(proxy.connectRequests.allSatisfy { $0.hasPrefix("CONNECT rv.invalid:\(serverPort) HTTP/") })
            #expect(server.receivedRequests.isEmpty)
        }
    }
}
