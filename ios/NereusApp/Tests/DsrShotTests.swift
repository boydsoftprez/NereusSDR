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

    /// What the pan shows in a picture: the frames above, and for the
    /// screen walk the past paused, the band's own look back, and each
    /// reason the past cannot play, as its note on the waterfall.
    enum PanState: String, CaseIterable, Sendable {
        case live
        case past
        case paused
        case drawing
        case save
        case olderCore = "older-core"
        case ipadPast = "ipad-past"
        case lookBack = "look-back"
        case off = "reason-off"
        case gone = "reason-gone"
        case rate = "reason-rate"
        case diversity = "reason-diversity"
        case noFrequency = "reason-no-frequency"

        /// The frames the screen walk draws at each phone shape.
        static let walked: [PanState] = [.live, .paused, .past, .save, .lookBack, .off, .gone, .rate, .diversity,
                                         .noFrequency]

        /// The note the Core or the model shows on the waterfall, for a reason frame.
        var note: String? {
            switch self {
            case .off: return DsrPanModel.offReason
            case .gone: return "That moment is no longer kept."
            case .rate: return "That moment was kept at another sample rate."
            case .diversity: return "Turn diversity off to rewind that slice."
            case .noFrequency: return DsrPanModel.noFrequencyReason
            default: return nil
            }
        }
    }

    @Test(arguments: Frame.allCases)
    func thePanShowsTheRecorder(_ frame: Frame) async throws {
        let state: PanState
        switch frame {
        case .live: state = .live
        case .past: state = .past
        case .drawing: state = .drawing
        case .save: state = .save
        case .olderCore: state = .olderCore
        case .ipadPast: state = .ipadPast
        }
        try await shootPan(state, size: frame.size, type: .large, name: frame.description)
    }

    /// The screen walk (PR 808): each pan frame at the smallest and largest
    /// iPhone, upright and sideways, at the default text size and one step
    /// larger, written as `dsr-phone-walk-<state>-<shape>.png`.
    @Test(arguments: PanState.walked, WalkShape.all)
    func thePanAtEachShape(_ state: PanState, _ shape: WalkShape) async throws {
        try await shootPan(state, size: shape.pan, type: shape.type, name: "dsr-phone-walk-\(state.rawValue)-\(shape)")
    }

    private func shootPan(_ frame: PanState, size: CGSize, type: DynamicTypeSize, name: String) async throws {
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
        let (rig, model, _) = await DsrTests.pan(version: frame == .olderCore ? 3 : 4,
                                                 dsr: DsrTests.dsrPart(enabled: frame != .off))
        let band = main.band
        model.ownSeconds = { [weak band] in band?.mostLookBackSeconds ?? 0 }
        model.lookBack = { [weak band] seconds in band?.lookBack(seconds: seconds) }
        band.liveSpectrumHidden = { [weak model] in model?.hidesLiveSpectrum ?? false }
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "DsrShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, flagControls: main.flagControls, dsr: model)
            // As Pan 1 has it: the strip's LIVE is the only one.
            BandReadouts(band: band, lookBackLive: false)
        }
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        .dynamicTypeSize(type)
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
        if frame != .olderCore, frame != .off {
            let (hoursIndex, _) = try #require(await DsrTests.invoke(rig, "dsr.hours"))
            await Flag.answer(rig, hoursIndex, values: Self.hours())
            #expect(await ShotWait.until { !model.hours.isEmpty })
        }

        switch frame {
        case .live:
            #expect(!model.liveLit)
        case .past, .ipadPast, .paused:
            // Half an hour back, Slice A playing from a signal there.
            model.drag(from: model.top, by: -1_800 / model.secondsPerPoint)
            model.dragEnded()
            try await answerRelease(rig, model)
            try await answerRows(rig, model, geometry: geometry, share: 1)
            let (spectrumIndex, _) = try #require(await DsrTests.invoke(rig, "dsr.spectrum"))
            await Flag.answer(rig, spectrumIndex, values: Self.frames([model.top], geometry: geometry))
            if frame != .paused {
                // Rewound from this device, so the picture follows the sound.
                try await DsrTests.rewindHere(rig, model, moment: model.top, revision: 2)
            }
            if frame == .ipadPast {
                model.toggleSave()
            }
            #expect(await ShotWait.until { model.pastSpectrum != nil && model.drawingText == nil })
            #expect(frame == .paused ? !model.followsSound : model.rewound != nil)
            #expect(model.liveLit)
        case .drawing:
            model.drag(from: model.top, by: -3_600 / model.secondsPerPoint)
            model.dragEnded()
            try await answerRelease(rig, model)
            try await answerRows(rig, model, geometry: geometry, share: 0.3)
            #expect(await ShotWait.until { model.drawingText != nil && !model.rows.isEmpty })
        case .save:
            model.toggleSave()
            #expect(model.saving)
        case .olderCore:
            model.toggleSave()
            #expect(model.note == RecordingsClient.updateReason && !model.saving)
        case .lookBack:
            // The Display sheet's Look back, within the band's own minutes: the strip's LIVE is red.
            band.lookBack(seconds: 20)
            #expect(await ShotWait.until { band.lookBackLines > 0 })
        case .off, .gone, .rate, .diversity, .noFrequency:
            model.showNote(try #require(frame.note))
            #expect(model.note == frame.note)
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: name)
    }

    /// Letting go plays Slice A from the top (rewind plays on release): the
    /// Core takes it, so no unanswered call ends in a note on the picture.
    private func answerRelease(_ rig: Flag.Rig, _ model: DsrPanModel) async throws {
        let (index, _) = try #require(await DsrTests.invoke(rig, "dsr.rewind"))
        await Flag.answer(rig, index)
        #expect(await ShotWait.until { model.mine.contains(0) })
    }

    /// Answers the picture's `dsr.rows` with `share` of its rows, newest first.
    private func answerRows(_ rig: Flag.Rig, _ model: DsrPanModel, geometry: BandGeometry,
                            share: Double) async throws {
        let (index, _) = try #require(await DsrTests.invoke(rig, "dsr.rows"))
        let top = model.top
        let total = Int(model.waterfallPoints)
        let count = Int(model.waterfallPoints * share)
        let moments = (0..<count).map { top - Double($0) * model.secondsPerPoint }
        // Part of the picture: the rest is still to come, so the ask stays with the Core.
        let next = Int64(((top - Double(count) * model.secondsPerPoint) * 1e9).rounded())
        await Flag.answer(rig, index, values: Self.frames(moments, geometry: geometry,
                                                          rowsLeft: count < total ? total - count : 0,
                                                          next: count < total ? next : 0))
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
    static func frames(_ moments: [Double], geometry: BandGeometry, rowsLeft: Int = 0,
                       next: Int64 = 0) -> [LinkMessage.PropertyEntry] {
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
                .init(name: "rowsLeft", value: .i64(Int64(rowsLeft))), .init(name: "nextToUtcNs", value: .i64(next))]
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
        // The disk row at an iPhone 17 Pro's width upright and sideways, at
        // the default text size and one step larger: "Disk" keeps its width
        // and the menu stays compact.
        for (width, way) in [(CGFloat(402), "upright"), (CGFloat(874), "sideways")] {
            for (type, size) in [(DynamicTypeSize.large, "default"), (DynamicTypeSize.xLarge, "larger")] {
                let (_, model) = await DsrTests.setup(dsr: DsrTests.dsrPart(disk: "/media/ssd"),
                                                      disks: [DsrTests.sdCard, DsrTests.ssd])
                let probe = SetupLayoutProbe()
                try await shoot("dsr-phone-setup-\(way)-\(size)", height: 1500, width: width, type: type) {
                    NavigationStack {
                        DsrSetupPage(model: model)
                    }
                    .environment(\.setupLayoutProbe, probe)
                }
                let label = try #require(probe.frame("Disk", .label))
                let value = try #require(probe.frame("Disk", .value))
                // One line of "Disk", beside the menu, and the menu no wider than half the row.
                #expect(label.height < 2 * UIFont.preferredFont(forTextStyle: .body).lineHeight, "\(way) \(size)")
                #expect(label.width >= 28, "\(way) \(size): Disk is \(label.width) wide")
                #expect(value.width <= width * 0.6, "\(way) \(size): the menu is \(value.width) wide")
            }
        }
    }

    /// The screen walk (PR 808): the time sheet and Setup > Digital Spectrum
    /// Recorder (connected, an older Core, not connected) at each phone
    /// shape, written as `dsr-phone-walk-<page>-<shape>.png`. The pages are
    /// drawn whole, taller than the screen, so every group can be read.
    @Test(arguments: WalkShape.all)
    func pagesAtEachShape(_ shape: WalkShape) async throws {
        let width = shape.screen.width
        let (_, pan, _) = await DsrTests.pan()
        pan.openTimeSheet()
        try await shoot("dsr-phone-walk-time-\(shape)", height: DsrTimeSheet.height, tabBar: false, width: width,
                        type: shape.type) {
            DsrTimeSheet(model: pan)
        }
        for (name, version, connected) in [("setup", Int64(4), true), ("setup-older-core", Int64(3), true),
                                           ("setup-not-connected", Int64(4), false)] {
            let (_, model) = await DsrTests.setup(version: version, dsr: DsrTests.dsrPart(disk: "/media/ssd"),
                                                  disks: [DsrTests.sdCard, DsrTests.ssd], connected: connected)
            try await shoot("dsr-phone-walk-\(name)-\(shape)", height: 1500, width: width, type: shape.type,
                            sideways: shape.sideways) {
                NavigationStack {
                    DsrSetupPage(model: model)
                }
            }
        }
    }

    /// One page on an upright phone, over the Setup tab's bar, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat, tabBar: Bool = true, width: CGFloat = 402,
                                      type: DynamicTypeSize = .large, sideways: Bool = false,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: width, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            if tabBar {
                TabBar(selection: .constant(.setup), sideways: sideways)
            }
        }
        .background(ChromeColours.page)
        .preferredColorScheme(.dark)
        .dynamicTypeSize(type)
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
