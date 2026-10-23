// NereusSDR for iOS: Setup in the workspace panels: pictures at default and accessibility sizes, and where a value row lays out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-51 (W6, W7, W9; spec sections 7.1, 7.2, 7.4): Setup's pages drawn
/// in the workspace panels on the phone, at the default text size and at
/// accessibility extra large: Navigation, DSP's AGC/ALC page the Core
/// describes, and a described page whose rows are greyed with their
/// reasons. At the accessibility sizes a number row's value field goes
/// under its title; at the default size it sits beside it. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the pages are written there as PNGs.
@Suite("Setup in the workspace panels", .serialized)
@MainActor
struct SetupWorkspaceShotTests {
    typealias Described = SetupDescribedPagesTests

    static let navigation: [SetupTree.Route] = [.category("General"), .page(.navigation)]
    static let agcAlc: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.agcAlc")]
    /// This phone's peak settings, greyed by the fake Core's display extras with their values shown.
    static let peaks: [SetupTree.Route] = [.category("Display"),
                                           .described(category: "display", page: "display.spectrumPeaks")]
    static let multimeter: [SetupTree.Route] = [.category("Display"),
                                                .described(category: "display", page: "display.multimeter")]
    static let sizes: [(name: String, size: DynamicTypeSize)] = [("default", .large),
                                                                  ("axl", .accessibility3)]

