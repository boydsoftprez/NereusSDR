// NereusSDR for iOS: the 3D view on the phone: the sheet's rows, the arrows, the fallback, the data ask and an older Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-11, R-IOS-18, R-IOS-23; JJ's 3D View board (2026-09-29): the
/// Display sheet's View row and 3D rows, the dBm arrows' 3D Floor step, the
/// phone's fallback to 2D (could not keep up, Low Power Mode, heat), the
/// spectrum beside the pan asked for only while used, an older Core's
/// greyed choice and page, and Reset 3D to defaults.
@Suite("3D view", .serialized)
@MainActor
struct ThreeDViewTests {
    typealias Rig = DisplaySheetTests.Rig

    /// The rig's Core raised to one that offers the 3D view (description
    /// 12), or held below it.
    static func offer(_ rig: Rig, description: Int64 = 12, media: Int64 = 1) {
        rig.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "remoteMediaVersion", value: .i64(media)),
            .init(name: "remoteWidebandDisplayVersion", value: .i64(1)),
            .init(name: "displayExtrasVersion", value: .i64(2)),
            .init(name: "stationCatalogVersion", value: .i64(1)),
            .init(name: "setupDescriptionVersion", value: .i64(description)),
        ])))
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

    // MARK: The sheet (recommendation 1)

    @Test("the View row is always there; the five 3D rows and Reset show only in 3D; Fill and Top grey in 3D")
    func sheetRowsPerMode() async throws {
        let rig = try Rig()
        Self.offer(rig)
        let display = rig.main.display
        #expect(await settle { rig.main.band.stackOffered })
        #expect(display.stackOffered && display.viewNote == nil)
        #expect(!display.stackChosen && display.flatControlsAvailable)
        display.selectView(.stacked)
        #expect(rig.main.band.settings.spectrumView == .stacked)
        #expect(display.stackChosen && rig.main.band.drawsStack)
        #expect(!display.flatControlsAvailable)
        #expect(DisplaySheetModel.flatOnlyText == "Fill and Top shape the 2D trace only.")
        #expect(DisplaySheetModel.flatViewLabel == "2D Waterfall" && DisplaySheetModel.stackViewLabel == "3D Stacked Trace")
        #expect(display.stackFloor == 6 && display.stackGain == 70 && display.stackSpan == 100 && display.stackAngle == 50)
        display.setStackGain(95)
        display.setStackAngle(130)
        display.setStackFloor(-3)
        display.setSliceShadow(true)
        let settings = rig.main.band.settings
        #expect(settings.threeDGain == 95 && settings.threeDAngle == 100 && settings.threeDFloorDb == 0)
        #expect(settings.threeDSliceShadow)
        // Kept for the pan on this phone.
        let kept = rig.store.settings(forPan: BandSubscriber.panId)
        #expect(kept.spectrumView == .stacked && kept.threeDGain == 95)
        display.selectView(.flat)
        #expect(!display.stackChosen && display.flatControlsAvailable && !rig.main.band.drawsStack)
    }

    @Test("3D Floor names its band and each band keeps its own")
    func floorNamesItsBand() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let display = rig.main.display
        // The pan on the Core's band 3.
        rig.main.changeDisplay { $0.enterBand("3") }
        let band = try #require(rig.main.band.settings.scaleBand.flatMap(Int.init))
        #expect(display.floorBand == SetupDescription.bandNames[band])
        display.setStackFloor(14)
        rig.main.changeDisplay { $0.enterBand(String(band + 1)) }
        #expect(display.stackFloor == 6)
        rig.main.changeDisplay { $0.enterBand(String(band)) }
        #expect(display.stackFloor == 14)
        // 2 m, the Core's band 27, is named as the Core names it, for 3D
        // Floor and for Setup's per-band rows.
        rig.main.changeDisplay { $0.enterBand("27") }
        #expect(display.floorBand == "2m")
        #expect(PhoneSetupKeys(main: rig.main).perBandName == "2m")
        #expect(SetupDescription.bandName(13) == "XVTR" && SetupDescription.bandName(14) == nil)
    }

    // MARK: The arrows (recommendation 2)

    @Test("in 3D the dBm scale's arrows step 3D Floor by 1 dB, and say so")
    func arrowsStepTheFloor() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let display = rig.main.display
        let top = display.top
        #expect(!display.arrowsMoveFloor)
        display.selectView(.stacked)
        #expect(display.arrowsMoveFloor)
        display.nudgeFloor(up: true)
        #expect(rig.main.band.settings.threeDFloorDb == 7)
        display.nudgeFloor(up: false)
        display.nudgeFloor(up: false)
        #expect(rig.main.band.settings.threeDFloorDb == 5)
        #expect(display.top == top, "the scale's top stays")
        rig.main.changeDisplay { $0.threeDFloorDb = 24 }
        #expect(!display.canRaiseFloor && display.canLowerFloor)
        display.nudgeFloor(up: true)
        #expect(rig.main.band.settings.threeDFloorDb == 24)
        rig.main.changeDisplay { $0.threeDFloorDb = 0 }
        #expect(display.canRaiseFloor && !display.canLowerFloor)
        #expect(DbmScaleArrows.floorUpLabel == "3D Floor 1 dB deeper")
        #expect(DbmScaleArrows.floorDownLabel == "3D Floor 1 dB shallower")
    }

    @Test("in 3D a drag on the dBm scale moves 3D Floor as the arrows do, down deeper; in 2D it moves the scale")
    func scaleDragMovesTheFloor() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let band = rig.main.band
        let display = rig.main.display
        let layout = BandLayout(size: CGSize(width: 402, height: 640), scale: 1, settings: band.settings)
        let onScale = CGPoint(x: layout.dbmScale.midX, y: layout.dbmArrows.maxY + 20)
        // 2D: the scale.
        guard case .scale? = BandDrag.beginOnTheBandsParts(at: onScale, translation: 0, rise: 10, layout: layout,
                                                           display: display, band: band) else {
            Issue.record("a 2D drag on the scale should move the scale")
            return
        }
        display.selectView(.stacked)
        let target = BandDrag.beginOnTheBandsParts(at: onScale, translation: 0, rise: 10, layout: layout,
                                                   display: display, band: band)
        guard case .floor(let start, let dbPerPoint)? = target else {
            Issue.record("a 3D drag on the scale should move 3D Floor")
            return
        }
        #expect(start == 6 && dbPerPoint > 0)
        let top = display.top
        display.dragFloor(to: Double(start) + 40 * dbPerPoint)
        #expect(band.settings.threeDFloorDb > 6, "down is deeper, as the raise arrow")
        display.dragFloor(to: -50)
        #expect(band.settings.threeDFloorDb == 0)
        display.dragFloor(to: 99)
        #expect(band.settings.threeDFloorDb == 24)
        #expect(display.top == top, "the scale stays in 3D")
    }

    // MARK: The fallback (recommendation 3)

    @Test("a phone that cannot keep up draws every second frame, then shows 2D with the notice, and tries again")
    func fallback() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let band = rig.main.band
        var now = 1_000.0
        band.clock = { now }
        rig.main.display.selectView(.stacked)
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        // Slow frames: 80 ms each, under 20 a second.
        for _ in 0..<15 {
            now += 0.08
            band.noteStackFrame(drawSeconds: 0.08)
        }
        #expect(band.stackStage == .halfRate && band.drawsStack)
        #expect(band.stackDrawInterval == 2.0 / 30, "half the Core's 30 frames a second")
        // At half rate only every second frame redraws.
        let before = band.revision
        for sequence in 1...4 {
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
        }
        #expect(band.revision - before == 2)
        while band.stackStage != .fellBack && now < 1_020 {
            now += 0.16
            band.noteStackFrame(drawSeconds: 0.07)
        }
        #expect(band.stackStage == .fellBack)
        #expect(!band.drawsStack && band.stackHold == .cannotKeepUp)
        #expect(band.settings.spectrumView == .stacked, "3D stays the pan's choice")
        #expect(band.stackNotice == .cannotKeepUp)
        #expect(band.overlays(scale: 3).stacked == nil)
        band.dismissStackNotice()
        #expect(band.stackNotice == nil && !band.drawsStack)
        band.tryStackAgain()
        #expect(band.stackStage == .full && band.drawsStack && band.overlays(scale: 3).stacked != nil)
    }

    // MARK: Low Power Mode and heat (recommendation 4)

    @Test("Low Power Mode and a hot phone show 2D with the reason; 3D comes back after")
    func lowPowerAndHeat() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let band = rig.main.band
        rig.main.display.selectView(.stacked)
        band.stackConditions = .init(lowPower: true)
        #expect(!band.drawsStack && band.stackNotice == .lowPower)
        #expect(StackedTraceHold.lowPower.notice(pan: 1)
                == "Low Power Mode is on. 3D comes back when it is off, and stays your choice for Pan 1.")
        band.stackConditions = .init(hot: true)
        #expect(!band.drawsStack && band.stackNotice == .hot)
        band.stackConditions = .init()
        #expect(band.drawsStack && band.stackNotice == nil)
        // The session's rule: Low Power Mode, heat serious or critical with Slow the band when hot.
        let hot = SessionController.stackConditions(.init(heat: .serious), mode: .full)
        #expect(hot.hot && !hot.lowPower && !hot.savesData)
        #expect(!SessionController.stackConditions(.init(heat: .serious, hotPhoneSlows: false), mode: .full).hot)
        #expect(!SessionController.stackConditions(.init(heat: .fair), mode: .full).hot)
        #expect(SessionController.stackConditions(.init(lowPowerMode: true), mode: .saver).lowPower)
    }

    // MARK: The spectrum beside the pan (recommendation 5)

    @Test("the Core is asked for the wider spectrum only while 3D is drawn with Span over 0, and not while saving data")
    func spanRequestRule() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { !rig.sent.isEmpty })
        #expect(rig.sent.last?.wideSpanFactor == 0, "2D asks for none")
        rig.main.display.selectView(.stacked)
        #expect(await settle { rig.sent.last?.wideSpanFactor == StackedTrace.widestRowSpan })
        rig.main.display.setStackSpan(0)
        #expect(await settle { rig.sent.last?.wideSpanFactor == 0 })
        rig.main.display.setStackSpan(40)
        #expect(await settle { rig.sent.last?.wideSpanFactor == StackedTrace.widestRowSpan })
        rig.main.band.stackConditions = .init(savesData: true)
        #expect(await settle { rig.sent.last?.wideSpanFactor == 0 })
        #expect(rig.main.display.spanNote == "Span is off while the band saves data.")
        rig.main.band.stackConditions = .init()
        #expect(await settle { rig.sent.last?.wideSpanFactor == StackedTrace.widestRowSpan })
        #expect(rig.main.display.spanNote == nil)
        // Saver, or cellular without Full, saves data; Wi-Fi Full does not.
        #expect(SessionController.stackConditions(.init(), mode: .saver).savesData)
        #expect(SessionController.stackConditions(.init(network: .cellular, cellularMode: .balanced),
                                                  mode: .balanced).savesData)
        #expect(!SessionController.stackConditions(.init(network: .cellular, cellularMode: .full), mode: .full).savesData)
        #expect(!SessionController.stackConditions(.init(), mode: .full).savesData)
    }

    // MARK: An older Core (recommendation 7)

    @Test("a Core below description 12, or without the 3D gate, greys the View row and lists 3D View greyed")
    func olderCore() async throws {
        for (description, media) in [(Int64(11), Int64(1)), (Int64(12), Int64(0))] {
            let rig = try Rig()
            Self.offer(rig, description: description, media: media)
            await Task.yield()
            let display = rig.main.display
            #expect(!rig.main.band.stackOffered)
            #expect(display.viewNote == "This Core does not offer the 3D view. Updating the Core may help.")
            display.selectView(.stacked)
            #expect(rig.main.band.settings.spectrumView == .flat)
            // A 3D choice kept from another Core stays chosen and draws 2D, without the band's notice.
            rig.main.changeDisplay { $0.spectrumView = .stacked }
            #expect(rig.main.band.stackHold == .coreOlder && !rig.main.band.drawsStack)
            #expect(rig.main.band.stackNotice == nil)
        }
        #expect(MainScreenModel.offersStack { ["setupDescriptionVersion": 12, "remoteMediaVersion": 1][$0] ?? 0 })
        #expect(!MainScreenModel.offersStack { ["setupDescriptionVersion": 11, "remoteMediaVersion": 1][$0] ?? 0 })
        #expect(!MainScreenModel.offersStack { ["setupDescriptionVersion": 12][$0] ?? 0 })
    }

    @Test("Setup's Display list shows 3D View greyed with the reason on an older Core, and live on one that offers it")
    func setupListsTheThreeDPage() throws {
        let v11 = try SetupDescription.parse(json: FakeStation.syntheticSetup("display", title: "Spectrum Defaults",
                                                                               version: 11))
        let older = SetupTree.categories(described: ["display": v11], order: ["display"], unreadable: [:],
                                         stackOffered: false)
        let display = try #require(older.first { $0.title == "Display" })
        let entry = try #require(display.pages.last)
        #expect(entry.title == "3D View" && entry.tag == .thisPhone)
        #expect(entry.unavailableReason == "This Core does not offer the 3D view. Updating the Core may help.")
        // No description at all: nothing is listed for it.
        let none = SetupTree.categories(described: [:], order: [], unreadable: [:], stackOffered: false)
        #expect(!(none.first { $0.title == "Display" }?.pages.map(\.title).contains("3D View") ?? false))
    }

    // MARK: Reset and Setup's rows

    @Test("Reset 3D to defaults, from the sheet or Setup, puts the six rows back, 3D Floor for this band only")
    func reset() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        rig.main.changeDisplay { $0.enterBand("3") }
        let band = try #require(rig.main.band.settings.scaleBand.flatMap(Int.init))
        let other = String(band + 1)
        rig.main.changeDisplay { $0.enterBand(other); $0.threeDFloorDb = 20; $0.enterBand(String(band)) }
        let keys = PhoneSetupKeys(main: rig.main)
        for (key, value) in [("DisplaySpectrumRenderMode", SetupValue.integer(1)), ("Display3DFloorDepth", .integer(12)),
                             ("Display3DGain", .integer(85)), ("Display3DSpan", .integer(60)),
                             ("Display3DAngle", .integer(30)), ("Display3DSliceShadow", .bool(true))] {
            #expect(keys.set(value, forPhoneKey: key), "\(key)")
            #expect(keys.value(forPhoneKey: key) == value, "\(key)")
        }
        #expect(keys.canPerform("reset3d"))
        #expect(keys.perform("reset3d"))
        let settings = rig.main.band.settings
        #expect(settings.spectrumView == .flat && settings.threeDFloorDb == 6 && settings.threeDGain == 70)
        #expect(settings.threeDSpan == 100 && settings.threeDAngle == 50 && !settings.threeDSliceShadow)
        #expect(settings.threeDFloors[other] == 20)
        // The sheet's Reset does the same after Yes.
        rig.main.changeDisplay { $0.spectrumView = .stacked; $0.threeDGain = 10 }
        rig.main.display.resetStack()
        #expect(rig.main.band.settings.spectrumView == .flat && rig.main.band.settings.threeDGain == 70)
        #expect(DisplaySheetModel.resetStackQuestion == "This will restore Spectrum render mode, 3D Floor, 3D Gain, "
                + "3D Span, 3D Angle and 3D Slice Shadow to their ship defaults.\n\nContinue?")
        #expect(!keys.set(.integer(2), forPhoneKey: "DisplaySpectrumRenderMode"))
    }

    @Test("with Stop on TX the drawn stack pauses with the waterfall, and says so")
    func pausedWhileTransmitting() async throws {
        let rig = try Rig()
        Self.offer(rig)
        #expect(await settle { rig.main.band.stackOffered })
        let band = rig.main.band
        rig.main.display.selectView(.stacked)
        rig.main.changeDisplay { $0.waterfallStopOnTx = true }
        #expect(!band.stackPausedWhileTransmitting)
        band.keyed = true
        #expect(band.stackPausedWhileTransmitting && band.stackGlide(at: 1e9) == 0)
        band.keyed = false
        #expect(!band.stackPausedWhileTransmitting)
        #expect(StackNotice.pausedText == "Paused while transmitting")
    }

    @Test("no new words carry an em dash or the word yet")
    func words() {
        let words = [DisplaySheetModel.flatOnlyText, DisplaySheetModel.spanSavesDataText,
                     DisplaySheetModel.sliceShadowNote, DisplaySheetModel.resetStackQuestion,
                     StackedTraceHold.noticeTitle, StackNotice.tryAgainLabel, StackNotice.pausedText,
                     DbmScaleArrows.floorUpLabel, DbmScaleArrows.floorDownLabel]
            + [StackedTraceHold.coreOlder, .cannotKeepUp, .lowPower, .hot].map { $0.notice(pan: 1) }
        for text in words {
            #expect(!text.contains("\u{2014}") && !text.lowercased().split(separator: " ").contains("yet"), "\(text)")
        }
    }
}

/// Synthetic frames for the band (D4).
enum BandFixturesForApp {
    static func frame(sequence: UInt32, width: Int = 64) -> DisplayFrame {
        let line = [Float](repeating: -120, count: width)
        return DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: sequence,
                            producerTimestamp: UInt64(sequence) * 33_000_000, isKeyframe: sequence == 1,
                            waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: line, waterfallDbm: line,
                            wideDbm: [])
    }
}
