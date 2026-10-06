// NereusSDR for iOS: the Core's display settings from its catalogue: the FFT size and Hz/bin written to the Core, the rest kept per pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// R-IOS-06, R-IOS-18, R-IOS-27 (link 7.4, `display`), D84: the Core's
/// station settings go out as `settings.write` and come back in the
/// subscription; the device ones stay on this phone for the pan; a Core
/// without `display` keeps the earlier behaviour with its rows greyed.
@Suite("The Core's display settings", .serialized)
@MainActor
struct CoreDisplayTests {
    typealias Rig = DisplaySheetTests.Rig

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

    /// The settings writes the rig has sent, as key and value.
    private func writes(_ rig: Rig) -> [(key: String, value: String)] {
        rig.recorded.settingsSent.compactMap { message in
            guard case .settingsWrite(let write) = message, case .utf8(let value)? = write.properties.first?.value else {
                return nil
            }
            return (write.key, value)
        }
    }

    /// The Core's echo of this phone's write.
    private func echo(_ rig: Rig, _ key: String, _ value: String) {
        rig.settings.apply(.settingsValue(LinkMessage.SettingsValue(
            key: key, origin: rig.settings.origin, properties: [.init(name: key, value: .utf8(value))])))
    }

    /// The Core's catalogue again without its `display`, as an older Core sends it.
    private func withoutDisplay(_ rig: Rig) async {
        guard case .text(let json)? = rig.mirror.object("catalog")?["json"],
              var object = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any] else {
            return
        }
        object.removeValue(forKey: "display")
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return
        }
        rig.mirror.apply(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(String(decoding: data, as: UTF8.self))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        _ = await settle { rig.main.catalogFeed.catalog?.display == nil }
    }

    @Test("a Core display timeout restores the latest authoritative option without replay")
    func timeoutRestoresLatestCore() async throws {
        let clock = TestLinkClock()
        let rig = try Rig(clock: clock)
        await quiet()
        let core = rig.main.coreDisplay
        let size = try #require(core.sizeControl)
        core.setSizeIndex(1)
        #expect(await settle { writes(rig).count == 1 })
        let latest = CoreDisplayModel.settingText(size.options[2].value)
        rig.settings.apply(.settingsValue(.init(key: CoreDisplayModel.fftSizeKey, origin: "another-phone",
                                                properties: [.init(name: CoreDisplayModel.fftSizeKey, value: .utf8(latest))])))
        await clock.advance(by: 4_999)
        #expect(core.sizeIndex == 1 && core.note == nil)
        await clock.advance(by: 1)
        #expect(await settle { core.note == PropertyWriteOutcome.notConfirmed.reason })
        #expect(core.sizeIndex == 2)
        #expect(rig.settings.isUnconfirmed(CoreDisplayModel.fftSizeKey))
        let newer = CoreDisplayModel.settingText(size.options[3].value)
        rig.settings.apply(.settingsValue(.init(key: CoreDisplayModel.fftSizeKey, origin: "another-phone",
                                                properties: [.init(name: CoreDisplayModel.fftSizeKey, value: .utf8(newer))])))
        #expect(await settle { core.sizeIndex == 3 })
        #expect(core.note == PropertyWriteOutcome.notConfirmed.reason && writes(rig).count == 1)
        rig.settings.apply(.settingsReject(.init(key: CoreDisplayModel.fftSizeKey, properties: [
            .init(name: CoreDisplayModel.fftSizeKey, value: .utf8(newer)),
        ], reason: "Current FFT refusal.")))
        #expect(await settle { core.note == "Current FFT refusal." })
        #expect(core.sizeIndex == 3 && writes(rig).count == 1)
    }

    @Test("the Core's groups come in the desktop's order, each marked with where it lives")
    func groups() async throws {
        let rig = try Rig()
        await quiet()
        let core = rig.main.coreDisplay
        #expect(core.available)
        let display = try #require(rig.main.catalogFeed.catalog?.display)
        #expect(core.groups.flatMap(\.controls) == display.controls)
        let tags = core.groups.map { CoreDisplayModel.tag(of: $0.controls) }
        // The FFT group is the Core's; the rendering group mixes FPS with
        // this pan's; the waterfall's are this phone's.
        #expect(tags.first == .core)
        #expect(tags.contains(.both) && tags.last == .thisPhone)
        #expect(core.groups.allSatisfy { !$0.title.isEmpty })
    }

    @Test("FFT size steps through the Core's sizes and is written to the Core, one write at a time")
    func fftSize() async throws {
        let rig = try Rig()
        await quiet()
        let core = rig.main.coreDisplay
        let size = try #require(core.sizeControl)
        #expect(core.sizeIndex == 0)
        #expect(core.sizeIndexRange?.max == Double(size.options.count - 1))
        core.setSizeIndex(1)
        #expect(await settle { writes(rig).count == 1 })
        #expect(writes(rig).last?.key == CoreDisplayModel.fftSizeKey)
        #expect(writes(rig).last?.value == CoreDisplayModel.settingText(size.options[1].value))
        #expect(core.sizeIndex == 1)
        // A second move before the Core answers waits, then its latest goes.
        core.setSizeIndex(2)
        core.setSizeIndex(3)
        await quiet()
        #expect(writes(rig).count == 1)
        echo(rig, CoreDisplayModel.fftSizeKey, CoreDisplayModel.settingText(size.options[1].value))
        #expect(await settle { writes(rig).count == 2 })
        #expect(writes(rig).last?.value == CoreDisplayModel.settingText(size.options[3].value))
        #expect(core.sizeOption(at: 99)?.value == size.options.last?.value)
    }

    @Test("bin diagnostic text carries only bounded numbers and outcome tokens")
    func binDiagnosticPrivacy() {
        let outcome = DisplayBinDiagnostic.outcome(.rejected(reason: "private Core refusal"))
        let record = DisplayBinDiagnostic(stage: .writeOutcome, field: .target, value: .infinity,
                                          rateHz: .nan, spanHz: 1e20, widthHz: -20, outcome: outcome)
        #expect(record.logText.contains("stage=writeOutcome field=target value=-1"))
        #expect(record.logText.contains("rate=-1 span=-1"))
        #expect(record.logText.contains("width=-1"))
        #expect(record.logText.contains("outcome=rejected"))
        #expect(!record.logText.contains("private"))
    }

    @Test("a late answer keeps its sequence after a newer different-key note owner")
    func binDiagnosticLateNewerNoteOwner() async throws {
        let clock = TestLinkClock()
        let rig = try Rig(clock: clock)
        await quiet()
        let core = rig.main.coreDisplay
        var records: [DisplayBinDiagnostic] = []
        core.binDiagnostic = { records.append($0) }
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { writes(rig).count == 1 })
        let first = try #require(records.last { $0.stage == .writeAttempt }?.edit)
        await clock.advance(by: 5_000)
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.edit == first && $0.outcome == .notConfirmed } })
        core.setSizeIndex(1)
        #expect(await settle { writes(rig).count == 2 })
        let sizeWrite = try #require(writes(rig).last)
        #expect(sizeWrite.key == CoreDisplayModel.fftSizeKey)
        let second = try #require(records.last { $0.stage == .writeAttempt && $0.field == .size }?.edit)
        #expect(second > first)
        echo(rig, CoreDisplayModel.hzPerBinKey, "5")
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.edit == first && $0.outcome == .accepted && $0.current == false } })
        echo(rig, sizeWrite.key, sizeWrite.value)
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.edit == second && $0.outcome == .accepted && $0.current == true } })
        #expect(writes(rig).count == 2 && rig.sent.isEmpty)
    }

    @Test("an expired same-key answer is suppressed after a newer same-value admission")
    func binDiagnosticExpiredSameKeySuppressed() async throws {
        let clock = TestLinkClock()
        let rig = try Rig(clock: clock)
        await quiet()
        let core = rig.main.coreDisplay
        var records: [DisplayBinDiagnostic] = []
        core.binDiagnostic = { records.append($0) }
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { writes(rig).count == 1 })
        let first = try #require(records.last { $0.stage == .writeAttempt }?.edit)
        await clock.advance(by: 5_000)
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.edit == first && $0.outcome == .notConfirmed } })
        core.set(target, to: 5)
        #expect(await settle { writes(rig).count == 2 })
        let second = try #require(records.last { $0.stage == .writeAttempt }?.edit)
        #expect(second > first)
        echo(rig, CoreDisplayModel.hzPerBinKey, "5")
        #expect(!records.contains { $0.stage == .writeOutcome && $0.edit == first && $0.outcome == .accepted })
        #expect(!records.contains { $0.stage == .writeOutcome && $0.edit == second && $0.outcome == .accepted })
        echo(rig, CoreDisplayModel.hzPerBinKey, "5")
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.edit == second && $0.outcome == .accepted && $0.current == true } })
        #expect(!records.contains { $0.stage == .writeOutcome && $0.edit == first && $0.outcome == .accepted })
        #expect(writes(rig).count == 2 && rig.sent.isEmpty)
    }

    @Test("readout diagnostic source follows the valid grant and finite planned fallback")
    func binDiagnosticReadoutSource() {
        #expect(DisplayBinDiagnostic.readoutSource(granted: 23.4375, plannedRate: nil, plannedSize: nil) == .granted)
        for grant in [Double.nan, Double.infinity, -1, 0] {
            #expect(DisplayBinDiagnostic.readoutSource(granted: grant, plannedRate: 192_000, plannedSize: 4096) == .planned)
        }
        #expect(DisplayBinDiagnostic.readoutSource(granted: nil, plannedRate: .infinity, plannedSize: 4096) == .absent)
        #expect(DisplayBinDiagnostic.readoutSource(granted: nil, plannedRate: 192_000, plannedSize: 0) == .absent)
        #expect(DisplayBinDiagnostic.readoutSource(granted: nil, plannedRate: nil, plannedSize: 4096) == .absent)
    }

    @Test("bin diagnostics trace the offer, echo, plan and subscription without extra writes")
    func binDiagnosticBoundaries() async throws {
        let rig = try Rig()
        await quiet()
        var records: [DisplayBinDiagnostic] = []
        let core = rig.main.coreDisplay
        core.binDiagnostic = { records.append($0) }
        rig.main.subscriber.binDiagnostic = { records.append($0) }
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { writes(rig).count == 1 && rig.sent.last?.fftSize == 65_536 })
        echo(rig, CoreDisplayModel.hzPerBinKey, "5")
        #expect(await settle { records.contains { $0.stage == .writeOutcome && $0.outcome == .accepted } })
        #expect(records.contains { $0.stage == .offer && $0.field == .target && $0.value == 5 })
        #expect(records.contains { $0.stage == .proxyValue && $0.field == .target && $0.value == 5 })
        #expect(records.contains { $0.stage == .plan && $0.value == 5 && $0.fftSize == 65_536 && $0.floorSize == 4096 && $0.rateHz == 192_000 })
        let sent = try #require(rig.sent.last)
        #expect(records.contains { $0.stage == .subscription && $0.fftSize == sent.fftSize && $0.revision == sent.revision })
        await quiet()
        let before = records.count
        rig.main.subscriber.replan()
        rig.main.subscriber.replan()
        await quiet()
        #expect(records.count == before)
        #expect(writes(rig).count == 1 && rig.sent.count == 2)
    }

    @Test("the subscription asks for the Size setting as its floor, and the Hz/bin target holds the bin width")
    func subscriptionFollowsTheCore() async throws {
        let rig = try Rig()
        await quiet()
        let core = rig.main.coreDisplay
        #expect(await settle { core.binWidthText == "46.875" })
        #expect(rig.sent.isEmpty)
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        let first = try #require(rig.sent.last)
        #expect(first.fftSize == 4096 && first.tier == .wide)
        // The bin width before the Core grants a size: the pan's rate over the planned size.
        #expect(core.binWidthText == "46.875")
        #expect(core.binWidthLabel == rig.main.catalogFeed.catalog?.display?.binWidth.label)
        // Hz/bin target 5: 192 kHz / 5 needs 65536 bins, finer than the floor.
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { writes(rig).last?.key == CoreDisplayModel.hzPerBinKey })
        #expect(writes(rig).last?.value == "5")
        #expect(await settle { rig.sent.last?.fftSize == 65_536 })
        #expect(rig.sent.last?.tier == .fine)
        #expect(core.binWidthText == "2.930")
        // A value between steps is kept on the Core's step; Off is 0.
        echo(rig, CoreDisplayModel.hzPerBinKey, "5")
        core.set(target, to: 12.3)
        #expect(await settle { writes(rig).last?.value == "12.5" })
        #expect(target.text(0) == target.offLabel)
        // Window and FPS go to the Core. Full mode caps the phone's
        // effective subscription at 30 fps even when Core FPS is higher.
        let window = try #require(core.control(CoreDisplayModel.windowKey))
        core.set(window, to: window.options[2].value)
        #expect(await settle { writes(rig).last?.key == CoreDisplayModel.windowKey })
        #expect(await settle { rig.sent.last?.windowType == Int(window.options[2].value) })
        let fps = try #require(core.control(CoreDisplayModel.fpsKey))
        core.set(fps, to: 45)
        #expect(await settle { writes(rig).last?.key == CoreDisplayModel.fpsKey && writes(rig).last?.value == "45" })
        echo(rig, CoreDisplayModel.fpsKey, "45")
        #expect(await settle { rig.sent.last?.fps == 30 })
        core.set(fps, to: 15)
        #expect(await settle { writes(rig).last?.value == "15" })
        echo(rig, CoreDisplayModel.fpsKey, "15")
        #expect(await settle { rig.sent.last?.fps == 15 })
    }

    @Test("a valid Core FFT grant takes precedence over the current planned width")
    func grantedWidthTakesPrecedence() async throws {
        let rig = try Rig()
        let core = rig.main.coreDisplay
        #expect(await settle { core.binWidthText == "46.875" })
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        let request = try #require(rig.sent.last)
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("tests"),
            "endpointId": .number(Double(request.endpointId)), "revision": .number(Double(request.revision)),
            "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(request.centreHz), "sampleRateHz": .number(192_000),
            "centreHz": .number(request.centreHz), "spanHz": .number(request.spanHz),
            "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(Double(request.pixels)), "waterfallSamples": .number(Double(request.pixels)),
            "wideSamples": .number(0), "minDbm": .number(-160), "maxDbm": .number(0),
            "fps": .number(Double(request.fps)), "framesPerLine": .number(Double(request.framesPerLine)),
            "grantedFftSize": .number(8_192), "grantedTier": .string("wide"),
            "requestedPixels": .number(Double(request.pixels)), "grantedPixels": .number(Double(request.pixels)),
            "limit": .string("none"),
        ]
        rig.main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: true))))
        #expect(core.binWidthText == "23.438")
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { rig.main.subscriber.plannedFftSize == 65_536 })
        #expect(core.binWidthText == "23.438")

        rig.main.receive(.mediaState(.closed))
        #expect(core.binWidthText == "2.930")
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 3 })
        #expect(core.binWidthText == "2.930")
        rig.main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: true))))
        #expect(core.binWidthText == "23.438")
        rig.main.receive(.mediaState(.failed))
        #expect(core.binWidthText == "2.930")
    }

    @Test("shared grant diagnostics keep the actual readout and add no writes")
    func binDiagnosticSharedGrant() async throws {
        let rig = try Rig()
        var diagnostics: [DisplayBinDiagnostic] = []
        rig.main.band.binDiagnostic = { diagnostics.append($0) }
        var readouts: [DisplayBinDiagnostic] = []
        let core = rig.main.coreDisplay
        core.binDiagnostic = { if $0.stage == .readout { readouts.append($0) } }
        #expect(await settle { core.binWidthText == "46.875" })
        rig.main.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        let request = try #require(rig.sent.last)
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("tests"),
            "endpointId": .number(Double(request.endpointId)), "revision": .number(Double(request.revision)),
            "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(request.centreHz), "sampleRateHz": .number(192_000),
            "centreHz": .number(request.centreHz), "spanHz": .number(request.spanHz),
            "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(Double(request.pixels)), "waterfallSamples": .number(Double(request.pixels)),
            "wideSamples": .number(0), "minDbm": .number(-160), "maxDbm": .number(0),
            "fps": .number(Double(request.fps)), "framesPerLine": .number(Double(request.framesPerLine)),
            "grantedFftSize": .number(8_192), "grantedTier": .string("wide"),
            "requestedPixels": .number(Double(request.pixels)), "grantedPixels": .number(Double(request.pixels)),
            "limit": .string("shared"),
        ]
        rig.main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: true))))
        #expect(core.binWidthText == "23.438")
        #expect(diagnostics.contains { $0.stage == .grant && $0.fftSize == 8_192 && $0.rateHz == 192_000 && $0.limit == .shared && $0.revision == request.revision && $0.widthHz == 23.4375 })
        let beforeDuplicate = diagnostics.count
        rig.main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: true))))
        #expect(diagnostics.count == beforeDuplicate)
        let target = try #require(core.hzPerBinControl)
        core.set(target, to: 5)
        #expect(await settle { rig.main.subscriber.plannedFftSize == 65_536 })
        #expect(core.binWidthText == "23.438")
        #expect(await settle { writes(rig).count == 1 && rig.sent.count == 2 })
        #expect(diagnostics.last?.widthHz == 23.4375)
        #expect(await settle { readouts.last?.widthHz == 23.438 && readouts.last?.widthSource == .granted })

    }

    @Test("the detectors, averaging and decimation are kept for the pan and go only in the subscription")
    func deviceControls() async throws {
        let rig = try Rig()
        await quiet()
        let core = rig.main.coreDisplay
        let display = try #require(rig.main.catalogFeed.catalog?.display)
        let detector = try #require(display.control(subscribe: CoreDisplayModel.Field.traceDetector))
        let rms = try #require(detector.options.last)
        core.set(detector, to: rms.value)
        #expect(rig.main.band.settings.spectrumDetector.rawValue == Int(rms.value))
        #expect(core.value(detector) == rms.value)
        let averaging = try #require(display.control(subscribe: CoreDisplayModel.Field.waterfallAveraging))
        core.set(averaging, to: averaging.options[1].value)
        #expect(rig.main.band.settings.waterfallAveraging.rawValue == Int(averaging.options[1].value))
        let time = try #require(display.control(subscribe: CoreDisplayModel.Field.traceAverageTime))
        core.set(time, to: 123)
        #expect(rig.main.band.settings.spectrumAverageTimeMs == 120)
        #expect(writes(rig).isEmpty)
        // Kept for the pan.
        let again = try Rig(suite: rig.suite)
        #expect(again.main.band.settings.spectrumDetector.rawValue == Int(rms.value))
        // This Core takes no decimation: greyed with the reason.
        let decimation = try #require(display.control(subscribe: CoreDisplayModel.Field.decimation))
        #expect(core.reason(decimation) == CatalogFeed.needsNewerCoreText)
        core.set(decimation, to: 4)
        #expect(rig.main.band.settings.decimation == 1)
    }

    @Test("without the Core's extras its averaging is greyed; the detectors still work")
    func olderExtras() async throws {
        let rig = try Rig(extras: 0)
        await quiet()
        let core = rig.main.coreDisplay
        let display = try #require(rig.main.catalogFeed.catalog?.display)
        let averaging = try #require(display.control(subscribe: CoreDisplayModel.Field.traceAveraging))
        #expect(core.reason(averaging) == CatalogFeed.needsNewerCoreText)
        let detector = try #require(display.control(subscribe: CoreDisplayModel.Field.waterfallDetector))
        #expect(core.reason(detector) == nil)
    }

    @Test("a Core without the description keeps the earlier behaviour, its rows greyed with the reason")
    func olderCore() async throws {
        let rig = try Rig()
        await quiet()
        await withoutDisplay(rig)
        let core = rig.main.coreDisplay
        #expect(!core.available)
        #expect(core.sizeControl == nil && core.hzPerBinControl == nil)
        #expect(core.sizeIndex == nil && core.sizeIndexRange == nil)
        core.setSizeIndex(3)
        await quiet()
        #expect(writes(rig).isEmpty)
        #expect(CoreDisplayModel.olderCoreText == "This Core can't change its display settings from a phone. Updating the Core may help.")
        // The subscription asks with the desktop's defaults, as before.
        rig.main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.count == 1 })
        #expect(rig.sent.last?.fftSize == BandSubscriber.defaultFftSize)
        #expect(rig.sent.last?.windowType == BandSubscriber.defaultWindowType)
        #expect(rig.sent.last?.fps == BandSubscriber.defaultFps)
        #expect(core.binWidthLabel == CoreDisplayModel.binWidthFallbackLabel)
    }

    @Test("a setting the Core refuses shows its words")
    func refused() async throws {
        let rig = try Rig()
        await quiet()
        let core = rig.main.coreDisplay
        let fps = try #require(core.control(CoreDisplayModel.fpsKey))
        core.set(fps, to: 50)
        #expect(await settle { writes(rig).count == 1 })
        rig.settings.apply(.settingsReject(LinkMessage.SettingsReject(
            key: CoreDisplayModel.fpsKey, properties: [], reason: "Not now.")))
        #expect(await settle { core.note == "Not now." })
    }

    @Test("the setting's text is the Core's: whole numbers without a point, others as short as they go")
    func settingText() {
        #expect(CoreDisplayModel.settingText(4096) == "4096")
        #expect(CoreDisplayModel.settingText(12.5) == "12.5")
        #expect(CoreDisplayModel.settingText(0) == "0")
        #expect(CoreDisplayModel.settingText(0.25) == "0.25")
    }

    @Test("the new words are plain and promise nothing to come")
    func plainWords() {
        for text in [CoreDisplayModel.olderCoreText, CoreDisplayModel.choiceNotSentText, CoreDisplayModel.refusedText,
                     CoreDisplayModel.binWidthFallbackLabel, DisplayOnThisPhonePage.coreNote] {
            let words = text.lowercased().split { !$0.isLetter }
            for promise in ["yet", "soon", "later", "coming", "build", "bench", "station", "capability", "grant"] {
                #expect(!words.contains(Substring(promise)), "\(text)")
            }
            #expect(!text.contains("\u{2014}"), "\(text)")
        }
        #expect(DisplayOnThisPhonePage.olderCoreRows.map(\.key) == CoreDisplayModel.stationKeys)
    }

    // MARK: D84

    @Test("sideways the band starts at 55 percent, upright at 40; each keeps its own, and upright carries over")
    func sidewaysShare() async throws {
        let rig = try Rig()
        await quiet()
        let display = rig.main.display
        #expect(display.spectrumHeight == 40)
        rig.main.setBandSideways(true)
        #expect(display.spectrumHeight == 55)
        #expect(rig.main.band.settings.spectrumShare == 0.55)
        display.setSpectrumHeight(60)
        #expect(rig.main.band.settings.spectrumSharePercentSideways == 60)
        #expect(rig.main.band.settings.spectrumSharePercent == 40)
        rig.main.setBandSideways(false)
        #expect(display.spectrumHeight == 40)
        display.setSpectrumHeight(33)
        // Kept for the pan; which way the band is turned is not.
        let again = try Rig(suite: rig.suite)
        #expect(!again.main.band.settings.sideways)
        #expect(again.main.display.spectrumHeight == 33)
        again.main.setBandSideways(true)
        #expect(again.main.display.spectrumHeight == 60)
    }
    @Test("display touches retired by a lost session never replay; a fresh snapshot touch survives", arguments: [false, true])
    func queuedDisplayTouchHasSnapshotOwner(reconnect: Bool) async throws {
        let rig = try Rig(clock: TestLinkClock())
        let core = rig.main.coreDisplay
        #expect(await settle { core.available })
        core.setSizeIndex(1)
        #expect(await settle { writes(rig).count == 1 })
        core.setSizeIndex(2)
        let prior = try #require(rig.settings.currentSnapshotIdentity)
        // Main actor events retire the old waiter without letting its
        // continuation run until the replacement touch has been queued.
        rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        if reconnect {
            rig.settings.handle(.stateChanged(.receivingSnapshot))
            rig.settings.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            rig.settings.apply(.settingsSnapshot(.init(properties: [])))
            rig.settings.handle(.stateChanged(.ready))
            #expect(rig.settings.currentSnapshotIdentity != prior)
            core.setSizeIndex(3)
            #expect(core.sizeIndex == 3)
        }
        #expect(await settle { reconnect ? writes(rig).count == 2 : core.sizeIndex != 2 })
        if reconnect {
            let size = try #require(core.sizeControl)
            #expect(writes(rig).map(\.value) == [CoreDisplayModel.settingText(size.options[1].value),
                                                   CoreDisplayModel.settingText(size.options[3].value)])
            #expect(core.sizeIndex == 3)
            #expect(core.note == nil)
            echo(rig, CoreDisplayModel.fftSizeKey, CoreDisplayModel.settingText(size.options[3].value))
        } else {
            #expect(writes(rig).count == 1)
            #expect(core.sizeIndex != 2)
            #expect(core.note == PropertyWriteOutcome.linkLost.reason)
        }
    }

    @Test("a late display refusal replaces its timeout hint with the Core's exact words")
    func lateDisplayRefusal() async throws {
        let clock = TestLinkClock()
        let rig = try Rig(clock: clock)
        let core = rig.main.coreDisplay
        #expect(await settle { core.available })
        core.setSizeIndex(1)
        #expect(await settle { writes(rig).count == 1 })
        await clock.advance(by: 5_000)
        #expect(await settle { core.note == PropertyWriteOutcome.notConfirmed.reason })
        let reason = "This FFT size is unavailable."
        rig.settings.apply(.settingsReject(.init(key: CoreDisplayModel.fftSizeKey, properties: [], reason: reason)))
        #expect(await settle { core.note == reason })
    }

    @Test("accessory and spot settings show their timeout and late refusal at the responsible note")
    func settingOwnersReceiveTimeoutAndLateRefusal() async throws {
        let clock = TestLinkClock()
        let rig = try Rig(clock: clock)
        let accessories = rig.main.accessories
        let key = AccessoriesModel.rfKitPollKey
        let task = Task { await accessories.writeSettingNow(.rfKit, key, "500") }
        #expect(await settle { writes(rig).count == 1 })
        await clock.advance(by: 5_000)
        #expect(await task.value == .notConfirmed)
        #expect(accessories.notes[.rfKit] == PropertyWriteOutcome.notConfirmed.reason)
        let reason = "This polling interval is unavailable."
        rig.settings.apply(.settingsReject(.init(key: key, properties: [], reason: reason)))
        #expect(await settle { accessories.notes[.rfKit] == reason })
        #expect(accessories.notes[.tunerGenius] == nil)

        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        let spots = SpotsModel(records: nil, mirror: rig.mirror, commands: nil, settings: rig.settings,
                               slices: rig.main.slices, catalogFeed: rig.main.catalogFeed,
                               phone: PhoneSettings(defaults: defaults))
        spots.write(SpotsModel.lifetimeKey, "120", for: nil)
        #expect(await settle { writes(rig).count == 2 })
        await clock.advance(by: 5_000)
        #expect(await settle { spots.displayNote == PropertyWriteOutcome.notConfirmed.reason })
        let spotReason = "This spot lifetime is unavailable."
        rig.settings.apply(.settingsReject(.init(key: SpotsModel.lifetimeKey, properties: [], reason: spotReason)))
        #expect(await settle { spots.displayNote == spotReason })
        #expect(spots.notes.isEmpty)
    }

}
