// NereusSDR for iOS: the Digital Spectrum Recorder on the pan, its time sheet and its Setup page on screen, for comparing with the mockups
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Recording plan Task 28 (A16 for the iPhone and iPad): the real band,
/// flag, strip, hours map, LIVE, SAVE and past picture, fed a recordings
/// client by hand. With `NEREUS_BAND_SHOTS` set (through
/// `TEST_RUNNER_NEREUS_BAND_SHOTS`) each pan frame is written there as
/// `dsr-phone-<name>.png` or `dsr-ipad-<name>.png`; with `NEREUS_MAIN_SHOTS`
/// the time sheet and Setup pages are written as `dsr-phone-<name>.png`.
@Suite("Digital Spectrum Recorder on screen", .serialized, .timeLimit(.minutes(2)))
@MainActor
struct DsrShotTests {
    typealias Flag = RecordingFlagTests

    enum Frame: String, CaseIterable, Sendable, CustomStringConvertible {
        case live = "phone-live"
        case past = "phone-past"
        case drawing = "phone-drawing"
        case save = "phone-save"
        case olderCore = "phone-older-core"
        case ipadPast = "ipad-past"

        var description: String { "dsr-\(rawValue)" }

        var size: CGSize {
            self == .ipadPast ? CGSize(width: 1024, height: 700) : CGSize(width: 402, height: 640)
        }
    }

