// NereusSDR for iOS: the phone's controls act at the touch and hold the operator's value until the Core answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ, 2026-10-01: "the controls should actually work. In real time not
/// snapping back to previous settings." A control shows the operator's
/// value at the touch and keeps it until the Core answers that write, as
/// the desktop's remote window does (`StationClient.cpp:1040-1068`): a
/// refusal returns to the Core's value with its words, an answer that does
/// not come within 5 s restores latest Core under JJ's revised ruling,
/// marked not confirmed while retaining its actual late owner, and a
/// dropped link shows the Core's value and says so. The Core is
/// `FakeStation`, whose answers come only when a test gives them; the
/// mirror's time moves only on a test clock. With `NEREUS_LIVEUI_SHOTS`
/// set to a directory (through `TEST_RUNNER_NEREUS_LIVEUI_SHOTS`), the
/// pictures are written there.
@Suite("Live controls", .serialized)
@MainActor
struct LiveControlsTests {
    @Test("the debug band fixture answers an AF write rather than restoring its unsent value")
    func debugBandAnswersItsPropertyWrite() async throws {
        let model = AppModel()
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent()
        UITestBand.show(model, arguments: [UITestBand.argument], environment: [
            UITestBand.catalogueEnvironment: root.appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path,
        ])
        let slice = try #require(model.mirror.object("slice:0"))
        #expect(slice["afGain"] == .int(37))
        let outcome = await model.mirror.write("slice:0", property: "afGain", value: .int(85))
        #expect(outcome.accepted)
        #expect(outcome.reason.isEmpty)
        #expect(slice["afGain"] == .int(85))
    }

    final class Answered: @unchecked Sendable {
        private let lock = NSLock()
        private var ids: Set<UInt32> = []
        var all: Set<UInt32> { lock.withLock { ids } }
        func insert(_ id: UInt32) { lock.withLock { _ = ids.insert(id) } }
    }

    private let answered = Answered()

    @Test("a submitted filter entry restores latest Core on timeout, while newer typing stays untouched", arguments: [false, true])
    func timeoutRestoresOnlySubmittedNumberEntry(newerEntry: Bool) async throws {
        let (model, station, clock) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        let before = try #require(rx.filterLowHz)
        var closed = 0
        let pad = try #require(rx.filterEdgePad(low: true, close: { closed += 1 }))
        func type(_ value: Int64) {
            while !pad.entry.isEmpty { pad.press(.delete) }
            if pad.negative != (value < 0) { pad.press(.minus) }
            for digit in String(value.magnitude) { pad.press(.digit(Int(String(digit))!)) }
        }
        type(before - 100)
        let entered = Task { await pad.enter() }
        let write = try #require(await nextWrite(station, key: key, "filterLow"))
        await delta(station, key: key, write, .i64(before - 200))
        await idle()
        if newerEntry { type(before - 150) }
        let entry = pad.entry
        await clock.advance(by: 5_000)
        #expect(await entered.value == false)
        #expect(rx.filterLowHz == before - 200)
        if newerEntry {
            #expect(pad.entry == entry && pad.value == before - 150 && pad.refusal == nil)
            #expect(rx.note == nil)
        } else {
            #expect(pad.value == before - 200)
            #expect(pad.refusal == PropertyWriteOutcome.notConfirmed.reason)
            #expect(rx.note == PropertyWriteOutcome.notConfirmed.reason)
        }
        await result(station, key: key, write, accepted: false, reason: "Current filter refusal.", kept: .i64(before - 200))
        await idle()
        #expect(pad.refusal == (newerEntry ? nil : "Current filter refusal."))
        #expect(pad.value == (newerEntry ? before - 150 : before - 200))
        #expect(closed == 0 && writes(station, key: key, "filterLow").count == 1)
        await model.disconnect()
    }

