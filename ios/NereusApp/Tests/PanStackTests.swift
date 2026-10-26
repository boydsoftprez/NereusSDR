// NereusSDR for iOS: the iPad's pans against a fake Core: the radio's limit, Add a pan, Close this pan, a smaller radio, the iPhone
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
import UIKit

/// R-IOS-59, DC9, DC10 (spec section 6.3): the pans the iPad shows, up to
/// the radio's limit; a new pan takes a spare slice or asks the Core for
/// one; a closed pan's slices move to Pan 1 and its own values stay; a
/// smaller radio closes the highest pans without writing to their slices;
/// an iPhone never opens a second pan. Each action's status line names the
/// slices that moved, and every refusal shows the Core's words.
@Suite("Pan stack", .serialized)
@MainActor
struct PanStackTests {
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<5_000 {
            await Task.yield()
        }
    }

    @MainActor
    private struct Connected {
        let model: AppModel
        let station: FakeStation
        let store: BandDisplaySettingsStore
        var stack: PanStackModel { model.main.panStack }
    }

    /// A model connected to a fake Core (five slices, two receivers) with
    /// the ANAN-G2 catalogue, as an iPad.
    private func connected(media: Bool = false, budget: Bool = false) async throws -> Connected {
        // The extended view's fake answers subscribes and opens media, as PanSheetTests' withMedia;
        // with the budget wire it answers each with an allocation-result under the Core's ID rules.
        let station = try FakeStation(additions: media ? (budget ? [.wideband, .displayBudget] : .wideband) : [])
        let suite = "PanStackTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let store = BandDisplaySettingsStore(defaults: defaults)
        let model = media ? AppModel(mediaPeerFactory: station.mediaPeerFactory, displaySettings: store)
            : AppModel(displaySettings: store)
        model.main.panStack.allowsMorePans = true
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { model.main.catalogFeed.catalog != nil && model.main.pan.sliceLetter == "A" })
        await quiet()
        return Connected(model: model, station: station, store: store)
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap {
            if case .commandInvoke(let invoke) = $0, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    /// Every `panKey` write the app sent: the slice's key and the pan.
    private func panKeyWrites(_ station: FakeStation) -> [(key: String, pan: String)] {
        station.messages.compactMap {
            if case .propertyWrite(let write) = $0, let entry = write.properties.first(where: { $0.name == "panKey" }),
               case .utf8(let pan) = entry.value {
                return (write.key, pan)
            }
            return nil
        }
    }

    private static let second = PanIdentity(number: 2)

    // MARK: The limit

    /// The PR 653 re-review d, point b: a finger on a pan marks it as it
    /// comes down. The pan's recognizer says so at the touch's start and
    /// fails at once, so it takes, delays and cancels no touch and blocks no
    /// other gesture.
    @Test("a pan's touch-down recognizer says when a finger comes down and takes no touch")
    func touchDownMarksAndTakesNothing() {
        let recognizer = PanTouchDownRecognizer.make()
        #expect(!recognizer.cancelsTouchesInView && !recognizer.delaysTouchesBegan && !recognizer.delaysTouchesEnded)
        var downs = 0
        recognizer.touched = { downs += 1 }
        recognizer.touchesBegan([], with: UIEvent())
        #expect(downs == 1)
        // It never recognizes. (Outside UIKit's own delivery of a touch, the
        // move to failed it makes is not kept, so only that is checked here.)
        #expect(![.began, .changed, .ended].contains(recognizer.state))
    }

    @Test("Add a pan works up to the radio's limit, then greys with the radio's name and the number as a word")
    func limitAndItsReason() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(stack.limit == 2)
        #expect(stack.open == [.first])
        #expect(stack.addPanState() == (true, nil))
        stack.addPan()
        #expect(await settle { stack.status != nil })
        #expect(stack.open == [.first, Self.second])
        #expect(stack.addPanState() == (false, "The ANAN-G2 can show two pans."))
        // At the limit Add a pan sends nothing.
        let asked = invokes(rig.station, PanSheetModel.addSliceVerb).count
        stack.addPan()
        await quiet()
        #expect(invokes(rig.station, PanSheetModel.addSliceVerb).count == asked)
        #expect(stack.open == [.first, Self.second])
        // One shared helper words both limits.
        #expect(RadioWords.panLimit(radio: "Hermes Lite 2", limit: 2) == "The Hermes Lite 2 can show two pans.")
        #expect(RadioWords.panLimit(radio: nil, limit: 1) == "The radio can show one pan.")
        #expect(RadioWords.sliceLimit(radio: "ANAN-G2", limit: 5) == "The ANAN-G2 runs five slices.")
        await rig.model.disconnect()
    }

    @Test("Close this pan is greyed on Pan 1 with its reason")
    func firstStaysOpen() async throws {
        let rig = try await connected()
        #expect(rig.stack.closePanState(.first) == (false, "Pan 1 stays open."))
        rig.stack.closePan(.first)
        await quiet()
        #expect(rig.stack.open == [.first])
        #expect(panKeyWrites(rig.station).isEmpty)
        await rig.model.disconnect()
    }

    @Test("before the Core's snapshot is whole Add a pan waits for the Core")
    func notConnected() {
        let model = AppModel(displaySettings: BandDisplaySettingsStore(
            defaults: UserDefaults(suiteName: "PanStackTests-\(UUID().uuidString)") ?? .standard))
        model.main.panStack.allowsMorePans = true
        #expect(model.main.panStack.addPanState() == (false, "Wait for the Core to connect."))
    }

    // MARK: Add a pan

    @Test("with no spare slice Add a pan asks for a slice on the new pan by its Core key and never moves Pan 1's only slice")
    func addWithoutASpare() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.addPan()
        // While the Core has not answered, the new pan cannot be closed: its slice would be stranded.
        #expect(stack.closePanState(Self.second) == (false, "Wait for the Core to finish opening this pan."))
        stack.closePan(Self.second)
        #expect(stack.open == [.first, Self.second])
        #expect(await settle { stack.status != nil })
        #expect(stack.closePanState(Self.second) == (true, nil))
        let sent = try #require(invokes(rig.station, PanSheetModel.addSliceVerb).last)
        #expect(sent.args == [.init(name: "panId", value: .utf8("pan-1"))])
        #expect(stack.status == "Pan 2 opened below Pan 1. Slice B added on it.")
        // The band shows it for a moment from when it was set (the board's 3.2 s).
        #expect(stack.statusSerial > 0 && stack.statusSince != nil)
        #expect(rig.station.panKey(ofSlice: 1) == "pan-1")
        // Pan 1's single slice stays where it is.
        #expect(rig.station.panKey(ofSlice: 0) == "pan-0")
        #expect(panKeyWrites(rig.station).isEmpty)
        #expect(stack.lastTouched == Self.second)
        // Each band draws its own pan's slices.
        #expect(await settle { stack.band(for: Self.second).slices.entries.map(\.id) == [1] })
        #expect(stack.band(for: .first).slices.entries.map(\.id) == [0])
        await rig.model.disconnect()
    }

    @Test("with a spare Add a pan moves it by its panKey and says which slice came from where")
    func addTakesASpare() async throws {
        let rig = try await connected()
        let stack = rig.stack
        // +RX adds a second slice on Pan 1.
        rig.model.main.pan.addSlice(on: .first)
        #expect(await settle { rig.model.main.slices.entries.count == 2 })
        let asked = invokes(rig.station, PanSheetModel.addSliceVerb).count
        stack.addPan()
        #expect(await settle { stack.status != nil })
        #expect(stack.status == "Pan 2 opened below Pan 1. Slice B moved to it from Pan 1, which had a spare.")
        #expect(panKeyWrites(rig.station).map(\.key) == ["slice:1"])
        #expect(panKeyWrites(rig.station).map(\.pan) == ["pan-1"])
        #expect(await settle { rig.station.panKey(ofSlice: 1) == "pan-1" })
        #expect(rig.station.panKey(ofSlice: 0) == "pan-0")
        // Nothing new was asked for.
        #expect(invokes(rig.station, PanSheetModel.addSliceVerb).count == asked)
        await rig.model.disconnect()
    }

    @Test("a refused Add a pan closes the new pan again and shows the Core's words")
    func refusedAdd() async throws {
        let rig = try await connected()
        let stack = rig.stack
        let words = "No receiver is free for another pan."
        rig.station.refuseNext(PanSheetModel.addSliceVerb, reason: words)
        stack.addPan()
        #expect(await settle { stack.status != nil })
        #expect(stack.status == words)
        #expect(stack.open == [.first])
        #expect(stack.lastTouched == .first)
        #expect(stack.addPanState() == (true, nil))
        await rig.model.disconnect()
    }

    // MARK: Close this pan

    @Test("Close this pan moves its slices to Pan 1 by panKey, keeps its own values and says where they went")
    func closeMovesSlicesHome() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.addPan()
        #expect(await settle { stack.status != nil && rig.station.panKey(ofSlice: 1) == "pan-1" })
        #expect(await settle { stack.band(for: Self.second).slices.entries.map(\.id) == [1] })
        // Pan 2 keeps a value of its own.
        var own = rig.store.settings(forPan: Self.second.storeKey)
        own.noiseFloorLine.toggle()
        rig.store.setSettings(own, forPan: Self.second.storeKey)
        let kept = rig.store.settings(forPan: Self.second.storeKey)
        let ownFields = rig.store.ownFields(forPan: Self.second.storeKey, among: BandDisplaySettingsStore.pictureFields)

        #expect(stack.closePanState(Self.second) == (true, nil))
        stack.closePan(Self.second)
        #expect(stack.open == [.first])
        #expect(stack.lastTouched == .first)
        #expect(await settle { rig.station.panKey(ofSlice: 1) == "pan-0" })
        #expect(panKeyWrites(rig.station).map(\.key) == ["slice:1"])
        #expect(panKeyWrites(rig.station).map(\.pan) == ["pan-0"])
        let status = try #require(stack.status)
        #expect(status.hasPrefix("Pan 2 closed. Slice B moved to Pan 1, at "))
        #expect(status.hasSuffix(" Pan 2's own values are kept for next time."))
        // Its own values are where they were.
        #expect(rig.store.settings(forPan: Self.second.storeKey) == kept)
        #expect(rig.store.ownFields(forPan: Self.second.storeKey, among: BandDisplaySettingsStore.pictureFields)
                == ownFields)
        // Pan 1 alone draws every slice again.
        #expect(await settle { rig.model.main.slices.entries.map(\.id) == [0, 1] })
        await rig.model.disconnect()
    }

    @Test("a refused move home without the Core's words still says it was refused")
    func refusedRehomeWithoutWords() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.addPan()
        #expect(await settle { stack.status != nil && rig.station.panKey(ofSlice: 1) == "pan-1" })
        #expect(await settle { stack.band(for: Self.second).slices.entries.map(\.id) == [1] })
        rig.station.refuseNext(FakeStation.panKeyRefusalKey(sliceId: 1), reason: "")
        stack.closePan(Self.second)
        #expect(await settle { stack.status == BandSlicesModel.refusedText })
        #expect(rig.station.panKey(ofSlice: 1) == "pan-1")
        await rig.model.disconnect()
    }

    // MARK: A smaller radio and a lost link

    @Test("a Core reporting fewer pans closes the highest with the limit's words and writes nothing to their slices")
    func lowerLimitClosesWithoutWrites() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.addPan()
        #expect(await settle { stack.status != nil && stack.open.count == 2 })
        let writes = panKeyWrites(rig.station).count
        // The radio the Core runs now has one receiver.
        let lowered = rig.model.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name,
                                      value: name == "userDdcCount" ? .i64(1) : value.wireValue)
        }
        await rig.station.deliver(.capabilities(LinkMessage.Capabilities(properties: lowered)))
        #expect(await settle { stack.open == [.first] })
        #expect(stack.limit == 1)
        #expect(stack.status == "The ANAN-G2 can show one pan. Pan 2 closed.")
        await quiet()
        #expect(panKeyWrites(rig.station).count == writes)
        #expect(stack.addPanState() == (false, "The ANAN-G2 can show one pan."))
        await rig.model.disconnect()
    }

    @Test("a lost link closes no pan")
    func lostLinkKeepsPans() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.addPan()
        #expect(await settle { stack.status != nil && stack.open.count == 2 })
        let status = stack.status
        await rig.model.disconnect()
        await quiet()
        #expect(stack.open == [.first, Self.second])
        #expect(stack.status == status)
    }

    // MARK: The iPhone

    @Test("an iPhone shows one pan and never opens another")
    func iPhoneNeverOpensASecondPan() async throws {
        let rig = try await connected()
        let stack = rig.stack
        stack.allowsMorePans = false
        #expect(stack.addPanState() == (false, "An iPhone shows one pan."))
        stack.addPan()
        await quiet()
        #expect(stack.open == [.first])
        #expect(invokes(rig.station, PanSheetModel.addSliceVerb).isEmpty)
        #expect(panKeyWrites(rig.station).isEmpty)
        await rig.model.disconnect()
    }

    // MARK: The pan last touched

    /// Every property write the app sent for `property`: the slice's key.
    private func writes(_ station: FakeStation, _ property: String) -> [String] {
        station.messages.compactMap {
            if case .propertyWrite(let write) = $0, write.properties.contains(where: { $0.name == property }) {
                return write.key
            }
            return nil
        }
    }

    /// Two pans, slice A on Pan 1 and slice B moved onto Pan 2 by Add a pan.
    private func twoPans(_ rig: Connected) async throws {
        let main = rig.model.main
        main.pan.addSlice(on: .first)
        #expect(await settle { main.slices.entries.count == 2 })
        rig.stack.addPan()
        #expect(await settle { rig.stack.status != nil && rig.station.panKey(ofSlice: 1) == "pan-1" })
        #expect(await settle { rig.stack.band(for: Self.second).slices.entries.map(\.id) == [1] })
        #expect(await settle { rig.stack.band(for: Self.second).slices.activeSliceId == 1 })
        #expect(await settle { main.slices.entries.map(\.id) == [0] })
    }

    @Test("with two pans the RX panel, Modes, Match RX, the S-meter and the toolbar follow the pan last touched")
    func rxPanelFollowsThePanLastTouched() async throws {
        let rig = try await connected()
        let main = rig.model.main
        let stack = rig.stack
        try await twoPans(rig)
        let second = stack.band(for: Self.second)
        // Add a pan touched Pan 2: the toolbar names it, with its active slice.
        #expect(stack.lastTouched == Self.second)
        #expect(main.toolbarPan == Self.second)
        #expect(await settle { main.sliceLetter == "B" && main.rx.sliceLetter == "B" })
        #expect(main.rx.slices === second.slices)
        #expect(main.modes.slices === second.slices)
        #expect(main.transmit.receiveSlices === second.slices)
        // An RX change reaches Pan 2's active slice, and never Pan 1's.
        let before = writes(rig.station, "agcMode").count
        main.rx.selectAgc(2)
        #expect(await settle { self.writes(rig.station, "agcMode").count == before + 1 })
        #expect(writes(rig.station, "agcMode").last == "slice:1")
        // Pan 1 touched again: everything is back on slice A.
        stack.lastTouched = .first
        #expect(main.toolbarPan == .first)
        #expect(main.rx.slices === main.slices)
        #expect(main.transmit.receiveSlices == nil)
        #expect(await settle { main.sliceLetter == "A" && main.rx.sliceLetter == "A" })
        main.rx.selectAgc(3)
        #expect(await settle { self.writes(rig.station, "agcMode").count == before + 2 })
        #expect(writes(rig.station, "agcMode").last == "slice:0")
        // Pan 2 closing puts the toolbar back on Pan 1 for good.
        stack.lastTouched = Self.second
        #expect(main.toolbarPan == Self.second)
        stack.closePan(Self.second)
        #expect(main.toolbarPan == .first)
        #expect(main.rx.slices === main.slices)
        await rig.model.disconnect()
    }

    @Test("a shared row changed on Pan 2's pill shows on Pan 1's; an own row stays Pan 2's")
    func sharedRowsReachEveryPan() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        let first = main.band
        let second = rig.stack.band(for: Self.second).band
        // Pan 2's pill is open: its rows edit Pan 2.
        main.displayEditScope = .pan(Self.second)
        #expect(main.displayEditingSettings == second.settings)
        let ctun = first.settings.ctun
        let rewind = first.settings.rewindSeconds == 900 ? 300 : 900
        main.changeDisplay { settings in
            settings.ctun = !ctun
            settings.displayDuplex = !settings.displayDuplex
            settings.rewindSeconds = rewind
            settings.decimation = 4
        }
        #expect(second.settings.ctun == !ctun)
        #expect(first.settings.ctun == !ctun)
        #expect(first.settings.displayDuplex == second.settings.displayDuplex)
        #expect(first.settings.rewindSeconds == rewind)
        #expect(first.settings.decimation == 4)
        // Spectrum Height from Pan 2's own Display model reaches Pan 1 too.
        main.controls(for: Self.second).display.setSpectrumHeight(61)
        #expect(second.settings.currentSpectrumSharePercent == 61)
        #expect(first.settings.currentSpectrumSharePercent == 61)
        // An own row: Pan 2's fill changes, Pan 1's does not.
        let fill = first.settings.traceFill
        main.changeDisplay { $0.traceFill = !fill }
        #expect(second.settings.traceFill == !fill)
        #expect(first.settings.traceFill == fill)
        #expect(main.ownDisplayFields(for: Self.second).contains("traceFill"))
        #expect(!main.ownDisplayFields(for: .first).contains("traceFill"))
        // Pan 1's pill sees Pan 1's own value again.
        main.displayEditScope = .pan(.first)
        #expect(main.displayEditingSettings.traceFill == fill)
        await rig.model.disconnect()
    }

    @Test("each pan's controls act on its own slices; Pan 1's are the main screen's")
    func eachPanHasItsOwnControls() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        let first = main.controls(for: .first)
        #expect(first.sheet === main.pan)
        #expect(first.display === main.display)
        #expect(first.flagControls === main.flagControls)
        let second = main.controls(for: Self.second)
        #expect(second.band.slices === rig.stack.band(for: Self.second).slices)
        #expect(second.sheet !== main.pan)
        #expect(main.controls(for: Self.second) === second)
        #expect(await settle { second.sheet.sliceLetter == "B" })
        // Pan 2's +RX asks for a slice on Pan 2.
        second.sheet.addSlice(on: Self.second)
        #expect(await settle { self.invokes(rig.station, PanSheetModel.addSliceVerb).last?.args
                == [.init(name: "panId", value: .utf8("pan-1"))] })
        // Closed, its controls go; Pan 2 opened again gets new ones.
        rig.stack.closePan(Self.second)
        #expect(main.controls(for: Self.second) === first)
        await rig.model.disconnect()
    }

    // MARK: Pan 2's display

    /// The subscriptions the app sent the fake, in order.
    private func subscribes(_ station: FakeStation) -> [[String: LinkJSON]] {
        station.messages.compactMap {
            if case .mediaControl(let control) = $0, control.payload["op"] == .string("subscribe") {
                return control.payload
            }
            return nil
        }
    }

    @Test("Pan 2 subscribes on its own slice with its own endpoint, and a Core that refuses it says why on Pan 2")
    func secondPanDisplayRefused() async throws {
        let rig = try await connected(media: true)
        let stack = rig.stack
        _ = rig.model.main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        #expect(await settle { !self.subscribes(rig.station).isEmpty })
        #expect(subscribes(rig.station).allSatisfy { $0["endpointId"] == .number(1) })
        // One counter for every pan on the media connection: Pan 2's first endpoint is the next after Pan 1's.
        let pan2Endpoint: UInt32 = 2
        let words = "The Core has no display free for another pan."
        rig.station.refuseNext(FakeStation.subscribeRefusalKey(pan2Endpoint), reason: words)
        stack.addPan()
        #expect(await settle { stack.status != nil && rig.station.panKey(ofSlice: 1) == "pan-1" })
        // The fake's new slice carries its index and pan; give it a receiver's rate and a frequency.
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_213_000)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        _ = stack.band(for: Self.second).band.prepareToDraw(size: CGSize(width: 1206, height: 700), scale: 3)
        #expect(await settle {
            self.subscribes(rig.station).contains { $0["endpointId"] == .number(Double(pan2Endpoint)) }
        })
        let second = try #require(subscribes(rig.station).first { $0["endpointId"] == .number(Double(pan2Endpoint)) })
        #expect(second["sliceId"] == .number(1))
        #expect(await settle { stack.displayRefusals[Self.second] == words })
        #expect(stack.band(for: Self.second).subscriber.displayRefusal == words)
        #expect(stack.displayRefusals[.first] == nil)
        // Closing the pan clears its refusal.
        stack.closePan(Self.second)
        #expect(stack.displayRefusals.isEmpty)
        await rig.model.disconnect()
    }

    // MARK: Endpoint IDs on the budget wire (the Core's high-water rule)

    /// Opens Pan 2 with slice B on it, tuned and drawn, and waits for its display.
    private func openSecondPan(_ rig: Connected) async throws -> PanBand {
        let stack = rig.stack
        let before = stack.status
        stack.addPan()
        #expect(await settle { stack.status != before && stack.open.contains(Self.second) })
        #expect(await settle { rig.station.panKey(ofSlice: 1) == "pan-1" })
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_213_000)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        let band = stack.band(for: Self.second)
        _ = band.band.prepareToDraw(size: CGSize(width: 1206, height: 700), scale: 3)
        #expect(await settle { band.subscriber.acceptedDisplay != nil || band.subscriber.displayRefusal != nil })
        return band
    }

    @Test("on the budget wire, Pan 1 coming back after Pan 2 subscribed is accepted: its new ID is above Pan 2's")
    func firstPanResubscribesAboveSecond() async throws {
        let rig = try await connected(media: true, budget: true)
        let first = rig.stack.band(for: .first)
        _ = first.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        #expect(await settle { first.subscriber.acceptedDisplay != nil })
        let second = try await openSecondPan(rig)
        #expect(second.subscriber.displayRefusal == nil)
        let secondId = try #require(second.subscriber.acceptedDisplay?.subscription.endpointId)
        // Sound only lets every pan's endpoint go; full display brings each back on a new one.
        first.subscriber.session = .none
        #expect(await settle { first.subscriber.acceptedDisplay == nil && second.subscriber.acceptedDisplay == nil })
        first.subscriber.session = SessionPolicy.Mode.full.request
        #expect(await settle {
            (first.subscriber.acceptedDisplay != nil || first.subscriber.displayRefusal != nil)
                && (second.subscriber.acceptedDisplay != nil || second.subscriber.displayRefusal != nil)
        })
        #expect(first.subscriber.displayRefusal == nil)
        #expect(second.subscriber.displayRefusal == nil)
        let firstBack = try #require(first.subscriber.acceptedDisplay?.subscription.endpointId)
        #expect(firstBack > secondId)
        await rig.model.disconnect()
    }

    @Test("on the budget wire, Pan 2 closed and opened again is accepted: it never asks for a closed ID")
    func reopenedSecondPanIsAccepted() async throws {
        let rig = try await connected(media: true, budget: true)
        let first = rig.stack.band(for: .first)
        _ = first.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        #expect(await settle { first.subscriber.acceptedDisplay != nil })
        let opened = try await openSecondPan(rig)
        #expect(opened.subscriber.displayRefusal == nil)
        let closedId = try #require(opened.subscriber.acceptedDisplay?.subscription.endpointId)
        rig.stack.closePan(Self.second)
        #expect(await settle { opened.subscriber.holdsNothing })
        #expect(await settle { rig.station.panKey(ofSlice: 1) == "pan-0" })
        let reopened = try await openSecondPan(rig)
        #expect(reopened.subscriber.displayRefusal == nil)
        let reopenedId = try #require(reopened.subscriber.acceptedDisplay?.subscription.endpointId)
        #expect(reopenedId > closedId)
        #expect(first.subscriber.displayRefusal == nil)
        await rig.model.disconnect()
    }

    // MARK: Each pan's controls act on that pan (final review, part A)

    @Test("Pan 1's own 3D Floor, band-plan size and extended view stay Pan 1's whatever pan Setup or a pill has chosen")
    func firstPanModelsIgnoreTheEditScope() async throws {
        let rig = try await connected(media: true)
        let main = rig.model.main
        try await twoPans(rig)
        let first = main.band
        let second = rig.stack.band(for: Self.second).band
        #expect(await settle { main.pan.extendedViewAvailable })
        for scope in [MainScreenModel.DisplayEditScope.pan(Self.second), .all] {
            main.displayEditScope = scope
            let secondBefore = second.settings
            let floor = first.settings.threeDFloorDb == 12 ? 18 : 12
            main.display.setStackFloor(Double(floor))
            let size: BandPlanSize = first.settings.bandPlanSize == .large ? .small : .large
            main.display.setBandPlanSize(size)
            let extended = first.settings.extendedView
            main.pan.toggleExtendedView()
            #expect(first.settings.threeDFloorDb == floor)
            #expect(first.settings.bandPlanSize == size)
            #expect(first.settings.extendedView == !extended)
            #expect(second.settings == secondBefore)
        }
        await rig.model.disconnect()
    }

    @Test("Pan 2's flag Diversity is the one Diversity, acting on the active slice of the pan last touched")
    func secondPanDiversityFollowsItsPan() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        main.flagControls.diversity = rig.model.diversity
        let second = main.controls(for: Self.second)
        #expect(second.flagControls !== main.flagControls)
        #expect(second.flagControls.diversity === rig.model.diversity)
        rig.stack.lastTouched = Self.second
        #expect(await settle { rig.model.diversity.activeTargetLetter == "B" })
        rig.stack.lastTouched = .first
        #expect(await settle { rig.model.diversity.activeTargetLetter == "A" })
        await rig.model.disconnect()
    }

    @Test("Pan 2's dial plays the haptics Setup sets, and so does a pan opened later")
    func secondPanDialHaptics() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        let second = main.controls(for: Self.second)
        rig.model.phoneSettings.dialDetentTicks = false
        #expect(await settle { main.tuning.dialHaptics.ticksOnDetents == false })
        #expect(second.tuning.dialHaptics.ticksOnDetents == false)
        rig.stack.closePan(Self.second)
        #expect(await settle { rig.station.panKey(ofSlice: 1) == "pan-0" })
        rig.stack.addPan()
        #expect(await settle { rig.stack.open.contains(Self.second) })
        #expect(main.controls(for: Self.second).tuning.dialHaptics.ticksOnDetents == false)
        rig.model.phoneSettings.dialDetentTicks = true
        await rig.model.disconnect()
    }

    @Test("keyed on Pan 2's slice, Pan 2 shows the transmit display and Pan 1 does not, as the desktop's rise does")
    func transmitDisplayOnTheTransmitPan() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        let second = rig.stack.band(for: Self.second).band
        // The Core says its radio transmits on slice B (no key is sent: the fake's mirror says so).
        let transmitting = LinkMessage.PropertyEntry(ordinal: 0, name: "transmitting", value: .bool(true))
        if rig.model.mirror.object("radio") == nil {
            await rig.station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel",
                                                                            properties: [transmitting])))
        } else {
            await rig.station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [transmitting])))
        }
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 20, name: "txSlice", value: .bool(true)),
        ])))
        #expect(await settle { second.transmit.keyedHere })
        await quiet()
        #expect(!main.band.transmit.keyedHere)
        await rig.model.disconnect()
    }

    @Test("this device's slice on a pan not shown is drawn on Pan 1, as the desktop rehomes it")
    func sliceOnAPanNotShownIsDrawnOnTheFirst() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-2")),
        ])))
        #expect(await settle { main.slices.entries.map(\.id) == [0, 1] })
        #expect(rig.stack.band(for: Self.second).slices.entries.isEmpty)
        await rig.model.disconnect()
    }

    @Test("with no spare slice and the radio's slices full, Add a pan greys with the slice limit's words")
    func addGreysAtTheSliceLimit() async throws {
        let rig = try await connected()
        let main = rig.model.main
        let limited = rig.model.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: name == "effectiveMaxSlices" ? .i64(2)
                                          : name == "userDdcCount" ? .i64(2) : value.wireValue)
        }
        await rig.station.deliver(.capabilities(LinkMessage.Capabilities(properties: limited)))
        main.pan.addSlice(on: .first)
        #expect(await settle { main.slices.entries.count == 2 })
        #expect(rig.stack.addPanState() == (true, nil))
        // Slice B on no pan: Pan 1 holds one slice, none is spare, and both the radio runs are taken.
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("")),
        ])))
        #expect(await settle { rig.stack.addPanState() == (false, "The ANAN-G2 runs two slices.") })
        let asked = invokes(rig.station, PanSheetModel.addSliceVerb).count
        rig.stack.addPan()
        await quiet()
        #expect(invokes(rig.station, PanSheetModel.addSliceVerb).count == asked)
        await rig.model.disconnect()
    }

    @Test("a later identity change reaches every pan's slices")
    func identityReachesEveryPan() async throws {
        let rig = try await connected()
        let main = rig.model.main
        try await twoPans(rig)
        main.slices.thisDeviceId = "device-later"
        #expect(rig.stack.band(for: Self.second).slices.thisDeviceId == "device-later")
        await rig.model.disconnect()
    }

    @Test("Setup's strip knows each open pan's slices, so an open Pan 2 is never called not open")
    func setupStripSeesEveryOpenPan() async throws {
        let rig = try await connected()
        try await twoPans(rig)
        #expect(DisplayStripPans.slicesByPan(rig.stack) == [.first: ["A"], Self.second: ["B"]])
        await rig.model.disconnect()
    }
}