    @Test(arguments: Frame.allCases)
    func thePanShowsTheRecorder(_ frame: Frame) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ])))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        mirror.apply(FlagButtonsShotTests.slice(0, hz: 7_236_400, active: true))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        let (rig, model, _) = await DsrTests.pan(version: frame == .olderCore ? 3 : 4)
        let band = main.band
        model.ownSeconds = { [weak band] in band?.mostLookBackSeconds ?? 0 }
        model.lookBack = { [weak band] seconds in band?.lookBack(seconds: seconds) }
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "DsrShotTests") ?? .standard)

        let size = frame.size
        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, flagControls: main.flagControls, dsr: model)
        }
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(await ShotWait.until { model.waterfallPoints > 0 })
        let geometry = try #require(band.pointGeometry(size: size)?.geometry)

        // The hours, once the map has asked.
        if frame != .olderCore {
            let (hoursIndex, _) = try #require(await DsrTests.invoke(rig, "dsr.hours"))
            await Flag.answer(rig, hoursIndex, values: Self.hours())
            #expect(await ShotWait.until { !model.hours.isEmpty })
        }

        switch frame {
        case .live:
            #expect(!model.liveLit)
        case .past, .ipadPast:
            // Half an hour back, Slice A playing from a signal there.
            model.drag(from: model.top, by: -1_800 / model.secondsPerPoint)
            model.dragEnded()
            try await answerRows(rig, model, geometry: geometry, share: 1)
            let (spectrumIndex, _) = try #require(await DsrTests.invoke(rig, "dsr.spectrum"))
            await Flag.answer(rig, spectrumIndex, values: Self.frames([model.top], geometry: geometry))
            Flag.publish(rig, DsrTests.document(dsr: DsrTests.dsrPart(
                rewound: DsrTests.rewound(Int64((model.top * 1e9).rounded())))), revision: 2)
            model.documentChanged()
            if frame == .ipadPast {
                model.toggleSave()
            }
            #expect(await ShotWait.until { model.pastSpectrum != nil && model.drawingText == nil })
            #expect(model.rewound != nil && model.liveLit)
        case .drawing:
            model.drag(from: model.top, by: -3_600 / model.secondsPerPoint)
            model.dragEnded()
            try await answerRows(rig, model, geometry: geometry, share: 0.3)
            #expect(await ShotWait.until { model.drawingText != nil && !model.rows.isEmpty })
        case .save:
            model.toggleSave()
            #expect(model.saving)
        case .olderCore:
            model.toggleSave()
            #expect(model.note == RecordingsClient.updateReason && !model.saving)
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: frame.description)
    }

    /// Answers the picture's `dsr.rows` with `share` of its rows, newest first.
    private func answerRows(_ rig: Flag.Rig, _ model: DsrPanModel, geometry: BandGeometry,
                            share: Double) async throws {
        let (index, _) = try #require(await DsrTests.invoke(rig, "dsr.rows"))
        let top = model.top
        let count = Int(model.waterfallPoints * share)
        let moments = (0..<count).map { top - Double($0) * model.secondsPerPoint }
        await Flag.answer(rig, index, values: Self.frames(moments, geometry: geometry))
        #expect(await ShotWait.until { model.rows.count >= count })
    }

    static func hours() -> [LinkMessage.PropertyEntry] {
        let newest = (DsrTests.now / 3600).rounded(.down) * 3600
        let starts = (0..<3).map { Int64((newest - Double($0) * 3600) * 1e9) }
        return [.init(name: "starts", value: .utf8(DsrPastFrames.packTimes(starts).base64EncodedString())),
                .init(name: "activity", value: .utf8("[0.85,0.35,0.6]"))]
    }

    /// The Core's rows across the band's width: the noise, a few signals
    /// that come and go, and a stretch you transmitted.
    static func frames(_ moments: [Double], geometry: BandGeometry) -> [LinkMessage.PropertyEntry] {
        let bins = Int(geometry.size.width)
        let hzPerBin = geometry.spanHz / Double(bins)
        var tenths: [Int16] = []
        var transmit: [UInt8] = []
        for moment in moments {
            let second = Int(moment.rounded(.down))
            let keyed = second % 240 < 20
            transmit.append(keyed ? 1 : 0)
            for bin in 0..<bins {
                var level = -1_150 + (bin * 37 + second * 11) % 50
                let at = Double(bin) / Double(bins)
                for (centre, every) in [(0.3, 4), (0.55, 7), (0.62, 3), (0.8, 11)]
                    where abs(at - centre) < 0.006 && second % every != 0 {
                    level = -700
                }
                if keyed, abs(at - 0.5) < 0.02 {
                    level = -600
                }
                tenths.append(Int16(level))
            }
        }
        let times = moments.map { Int64(($0 * 1e9).rounded()) }
        return [.init(name: "slice", value: .i64(0)),
                .init(name: "firstBinHz", value: .f64(geometry.lowHz + hzPerBin / 2)),
                .init(name: "hzPerBin", value: .f64(hzPerBin)), .init(name: "binCount", value: .i64(Int64(bins))),
                .init(name: "times", value: .utf8(DsrPastFrames.packTimes(times).base64EncodedString())),
                .init(name: "bins", value: .utf8(DsrPastFrames.packBins(tenths).base64EncodedString())),
                .init(name: "transmit", value: .utf8(Data(transmit).base64EncodedString())),
                .init(name: "rowsLeft", value: .i64(0)), .init(name: "nextToUtcNs", value: .i64(0))]
    }

    // MARK: The time sheet and Setup

    @Test("The time sheet and Setup > Digital Spectrum Recorder, with an SSD, an SD card, older and not connected")
    func pagePictures() async throws {
        let (_, pan, _) = await DsrTests.pan()
        pan.openTimeSheet()
        try await shoot("dsr-phone-time", height: DsrTimeSheet.height, tabBar: false) {
            DsrTimeSheet(model: pan)
        }
        for (name, version, connected, dsr, disks) in [
            ("dsr-phone-setup", Int64(4), true, DsrTests.dsrPart(disk: "/media/ssd"), [DsrTests.sdCard, DsrTests.ssd]),
            ("dsr-phone-setup-sd", Int64(4), true,
             DsrTests.dsrPart(recording: false, reason: DsrSetupModel.onlySdReason), [DsrTests.sdCard]),
            ("dsr-phone-setup-older-core", Int64(3), true, DsrTests.dsrPart(), [DsrTests.ssd]),
            ("dsr-phone-setup-not-connected", Int64(4), false, DsrTests.dsrPart(), [DsrTests.ssd]),
        ] {
            let (_, model) = await DsrTests.setup(version: version, dsr: dsr, disks: disks, connected: connected)
            try await shoot(name, height: 1500) {
                NavigationStack {
                    DsrSetupPage(model: model)
                }
            }
        }
    }

    /// One page on an upright phone, over the Setup tab's bar, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat, tabBar: Bool = true,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: 402, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            if tabBar {
                TabBar(selection: .constant(.setup), sideways: false)
            }
        }
        .background(ChromeColours.page)
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
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        #expect(image.size == size)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
