// NereusSDR for iOS: tuning on the band on screen: the step menu, the number pad, the Line row and three trace widths
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

/// D74, D75: the real main screen connected to a fake Core, with the flag's
/// step menu open, the number pad open, the Display sheet showing its Line
/// row, the band at the thinnest Line, at 0.5 and at 3 points, the band
/// panned so slice A's flag leaves it, and the band mid-drag before a late
/// Core has answered. With `NEREUS_MAIN_SHOTS` set to a
/// directory (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is
/// written there as a PNG for comparing with pictures 25 and 26.
@Suite("Tuning on the band on screen", .serialized)
@MainActor
struct TuningShotTests {
    @Test("the step menu, the number pad, the Line row and the trace at three widths")
    func tuningShots() async throws {
        let suite = "TuningShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { QuietPeer() },
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

        // The step menu, from slice A's step.
        #expect(main.tuning.stepLabel(for: try #require(main.slices.entries.first)) == "100 Hz")
        main.tuning.openStepMenu(sliceId: 0)
        try await shoot("step-menu", sheet: nil, model: model)
        main.tuning.closeStepMenu()

        // The number pad with 14.074 typed in MHz.
        main.tuning.openPad(sliceId: 0)
        let pad = try #require(main.tuning.pad)
        for key: FrequencyPadModel.Key in [.digit(1), .digit(4), .point, .digit(0), .digit(7), .digit(4)] {
            pad.press(key)
        }
        #expect(pad.enterLabel == "Tune to 14.074.000")
        try await shoot("number-pad", sheet: nil, model: model)
        main.tuning.closePad()

        // The Display sheet with its Line row between Fill and Top.
        try await shoot("display-sheet-line", sheet: .display, model: model)

        // The band at the thinnest Line (one pixel), at 0.5 and at 3 points.
        let pixel = 1 / Double(UIScreen.main.scale)
        for (name, width) in [("band-line-thinnest", pixel), ("band-line-0.5", 0.5), ("band-line-3", 3.0)] {
            main.changeDisplay { $0.traceWidthPoints = width }
            try await shoot(name, sheet: nil, model: model)
        }
        main.changeDisplay { $0.traceWidthPoints = 0.5 }

        // The band panned 30 kHz up and the Core answered: slice A's flag
        // goes with its line.
        let panned = BandFlagShotTests.centre + 30_000
        band.requestView(TuneGestures.View(centerHz: panned, spanHz: BandFlagShotTests.span))
        band.receive(.context(try #require(Self.context(centreHz: panned, generation: 2))))
        try await shoot("band-panned", sheet: nil, model: model, centreHz: panned, generation: 2)

        // Mid-drag, 10 kHz further, before a late Core has answered: the
        // band has moved with the finger, and the edge nothing has arrived
        // for yet is empty.
        band.requestView(TuneGestures.View(centerHz: panned + 10_000, spanHz: BandFlagShotTests.span))
        #expect(band.centerHz == panned)
        try await shoot("band-panning-live", sheet: nil, model: model, centreHz: panned, generation: 2)
        await model.disconnect()
    }

    // MARK: Inside

    static func context(centreHz: Double, spanHz: Double = BandFlagShotTests.span,
                        sourceCentreHz: Double = BandFlagShotTests.centre, generation: UInt32 = 1,
                        endpointId: UInt32 = 1) -> MediaControlEvent.DisplayContext? {
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("shots"), "endpointId": .number(Double(endpointId)),
            "revision": .number(2), "contextGeneration": .number(Double(generation)), "sourceStream": .number(0),
            "sourceCentreHz": .number(sourceCentreHz), "sampleRateHz": .number(192_000),
            "centreHz": .number(centreHz), "spanHz": .number(spanHz), "wideCentreHz": .number(0),
            "wideSpanHz": .number(0), "traceSamples": .number(1_206), "waterfallSamples": .number(1_206),
            "wideSamples": .number(0), "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30),
            "framesPerLine": .number(1),
        ]
        return MediaControlDecoder.context(payload, wideband: false, grant: false)
    }

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

    /// Draws the upright main screen, `sheet` open, the band fed synthetic
    /// rows (shifted with the view when it has panned).
    private func shoot(_ name: String, sheet: OpenSheet?, model: AppModel,
                       centreHz: Double = BandFlagShotTests.centre, generation: UInt32 = 1) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = TuningShotRoot(model: model, sheet: sheet).preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
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
        if centreHz == BandFlagShotTests.centre {
            BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        } else {
            Self.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3), centreHz: centreHz,
                      generation: generation)
        }
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

    /// The same stations as ``BandFlagShotTests/feed(_:width:lines:)``,
    /// seen through a view centred on `centreHz`.
    static func feed(_ band: BandModel, width: Int, lines: Int, centreHz: Double, generation: UInt32) {
        var seed: UInt64 = 7
        func random() -> Float {
            seed = seed &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
            return Float(seed >> 40) / Float(1 << 24)
        }
        let span = BandFlagShotTests.span
        let low = centreHz - span / 2
        let stations: [(hz: Double, dbm: Float, width: Double)] = [(7_234_900, -72, 2_600), (7_247_500, -84, 2_600),
                                                                   (7_261_000, -96, 2_400), (7_268_000, -80, 2_400)]
        for sequence in 1...lines {
            let trace = (0..<width).map { index -> Float in
                let hz = low + (Double(index) + 0.5) / Double(width) * span
                var level = -128 + random() * 12
                for station in stations where abs(hz - station.hz) < station.width / 2 && sequence % 40 < 32 {
                    level = max(level, station.dbm - random() * 8)
                }
                return level
            }
            band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: generation,
                                                    encoderSequence: UInt32(sequence),
                                                    producerTimestamp: UInt64(sequence) * 33_000_000,
                                                    isKeyframe: sequence == 1, waterfallAdvance: true, minDbm: -160,
                                                    maxDbm: 0, traceDbm: trace, waterfallDbm: trace, wideDbm: [])))
        }
    }
}

/// The app's root as `RootView` lays it out, with one sheet open.
private struct TuningShotRoot: View {
    @ObservedObject var model: AppModel
    let sheet: OpenSheet?

    var body: some View {
        VStack(spacing: 0) {
            MainScreen(app: model, main: model.main, sheet: sheet)
            TabBar(selection: .constant(.panadapter), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so the fake Core starts nothing on
/// the network in these pictures.
private final class QuietPeer: MediaPeerConnection, @unchecked Sendable {
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
