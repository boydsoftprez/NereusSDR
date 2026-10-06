// NereusSDR for iOS: focused diagnostics catalogue, state and rendering fixtures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Connection and performance", .serialized)
@MainActor
struct ConnectionPerformanceTests {
    private func history(_ values: [(Int64, UInt64, Double?)], metric: DiagnosticsMetric) -> DiagnosticsHistory {
        var history = DiagnosticsHistory()
        for (time, segment, value) in values {
            history.append(DiagnosticsSample(monotonicMilliseconds: time, sessionSegment: segment,
                                             updated: [metric], values: value.map { [metric: $0] } ?? [:]))
        }
        return history
    }

    private func route(_ peer: DiagnosticsReading<String>) -> DiagnosticsSelectedRoute {
        DiagnosticsSelectedRoute(numericPeer: peer, addressFamily: .current("IPv6 scoped"),
                                 transport: .current("secure WebSocket"), path: .current("direct"),
                                 generation: .current("G12"),
                                 rendezvousRole: .current("Introduced direct route; carries no current traffic"))
    }

    private func fixture(stale: Bool = false) -> ConnectionPerformanceInput {
        let current: DiagnosticsReading<String> = stale
            ? .stale(reason: "Core sample last seen 8 s ago", historical: "Connected")
            : .current("Example fixture · connected; Core sample age 0.8 s")
        let coreValue: (Double) -> DiagnosticsReading<Double> = { value in
            stale ? .stale(reason: "Core sample last seen 8 s ago", historical: value) : .current(value)
        }
        let plot = populatedSeries()
        return ConnectionPerformanceInput(
            sampledAtMonotonicMilliseconds: 60_000, displayWallTime: "Example fixture, 9:41",
            status: current, selectedRange: .oneMinute, series: plot,
            current: [.coreGuiRxKbps: .current(1_200), .coreGuiTxKbps: .current(24),
                      .coreGuiTotalKbps: .current(1_224), .radioRttMs: coreValue(13),
                      .sessionRttMs: .current(42), .audioSendRejectedPerSecond: .current(0),
                      .coreSystemCpuPercent: coreValue(29),
                      .coreProcessCpuPercent: coreValue(18),
                      .coreMemoryAvailableMiB: coreValue(3_120),
                      .coreProcessResidentMiB: coreValue(820),
                      .coreReceiverLoadPercentSlot0: coreValue(34),
                      .coreReceiverLoadPercentSlot1: coreValue(46)],
            details: [.controlState: .current("Connected · direct secure WebSocket"),
                      .radioAddress: .missing(reason: "The Core has not sent the radio's network address"),
                      .sessionUnderruns: .current("0 events · this diagnostics session")],
            controlRoute: route(.current("[2001:db8:42::10%en0]:50055")),
            mediaRoute: DiagnosticsSelectedRoute(
                numericPeer: .current("203.0.113.42:51820"), addressFamily: .current("IPv4"),
                transport: .current("WebRTC"), path: .current("direct"), generation: .current("G12"),
                rendezvousRole: .current("Introduced direct route; carries no current traffic")),
            latestAttempt: .current("RV introduction 18 min ago; direct selected 17 min 58 s ago"),
            latestFailure: .missing(reason: "No failure observed"),
            reset: stale ? .disabled(reason: "Session baseline unavailable") : .enabled)
    }

    /// Example only. The gap and segment switch are deliberate; no app or Core was measured.
    private func populatedSeries() -> [DiagnosticsMetric: DiagnosticsSeries] {
        var result: [DiagnosticsMetric: DiagnosticsSeries] = [:]
        let examples: [DiagnosticsMetric: [Double]] = [
            .coreGuiRxKbps: [900, 1_001, 1_200, 1_150],
            .coreGuiTxKbps: [20, 22, 24, 23],
            .coreGuiTotalKbps: [920, 1_023, 1_224, 1_173],
            .radioRttMs: [12, 13, 13, 14], .sessionRttMs: [38, 40, 42, 41],
            .speakerBufferMs: [32, 34, 34, 33], .playbackPacketAgeMs: [25, 23, 22, 26],
            .audioDelayMs: [160, 166, 168, 165], .audioDeliveryDelayMs: [110, 112, 111, 110],
            .audioDelayAccuracyMs: [8, 8, 8, 8],
            .audioSendRejectedPerSecond: [0, 0, 0, 0],
            .coreSystemCpuPercent: [27, 28, 29, 30],
            .coreProcessCpuPercent: [17, 18, 18, 19],
            .coreMemoryAvailableMiB: [3_100, 3_110, 3_120, 3_115],
            .coreProcessResidentMiB: [810, 815, 820, 817],
            .coreReceiverLoadPercentSlot0: [32, 33, 34, 35],
            .coreReceiverLoadPercentSlot1: [45, 44, 46, 43]
        ]
        for (metric, values) in examples {
            let measured = history([(10_000, 1, values[0]), (11_000, 1, values[1]),
                                    (20_000, 1, nil), (30_000, 1, values[2]),
                                    (31_000, 1, values[2]), (50_000, 2, values[3]),
                                    (51_000, 2, values[3])], metric: metric)
            result[metric] = measured.series(metric, at: 60_000, range: .oneMinute)
        }
        return result
    }

