// NereusSDR for iOS: the band's display subscription: what it asks for, and the budget's order of sending
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

/// R-IOS-11: the band's `subscribe` carries its width, its view and the
/// phone's extras, and on the display budget wire it goes in the budget
/// design's order: nothing more while a result is awaited, reductions
/// before increases, a refusal not repeated, a stall after the deadline.
@Suite("Band subscription", .serialized)
@MainActor
struct BandSubscriberTests {
    /// The operations the subscriber sent, in order.
    final class Sent {
        enum Operation: Equatable {
            case subscribe(DisplaySubscription)
            case unsubscribe(UInt32)
        }

        var operations: [Operation] = []

        var subscriptions: [DisplaySubscription] {
            operations.compactMap { if case .subscribe(let subscription) = $0 { return subscription } else { return nil } }
        }
    }

    struct Rig {
        let store: MirrorStore
        let settings: SettingsProxyClient
        let band: BandModel
        let slices: BandSlicesModel
        let subscriber: BandSubscriber
        let sent: Sent
    }

    static let frequency = 7_236_400.0
    static let sampleRate: Int64 = 192_000
    static let generation: Int64 = 3

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    /// Lets queued plans run, for checks that nothing more is sent.
    private func quiet() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    private func rig(budget: Bool = true, extras: Bool = false, bytes: Int64 = 50_000_000,
                     deadline: Duration = .seconds(10)) -> Rig {
        let store = MirrorStore(send: { _ in })
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 0, name: "remoteMediaVersion", value: .i64(1)),
            .init(ordinal: 0, name: "displayExtrasVersion", value: .i64(extras ? 1 : 0)),
        ]
        if budget {
            capabilities += [
                .init(ordinal: 0, name: "remoteDisplayBudgetVersion", value: .i64(1)),
                .init(ordinal: 0, name: "displayApplicationBytesPerSecond", value: .i64(bytes)),
                .init(ordinal: 0, name: "spectrumSampleUnitsPerSecond", value: .i64(50_000_000)),
                .init(ordinal: 0, name: "displayBudgetGeneration", value: .i64(Self.generation)),
                .init(ordinal: 0, name: "displayBudgetReason", value: .utf8("none")),
            ]
        }
        store.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        store.apply(slice(hz: Self.frequency))
        let settings = SettingsProxyClient(send: { _ in })
        let band = BandModel()
        let slices = BandSlicesModel(store: store, commands: nil)
        let sent = Sent()
        let subscriber = BandSubscriber(band: band, slices: slices, mirror: store, settings: settings,
                                        operations: BandSubscriber.Operations(
                                            subscribe: { sent.operations.append(.subscribe($0)) },
                                            unsubscribe: { sent.operations.append(.unsubscribe($0)) }),
                                        deadline: deadline)
        // A band 402 points wide at 3x.
        _ = band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        return Rig(store: store, settings: settings, band: band, slices: slices, subscriber: subscriber, sent: sent)
    }

    private func slice(hz: Double) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 11, name: "active", value: .bool(true)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(Self.sampleRate)),
        ]))
    }

    private func result(for subscription: DisplaySubscription, accepted: Bool = true,
                        reason: String = "") -> MediaControlEvent {
        allocationResult(endpointId: subscription.endpointId, revision: subscription.revision, accepted: accepted,
                         reason: reason, acceptedRevision: accepted ? subscription.revision : 0)
    }

    private func allocationResult(endpointId: UInt32, revision: UInt32, accepted: Bool, reason: String,
                                  acceptedRevision: UInt32) -> MediaControlEvent {
        let held = acceptedRevision != 0
        let payload: [String: LinkJSON] = [
            "op": .string("allocation-result"), "connectionId": .string("test"),
            "endpointId": .number(Double(endpointId)), "revision": .number(Double(revision)),
            "accepted": .bool(accepted), "reason": .string(reason),
            "budgetGeneration": .number(Double(Self.generation)),
            "acceptedRevision": .number(Double(acceptedRevision)),
            "applicationBytesPerSecond": .number(held ? 1000 : 0),
            "spectrumSampleUnitsPerSecond": .number(held ? 1000 : 0), "messagesPerSecond": .number(held ? 30 : 0),
        ]
        guard let decoded = MediaControlDecoder.allocationResult(payload) else {
            Issue.record("the allocation result did not decode")
            return .mediaState(.new)
        }
        return .allocationResult(decoded)
    }

    // MARK: What it asks for

    @Test("nothing is asked for until the media connection is up")
    func waitsForMedia() async {
        let rig = rig()
        await quiet()
        #expect(rig.sent.operations.isEmpty)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.operations.count == 1 })
    }

    @Test("the current FFT plan follows settings, rate and view before media, without sending")
    func plansBeforeMedia() async {
        let rig = rig(budget: false)
        #expect(await settle { rig.subscriber.plannedFftSize == 4096 })
        #expect(rig.subscriber.plannedSampleRateHz == Double(Self.sampleRate))
        #expect(rig.band.requestedView == nil, "planning before media cannot move the visible band")
        #expect(rig.sent.operations.isEmpty)

        rig.settings.apply(.settingsSnapshot(LinkMessage.SettingsSnapshot(properties: [
            .init(name: "DisplayFftSize", value: .utf8("8192")),
        ])))
        #expect(await settle { rig.subscriber.plannedFftSize == 8192 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 6_000))
        #expect(await settle { rig.subscriber.plannedFftSize == 65_536 })
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(96_000)),
        ])))
        #expect(await settle { rig.subscriber.plannedSampleRateHz == 96_000 })
        #expect(await settle { rig.subscriber.plannedFftSize == 32_768 })
        #expect(rig.sent.operations.isEmpty)

        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        #expect(rig.sent.subscriptions.first?.fftSize == rig.subscriber.plannedFftSize)
        #expect(rig.sent.subscriptions.first?.spanHz == 6_000)
    }

    @Test("a Core that describes Sizing plans by the choice: Auto by the zoom, Fixed the Size, Hz/bin the target")
    func plansBySizing() async throws {
        let rig = rig(budget: false)
        let catalog: StationCatalog = try #require(BandFlagShotTests.catalogue())
        rig.band.catalog = catalog
        #expect(rig.band.catalog?.display?.sizingControl != nil)
        func stored(_ values: [String: String]) {
            rig.settings.apply(.settingsSnapshot(LinkMessage.SettingsSnapshot(
                properties: values.sorted { $0.key < $1.key }.map { .init(name: $0.key, value: .utf8($0.value)) })))
        }
        typealias Sizing = StationCatalog.Display.FftSizing
        // Auto: the stored size plays no part; the zoom picks, never below 4096.
        stored(["DisplayFftSize": "8192", Sizing.settingsKey: "Auto"])
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 12_000))
        #expect(await settle { rig.subscriber.plannedFftSize == 32_768 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 6_000))
        // 192 kHz across 1206 pixels of 6 kHz: 38 592 bins, so 65 536.
        #expect(await settle { rig.subscriber.plannedFftSize == 65_536 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 192_000))
        #expect(await settle { rig.subscriber.plannedFftSize == 4096 })
        // The choice never written and no target: Auto, as that Core runs.
        stored(["DisplayFftSize": "32768", "DisplayHzPerBinTarget": "0"])
        await quiet()
        #expect(rig.subscriber.plannedFftSize == 4096)
        // Fixed: exactly the stored size, whatever the zoom or a stray target.
        stored(["DisplayFftSize": "8192", "DisplayHzPerBinTarget": "5", Sizing.settingsKey: "Fixed"])
        #expect(await settle { rig.subscriber.plannedFftSize == 8192 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 6_000))
        await quiet()
        #expect(rig.subscriber.plannedFftSize == 8192)
        // A stored size the Size control does not offer runs the built-in size.
        stored(["DisplayFftSize": "2048", Sizing.settingsKey: "Fixed"])
        #expect(await settle { rig.subscriber.plannedFftSize == 4096 })
        // Hz/bin: the smallest size at or below the target, whatever the zoom.
        stored(["DisplayFftSize": "8192", "DisplayHzPerBinTarget": "5", Sizing.settingsKey: "HzPerBin"])
        #expect(await settle { rig.subscriber.plannedFftSize == 65_536 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 192_000))
        await quiet()
        #expect(rig.subscriber.plannedFftSize == 65_536)
        // Never a transform longer than 0.7 s: 131072 at 192 kHz.
        stored(["DisplayHzPerBinTarget": "0.5", Sizing.settingsKey: "HzPerBin"])
        #expect(await settle { rig.subscriber.plannedFftSize == 131_072 })
        // The choice never written, a target above 0: Hz/bin.
        stored(["DisplayHzPerBinTarget": "12.5"])
        #expect(await settle { rig.subscriber.plannedFftSize == 16_384 })
        // What is sent is the plan.
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        #expect(rig.sent.subscriptions.first?.fftSize == 16_384 && rig.sent.subscriptions.first?.tier == .fine)
    }

    @Test("the least averaging time is the Core's own: 1 ms where its controls start there, 10 ms otherwise")
    func averagingFloor() async throws {
        #expect(BandSubscriber.averageTimeFloorMs(nil) == 10)
        let display = try #require(BandFlagShotTests.catalogue()?.display)
        #expect(display.control(subscribe: CoreDisplayModel.Field.traceAverageTime)?.min == 1)
        #expect(BandSubscriber.averageTimeFloorMs(display) == 1)
        // A Core with the extras and the description takes this phone's 1 ms.
        let rig = rig(budget: false, extras: true)
        let catalog: StationCatalog = try #require(BandFlagShotTests.catalogue())
        rig.band.catalog = catalog
        rig.band.settings.spectrumAverageTimeMs = 1
        rig.band.settings.waterfallAverageTimeMs = 3
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        #expect(first.extras?.averageTimeMs == 1 && first.extras?.waterfallAverageTimeMs == 3)
        #expect(throws: Never.self) { try DisplayEndpointRequest.validate(first, gates: rig.subscriber.gates) }
        // A Core that describes no display gets 10 ms for the same settings.
        let older = self.rig(budget: false, extras: true)
        older.band.settings.spectrumAverageTimeMs = 1
        older.band.settings.waterfallAverageTimeMs = 3
        older.subscriber.receive(.mediaState(.connected))
        #expect(await settle { older.sent.subscriptions.count == 1 })
        let asked = try #require(older.sent.subscriptions.first)
        #expect(asked.extras?.averageTimeMs == 10 && asked.extras?.waterfallAverageTimeMs == 10)
        #expect(throws: Never.self) { try DisplayEndpointRequest.validate(asked, gates: older.subscriber.gates) }
    }

    @Test("a missing current rate or slice clears the plan before media")
    func missingSliceClearsPlan() async {
        let rig = rig()
        #expect(await settle { rig.subscriber.plannedFftSize == 4096 })
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(0)),
        ])))
        #expect(await settle { rig.subscriber.plannedFftSize == nil })
        #expect(rig.subscriber.plannedSampleRateHz == nil)
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(Self.sampleRate)),
        ])))
        #expect(await settle { rig.subscriber.plannedFftSize == 4096 })
        rig.store.apply(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:0", className: "SliceModel")))
        #expect(await settle { rig.subscriber.plannedFftSize == nil })
        #expect(rig.subscriber.plannedSampleRateHz == nil)
        #expect(rig.sent.operations.isEmpty)
    }

    @Test("the band asks for its width, with the active slice in the middle of its sample rate")
    func firstRequest() async throws {
        let rig = rig()
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        #expect(first.endpointId == 1)
        #expect(first.revision == 1)
        #expect(first.sliceId == 0)
        #expect(first.pixels == 1206)
        #expect(first.centreHz == Self.frequency)
        #expect(first.spanHz == Double(Self.sampleRate))
        #expect(first.fps == BandSubscriber.defaultFps)
        #expect(first.fftSize == BandSubscriber.defaultFftSize)
        #expect(first.tier == .wide)
        #expect(first.windowType == BandSubscriber.defaultWindowType)
        #expect(first.framesPerLine == 1)
        #expect(first.extras == nil)
        #expect(rig.band.endpointId == 1)
        #expect(throws: Never.self) { try DisplayEndpointRequest.validate(first, gates: rig.subscriber.gates) }
    }

    @Test("the Core's display settings set the frame rate, FFT size and window")
    func coresSettings() async throws {
        let rig = rig()
        rig.settings.apply(.settingsSnapshot(LinkMessage.SettingsSnapshot(properties: [
            .init(ordinal: 0, name: "DisplaySpectrumFps", value: .utf8("20")),
            .init(ordinal: 0, name: "DisplayFftSize", value: .utf8("8192")),
            .init(ordinal: 0, name: "DisplayFftWindow", value: .utf8("2")),
        ])))
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        #expect(first.fps == 20)
        #expect(first.fftSize == 8192)
        #expect(first.windowType == 2)
    }

    @Test("a long session's mode caps the frames and halves the width; with none the band unsubscribes and comes back as a new endpoint")
    func longSession() async throws {
        let rig = rig(budget: false)
        rig.subscriber.session = SessionPolicy.Mode.saver.request
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let saver = try #require(rig.sent.subscriptions.last)
        #expect(saver.fps == 5)
        #expect(saver.pixels == 603)
        #expect(saver.endpointId == 1)
        // Locked with Sound only: no display endpoint at all.
        rig.subscriber.session = .none
        #expect(await settle { rig.sent.operations.last == .unsubscribe(1) })
        await quiet()
        #expect(rig.sent.operations.count == 2)
        // Back: the band asks again, on a new endpoint, so the Core's new
        // context brings a keyframe.
        rig.subscriber.session = SessionPolicy.Mode.full.request
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        let back = try #require(rig.sent.subscriptions.last)
        #expect(back.endpointId == 2)
        #expect(back.fps == BandSubscriber.defaultFps)
        #expect(back.pixels == 1206)
        #expect(rig.band.endpointId == 2)
    }

    @Test("with the Core's extras the phone's display settings ride along")
    func extrasRideAlong() async throws {
        let rig = rig(extras: true)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        #expect(first.extras == BandDisplaySettings.desktopDefaults.extrasRequest(gates: rig.subscriber.gates))
        #expect(first.extras?.waterfallLevels?.mode == .clarity)
    }

    @Test("a deep zoom asks for a finer engine of its own")
    func deepZoomIsFine() async throws {
        let rig = rig(budget: false)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 6_000))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        let zoomed = try #require(rig.sent.subscriptions.last)
        #expect(zoomed.spanHz == 6_000)
        #expect(zoomed.tier == .fine)
        // 192 kHz across 1206 pixels of 6 kHz: 38 592 bins, so 65 536.
        #expect(zoomed.fftSize == 65_536)
    }

    @Test("when the slice leaves the view, the view moves to it")
    func followsTheSlice() async throws {
        let rig = rig(budget: false)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 24_000))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_300_000)),
        ])))
        #expect(await settle { rig.sent.subscriptions.last?.centreHz == 7_300_000 })
        #expect(rig.sent.subscriptions.last?.spanHz == 24_000)
    }

    // MARK: The budget's order

    @Test("nothing more is sent while a result is awaited; the result lets the next go")
    func waitsForResults() async throws {
        let rig = rig()
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 24_000))
        await quiet()
        #expect(rig.sent.operations.count == 1)
        rig.subscriber.receive(result(for: first))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        let second = try #require(rig.sent.subscriptions.last)
        #expect(second.revision == 2)
        #expect(second.spanHz == 24_000)
    }

    @Test("without the budget wire each change goes at once")
    func withoutTheBudgetWire() async {
        let rig = rig(budget: false)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 24_000))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        #expect(rig.sent.subscriptions.map(\.revision) == [1, 2])
    }

    @Test("a refused request is not sent again until the wishes change")
    func refusalIsNotRepeated() async throws {
        let rig = rig()
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        rig.subscriber.receive(result(for: first, accepted: false, reason: "The display does not fit."))
        // Anything that asks for a new plan without changing the wish sends nothing.
        rig.store.apply(.capabilities(LinkMessage.Capabilities(properties: rig.store.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
        })))
        rig.subscriber.replan()
        await quiet()
        #expect(rig.sent.operations.count == 1)
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 24_000))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
    }

    @Test("past the deadline the allocation stalls: an increase waits, a reduction goes, a late result settles it")
    func stallsAfterTheDeadline() async throws {
        let rig = rig(deadline: .milliseconds(50))
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        rig.subscriber.receive(result(for: first))
        // A new view at the same size charges the same: not an increase.
        rig.band.requestView(TuneGestures.View(centerHz: Self.frequency, spanHz: 96_000))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        let unanswered = try #require(rig.sent.subscriptions.last)
        try await Task.sleep(for: .milliseconds(200))
        #expect(await settle { rig.subscriber.stalled })
        // Wider asks more of the budget: it waits while stalled.
        _ = rig.band.prepareToDraw(size: CGSize(width: 2000, height: 1500), scale: 3)
        await quiet()
        #expect(rig.sent.subscriptions.count == 2)
        // Narrower asks less: it goes.
        _ = rig.band.prepareToDraw(size: CGSize(width: 900, height: 1500), scale: 3)
        #expect(await settle { rig.sent.subscriptions.count == 3 })
        #expect(rig.sent.subscriptions.last?.pixels == 900)
        rig.subscriber.receive(result(for: unanswered))
        rig.subscriber.receive(result(for: try #require(rig.sent.subscriptions.last)))
        #expect(await settle { !rig.subscriber.stalled })
    }

    @Test("a budget below the band's floor pauses it and lets its endpoint go")
    func suspendsBelowTheFloor() async throws {
        let rig = rig(budget: true)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let first = try #require(rig.sent.subscriptions.first)
        rig.subscriber.receive(result(for: first))
        #expect(rig.subscriber.acceptedDisplay?.subscription.revision == first.revision)
        // The Core's budget falls below even 256 pixels at 10 frames a second.
        rig.store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: "remoteMediaVersion", value: .i64(1)),
            .init(ordinal: 0, name: "remoteDisplayBudgetVersion", value: .i64(1)),
            .init(ordinal: 0, name: "displayApplicationBytesPerSecond", value: .i64(100)),
            .init(ordinal: 0, name: "spectrumSampleUnitsPerSecond", value: .i64(100)),
            .init(ordinal: 0, name: "displayBudgetGeneration", value: .i64(Self.generation)),
        ])))
        #expect(await settle { rig.sent.operations.last == .unsubscribe(1) })
        // The Core has not acknowledged the unsubscribe yet. The old
        // accepted display must already be unavailable to the S-meter.
        #expect(rig.subscriber.acceptedDisplay == nil)
        #expect(rig.band.paused)
        // A released endpoint's ID is not used again: the band draws none
        // until its next subscribe takes a new one.
        #expect(rig.band.endpointId == nil)
    }

    // MARK: Asking for what the operator wants (the several-devices design, 9.3)

    /// The Core's charge for the band as the rig draws it, at `pixels` wide.
    private func wantedCharge(pixels: Int = 1206) throws -> DisplayQualityAllocator.Charge {
        let intent = DisplayQualityAllocator.Intent(panId: BandSubscriber.panId, pixels: pixels,
                                                    fps: BandSubscriber.defaultFps,
                                                    waterfallPeriodMs: BandSubscriber.waterfallPeriodMs, active: true)
        let allocation = try DisplayQualityAllocator.allocate(budget: nil, intents: [intent]).get()
        return allocation.total
    }

    static let budgetRefusal = "The Core's display limit has no room left."

    @Test("a new media session asks for the band as wanted, beyond the share; a budget refusal plans inside it")
    func asksThenPlansInsideTheShare() async throws {
        let half = Int64(try wantedCharge().applicationBytesPerSecond / 2)
        let rig = rig(bytes: half)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let asked = try #require(rig.sent.subscriptions.first)
        #expect(asked.pixels == 1206 && asked.fps == BandSubscriber.defaultFps)
        #expect(rig.subscriber.asking)
        rig.subscriber.receive(result(for: asked, accepted: false, reason: Self.budgetRefusal))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        let planned = try #require(rig.sent.subscriptions.last)
        #expect(!rig.subscriber.asking)
        #expect(planned.endpointId == asked.endpointId)
        #expect(planned.pixels < 1206 || planned.fps < BandSubscriber.defaultFps)
        rig.subscriber.receive(result(for: planned))
        // A new budget generation alone never asks again.
        rig.store.apply(.capabilities(LinkMessage.Capabilities(properties: rig.store.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name,
                                      value: name == "displayBudgetGeneration" ? .i64(Self.generation + 1) : value.wireValue)
        })))
        await quiet()
        #expect(rig.sent.operations.count == 2)
        #expect(!rig.subscriber.asking)
    }

    @Test("alone on the Core with room, the ask is the whole request and it is kept")
    func aloneTheAskIsKept() async throws {
        let rig = rig()
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let asked = try #require(rig.sent.subscriptions.first)
        rig.subscriber.receive(result(for: asked))
        await quiet()
        #expect(rig.sent.operations.count == 1)
        #expect(!rig.subscriber.asking)
    }

    @Test("a wider band asks again once its width holds, and a change of transmit holder asks again")
    func growthAndTheHolderAskAgain() async throws {
        let half = Int64(try wantedCharge(pixels: 1500).applicationBytesPerSecond / 2)
        let rig = rig(bytes: half)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        rig.subscriber.receive(result(for: try #require(rig.sent.subscriptions.last), accepted: false,
                                      reason: Self.budgetRefusal))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        rig.subscriber.receive(result(for: try #require(rig.sent.subscriptions.last)))
        await quiet()
        // Wider: planned inside the share at once, and the whole width asked
        // for only once it has held for 200 ms.
        _ = rig.band.prepareToDraw(size: CGSize(width: 1500, height: 1500), scale: 3)
        await quiet()
        let whole = { (subscription: DisplaySubscription?) in
            subscription?.pixels == 1500 && subscription?.fps == BandSubscriber.defaultFps
        }
        #expect(!whole(rig.sent.subscriptions.last))
        try await Task.sleep(for: .milliseconds(400))
        if rig.sent.subscriptions.count > 2, let planned = rig.sent.subscriptions.last, !whole(planned) {
            // The plan inside the share went first; the ask follows its answer.
            rig.subscriber.receive(result(for: planned))
        }
        #expect(await settle { whole(rig.sent.subscriptions.last) })
        let widened = try #require(rig.sent.subscriptions.last)
        let asked = rig.sent.subscriptions.count
        rig.subscriber.receive(result(for: widened, accepted: false, reason: Self.budgetRefusal))
        #expect(await settle { rig.sent.subscriptions.count > asked })
        rig.subscriber.receive(result(for: try #require(rig.sent.subscriptions.last)))
        await quiet()
        let before = rig.sent.subscriptions.count
        rig.subscriber.transmitHolderChanged()
        #expect(await settle { rig.sent.subscriptions.count == before + 1 })
        #expect(whole(rig.sent.subscriptions.last))
    }

    @Test("a display the Core refused and the band then drops is unsubscribed")
    func aRefusedDisplayDroppedIsReleased() async throws {
        let rig = rig()
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        let asked = try #require(rig.sent.subscriptions.first)
        rig.subscriber.receive(result(for: asked, accepted: false, reason: Self.budgetRefusal))
        // The Core's share falls below the band's floor: the band pauses.
        rig.store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: "remoteMediaVersion", value: .i64(1)),
            .init(ordinal: 0, name: "remoteDisplayBudgetVersion", value: .i64(1)),
            .init(ordinal: 0, name: "displayApplicationBytesPerSecond", value: .i64(100)),
            .init(ordinal: 0, name: "spectrumSampleUnitsPerSecond", value: .i64(100)),
            .init(ordinal: 0, name: "displayBudgetGeneration", value: .i64(Self.generation)),
        ])))
        #expect(await settle { rig.sent.operations.last == .unsubscribe(asked.endpointId) })
        #expect(rig.band.paused)
        #expect(rig.band.endpointId == nil)
        // It is not waited for: the Core held nothing for it.
        #expect(!rig.subscriber.stalled)
    }

    @Test("a new media connection starts over from endpoint 1")
    func newConnectionStartsOver() async {
        let rig = rig(budget: false)
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 1 })
        rig.subscriber.receive(.mediaState(.closed))
        rig.subscriber.receive(.mediaState(.connected))
        #expect(await settle { rig.sent.subscriptions.count == 2 })
        #expect(rig.sent.subscriptions.map(\.endpointId) == [1, 1])
        #expect(rig.sent.subscriptions.map(\.revision) == [1, 1])
    }

    // MARK: Several pans (Task 9)

    /// Two pans' subscribers on one shared allocator, each on its own
    /// pan's slice (slice:0 on pan-0, slice:1 on pan-1), as the iPad's
    /// stack makes them.
    private func pans(_ rig: Rig) -> (shared: SharedDisplayAllocator,
                                      make: (PanIdentity, Sent) -> BandSubscriber) {
        rig.store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
        ])))
        rig.store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:1", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(14_100_000)),
            .init(ordinal: 3, name: "filterLow", value: .i64(100)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(2800)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(1)),
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-1")),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(Self.sampleRate)),
        ])))
        let shared = SharedDisplayAllocator()
        let make = { (pan: PanIdentity, sent: Sent) -> BandSubscriber in
            let band = BandModel()
            let slices = BandSlicesModel(store: rig.store, commands: nil)
            slices.panKey = pan.corePanKey
            let subscriber = BandSubscriber(band: band, slices: slices, mirror: rig.store, settings: rig.settings,
                                            operations: BandSubscriber.Operations(
                                                subscribe: { sent.operations.append(.subscribe($0)) },
                                                unsubscribe: { sent.operations.append(.unsubscribe($0)) }),
                                            pan: pan, shared: shared)
            _ = band.prepareToDraw(size: CGSize(width: 1206, height: 700), scale: 3)
            return subscriber
        }
        return (shared, make)
    }

    /// The Core's answer to an unsubscribe of `endpointId` at `revision`: let go.
    private func released(_ endpointId: UInt32, revision: UInt32) -> MediaControlEvent {
        allocationResult(endpointId: endpointId, revision: revision, accepted: true, reason: "", acceptedRevision: 0)
    }

    @Test("two pans' subscribers share one budget, each on its own pan's slice with IDs from one counter, and a refusal stays with its pan")
    func twoPansShareOneAllocator() async throws {
        let rig = rig()
        let (shared, make) = pans(rig)
        let sentFirst = Sent()
        let sentSecond = Sent()
        let first = make(.first, sentFirst)
        let second = make(PanIdentity(number: 2), sentSecond)
        first.receive(.mediaState(.connected))
        second.receive(.mediaState(.connected))
        #expect(await settle { !sentFirst.subscriptions.isEmpty && !sentSecond.subscriptions.isEmpty })
        let one = try #require(sentFirst.subscriptions.last)
        let two = try #require(sentSecond.subscriptions.last)
        #expect(one.sliceId == 0 && two.sliceId == 1)
        // One counter for the connection: the two pans' first endpoints are 1 and 2.
        #expect(Set([one.endpointId, two.endpointId]) == [1, 2])
        // One allocation holds both wishes, and both pans get frames.
        #expect(Set(shared.intents.map(\.panId)) == ["1", "2"])
        #expect(shared.quality(forPan: "1") != nil && shared.quality(forPan: "2") != nil)
        // A refusal of Pan 2's request is Pan 2's alone, in the Core's words.
        second.receive(result(for: two, accepted: false, reason: "No display is free for this pan."))
        #expect(await settle { second.displayRefusal == "No display is free for this pan." })
        #expect(first.displayRefusal == nil)
        // An empty reason still says something.
        first.receive(result(for: try #require(sentFirst.subscriptions.last), accepted: false, reason: ""))
        #expect(await settle { first.displayRefusal == BandSlicesModel.refusedText })
        // Closing Pan 2 drops its wish from the shared budget.
        second.close()
        #expect(await settle { !shared.intents.contains { $0.panId == "2" } })
        // Its refused endpoint is let go at once, as the Core holds nothing for it.
        #expect(await settle { second.holdsNothing && sentSecond.operations.last == .unsubscribe(two.endpointId) })
    }

    @Test("a pan that releases after another pan subscribed takes a higher ID, and a reopened pan never reuses its old one")
    func endpointIdsRiseAcrossPans() async throws {
        let rig = rig(budget: false)
        let (_, make) = pans(rig)
        let sentFirst = Sent()
        let sentSecond = Sent()
        let first = make(.first, sentFirst)
        first.receive(.mediaState(.connected))
        #expect(await settle { sentFirst.subscriptions.count == 1 })
        let second = make(PanIdentity(number: 2), sentSecond)
        second.receive(.mediaState(.connected))
        #expect(await settle { sentSecond.subscriptions.count == 1 })
        #expect(sentFirst.subscriptions.map(\.endpointId) == [1])
        #expect(sentSecond.subscriptions.map(\.endpointId) == [2])
        // Pan 1 lets its endpoint go (Sound only) and comes back: above Pan 2's.
        first.session = .none
        #expect(await settle { sentFirst.operations.last == .unsubscribe(1) })
        first.session = SessionPolicy.Mode.full.request
        #expect(await settle { sentFirst.subscriptions.count == 2 })
        #expect(sentFirst.subscriptions.last?.endpointId == 3)
        // Pan 2 closes and opens again: its new subscriber takes the next ID.
        second.close()
        #expect(await settle { sentSecond.operations.last == .unsubscribe(2) })
        let sentReopened = Sent()
        let reopened = make(PanIdentity(number: 2), sentReopened)
        reopened.receive(.mediaState(.connected))
        #expect(await settle { sentReopened.subscriptions.count == 1 })
        #expect(sentReopened.subscriptions.last?.endpointId == 4)
    }

    @Test("Pan 2's claim keeps Pan 1's increase out of the headroom it holds until the Core lets it go")
    func anotherPansClaimBlocksAnIncrease() async throws {
        let budget = try wantedCharge().applicationBytesPerSecond
        let rig = rig(bytes: Int64(budget))
        let (shared, make) = pans(rig)
        let sentFirst = Sent()
        let sentSecond = Sent()
        let first = make(.first, sentFirst)
        let second = make(PanIdentity(number: 2), sentSecond)
        first.receive(.mediaState(.connected))
        second.receive(.mediaState(.connected))
        #expect(await settle { sentFirst.subscriptions.count == 1 && sentSecond.subscriptions.count == 1 })
        // Both ask for the whole band and the Core answers both asks; each
        // then plans inside its share of a budget with room for one. (A
        // refused ask is not asked again until the wishes change, so the
        // asks are taken here to leave Pan 1's later increase its own.)
        first.receive(result(for: try #require(sentFirst.subscriptions.last)))
        second.receive(result(for: try #require(sentSecond.subscriptions.last)))
        #expect(await settle { sentFirst.subscriptions.count == 2 && sentSecond.subscriptions.count == 2 })
        let held = try #require(sentFirst.subscriptions.last)
        let secondHeld = try #require(sentSecond.subscriptions.last)
        first.receive(result(for: held))
        second.receive(result(for: secondHeld))
        await quiet()
        #expect(sentFirst.subscriptions.count == 2)
        #expect(held.pixels < 1206 || held.fps < BandSubscriber.defaultFps)
        // Pan 2 closes: Pan 1's share is now the whole budget, but Pan 2's
        // display still counts until the Core lets it go.
        second.close()
        #expect(await settle { sentSecond.operations.last == .unsubscribe(secondHeld.endpointId) })
        #expect(shared.quality(forPan: "1")?.reduced == false)
        await quiet()
        #expect(sentFirst.subscriptions.count == 2)
        // The Core lets Pan 2's display go: Pan 1 grows into the room.
        second.receive(released(secondHeld.endpointId, revision: secondHeld.revision &+ 1))
        #expect(await settle { sentFirst.subscriptions.count == 3 })
        let grown = try #require(sentFirst.subscriptions.last)
        #expect(grown.pixels > held.pixels || grown.fps > held.fps)
        #expect(second.holdsNothing)
    }

    @Test("a closed pan's subscriber letting go leaves the reopened pan's claim in place")
    func retiringClaimLeavesTheReopenedClaim() async throws {
        let rig = rig()
        let (shared, make) = pans(rig)
        let sentFirst = Sent()
        let sentOld = Sent()
        let sentNew = Sent()
        let first = make(.first, sentFirst)
        let old = make(PanIdentity(number: 2), sentOld)
        first.receive(.mediaState(.connected))
        old.receive(.mediaState(.connected))
        #expect(await settle { sentFirst.subscriptions.count == 1 && sentOld.subscriptions.count == 1 })
        let oldHeld = try #require(sentOld.subscriptions.last)
        old.receive(result(for: oldHeld))
        // Pan 2 closes, its endpoint not yet let go, and opens again at once.
        old.close()
        #expect(await settle { sentOld.operations.last == .unsubscribe(oldHeld.endpointId) })
        let reopened = make(PanIdentity(number: 2), sentNew)
        reopened.receive(.mediaState(.connected))
        #expect(await settle { sentNew.subscriptions.count == 1 })
        let full = try #require(shared.budget).applicationBytesPerSecond
        let withBoth = try #require(shared.headroom(for: ObjectIdentifier(first))).applicationBytesPerSecond
        // The old display goes: only its claim leaves; the reopened pan's ask still counts.
        old.receive(released(oldHeld.endpointId, revision: oldHeld.revision &+ 1))
        #expect(await settle { old.holdsNothing })
        let left = try #require(shared.headroom(for: ObjectIdentifier(first))).applicationBytesPerSecond
        #expect(left > withBoth)
        #expect(left < full)
        #expect(sentNew.subscriptions.last?.endpointId != oldHeld.endpointId)
    }
}