    @Test("only the submitted frequency entry restores on timeout; newer or replacement drafts survive", arguments: [0, 1, 2])
    func timeoutRestoresOnlySubmittedFrequencyEntry(owner: Int) async throws {
        let (model, station, clock) = try await connected()
        let slice = try #require(model.main.rx.slice)
        let sliceId = try #require(model.main.slices.activeSliceId)
        let tuning = model.main.tuning
        tuning.openPad(sliceId: sliceId)
        let submitted = try #require(tuning.pad)
        for digit in [7, 1] {
            submitted.press(.digit(digit))
            if digit == 7 { submitted.press(.point) }
        }
        let entered = Task { await submitted.enter() }
        let write = try #require(await nextWrite(station, key: slice.key, "frequency"))
        await delta(station, key: slice.key, write, .f64(7_074_000))
        await idle()
        if owner == 2 { tuning.openPad(sliceId: sliceId) }
        let current = try #require(tuning.pad)
        if owner != 0 {
            while !current.entry.isEmpty { current.press(.delete) }
            for key: FrequencyPadModel.Key in [.digit(7), .point, .digit(2)] { current.press(key) }
        }
        let entry = current.entry
        await clock.advance(by: 5_000)
        #expect(await entered.value == false)
        #expect(tuning.pad === current)
        #expect(slice["frequency"] == .double(7_074_000))
        if owner == 0 {
            #expect(current.hertz == 7_074_000)
            #expect(current.refusal == PropertyWriteOutcome.notConfirmed.reason)
        } else {
            #expect(current.entry == entry && current.hertz == 7_200_000 && current.refusal == nil)
        }
        await result(station, key: slice.key, write, accepted: false, reason: "Current frequency refusal.", kept: .f64(7_074_000))
        await idle()
        #expect(current.refusal == (owner == 0 ? "Current frequency refusal." : nil))
        #expect(current.hertz == (owner == 0 ? 7_074_000 : 7_200_000))
        #expect(tuning.pad === current && writes(station, key: slice.key, "frequency").count == 1)
        await model.disconnect()
    }

    @Test("a five-second timeout restores the latest Core AF even during a stationary shared gesture")
    func timeoutRestoresStationaryGesture() async throws {
        let (model, station, clock) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        var gesture = SliderGestureDraft()
        let pacer = SliderSendPacer(clock: clock) { gesture.submitted($0); rx.setAfGain($0) }
        gesture.move(to: 85)
        pacer.move(to: 85)
        let write = try #require(await nextWrite(station, key: key, "afGain"))
        await delta(station, key: key, write, .i64(60))
        await idle()
        gesture.modelChanged(notConfirmed: rx.isUnconfirmed("afGain"))
        await clock.advance(by: 4_999)
        #expect(rx.afGain == 85 && (gesture.value ?? rx.afGain) == 85)
        #expect(rx.note == nil)
        await clock.advance(by: 1)
        #expect(await settle { rx.note == PropertyWriteOutcome.notConfirmed.reason })
        // This is the shared draft lifetime used by the real rows, with a
        // real RX write, Core delta and injected five-second deadline.
        gesture.modelChanged(notConfirmed: rx.isUnconfirmed("afGain"))
        #expect(rx.afGain == 60)
        #expect((gesture.value ?? rx.afGain) == 60)
        #expect(gesture.editing)
        #expect(model.mirror.isUnconfirmed(key, property: "afGain"))
        try await shootBoth("timeout-restored", rx: rx, main: model.main)
        await delta(station, key: key, write, .i64(65))
        await idle()
        gesture.modelChanged(notConfirmed: rx.isUnconfirmed("afGain"))
        #expect(rx.afGain == 65 && (gesture.value ?? rx.afGain) == 65)
        pacer.release()
        gesture.release(model: rx.afGain)
        await idle()
        #expect(writes(station, key: key, "afGain").count == 1)
        #expect(rx.afGain == 65 && gesture.value == nil)
        await model.disconnect()
    }

    @Test("an older timeout and late refusal cannot roll back a newer RX touch")
    func timeoutCannotRestoreOverNewerTouch() async throws {
        let (model, station, clock) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        rx.setAfGain(80)
        let first = try #require(await nextWrite(station, key: key, "afGain"))
        await clock.advance(by: 4_000)
        rx.setAfGain(90)
        #expect(await settle { rx.afGain == 90 })
        await delta(station, key: key, first, .i64(65))
        await idle()
        await clock.advance(by: 1_000)
        let second = try #require(await nextWrite(station, key: key, "afGain"))
        #expect(rx.afGain == 90 && rx.note == nil)
        #expect(!model.mirror.isUnconfirmed(key, property: "afGain"))
        await result(station, key: key, first, accepted: false, reason: "Obsolete AF refusal.", kept: .i64(50))
        await idle()
        #expect(rx.afGain == 90 && rx.note == nil)
        await clock.advance(by: 4_999)
        #expect(rx.afGain == 90)
        await clock.advance(by: 1)
        #expect(await settle { rx.note == PropertyWriteOutcome.notConfirmed.reason })
        #expect(rx.afGain == 65)
        #expect(model.mirror.isUnconfirmed(key, property: "afGain"))
        #expect(writes(station, key: key, "afGain").count == 2)
        await result(station, key: key, second, accepted: false, reason: "Current AF refusal.", kept: .i64(65))
        #expect(await settle { rx.note == "Current AF refusal." })
        #expect(rx.afGain == 65)
        await model.disconnect()
    }

