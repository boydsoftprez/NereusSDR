// NereusSDR for iOS: the real transport reports the Core's certificate on every connection
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import CFNetwork
import LinkSessionTestSupport
import Testing
@testable import NereusLink

private struct LocalProxySettings: @unchecked Sendable { let value: NSDictionary }

private final class BoolLatch: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: Bool?
    var value: Bool? { lock.withLock { stored } }
    func record(_ value: Bool) { lock.withLock { stored = value } }
}

private struct SlowPAC: SystemProxyPACEvaluating {
    func evaluate(_ source: SystemProxyPACSource, for target: URL, timeout: Duration) async throws -> SystemProxyPACEntries {
        // Deliberately deliver a result after cancellation, as a native PAC
        // callback can do once its script has begun running.
        try? await Task.sleep(for: .milliseconds(100))
        return SystemProxyPACEntries(entries: [[kCFProxyTypeKey as NSString: kCFProxyTypeNone] as NSDictionary])
    }
}

/// A resumed TLS session presents no certificate, so a reconnect could
/// report no digest and be taken for another Core. Every connection must
/// present and report the certificate, whatever the trust.
extension PACEvaluatingSuites {
    @Suite struct WebSocketLinkTransportTests {
        @Test func queuedMediaDroppedBeforeWebSocketUpgradeDoesNotCount() async throws {
            let listener = try SilentTLSListener(address: "127.0.0.1")
            let port = try await listener.start()
            defer { listener.stop() }
            let counter = TrafficCounter()
            let transport = WebSocketLinkTransport(
                endpoint: StationEndpoint(host: "127.0.0.1", port: port), trust: .pairing,
                openDeadline: .seconds(3), proxyResolver: SystemProxyResolver(), traffic: counter)
            let opening = Task { try? await transport.open { _ in } }
            let deadline = ContinuousClock.now + .seconds(2)
            while listener.receivedRequests.isEmpty && ContinuousClock.now < deadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            try #require(!listener.receivedRequests.isEmpty)
            let frame = Data([2] + Array(repeating: UInt8(7), count: 255))
            for _ in 0..<100 { _ = transport.sendBinary(frame) }
            transport.send("queued text")
            transport.close()
            _ = await opening.value
            #expect(counter.reading.bytesOut == 0)
        }

        @Test func rejectedAndClosedSendsDoNotCountAsTransferredPayload() async throws {
            let counter = TrafficCounter()
            let station = try LoopbackStation()
            let endpoint = try await station.start()
            defer { station.stop() }
            let transport = WebSocketLinkTransport(endpoint: endpoint, trust: .pairing,
                                                   openDeadline: .seconds(3), proxyResolver: SystemProxyResolver(),
                                                   traffic: counter)
            let frame = Data([2] + Array(repeating: UInt8(7), count: 17))
            #expect(!transport.send("before open"))
            #expect(!transport.sendBinary(frame))
            #expect(counter.reading.bytesOut == 0)
            _ = try await transport.open { _ in }
            #expect(transport.send("count this text"))
            let deadline = ContinuousClock.now + .seconds(2)
            while !station.receivedTexts.contains("count this text") && ContinuousClock.now < deadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            #expect(station.receivedTexts.contains("count this text"))
            while counter.reading.bytesOut < UInt64("count this text".utf8.count) && ContinuousClock.now < deadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            let sent = counter.reading.bytesOut
            #expect(sent == UInt64("count this text".utf8.count))
            #expect(transport.sendBinary(frame))
            let binaryDeadline = ContinuousClock.now + .seconds(2)
            while station.receivedBinaryFrames == 0 && ContinuousClock.now < binaryDeadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            #expect(station.receivedBinaryFrames == 1)
            // Tag 2 is encrypted media carried over WSS. RtcBridge counts its
            // inner app payload; counting the carrier here would count it twice.
            #expect(counter.reading.bytesOut == sent)
            let incoming = BoolLatch()
            transport.setBinaryReceiver { _ in incoming.record(true) }
            let beforeIncoming = counter.reading.bytesIn
            station.sendBinary(frame)
            let incomingDeadline = ContinuousClock.now + .seconds(2)
            while incoming.value == nil && ContinuousClock.now < incomingDeadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            #expect(incoming.value == true)
            #expect(counter.reading.bytesIn == beforeIncoming)
            transport.close()
            #expect(!transport.send("after close"))
            #expect(!transport.sendBinary(frame))
            #expect(counter.reading.bytesOut == sent)
        }

