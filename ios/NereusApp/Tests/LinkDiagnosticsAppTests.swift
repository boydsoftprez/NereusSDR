// NereusSDR for iOS: scene mapping and default phone-log diagnostics visibility
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
import SwiftUI
import Testing
@testable import NereusSDR

@Suite("App link diagnostics", .serialized)
@MainActor
struct LinkDiagnosticsAppTests {
    private final class Recorded: @unchecked Sendable {
        private let lock = NSLock()
        private var values: [LinkDiagnostics.Event] = []
        func append(_ event: LinkDiagnostics.Event) { lock.withLock { values.append(event) } }
        var events: [LinkDiagnostics.Event] { lock.withLock { values } }
    }

    @Test func liveSceneDispatcherMapsAllThreePhases() async {
        let clock = TestLinkClock()
        let recorded = Recorded()
        let diagnostics = LinkDiagnostics(clock: clock, sink: recorded.append)
        AppSceneDiagnostics.changed(.active, diagnostics: diagnostics)
        await clock.advance(by: 1)
        AppSceneDiagnostics.changed(.inactive, diagnostics: diagnostics)
        await clock.advance(by: 1)
        AppSceneDiagnostics.changed(.background, diagnostics: diagnostics)
        #expect(recorded.events == [
            .scene(.foreground, milliseconds: 0),
            .scene(.inactive, milliseconds: 1),
            .scene(.background, milliseconds: 2),
        ])
    }

    @Test func defaultSinkIsReadByPhoneLogAndOfflineSupportBundle() async throws {
        let clock = TestLinkClock()
        await clock.advance(by: 918_273_645)
        let diagnostics = LinkDiagnostics(clock: clock)
        diagnostics.sceneChanged(to: .foreground)
        let marker = LinkDiagnostics.Event.scene(.foreground, milliseconds: 918_273_645).line
        // Assertions expose counts/booleans only, never unrelated phone-log contents.
        var found = false
        for _ in 0..<100 {
            found = try PhoneLog.system.read().contains { $0.contains(marker) }
            if found { break }
            await Task.yield()
        }
        #expect(found, "The default diagnostics sink must be visible to PhoneLog.system")
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("linklog-support-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: directory) }
        let mirror = MirrorStore(send: { _ in }, clock: clock)
        let bundle = SupportBundleModel(mirror: mirror, commands: nil, directory: directory)
        await bundle.collect()
        #expect(!bundle.phoneLogFailed)
        let file = try #require(bundle.files.first)
        let containsMarker = try String(contentsOf: file, encoding: .utf8).contains(marker)
        #expect(containsMarker, "The default offline support bundle must include the diagnostics event")
    }
}
