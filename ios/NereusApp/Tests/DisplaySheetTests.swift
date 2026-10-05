// NereusSDR for iOS: the Display sheet: palette and levels, fill and scale, the Core's extras, kept per pan, sent only when the Core computes them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// D73, R-IOS-11, R-IOS-27, D23: every Display change is kept for the pan on
/// this phone and redraws the band; only a change to what the Core computes
/// sends the subscription again. What the Core cannot compute is greyed.
@Suite("Display sheet", .serialized)
@MainActor
struct DisplaySheetTests {
    /// A main screen over a mirror fed by hand, its display operations recorded.
    @MainActor
    final class Rig {
        let mirror: MirrorStore
        let store: BandDisplaySettingsStore
        let settings: SettingsProxyClient
        let main: MainScreenModel
        let recorded = Recorded()
        let suite: String

        /// What the band's subscriber sent.
        @MainActor
        final class Recorded {
            var sent: [DisplaySubscription] = []
            var retunes: [UInt32] = []
            var settingsSent: [LinkMessage] = []
        }

        var sent: [DisplaySubscription] { recorded.sent }
        var retunes: [UInt32] { recorded.retunes }

        /// A Core at minor 11 with media, without the display budget (each
        /// request goes as it changes), with display extras at `extras`
        /// (0 for none) and the extended view when `wideband`. Without
        /// `marksActive`, a Core from before it followed its band plan: its
        /// catalogue marks no plan `active`, and its setting says which.
        init(extras: Int64 = 2, wideband: Bool = true, catalogue: Bool = true, planName: String? = nil,
             marksActive: Bool = true, clock: any LinkClock = SystemLinkClock(), suite: String = "DisplaySheetTests-\(UUID().uuidString)") throws {
            self.suite = suite
            let mirror = MirrorStore(send: { _ in }, clock: clock)
            mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
            mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
                .init(name: "remoteMediaVersion", value: .i64(1)),
                .init(name: "remoteWidebandDisplayVersion", value: .i64(wideband ? 1 : 0)),
                .init(name: "displayExtrasVersion", value: .i64(extras)),
                .init(name: "stationCatalogVersion", value: .i64(catalogue ? 1 : 0)),
            ])))
            mirror.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
                .init(ordinal: 11, name: "active", value: .bool(true)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
                .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
                .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            ])))
            if catalogue, let json = MainScreenTests.catalogueJSON().map({ marksActive ? $0 : Self.unmarked($0) }) {
                mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "catalog", className: "StationCatalog",
                                                                    properties: [
                    .init(ordinal: 0, name: "json", value: .utf8(json)),
                    .init(ordinal: 1, name: "revision", value: .i64(1)),
                ])))
            }
            mirror.apply(.snapshotComplete)
            let defaults = try #require(UserDefaults(suiteName: suite))
            store = BandDisplaySettingsStore(defaults: defaults)
            self.mirror = mirror
            let recorded = recorded
            settings = SettingsProxyClient(send: { message in
                await MainActor.run { recorded.settingsSent.append(message) }
            }, clock: clock)
            settings.handle(.stateChanged(.receivingSnapshot))
            settings.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            settings.apply(.settingsSnapshot(.init(properties: planName.map {
                [.init(name: DisplaySheetModel.bandPlanKey, value: .utf8($0))]
            } ?? [])))
            settings.handle(.stateChanged(.ready))
            main = MainScreenModel(mirror: mirror, settings: settings, commands: nil,
                                   operations: BandSubscriber.Operations(
                                       subscribe: { recorded.sent.append($0) }, unsubscribe: { _ in },
                                       retuneClarity: { recorded.retunes.append($0) }),
                                   displaySettings: store)
            _ = main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        }

        /// The catalogue with no plan's `active`, as an older Core sends it.
        static func unmarked(_ json: String) -> String {
            edited(json) { plan in plan.removeValue(forKey: "active") }
        }

        /// The Core moves its `active` mark to the plan named `name`, one
        /// revision on, as it does when its plan changes (link 7.4).
        func markActive(_ name: String, revision: Int64) {
            guard case .text(let json)? = mirror.object("catalog")?["json"] else {
                return
            }
            let moved = Self.edited(json) { plan in plan["active"] = plan["name"] as? String == name }
            mirror.apply(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(moved)),
                .init(ordinal: 1, name: "revision", value: .i64(revision)),
            ])))
        }

        private static func edited(_ json: String, _ change: (inout [String: Any]) -> Void) -> String {
            guard var object = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any],
                  let plans = object["bandPlans"] as? [[String: Any]] else {
                return json
            }
            object["bandPlans"] = plans.map { plan -> [String: Any] in
                var plan = plan
                change(&plan)
                return plan
            }
            guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
                return json
            }
            return String(decoding: data, as: UTF8.self)
        }
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<3_000 {
            await Task.yield()
        }
    }

    // MARK: The waterfall

    @Test("Palette lists the Core's palettes by name and shows the current one")
    func palettes() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        let catalog = try #require(rig.main.catalogFeed.catalog)
        #expect(display.palettes.map(\.name) == catalog.palettes.map(\.name))
        #expect(display.paletteName == catalog.palettes.first { $0.id == 0 }?.name)
        let other = try #require(catalog.palettes.last)
        display.selectPalette(other.id)
        #expect(display.paletteName == other.name)
        #expect(rig.main.band.settings.waterfallPaletteId == other.id)
    }

    @Test("without a catalogue the palette menu is empty, greyed by the view")
    func noCatalogue() async throws {
        let rig = try Rig(catalogue: false)
        await quiet()
        #expect(rig.main.display.palettes.isEmpty)
        #expect(rig.main.display.paletteName == nil)
    }

    @Test("Clarity, Auto and Manual set the level mode, and the note follows; noise-floor AGC lights none")
    func levels() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.level == .clarity)
        #expect(display.levelNote == "Clarity sets the waterfall's levels from the noise floor.")
        display.select(.auto)
        #expect(rig.main.band.settings.waterfallLevelMode == .agc)
        #expect(display.levelNote == "Auto follows each line's weakest and strongest signals.")
        display.select(.manual)
        #expect(rig.main.band.settings.waterfallLevelMode == .manual)
        #expect(display.levelNote == "Manual uses the levels set in Setup.")
        rig.main.changeDisplay { $0.waterfallLevelMode = .noiseFloorAgc }
        #expect(display.level == nil)
        #expect(display.levelNote == nil)
    }

    @Test("without the Core's extras Clarity, Auto and the band's switches are greyed and the waterfall is manual")
    func olderCore() async throws {
        let rig = try Rig(extras: 0)
        await quiet()
        let display = rig.main.display
        #expect(!display.extrasAvailable)
        #expect(!display.isAvailable(.clarity))
        #expect(!display.isAvailable(.auto))
        #expect(display.isAvailable(.manual))
        #expect(display.level == .manual)
        display.select(.auto)
        #expect(rig.main.band.settings.waterfallLevelMode == .clarity)
        for feature in DisplaySheetModel.Feature.allCases {
            display.toggle(feature)
            #expect(!display.isOn(feature))
        }
        #expect(rig.main.band.settings == .desktopDefaults)
        #expect(!display.retuneAvailable)
    }

    @Test("Re-tune asks the Core for this pan's display endpoint only when the Core offers it, and shows its refusal")
    func retune() async throws {
        let older = try Rig(extras: 1)
        await quiet()
        #expect(!older.main.display.retuneAvailable)
        older.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { older.sent.count == 1 })
        older.main.display.retune()
        await quiet()
        #expect(older.retunes.isEmpty)

        let rig = try Rig(extras: 2)
        await quiet()
        let display = rig.main.display
        #expect(display.retuneAvailable)
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        display.retune()
        #expect(await settle { rig.retunes == [rig.sent[0].endpointId] })
        rig.main.receive(.clarityRetuneRefused(MediaControlEvent.Rejection(
            endpointId: rig.sent[0].endpointId, revision: rig.sent[0].revision, reason: "This display is not in Clarity.")))
        #expect(display.note == "This display is not in Clarity.")
        // Re-tune belongs to Clarity alone.
        display.select(.manual)
        display.retune()
        await quiet()
        #expect(rig.retunes.count == 1)
    }

    // MARK: The spectrum

    @Test("Fill, Top and Range set the fill and the scale within the desktop's ranges")
    func spectrum() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.fill == 70)
        #expect(display.top == -40)
        #expect(display.range == 100)
        #expect(display.rangeRange == .init(min: 1, max: 160, step: 1))
        display.setFill(40)
        #expect(rig.main.band.settings.traceFillOpacity == 0.4)
        display.setFill(140)
        #expect(display.fill == 100)
        // A new top keeps the range.
        display.setTop(-60)
        #expect(rig.main.band.settings.scaleTopDbm == -60)
        #expect(rig.main.band.settings.scaleBottomDbm == -160)
        display.setTop(20)
        #expect(display.top == 0)
        #expect(display.range == 100)
        display.setRange(500)
        #expect(display.range == 200)
        #expect(rig.main.band.settings.scaleBottomDbm == -200)
        display.setRange(0)
        #expect(display.range == 1)
        // A lower top shortens the range to keep the bottom at -200 dBm.
        display.setRange(150)
        display.setTop(-120)
        #expect(rig.main.band.settings.scaleBottomDbm == -200)
        #expect(display.range == 80)
    }

    @Test("Line sets the trace's width in whole screen pixels, from one pixel to 3 points, and is kept")
    func line() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.line == 0.5)
        #expect(DisplaySheetModel.lineText(display.line) == "0.5 pt")
        let range = DisplaySheetModel.lineRange(scale: 3)
        #expect(range.min == 1.0 / 3)
        #expect(range.max == 3)
        #expect(range.step == 1.0 / 3)
        display.setLine(0.1, scale: 3)
        #expect(rig.main.band.settings.traceWidthPoints == 1.0 / 3)
        #expect(DisplaySheetModel.lineText(display.line) == "0.33 pt")
        display.setLine(0.9, scale: 3)
        #expect(abs(rig.main.band.settings.traceWidthPoints - 1) < 1e-9)
        #expect(DisplaySheetModel.lineText(display.line) == "1 pt")
        display.setLine(9, scale: 3)
        #expect(rig.main.band.settings.traceWidthPoints == 3)
        display.setLine(0.5, scale: 2)
        #expect(rig.main.band.settings.traceWidthPoints == 0.5)
        let again = try Rig(suite: rig.suite)
        #expect(again.main.band.settings.traceWidthPoints == 0.5)
        display.setLine(2, scale: 3)
        let kept = try Rig(suite: rig.suite)
        #expect(abs(kept.main.band.settings.traceWidthPoints - 2) < 1e-9)
        // The band draws the kept width.
        #expect(abs(kept.main.band.overlays(scale: 3).settings.traceWidthPixels(scale: 3) - 6) < 1e-9)
    }

    // MARK: Kept, redrawn and sent

    @Test("every change is kept for the pan and survives a relaunch")
    func keptPerPan() async throws {
        let first = try Rig()
        await quiet()
        first.main.display.setFill(40)
        first.main.display.select(.auto)
        first.main.display.toggle(.peaks)
        let again = try Rig(suite: first.suite)
        #expect(again.main.band.settings.traceFillOpacity == 0.4)
        #expect(again.main.band.settings.waterfallLevelMode == .agc)
        #expect(again.main.band.settings.peakBlobs)
    }

    @Test("a change to what the Core computes sends the subscription again, and nothing else does")
    func onlyComputedChangesResend() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        // The phone's own drawing. (The scale's top and range now move the
        // quantisation window, as the desktop's do, so they are not here.)
        display.setFill(80)
        display.setLine(1, scale: 3)
        display.selectPalette(try #require(display.palettes.last).id)
        await quiet()
        #expect(rig.sent.count == 1)
        // What the Core computes.
        for (index, feature) in DisplaySheetModel.Feature.allCases.enumerated() {
            display.toggle(feature)
            #expect(await settle { rig.sent.count == 2 + index }, "\(feature)")
        }
        let last = try #require(rig.sent.last?.extras)
        #expect(last.activePeakHold?.enabled == true)
        #expect(last.peakBlobs != nil)
        #expect(last.noiseFloor?.enabled == true)
        display.select(.manual)
        #expect(await settle { rig.sent.count == 5 })
        #expect(rig.sent.last?.extras?.waterfallLevels?.mode == .manual)
    }

    // MARK: The band plan (D79)

    /// The `BandPlanName` writes the rig's settings proxy sent.
    private static func planWrites(_ rig: Rig) -> [LinkMessage.SettingsWrite] {
        rig.recorded.settingsSent.compactMap { message in
            if case .settingsWrite(let write) = message, write.key == DisplaySheetModel.bandPlanKey {
                return write
            }
            return nil
        }
    }

    @Test("Band plan lists the Core's plans in its order; a Core that marks none is followed by its setting, else the default")
    func planList() async throws {
        let rig = try Rig(marksActive: false)
        await quiet()
        let catalog = try #require(rig.main.catalogFeed.catalog)
        let display = rig.main.display
        #expect(display.plans.map(\.name) == catalog.bandPlans.map(\.name))
        #expect(display.shownPlan == catalog.defaultBandPlan)
        #expect(rig.main.band.shownPlan == catalog.defaultBandPlan)
        let other = try #require(catalog.bandPlans.first { !$0.isDefault })
        let named = try Rig(planName: other.name, marksActive: false)
        await quiet()
        #expect(named.main.display.shownPlan?.id == other.id)
        #expect(named.main.band.overlays(scale: 3).bandPlan?.id == other.id)
        // Another device changes the Core's plan: the band follows.
        named.settings.apply(.settingsValue(LinkMessage.SettingsValue(key: DisplaySheetModel.bandPlanKey, origin: "",
                                                                      properties: [])))
        #expect(await settle { named.main.display.shownPlan == catalog.defaultBandPlan })
    }

    @Test("from a Core that marks none, picking a plan sends one settings.write of its name, and the tick moves with the echo")
    func pickPlan() async throws {
        let rig = try Rig(marksActive: false)
        await quiet()
        let catalog = try #require(rig.main.catalogFeed.catalog)
        let display = rig.main.display
        let usual = try #require(catalog.defaultBandPlan)
        let other = try #require(catalog.bandPlans.first { !$0.isDefault })
        display.pickPlan(other)
        #expect(await settle { Self.planWrites(rig).count == 1 })
        let write = try #require(Self.planWrites(rig).first)
        #expect(write.properties == [.init(name: DisplaySheetModel.bandPlanKey, value: .utf8(other.name))])
        #expect(write.origin == rig.settings.origin)
        // Until the Core answers, the tick and the band stay on the Core's plan.
        await quiet()
        #expect(display.shownPlan?.id == usual.id)
        #expect(rig.main.band.shownPlan?.id == usual.id)
        rig.settings.apply(.settingsValue(LinkMessage.SettingsValue(key: write.key, origin: write.origin,
                                                                    properties: write.properties)))
        #expect(await settle { display.shownPlan?.id == other.id })
        #expect(rig.main.band.shownPlan?.id == other.id)
        #expect(display.planNote == nil)
        // The plan showing sends nothing; nor does a plan the Core does not have.
        display.pickPlan(other)
        await quiet()
        #expect(Self.planWrites(rig).count == 1)
        // The size is this phone's alone and sends nothing.
        display.setBandPlanSize(.huge)
        await quiet()
        #expect(rig.recorded.settingsSent.count == 1)
    }

    @Test("a Core that marks its plan active is followed: its setting alone moves nothing, its catalogue's mark does")
    func pickPlanActive() async throws {
        let rig = try Rig(planName: "IARU Region 1")
        await quiet()
        let catalog = try #require(rig.main.catalogFeed.catalog)
        let display = rig.main.display
        let active = try #require(catalog.bandPlans.first { $0.isActive })
        // The mark wins over a setting that names another plan.
        #expect(display.shownPlan?.id == active.id)
        #expect(rig.main.band.shownPlan?.id == active.id)
        let other = try #require(catalog.bandPlans.first { !$0.isActive })
        display.pickPlan(other)
        #expect(await settle { Self.planWrites(rig).count == 1 })
        let write = try #require(Self.planWrites(rig).first)
        #expect(write.properties == [.init(name: DisplaySheetModel.bandPlanKey, value: .utf8(other.name))])
        // The echo alone leaves the tick on the Core's plan.
        rig.settings.apply(.settingsValue(LinkMessage.SettingsValue(key: write.key, origin: write.origin,
                                                                    properties: write.properties)))
        await quiet()
        #expect(display.shownPlan?.id == active.id)
        // The catalogue's mark moves, and so do the tick and the band.
        rig.markActive(other.name, revision: 2)
        #expect(await settle { display.shownPlan?.id == other.id })
        #expect(rig.main.band.shownPlan?.id == other.id)
        #expect(display.planNote == nil)
        display.pickPlan(other)
        await quiet()
        #expect(Self.planWrites(rig).count == 1)
    }

    @Test("a refused plan shows the Core's words and leaves the tick")
    func planRefused() async throws {
        let rig = try Rig()
        await quiet()
        let catalog = try #require(rig.main.catalogFeed.catalog)
        let display = rig.main.display
        let usual = try #require(catalog.defaultBandPlan)
        let other = try #require(catalog.bandPlans.first { !$0.isDefault })
        display.pickPlan(other)
        #expect(await settle { Self.planWrites(rig).count == 1 })
        let reason = "This Core does not have that band plan."
        rig.settings.apply(.settingsReject(LinkMessage.SettingsReject(key: DisplaySheetModel.bandPlanKey, properties: [],
                                                                      reason: reason)))
        #expect(await settle { display.planNote == reason })
        #expect(display.shownPlan?.id == usual.id)
        #expect(rig.main.band.shownPlan?.id == usual.id)
        // Picking again starts afresh.
        display.pickPlan(other)
        #expect(display.planNote == nil)
    }

    // MARK: Desktop parity (Task 54e)

    @Test("Colour gain and Black level start at the desktop's 45 and 104, stay in range, are kept, and colour each new line")
    func colourGainAndBlackLevel() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.colorGain == 45)
        #expect(display.blackLevel == 104)
        #expect(rig.main.band.state.levelAdjustment == .init(colorGain: 45, blackLevel: 104))
        display.setColorGain(200)
        #expect(display.colorGain == 100)
        display.setBlackLevel(-5)
        #expect(display.blackLevel == 0)
        display.setColorGain(20)
        display.setBlackLevel(90)
        #expect(rig.main.band.state.levelAdjustment == .init(colorGain: 20, blackLevel: 90))
        let again = try Rig(suite: rig.suite)
        #expect(again.main.display.colorGain == 20)
        #expect(again.main.display.blackLevel == 90)
        // The phone's own drawing: nothing goes to the Core.
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        display.setColorGain(60)
        await quiet()
        #expect(rig.sent.count == 1)
    }

    @Test("Fill switches the fill under the trace and greys its strength, kept for the pan")
    func fillSwitch() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.fillOn)
        display.toggleFill()
        #expect(!rig.main.band.settings.traceFill)
        let again = try Rig(suite: rig.suite)
        #expect(!again.main.display.fillOn)
        display.toggleFill()
        #expect(rig.main.band.settings.traceFill)
    }

    @Test("Spectrum height starts at the desktop's 40 percent, stays within 20 and 80, and is kept for the pan")
    func spectrumHeight() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.spectrumHeight == 40)
        #expect(DisplaySheetModel.percentText(display.spectrumHeight) == "40%")
        display.setSpectrumHeight(62.4)
        #expect(rig.main.band.settings.spectrumSharePercent == 62)
        display.setSpectrumHeight(5)
        #expect(display.spectrumHeight == 20)
        display.setSpectrumHeight(99)
        #expect(display.spectrumHeight == 80)
        let again = try Rig(suite: rig.suite)
        #expect(again.main.display.spectrumHeight == 80)
        // The band's layout follows it.
        let size = CGSize(width: 402, height: 700)
        let layout = try axes(again, size: size).layout
        #expect(layout.spectrum.height == ((700 - BandLayout.scaleHeightPoints) * 0.8).rounded())
    }

    @Test("the dBm scale's arrows move the top 10 dB and keep the bottom, greyed at their ends")
    func scaleArrows() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.top == -40)
        display.nudgeTop(up: true)
        #expect(rig.main.band.settings.scaleTopDbm == -30)
        #expect(rig.main.band.settings.scaleBottomDbm == -140)
        display.nudgeTop(up: false)
        display.nudgeTop(up: false)
        #expect(rig.main.band.settings.scaleTopDbm == -50)
        #expect(rig.main.band.settings.scaleBottomDbm == -140)
        // At 0 dBm ▲ goes no further.
        display.setTop(0)
        #expect(!display.canRaiseTop)
        display.nudgeTop(up: true)
        #expect(display.top == 0)
        // ▼ stops 1 dB over the bottom.
        display.setRange(15)
        display.nudgeTop(up: false)
        display.nudgeTop(up: false)
        #expect(display.top == rig.main.band.settings.scaleBottomDbm + 1)
        #expect(!display.canLowerTop)
        #expect(DbmScaleArrows.raiseLabel == "Raise the scale's top 10 dB")
        #expect(DbmScaleArrows.lowerLabel == "Lower the scale's top 10 dB")
    }

    /// The band's parts and axes in points at `size`, after the Core's first
    /// context, so the band has a view.
    private func axes(_ rig: Rig, size: CGSize) throws -> (layout: BandLayout, geometry: BandGeometry) {
        if rig.main.band.spanHz == 0 {
            rig.main.band.endpointId = 1
            rig.main.band.receive(.context(try #require(TuningShotTests.context(centreHz: 7_236_400))))
        }
        return try #require(rig.main.band.pointGeometry(size: size))
    }

    /// One drag on the band of `size` points, from `start` through
    /// (`across`, `down`) steps, then the finger lifts.
    private func drag(_ rig: Rig, size: CGSize, from start: CGPoint, through steps: [(CGFloat, CGFloat)]) throws {
        let parts = try axes(rig, size: size)
        let dragger = BandDrag()
        for (across, down) in steps {
            dragger.changed(from: start, translation: across, rise: down, geometry: parts.geometry,
                            entries: rig.main.slices.entries, placements: [], band: rig.main.band,
                            slices: rig.main.slices, dragToTune: true, layout: parts.layout, display: rig.main.display)
        }
        dragger.ended(slices: rig.main.slices)
    }

    @Test("a drag up or down on the frequency-scale row moves the split, smoothly, within 20 and 80 percent, kept")
    func splitDrag() async throws {
        let rig = try Rig()
        await quiet()
        let size = CGSize(width: 402, height: 718)
        let layout = try axes(rig, size: size).layout
        let shared = size.height - BandLayout.scaleHeightPoints
        let row = CGPoint(x: 200, y: layout.frequencyScale.midY)
        let centre = rig.main.band.view.centerHz
        // 70 points down: the spectrum takes 70 more points of the 700 shared.
        try drag(rig, size: size, from: row, through: [(1, 8), (2, 70)])
        #expect(abs(rig.main.band.settings.spectrumSharePercent - (40 + 70 / Double(shared) * 100)) < 1e-9)
        #expect(rig.main.band.view.centerHz == centre, "the band did not pan")
        // Far up, it stops at 20 percent; far down, at 80. The row has moved with the split.
        let lowered = try axes(rig, size: size).layout
        #expect(lowered.frequencyScale.minY > layout.frequencyScale.minY + 60)
        try drag(rig, size: size, from: CGPoint(x: 200, y: lowered.frequencyScale.midY), through: [(0, -900)])
        #expect(rig.main.band.settings.spectrumSharePercent == 20)
        let moved = try axes(rig, size: size).layout
        try drag(rig, size: size, from: CGPoint(x: 100, y: moved.frequencyScale.midY), through: [(0, 900)])
        #expect(rig.main.band.settings.spectrumSharePercent == 80)
        let again = try Rig(suite: rig.suite)
        #expect(again.main.band.settings.spectrumSharePercent == 80)
    }

    @Test("a drag up or down on the dBm scale moves its top and bottom together; across it still pans the band")
    func scaleDrag() async throws {
        let rig = try Rig()
        await quiet()
        let size = CGSize(width: 402, height: 718)
        let parts = try axes(rig, size: size)
        let scale = parts.layout.dbmScale
        let point = CGPoint(x: scale.midX, y: parts.layout.dbmArrows.maxY + 30)
        // The scale follows the finger: 100 dB over the spectrum's height,
        // so a drag down of a fifth of it shows levels 20 dB higher.
        let height = parts.geometry.size.height
        try drag(rig, size: size, from: point, through: [(0, 10), (0, height / 5)])
        #expect(abs(rig.main.band.settings.scaleTopDbm - -20) < 1e-9)
        #expect(abs(rig.main.band.settings.scaleBottomDbm - -120) < 1e-9)
        // Past 0 dBm the range stays and the top stops.
        try drag(rig, size: size, from: point, through: [(0, height * 3)])
        #expect(rig.main.band.settings.scaleTopDbm == 0)
        #expect(rig.main.band.settings.scaleBottomDbm == -100)
        // On the arrows, a drag does not move the scale.
        try drag(rig, size: size, from: CGPoint(x: scale.midX, y: 10), through: [(0, 40)])
        #expect(rig.main.band.settings.scaleTopDbm == 0)
        // Across, from the same place, the band pans as it always did.
        let centre = rig.main.band.view.centerHz
        try drag(rig, size: size, from: point, through: [(-40, 2)])
        #expect(rig.main.band.view.centerHz > centre)
        #expect(rig.main.band.settings.scaleTopDbm == 0)
    }

    @Test("a drag starting on empty band still pans, whichever way the finger goes")
    func emptyBandStillPans() async throws {
        let rig = try Rig()
        await quiet()
        let size = CGSize(width: 402, height: 718)
        let parts = try axes(rig, size: size)
        let before = rig.main.band.settings
        let centre = rig.main.band.view.centerHz
        // In the waterfall, mostly down: the band pans by the across part only.
        try drag(rig, size: size, from: CGPoint(x: 30, y: parts.layout.waterfall.midY), through: [(-20, 60)])
        #expect(rig.main.band.view.centerHz > centre)
        #expect(rig.main.band.settings == before)
        // In the spectrum, away from the scale, the same.
        let panned = rig.main.band.view.centerHz
        try drag(rig, size: size, from: CGPoint(x: 30, y: parts.layout.spectrum.midY), through: [(20, -80)])
        #expect(rig.main.band.view.centerHz < panned)
        #expect(rig.main.band.settings == before)
    }

    @Test("each band keeps its own scale: the active slice moving to another band brings that band's back")
    func perBandScale() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        func slice(band: Int64) {
            rig.mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                .init(ordinal: 30, name: "band", value: .i64(band)),
            ])))
        }
        slice(band: 3)
        #expect(await settle { rig.main.band.settings.scaleBand == "3" })
        display.setTop(-60)
        slice(band: 7)
        #expect(await settle { rig.main.band.settings.scaleBand == "7" })
        #expect(display.top == -40)
        #expect(rig.main.band.settings.scaleBottomDbm == -140)
        display.setTop(-70)
        slice(band: 3)
        #expect(await settle { display.top == -60 })
        let again = try Rig(suite: rig.suite)
        #expect(again.main.band.settings.bandScales["7"]?.topDbm == -70)
    }

    @Test("Calibration offset is never sent to the Core, however it was kept")
    func calibrationNeverSent() async throws {
        let rig = try Rig()
        await quiet()
        rig.main.changeDisplay { $0.calibrationOffsetDb = -4 }
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        #expect(rig.sent[0].extras?.calibrationOffsetDb == nil)
        #expect(DisplayOnThisPhonePage.calibrationNote == "Calibration offset: The Core calibrates the display for its radio.")
    }

    @Test("Size sets the strip's size on this phone, kept for the pan")
    func planSize() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.bandPlanSize == .small)
        for size in BandPlanSize.allCases {
            display.setBandPlanSize(size)
            #expect(display.bandPlanSize == size)
            #expect(rig.main.band.settings.bandPlanStrip == (size != .off))
        }
        display.setBandPlanSize(.large)
        let again = try Rig(suite: rig.suite)
        #expect(again.main.display.bandPlanSize == .large)
        #expect(rig.recorded.settingsSent.isEmpty)
    }
}
