// NereusSDR for iOS: deterministic fixed-field link diagnostic regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct LinkDiagnosticsTests {
    private final class Recorded: @unchecked Sendable {
        private let lock = NSLock()
        private var held: [LinkDiagnostics.Event] = []
        func append(_ event: LinkDiagnostics.Event) { lock.withLock { held.append(event) } }
        var events: [LinkDiagnostics.Event] { lock.withLock { held } }
        var gaps: [LinkDiagnostics.Event] {
            events.filter { if case .keepaliveGap = $0 { return true }; return false }
        }
    }

    @Test func activityBoundaryExcludesIdleButPreservesSuppressedCount() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let first = TransmitHeartbeatGate()
        diagnostics.successfulKeepaliveHandoff(activity: first)
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: first)
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: first)
        first.retire()
        await clock.advance(by: 5000)
        let second = TransmitHeartbeatGate()
        diagnostics.successfulKeepaliveHandoff(activity: second)
        #expect(recorded.gaps == [.keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 0, observedAt: 200)])
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: second)
        #expect(recorded.gaps == [
            .keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 0, observedAt: 200),
            .keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 1, observedAt: 5600),
        ])
    }

    @Test func limiterSurvivesWindowChangeAndUsesLatestScene() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let first = TransmitHeartbeatGate()
        diagnostics.sceneChanged(to: .foreground)
        diagnostics.successfulKeepaliveHandoff(activity: first)
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: first)
        first.retire()
        await clock.advance(by: 100)
        let second = TransmitHeartbeatGate()
        diagnostics.successfulKeepaliveHandoff(activity: second)
        diagnostics.sceneChanged(to: .background)
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: second)
        await clock.advance(by: 700)
        diagnostics.successfulKeepaliveHandoff(activity: second)
        #expect(recorded.gaps == [
            .keepaliveGap(milliseconds: 200, scene: .foreground, suppressed: 0, observedAt: 200),
            .keepaliveGap(milliseconds: 700, scene: .background, suppressed: 1, observedAt: 1200),
        ])
    }

    @Test func exactThresholdsAndRepeatedSceneChangesAreDeterministic() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let activity = TransmitHeartbeatGate()
        diagnostics.sceneChanged(to: .foreground)
        diagnostics.sceneChanged(to: .foreground)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 150)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        #expect(recorded.gaps.isEmpty)
        await clock.advance(by: 151)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 999)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 1)
        diagnostics.successfulKeepaliveHandoff(activity: activity) // <=150 does not consume limiter
        await clock.advance(by: 151)
        diagnostics.sceneChanged(to: .inactive)
        diagnostics.sceneChanged(to: .background)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        #expect(recorded.events == [
            .scene(.foreground, milliseconds: 0),
            .keepaliveGap(milliseconds: 151, scene: .foreground, suppressed: 0, observedAt: 301),
            .scene(.inactive, milliseconds: 1452),
            .scene(.background, milliseconds: 1452),
            .keepaliveGap(milliseconds: 151, scene: .background, suppressed: 1, observedAt: 1452),
        ])
    }

    @Test func rateLimitAllowsExactlyOneSecondAndCarriesSuppression() async {
        let clock = ManualLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        let activity = TransmitHeartbeatGate()
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 200)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 849)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        await clock.advance(by: 151)
        diagnostics.successfulKeepaliveHandoff(activity: activity)
        #expect(recorded.gaps == [
            .keepaliveGap(milliseconds: 200, scene: .inactive, suppressed: 0, observedAt: 200),
            .keepaliveGap(milliseconds: 151, scene: .inactive, suppressed: 1, observedAt: 1200),
        ])
    }

    @Test func routeFormatHasOnlyFixedTokensAndDecimalIntegers() {
        for connection in LinkDiagnostics.Connection.allCases {
            for kind in LinkDiagnostics.RouteKind.allCases {
                for rank in LinkDiagnostics.RouteRank.allCases {
                    let event = LinkDiagnostics.Event.observedRoute(connection: connection, kind: kind,
                                                                   rank: rank, milliseconds: 123)
                    #expect(event.line == "observed-route connection=\(connection.rawValue) kind=\(kind.rawValue) rank=\(rank.rawValue) mono_ms=123")
                    #expect(!event.line.contains("."))
                    #expect(!event.line.contains(":"))
                }
            }
        }
    }

    @Test func independentPrivateIdentitiesDoNotReachRouteEvents() {
        var control = ObservedRouteDiagnosticLedger<Int>()
        var media = ObservedRouteDiagnosticLedger<Int>()
        let firstControl = control.changed(1)
        #expect(firstControl)
        let repeatedControl = control.changed(1)
        #expect(!repeatedControl)
        let firstMedia = media.changed(1)
        #expect(firstMedia)
        let changedControl = control.changed(2)
        #expect(changedControl)
        let repeatedMedia = media.changed(1)
        #expect(!repeatedMedia)
        control.reset()
        let resetControl = control.changed(2)
        #expect(resetControl)
    }
}
