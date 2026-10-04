// NereusSDR for iOS: pictures of the transmit panadapter while keyed, high SWR, DUP and Setup's transmit display
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

/// Task 54f's pictures (R-IOS-11, R-IOS-13): the real app keyed on a fake
/// Core that offers the transmit display with DUP, the band fed the Core's
/// transmit context and synthetic transmit rows (D4: no radio's data). With
/// `NEREUS_54F_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_54F_SHOTS`), each is written there.
@Suite("Transmit display pictures", .serialized)
@MainActor
struct TransmitDisplayShotTests {
    static let dial = 7_236_400.0

    @Test("keyed upright and sideways, high SWR, a Core without it, the Display sheet's DUP and Setup")
    func transmitDisplayShots() async throws {
        let (model, station) = try await connected()
        let main = model.main
        let transmit = main.transmit
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle { main.slices.entries.count == 2 && transmit.amp != nil })
        let band = main.band
        band.endpointId = 1
        band.gates = MediaFeatureGates(agreedMinor: 11) { name in
            ["remoteMediaVersion": 1, "spectrumGrantVersion": 1, "txDisplayVersion": 3][name] ?? 0
        }
        band.receive(.context(try #require(BandFlagShotTests.context())))

        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state.isKeyed })
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 3, name: "txSliceId", value: .i64(0)),
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(92)),
            .init(ordinal: 11, name: "swr", value: .f64(1.2)),
            .init(ordinal: 13, name: "micLevelDb", value: .f64(-12)),
        ]))
        #expect(await settle { band.transmit.keyedHere && band.keyedView })
        let view = band.view
        #expect(view.centerHz == Self.dial && view.spanHz == 8_000)
        // The transmit model reads the keyed view again on the main queue's next turn.
        #expect(await settle { transmit.txFilterHz != nil })
        let passband = try #require(transmit.txFilterHz)
        try await shoot("54f-keyed-upright", sideways: false, model: model) {
            feedTransmit(band, generation: 7, limit: .none, passband: passband)
        }
        try await shoot("54f-keyed-sideways", sideways: true, model: model) {
            feedTransmit(band, generation: 7, limit: .none, passband: passband)
        }

        // Another device sets the shared transmit view.
        try await shoot("54f-keyed-shared-upright", sideways: false, model: model) {
            feedTransmit(band, generation: 8, limit: .shared, passband: passband)
        }

        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 29, name: "highSwr", value: .bool(true)),
            .init(ordinal: 30, name: "swrWindBackLatched", value: .bool(true)),
            .init(ordinal: 11, name: "swr", value: .f64(3.4)),
        ]))
        #expect(await settle { band.transmit.windBackLatched })
        try await shoot("54f-keyed-high-swr-upright", sideways: false, model: model) {
            feedTransmit(band, generation: 9, limit: .none, passband: passband)
        }
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 29, name: "highSwr", value: .bool(false)),
            .init(ordinal: 30, name: "swrWindBackLatched", value: .bool(false)),
        ]))

        // The Display sheet: DUP on this Core, then greyed on one below 3.
        try await shoot("54f-display-sheet-dup-upright", sideways: false, sheet: .display, model: model)

        // A Core that sends no transmit display: the picture held and the line.
        band.gates = MediaFeatureGates(agreedMinor: 11) { name in
            ["remoteMediaVersion": 1, "spectrumGrantVersion": 1][name] ?? 0
        }
        #expect(band.transmitDisplayMissing)
        try await shoot("54f-no-transmit-display-upright", sideways: false, model: model)

        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state == .idle })
        await model.disconnect()
    }

    @Test("Setup's transmit display, on a Core that takes the settings and one that does not")
    func setupTransmitDisplayShots() async throws {
        for (version, name) in [(Int64(3), "54f-setup-transmit-display"), (Int64(1), "54f-setup-transmit-display-older-core")] {
            let rig = try TransmitDisplayTests.Rig(txDisplay: version)
            let page = DisplayOnThisPhonePage(main: rig.main)
            let root = NavigationStack {
                List {
                    page.transmitDisplay
                    page.coreTransmitDisplay
                }
                .navigationTitle("Display")
            }
            .preferredColorScheme(.dark)
            try await render(root, name: name, size: CGSize(width: 402, height: 1300))
        }
    }

    // MARK: Inside

    /// The app connected to a fake Core that lets it transmit and sends its
    /// transmit display with DUP, with an empty transmit state.
    private func connected() async throws -> (AppModel, FakeStation) {
        let suite = "TransmitDisplayShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        UIApplication.shared.isIdleTimerDisabled = false
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: [.remoteTx, .txDisplay])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0)),
                   .init(ordinal: 3, name: "txSliceId", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    /// The Core's transmit context for the keyed view and a screen of
    /// synthetic transmit rows: a voice filling the transmit filter
    /// (`passband`, on the slice's side of the carrier), some 50 dB over the
    /// transmitter's own floor.
    private func feedTransmit(_ band: BandModel, generation: UInt32, limit: MediaControlEvent.Grant.Limit,
                              passband: ClosedRange<Double>) {
        let view = band.view
        let pixels = max(band.requestedPixels, 64)
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("shots"), "endpointId": .number(1),
            "revision": .number(3), "contextGeneration": .number(Double(generation)), "sourceStream": .number(0),
            "sourceCentreHz": .number(view.centerHz), "sampleRateHz": .number(96_000),
            "centreHz": .number(view.centerHz), "spanHz": .number(view.spanHz), "wideCentreHz": .number(0),
            "wideSpanHz": .number(0), "traceSamples": .number(Double(pixels)),
            "waterfallSamples": .number(Double(pixels)), "wideSamples": .number(0), "minDbm": .number(-80),
            "maxDbm": .number(30), "fps": .number(15), "framesPerLine": .number(1),
            "grantedFftSize": .number(32_768), "grantedTier": .string("wide"),
            "requestedPixels": .number(Double(pixels)), "grantedPixels": .number(Double(pixels)),
            "limit": .string(limit.rawValue), "transmit": .bool(true),
        ]
        guard let context = MediaControlDecoder.context(payload, wideband: false, grant: true, transmit: true) else {
            Issue.record("the transmit context did not decode")
            return
        }
        band.receive(.context(context))
        let low = view.centerHz - view.spanHz / 2
        let binHz = view.spanHz / Double(pixels)
        for sequence in 0..<600 {
            let trace = (0..<pixels).map { index -> Float in
                let hz = low + (Double(index) + 0.5) * binHz
                let wobble = Float(sin(Double(index) * 0.37 + Double(sequence) * 0.21)) * 3
                if passband.contains(hz) {
                    let syllable = Float(sin(hz / 420 + Double(sequence) * 0.09)) * 9
                    return -18 + syllable + wobble
                }
                let skirt = hz > passband.upperBound ? hz - passband.upperBound : passband.lowerBound - hz
                return max(-66 + wobble, -30 - Float(skirt / 60))
            }
            band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: generation,
                                                    encoderSequence: UInt32(sequence),
                                                    producerTimestamp: UInt64(sequence) * 66_000_000,
                                                    isKeyframe: sequence == 0, waterfallAdvance: true,
                                                    minDbm: -80, maxDbm: 30, traceDbm: trace, waterfallDbm: trace,
                                                    wideDbm: [])))
        }
    }

    private func settle(seconds: Double = 5, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// `feed` runs once the band has sized its waterfall on screen.
    private func shoot(_ name: String, sideways: Bool, sheet: OpenSheet? = nil, model: AppModel,
                       feed: () -> Void = {}) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let root = TransmitDisplayShotRoot(model: model, sheet: sheet).preferredColorScheme(.dark)
        try await render(root, name: name, size: size, sideways: sideways, band: model.main.band, feed: feed)
    }

    private func render(_ view: some View, name: String, size: CGSize, sideways: Bool = false,
                        band: BandModel? = nil, feed: () -> Void = {}) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let host = UIHostingController(rootView: view)
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
        // Drawn, fed once the band has sized itself, then drawn again.
        if let band {
            let draw = try await ShotWait.requireBandLaidOut(band, in: window)
            feed()
            try await ShotWait.requireBandShown(band, in: window, after: draw)
        } else {
            await ShotWait.laidOut(window)
            feed()
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_54F_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, with one sheet open or none.
private struct TransmitDisplayShotRoot: View {
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
