// NereusSDR for iOS: a pan's band stack against a fake Core: its verbs and arguments, the Core's answers, and following a recall
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// The phone reads the Core's stack and sends its verbs. The fake holds each
/// verb to `surface.json` and refuses what does not fit it, and holds no
/// stack rules, which stay on the Core.
@Suite("Band stack model", .serialized, .timeLimit(.minutes(3)))
@MainActor
struct BandStackModelTests {
    @MainActor
    private struct Rig {
        let app: AppModel
        let station: FakeStation
        let suite: String
        var stack: BandStackModel { app.main.controls(for: .first).bandStack }
        var band: BandModel { app.main.band }
        var slices: BandSlicesModel { app.main.slices }

        func close() async {
            await app.disconnect()
            UserDefaults.standard.removePersistentDomain(forName: suite)
        }
    }

    private func connected(additions: FakeStation.Additions = .bandStack,
                           before: (FakeStation) -> Void = { _ in }) async throws -> Rig {
        let suite = "BandStackModelTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let app = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: additions)
        before(station)
        // Made before the snapshot, as a screen's model is, so it reads the
        // first document as it arrives.
        _ = app.main.controls(for: .first)
        await app.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                          transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await Eventually.holds { app.main.slices.entries.count == 2 && app.main.slices.activeSliceId == 0 })
        return Rig(app: app, station: station, suite: suite)
    }

    /// A stack on one band whose `lastRecall` is the given one.
    private static func document(seq: Int, sliceId: Int = 0, applyView: Bool = true, centerHz: Int64 = 7_240_000,
                                 spanHz: Int64 = 48_000, note: String = "") -> String {
        """
        {"schema":1,"region":"United States\(note)","options":{"dupeProtection":true,"expiryMinutes":15,"dwellSeconds":30},\
        "bands":{"40m":{"order":["40m-net"],"returnMode":"current","specificId":""}},\
        "entries":[{"id":"40m-net","band":"40m","frequencyHz":7185000,"mode":"LSB","filterLowHz":-2800,"filterHighHz":-100,\
        "centerHz":7188000,"spanHz":192000,"locked":false,"seeded":false,"name":"","lastUsedMs":0}],\
        "places":{},"lastExpiry":{"batchId":"","count":0},\
        "lastRecall":{"seq":\(seq),"sliceId":\(sliceId),"applyView":\(applyView),"centerHz":\(centerHz),"spanHz":\(spanHz)}}
        """
    }

    private func setCtun(_ on: Bool, _ band: BandModel) {
        var settings = band.settings
        settings.ctun = on
        band.settings = settings
    }

    private func args(_ invoke: LinkMessage.CommandInvoke?) -> [String: LinkMessage.PropertyValue] {
        var named: [String: LinkMessage.PropertyValue] = [:]
        for argument in invoke?.args ?? [] {
            named[argument.name] = argument.value
        }
        return named
    }

    // MARK: Availability

    @Test("it reads the Core's document, ready, with the shared sample's order")
    func readsTheDocument() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .ready })
        #expect(stack.unavailableReason == nil)
        #expect(stack.stack?.entries(band: "40m").map(\.id) == ["40m-net", "40m-ssb"])
        await rig.close()
    }

    @Test("a Core without band stacks greys it with the words for that, and nothing is sent")
    func olderCore() async throws {
        let rig = try await connected(additions: [])
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .needsNewerCore })
        #expect(stack.stack == nil)
        #expect(stack.unavailableReason == "This Core does not have band stacks. Updating the Core may help.")
        stack.press(bandKey: "40m", .next)
        #expect(stack.note == "This Core does not have band stacks. Updating the Core may help.")
        stack.reseed()
        #expect(!rig.station.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb.hasPrefix("bandStack.") }
            return false
        })
        await rig.close()
    }

    @Test("with no connection it greys with the words for that, and a verb shows them")
    func notConnected() {
        let store = MirrorStore(send: { _ in })
        let stack = BandStackModel(store: store, slices: BandSlicesModel(store: store, commands: nil), commands: nil)
        #expect(stack.availability == .notConnected)
        #expect(stack.stack == nil)
        #expect(stack.unavailableReason == "Connect to a radio to use band stacks.")
        stack.add()
        #expect(stack.note == "Connect to a radio to use band stacks.")
    }

    @Test("a dropped link goes back to not connected")
    func linkLost() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .ready })
        await rig.close()
        #expect(await Eventually.holds { stack.availability == .notConnected })
    }

    // MARK: The verbs

    @Test("each verb sends the arguments the Core takes, the slice's with the pan's view")
    func verbsAndArguments() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .ready })
        rig.band.requestView(TuneGestures.View(centerHz: 7_190_000.4, spanHz: 96_000))
        let view = rig.band.view
        #expect(view.centerHz > 0 && view.spanHz > 0)
        let viewArgs: [String: LinkMessage.PropertyValue] = [
            "viewCenterHz": .i64(Int64(view.centerHz.rounded())), "viewSpanHz": .i64(Int64(view.spanHz.rounded())),
        ]
        func slice(_ other: [String: LinkMessage.PropertyValue]) -> [String: LinkMessage.PropertyValue] {
            other.merging(viewArgs) { first, _ in first }.merging(["sliceId": .i64(0)]) { first, _ in first }
        }
        let calls: [(verb: String, send: () -> Void, expected: [String: LinkMessage.PropertyValue])] = [
            ("bandStack.press", { stack.press(bandKey: "40m", .next) },
             slice(["band": .utf8("40m"), "direction": .utf8("next")])),
            ("bandStack.press", { stack.press(bandKey: "20m", .previous) },
             slice(["band": .utf8("20m"), "direction": .utf8("previous")])),
            ("bandStack.step", { stack.step(.previous) }, slice(["direction": .utf8("previous")])),
            ("bandStack.goTo", { stack.goTo("40m-net") }, slice(["entryId": .utf8("40m-net")])),
            ("bandStack.add", { stack.add() }, slice([:])),
            ("bandStack.saveHere", { stack.saveHere("40m-ssb") }, slice(["entryId": .utf8("40m-ssb")])),
            ("bandStack.edit", { stack.edit("40m-net", name: "Evening net", locked: true) },
             ["entryId": .utf8("40m-net"), "name": .utf8("Evening net"), "locked": .bool(true)]),
            ("bandStack.edit", { stack.edit("40m-net", name: "", locked: nil) },
             ["entryId": .utf8("40m-net"), "name": .utf8("")]),
            ("bandStack.edit", { stack.edit("40m-net", name: nil, locked: false) },
             ["entryId": .utf8("40m-net"), "locked": .bool(false)]),
            ("bandStack.delete", { stack.delete("40m-ssb") }, ["entryId": .utf8("40m-ssb")]),
            ("bandStack.reorder", { stack.reorder(bandKey: "40m", ids: ["40m-ssb", "40m-net"]) },
             ["band": .utf8("40m"), "ids": .utf8("[\"40m-ssb\",\"40m-net\"]")]),
            ("bandStack.setReturn", { stack.setReturn(bandKey: "40m", mode: .specific, entryId: "40m-net") },
             ["band": .utf8("40m"), "mode": .utf8("specific"), "entryId": .utf8("40m-net")]),
            ("bandStack.setReturn", { stack.setReturn(bandKey: "20m", mode: .lastUsed, entryId: nil) },
             ["band": .utf8("20m"), "mode": .utf8("lastUsed")]),
            ("bandStack.setReturn", { stack.setReturn(bandKey: "80m", mode: .current, entryId: "") },
             ["band": .utf8("80m"), "mode": .utf8("current")]),
            ("bandStack.clear", { stack.clear(bandKey: "40m") }, ["band": .utf8("40m")]),
            ("bandStack.clear", { stack.clear(bandKey: nil) }, [:]),
            ("bandStack.reseed", { stack.reseed() }, [:]),
            ("bandStack.options", { stack.options(dupeProtection: false, expiryMinutes: 30, dwellSeconds: 60) },
             ["dupeProtection": .bool(false), "expiryMinutes": .i64(30), "dwellSeconds": .i64(60)]),
            ("bandStack.options", { stack.options(dupeProtection: nil, expiryMinutes: 15, dwellSeconds: nil) },
             ["expiryMinutes": .i64(15)]),
            ("bandStack.restoreExpired", { stack.restoreExpired(batchId: "batch-1") }, ["batchId": .utf8("batch-1")]),
        ]
        for (index, call) in calls.enumerated() {
            // The view is read at the touch: the band may be asked to fit it between touches.
            rig.band.requestView(view)
            call.send()
            #expect(await Eventually.holds { rig.station.bandStackCommands.count > index },
                    "\(call.verb) was not taken: \(stack.note ?? "")")
            let sent = rig.station.bandStackCommands.dropFirst(index).first
            #expect(sent?.verb == call.verb)
            #expect(args(sent) == call.expected, "\(call.verb)")
        }
        // No option given: nothing to send.
        stack.options(dupeProtection: nil, expiryMinutes: nil, dwellSeconds: nil)
        stack.reseed()
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == calls.count + 1 })
        #expect(rig.station.bandStackCommands.last?.verb == "bandStack.reseed")
        #expect(stack.note == nil)
        await rig.close()
    }

    @Test("with no view yet the slice's verb leaves the view out, so the entry keeps its own")
    func noViewNoArguments() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .ready })
        #expect(rig.band.view.spanHz == 0)
        stack.add()
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 1 })
        #expect(args(rig.station.bandStackCommands.first) == ["sliceId": .i64(0)])
        await rig.close()
    }

    @Test("reorder's list is the text of a JSON array of strings, as the desktop's bandStackIdList reads it")
    func idListText() throws {
        #expect(BandStackModel.idList(["40m-ssb", "40m-net"]) == "[\"40m-ssb\",\"40m-net\"]")
        #expect(BandStackModel.idList([]) == "[]")
        let hard = ["a\"b", "c/d", "Fünf", "e\\f"]
        let text = try #require(BandStackModel.idList(hard))
        #expect(try JSONDecoder().decode([String].self, from: Data(text.utf8)) == hard)
    }

    // MARK: The Core's answers

    @Test("a refusal's words show unchanged, and the next verb the Core takes closes them")
    func refusalWords() async throws {
        let rig = try await connected()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.availability == .ready })
        let words = "Band 40m ignored: the slice is locked. Unlock it to change bands."
        rig.station.refuseNext("bandStack.press", reason: words)
        stack.press(bandKey: "40m", .next)
        #expect(await Eventually.holds { stack.note == words })
        stack.step(.next)
        #expect(await Eventually.holds { stack.note == nil })
        await rig.close()
    }

    @Test("a press the Core holds sets no note, the sheet comes from its question, and Cancel changes nothing")
    func heldPress() async throws {
        let rig = try await connected()
        let stack = rig.stack
        let devices = rig.app.devices
        #expect(await Eventually.holds { stack.availability == .ready && !rig.app.mirror.capabilities.isEmpty })
        var capabilities = rig.app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        capabilities.append(LinkMessage.PropertyEntry(name: SeveralDevices.capability, value: .i64(1)))
        await rig.station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        #expect(await Eventually.holds { SeveralDevices.available(in: rig.app.mirror) })
        let before = stack.stack

        // A note still up is not this answer's: it goes, and none takes its place.
        stack.note = "An earlier refusal."
        rig.station.refuseNext("bandStack.press", reason: SeveralDevices.waitingReason)
        stack.press(bandKey: "20m", .next)
        #expect(await Eventually.holds { stack.note == nil })

        await rig.station.deliver(SeveralDevicesScreenTests.panMove(id: 44, effect: "moves"))
        #expect(await Eventually.holds { devices.question?.id == 44 })
        #expect(devices.question.map { ConfirmationLayer.shows($0.kind, takesTransmit: false) } == true)

        devices.cancel()
        let cancel = await rig.station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "confirm.cancel" }
            return false
        }
        #expect(cancel != nil)
        #expect(devices.question == nil)
        #expect(stack.note == nil)
        #expect(stack.stack == before)
        #expect(rig.station.bandStackCommands.isEmpty)
        await rig.close()
    }

    // MARK: Following a recall

    @Test("a recall of this phone's slice moves the pan's view once per seq; another's, or a stale one, does not")
    func followsARecall() async throws {
        let first = Self.document(seq: 5, centerHz: 14_000_000, spanHz: 10_000)
        let rig = try await connected(additions: .bandStack) { $0.setBandStack(first) }
        let stack = rig.stack
        let band = rig.band
        let slices = rig.slices
        #expect(await Eventually.holds { stack.stack?.lastRecall.seq == 5 && !rig.app.mirror.capabilities.isEmpty })
        // The first document says what has already happened.
        #expect(band.requestedView?.spanHz != 10_000)

        // Slice A is this phone's, and the MacBook controls slice B.
        var capabilities = rig.app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        capabilities.append(LinkMessage.PropertyEntry(name: SliceAccess.capability, value: .i64(3)))
        await rig.station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        #expect(await Eventually.holds { SliceAccess.available(in: rig.app.mirror) })
        let me = slices.thisDeviceId ?? "this-phone"
        slices.thisDeviceId = me
        await rig.station.deliver(SliceListTests.access(0, me, revision: 1))
        await rig.station.deliver(SliceListTests.access(1, "the-macbook", revision: 1))
        #expect(await Eventually.holds {
            slices.entries.first { $0.id == 1 }?.listening == true && slices.entries.first { $0.id == 0 }?.listening == false
        })

        func deliver(_ json: String) async -> Bool {
            let revision = (stack.stack?.revision ?? 0) + 1
            await rig.station.deliver(.delta(LinkMessage.Delta(key: "bandStack", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(json)),
                .init(ordinal: 1, name: "revision", value: .i64(revision)),
            ])))
            // The model follows a recall as it publishes the document.
            return await Eventually.holds { stack.stack?.revision == revision }
        }

        // Alone with click tune: the saved zoom and center come back.
        setCtun(true, band)
        #expect(await deliver(Self.document(seq: 6)))
        #expect(await Eventually.holds { band.requestedView == TuneGestures.View(centerHz: 7_240_000, spanHz: 48_000) })

        // The same seq again changes nothing, whatever else the document says.
        let moved = TuneGestures.View(centerHz: 7_236_000, spanHz: 24_000)
        band.requestView(moved)
        #expect(await deliver(Self.document(seq: 6, centerHz: 7_238_000, spanHz: 96_000, note: " again")))
        #expect(band.requestedView == moved)

        // The Core said to leave the view.
        #expect(await deliver(Self.document(seq: 7, applyView: false, spanHz: 96_000)))
        #expect(band.requestedView == moved)

        // Another device's slice, and a slice this phone is not in.
        #expect(await deliver(Self.document(seq: 8, sliceId: 1, spanHz: 96_000)))
        #expect(band.requestedView == moved)
        #expect(await deliver(Self.document(seq: 9, sliceId: 3, spanHz: 96_000)))
        #expect(band.requestedView == moved)

        // Without click tune the pan sits on its slice.
        setCtun(false, band)
        #expect(await deliver(Self.document(seq: 10, spanHz: 96_000)))
        #expect(await Eventually.holds { band.requestedView == TuneGestures.View(centerHz: 7_236_400, spanHz: 96_000) })
        await rig.close()
    }

    @Test("V14: the zoom comes back only on a pan the slice has to itself")
    func theViewRule() {
        func recall(center: Int64, span: Int64) -> BandStack.LastRecall? {
            BandStack(json: Self.document(seq: 1, centerHz: center, spanHz: span), revision: 1)?.lastRecall
        }
        let limits = 1_000.0...192_000.0
        let current = TuneGestures.View(centerHz: 14_100_000, spanHz: 24_000)
        func view(center: Int64 = 7_188_000, span: Int64 = 96_000, alone: Bool, clickTune: Bool,
                  current: TuneGestures.View? = nil) -> TuneGestures.View? {
            BandStackModel.recalledView(recall(center: center, span: span)!, frequencyHz: 7_185_000,
                                        current: current ?? TuneGestures.View(centerHz: 14_100_000, spanHz: 24_000),
                                        alone: alone, clickTune: clickTune, spanLimits: limits)
        }
        // Alone, click tune: its zoom and center.
        #expect(view(alone: true, clickTune: true) == TuneGestures.View(centerHz: 7_188_000, spanHz: 96_000))
        // Alone, no click tune: its zoom, on the slice.
        #expect(view(alone: true, clickTune: false) == TuneGestures.View(centerHz: 7_185_000, spanHz: 96_000))
        // Alone, click tune, the center would leave the frequency off screen.
        #expect(view(center: 7_300_000, alone: true, clickTune: true)
                == TuneGestures.View(centerHz: 7_185_000, spanHz: 96_000))
        // Shared: the span it has, on the slice, with or without click tune.
        #expect(view(alone: false, clickTune: true) == TuneGestures.View(centerHz: 7_185_000, spanHz: current.spanHz))
        #expect(view(alone: false, clickTune: false) == TuneGestures.View(centerHz: 7_185_000, spanHz: current.spanHz))
        // No saved zoom keeps the span.
        #expect(view(span: 0, alone: true, clickTune: false)
                == TuneGestures.View(centerHz: 7_185_000, spanHz: current.spanHz))
        // A saved zoom stays inside what this pan can show.
        #expect(view(span: 500_000, alone: true, clickTune: false)?.spanHz == 192_000)
        #expect(view(span: 200, alone: true, clickTune: false)?.spanHz == 1_000)
        // No span known and none saved: nothing to ask for.
        #expect(view(span: 0, alone: true, clickTune: false, current: TuneGestures.View(centerHz: 0, spanHz: 0)) == nil)
    }

    @Test("each open pan has its own band stack model")
    func onePerPan() async throws {
        let rig = try await connected()
        let main = rig.app.main
        main.panStack.allowsMorePans = true
        let firstModel = main.controls(for: .first).bandStack
        #expect(await Eventually.holds { main.panStack.addPanState().enabled })
        main.panStack.addPan()
        let second = PanIdentity(number: 2)
        #expect(await Eventually.holds { main.panStack.open.contains(second) })
        let secondModel = main.controls(for: second).bandStack
        #expect(firstModel !== secondModel)
        #expect(main.controls(for: second).bandStack === secondModel)
        #expect(await Eventually.holds { secondModel.availability == .ready })
        await rig.close()
    }

    // MARK: The block's readings and the band buttons

    /// A Core with bands and the given additions, its catalogue delivered, slice A on `band`.
    private func withBands(_ additions: FakeStation.Additions = [.all, .bandStack], band: Int = 3) async throws -> Rig {
        let rig = try await connected(additions: additions)
        let json = try #require(MainScreenTests.catalogueJSON())
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(rig.station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await Eventually.holds { rig.band.catalog != nil && rig.app.main.pan.canSelectBand })
        await setBand(rig, slice: 0, band)
        return rig
    }

    private func setBand(_ rig: Rig, slice: Int, _ band: Int) async {
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:\(slice)", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(Int64(band))),
        ])))
        #expect(await Eventually.holds { rig.slices.entries.first { $0.id == slice }?.band == band })
    }

    private func selectBands(_ station: FakeStation) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap {
            if case .commandInvoke(let invoke) = $0, invoke.verb == PanSheetModel.selectBandVerb {
                return invoke
            }
            return nil
        }
    }

    @Test("the grid's labels and the slice's band numbers are the Core's band keys")
    func bandKeys() {
        #expect(BandStackModel.bandKey(label: "40") == "40m")
        #expect(BandStackModel.bandKey(label: "2") == "2m")
        #expect(BandStackModel.bandKey(label: "WWV") == "WWV")
        let bands = [(id: 3, label: "40"), (id: 27, label: "2"), (id: 12, label: "WWV")]
        #expect(BandStackModel.bandKey(id: 3, bands: bands) == "40m")
        #expect(BandStackModel.bandKey(id: 27, bands: bands) == "2m")
        #expect(BandStackModel.bandKey(id: 11, bands: bands) == "GEN")
        #expect(BandStackModel.bandKey(id: 13, bands: bands) == "XVTR")
        #expect(BandStackModel.bandKey(id: 99, bands: bands) == nil)
        #expect(BandStackModel.bandKey(id: nil, bands: bands) == nil)
    }

    @Test("with the capability a band's press goes through the stack, lit or not, and a menu showing it stays open")
    func pressThroughTheStack() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        let pan = rig.app.main.pan
        #expect(await Eventually.holds { stack.availability == .ready && stack.currentBand == "40m" })
        #expect(pan.pressBand(3, stack: stack))
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 1 })
        var sent = rig.station.bandStackCommands[0]
        #expect(sent.verb == "bandStack.press")
        #expect(args(sent)["band"] == .utf8("40m"))
        #expect(args(sent)["direction"] == .utf8("next"))
        #expect(pan.pressBand(5, stack: stack))
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 2 })
        sent = rig.station.bandStackCommands[1]
        #expect(args(sent)["band"] == .utf8("20m"))
        #expect(args(sent)["direction"] == .utf8("next"))
        #expect(selectBands(rig.station).isEmpty)
        await rig.close()
    }

    @Test("without the capability a band's press opens it as today, and a menu closes on it")
    func pressOnAnOlderCore() async throws {
        let rig = try await withBands(.all)
        let stack = rig.stack
        let pan = rig.app.main.pan
        #expect(await Eventually.holds { stack.availability == .needsNewerCore })
        #expect(!pan.pressBand(5, stack: stack))
        #expect(await Eventually.holds { selectBands(rig.station).count == 1 })
        #expect(selectBands(rig.station)[0].args == [.init(name: "sliceId", value: .i64(0)),
                                                     .init(name: "band", value: .i64(5))])
        #expect(rig.station.bandStackCommands.isEmpty)
        #expect(stack.unavailableReason == BandStackModel.needsNewerCoreText)
        await rig.close()
    }

    @Test("the list shown is the slice's band until another is picked, and the slice getting there ends the view")
    func viewingAnotherBand() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.currentBand == "40m" && stack.availability == .ready })
        #expect(stack.shownBand == "40m")
        #expect(!stack.isViewingAnotherBand)
        // Picking its own band is no view at all, and a pick sends nothing.
        stack.showBand("40m")
        #expect(!stack.isViewingAnotherBand)
        stack.showBand("20m")
        #expect(stack.shownBand == "20m")
        #expect(stack.isViewingAnotherBand)
        #expect(rig.station.bandStackCommands.isEmpty)
        // A press goes back to the slice's list.
        stack.press(bandKey: "40m", .next)
        #expect(!stack.isViewingAnotherBand)
        // The slice reaching the list being read ends the view.
        stack.showBand("20m")
        await setBand(rig, slice: 0, 5)
        #expect(await Eventually.holds { stack.currentBand == "20m" && !stack.isViewingAnotherBand })
        #expect(stack.shownBand == "20m")
        await rig.close()
    }

    @Test("another active slice ends the view: the list follows the controls")
    func activeSliceEndsTheView() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.currentBand == "40m" })
        stack.showBand("20m")
        #expect(stack.isViewingAnotherBand)
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 11, name: "active", value: .bool(false)),
        ])))
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 11, name: "active", value: .bool(true)),
        ])))
        #expect(await Eventually.holds { rig.slices.activeSliceId == 1 && !stack.isViewingAnotherBand })
        await rig.close()
    }

    @Test("the count reads the slice's place in the list, a dash with none, and what is saved on another band")
    func countText() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.currentBand == "40m" && stack.stack != nil })
        #expect(stack.countText(band: "40m") == "1 of 2")
        #expect(stack.countText(band: "20m") == "1 saved")
        #expect(stack.countText(band: "6m") == "0 saved")
        #expect(stack.activeSlicePlace(band: "40m") == "40m-net")
        await setBand(rig, slice: 0, 1)
        // The sample has A on no 80 m entry.
        #expect(await Eventually.holds { stack.currentBand == "80m" })
        #expect(stack.countText(band: "80m") == "- of 1")
        await rig.close()
    }

    @Test("the picker lists the Core's grid in its order, then any other band the stack or the slice is on")
    func pickerBands() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.currentBand == "40m" && stack.stack != nil })
        let grid = FakeStation.bands.map { BandStackModel.bandKey(label: $0.label) }
        #expect(stack.pickerBands == grid)
        await setBand(rig, slice: 0, 11)
        #expect(await Eventually.holds { stack.currentBand == "GEN" })
        #expect(stack.pickerBands == grid + ["GEN"])
        await rig.close()
    }

    @Test("a row shows another slice standing on it, and not the active slice or one on another band")
    func otherSlicesOnARow() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.stack != nil && stack.currentBand == "40m" })
        let entry = try #require(stack.stack?.entries(band: "20m").first)
        #expect(stack.otherSlices(on: entry).isEmpty)
        await setBand(rig, slice: 1, 5)
        #expect(await Eventually.holds { stack.otherSlices(on: entry).map(\.letter) == ["B"] })
        let own = try #require(stack.stack?.entries(band: "40m").first)
        #expect(stack.otherSlices(on: own).isEmpty)
        await rig.close()
    }

    @Test("a row's tint is the segment of the plan the band's strip shows")
    func tintFromTheBandsPlan() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        let plan = try #require(rig.band.shownPlan)
        let segment = try #require(plan.segments.first { $0.highHz > $0.lowHz })
        let inside = Int64((segment.lowHz + segment.highHz) / 2)
        #expect(stack.tintColour(hz: inside) == plan.segments.first { $0.lowHz <= Double(inside) && Double(inside) <= $0.highHz }?.colour)
        #expect(stack.tintColour(hz: inside) != nil)
        #expect(stack.tintColour(hz: 1) == nil)
        await rig.close()
    }

    @Test("Done sends every id: the order made here, less what went, with what came at the foot")
    func reorderSendsEveryId() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.stack != nil })
        #expect(stack.reorderIds(working: ["40m-ssb", "40m-net"], band: "40m") == ["40m-ssb", "40m-net"])
        #expect(stack.reorderIds(working: ["gone", "40m-ssb"], band: "40m") == ["40m-ssb", "40m-net"])
        stack.reorder(bandKey: "40m", ids: stack.reorderIds(working: ["40m-ssb", "40m-net"], band: "40m"))
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 1 })
        let sent = rig.station.bandStackCommands[0]
        #expect(args(sent)["band"] == .utf8("40m"))
        #expect(args(sent)["ids"] == .utf8("[\"40m-ssb\",\"40m-net\"]"))
        await rig.close()
    }

    @Test("Reorder is a mode on the list on show: moves send nothing, Done sends every id, leaving the list ends it")
    func reorderMode() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.stack != nil && stack.currentBand == "40m" })
        stack.toggleReorder()
        #expect(stack.reorder == BandStackModel.Reorder(band: "40m", ids: ["40m-net", "40m-ssb"]))
        stack.moveInReorder("40m-net", by: 5)
        #expect(stack.reorder?.ids == ["40m-ssb", "40m-net"])
        stack.moveInReorder("40m-net", by: -9)
        #expect(stack.reorder?.ids == ["40m-net", "40m-ssb"])
        stack.moveInReorder("40m-net", by: 1)
        #expect(rig.station.bandStackCommands.isEmpty)
        // Looking at another band ends the mode, with nothing sent.
        stack.showBand("20m")
        #expect(stack.reorder == nil)
        stack.showBand(nil)
        stack.toggleReorder()
        stack.moveInReorder("40m-net", by: 1)
        stack.toggleReorder()
        #expect(stack.reorder == nil)
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 1 })
        #expect(args(rig.station.bandStackCommands[0])["ids"] == .utf8("[\"40m-ssb\",\"40m-net\"]"))
        // Closing the sheet ends the view and the mode.
        stack.showBand("20m")
        stack.toggleReorder()
        #expect(stack.reorder?.band == "20m")
        stack.endViewing()
        #expect(stack.reorder == nil && !stack.isViewingAnotherBand)
        await rig.close()
    }

    @Test("Rename sends the typed name once, trimmed, and nothing when it has not changed")
    func rename() async throws {
        let rig = try await withBands()
        let stack = rig.stack
        #expect(await Eventually.holds { stack.stack != nil })
        let net = try #require(stack.stack?.entries(band: "40m").first { $0.id == "40m-net" })
        stack.startRename(net)
        #expect(stack.renaming == "40m-net" && stack.renameDraft == "Evening net")
        stack.finishRename()
        #expect(stack.renaming == nil)
        stack.startRename(net)
        stack.renameDraft = "  Morning net "
        stack.finishRename()
        stack.finishRename()
        #expect(await Eventually.holds { rig.station.bandStackCommands.count == 1 })
        #expect(args(rig.station.bandStackCommands[0]) == ["entryId": .utf8("40m-net"), "name": .utf8("Morning net")])
        // Leaving the list drops what was typed.
        stack.startRename(net)
        stack.showBand("20m")
        #expect(stack.renaming == nil)
        await rig.close()
    }

    @Test("a row's frequency drops hertz of zero and keeps any others")
    func rowFrequency() {
        #expect(BandStackBlock.frequencyText(7_185_000) == "7.185")
        #expect(BandStackBlock.frequencyText(7_185_250) == "7.185.250")
        #expect(BandStackBlock.frequencyText(14_074_000) == "14.074")
    }
}
