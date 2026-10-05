// NereusSDR for iOS: the Pan 1 and Display sheets on screen, newer and older Cores, for comparing with picture 25
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// D73: the real main screen with the Pan 1 or Display sheet open, connected
/// to a fake Core that is, scene by scene, the newer Core (every addition),
/// the Core JJ's first install talks to (no `bands`, no band select, notch
/// control 1, display extras 1), or older still. With `NEREUS_MAIN_SHOTS`
/// set to a directory (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each
/// screen is written there as a PNG for comparing with
/// `25-pan-and-display-sheets.jpg`.
@Suite("Pan and Display sheets on screen", .serialized)
@MainActor
struct DropSheetShotTests {
    @Test("Pan 1 stays usable after this phone's last slice closes")
    func emptyPanShot() async throws {
        let suite = "DropSheetShotTests-empty-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle(seconds: 5) { model.main.pan.sliceLetter == "A" })
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:0", className: "SliceModel")))
        #expect(await settle(seconds: 5) { model.main.pan.sliceLetter == nil && model.main.pan.canAddSlice })
        #expect(!model.main.pan.canSelectBand)
        #expect(!model.main.pan.canAddNotch)
        try await shoot("pan-sheet-empty-slice", sheet: .pan, sideways: false, model: model, feed: { _ in })
        await model.disconnect()
    }

    /// What the Core offers in one scene.
    struct Core {
        var bands: Bool
        var capabilities: [String: Int64]
    }

