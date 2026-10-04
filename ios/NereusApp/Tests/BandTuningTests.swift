// NereusSDR for iOS: tuning on the band: a drag pans or tunes, the flag's step menu, the number pad
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

/// D74, R-IOS-12, R-IOS-27: a drag from empty band moves the view and
/// writes no frequency; a drag from a flag or its passband tunes that slice
/// in its step; the step menu lists the Core's steps and writes the step;
/// the number pad reads MHz and kHz and shows the Core's refusal. The Core
/// here is a recording send closure; its catalogue is read from the link's
/// conformance suite at run time and never bundled (D4).
@Suite("Tuning on the band", .serialized)
@MainActor
struct BandTuningTests {
    typealias Outbox = BandSlicesModelTests.Outbox

    /// A mirror at minor 11 with property results, slice A (100 Hz step)
    /// and slice B, the catalogue, and a band showing 48 kHz about 7.2445 MHz
    /// from a 192 kHz receiver.
    @MainActor
    struct Rig {
        let store: MirrorStore
        let outbox: Outbox
        let commands: CommandClient
        let slices: BandSlicesModel
        let band: BandModel
        let tuning: BandTuningModel
        let geometry: BandGeometry

        static let size = CGSize(width: 402, height: 640)

        init(ctun: Int64 = 1, catalogue: Bool = true, lockedB: Bool = false, tuneStepsHz: [Int]? = nil) throws {
            let outbox = Outbox()
            let store = MirrorStore(send: { outbox.record($0) })
            store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
            store.apply(.capabilities(LinkMessage.Capabilities(properties: [
                .init(name: "propertyResultVersion", value: .i64(1)),
                .init(name: "remoteCtunVersion", value: .i64(ctun)),
            ])))
            store.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
            store.apply(Self.slice(0, hz: 7_236_400, active: true, step: 100))
            store.apply(Self.slice(1, hz: 7_255_000, active: false, step: 1_000, locked: lockedB))
            store.apply(.snapshotComplete)
            let commands = CommandClient(send: { outbox.record($0) })
            var catalog = catalogue ? BandFlagShotTests.catalogue() : nil
            if catalogue, let tuneStepsHz {
                // Scope this input to a supported Core step list while retaining
                // the fixture's other catalog fields and its labels for these steps.
                let json = try #require(MainScreenTests.catalogueJSON())
                var object = try #require(try JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
                let steps = try #require(object["tuneSteps"] as? [[String: Any]])
                object["tuneSteps"] = steps.filter { step in
                    guard let hz = step["hz"] as? Int else { return false }
                    return tuneStepsHz.contains(hz)
                }
                let scoped = try JSONSerialization.data(withJSONObject: object)
                catalog = try #require(StationCatalog.parse(json: String(decoding: scoped, as: UTF8.self)))
            }
            let slices = BandSlicesModel(store: store, commands: commands, catalog: catalog)
            let band = BandModel()
            band.catalog = catalog
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
            self.store = store
            self.outbox = outbox
            self.commands = commands
            self.slices = slices
            self.band = band
            tuning = BandTuningModel(slices: slices)
            geometry = try #require(band.pointGeometry(size: Self.size)).geometry
        }

