// NereusSDR for iOS: controlled CONNECT proof for service WebSockets.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CFNetwork
import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

private struct ServiceProxySettings: @unchecked Sendable { let value: NSDictionary }

extension PACEvaluatingSuites {
    @Suite struct ProxyTransportIntegrationTests {
        @Test func rendezvousAndRelayBothUseConfiguredConnectProxy() async throws {
            let receipts = TestReceipts()
            defer { print("KIT RECEIPTS proxy\n\(receipts.summary)") }
            let server = try SilentTLSListener(address: "127.0.0.1", observe: receipts.mark)
            let serverPort = try await server.start()
            defer { server.stop() }
            let proxy = try LocalConnectProxy(upstreamPort: serverPort, targetHost: "rv.invalid", observe: receipts.mark)
            let proxyPort = try await proxy.start()
            defer { proxy.stop() }
            let settings = ServiceProxySettings(value: NSDictionary(dictionary: [
                "HTTPSEnable": 1, "HTTPSProxy": "127.0.0.1", "HTTPSPort": Int(proxyPort),
            ]))
            let resolver = SystemProxyResolver(settingsProvider: {
                receipts.mark("resolver settings lookup entered")
                defer { receipts.mark("resolver settings supplied (not resolver completion)") }
                return settings.value
            }, pacEvaluator: CFNetworkPACEvaluator())

            // The local service has a self-signed test certificate. Reaching it
            // through CONNECT must still fail ordinary system trust.
            let rendezvous = RendezvousWebSocket(
                server: RendezvousServer(host: "rv.invalid", port: serverPort),
                openDeadline: .seconds(2), plainOnLoopback: false, proxyResolver: resolver)
            receipts.mark("rendezvous opening entered")
            do {
                _ = try await rendezvous.open { _ in }
                Issue.record("an untrusted rendezvous certificate was accepted")
            } catch let error as LinkTransportError {
                receipts.mark("rendezvous socket settled: \(error)")
                if case .failed(let words) = error { #expect(words.contains("TLS connection failed")) }
                else { Issue.record("unexpected rendezvous refusal: \(error)") }
            }
            #expect(proxy.connectRequests.count == 1)

            receipts.mark("rendezvous observer resumed; CONNECT count \(proxy.connectRequests.count)")
            let relayURL = URL(string: "wss://rv.invalid:\(serverPort)/v1/relay")!
            receipts.mark("relay opening entered")
            do {
                _ = try await NetworkRelaySocket.connect(to: relayURL, resolver: resolver)
                Issue.record("an untrusted relay certificate was accepted")
            } catch let error as RelaySocketError {
                receipts.mark("relay socket settled: \(error)")
                if case .network(let words) = error { #expect(words.contains("TLS connection failed")) }
                else { Issue.record("unexpected relay refusal: \(error)") }
            }
            receipts.mark("relay observer resumed; CONNECT count \(proxy.connectRequests.count)")
            #expect(proxy.connectRequests.count == 2)
            #expect(proxy.connectRequests.allSatisfy { $0.hasPrefix("CONNECT rv.invalid:\(serverPort) HTTP/") })
            #expect(server.receivedRequests.isEmpty)
        }
    }
}