    @Test("Navigation, AGC/ALC and a greyed described page, at default and accessibility extra large")
    func pictures() async throws {
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Described.coreCategories())
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "dsp") != nil })
        let numbers = try Self.numberRowIds(app, category: "dsp", page: "dsp.agcAlc")
        #expect(!numbers.isEmpty, "AGC/ALC has no number row to lay out")
        let peakNumbers = try Self.numberRowIds(app, category: "display", page: "display.spectrumPeaks")
        #expect(!peakNumbers.isEmpty, "Spectrum Peaks has no number row to lay out")

        for (name, size) in Self.sizes {
            _ = try await Self.shoot("setup-ws-navigation-\(name)", rig: rig, path: Self.navigation, dynamicType: size)
            let probe = try await Self.shoot("setup-ws-agc-\(name)", rig: rig, path: Self.agcAlc,
                                             dynamicType: size)
            Self.expectLayout(probe, ids: numbers, stacked: size.isAccessibilitySize)
            // Greyed number rows, each with its reason under it.
            let peaks = try await Self.shoot("setup-ws-peaks-\(name)", rig: rig, path: Self.peaks, dynamicType: size)
            Self.expectLayout(peaks, ids: peakNumbers, stacked: size.isAccessibilitySize)
        }

        // Away from the Core every row of a described page greys, with its reason under it.
        await app.disconnect()
        #expect(await Described.settle { !app.setupPages.isCurrent })
        for (name, size) in Self.sizes {
            _ = try await Self.shoot("setup-ws-greyed-\(name)", rig: rig, path: Self.multimeter, dynamicType: size)
        }
    }

    @Test("A value row's layout follows the text size: under its title only at the accessibility sizes")
    func valueRowStacksAtAccessibilitySizes() {
        for size in DynamicTypeSize.allCases {
            #expect(SetupValueRowLayout<EmptyView, EmptyView>.stacks(size) == size.isAccessibilitySize)
        }
    }

    /// I2 of the final review, and M-a and M-b of its re-review: a Setup
    /// slider is UIKit's slider in the workspace look, so the value moves
    /// only while its thumb is dragged, a tap on the track changes nothing, a
    /// vertical drag scrolls the page and VoiceOver adjusts it, all as the
    /// system slider does. Through the fake Core (the request seam):
    /// accessibility activation (VoiceOver's double tap, not a finger tap)
    /// sends no request, and one VoiceOver adjust sends exactly one. A finger
    /// tap is UISlider's own touch handling, which no hosted test can send.
    @Test("A Setup slider: accessibility activation sends no request, one VoiceOver adjust sends exactly one")
    func sliderSendsOnlyWhenMoved() async throws {
        let rig = try await Self.sliderRig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        try await Self.hosted(rig: rig, path: Self.sliderRoute) { window in
            // The UISlider is its own accessibility element, the one VoiceOver focuses.
            let slider = try #require(await Self.firstSlider(in: window, under: Self.sliderId),
                                      "the page draws no workspace slider")
            #expect(slider.isAccessibilityElement && slider.accessibilityTraits.contains(.adjustable),
                    "the workspace slider is not an adjustable accessibility element")
            // N3: as the stock Slider at 8937553ad, the slider carries the
            // row's identifier and no label of its own (the title is the
            // element above it), and reads its value as UISlider's percentage.
            #expect(slider.accessibilityIdentifier == Self.sliderId)
            #expect(slider.accessibilityLabel == nil)
            let before = slider.accessibilityValue

            // Accessibility activation gives it no value and sends nothing.
            _ = slider.accessibilityActivate()
            await ShotWait.laidOut(window)
            #expect(await rig.station.waitForMessage(within: .seconds(2), matching: Self.isSliderWrite) == nil,
                    "accessibility activation of the slider sent a request")
            #expect(slider.accessibilityValue == before, "accessibility activation of the slider moved it")

            // VoiceOver's adjust moves it one step and sends that value, once:
            // up, or down when it is already at the top.
            slider.accessibilityIncrement()
            if await rig.station.waitForMessage(within: .seconds(2), matching: Self.isSliderWrite) == nil {
                slider.accessibilityDecrement()
            }
            try await Self.expectOneWrite(rig, "a VoiceOver adjust of the slider")
        }
    }

    /// M-a of the re-review: an edit (the thumb taken, moved through several
    /// steps and let go) sends exactly one request, with the value it was let
    /// go at, as DescribedControl sends on the edit's end.
    @Test("A Setup slider: one edit through several steps sends exactly one request")
    func sliderEditSendsOneRequest() async throws {
        let rig = try await Self.sliderRig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        try await Self.hosted(rig: rig, path: Self.sliderRoute) { window in
            let slider = try #require(await Self.firstSlider(in: window, under: Self.sliderId),
                                      "the page draws no workspace slider")
            let start = Double(slider.value)
            let step = slider.step > 0 ? slider.step : 1
            let direction: Double = Double(slider.maximumValue) - start >= 3 * step ? 1 : -1
            slider.editingBegan()
            for moved in 1...3 {
                slider.value = Float(start + direction * Double(moved) * step)
                slider.valueMoved()
                await ShotWait.laidOut(window)
            }
            #expect(await rig.station.waitForMessage(within: .seconds(1), matching: Self.isSliderWrite) == nil,
                    "the slider sent a request before its edit ended")
            slider.editingEnded()
            let sent = try await Self.expectOneWrite(rig, "an edit of the slider")
            guard case .propertyWrite(let write) = sent,
                  let property = write.properties.first(where: { $0.name == "power" }) else {
                Issue.record("the slider's write carries no power")
                return
            }
            let letGo = start + direction * 3 * step
            let number: Double? = switch property.value {
            case .i64(let whole): Double(whole)
            case .f64(let decimal): decimal
            default: nil
            }
            #expect(number.map { abs($0 - letGo) < 1e-9 } == true,
                    "the slider sent \(property.value), not \(letGo), the value it was let go at")
        }
        // A picture of the slider in the workspace look (spec section 7.1).
        _ = try await Self.shoot("setup-ws-slider", rig: rig, path: Self.sliderRoute, dynamicType: .large)
    }

    /// N3 of the second re-review: the Core's display slider (Display on
    /// this phone) is a UIViewRepresentable now, so its name, identifier
    /// and value are set on the UISlider itself, as the stock Slider carried
    /// them at 8937553ad: the control's label, the row's "core.<key>", and
    /// the row's value text (the row's accessibilityValue reached the slider).
    @Test("The Core's display slider names itself and carries its row's identifier on the UISlider")
    func coreDisplaySliderAccessibility() async throws {
        let rig = try DisplaySheetTests.Rig()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let core = rig.main.coreDisplay
        #expect(await Described.settle { !core.groups.isEmpty })
        let control = try #require(core.groups.flatMap(\.controls).first { control in
            guard case .slider = control.kind, control.options.isEmpty, let range = core.range(control) else {
                return false
            }
            return (range.max - range.min) / range.step > 50
        }, "the fake Core's display catalogue has no slider with many steps")
        try await Self.hostedView(CoreControlRow(core: core, control: control).padding()) { window in
            let slider = try #require(Self.views(in: window).compactMap { $0 as? SetupUISlider }.first,
                                      "the row draws no workspace slider")
            #expect(slider.accessibilityLabel == control.label)
            #expect(slider.accessibilityIdentifier == "core.\(control.settingsKey ?? control.subscribe)")
            #expect(slider.accessibilityValue == (core.value(control).map(control.text) ?? ""))
        }
    }

    /// `view` in an upright phone's window, under the app's presentation
    /// root, for `body` to act on.
    static func hostedView<Content: View>(_ view: Content, _ body: (UIWindow) async throws -> Void) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(view.preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await body(window)
    }

    @Test("A Setup slider snaps to its step and stays inside its range")
    func sliderSnapsToItsStep() {
        #expect(SetupUISlider.snapped(4.4, range: 0...10, step: 1) == 4)
        #expect(SetupUISlider.snapped(4.6, range: 0...10, step: 1) == 5)
        #expect(SetupUISlider.snapped(12, range: 0...10, step: 1) == 10)
        #expect(SetupUISlider.snapped(-3, range: 0...10, step: 0) == 0)
        #expect(SetupUISlider.snapped(3.3, range: 0...10, step: 0) == 3.3)
        #expect(SetupUISlider.snapped(-14, range: -20...20, step: 5) == -15)
    }

    /// A page with one described slider, the transmit power (kind slider,
    /// 0 to 100 in steps of 1), which the fake Core's mirror holds and lets
    /// the phone edit with no slice chosen.
    static let sliderId = "general.sliders.power"
    static let sliderRoute: [SetupTree.Route] = [.category("General"),
                                                 .described(category: "general", page: "general.sliders")]
    static let sliderPage = #"{"version":3,"category":{"id":"general","title":"General","where":"mixed"},"pages":[{"id":"general.sliders","title":"Sliders","where":"station","sections":[{"title":"Sample","controls":[{"id":"general.sliders.power","label":"Sample power","tooltip":"","kind":"slider","binding":{"property":{"object":"transmit","name":"power"}},"applies":"live","min":0,"max":100,"step":1,"unit":"W"}]}]}]}"#

    /// A connected phone whose General category is ``sliderPage``.
    static func sliderRig() async throws -> Described.Rig {
        let rig = try await Described.connected()
        var categories = try Described.coreCategories()
        categories.removeAll { $0.id == "general" }
        categories.insert(("general", sliderPage), at: 0)
        await rig.station.deliverSetup(categories)
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "general") != nil })
        return rig
    }

    /// A write of the slider's property, the transmit power.
    nonisolated static let isSliderWrite: @Sendable (LinkMessage) -> Bool = { message in
        guard case .propertyWrite(let write) = message else { return false }
        return write.key == "transmit" && write.properties.contains { $0.name == "power" }
    }

    /// Waits for one transmit power write, then expects no second one; returns the write.
    @discardableResult
    static func expectOneWrite(_ rig: Described.Rig, _ what: String) async throws -> LinkMessage {
        let sent = try #require(await rig.station.waitForMessage(within: .seconds(10), matching: isSliderWrite),
                                "\(what) sent no request")
        let station = rig.station
        let isWrite = isSliderWrite
        _ = await station.waitUntil(within: .seconds(2)) {
            station.messages.filter(isWrite).count > 1
        }
        #expect(rig.station.messages.filter(isSliderWrite).count == 1, "\(what) sent more than one request")
        return sent
    }

    /// The workspace UISlider of the described row with `id`: the slider
    /// under the row's title, scrolled into the window first, since a list
    /// builds only the rows on screen. (The row's identifier lands on its
    /// title; the UISlider is an accessibility element of its own.)
    static func firstSlider(in window: UIWindow, under id: String) async -> SetupUISlider? {
        guard let title = SetupTypedEntryTests.element(id, in: window) else {
            return nil
        }
        let frame = title.accessibilityFrame
        if frame.maxY > window.bounds.maxY - 160 || frame.minY < 120,
           let scroll = views(in: window).compactMap({ $0 as? UIScrollView })
               .max(by: { $0.contentSize.height < $1.contentSize.height }) {
            let target = scroll.contentOffset.y + frame.midY - window.bounds.midY
            let limit = max(scroll.contentSize.height + scroll.adjustedContentInset.bottom - scroll.bounds.height,
                            -scroll.adjustedContentInset.top)
            scroll.setContentOffset(CGPoint(x: scroll.contentOffset.x,
                                            y: min(max(target, -scroll.adjustedContentInset.top), limit)),
                                    animated: false)
            await ShotWait.laidOut(window)
        }
        guard let shown = SetupTypedEntryTests.element(id, in: window)?.accessibilityFrame else {
            return nil
        }
        return views(in: window).compactMap { $0 as? SetupUISlider }
            .filter { slider in
                let box = slider.convert(slider.bounds, to: nil)
                return box.minY >= shown.minY && box.minY - shown.maxY < 60
                    && box.maxX > shown.minX && box.minX < shown.maxX
            }
            .min { $0.convert($0.bounds, to: nil).minY < $1.convert($1.bounds, to: nil).minY }
    }

    /// Every view under `root`.
    static func views(in root: UIView) -> [UIView] {
        var found: [UIView] = []
        var stack = [root]
        while let view = stack.popLast() {
            found.append(view)
            stack += view.subviews
        }
        return found
    }

    @Test("A swipe to delete starts only outside the row's fields (final review, Minor 4)")
    func swipeStartsOutsideTheFields() {
        let fields = [CGRect(x: 10, y: 4, width: 180, height: 32), CGRect(x: 200, y: 4, width: 90, height: 32)]
        #expect(!SetupSwipeToDelete.swipes(from: CGPoint(x: 150, y: 20), fields: fields))
        #expect(!SetupSwipeToDelete.swipes(from: CGPoint(x: 280, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 300, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 195, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 150, y: 20), fields: []))
    }

    /// The adjustable accessibility element under the element with `id`.
    static func adjustable(_ id: String, in root: NSObject) -> NSObject? {
        guard let control = SetupTypedEntryTests.element(id, in: root) else {
            return nil
        }
        var queue: [NSObject] = [control]
        var visited = 0
        while !queue.isEmpty, visited < 2_000 {
            let node = queue.removeFirst()
            visited += 1
            if node.accessibilityTraits.contains(.adjustable) {
                return node
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    queue += (0..<count).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return nil
    }

    /// The Setup tab at `path` in an upright phone's window, under the
    /// app's presentation root, for `body` to act on.
    static func hosted(rig: Described.Rig, path: [SetupTree.Route],
                       _ body: (UIWindow) async throws -> Void) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let root = SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await body(window)
    }

    /// The number rows a described page draws, by their ids.
    static func numberRowIds(_ app: AppModel, category: String, page: String) throws -> [String] {
        let description = try #require(app.setupFeed.description(for: category))
        let found = try #require(description.pages.first { $0.id == page })
        return found.sections.flatMap(\.controls)
            .filter { DescribedControl.row(for: $0) == .numberRow && $0.readout == nil }
            .map(\.id)
    }

    /// Each number row's value field: under its title when `stacked`, beside it otherwise.
    static func expectLayout(_ probe: SetupLayoutProbe, ids: [String], stacked: Bool) {
        var measured = 0
        for id in ids {
            guard let label = probe.frame(id, .label), let value = probe.frame(id, .value) else {
                continue
            }
            measured += 1
            if stacked {
                #expect(value.minY >= label.maxY - 0.5, "\(id): the value field is not under its title")
                #expect(abs(value.minX - label.minX) < 1, "\(id): the value field does not start under its title")
            } else {
                #expect(value.minY < label.maxY && value.maxY > label.minY, "\(id): the value field is not beside its title")
                #expect(value.minX >= label.maxX, "\(id): the value field is not after its title")
            }
        }
        #expect(measured > 0, "No number row was laid out on the page")
    }

    /// Draws the Setup tab at `path` in an upright phone's window with the
    /// Setup switch, as the app's root gives it, and returns where its value
    /// rows were laid out.
    static func shoot(_ name: String, rig: Described.Rig, path: [SetupTree.Route],
                      dynamicType: DynamicTypeSize, height: CGFloat = 874,
                      width: CGFloat = 402) async throws -> SetupLayoutProbe {
        let size = CGSize(width: width, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let probe = SetupLayoutProbe()
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(.dark)
        .environment(\.dynamicTypeSize, dynamicType)
        .environment(\.setupLayoutProbe, probe)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return probe
    }
}
