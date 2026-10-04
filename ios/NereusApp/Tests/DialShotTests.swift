// NereusSDR for iOS: the three tuning dials on screen, for comparing with picture 3
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

/// D12, spec section 5.1 item 8: the real main screen connected to a fake
/// Core with no dial (a new install), the knob on the waterfall, its step
/// menu open, the knob in a sheet, the thumbwheel, and the knob sideways.
/// With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is written there as a PNG
/// for comparing with `03-tuning-dial.jpg`.
@Suite("The tuning dials on screen", .serialized)
@MainActor
struct DialShotTests {
    @Test("no dial, the knob, its step menu, the sheet, the thumbwheel and the knob sideways")
    func dialShots() async throws {
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: { QuietDialPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            .init(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
        ])))
        let main = model.main
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.slices.entries.first?.stepHz == 100
                && main.band.catalog != nil
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        main.changeDisplay { $0.traceFillOpacity = 0.7 }
        let band = main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        #expect(settings.dialKind == .off)
        try await shoot("dial-off", model: model)

        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob", model: model)
        main.tuning.toggleDialStepMenu()
        try await shoot("dial-knob-steps", model: model)
        main.tuning.closeStepMenu()

        settings.dialKind = .sheetKnob
        main.tuning.openDialSheet(sliceId: 0)
        try await shoot("dial-sheet", model: model)
        main.tuning.closeDialSheet()

        settings.dialKind = .thumbwheel
        try await shoot("dial-thumbwheel", model: model)

        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob-sideways", model: model, sideways: true)

        // Fix round A: sideways the thumbwheel keeps its upright width, at
        // the right, on this phone and on an iPhone 17 Pro Max.
        settings.dialKind = .thumbwheel
        try await shoot("dial-thumbwheel-sideways", model: model, sideways: true)
        try await shoot("dial-thumbwheel-promax", model: model, size: Self.proMax)
        try await shoot("dial-thumbwheel-promax-sideways", model: model, sideways: true, size: Self.proMax)
        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob-promax", model: model, size: Self.proMax)
        try await shoot("dial-knob-promax-sideways", model: model, sideways: true, size: Self.proMax)
        await model.disconnect()
    }

    /// Task 61, fix round 1: a flick on the pop-up knob at 30 rad/s, then
    /// the coast drawn at nine moments on a test clock, `coast-0.png` at
    /// the release to `coast-8.png` where it has stopped: the grip keeps
    /// turning, each step shorter, and the frequency follows it.
    @Test("a flick's coast, frame by frame")
    func coastShots() async throws {
        let rig = try BandTuningTests.Rig()
        #expect(await settle(seconds: 30) { rig.slices.entries.count == 2 })
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        var now = 1_000.0
        let spinner = DialSpinner(clock: { now }, drivesItself: false)
        let tuning = rig.tuning
        let size = CGSize(width: 402, height: 460)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = SheetKnob(tuning: tuning, slices: rig.slices, settings: settings, spinner: spinner)
            .fixedSize(horizontal: false, vertical: true)
            .frame(width: size.width, height: size.height, alignment: .top)
            .background(Color.black)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        spinner.touchDown(tuning: tuning, reversed: false)
        for _ in 0..<8 {
            now += 1.0 / 120
            spinner.follow(byRadians: 30.0 / 120)
        }
        spinner.release()
        #expect(spinner.isCoasting)
        let start = now
        let moments = [0.0, 0.1, 0.2, 0.35, 0.5, 0.75, 1.0, 1.3, 1.7]
        var frame = 0
        for (index, moment) in moments.enumerated() {
            while now < start + moment - 1e-9 {
                now += 1.0 / 120
                frame += 1
                spinner.frame(at: now)
            }
            try await Task.sleep(for: .milliseconds(150))
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let hz = rig.slices.entries.first?.slice.frequencyHz ?? 0
            print("coast-\(index): t=\(moment) s, frame \(frame), angle \(spinner.angle) rad, \(hz) Hz, "
                + "coasting \(spinner.isCoasting)")
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image.pngData() {
                try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("coast-\(index).png"))
            }
        }
        #expect(!spinner.isCoasting)
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

    /// An iPhone 17 Pro Max's window, upright.
    static let proMax = CGSize(width: 440, height: 956)

    /// Draws the main screen upright or sideways, the band fed synthetic
    /// rows, in a window `size` wide upright (an iPhone 17 Pro's by default).
    private func shoot(_ name: String, model: AppModel, sideways: Bool = false,
                       size upright: CGSize = CGSize(width: 402, height: 874)) async throws {
        let size = sideways ? CGSize(width: upright.height, height: upright.width) : upright
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = DialShotRoot(model: model, sideways: sideways).preferredColorScheme(.dark)
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
        let band = model.main.band
        band.reset()
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
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

/// The app's root as `RootView` lays it out, on the Panadapter tab, with
/// the band's upright width from the window as `RootView` gives it.
private struct DialShotRoot: View {
    @ObservedObject var model: AppModel
    let sideways: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: sideways)
            }
            .environment(\.uprightBandWidth, proxy.uprightBandWidth)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so the fake Core starts nothing on
/// the network in these pictures.
private final class QuietDialPeer: MediaPeerConnection, @unchecked Sendable {
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
