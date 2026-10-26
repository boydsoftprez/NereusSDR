// NereusSDR for iOS: identifiers that tests look up on a scrolling page are still found after another suite's early accessibility walk
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// With application accessibility off at launch, once a test has walked a
/// hosted SwiftUI scroll view's accessibility tree before the window's first
/// layout (as the band column's reveal test did), SwiftUI's platform scroll
/// views (HostingScrollView) report no identifier for the rest of the
/// process, while plain accessibility nodes keep theirs. So the identifiers
/// tests look up on a scrolling page sit on the page's content, not on its
/// scroll view, and are found after such a walk.
@Suite("Scroll view identifiers", .serialized)
@MainActor
struct ScrollViewIdentifierTests {
    @Test("the diagnostics page and the iPad Setup list are found after an early walk of a scroll view's tree",
          .timeLimit(.minutes(2)))
    func foundAfterAnEarlyWalk() async throws {
        try await walkBeforeLayout()
        let performance = ConnectionPerformanceView(input: ConnectionPerformanceTests().fixture(),
                                                    initialSection: .connection,
                                                    onSelectRange: { _ in }, onResetSessionStats: {})
        #expect(try await found("connectionPerformance.page", in: performance, size: CGSize(width: 402, height: 874)))
        let split = SetupSplitView(router: SetupRouter(), tree: []) { _ in
            Text("Page")
        } footer: {
            Text("Footer")
        }
        #expect(try await found("setupSplitList", in: split, size: CGSize(width: 1_194, height: 834)))
    }

    /// The band column on a band too short for it, in a window whose
    /// accessibility tree is walked as soon as application accessibility is
    /// on, before the window has laid out: BandButtonColumnTests' walk at
    /// 65777fcf7, the one that left the process in that state.
    private func walkBeforeLayout() async throws {
        let suite = "ScrollViewIdentifierTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 200, width: 400, height: 300)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(ColumnHost(main: model.main).preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: window.frame.size)
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        var attenuator: NSObject?
        for _ in 0..<100 where attenuator == nil {
            attenuator = SetupTypedEntryTests.element("bandColumn.attenuator", in: window)
            if attenuator == nil {
                try? await Task.sleep(for: .milliseconds(50))
            }
        }
        #expect(attenuator != nil)
    }

    /// Whether `id` is under `view`, hosted with application accessibility on,
    /// within five seconds.
    private func found<Content: View>(_ id: String, in view: Content, size: CGSize) async throws -> Bool {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(view.preferredColorScheme(.dark))
        host.view.frame = window.bounds
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let deadline = Date().addingTimeInterval(5)
        while Date() < deadline {
            if SetupTypedEntryTests.element(id, in: window) != nil {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return false
    }
}

/// Pan 1's band column alone on a band 80 points tall, too short for it.
private struct ColumnHost: View {
    let main: MainScreenModel
    @State private var open: BandColumnMenu?
    @State private var folded = false
    @State private var tab: DisplayPillTab = .spectrum

    var body: some View {
        BandButtonColumn(pan: .first, open: $open, folded: $folded, main: main, displayTab: $tab,
                         openSetup: {}, onSetupDefaults: { _, _ in }, onOpenPage: { _ in }, onOpen: { _ in })
            .frame(width: 400, height: 80)
            .coordinateSpace(name: BandColumnLayout.space)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(ChromeColours.bar)
    }
}