    @Test("all desktop charts and every metric have a visible definition")
    func inventory() {
        let charts = ConnectionPerformanceCatalog.charts
        #expect(charts.count == 15)
        #expect(charts.map(\.section).filter { $0 == .connection }.count == 3)
        #expect(charts.map(\.section).filter { $0 == .roundTrip }.count == 4)
        #expect(charts.map(\.section).filter { $0 == .audio }.count == 4)
        #expect(charts.map(\.section).filter { $0 == .core }.count == 4)
        #expect(charts.flatMap(\.lines).count == 36)
        #expect(Set(charts.flatMap(\.lines).map(\.metric)) == Set(DiagnosticsMetric.allCases))
        #expect(charts.allSatisfy { !$0.explanation.isEmpty && $0.lines.allSatisfy { !$0.title.isEmpty } })
        #expect(charts.last?.reference == 100)
        #expect(ConnectionPerformanceCatalog.details.flatMap(\.rows).count == PerformanceDetailID.allCases.count)
        #expect(ConnectionPerformanceCatalog.details.flatMap(\.rows).map(\.id).count ==
                Set(ConnectionPerformanceCatalog.details.flatMap(\.rows).map(\.id)).count)
    }

    @Test("zero, missing, stale and historical values remain distinct in copy")
    func copyAndAvailability() {
        let input = fixture(stale: true)
        #expect(input.reading(for: .audioSendRejectedPerSecond).text { "\($0)" } == "0.0")
        #expect(input.reading(for: .radioRttMs).text { "\($0)" }.contains("Stale"))
        #expect(input.status.current == nil)
        #expect(input.copyText.contains("Stale: Core sample last seen 8 s ago. Previous: Connected (historical)"))
        #expect(input.copyText.contains("[2001:db8:42::10%en0]:50055"))
        #expect(input.copyText.contains("203.0.113.42:51820"))
        #expect(input.copyText.contains("Radio address on its network, as the Core sees it [Core]: Unavailable"))
        #expect(input.copyText.contains("ADC overload by ADC [Core]: Unavailable"))
        #expect(input.copyText.contains("Reset session stats: Unavailable: Session baseline unavailable"))
        #expect(!input.copyText.contains("60000"))
    }

    @Test("a reading this connection or Core never reports says so in a plain sentence")
    func neverReportedReadsPlainly() {
        let reason = "This Core does not report its computer's processor, memory or temperature"
        let never = DiagnosticsReading<Double>.unsupported(reason: reason)
        #expect(never.text { "\($0)" } == reason)
        #expect(!never.text { "\($0)" }.contains("Unsupported"))
        #expect(never.text { "\($0)" } != DiagnosticsReading<Double>.missing(reason: reason).text { "\($0)" })
    }

    @Test("range units and session gaps use one chart scale")
    func chartMath() {
        let traffic = try! #require(ConnectionPerformanceCatalog.charts.first)
        let memory = try! #require(ConnectionPerformanceCatalog.charts.first { $0.id == "memory" })
        let incoming = history([(1_000, 1, 0), (2_000, 1, 1_001), (3_000, 1, nil),
                                (4_000, 1, 1_200), (5_000, 2, 1_300), (10_000, 2, 1_400)],
                               metric: .coreGuiRxKbps)
        let series = incoming.series(.coreGuiRxKbps, at: 60_000, range: .oneMinute)
        let segments = PerformanceChart.pathSegments(series)
        #expect(segments.map(\.count) == [2, 1, 1, 1])
        #expect(segments[0][0].value == 0)
        #expect(traffic.displayUnit([.coreGuiRxKbps: series]).label == "Mbps")
        #expect(traffic.axisMaximum([.coreGuiRxKbps: series]) == 1_400)
        let short = history([(1_000, 1, 999)], metric: .coreGuiRxKbps)
            .series(.coreGuiRxKbps, at: 60_000, range: .oneMinute)
        #expect(traffic.displayUnit([.coreGuiRxKbps: short]).label == "kbps")
        let mem = history([(1_000, 1, 1_024)], metric: .coreMemoryAvailableMiB)
            .series(.coreMemoryAvailableMiB, at: 60_000, range: .oneMinute)
        #expect(memory.displayUnit([.coreMemoryAvailableMiB: mem]).label == "GiB")
        #expect(DiagnosticsRange.allCases.map(\.label) == ["1 min", "5 min", "15 min", "1 h", "24 h", "7 d"])
    }

    @Test("Focus, Compare and Show all keep the visible selection explicit")
    func seriesSelection() {
        let first = DiagnosticsMetric.radioRxMbps
        let second = DiagnosticsMetric.radioTxMbps
        let focused = PerformanceSeriesSelection.apply(.focus, to: [], metric: first)
        #expect(focused == [first])
        let compared = PerformanceSeriesSelection.apply(.compare, to: focused, metric: second)
        #expect(compared == [first, second])
        #expect(PerformanceSeriesSelection.apply(.compare, to: compared, metric: first) == [second])
        #expect(PerformanceSeriesSelection.apply(.showAll, to: compared, metric: first).isEmpty)
    }

    @Test("populated examples preserve gap, new session, zero and missing receiver")
    func populatedFixture() {
        let input = fixture()
        let traffic = try! #require(input.series[.coreGuiRxKbps])
        #expect(PerformanceChart.pathSegments(traffic).map(\.count) == [2, 2, 2])
        #expect(input.series[.audioSendRejectedPerSecond]?.points.first?.value == 0)
        #expect(input.series[.coreReceiverLoadPercentSlot2] == nil)
        let total = try! #require(ConnectionPerformanceCatalog.charts.first { $0.id == "total" })
        #expect(total.displayUnit(input.series).label == "Mbps")
        let memory = try! #require(ConnectionPerformanceCatalog.charts.first { $0.id == "memory" })
        #expect(memory.displayUnit(input.series).label == "GiB")
    }

    @Test("portrait, landscape, small phone and large type render from labelled fixtures")
    func screenshots() async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        for (name, size, large, stale, section) in [
            ("portrait", CGSize(width: 402, height: 874), false, false, PerformanceSection.connection),
            ("landscape", CGSize(width: 874, height: 402), false, false, .roundTrip),
            ("small", CGSize(width: 320, height: 568), false, false, .core),
            ("large-text-stale", CGSize(width: 402, height: 874), true, true, .details)
        ] {
            let window = UIWindow(windowScene: scene)
            window.frame = CGRect(origin: .zero, size: size)
            window.windowLevel = .alert + 1
            let page = ConnectionPerformanceView(input: fixture(stale: stale), initialSection: section,
                                                 onSelectRange: { _ in },
                                                 onResetSessionStats: {})
                .environment(\.dynamicTypeSize, large ? .accessibility2 : .large)
                .preferredColorScheme(.dark)
            let host = UIHostingController(rootView: page)
            host.view.frame = CGRect(origin: .zero, size: size)
            window.rootViewController = host
            window.isHidden = false
            await ShotWait.laidOut(window)
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            if let directory = ProcessInfo.processInfo.environment["NEREUS_DIAGNOSTICS_SHOTS"],
               let data = image.pngData() {
                try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
            }
            window.isHidden = true
            window.rootViewController = nil
        }
    }

    /// Example only: a relayed session as JJ's build 11 reached the Rock Core,
    /// with documentation addresses. The routes come from the collector's
    /// own route reading of libdatachannel-form candidate lines.
    private func relayedFixture(nearRelay: Bool = false) -> ConnectionPerformanceInput {
        let role = "Found through the remote access service"
        // Far relay: the Core's relay allocation. Near relay: this phone's
        // relay allocation reaching the Core's reflexive address.
        let local = nearRelay
            ? "a=candidate:4 1 UDP 16777215 198.51.100.23 62000 typ relay raddr 0.0.0.0 rport 0"
            : "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host"
        let remote = nearRelay
            ? "a=candidate:5 1 UDP 1686052607 198.51.100.7 50055 typ srflx raddr 0.0.0.0 rport 0"
            : "a=candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0"
        let control = ConnectionPerformanceModel.route(
            .fromSelectedICEPair(local: local, remote: remote), generation: "0", rendezvousRole: role,
            relayService: "rv.nereussdr.com")
        let media = ConnectionPerformanceModel.route(
            .fromSelectedICEPair(local: local, remote: remote), generation: "1 · example", rendezvousRole: role,
            relayService: "rv.nereussdr.com")
        let attempt = ConnectionAttempt(tries: [
            .init(path: .relay, address: "rv.nereussdr.com", outcome: .connected),
            .init(path: .direct, address: "192.0.2.106 port 50055", outcome: .cancelled),
        ])
        return ConnectionPerformanceInput(sampledAtMonotonicMilliseconds: 60_000,
            displayWallTime: "Example fixture, 6:08", status: .current("Connected"),
            selectedRange: .oneMinute, series: populatedSeries(), current: [:], details: [:],
            controlRoute: control, mediaRoute: media, latestAttempt: .current("\(attempt.summary) 49 s ago"),
            latestFailure: ConnectionPerformanceModel.latestFailure(in: attempt, attemptAgeSeconds: 49),
            reset: .enabled)
    }

    @Test("a relayed session's paths read once, in plain words")
    func relayedRouteScreenshot() async throws {
        let far = relayedFixture()
        #expect(far.controlRoute.headline == "Relayed · IPv4 · peer-to-peer link")
        #expect(far.mediaRoute.lines == ["Relay address: 198.51.100.22:61000",
                                         "Remote access service: rv.nereussdr.com"])
        #expect(far.latestFailure.current == "None")
        #expect(far.latestAttempt.current?.contains("stopped after another path connected") == true)
        let near = relayedFixture(nearRelay: true)
        #expect(near.controlRoute.lines == ["Core address: 198.51.100.7:50055",
                                            "Remote access service: rv.nereussdr.com"])
        try await shoot(far, name: "relayed")
        try await shoot(near, name: "relayed-near")
    }

    private func shoot(_ input: ConnectionPerformanceInput, name: String) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let size = CGSize(width: 402, height: 874)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let page = ConnectionPerformanceView(input: input, initialSection: .connection,
                                             onSelectRange: { _ in }, onResetSessionStats: {})
            .environment(\.dynamicTypeSize, .large).preferredColorScheme(.dark)
        let hosting = UIHostingController(rootView: page)
        hosting.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = hosting
        window.isHidden = false
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_DIAGNOSTICS_SHOTS"],
           let data = image.pngData() {
            try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
        }
        window.isHidden = true
        window.rootViewController = nil
    }

    @Test("populated chart cards and controls fit after scrolling at small and large text")
    func chartCardScreenshots() async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let cases: [(String, CGSize, DynamicTypeSize, PerformanceSection, CGFloat)] = [
            ("small-chart-scrolled", CGSize(width: 320, height: 568), .large, .core, 190),
            ("narrow-full-chart", CGSize(width: 320, height: 780), .large, .core, 20),
            ("large-text-chart-scrolled", CGSize(width: 402, height: 874), .accessibility2, .roundTrip, 270),
            ("landscape-chart-scrolled", CGSize(width: 874, height: 402), .large, .roundTrip, 190)
        ]
        for (name, size, dynamicType, section, offset) in cases {
            let window = UIWindow(windowScene: scene)
            window.frame = CGRect(origin: .zero, size: size)
            window.windowLevel = .alert + 1
            let page = ConnectionPerformanceView(input: fixture(), initialSection: section,
                                                 onSelectRange: { _ in }, onResetSessionStats: {})
                .environment(\.dynamicTypeSize, dynamicType).preferredColorScheme(.dark)
            let host = UIHostingController(rootView: page)
            host.view.frame = CGRect(origin: .zero, size: size)
            window.rootViewController = host
            window.isHidden = false
            await ShotWait.laidOut(window)
            let scroll = try #require(findScroll(in: host.view))
            scroll.setContentOffset(CGPoint(x: 0, y: offset), animated: false)
            window.layoutIfNeeded()
            await ShotWait.laidOut(window)
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            if let directory = ProcessInfo.processInfo.environment["NEREUS_DIAGNOSTICS_SHOTS"],
               let data = image.pngData() {
                try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
            }
            window.isHidden = true
            window.rootViewController = nil
        }
    }

    private func findScroll(in view: UIView) -> UIScrollView? {
        if let scroll = view as? UIScrollView { return scroll }
        for child in view.subviews {
            if let found = findScroll(in: child) { return found }
        }
        return nil
    }
}