    @Test("a slider's value holds through a slow answer and a remote change made meanwhile")
    func sliderHoldsThroughASlowAnswer() async throws {
        let (model, station, clock) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        let before = try #require(rx.afGain)
        let wanted = before == 20 ? 30.0 : 20.0
        rx.setAfGain(wanted)
        #expect(await settle { rx.afGain == wanted })
        let write = try #require(await nextWrite(station, key: key, "afGain"))
        // Another device's change while this write waits does not undo it.
        await delta(station, key: key, write, .i64(Int64(before) + 7))
        await idle()
        #expect(rx.afGain == wanted)
        await clock.advance(by: 4_999)
        #expect(rx.afGain == wanted)
        #expect(rx.note == nil)
        await result(station, key: key, write, accepted: true, kept: write.properties[0].value)
        await idle()
        #expect(rx.afGain == wanted)
        // After the answer, another device's change shows as it comes.
        await delta(station, key: key, write, .i64(Int64(before) + 3))
        #expect(await settle { rx.afGain == before + 3 })
        try await shootBoth("liveui-slider-after-remote-change", rx: rx, main: model.main)
        await model.disconnect()
    }

    @Test("a picture of the AF slider held at the operator's value while the Core has not answered")
    func sliderHeldShot() async throws {
        let (model, station, _) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        rx.setAfGain(85)
        #expect(await settle { rx.afGain == 85 })
        _ = try #require(await nextWrite(station, key: key, "afGain"))
        try await shootBoth("liveui-slider-held-slow-answer", rx: rx, main: model.main)
        #expect(rx.afGain == 85)
        await model.disconnect()
    }

    @Test("an answer that never comes restores Core and says it is not confirmed; a late answer settles it")
    func droppedAnswerIsNotConfirmed() async throws {
        let (model, station, clock) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        let before = try #require(rx.afGain)
        let wanted = before == 20 ? 30.0 : 20.0
        rx.setAfGain(wanted)
        let write = try #require(await nextWrite(station, key: key, "afGain"))
        await clock.advance(by: 5_000)
        #expect(await settle { rx.note == PropertyWriteOutcome.notConfirmed.reason })
        #expect(rx.afGain == before)
        #expect(model.mirror.isUnconfirmed(key, property: "afGain"))
        await clock.advance(by: 60_000)
        #expect(rx.afGain == before)
        await result(station, key: key, write, accepted: true, kept: write.properties[0].value)
        #expect(await settle { rx.note == nil && !model.mirror.isUnconfirmed(key, property: "afGain") && rx.afGain == wanted })
        #expect(rx.afGain == wanted)
        await model.disconnect()
    }

    @Test("a refusal returns to the Core's value and shows its words at the panel")
    func refusalShowsTheCoresWords() async throws {
        let (model, station, _) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        let anf = try #require(rx.noise.first { $0.kind == .toggle("anfEnabled") })
        let wasLit = anf.lit
        rx.tap(anf)
        #expect(await settle { rx.noise.first { $0.id == anf.id }?.lit == !wasLit })
        let write = try #require(await nextWrite(station, key: key, "anfEnabled"))
        let reason = "That slice belongs to MacBook Pro."
        await result(station, key: key, write, accepted: false, reason: reason, kept: .bool(wasLit))
        #expect(await settle { rx.note == reason })
        #expect(rx.noise.first { $0.id == anf.id }?.lit == wasLit)
        try await shootBoth("liveui-refusal-reason", rx: rx, main: model.main)
        await model.disconnect()
    }

    @Test("a newer touch replaces an older one waiting; only the newest is sent next")
    func newestTouchWins() async throws {
        let (model, station, _) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        rx.setAfGain(40)
        let first = try #require(await nextWrite(station, key: key, "afGain"))
        rx.setAfGain(45)
        rx.setAfGain(50)
        #expect(await settle { rx.afGain == 50 })
        await result(station, key: key, first, accepted: true, kept: first.properties[0].value)
        let second = try #require(await nextWrite(station, key: key, "afGain"))
        #expect([LinkMessage.PropertyValue.i64(50), .f64(50)].contains(second.properties[0].value))
        #expect(rx.afGain == 50)
        await result(station, key: key, second, accepted: true, kept: second.properties[0].value)
        await idle()
        #expect(writes(station, key: key, "afGain").count == 2)
        #expect(rx.afGain == 50)
        await model.disconnect()
    }