        static func slice(_ index: Int64, hz: Double, active: Bool, step: Int64, locked: Bool = false) -> LinkMessage {
            .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(hz)),
                .init(ordinal: 2, name: "dspMode", value: .i64(0)),
                .init(ordinal: 3, name: "filterLow", value: .i64(-2_900)),
                .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
                .init(ordinal: 6, name: "stepHz", value: .i64(step)),
                .init(ordinal: 11, name: "active", value: .bool(active)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(index)),
                .init(ordinal: 24, name: "streamCtunPinned", value: .bool(false)),
                .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
                .init(ordinal: 35, name: "locked", value: .bool(locked)),
            ]))
        }

        var placements: [FlagPlacement] {
            FlagLayout.layout(slices: slices.entries.map(\.slice), activeSliceId: slices.activeSliceId,
                              geometry: geometry)
        }

        /// A drag from `start` across `translations` points, then the finger lifts.
        func drag(from start: CGPoint, through translations: [CGFloat], dragToTune: Bool = true,
                  haptics: DialHaptics = DialHaptics(), eachFrame: () -> Void = {}) {
            let dragger = BandDrag(haptics: haptics)
            for translation in translations {
                eachFrame()
                dragger.changed(from: start, translation: translation, geometry: geometry, entries: slices.entries,
                                placements: placements, band: band, slices: slices, dragToTune: dragToTune)
            }
            dragger.ended(slices: slices)
        }

        /// Answers the frequency write last sent for `key` as kept.
        func keepLastWrite() {
            guard let write = outbox.messages.reversed().compactMap({ message -> LinkMessage.PropertyWrite? in
                if case .propertyWrite(let write) = message { return write }
                return nil
            }).first, let writeId = write.writeId, let entry = write.properties.first else {
                return
            }
            store.apply(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId, results: [
                .init(property: entry.name, accepted: true, reason: "", value: entry),
            ])))
            store.apply(.delta(LinkMessage.Delta(key: write.key, properties: [entry])))
        }

        var commandInvokes: [LinkMessage.CommandInvoke] {
            outbox.messages.compactMap { message in
                if case .commandInvoke(let invoke) = message { return invoke }
                return nil
            }
        }
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    // MARK: Drags

    @Test("a drag from empty band moves the view by the dragged distance and writes no frequency")
    func panMovesTheView() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        // Far left of the spectrum, clear of both flags and passbands.
        let start = CGPoint(x: 6, y: 300)
        #expect(TuneGestures.dragTarget(at: start, slices: rig.slices.entries.map(\.slice), placements: rig.placements,
                                        activeSliceId: 0, geometry: rig.geometry, tunable: rig.slices.tunableIds) == .band)
        rig.drag(from: start, through: [10, 40, 80])
        let view = try #require(rig.band.requestedView)
        let expected = BandFlagShotTests.centre - 80 / Double(Rig.size.width) * BandFlagShotTests.span
        #expect(abs(view.centerHz - expected) < 0.001)
        #expect(view.spanHz == BandFlagShotTests.span)
        for _ in 0..<200 {
            await Task.yield()
        }
        #expect(rig.outbox.frequencies.isEmpty)
        #expect(rig.commandInvokes.isEmpty)
        // The slices keep their frequencies.
        #expect(rig.slices.entries.map(\.slice.frequencyHz) == [7_236_400, 7_255_000])
    }

    @Test("a pan past the receiver's window asks the Core to keep the window still and move it")
    func panPastTheWindowMovesIt() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        await rig.commands.handle(.stateChanged(.ready))
        // The view is 48 kHz of a 192 kHz window about 7.2445 MHz: 72 kHz of
        // room to the right. Dragging left 1.2 widths shows 57.6 kHz higher,
        // still inside; 2 widths (96 kHz) goes 24 kHz past.
        rig.drag(from: CGPoint(x: 6, y: 300), through: [-Rig.size.width * 2])
        #expect(await settle { rig.commandInvokes.count == 1 })
        let pin = try #require(rig.commandInvokes.first)
        #expect(pin.verb == "requestStreamCtunPinned")
        #expect(pin.args.map(\.name) == ["sliceId", "pinned"])
        #expect(pin.args.last?.value == .bool(true))
        await rig.commands.receive(.commandResult(.init(verb: pin.verb, id: pin.id, accepted: true, reason: "",
                                                        affected: [])))
        #expect(await settle { rig.commandInvokes.count == 2 })
        let centre = rig.commandInvokes[1]
        #expect(centre.verb == "requestStreamCentre")
        #expect(centre.args.first?.value == .i64(0))
        #expect(centre.args.last?.value == .f64(BandFlagShotTests.centre + 24_000))
        #expect(rig.band.requestedView?.centerHz == BandFlagShotTests.centre + 96_000)
        await rig.commands.receive(.commandResult(.init(verb: centre.verb, id: centre.id, accepted: true, reason: "",
                                                        affected: [])))
        #expect(rig.outbox.frequencies.isEmpty)
    }

    @Test("without the Core's window moves a pan stops at the receiver's window")
    func panStopsAtTheWindow() async throws {
        let rig = try Rig(ctun: 0)
        #expect(await settle { rig.slices.entries.count == 2 })
        rig.drag(from: CGPoint(x: 6, y: 300), through: [-Rig.size.width * 2])
        // The right edge of the view at the window's: 7.2445 + 0.096 - 0.024.
        #expect(rig.band.requestedView?.centerHz == BandFlagShotTests.centre + 72_000)
        for _ in 0..<200 {
            await Task.yield()
        }
        #expect(rig.commandInvokes.isEmpty)
    }

    @Test("a drag from a flag or its passband tunes that slice in its step, the last write the final place")
    func flagDragTunesInSteps() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let flag = rig.placements[0].rect
        let perPoint = BandFlagShotTests.span / Double(Rig.size.width)
        rig.drag(from: CGPoint(x: flag.midX, y: flag.midY), through: [3, 7, 12])
        // One write in flight at a time: the first, then the final value.
        #expect(await settle { rig.outbox.frequencies.count == 1 })
        rig.keepLastWrite()
        #expect(await settle { rig.outbox.frequencies.count == 2 })
        let final = TuneGestures.snapped(7_236_400 + 12 * perPoint, stepHz: 100)
        #expect(rig.outbox.frequencies.last == final)
        #expect(rig.outbox.frequencies.allSatisfy { $0.truncatingRemainder(dividingBy: 100) == 0 })
        rig.keepLastWrite()

        // Slice B's passband, down in the waterfall: B tunes in its 1 kHz step.
        let bx = rig.geometry.x(forHz: 7_255_000 - 1_500)
        let before = rig.outbox.frequencies.count
        rig.drag(from: CGPoint(x: bx, y: 600), through: [40])
        #expect(await settle { rig.outbox.frequencies.count == before + 1 })
        let write = try #require(rig.outbox.messages.reversed().compactMap { message -> LinkMessage.PropertyWrite? in
            if case .propertyWrite(let write) = message { return write }
            return nil
        }.first)
        #expect(write.key == "slice:1")
        #expect(rig.outbox.frequencies.last == TuneGestures.snapped(7_255_000 + 40 * perPoint, stepHz: 1_000))
        #expect(rig.band.requestedView == nil)
    }

    @Test("a flag drag ticks softly once per step it moves, at most once a frame; a pan drag never ticks")
    func flagDragTicks() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let recorder = DialTests.Recorder()
        let clock = DialTests.TestClock()
        let haptics = DialHaptics(generator: recorder, clock: { clock.now })
        let flag = rig.placements[0].rect
        let start = CGPoint(x: flag.midX, y: flag.midY)
        // Slice A's step is 100 Hz: five frames, each a step further, a
        // sixtieth of a second apart.
        let perStep = CGFloat(100 / (BandFlagShotTests.span / Double(Rig.size.width)))
        rig.drag(from: start, through: (1...5).map { CGFloat($0) * perStep }, haptics: haptics,
                 eachFrame: { clock.now += 1.0 / 60 })
        #expect(recorder.impacts == Array(repeating: .soft, count: 5))
        #expect(recorder.prepared >= 1)
        // Held still between frames: a frame that moves no step plays nothing.
        recorder.impacts.removeAll()
        rig.drag(from: start, through: [perStep * 0.1, perStep * 0.2, perStep * 0.3], haptics: haptics,
                 eachFrame: { clock.now += 1.0 / 60 })
        #expect(recorder.impacts.isEmpty)
        // At speed, frames at 120 a second each passing steps: one tick a sixtieth of a second.
        recorder.impacts.removeAll()
        rig.drag(from: start, through: (1...12).map { CGFloat($0) * perStep * 3 }, haptics: haptics,
                 eachFrame: { clock.now += 1.0 / 120 })
        #expect(recorder.impacts.count == 6)
        #expect(recorder.impacts.allSatisfy { $0 == .soft })

        // A pan from empty band moves the view and never ticks.
        recorder.impacts.removeAll()
        rig.drag(from: CGPoint(x: 6, y: 300), through: [10, 40, 80], haptics: haptics,
                 eachFrame: { clock.now += 1.0 / 60 })
        #expect(rig.band.requestedView != nil)
        #expect(recorder.impacts.isEmpty)
    }

    @Test("a drag on a slice this phone may not change, or with drag to tune off, moves the band")
    func untunableFlagPans() async throws {
        let rig = try Rig(lockedB: true)
        #expect(await settle { rig.slices.entries.count == 2 && rig.slices.entries[1].locked })
        let bx = rig.geometry.x(forHz: 7_255_000 - 1_500)
        rig.drag(from: CGPoint(x: bx, y: 600), through: [40])
        #expect(rig.band.requestedView != nil)
        let flag = rig.placements[0].rect
        rig.drag(from: CGPoint(x: flag.midX, y: flag.midY), through: [40], dragToTune: false)
        for _ in 0..<200 {
            await Task.yield()
        }
        #expect(rig.outbox.frequencies.isEmpty)
    }

    @Test("a tap still tunes the active slice at once")
    func tapTunesAtOnce() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.active != nil })
        rig.slices.tap(to: TuneGestures.tapped(atX: 100, geometry: rig.geometry, snap: true, stepHz: 100))
        #expect(await settle { rig.outbox.frequencies.count == 1 })
        #expect(rig.outbox.frequencies[0].truncatingRemainder(dividingBy: 100) == 0)
    }

    // MARK: The step

    @Test("the step menu lists the Core's steps with the slice's lit, and a pick writes the step")
    func stepMenuWritesTheStep() async throws {
        // Preserve the original six-step scenario independently of catalog expansion.
        let rig = try Rig(tuneStepsHz: [1, 10, 100, 500, 1_000, 10_000])
        #expect(await settle { rig.slices.entries.count == 2 })
        let entry = try #require(rig.slices.entries.first)
        #expect(rig.tuning.stepLabel(for: entry) == "100 Hz")
        #expect(rig.tuning.stepsAvailable)
        rig.tuning.openStepMenu(sliceId: 0)
        #expect(rig.tuning.stepMenuTitle(sliceId: 0) == "Step for slice A")
        #expect(rig.tuning.steps.map(\.label) == ["1 Hz", "10 Hz", "100 Hz", "500 Hz", "1 kHz", "10 kHz"])
        #expect(rig.tuning.steps.filter(rig.tuning.isCurrent).map(\.label) == ["100 Hz"])
        rig.tuning.pick(try #require(rig.tuning.steps.first { $0.label == "1 kHz" }))
        #expect(rig.tuning.stepMenuSliceId == nil)
        #expect(await settle {
            rig.outbox.messages.contains { message in
                guard case .propertyWrite(let write) = message else { return false }
                return write.key == "slice:0" && write.properties.first?.name == "stepHz"
                    && write.properties.first?.value == .i64(1_000)
            }
        })
    }

    @Test("the current Core catalog lists all 26 steps with the slice's lit, and a pick writes the step")
    func currentCoreStepMenuWritesTheStep() async throws {
        // Canonical catalog-anan-g2.json step list from Core commit
        // 0bfd282fb9bf4480dfb254e393c6a77a738a1d1a; read the unmodified fixture.
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        let entry = try #require(rig.slices.entries.first)
        #expect(rig.tuning.stepLabel(for: entry) == "100 Hz")
        #expect(rig.tuning.stepsAvailable)
        rig.tuning.openStepMenu(sliceId: 0)
        #expect(rig.tuning.stepMenuTitle(sliceId: 0) == "Step for slice A")
        #expect(rig.tuning.steps.map(\.hz) == [
            1, 2, 10, 25, 50, 100, 250, 500, 1_000, 2_000, 2_500, 5_000, 6_250,
            9_000, 10_000, 12_500, 15_000, 20_000, 25_000, 30_000, 50_000,
            100_000, 250_000, 500_000, 1_000_000, 10_000_000,
        ])
        #expect(rig.tuning.steps.map(\.label) == [
            "1 Hz", "2 Hz", "10 Hz", "25 Hz", "50 Hz", "100 Hz", "250 Hz", "500 Hz",
            "1 kHz", "2 kHz", "2.5 kHz", "5 kHz", "6.25 kHz", "9 kHz", "10 kHz",
            "12.5 kHz", "15 kHz", "20 kHz", "25 kHz", "30 kHz", "50 kHz", "100 kHz",
            "250 kHz", "500 kHz", "1 MHz", "10 MHz",
        ])
        #expect(rig.tuning.steps.filter(rig.tuning.isCurrent).map(\.label) == ["100 Hz"])
        rig.tuning.pick(try #require(rig.tuning.steps.first { $0.label == "1 kHz" }))
        #expect(rig.tuning.stepMenuSliceId == nil)
        #expect(await settle {
            rig.outbox.messages.contains { message in
                guard case .propertyWrite(let write) = message else { return false }
                return write.key == "slice:0" && write.properties.first?.name == "stepHz"
                    && write.properties.first?.value == .i64(1_000)
            }
        })
    }

    @Test("without the catalogue's steps the step button is greyed and its menu says so")
    func stepMenuNeedsANewerCore() async throws {
        let rig = try Rig(catalogue: false)
        #expect(await settle { rig.slices.entries.count == 2 })
        #expect(!rig.tuning.stepsAvailable)
        #expect(rig.tuning.steps.isEmpty)
        #expect(CatalogFeed.needsNewerCoreText == "Needs a newer Core")
    }

    // MARK: The number pad

    @Test("the pad reads MHz and kHz, and refuses an empty or non-number entry without sending")
    func padParses() async throws {
        #expect(FrequencyPadModel.hertz("7.074", unit: .megahertz) == 7_074_000)
        #expect(FrequencyPadModel.hertz("7074", unit: .kilohertz) == 7_074_000)
        #expect(FrequencyPadModel.hertz("14074.5", unit: .kilohertz) == 14_074_500)
        #expect(FrequencyPadModel.hertz("", unit: .megahertz) == nil)
        #expect(FrequencyPadModel.hertz(".", unit: .megahertz) == nil)
        #expect(FrequencyPadModel.hertz("7.0.74", unit: .megahertz) == nil)
        #expect(FrequencyPadModel.hertz("abc", unit: .kilohertz) == nil)

        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        rig.tuning.openPad(sliceId: 0)
        let pad = try #require(rig.tuning.pad)
        #expect(pad.letter == "A")
        #expect(pad.enterLabel == "Tune to")
        #expect(await pad.enter() == false)
        pad.press(.point)
        #expect(await pad.enter() == false)
        for _ in 0..<100 {
            await Task.yield()
        }
        #expect(rig.outbox.frequencies.isEmpty)
        #expect(rig.tuning.pad != nil)
        pad.press(.delete)
        for key: FrequencyPadModel.Key in [.digit(1), .digit(4), .digit(0), .digit(7), .digit(4), .point, .digit(5)] {
            pad.press(key)
        }
        pad.select(.kilohertz)
        #expect(pad.entry == "14074.5")
        #expect(pad.enterLabel == "Tune to 14.074.500")
    }

    @Test("Enter tunes the slice; the Core's refusal shows its words and the pad stays open")
    func padShowsTheCoresRefusal() async throws {
        let rig = try Rig()
        #expect(await settle { rig.slices.entries.count == 2 })
        rig.tuning.openPad(sliceId: 0)
        let pad = try #require(rig.tuning.pad)
        for key: FrequencyPadModel.Key in [.digit(9), .digit(9), .digit(9)] {
            pad.press(key)
        }
        let refused = Task { await pad.enter() }
        #expect(await settle { rig.outbox.frequencies == [999_000_000] })
        let write = try #require(rig.outbox.messages.compactMap { message -> LinkMessage.PropertyWrite? in
            if case .propertyWrite(let write) = message { return write }
            return nil
        }.last)
        let words = "That frequency is outside what this radio can receive."
        rig.store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: try #require(write.writeId),
                                                                   results: [
            .init(property: "frequency", accepted: false, reason: words, value: nil),
        ])))
        #expect(await refused.value == false)
        #expect(pad.refusal == words)
        #expect(rig.tuning.pad != nil)

        // A kept frequency closes the pad.
        for _ in 0..<3 {
            pad.press(.delete)
        }
        for key: FrequencyPadModel.Key in [.digit(7), .point, .digit(0), .digit(7), .digit(4)] {
            pad.press(key)
        }
        #expect(pad.refusal == nil)
        let kept = Task { await pad.enter() }
        #expect(await settle { rig.outbox.frequencies.last == 7_074_000 })
        rig.keepLastWrite()
        #expect(await kept.value)
        #expect(rig.tuning.pad == nil)
    }

    @Test("the pad does not open for a slice this phone may not change")
    func padOnlyForTunableSlices() async throws {
        let rig = try Rig(lockedB: true)
        #expect(await settle { rig.slices.entries.count == 2 && rig.slices.entries[1].locked })
        rig.tuning.openPad(sliceId: 1)
        #expect(rig.tuning.pad == nil)
        rig.tuning.openPad(sliceId: 0)
        #expect(rig.tuning.pad?.sliceId == 0)
    }
    // MARK: The band follows the finger before the Core answers

    /// A Core that answers each display request 800 ms after it arrives, on
    /// a clock the test moves by hand.
    @MainActor
    final class LateCore {
        static let delayMs = 800.0
        var nowMs = 0.0
        private(set) var asked: [(atMs: Double, subscription: DisplaySubscription)] = []
        private var answered = 0

        func record(_ subscription: DisplaySubscription) {
            asked.append((nowMs, subscription))
        }

        /// Moves the clock on and answers every request now 800 ms old, in order.
        func advance(ms: Double, to main: MainScreenModel) {
            nowMs += ms
            while answered < asked.count, nowMs - asked[answered].atMs >= Self.delayMs {
                let request = asked[answered].subscription
                answered += 1
                if let context = TuningShotTests.context(centreHz: request.centreHz, spanHz: request.spanHz,
                                                         sourceCentreHz: 7_236_400, generation: UInt32(answered + 1),
                                                         endpointId: request.endpointId) {
                    main.receive(.context(context))
                }
            }
        }
    }

    @Test("a pan shows at once, before a late Core answers, and stays where it was dropped after")
    func panShowsBeforeTheCoreAnswers() async throws {
        let late = LateCore()
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
        ])))
        mirror.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        mirror.apply(Rig.slice(0, hz: 7_236_400, active: true, step: 100))
        mirror.apply(.snapshotComplete)
        let suite = "BandTuningTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { late.record($0) },
                                                                         unsubscribe: { _ in }),
                                   displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let band = main.band
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        main.subscriber.receive(.mediaState(.connected))
        #expect(await settle { late.asked.count == 1 })
        band.requestView(TuneGestures.View(centerHz: 7_236_400, spanHz: 48_000))
        #expect(await settle { late.asked.last?.subscription.spanHz == 48_000 })
        late.advance(ms: 800, to: main)
        #expect(band.centerHz == 7_236_400 && band.spanHz == 48_000)

        let size = CGSize(width: 402, height: 500)
        let geometry = try #require(band.pointGeometry(size: size)).geometry
        let revision = band.revision
        let dragger = BandDrag()
        dragger.changed(from: CGPoint(x: 6, y: 300), translation: -100, geometry: geometry,
                        entries: main.slices.entries, placements: [], band: band, slices: main.slices, dragToTune: true)
        let dropped = 7_236_400 + 100.0 / 402 * 48_000
        // The first frame after the finger moved: the band, its scale, strip
        // and flags are drawn at the new view, before the Core has answered.
        #expect(band.revision != revision)
        #expect(abs(band.view.centerHz - dropped) < 1e-6)
        #expect(abs(band.overlays(scale: 3).centerHz - dropped) < 1e-6)
        #expect(abs((band.pointGeometry(size: size)?.geometry.centerHz ?? 0) - dropped) < 1e-6)
        #expect(band.centerHz == 7_236_400)
        dragger.ended(slices: main.slices)
        // The request goes; the Core has not answered it 400 ms on.
        #expect(await settle { abs((late.asked.last?.subscription.centreHz ?? 0) - dropped) < 1e-6 })
        late.advance(ms: 400, to: main)
        #expect(band.centerHz == 7_236_400)
        #expect(abs(band.view.centerHz - dropped) < 1e-6)
        // Its answer arrives: the view stays where the finger dropped it, and
        // the new frames cover it.
        late.advance(ms: 400, to: main)
        #expect(abs(band.centerHz - dropped) < 1e-6)
        #expect(abs(band.view.centerHz - dropped) < 1e-6)
        let generation = UInt32(late.asked.count + 1)
        main.receive(.displayFrame(DisplayFrame(endpointId: band.endpointId ?? 1, contextGeneration: generation,
                                                encoderSequence: 1, producerTimestamp: 0, isKeyframe: true,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -100], waterfallDbm: [-100, -100], wideDbm: [])))
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        #expect(abs((band.overlays(scale: 3).frameCoverage?.centerHz ?? 0) - dropped) < 1e-6)
    }
}
