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

    /// For a simulator video of the motion (PR 808 screen walk): with
    /// `NEREUS_MOTION` set (`TEST_RUNNER_NEREUS_MOTION`), the past plays on a
    /// running clock for eight seconds, the Core answering each spectrum and
    /// rows ask, then a finger drags the strip for a second and a half.
    /// Without it the test does nothing.
    @Test func pastMotion() async throws {
        guard let wanted = ProcessInfo.processInfo.environment["NEREUS_MOTION"], !wanted.isEmpty else {
            return
        }
        // The small upright phone, so the whole pan fits a small simulator's screen.
        let small = WalkShape(small: true, sideways: false, larger: false)
        try await shootPan(.past, size: small.screen, type: .large, name: "dsr-phone-motion", motion: true)
    }

    /// Where the pan's slices stand for the PAST-on-the-flag pictures (PR 808
    /// part 2), on the shots' band of 7.2205 to 7.2685 MHz.
    enum FlagCase: String, CaseIterable, Sendable {
        /// The flag hangs right of its slice, at the band's left.
        case flagLeft = "flag-left"
        /// The flag hangs left of its slice, at the band's right.
        case flagRight = "flag-right"
        /// Two slices close together: one flag folds to its one-line tag.
        case folded
        /// The only slice is off this band: no flag, so the PAST tag shows on the spectrum.
        case noFlag = "no-flag"
        /// Two slices apart, each with its full flag.
        case twoSlices = "two-slices"

        /// Each slice's index and frequency; the first is active.
        var slices: [(Int, Double)] {
            switch self {
            case .flagLeft: return [(0, 7_224_000)]
            case .flagRight: return [(0, 7_265_000)]
            case .folded: return [(0, 7_236_400), (1, 7_238_000)]
            case .noFlag: return [(0, 7_300_000)]
            case .twoSlices: return [(0, 7_226_000), (1, 7_262_000)]
            }
        }
    }

    /// PAST and its time on every flag's top line and folded tag, and the
    /// PAST tag only where no flag shows (PR 808 part 2), on the small and
    /// the big iPhone upright, as `dsr-phone-past-on-<case>-<shape>.png`.
    @Test(arguments: FlagCase.allCases, [false, true])
    func pastOnTheFlag(_ flagCase: FlagCase, _ big: Bool) async throws {
        let shape = WalkShape(small: !big, sideways: false, larger: false)
        try await shootPan(.past, size: shape.pan, type: shape.type, name: "dsr-phone-past-on-\(flagCase.rawValue)-\(shape)",
                           slices: flagCase.slices) { model, band, _, _ in
            #expect(model.inPast)
            #expect(band.viewHeld())
        }
    }

    /// The band's view holds while in the past (PR 808 part 2, JJ's video:
    /// the scale jumped 5 kHz as the rewind retuned the slice): a new
    /// context centre and a retuned slice leave it where it was; live, it
    /// follows the Core again.
    @Test func theViewHoldsInThePast() async throws {
        let shape = WalkShape(small: true, sideways: false, larger: false)
        try await shootPan(.past, size: shape.pan, type: shape.type, name: "dsr-phone-view-held") { model, band, mirror, _ in
            let held = band.view
            mirror.apply(FlagButtonsShotTests.slice(0, hz: 7_237_400, active: true))
            band.receive(.context(try #require(BandFlagShotTests.context(centreHz: BandFlagShotTests.centre + 5_000))))
            #expect(band.view == held)
            #expect(await ShotWait.until { model.inPast })
            #expect(band.view == held)
            // A zoom asked in the past moves it.
            let zoomed = TuneGestures.View(centerHz: held.centerHz, spanHz: held.spanHz / 2)
            band.requestView(zoomed)
            #expect(band.view == zoomed)
        }
    }

    /// The past over a slow link (PR 808 part 2, JJ on cellular): the past
    /// plays for six seconds with each answer held back by `delayMs`. The
    /// spectrum keeps its last picture between answers (never empty once
    /// the first came), the view never moves, and "Drawing" never covers
    /// the picture. Prints how often the spectrum changed and the largest
    /// share of the picture still waiting.
    @Test(arguments: [100, 800])
    func thePastOverASlowLink(_ delayMs: Int) async throws {
        let shape = WalkShape(small: true, sideways: false, larger: false)
        try await shootPan(.past, size: shape.pan, type: shape.type,
                           name: "dsr-phone-slow-link-\(delayMs)ms") { model, band, _, rig in
            let geometry = try #require(band.pointGeometry(size: shape.pan)?.geometry)
            let base = model.clock()
            let start = CACurrentMediaTime()
            model.clock = { base + (CACurrentMediaTime() - start) }
            let delay = Double(delayMs) / 1000
            var seen: [Int: Double] = [:]
            var answered: Set<Int> = []
            let view = band.view
            var spectra = 0
            var emptySamples = 0
            var samples = 0
            var mostWaiting = 0.0
            var drawingShown = false
            var lastFrame: DsrPastFrames?
            var gaps: [Double] = []
            var lastChange = CACurrentMediaTime()
            while CACurrentMediaTime() < start + 6 {
                let now = CACurrentMediaTime()
                let all = rig.sent.invokes
                for index in 0..<all.count where !answered.contains(index) {
                    let asked = seen[index] ?? now
                    seen[index] = asked
                    guard now - asked >= delay else {
                        continue
                    }
                    answered.insert(index)
                    let invoke = all[index]
                    func number(_ name: String) -> Int64? {
                        invoke.args.first { $0.name == name }.flatMap { entry in
                            if case .i64(let value) = entry.value { return value }
                            return nil
                        }
                    }
                    switch invoke.verb {
                    case "dsr.spectrum":
                        let moment = Double(number("utcNs") ?? 0) / 1e9
                        await Flag.answer(rig, index, values: Self.frames([moment], geometry: geometry))
                    case "dsr.rows":
                        // As the Core answers: the rows asked for, from the newest down.
                        let to = Double(number("toUtcNs") ?? 0) / 1e9
                        let from = Double(number("fromUtcNs") ?? 0) / 1e9
                        let count = Int(min(number("rowCount") ?? 0, 600))
                        let spp = max(model.secondsPerPoint, 0.001)
                        let moments = (0..<count).map { to - Double($0) * spp }.filter { $0 >= from - spp / 2 }
                        let values = await Task.detached { Self.frames(moments, geometry: geometry) }.value
                        await Flag.answer(rig, index, values: values)
                    default:
                        break
                    }
                }
                samples += 1
                if model.pastSpectrum == nil {
                    emptySamples += 1
                }
                if let frame = model.pastSpectrum, frame.utcNs != lastFrame?.utcNs {
                    spectra += 1
                    gaps.append(now - lastChange)
                    lastChange = now
                    lastFrame = frame
                }
                mostWaiting = max(mostWaiting, model.missingShare)
                drawingShown = drawingShown || model.drawingText != nil
                #expect(band.view == view)
                try await Task.sleep(for: .milliseconds(20))
            }
            let longest = gaps.dropFirst().max() ?? 0
            print("slow link \(delayMs) ms: \(spectra) past spectra in 6 s, longest hold \(String(format: "%.2f", longest)) s, "
                  + "empty in \(emptySamples) of \(samples) samples, most waiting \(String(format: "%.2f", mostWaiting)), "
                  + "Drawing shown \(drawingShown)")
            #expect(emptySamples == 0)
            #expect(!drawingShown)
            #expect(spectra >= 2)
        }
    }

    private func shootPan(_ frame: PanState, size: CGSize, type: DynamicTypeSize, name: String,
                          motion: Bool = false, slices: [(Int, Double)]? = nil,
                          check: ((DsrPanModel, BandModel, MirrorStore, Flag.Rig) async throws -> Void)? = nil)
        async throws {
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
        for (index, hz) in slices ?? [(0, 7_236_400)] {
            mirror.apply(FlagButtonsShotTests.slice(index, hz: hz, active: index == 0))
        }
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        let (rig, model, _) = await DsrTests.pan(version: frame == .olderCore ? 3 : 4,
                                                 dsr: DsrTests.dsrPart(enabled: frame != .off))
        let band = main.band
        model.ownSeconds = { [weak band] in band?.mostLookBackSeconds ?? 0 }
        model.lookBack = { [weak band] seconds in band?.lookBack(seconds: seconds) }
        band.liveSpectrumHidden = { [weak model] in model?.hidesLiveSpectrum ?? false }
        // As AppModel wires it: the past spectrum drawn by the band's renderer.
        band.pastTrace = { [weak model] in model?.pastTrace }
        model.pastSpectrumChanged = { [weak band] in band?.pastTraceChanged() }
        band.viewHeld = { [weak model] in model?.inPast ?? false }
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        let sliceCount = (slices ?? [(0, 7_236_400)]).count
        #expect(await ShotWait.until { main.slices.entries.count == sliceCount && main.slices.catalog != nil })
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "DsrShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, sideways: size.width > size.height,
                             zoomBesideStrip: size.width > size.height,
                             flagControls: main.flagControls, dsr: model)
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
                model.toggleSave(stripPoints: model.waterfallPoints)
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
            // As SAVE's tap does: the bracket opens inside the strip.
            model.toggleSave(stripPoints: model.waterfallPoints)
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
        if motion {
            try await playPast(rig, model, geometry: geometry)
        }
        if let check {
            try await check(model, band, mirror, rig)
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: name)
    }

    /// The past playing on a running clock, then a finger on the strip (``pastMotion()``).
    private func playPast(_ rig: Flag.Rig, _ model: DsrPanModel, geometry: BandGeometry) async throws {
        let base = model.clock()
        let start = CACurrentMediaTime()
        model.clock = { base + (CACurrentMediaTime() - start) }
        var answered = rig.sent.invokes.count
        func answerAsks() async {
            let all = rig.sent.invokes
            for index in answered..<all.count {
                let moment = model.playedMoment ?? model.top
                switch all[index].verb {
                case "dsr.spectrum":
                    await Flag.answer(rig, index, values: Self.frames([moment], geometry: geometry))
                case "dsr.rows":
                    // As the Core answers: the rows asked for, from the newest down.
                    // (Rows around the played moment instead left the asked
                    // stretch's newest rows out, and the model does not ask twice.)
                    func number(_ name: String) -> Int64? {
                        all[index].args.first { $0.name == name }.flatMap { entry in
                            if case .i64(let value) = entry.value { return value }
                            return nil
                        }
                    }
                    let to = Double(number("toUtcNs") ?? 0) / 1e9
                    let from = Double(number("fromUtcNs") ?? 0) / 1e9
                    let count = Int(min(number("rowCount") ?? 0, 600))
                    let spp = max(model.secondsPerPoint, 0.001)
                    let moments = (0..<count).map { to - Double($0) * spp }.filter { $0 >= from - spp / 2 }
                    // Made off the main thread: in a debug build making the rows
                    // takes long enough to stall the picture the video watches.
                    let values = await Task.detached { Self.frames(moments, geometry: geometry) }.value
                    await Flag.answer(rig, index, values: values)
                default:
                    break
                }
            }
            answered = all.count
        }
        while CACurrentMediaTime() < start + 8 {
            await answerAsks()
            try await Task.sleep(for: .milliseconds(8))
        }
        let from = model.top
        for step in 1...90 {
            model.drag(from: from, by: Double(step) * 2)
            await answerAsks()
            try await Task.sleep(for: .milliseconds(16))
        }
        try await Task.sleep(for: .milliseconds(500))
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
    nonisolated static func frames(_ moments: [Double], geometry: BandGeometry, rowsLeft: Int = 0,
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