    @Test("a dropped link shows the Core's value, says the change was not confirmed, and sends nothing again")
    func linkDropShowsTheCoresValue() async throws {
        let (model, station, _) = try await connected()
        let rx = model.main.rx
        let key = try #require(rx.slice?.key)
        let before = try #require(rx.afGain)
        let wanted = before == 20 ? 30.0 : 20.0
        rx.setAfGain(wanted)
        #expect(await settle { rx.afGain == wanted })
        _ = try #require(await nextWrite(station, key: key, "afGain"))
        await station.dropLink()
        #expect(await settle { rx.note == PropertyWriteOutcome.linkLost.reason })
        #expect(await settle { rx.afGain == before })
        await idle()
        #expect(writes(station, key: key, "afGain").count == 1)
        await model.disconnect()
    }

    // MARK: Inside

    private func connected() async throws -> (AppModel, FakeStation, TestLinkClock) {
        let clock = TestLinkClock()
        let station = try FakeStation()
        let defaults = try #require(UserDefaults(suiteName: "LiveControlsTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults), mirrorClock: clock)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        let json = try #require(ModesTabBindingTests.catalogueJSON(ModesTabBindingTests.anan))
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        // The AF slider sends nothing until the catalogue's range has arrived.
        #expect(await settle {
            model.main.rx.afGain != nil && model.main.rx.afRange != nil && !model.main.rx.noise.isEmpty
        })
        return (model, station, clock)
    }

    private func writes(_ station: FakeStation, key: String, _ property: String) -> [LinkMessage.PropertyWrite] {
        station.messages.compactMap { message in
            if case .propertyWrite(let write) = message, write.key == key, write.properties.first?.name == property {
                return write
            }
            return nil
        }
    }

    /// The app's next write of `property` this test has not taken yet.
    private func nextWrite(_ station: FakeStation, key: String, _ property: String) async -> LinkMessage.PropertyWrite? {
        let done = answered.all
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == key && write.properties.first?.name == property && !done.contains(write.writeId ?? 0)
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent else {
            Issue.record("no write of \(key).\(property) reached the Core")
            return nil
        }
        answered.insert(write.writeId ?? 0)
        return write
    }

    private func result(_ station: FakeStation, key: String, _ write: LinkMessage.PropertyWrite, accepted: Bool,
                        reason: String = "", kept: LinkMessage.PropertyValue) async {
        let entry = write.properties[0]
        let value = LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: entry.name, value: kept)
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: key, writeId: write.writeId ?? 0, results: [
            .init(property: entry.name, accepted: accepted, reason: reason, value: value),
        ])))
        if accepted {
            await station.deliver(.delta(LinkMessage.Delta(key: key, properties: [value])))
        }
    }

    /// Another device's change of the written property, as the Core's delta.
    private func delta(_ station: FakeStation, key: String, _ write: LinkMessage.PropertyWrite,
                       _ value: LinkMessage.PropertyValue) async {
        let entry = write.properties[0]
        await station.deliver(.delta(LinkMessage.Delta(key: key, properties: [
            .init(ordinal: entry.ordinal, name: entry.name, value: value),
        ])))
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

    private func idle() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    // MARK: Pictures

    private func shootBoth(_ name: String, rx: RxPanelModel, main: MainScreenModel) async throws {
        for scheme in [ColorScheme.light, .dark] {
            try await shoot("\(name)-\(scheme == .dark ? "dark" : "light")", scheme: scheme) {
                // The existing non-scrolling composition avoids the simulator's
                // ScrollView edge effect obscuring AF in this narrow capture.
                RxPanel(model: rx, sliceColour: main.sliceColour, scrolls: false)
            }
        }
    }

    private func shoot(_ name: String, scheme: ColorScheme, @ViewBuilder content: () -> some View) async throws {
        let size = CGSize(width: RxPanel.width, height: 700)
        let window = try BandFlagShotTests.window(size: size)
        let host = UIHostingController(rootView: content()
            .frame(width: size.width, height: size.height, alignment: .top)
            .preferredColorScheme(scheme))
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.safeAreaRegions = []
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
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_LIVEUI_SHOTS"], !directory.isEmpty,
              let data = image.pngData() else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try? data.write(to: url)
        print("Wrote \(url.path)")
    }
}