        @Test func trafficCountsTextAfterProcessingAndNeverBinaryOrPing() async throws {
            final class BinaryCount: @unchecked Sendable {
                private let lock = NSLock()
                private var value = 0
                func received() { lock.withLock { value += 1 } }
                var count: Int { lock.withLock { value } }
            }
            let station = try LoopbackStation()
            let endpoint = try await station.start()
            defer { station.stop() }
            let counter = TrafficCounter()
            let transport = WebSocketLinkTransport(endpoint: endpoint, trust: .pairing,
                                                   openDeadline: .seconds(3), proxyResolver: SystemProxyResolver(),
                                                   traffic: counter)
            let binary = BinaryCount()
            transport.setBinaryReceiver { _ in binary.received() }
            let events = EventLog()
            _ = try await transport.open { events.append($0) }
            #expect(await events.waitFor(count: 1))
            let initial = try #require(transport.trafficObservation)
            #expect(initial.active)
            #expect(initial.receivedPayloadBytes > 0)
            #expect(initial.acceptedPayloadBytes == 0)
            #expect(counter.reading.bytesIn == initial.receivedPayloadBytes)
            let outbound = "é日本語"
            transport.send(outbound)
            let clock = ContinuousClock()
            let deadline = clock.now + .seconds(2)
            while (transport.trafficObservation?.acceptedPayloadBytes != UInt64(outbound.utf8.count)
                   || !station.receivedTexts.contains(outbound))
                    && clock.now < deadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            #expect(station.receivedTexts.contains(outbound))
            #expect(transport.trafficObservation?.acceptedPayloadBytes == UInt64(outbound.utf8.count))
            #expect(counter.reading.bytesOut == UInt64(outbound.utf8.count))
            station.sendBinary(Data([2] + Array(repeating: 0, count: 17)))
            transport.ping()
            let carrierDeadline = clock.now + .seconds(2)
            while (binary.count == 0 || station.pingCount == 0) && clock.now < carrierDeadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            #expect(binary.count == 1 && station.pingCount == 1)
            #expect(transport.trafficObservation?.receivedPayloadBytes == initial.receivedPayloadBytes)
            #expect(transport.trafficObservation?.acceptedPayloadBytes == UInt64(outbound.utf8.count))
            #expect(counter.reading.bytesIn == initial.receivedPayloadBytes)
            #expect(counter.reading.bytesOut == UInt64(outbound.utf8.count))
            transport.close()
            #expect(transport.trafficObservation?.lifetime == initial.lifetime)
            #expect(transport.trafficObservation?.active == false)
            transport.send("closed")
            #expect(transport.trafficObservation?.acceptedPayloadBytes == UInt64(outbound.utf8.count))
        }

        @Test func closeDuringPACCannotStartLateConnection() async throws {
            let core = try SilentTLSListener(address: "127.0.0.1")
            let corePort = try await core.start()
            defer { core.stop() }
            let settings = LocalProxySettings(value: NSDictionary(dictionary: [
                "ProxyAutoConfigEnable": 1,
                "ProxyAutoConfigJavaScript": "function FindProxyForURL(url, host) { return 'DIRECT'; }",
            ]))
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: SlowPAC())
            let transport = WebSocketLinkTransport(
                endpoint: StationEndpoint(host: "core.invalid", port: corePort), trust: .pairing,
                openDeadline: .seconds(1), proxyResolver: resolver)
            let task = Task { try await transport.open { _ in } }
            try await Task.sleep(for: .milliseconds(20))
            transport.close()
            await #expect(throws: LinkTransportError.failed("closed")) { _ = try await task.value }
            #expect(core.receivedRequests.isEmpty)
        }

        @Test func configuredConnectProxyCarriesTheCoreWebSocket() async throws {
            let core = try SilentTLSListener(address: "127.0.0.1")
            let corePort = try await core.start()
            defer { core.stop() }
            let proxy = try LocalConnectProxy(upstreamPort: corePort)
            let proxyPort = try await proxy.start()
            defer { proxy.stop() }
            let settings = LocalProxySettings(value: NSDictionary(dictionary: [
                "HTTPSEnable": 1, "HTTPSProxy": "127.0.0.1", "HTTPSPort": Int(proxyPort),
            ]))
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            let transport = WebSocketLinkTransport(
                endpoint: StationEndpoint(host: "core.invalid", port: corePort), trust: .pairing,
                openDeadline: .seconds(3), proxyResolver: resolver)
            let task = Task { try await transport.open { _ in } }
            let clock = ContinuousClock()
            let deadline = clock.now + .seconds(2)
            while core.receivedRequests.isEmpty && clock.now < deadline {
                try await Task.sleep(for: .milliseconds(20))
            }
            transport.close()
            _ = try? await task.value
            #expect(proxy.connectRequests.count == 1)
            #expect(proxy.connectRequests.first?.hasPrefix("CONNECT core.invalid:\(corePort) HTTP/") == true)
            #expect(core.receivedRequests.first?.contains("Host: core.invalid:\(corePort)") == true)
        }

        @Test func proxyCannotHideWrongCoreCertificate() async throws {
            let core = try LoopbackStation()
            let localEndpoint = try await core.start()
            defer { core.stop() }
            let proxy = try LocalConnectProxy(upstreamPort: localEndpoint.port)
            let proxyPort = try await proxy.start()
            defer { proxy.stop() }
            let settings = LocalProxySettings(value: NSDictionary(dictionary: [
                "HTTPSEnable": 1, "HTTPSProxy": "127.0.0.1", "HTTPSPort": Int(proxyPort),
            ]))
            let resolver = SystemProxyResolver(settingsProvider: { settings.value }, pacEvaluator: CFNetworkPACEvaluator())
            let wrongPin = Data(repeating: 0, count: 32)
            #expect(wrongPin != core.pin)
            let transport = WebSocketLinkTransport(
                endpoint: StationEndpoint(host: "core.invalid", port: localEndpoint.port),
                trust: .certificate(pinSHA256: wrongPin), openDeadline: .seconds(3), proxyResolver: resolver)
            await #expect(throws: LinkTransportError.certificateMismatch) {
                _ = try await transport.open { _ in }
            }
            #expect(proxy.connectRequests.count == 1)
        }

        @Test(arguments: ["certificate", "identity", "pairing"])
        func twoConnectionsInARowEachReportTheCertificate(trustName: String) async throws {
            let station = try LoopbackStation()
            let endpoint = try await station.start()
            defer { station.stop() }
            let trust: StationTrust
            switch trustName {
            case "certificate": trust = .certificate(pinSHA256: station.pin)
            case "identity": trust = TestStationIdentity().trust
            default: trust = .pairing
            }
            for attempt in 1...3 {
                let transport = WebSocketLinkTransport(endpoint: endpoint, trust: trust)
                let digest = try await transport.open { _ in }
                #expect(digest == station.pin, "connection \(attempt) under \(trustName)")
                transport.close()
            }
        }
    }
}
