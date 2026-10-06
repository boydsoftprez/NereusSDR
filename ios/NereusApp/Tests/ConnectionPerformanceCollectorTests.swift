// NereusSDR for iOS: diagnostics collector ownership and rate boundaries
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Network
import NereusLink
import NereusMirror
import NereusKitTesting
@testable import NereusSDR
import Testing

@Suite("Connection performance collector")
@MainActor
struct ConnectionPerformanceCollectorTests {
    private actor Gate {
        private var waiting: CheckedContinuation<Void, Never>?
        private(set) var entries = 0
        func hold() async {
            entries += 1
            await withCheckedContinuation { waiting = $0 }
        }
        func release() { waiting?.resume(); waiting = nil }
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    @Test func ratesRequireOneIncreasingLifetimeAndPreserveMeasuredZero() {
        var ledger = DiagnosticsRateLedger()
        #expect(ledger.rate("rx", lifetime: "a", count: 0, at: 1_000, multiplier: 8) == nil)
        #expect(ledger.rate("rx", lifetime: "a", count: 0, at: 2_000, multiplier: 8) == 0)
        #expect(ledger.rate("rx", lifetime: "a", count: 1_000, at: 3_000, multiplier: 8) == 8)
        #expect(ledger.rate("rx", lifetime: "b", count: 2_000, at: 4_000, multiplier: 8) == nil)
        #expect(ledger.rate("rx", lifetime: "b", count: 1_000, at: 5_000, multiplier: 8) == nil)
        #expect(ledger.rate("rx", lifetime: "b", count: 2_000, at: 5_000, multiplier: 8) == nil)
        #expect(ledger.rate("rx", lifetime: "b", count: nil, at: 6_000, multiplier: 8) == nil)
        #expect(ledger.rate("rx", lifetime: "b", count: 2_000, at: 7_000, multiplier: 8) == nil)
    }

    @Test func latestFailureSkipsTryingAndCancelledPaths() {
        let attempt = ConnectionAttempt(tries: [
            .init(path: .thisNetwork, address: "192.0.2.1:4999", outcome: .timedOut),
            .init(path: .direct, address: "198.51.100.2:4999", outcome: .connected),
            .init(path: .relay, address: "relay.example:4999", outcome: .cancelled),
            .init(path: .direct, address: "example.test:4999", outcome: .trying),
        ])
        let selected = ConnectionPerformanceModel.latestFailure(in: attempt, attemptAgeSeconds: 9)
        #expect(selected.current == "no answer in time; attempt started 9 s ago")
        #expect(attempt.summary.contains("stopped after another path connected"))
        #expect(ConnectionPerformanceModel.latestFailure(in: ConnectionAttempt(tries: [
            .init(path: .direct, address: "example.test:4999", outcome: .cancelled),
        ]), attemptAgeSeconds: 9).current == "None")
    }

    /// R-IOS-16 (2026-09-29): a refused or unroutable direct path is a
    /// failure with its reason, and the details say what each end offered
    /// through the internet service.
    @Test func directFailuresKeepTheirReasonAndTheServiceOffersAreShown() {
        let attempt = ConnectionAttempt(tries: [
            .init(path: .direct, address: "[2001:db8::1]:50055", outcome: .refused),
            .init(path: .direct, address: "203.0.113.5:50055", outcome: .unreachable),
            .init(path: .direct, address: "rv.nereussdr.com", outcome: .connected, throughService: true,
                  ice: IceCandidateEvidence(
                    phone: ["candidate:1 1 UDP 2122317823 2001:db8:7700:48::5 61000 typ host"],
                    core: ["candidate:1 1 UDP 2122317823 2001:db8:467f:66e7::15f2 50000 typ host"],
                    chosen: nil, ending: .connected)),
        ])
        #expect(ConnectionPerformanceModel.latestFailure(in: attempt, attemptAgeSeconds: 4).current
                == "no route to it from this network; attempt started 4 s ago")
        #expect(ConnectionPerformanceModel.latestFailure(in: ConnectionAttempt(tries: [attempt.tries[0]]),
                                                         attemptAgeSeconds: 4).current
                == "the connection was refused; attempt started 4 s ago")
        #expect(ConnectionPerformanceModel.serviceOffers(in: attempt).current
                == attempt.tries[2].ice?.summary)
        #expect(ConnectionPerformanceModel.serviceOffers(in: ConnectionAttempt(tries: [attempt.tries[0]])).current
                == nil)
        let titles = Dictionary(uniqueKeysWithValues: ConnectionPerformanceCatalog.details.flatMap(\.rows)
            .map { ($0.id, $0.title) })
        #expect(titles[.serviceOffers] == "Addresses each end offered")
    }

    /// The media ladder's route label (R-IOS-16): media tunnelled through
    /// the Core's secure WebSocket takes the family of the control
    /// connection it rides on, never the loopback hand-off's; a direct pair
    /// shows its own family and UDP. The transport is named once.
    @Test func aTunnelledMediaPathNamesTheControlConnectionsFamily() {
        let role = AppModel().diagnosticsRendezvousRole
        let tunnelPair = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 198.51.100.10 50000 typ host",
            remote: "a=candidate:2 1 UDP 2122317823 127.0.0.1 53000 typ host", carrierClaim: .wssTunnel(port: 53000))
        let overIPv6 = ConnectionPerformanceModel.route(tunnelPair, generation: "0", rendezvousRole: role,
                                                        tunnelCarrierFamily: .ipv6)
        #expect(overIPv6.headline == "Tunnelled through the control connection · IPv6 · secure WebSocket")
        #expect(overIPv6.lines.isEmpty)
        let overIPv4 = ConnectionPerformanceModel.route(tunnelPair, generation: "0", rendezvousRole: role,
                                                        tunnelCarrierFamily: .ipv4)
        #expect(overIPv4.headline == "Tunnelled through the control connection · IPv4 · secure WebSocket")
        let unknown = ConnectionPerformanceModel.route(tunnelPair, generation: "0", rendezvousRole: role)
        #expect(unknown.headline == "Tunnelled through the control connection · secure WebSocket")
        for headline in [overIPv6.headline, overIPv4.headline, unknown.headline] {
            #expect(headline?.components(separatedBy: "WebSocket").count == 2, "the transport is said once")
        }
        #expect(unknown.addressFamily.current == nil)

        let directPair = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 2001:db8::10 50000 typ host",
            remote: "a=candidate:2 1 UDP 2122317823 2001:db8::7 50055 typ host")
        let direct = ConnectionPerformanceModel.route(directPair, generation: "0", rendezvousRole: role,
                                                      tunnelCarrierFamily: .ipv4)
        #expect(direct.headline == "Direct · IPv6 · peer-to-peer link")
        #expect(direct.lines == ["Core address: [2001:db8::7]:50055"])
        for route in [overIPv6, overIPv4, unknown, direct] {
            #expect(!(route.headline ?? "").contains("\u{2014}"))
            #expect(!(route.headline ?? "").contains("yet"))
        }
    }

    @Test func eachRouteSaysWhatIsKnownOnceInPlainWords() {
        let role = AppModel().diagnosticsRendezvousRole
        #expect(role == "Dialled by address")
        let unsupported = ConnectionPerformanceModel.route(.unavailable(.unsupported), generation: "0",
                                                           rendezvousRole: role)
        #expect(unsupported.headline == nil)
        #expect(unsupported.lines == ["This connection does not report its path"])
        let malformed = ConnectionPerformanceModel.route(.unavailable(.malformedSelectedPair),
                                                         generation: "1", rendezvousRole: role)
        #expect(malformed.lines == ["Unavailable: The phone could not read the path this connection chose"])

        let relayedPair = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host",
            remote: "a=candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0")
        let relayed = ConnectionPerformanceModel.route(relayedPair, generation: "0", rendezvousRole: role,
                                                       relayService: "relay.example")
        #expect(relayed.headline == "Relayed · IPv4 · peer-to-peer link")
        #expect(relayed.lines == ["Relay address: 198.51.100.22:61000", "Remote access service: relay.example"])

        // Relayed at this end only: the far end's candidate is the Core's
        // own reflexive address, not a relay's.
        let nearPair = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:4 1 UDP 16777215 198.51.100.23 62000 typ relay raddr 0.0.0.0 rport 0",
            remote: "a=candidate:5 1 UDP 1686052607 198.51.100.7 50055 typ srflx raddr 0.0.0.0 rport 0")
        let near = ConnectionPerformanceModel.route(nearPair, generation: "0", rendezvousRole: role,
                                                    relayService: "relay.example")
        #expect(near.headline == "Relayed · IPv4 · peer-to-peer link")
        #expect(near.lines == ["Core address: 198.51.100.7:50055", "Remote access service: relay.example"])

        let shimPair = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host",
            remote: "a=candidate:2 1 UDP 2122317823 127.0.0.1 53000 typ host", carrierClaim: .webRelay(port: 53000))
        let shim = ConnectionPerformanceModel.route(shimPair, generation: "0", rendezvousRole: role,
                                                    relayService: "relay.example")
        #expect(shim.headline == "Relayed over the web · IPv4 · peer-to-peer link")
        #expect(shim.lines == ["Remote access service: relay.example"])

        let path = NWEndpoint.hostPort(host: NWEndpoint.Host("192.0.2.44"), port: 50055)
        let direct = ConnectionPerformanceModel.route(
            .fromReadyWebSocket(pathEndpoint: path, usesSystemProxy: false), generation: "0",
            rendezvousRole: role, relayService: "relay.example")
        #expect(direct.headline == "Direct · IPv4 · secure WebSocket")
        #expect(direct.lines == ["Core address: 192.0.2.44:50055"])

        let input = ConnectionPerformanceInput(sampledAtMonotonicMilliseconds: 1, displayWallTime: nil,
            status: .current("Connected"), selectedRange: .oneMinute, series: [:], current: [:], details: [:],
            controlRoute: unsupported, mediaRoute: malformed, latestAttempt: .current("Tried relay: connected."),
            latestFailure: ConnectionPerformanceModel.latestFailure(in: ConnectionAttempt(tries: [
                .init(path: .relay, address: "relay.example", outcome: .connected)]), attemptAgeSeconds: 3),
            reset: .enabled)
        #expect(input.copyText.contains("Latest failure: None"))
        for jargon in ["numeric peer", "Nominated", "candidate pair", "Rendezvous introduced",
                       "No recorded failure", "Transport cannot report"] {
            #expect(!input.copyText.contains(jargon), "\(jargon)")
        }
        let titles = Dictionary(uniqueKeysWithValues: ConnectionPerformanceCatalog.details.flatMap(\.rows)
            .map { ($0.id, $0.title) })
        #expect(titles[.controlPeer] == "Control path address")
        #expect(titles[.mediaPeer] == "Media path address")
        #expect(titles[.rendezvous] == "How the path was found")
        #expect(titles[.routeGeneration] == "Connection number")
        let routeSection = input.copyText.components(separatedBy: "\nCurrent chart readings")[0]
        #expect(routeSection.components(separatedBy: "does not report its path").count == 2)
        #expect(routeSection.components(separatedBy: "could not read the path").count == 2)
    }

    /// The page's own reasons and notes say what is missing in plain words,
    /// not in protocol, framework or wire-format names.
    @Test func reasonsUsePlainWords() async throws {
        let app = AppModel()
        let collector = ConnectionPerformanceModel(app: app, playback: nil, nowMilliseconds: { 0 })
        collector.open()
        collector.tickForTesting()
        #expect(await settle { collector.inFlightArmForTesting == nil })
        let shown = PerformanceDetailID.allCases.map { collector.input.detail(for: $0).text { $0 } }
        let charts = ConnectionPerformanceCatalog.charts.flatMap { [$0.title, $0.explanation] + $0.lines.map(\.title) }
        let details = ConnectionPerformanceCatalog.details.flatMap { [$0.title] + $0.rows.flatMap { [$0.title, $0.owner] } }
        let role = app.diagnosticsRendezvousRole
        let udp = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host",
            remote: "a=candidate:2 1 UDP 2122317823 192.0.2.7 50055 typ host")
        let tcp = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 TCP 2122317823 192.0.2.10 50000 typ host tcptype active",
            remote: "a=candidate:2 1 TCP 2122317823 192.0.2.7 50055 typ host tcptype passive")
        let headlines = [udp, tcp].compactMap {
            ConnectionPerformanceModel.route($0, generation: "0", rendezvousRole: role).headline
        }
        #expect(headlines.count == 2)
        for words in shown + charts + details + headlines {
            for term in ["RTP", "UDP", "telemetry v6", "AVAudioEngine", "LAN ", "tx-channel", "Control transport",
                         "logical session", "Downstream", "observed", "RTT", "Core telemetry", "telemetry", "WebRTC",
                         "transport lifetime", "Application payload", "application payload", "payload", "LAN"] {
                #expect(!words.contains(term), "\(term) in \(words)")
            }
        }
        collector.close()
    }

    @Test func rapidReopenAndRangeSelectionDoNotDuplicatePublication() {
        let collector = AppModel().connectionPerformance
        collector.open()
        let initial = collector.publicationCountForTesting
        collector.close()
        collector.open()
        collector.selectRange(.fiveMinutes)
        collector.open()
        #expect(collector.publicationCountForTesting == initial)
    }

    @Test func heldReadExpiresCoreAndRouteRttThenBreaksFreshHistory() async throws {
        let clock = TestLinkClock()
        await clock.advance(by: SystemLinkClock().nowMilliseconds)
        let fake = try FakeStation()
        let app = AppModel(mirrorClock: clock)
        app.connectionPerformance = ConnectionPerformanceModel(app: app, playback: nil,
            nowMilliseconds: { clock.nowMilliseconds })
        let collector = app.connectionPerformance
        await app.connect(to: fake.endpoint, trust: fake.trust, authenticator: fake.authenticator,
                          transportFactory: fake.transportFactory)
        #expect(await settle { app.connection == .connected && collector.inFlightArmForTesting == nil })
        let session = try #require(app.session)
        await session.redialNow()
        for _ in 0..<50_000 {
            if await session.diagnosticsSnapshot().roundTrip != nil { break }
            await Task.yield()
        }
        let currentPong = try #require(await session.diagnosticsSnapshot().roundTrip)
        app.mirror.apply(.capabilities(.init(properties: [
            .init(name: "stationTelemetryVersion", value: .i64(6)),
        ])))
        func sample(_ sequence: Int) -> LinkMessage {
            .stationMetrics(.init(payload: [
                "sequence": .number(Double(sequence)),
                "sampledElapsedMs": .number(Double(sequence * 1_000)),
                "radio": .object(["connected": .bool(true), "rxMbps": .number(Double(sequence)),
                                   "rttMs": .number(12), "rttAgeMs": .number(10),
                                   "adcOverloads": .array([
                                       .object(["adc": .number(0), "eventsSinceConnection": .number(3),
                                                "statusAgeMs": .number(20), "overloaded": .bool(true),
                                                "lastOverloadAgeMs": .number(40)]),
                                       .object(["adc": .number(1), "eventsSinceConnection": .number(0)]),
                                   ])]),
            ]))
        }
        await clock.advance(by: max(0, currentPong.observedAtMilliseconds - clock.nowMilliseconds) + 1_000)
        app.mirror.apply(sample(1))
        let initialCore = try #require(app.mirror.currentTelemetryReceipt)
        #expect(clock.nowMilliseconds - initialCore.observedAtMilliseconds <= 3_000)
        collector.open()
        collector.tickForTesting()
        #expect(await settle { collector.inFlightArmForTesting == nil &&
            collector.historyCountForTesting(.radioRxMbps) == 1 })
        collector.tickForTesting() // Capture the pong if the attach timer read began before it arrived.
        #expect(await settle { collector.inFlightArmForTesting == nil })
        await clock.advance(by: 1_000)
        collector.open()
        #expect(collector.input.reading(for: .radioRxMbps).current != nil)
        #expect(collector.input.reading(for: .sessionRttMs).current != nil)
        #expect(collector.input.detail(for: .playbackState).current != nil)
        #expect(collector.input.detail(for: .delayExplanation).text { $0 }.contains("the Core's capture to the speaker"),
                "the delay is measured from the Core's capture, not from the radio")
        let adc = collector.input.detail(for: .adcOverload).text { $0 }
        #expect(adc.contains("status age 20 ms at Core sample"))
        #expect(adc.contains("last overload age 40 ms at Core sample"))
        #expect(adc.contains("status age unavailable (status age not reported)"))

        let gate = Gate()
        collector.beforePublicationForTesting = { await gate.hold() }
        collector.tickForTesting()
        for _ in 0..<50_000 {
            if await gate.entries > 0 { break }
            await Task.yield()
        }
        #expect(await gate.entries == 1)
        await clock.advance(by: 4_000)
        collector.tickForTesting() // Aging continues while this read is held.
        #expect(collector.inFlightArmForTesting != nil)
        #expect(collector.input.reading(for: .radioRxMbps).text { "\($0)" }.hasPrefix("Stale:"))
        #expect(collector.input.detail(for: .playbackState).text { $0 }.hasPrefix("Stale:"))
        #expect(collector.input.reading(for: .sessionRttMs).current != nil,
                "The independent current-route RTT remains valid before 60 s")
        await clock.advance(by: 57_000)
        collector.tickForTesting()
        #expect(collector.input.reading(for: .sessionRttMs).text { "\($0)" }.hasPrefix("Stale:"))
        collector.close()
        collector.open()
        #expect(collector.input.reading(for: .radioRxMbps).current == nil)
        #expect(collector.input.reading(for: .sessionRttMs).current == nil)

        collector.beforePublicationForTesting = nil
        await gate.release()
        #expect(await settle { collector.inFlightArmForTesting == nil })
        app.mirror.apply(sample(2))
        collector.tickForTesting()
        #expect(await settle { collector.inFlightArmForTesting == nil &&
            collector.historyCountForTesting(.radioRxMbps) == 2 })
        await clock.advance(by: 1_000)
        collector.open()
        #expect(collector.input.series[.radioRxMbps]?.points.last?.breakBefore == true)
        #expect(collector.input.reading(for: .radioRxMbps).current != nil)
        await app.disconnect()
        #expect(collector.input.reading(for: .radioRxMbps).current == nil)
        #expect(collector.input.controlRoute.numericPeer.current == nil)
        #expect(collector.input.status.current == nil)
    }

    @Test func heldOldCollectionCannotPublishIntoNewOrClearItsMarker() async throws {
        let old = try FakeStation()
        let new = try FakeStation()
        let app = AppModel()
        let collector = app.connectionPerformance
        await app.connect(to: old.endpoint, trust: old.trust, authenticator: old.authenticator,
                          transportFactory: old.transportFactory)
        #expect(await settle { app.connection == .connected && collector.inFlightArmForTesting == nil })
        collector.open()
        let gate = Gate()
        collector.beforePublicationForTesting = { await gate.hold() }
        collector.tickForTesting()
        for _ in 0..<50_000 {
            if await gate.entries > 0 { break }
            await Task.yield()
        }
        #expect(await gate.entries == 1)
        let heldMarker = collector.inFlightArmForTesting
        collector.tickForTesting()
        #expect(await gate.entries == 1, "a held read must skip later ticks")
        #expect(collector.inFlightArmForTesting == heldMarker)

        await app.disconnect()
        #expect(collector.input.status.current == nil)
        #expect(collector.input.controlRoute.numericPeer.current == nil)
        #expect(collector.input.mediaRoute.numericPeer.current == nil)
        let newGate = Gate()
        collector.beforePublicationForTesting = { await newGate.hold() }
        await app.connect(to: new.endpoint, trust: new.trust, authenticator: new.authenticator,
                          transportFactory: new.transportFactory)
        #expect(await settle { app.connection == .connected })
        #expect(collector.input.controlRoute.numericPeer.current == nil,
                "a rapid NEW owner must not show OLD's selected peer")
        // The collector's own timer may already have begun NEW's first read.
        collector.tickForTesting()
        for _ in 0..<50_000 {
            if await newGate.entries > 0 { break }
            await Task.yield()
        }
        #expect(await newGate.entries == 1)
        let newMarker = collector.inFlightArmForTesting
        #expect(newMarker != nil && newMarker != heldMarker)
        await gate.release()
        for _ in 0..<1_000 { await Task.yield() }
        #expect(collector.inFlightArmForTesting == newMarker)
        collector.beforePublicationForTesting = nil
        await newGate.release()
        #expect(await settle { collector.inFlightArmForTesting == nil })
        collector.open()
        let newPublication = collector.input.sampledAtMonotonicMilliseconds
        for _ in 0..<1_000 { await Task.yield() }
        #expect(collector.input.sampledAtMonotonicMilliseconds >= newPublication)
        #expect(collector.inFlightArmForTesting == nil)
        await app.disconnect()
    }
}
