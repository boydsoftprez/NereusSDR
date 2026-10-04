// NereusSDR for iOS: the slice list and another slice's band, one test per ruling of 2026-09-30
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-42, R-IOS-11, R-IOS-13, R-IOS-16: JJ's rulings of 2026-09-30 on
/// the slice list and showing another slice's band, against the mirror
/// and a recording Core. The cast: slice A is this phone's, on its own
/// receiver (0) at 7.236.400 with transmit; slice B is the shack desktop's
/// on receiver 1 at 14.074.000, which this phone listens to; slice C is
/// the desktop's too, which this phone is not in (a marker only).
@Suite("Slice list and another slice's band")
@MainActor
struct SliceListTests {
    final class Outbox: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [LinkMessage] = []

        var messages: [LinkMessage] {
            lock.lock()
            defer { lock.unlock() }
            return stored
        }

        func record(_ message: LinkMessage) {
            lock.lock()
            stored.append(message)
            lock.unlock()
        }

        func invokes(_ verb: String) -> [LinkMessage.CommandInvoke] {
            messages.compactMap { message in
                if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                    return invoke
                }
                return nil
            }
        }

        func writes(_ key: String, _ property: String) -> [LinkMessage.PropertyWrite] {
            messages.compactMap { message in
                if case .propertyWrite(let write) = message, write.key == key,
                   write.properties.first?.name == property {
                    return write
                }
                return nil
            }
        }
    }

    static let me = "me"
    static let desktop = "shack-desktop"
    static let bHz = 14_074_000.0
    /// The own band's view: 7.2 MHz, 200 kHz wide.
    static let homeView = 7_136_400.0...7_336_400.0

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    static func slice(_ index: Int64, hz: Double, stream: Int64, active: Bool, tx: Bool) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 2, name: "dspMode", value: .i64(0)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 11, name: "active", value: .bool(active)),
            .init(ordinal: 12, name: "txSlice", value: .bool(tx)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(index)),
            .init(ordinal: 14, name: "afGain", value: .f64(40)),
            .init(ordinal: 24, name: "streamCtunPinned", value: .bool(false)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 40, name: "streamIndex", value: .i64(stream)),
        ]))
    }

    static func access(_ id: Int64, _ controller: String, revision: Int64, onAir: Bool = false) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "access:\(id)", className: SliceAccess.accessClass, properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(id)),
            .init(ordinal: 1, name: "incarnation", value: .i64(40 + id)),
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
            .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8("[]")),
            .init(ordinal: 7, name: "onAir", value: .bool(onAir)),
        ]))
    }

    static func onAir(_ id: Int64, _ onAir: Bool) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "access:\(id)", properties: [
            .init(ordinal: 7, name: "onAir", value: .bool(onAir)),
        ]))
    }

    static func marker(_ id: Int64, hz: Double, stream: Int64) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "marker:\(id)", className: SeveralDevices.markerClass, properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(id)),
            .init(ordinal: 1, name: "ownerDeviceId", value: .utf8(desktop)),
            .init(ordinal: 2, name: "ownerName", value: .utf8("Shack desktop")),
            .init(ordinal: 3, name: "ownerShortName", value: .utf8("Shack")),
            .init(ordinal: 4, name: "ownerKind", value: .utf8("computer")),
            .init(ordinal: 5, name: "ownerAway", value: .bool(false)),
            .init(ordinal: 6, name: "frequency", value: .f64(hz)),
            .init(ordinal: 7, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 8, name: "streamIndex", value: .i64(stream)),
        ]))
    }

    /// The connected devices, the desktop hosting the Core named "Shack PC".
    static func hostedDevices() -> LinkMessage? {
        guard case .delta(let delta) = SeveralDevicesScreenTests.connectedDevices(
            macBookAwayFor: nil, hostedBy: (name: "Shack PC", short: "Shack")) else {
            return nil
        }
        return .objectCreate(LinkMessage.ObjectCreate(key: SeveralDevices.connectedDevicesKey,
                                                      className: "ConnectedDevices", properties: delta.properties))
    }

    /// The cast on a Core at `version` of slice access, answered and ready.
    private func cast(version: Int64 = 3) async -> (BandSlicesModel, MirrorStore, Outbox, CommandClient) {
        let outbox = Outbox()
        let store = MirrorStore(send: { message in outbox.record(message) })
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "propertyResultVersion", value: .i64(1)),
            .init(name: "remoteCtunVersion", value: .i64(1)),
            .init(name: "remoteTxVersion", value: .i64(1)),
            .init(name: SeveralDevices.capability, value: .i64(1)),
            .init(name: SliceAccess.capability, value: .i64(version)),
        ])))
        store.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        store.apply(Self.slice(0, hz: 7_236_400, stream: 0, active: true, tx: true))
        store.apply(Self.slice(1, hz: Self.bHz, stream: 1, active: false, tx: false))
        store.apply(Self.access(0, Self.me, revision: 1))
        store.apply(Self.access(1, Self.desktop, revision: 12))
        store.apply(Self.access(2, Self.desktop, revision: 20))
        store.apply(Self.marker(2, hz: 3_573_000, stream: 2))
        store.apply(.snapshotComplete)
        let commands = CommandClient(send: { message in outbox.record(message) })
        await commands.handle(.stateChanged(.ready))
        let model = BandSlicesModel(store: store, commands: commands)
        model.thisDeviceId = Self.me
        return (model, store, outbox, commands)
    }

    private func ready(_ model: BandSlicesModel) async -> Bool {
        await settle { model.entries.count == 2 && model.entries[1].listening && model.activeSliceId == 0 }
    }

    private func answer(_ commands: CommandClient, _ outbox: Outbox, _ verb: String, accepted: Bool,
                        reason: String = "", count: Int = 1, values: [LinkMessage.PropertyEntry]? = nil) async -> Bool {
        guard await settle({ outbox.invokes(verb).count >= count }), let invoke = outbox.invokes(verb).last else {
            return false
        }
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: invoke.id, accepted: accepted, reason: reason, affected: [], values: values)))
        return true
    }

    private func activated(_ outbox: Outbox) -> [Int64] {
        outbox.invokes(BandSlicesModel.activateVerb).compactMap { invoke in
            if case .i64(let id)? = invoke.args.first(where: { $0.name == "sliceId" })?.value {
                return id
            }
            return nil
        }
    }

    // MARK: Ruling 1: the Slice button's list, in letter order

    @Test("ruling 1: every live slice is listed in letter order and never regrouped, whatever is shown or taken")
    func listInLetterOrder() async {
        let (model, store, _, commands) = await cast()
        #expect(await ready(model))
        let list = SliceListModel(store: store, slices: model, commands: commands)
        #expect(await settle { list.rows.count == 3 })
        #expect(list.rows.map(\.letter) == ["A", "B", "C"])
        #expect(list.rows.map(\.relation) == [.control, .listening, .none])
        #expect(list.isOwnHere(list.rows[0]) && list.showsBand(list.rows[1]) && !list.showsBand(list.rows[2]))
        #expect(list.liveText == "3 live")
        model.show(1)
        #expect(await settle { model.activeSliceId == 1 })
        try? await Task.sleep(for: .milliseconds(20))
        #expect(list.rows.map(\.letter) == ["A", "B", "C"])
    }

    // MARK: Ruling 2: Listen on another pan jumps; Back to your band

    @Test("ruling 2: Listen on a slice on another pan jumps to its display, and Back to your band brings back the slice left")
    func listenJumpsAndBackReturns() async {
        let (model, store, outbox, commands) = await cast()
        #expect(await ready(model))
        // Listen on C, which this phone is not in: the Core joins it, then the band shows it.
        let listened = Task { await model.listen(2) }
        #expect(await answer(commands, outbox, SliceAccess.listenVerb, accepted: true))
        #expect(await listened.value == nil)
        store.apply(Self.slice(2, hz: 3_573_000, stream: 2, active: false, tx: false))
        #expect(await settle { model.activeSliceId == 2 && model.jumped })
        #expect(model.showingAnotherBand && model.shownEntry?.id == 2)
        // Only C's pan shows, with its full flag.
        #expect(model.bandEntries(viewHz: 3_473_000...3_673_000).flags.map(\.id) == [2])
        #expect(await settle { activated(outbox).last == 2 })
        // Back to your band: A is active again and C stays listened, at the band's edge.
        model.back()
        #expect(model.activeSliceId == 0 && !model.jumped && !model.showingAnotherBand)
        #expect(await settle { activated(outbox).last == 0 })
        let home = model.bandEntries(viewHz: Self.homeView)
        #expect(home.flags.map(\.id) == [0])
        #expect(home.edges.map(\.id) == [1, 2])
        #expect(home.edges.map(\.below) == [false, true])
        #expect(model.entries.first { $0.id == 2 }?.listening == true)
    }

    @Test("ruling 2: a slice asked for that the Core joins and drops in one moment is not shown when it joins later")
    func listenedJoinAndLeaveInOneMoment() async {
        let (model, store, outbox, commands) = await cast()
        #expect(await ready(model))
        let listened = Task { await model.listen(2) }
        #expect(await answer(commands, outbox, SliceAccess.listenVerb, accepted: true))
        #expect(await listened.value == nil)
        // The Core joins C and drops it again in the same turn of the main queue.
        store.apply(Self.slice(2, hz: 3_573_000, stream: 2, active: false, tx: false))
        store.apply(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:2", className: "SliceModel")))
        #expect(await settle { model.entries.map(\.id) == [0, 1] })
        // Later C joins again, not asked for this time: the phone stays on its own band.
        store.apply(Self.slice(2, hz: 3_573_000, stream: 2, active: false, tx: false))
        #expect(await settle { model.entries.map(\.id) == [0, 1, 2] })
        // Whatever the rebuild queued on the main queue has run once this, queued after it, has.
        await Task { @MainActor in }.value
        #expect(model.activeSliceId == 0 && !model.jumped && model.shownEntry == nil)
        #expect(!activated(outbox).contains(2))
    }

    @Test("ruling 2: Back to your band puts back the view of the band the phone left")
    func backRestoresTheView() async throws {
        let (model, store, _, commands) = await cast()
        #expect(await ready(model))
        let main = MainScreenModel(mirror: store, settings: SettingsProxyClient(send: { _ in }), commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        main.slices.thisDeviceId = Self.me
        #expect(await ready(main.slices))
        let left = TuneGestures.View(centerHz: 7_236_400, spanHz: 48_000)
        main.band.requestView(left)
        main.slices.show(1)
        #expect(await settle { main.slices.showingAnotherBand })
        // The band moves to B's display.
        main.band.requestView(TuneGestures.View(centerHz: Self.bHz, spanHz: 192_000))
        main.slices.back()
        #expect(await settle { main.band.requestedView == left })
        _ = model
    }

    // MARK: Ruling 3: the Core refuses the display

    @Test("ruling 3: the Core refusing the display keeps the phone on its own band with the Core's words, still hearing the slice at the edge")
    func displayRefused() async {
        let (model, _, _, _) = await cast()
        #expect(await ready(model))
        model.show(1)
        #expect(model.jumped)
        let words = "No receiver is free on the Core for slice B's display."
        model.displayRefused(sliceId: 1, reason: words)
        #expect(!model.jumped && model.activeSliceId == 0)
        #expect(model.refusal?.text == words)
        #expect(model.entries.first { $0.id == 1 }?.listening == true)
        #expect(model.bandEntries(viewHz: Self.homeView).edges.map(\.id) == [1])
        // A refusal for a slice not shown changes nothing.
        model.displayRefused(sliceId: 1, reason: "other")
        #expect(model.refusal?.text == words)
    }

    // MARK: Ruling 4: every slice can be taken, greyed only on the air

    @Test("ruling 4: Take control is greyed only while the slice is on the air, with the Core's words, at every version", arguments: [Int64(2), 3])
    func onAirGreys(_ version: Int64) async {
        let (model, store, outbox, commands) = await cast(version: version)
        #expect(await ready(model))
        let list = SliceListModel(store: store, slices: model, commands: commands)
        #expect(await settle { list.rows.count == 3 })
        #expect(model.entries[1].takeRefusal == nil && list.rows[2].takeRefusal == nil)
        store.apply(Self.onAir(1, true))
        store.apply(Self.onAir(2, true))
        let words = SliceAccess.transmittingText(1)
        #expect(await settle { model.entries[1].takeRefusal == words })
        #expect(await settle { list.rows[1].takeRefusal == words && list.rows[2].takeRefusal == SliceAccess.transmittingText(2) })
        model.takeControl(1)
        model.takeControl(2)
        #expect(model.taking.isEmpty)
        #expect(outbox.invokes(SliceAccess.takeControlVerb).isEmpty)
        // The transmission ends: live again, and a take goes at once with no question.
        store.apply(Self.onAir(1, false))
        #expect(await settle { model.entries[1].takeRefusal == nil })
        model.takeControl(1)
        #expect(await answer(commands, outbox, SliceAccess.takeControlVerb, accepted: true,
                             values: [.init(name: "controlRevision", value: .i64(13))]))
        #expect(await settle { model.taking.isEmpty && model.takenHere == [1] })
    }

    @Test("ruling 4: a slice this phone is not in can be taken from the list")
    func takeFromTheList() async {
        let (model, store, outbox, commands) = await cast()
        #expect(await ready(model))
        let list = SliceListModel(store: store, slices: model, commands: commands)
        #expect(await settle { list.rows.count == 3 })
        list.takeControl(list.rows[2])
        #expect(await settle { outbox.invokes(SliceAccess.takeControlVerb).count == 1 })
        let invoke = outbox.invokes(SliceAccess.takeControlVerb)[0]
        #expect(invoke.args.first { $0.name == "sliceId" }?.value == .i64(2))
    }

    // MARK: Ruling 5: a taken slice stays on its band and tunes

    @Test("ruling 5: a slice taken while shown stays on its own band until Back, and a tap then tunes it")
    func takenStaysAndTunes() async {
        let (model, store, outbox, commands) = await cast()
        #expect(await ready(model))
        model.show(1)
        // Listening, a tap on the band gives the Core's words and sends nothing.
        model.tap(to: Self.bHz + 1_000)
        #expect(model.refusal?.text == model.entries[1].ownerLine)
        #expect(outbox.writes("slice:1", "frequency").isEmpty)
        model.takeControl(1)
        #expect(await answer(commands, outbox, SliceAccess.takeControlVerb, accepted: true,
                             values: [.init(name: "controlRevision", value: .i64(13))]))
        store.apply(.delta(LinkMessage.Delta(key: "access:1", properties: [
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(Self.me)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(13)),
        ])))
        #expect(await settle { !model.entries[1].listening })
        #expect(model.jumped && model.activeSliceId == 1)
        model.tap(to: Self.bHz + 1_000)
        #expect(await settle { outbox.writes("slice:1", "frequency").count == 1 })
        #expect(outbox.writes("slice:0", "frequency").isEmpty)
        model.back()
        #expect(model.activeSliceId == 0 && !model.jumped)
    }

    // MARK: Ruling 6: controls follow the band; dimmed ones give the words; your volume stays live

    @Test("ruling 6: the Slice button, Modes tab and RX panel follow the shown slice; only this phone's volume changes")
    func controlsFollowTheShownSlice() async {
        let (_, store, outbox, commands) = await cast()
        let main = MainScreenModel(mirror: store, settings: SettingsProxyClient(send: { _ in }), commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        let slices = main.slices
        slices.thisDeviceId = Self.me
        #expect(await ready(slices))
        #expect(await settle { main.sliceLetter == "A" })
        slices.show(1)
        #expect(await settle { main.sliceLetter == "B" && main.rx.sliceLetter == "B" })
        #expect(await settle { main.modes.sliceChoices.first { $0.active }?.id == 1 })
        #expect(main.modes.sliceChoices.first { $0.id == 1 }?.listening == true)
        // Your volume: it starts at the slice's own AF gain and changes only this phone's level.
        #expect(slices.listenLevel(1) == .init(level: 0.4, muted: false))
        let audio = ListenerAudio(slices: slices, sliceId: 1, words: slices.entries[1].ownerLine ?? "",
                                  stopListening: {})
        audio.setGain(75)
        #expect(slices.listenLevel(1).level == 0.75)
        #expect(await settle { outbox.invokes(SliceAccess.setListenLevelVerb).count == 1 })
        #expect(outbox.writes("slice:1", "afGain").isEmpty)
        // A dimmed control's words are the Core's line, as the band's tap gives them.
        #expect(slices.entries[1].ownerLine == "Slice B is controlled by another device. Take control to change it.")
    }

    // MARK: Ruling 7: PTT goes back to the transmit slice's band

    @Test("ruling 7: PTT on a listened slice's band goes back to the transmit slice's band, which is active, and B stays listened")
    func pttGoesBack() async {
        let (model, _, outbox, _) = await cast()
        #expect(await ready(model))
        model.show(1)
        #expect(model.jumped && model.activeSliceId == 1)
        model.backForTransmit()
        #expect(!model.jumped && model.activeSliceId == 0)
        #expect(await settle { activated(outbox).last == 0 })
        // After the transmission the phone stays; B keeps listening, at the edge.
        #expect(model.entries[1].listening)
        #expect(model.bandEntries(viewHz: Self.homeView).edges.map(\.id) == [1])
        // Not showing another slice: PTT changes nothing.
        let before = activated(outbox).count
        model.backForTransmit()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(activated(outbox).count == before)
    }

    // MARK: Ruling 9: the hosting desktop by its name

    @Test("ruling 9: the desktop hosting the Core is named by its device name on the flag, its line and the list")
    func hostingDesktopNamed() async throws {
        let (model, store, _, commands) = await cast()
        #expect(await ready(model))
        store.apply(try #require(Self.hostedDevices()))
        store.apply(.delta(LinkMessage.Delta(key: "access:1", properties: [
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(SliceAccess.stationDeviceId)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(13)),
        ])))
        #expect(await settle { model.entries[1].ownerWords == "Shack PC controls B" })
        #expect(model.entries[1].ownerLine == "Slice B is controlled by Shack PC. Take control to change it.")
        let list = SliceListModel(store: store, slices: model, commands: commands)
        #expect(await settle { list.rows.count == 3 && list.rows[1].holder == "Shack PC" })
        #expect(!list.rows.contains { $0.holderLine.contains("the Core") })
    }

    // MARK: Ruling 10 and the X/RIT step

    @Test("ruling 10: the side column and dimmed controls give the Core's words; the step cycles on the X/RIT panel")
    func sideColumnAndStep() async {
        let (model, store, outbox, _) = await cast()
        #expect(await ready(model))
        let controls = FlagControls(slices: model, modes: nil, tuning: nil, store: store)
        let words = try? #require(model.entries[1].ownerLine)
        // The dimmed modifier's tap is showReason with the line.
        model.showReason(words ?? "")
        #expect(model.refusal?.text == words)
        controls.cycleStep(model.entries[0])
        #expect(await settle { outbox.writes("slice:0", "stepHz").count == 1 })
        #expect(outbox.writes("slice:0", "stepHz").first?.properties.first?.value == .i64(500))
    }

    // MARK: The flag's size against the scale strip, and the edge markers

    /// The spectrum's foot for a band of `size` at the default share.
    static func spectrumFoot(_ size: CGSize, sideways: Bool) -> CGFloat {
        var settings = BandDisplaySettings()
        settings.sideways = sideways
        return BandLayout(size: size, scale: 1, settings: settings).frequencyScale.minY
    }

    @Test("every flag, the jump bar's clearance included, ends above the scale strip upright on a Pro Max, at every text size")
    func flagsClearTheScaleUpright() {
        // The band between the toolbar and the tab bar on an iPhone 18 Pro Max, upright, as the screen shots measure it.
        let band = CGSize(width: 440, height: 738)
        let foot = Self.spectrumFoot(band, sideways: false)
        for large in [false, true] {
            let metrics = FlagMetrics(large: large)
            for rade in [false, true] {
                for listening in [false, true] {
                    let bottom = JumpBar.clearance + metrics.height(rade: rade, listening: listening)
                    #expect(bottom <= foot, "rade \(rade) listening \(listening) large \(large): \(bottom) past \(foot)")
                }
            }
        }
    }

    /// The upright bands: the iPhone 17 and 17 Pro at 402 x 640, the same
    /// band as the screen shots' window draws it on an iPhone 17 (402 x 656),
    /// and the 17 Pro Max and 18 Pro Max at 440 x 738.
    nonisolated static let uprightBands = [CGSize(width: 402, height: 640), CGSize(width: 402, height: 656),
                                           CGSize(width: 440, height: 738)]

    /// Where the saved split puts the scale on each band, and how far the
    /// jumped view moves it for the listened flag and the listened RADE flag.
    static let jumpedMoves: [CGFloat: (savedFoot: CGFloat, plain: CGFloat, rade: CGFloat)] = [
        640: (249, 5, 23), 656: (255, 0, 17), 738: (288, 0, 0),
    ]

    @Test("jumped upright, the split moves down just enough for the listened flag and the listened RADE flag on the 402-wide bands, and not at all on a 440 x 738 band",
          arguments: SliceListTests.uprightBands)
    func jumpedSplitPerBand(_ size: CGSize) throws {
        let saved = BandDisplaySettings()
        let savedFoot = Self.spectrumFoot(size, sideways: false)
        for large in [false, true] {
            let metrics = FlagMetrics(large: large)
            for rade in [false, true] {
                let foot = JumpBar.clearance + metrics.height(rade: rade, listening: true)
                let floor = JumpBar.spectrumFloor(flagFoot: foot, bandHeight: size.height,
                                                  savedShare: saved.spectrumShare)
                let band = BandModel(settings: saved)
                band.setJumpedShareFloor(floor)
                let shown = BandLayout(size: size, scale: 1, settings: band.shownSettings)
                let pixels = BandLayout(size: CGSize(width: size.width * 3, height: size.height * 3), scale: 3,
                                        settings: band.shownSettings)
                let label = "\(size) large \(large) rade \(rade)"
                let moves = try #require(Self.jumpedMoves[size.height], "\(label)")
                let move = rade ? moves.rade : moves.plain
                #expect(foot == (rade ? 272 : 254), "\(label)")
                #expect(savedFoot == moves.savedFoot, "\(label)")
                // Moved by just the points needed: the scale starts flush under the flag, or stays.
                #expect(shown.frequencyScale.minY - savedFoot == move, "\(label)")
                if move > 0 {
                    #expect(shown.frequencyScale.minY == foot, "\(label)")
                    // Drawn at the phone's 3 pixels a point, the same place.
                    #expect(pixels.frequencyScale.minY == foot * 3, "\(label)")
                } else {
                    #expect(floor == nil, "\(label)")
                }
                // The floor is never the pan's own split.
                #expect(band.settings == saved, "\(label)")
            }
        }
    }

    @Test("Back to your band puts back the saved split, and the jumped view's split writes nothing to the phone's settings")
    func backRestoresTheSplit() async throws {
        let (model, store, _, commands) = await cast()
        #expect(await ready(model))
        let suite = "jumpsplit-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let displaySettings = BandDisplaySettingsStore(defaults: defaults)
        let main = MainScreenModel(mirror: store, settings: SettingsProxyClient(send: { _ in }), commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }),
                                   displaySettings: displaySettings)
        main.slices.thisDeviceId = Self.me
        #expect(await ready(main.slices))
        let saved = main.band.settings
        main.slices.show(1)
        #expect(await settle { main.slices.showingAnotherBand })
        // What the band's layer sets for the listened RADE flag on a 402 x 640 band.
        let band = Self.uprightBands[0]
        let foot = JumpBar.clearance + FlagMetrics(large: false).height(rade: true, listening: true)
        let floor = JumpBar.spectrumFloor(flagFoot: foot, bandHeight: band.height,
                                          savedShare: saved.spectrumShare)
        main.band.setJumpedShareFloor(floor)
        #expect(main.band.jumpedShareFloor != nil)
        #expect(BandLayout(size: band, scale: 1, settings: main.band.shownSettings).frequencyScale.minY == foot)
        #expect(main.band.shownSpectrumShare > saved.spectrumShare)
        #expect(main.band.settings == saved)
        #expect(defaults.dictionaryRepresentation().keys.filter { $0.hasPrefix(BandDisplaySettingsStore.keyPrefix) }.isEmpty)
        // Back: the saved split is shown again, and still nothing was kept.
        main.slices.back()
        #expect(!main.slices.showingAnotherBand)
        #expect(main.band.jumpedShareFloor == nil)
        #expect(main.band.shownSpectrumShare == saved.spectrumShare)
        #expect(BandLayout(size: band, scale: 1, settings: main.band.shownSettings).frequencyScale.minY
            == Self.spectrumFoot(band, sideways: false))
        #expect(main.band.settings == saved)
        #expect(defaults.dictionaryRepresentation().keys.filter { $0.hasPrefix(BandDisplaySettingsStore.keyPrefix) }.isEmpty)
        _ = model
    }

    @Test("the first layout after the jump already holds the split at the floor: no frame shows the saved split under the flag")
    func jumpedSplitFirstFrame() async throws {
        let (model, _, _, _) = await cast()
        #expect(await ready(model))
        // A 402 x 640 band (an iPhone 17), centred on slice B's frequency.
        let size = Self.uprightBands[0]
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context(centreHz: Self.bHz))))
        let suite = "jumpsplit-first-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let layer = BandGestureLayer(band: band, slices: model, settings: PhoneSettings(defaults: defaults),
                                     foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
            .frame(width: size.width, height: size.height)
        let window = try BandFlagShotTests.window(size: size)
        let host = UIHostingController(rootView: layer)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        #expect(band.pointGeometry(size: size) != nil)
        #expect(!model.jumped && band.jumpedShareFloor == nil)
        let saved = band.settings
        // The jump, then one layout pass of the band's layer, with no turn
        // of the main queue between them: what the next frame draws.
        model.show(1)
        #expect(model.jumped)
        host.view.setNeedsLayout()
        host.view.layoutIfNeeded()
        // The listened flag under Back to your band ends at 254: the split
        // is already there, five points below the saved 249.
        let foot = JumpBar.clearance + FlagMetrics(large: false).height(rade: false, listening: true)
        #expect(band.jumpedShareFloor != nil)
        #expect(BandLayout(size: size, scale: 1, settings: band.shownSettings).frequencyScale.minY == foot)
        #expect(band.settings == saved)
    }

    @Test("the edge markers stack from the spectrum's foot and drop below any flag they would cover")
    func edgeMarkersKeepClear() {
        let entryB = BandSlicesModel.Entry(slice: BandSlice(id: 1, frequencyHz: Self.bHz, filterLowHz: 100,
                                                            filterHighHz: 2_900, colour: "#FF00FF",
                                                            lowerSideband: false, txSlice: false),
                                           rxAntenna: "ANT1", txAntenna: "ANT1", modeLabel: "USB", panKey: nil,
                                           signalDbm: nil, signalPeakDbm: nil, signalAverageDbm: nil, stepHz: nil,
                                           sampleRateHz: nil, locked: false, muted: false, band: nil, mode: nil,
                                           rade: nil)
        var entryC = entryB
        entryC.slice = BandSlice(id: 2, frequencyHz: 14_200_000, filterLowHz: 100, filterHighHz: 2_900,
                                 colour: "#FFFF00", lowerSideband: false, txSlice: false)
        let edges = [BandSlicesModel.EdgeMark(entry: entryB, below: false),
                     BandSlicesModel.EdgeMark(entry: entryC, below: false)]
        for large in [false, true] {
            let flag = CGRect(x: 200, y: 0, width: 200, height: 158)
            let rects = EdgeMarkers.rects(edges, spectrumFoot: 282, bandWidth: 440, avoid: [flag], sideways: false,
                                          large: large)
            #expect(rects.count == 2)
            #expect(!rects[0].intersects(rects[1]))
            for rect in rects {
                #expect(!rect.intersects(flag))
                #expect(rect.height >= 44 && rect.width >= 40)
                #expect(rect.maxX <= 440 - EdgeMarkers.inset)
            }
            #expect(rects[0].maxY == 282 - EdgeMarkers.inset)
        }
    }

    // MARK: Fix wave: a pan on a listened slice's band

    @Test("a pan past the receiver's window on a listened slice's band asks the Core nothing and stops at the window")
    func panOnAListenedBandStaysInsideTheWindow() async throws {
        let (model, _, outbox, _) = await cast()
        #expect(await ready(model))
        model.show(1)
        #expect(model.jumped && model.activeSliceId == 1)
        #expect(model.canMoveReceiverWindow)
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context(centreHz: Self.bHz))))
        let size = CGSize(width: 402, height: 640)
        let geometry = try #require(band.pointGeometry(size: size)).geometry
        let placements = FlagLayout.layout(slices: model.entries.map(\.slice), activeSliceId: model.activeSliceId,
                                           geometry: geometry)
        let window = try #require(band.receiverWindow())
        // Two widths left: 96 kHz higher, 24 kHz past the 192 kHz window.
        let dragger = BandDrag()
        dragger.changed(from: CGPoint(x: 6, y: 300), translation: -size.width * 2, geometry: geometry,
                        entries: model.entries, placements: placements, band: band, slices: model, dragToTune: true)
        dragger.ended(slices: model)
        for _ in 0..<500 {
            await Task.yield()
        }
        // The phone only listens: the Core would refuse both verbs.
        #expect(outbox.invokes(BandSlicesModel.pinVerb).isEmpty)
        #expect(outbox.invokes(BandSlicesModel.centreVerb).isEmpty)
        #expect(model.refusal == nil)
        // The view stops at the window's upper edge.
        let view = try #require(band.requestedView)
        #expect(abs(view.centerHz + view.spanHz / 2 - window.upperBound) < 0.001)
    }

    // MARK: Fix wave: markers that do not fit

    /// Slices B, C, D and E on other pans, all above the band's view.
    static func markers(count: Int) -> [BandSlicesModel.EdgeMark] {
        (0..<count).map { index in
            let entry = BandSlicesModel.Entry(slice: BandSlice(id: index + 1, frequencyHz: 14_074_000 + Double(index) * 1_000,
                                                               filterLowHz: 100, filterHighHz: 2_900,
                                                               colour: "#FF00FF", lowerSideband: false, txSlice: false),
                                              rxAntenna: "ANT1", txAntenna: "ANT1", modeLabel: "USB", panKey: nil,
                                              signalDbm: nil, signalPeakDbm: nil, signalAverageDbm: nil, stepHz: nil,
                                              sampleRateHz: nil, locked: false, muted: false, band: nil, mode: nil,
                                              rade: nil)
            return BandSlicesModel.EdgeMark(entry: entry, below: false)
        }
    }

    @Test("markers that do not fit full on a short spectrum fold to one line, the nearest the foot staying full, and none leaves the band's top",
          arguments: [false, true])
    func edgeMarkersFoldWhenTheyDoNotFit(_ large: Bool) {
        // Sideways on a Pro Max: a spectrum about 120 points high, room
        // for one full marker and two folded ones.
        let foot: CGFloat = large ? 128 : 120
        let rects = EdgeMarkers.rects(Self.markers(count: 3), spectrumFoot: foot, bandWidth: 956, avoid: [],
                                      sideways: true, large: large)
        #expect(rects.count == 3)
        for (index, rect) in rects.enumerated() {
            #expect(rect.minY >= EdgeMarkers.inset, "marker \(index) at \(rect)")
            #expect(rect.maxY <= foot - EdgeMarkers.inset, "marker \(index) at \(rect)")
            for other in rects[(index + 1)...] {
                #expect(!rect.intersects(other))
            }
        }
        // The first stays full; the ones that would leave the top fold (D9).
        #expect(rects[0].height == (large ? EdgeMarkers.largeHeight : EdgeMarkers.height))
        #expect(rects[1].height == FlagLayout.foldedHeight && rects[2].height == FlagLayout.foldedHeight)
        // Room for every one full: none folds.
        let roomy = EdgeMarkers.rects(Self.markers(count: 3), spectrumFoot: 282, bandWidth: 440, avoid: [],
                                      sideways: false, large: large)
        #expect(roomy.allSatisfy { $0.height == (large ? EdgeMarkers.largeHeight : EdgeMarkers.height) })
    }

    @Test("markers that do not fit even folded flow on down past the spectrum's foot, never off the band's top")
    func edgeMarkersFlowBelowWhenEvenFoldedTheyDoNotFit() {
        let rects = EdgeMarkers.rects(Self.markers(count: 4), spectrumFoot: 70, bandWidth: 956, avoid: [],
                                      sideways: true, large: false)
        #expect(rects.count == 4)
        for (index, rect) in rects.enumerated() {
            #expect(rect.minY >= EdgeMarkers.inset, "marker \(index) at \(rect)")
            for other in rects[(index + 1)...] {
                #expect(!rect.intersects(other))
            }
        }
        #expect(rects.contains { $0.minY > 70 })
    }

    @Test("a stack on the dBm scale's side stays under its arrows, folding or flowing on down to do so")
    func edgeMarkersStayUnderTheDbmArrows() {
        // Sideways on a Pro Max: the spectrum 111 points high, the arrows 52 at its right edge.
        let arrows = CGRect(x: 850, y: 0, width: 40, height: 52)
        let rects = EdgeMarkers.rects(Self.markers(count: 3), spectrumFoot: 111, bandWidth: 956, avoid: [],
                                      sideways: true, large: false, dbmArrows: arrows)
        #expect(rects.count == 3)
        for (index, rect) in rects.enumerated() {
            #expect(!rect.intersects(arrows), "marker \(index) at \(rect)")
            #expect(rect.minY >= EdgeMarkers.inset)
            for other in rects[(index + 1)...] {
                #expect(!rect.intersects(other))
            }
        }
    }

    @Test("edge markers past the spectrum's foot move in from the edge, clear of zoom and PTT")
    func edgeMarkersKeepClearOfTheBandsControls() {
        let arrows = CGRect(x: 850, y: 0, width: 40, height: 52)
        let zoom = CGRect(x: 790, y: 140, width: 101, height: 42)
        let ptt = CGRect(x: 76, y: 120, width: 76, height: 76)
        var edges = Self.markers(count: 3)
        edges += Self.markers(count: 3).map { BandSlicesModel.EdgeMark(entry: $0.entry, below: true) }
        let rects = EdgeMarkers.rects(edges, spectrumFoot: 111, bandWidth: 956, avoid: [], sideways: true,
                                      large: false, dbmArrows: arrows, controls: [zoom, ptt])
        #expect(rects.count == 6)
        for (index, rect) in rects.enumerated() {
            #expect(!rect.intersects(zoom), "marker \(index) at \(rect)")
            #expect(!rect.intersects(ptt), "marker \(index) at \(rect)")
            #expect(!rect.intersects(arrows), "marker \(index) at \(rect)")
        }
        Self.expectClear(rects, of: [zoom, ptt, arrows], bandWidth: 956)
        // Without the controls the same stack lands on them: the test covers the move.
        let unmoved = EdgeMarkers.rects(edges, spectrumFoot: 111, bandWidth: 956, avoid: [], sideways: true,
                                        large: false, dbmArrows: arrows)
        #expect(unmoved.contains { $0.intersects(zoom) || $0.intersects(ptt) })
    }

    /// Every marker clear of every control, flag and other marker, and
    /// inside the band's width.
    static func expectClear(_ rects: [CGRect], of obstacles: [CGRect], bandWidth: CGFloat,
                            sourceLocation: SourceLocation = #_sourceLocation) {
        for (index, rect) in rects.enumerated() {
            #expect(rect.minX >= 0 && rect.maxX <= bandWidth, "marker \(index) at \(rect) leaves the band",
                    sourceLocation: sourceLocation)
            for obstacle in obstacles {
                #expect(!rect.intersects(obstacle), "marker \(index) at \(rect) on \(obstacle)",
                        sourceLocation: sourceLocation)
            }
            for other in rects[(index + 1)...] {
                #expect(!rect.intersects(other), "marker \(index) at \(rect) on \(other)",
                        sourceLocation: sourceLocation)
            }
        }
    }

    /// Sideways on a Pro Max: the spectrum's foot at 111, a short
    /// waterfall under it, the dBm arrows at the right, five slices off the
    /// right edge (all but one flowing below the foot).
    static let sidewaysWaterfall = CGRect(x: 0, y: 131, width: 956, height: 89)
    static let sidewaysArrows = CGRect(x: 850, y: 0, width: 40, height: 52)

    /// The controls as BandGestureLayer lists them: zoom, PTT, then the dial.
    static func sidewaysControls(_ kind: DialKind) -> [CGRect] {
        let waterfall = sidewaysWaterfall
        let zoom = CGRect(origin: DialLayer.zoomOrigin(kind: kind, waterfall: waterfall, sideways: true,
                                                       trailingInset: 62),
                          size: ZoomButtons.size)
        let ptt = PttButton.frame(bandSize: CGSize(width: 956, height: waterfall.maxY), leadingInset: 62)
        switch kind {
        case .waterfallKnob:
            return [zoom, ptt, DialLayer.knobFrame(waterfall: waterfall, sideways: true, trailingInset: 62)]
        case .thumbwheel:
            return [zoom, ptt, DialLayer.wheelFrame(waterfall: waterfall, sideways: true, trailingInset: 62)]
        case .off, .sheetKnob:
            return [zoom, ptt]
        }
    }

    @Test("sideways with the waterfall knob, a marker moved off the knob or zoom stays off the flags, the controls and the other markers, inside the band")
    func edgeMarkersKeepClearOfTheKnobAndZoom() {
        let controls = Self.sidewaysControls(.waterfallKnob)
        // A flag just left of the markers' column, where a marker moved in from the edge would land.
        let flag = CGRect(x: 520, y: 100, width: 180, height: 120)
        let rects = EdgeMarkers.rects(Self.markers(count: 5), spectrumFoot: 111, bandWidth: 956, avoid: [flag],
                                      sideways: true, large: false, dbmArrows: Self.sidewaysArrows,
                                      controls: controls)
        #expect(rects.count == 5)
        Self.expectClear(rects, of: controls + [Self.sidewaysArrows, flag], bandWidth: 956)
    }

    @Test("sideways with the thumbwheel, a marker that cannot pass beside it goes below it, never off the band's side")
    func edgeMarkersGoBelowAControlTooWideToPass() {
        let controls = Self.sidewaysControls(.thumbwheel)
        var edges = Self.markers(count: 5)
        edges += Self.markers(count: 5).map { BandSlicesModel.EdgeMark(entry: $0.entry, below: true) }
        let rects = EdgeMarkers.rects(edges, spectrumFoot: 111, bandWidth: 956, avoid: [], sideways: true,
                                      large: false, dbmArrows: Self.sidewaysArrows, controls: controls)
        #expect(rects.count == 10)
        Self.expectClear(rects, of: controls + [Self.sidewaysArrows], bandWidth: 956)
    }

    @Test("on a band narrower than the markers' margins nothing traps and every marker stays inside the band",
          arguments: [false, true])
    func edgeMarkersOnAVeryNarrowBand(_ sideways: Bool) {
        var edges = Self.markers(count: 3)
        edges += Self.markers(count: 3).map { BandSlicesModel.EdgeMark(entry: $0.entry, below: true) }
        for width: CGFloat in [0, 6, 100, 130] {
            let placed = EdgeMarkers.placements(edges, spectrumFoot: 111, bandWidth: width, avoid: [],
                                                sideways: sideways, large: false)
            #expect(placed.count == 6)
            for place in placed {
                #expect(place.rect.width >= 0 && place.rect.minX >= 0 && place.rect.maxX <= max(width, 0),
                        "\(place.rect) on a band \(width) wide")
            }
        }
    }

    // MARK: Fix wave: this phone's own volume goes with the slice

    @Test("a listened slice's own volume is forgotten when the slice goes; a new slice with its id starts at its AF gain")
    func listenLevelGoesWithTheSlice() async {
        let (model, store, _, _) = await cast()
        #expect(await ready(model))
        model.setListenLevel(1, level: 0.75, muted: true)
        #expect(model.listenLevel(1) == .init(level: 0.75, muted: true))
        store.apply(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:1", className: "SliceModel")))
        #expect(await settle { model.entries.map(\.id) == [0] })
        #expect(model.listenLevels[1] == nil)
        // The Core opens a new slice under the same id: its own AF gain, 40.
        store.apply(Self.slice(1, hz: Self.bHz, stream: 1, active: false, tx: false))
        #expect(await settle { model.entries.map(\.id) == [0, 1] })
        #expect(model.listenLevel(1) == .init(level: 0.4, muted: false))
    }
}