    static let newer = Core(bands: true, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 1, "displayExtrasVersion": 2,
        "notchControlVersion": 2, "bandSelectVersion": 1,
    ])
    /// Integration 0426fe6b, on the Pi for the first listening test.
    static let listeningBuild = Core(bands: false, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 1, "displayExtrasVersion": 1,
        "notchControlVersion": 1, "bandSelectVersion": 0,
    ])
    static let bandsWithoutSelect = Core(bands: true, capabilities: listeningBuild.capabilities)
    static let noExtras = Core(bands: false, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 0, "displayExtrasVersion": 0,
        "notchControlVersion": 1, "bandSelectVersion": 0,
    ])

    @Test("the Pan 1 and Display sheets, upright and sideways, on newer and older Cores")
    func sheetShots() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // Slice A is on 40 m, as the board draws it.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        // The board's Display values: Fill 70, Top -40, Range 100, Clarity.
        model.main.changeDisplay {
            $0.traceFillOpacity = 0.7
            $0.extendedView = true
        }
        let base = model.mirror.capabilities
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        var revision: Int64 = 2
        func offer(_ core: Core) async throws {
            var properties = base.keys.sorted().compactMap { name -> LinkMessage.PropertyEntry? in
                guard core.capabilities[name] == nil, let value = base[name] else {
                    return nil
                }
                return LinkMessage.PropertyEntry(name: name, value: value.wireValue)
            }
            for (name, value) in core.capabilities.sorted(by: { $0.key < $1.key }) {
                properties.append(.init(name: name, value: .i64(value)))
            }
            await station.deliver(.capabilities(LinkMessage.Capabilities(properties: properties)))
            revision += 1
            let json = try #require(MainScreenTests.catalogueJSON())
            await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(FakeStation.catalogue(json, bands: core.bands))),
                .init(ordinal: 1, name: "revision", value: .i64(revision)),
            ])))
            #expect(await settle(seconds: 30) {
                (model.main.catalogFeed.catalog?.bands != nil) == core.bands
                    && model.mirror.capabilityVersion("displayExtrasVersion") == core.capabilities["displayExtrasVersion"]
            })
            try await Task.sleep(for: .milliseconds(100))
        }

        try await offer(Self.newer)
        #expect(model.main.pan.extendedViewOn)
        try await shoot("pan-sheet-upright", sheet: .pan, sideways: false, model: model)
        try await shoot("display-sheet-upright", sheet: .display, sideways: false, model: model)
        try await shoot("pan-sheet-sideways", sheet: .pan, sideways: true, model: model)
        try await shoot("display-sheet-sideways", sheet: .display, sideways: true, model: model)

        try await offer(Self.listeningBuild)
        #expect(model.main.pan.bandGrid == .needsNewerCore)
        try await shoot("pan-sheet-listening-build-no-bands", sheet: .pan, sideways: false, model: model)
        try await shoot("display-sheet-listening-build", sheet: .display, sideways: false, model: model)

        try await offer(Self.bandsWithoutSelect)
        try await shoot("pan-sheet-grid-greyed", sheet: .pan, sideways: false, model: model)

        try await offer(Self.noExtras)
        #expect(!model.main.display.extrasAvailable)
        try await shoot("display-sheet-extras-greyed", sheet: .display, sideways: false, model: model)
        await model.disconnect()
    }

    @Test("the Band plan and Size rows, the picker open, a plan picked, and the strip at Small and Huge")
    func bandPlanShots() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let display = model.main.display
        #expect(await settle(seconds: 30) { !display.plans.isEmpty })

        try await shoot("54d-display-sheet-band-plan", sheet: .display, sideways: false, model: model)
        display.planPickerOpen = true
        try await shoot("54d-band-plan-picker-open", sheet: .display, sideways: false, model: model)
        // Picked, the Core echoes it and the tick moves.
        let picked = try #require(display.plans.first { !$0.isDefault })
        display.pickPlan(picked)
        #expect(await settle(seconds: 30) { display.shownPlan?.id == picked.id })
        try await shoot("54d-band-plan-picked", sheet: .display, sideways: false, model: model)
        // A plan the Core refuses: its words in the picker, the tick left.
        let refused = try #require(display.plans.first { $0.id != picked.id && !$0.isDefault })
        station.refuseNext(FakeStation.bandPlanKey, reason: FakeStation.unknownBandPlanReason)
        display.pickPlan(refused)
        #expect(await settle(seconds: 30) { display.planNote == FakeStation.unknownBandPlanReason })
        #expect(display.shownPlan?.id == picked.id)
        try await shoot("54d-band-plan-refused", sheet: .display, sideways: false, model: model)
        display.planPickerOpen = false

        // The Core's plan back to the default, then the strip at each size.
        display.pickPlan(try #require(display.plans.first { $0.isDefault }))
        #expect(await settle(seconds: 30) { display.shownPlan?.isDefault == true })
        for size in [BandPlanSize.small, .huge] {
            display.setBandPlanSize(size)
            try await shoot("54d-band-plan-\(size.rawValue)", sheet: nil, sideways: false, model: model)
        }
        display.setBandPlanSize(.small)
        await model.disconnect()
    }

    @Test("Extended view On, zoomed out past the receiver, with the Core's rows across the wider span")
    func extendedViewZoomedOut() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let station = try FakeStation(additions: .all)
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        let band = model.main.band
        model.main.pan.toggleExtendedView()
        #expect(await settle(seconds: 30) { band.extendedSpanCeilingHz == FakeStation.adcRateHz / 2 })
        let span = 8_000_000.0
        try await shoot("pan-extended-view-zoomed-out", sheet: nil, sideways: false, model: model) { bandDraw in
            band.requestView(TuneGestures.View(centerHz: 7_236_400, spanHz: span))
            #expect(await settle(seconds: 30) { band.spanHz == span })
            station.sendDisplayRows(2_400)
            // The rows arrive through the media client one at a time.
            for _ in 0..<400 where (band.state.frame?.encoderSequence ?? 0) < 2_400 {
                try await Task.sleep(for: .milliseconds(50))
            }
            #expect((band.state.frame?.encoderSequence ?? 0) >= 2_400)
            #expect(band.state.frame?.traceDbm.count == bandDraw.requestedPixels)
        }
        await model.disconnect()
    }

    // MARK: Inside

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// Draws the main screen with `sheet` open. `feed` fills the band; the
    /// scene's synthetic rows by default.
    private func shoot(_ name: String, sheet: OpenSheet?, sideways: Bool, model: AppModel,
                       feed: ((ShotWait.BandDrawing) async throws -> Void)? = nil) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = SheetShotRoot(model: model, sheet: sheet).preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        if let feed {
            try await feed(bandDraw)
        } else {
            let band = model.main.band
            BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        }
        try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, with one sheet open.
private struct SheetShotRoot: View {
    @ObservedObject var model: AppModel
    let sheet: OpenSheet?

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, sheet: sheet)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so a Core that offers media starts
/// nothing on the network in these pictures.
private final class IdlePeer: MediaPeerConnection, @unchecked Sendable {
    let localDescription = AsyncStream<String> { _ in }
    let localCandidates = AsyncStream<(candidate: String, mid: String)> { _ in }
    let audioPackets = AsyncStream<RtpPacket> { _ in }
    let displayDatagrams = AsyncStream<Data> { _ in }
    let state = AsyncStream<MediaPeer.State> { _ in }

    func setRemoteDescription(_ sdp: String) throws {}
    func addRemoteCandidate(_ candidate: String, mid: String) throws {}
    func setExpectedAudioSsrc(_ ssrc: UInt32?) {}
    func close() {}
}
