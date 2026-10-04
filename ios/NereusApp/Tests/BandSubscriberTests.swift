// NereusSDR for iOS: the band's display subscription: what it asks for, and the budget's order of sending
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
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
        // A released endpoint's ID is not used again.
        #expect(rig.band.endpointId == 2)
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
        #expect(rig.band.endpointId == asked.endpointId + 1)
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
}
