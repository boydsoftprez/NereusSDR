// NereusSDR for iOS: the main screen's toolbar, tab bar and RX panel against a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// R-IOS-11: the toolbar's order, the tab bar's, and each RX panel
/// control's write reaching the Core, the control then showing the value
/// the Core answers with. The Core is `FakeStation`; its catalogue is the
/// conformance suite's ANAN-G2 one, read at run time and never bundled (D4).
@Suite("Main screen", .serialized)
@MainActor
struct MainScreenTests {
    /// The writes already answered, so each answer finds the next.
    final class Answered: @unchecked Sendable {
        private let lock = NSLock()
        private var ids: Set<UInt32> = []

        var all: Set<UInt32> { lock.withLock { ids } }

        func insert(_ id: UInt32) {
            _ = lock.withLock { ids.insert(id) }
        }
    }

    private let answered = Answered()

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    /// A model connected to a fake Core with the ANAN-G2 catalogue.
    private func connected(withoutCapabilities: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation(withoutCapabilities: withoutCapabilities)
        let defaults = try #require(UserDefaults(suiteName: "MainScreenTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        let json = try #require(Self.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { !model.main.rx.agc.isEmpty && !model.main.rx.presets.isEmpty })
        return (model, station)
    }

    /// Waits for the app's write of `property` to slice 0, then answers it
    /// as the Core does: `property.result` by the write's id and, when
    /// taken, a delta with the kept value.
    @discardableResult
    private func answer(_ station: FakeStation, _ property: String, keep value: LinkMessage.PropertyValue? = nil,
                        refuse reason: String? = nil) async -> LinkMessage.PropertyValue? {
        let done = answered.all
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.first?.name == property
                    && !done.contains(write.writeId ?? 0)
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId,
              let entry = write.properties.first else {
            Issue.record("no write of \(property) reached the Core")
            return nil
        }
        answered.insert(writeId)
        let kept = LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: property, value: value ?? entry.value)
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
            .init(property: property, accepted: reason == nil, reason: reason ?? "", value: kept),
        ])))
        if reason == nil {
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [kept])))
        }
        return entry.value
    }

    /// The conformance suite's ANAN-G2 catalogue, or nil outside the checkout.
    static func catalogueJSON() -> String? {
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json")
        guard let data = try? Data(contentsOf: file),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        for step in object["steps"] as? [[String: Any]] ?? [] {
            guard let message = step["message"] as? [String: Any], message["key"] as? String == "catalog",
                  let properties = message["properties"] as? [[String: Any]],
                  let json = properties.first(where: { $0["name"] as? String == "json" })?["value"] as? String else {
                continue
            }
            return json
        }
        return nil
    }

    // MARK: The toolbar and the tab bar

    @Test("the toolbar's items are the board's, in its order, the TX panel's button at the right end")
    func toolbarOrder() {
        #expect(Toolbar.Item.allCases == [.rxPanel, .speaker, .slice, .pan, .display, .link, .txPanel])
    }

    @Test("the link chip shows the round-trip time while up, Lost while retrying, nothing otherwise")
    func linkChip() {
        #expect(LinkState(connection: .connected, roundTripMs: 38).text == "38 ms")
        #expect(LinkState(connection: .connected, roundTripMs: nil).text == nil)
        #expect(LinkState(connection: .waitingToRetry(seconds: 2, reason: nil), roundTripMs: 38).text == "Lost")
        #expect(LinkState(connection: .notConnected, roundTripMs: nil).text == nil)
        #expect(LinkState(connection: .connected, roundTripMs: 38).spoken(core: "KG4VCF/shack")
            == "Core KG4VCF/shack, direct link, 38 milliseconds")
    }

    @Test("the tab bar draws a glyph of its own for every tab")
    func tabGlyphs() {
        for tab in AppTab.allCases {
            let path = TabGlyph.path(for: tab)
            #expect(!path.isEmpty, "\(tab) has no glyph")
            let bounds = path.boundingRect
            #expect(bounds.minX >= 0 && bounds.maxX <= TabGlyph.box.width, "\(tab) leaves the box")
            #expect(bounds.minY >= 0 && bounds.maxY <= TabGlyph.box.height, "\(tab) leaves the box")
        }
    }

    @Test("the speaker mutes and unmutes the band")
    func speakerMutes() {
        let model = AppModel()
        #expect(!model.audioMuted)
        model.setAudioMuted(true)
        #expect(model.audioMuted)
        model.setAudioMuted(false)
        #expect(!model.audioMuted)
    }

    // MARK: The RX panel

    @Test("the RX panel shows the Core's values with the catalogue's ranges and lists")
    func showsTheCoresValues() async throws {
        let (model, _) = try await connected()
        let rx = model.main.rx
        #expect(rx.sliceLetter == "A")
        #expect(rx.afGain == 50)
        #expect(rx.afRange == StationCatalog.Range(min: 0, max: 100, step: 1))
        #expect(rx.agc.map(\.label) == ["Off", "Long", "Slow", "Med", "Fast"])
        #expect(rx.agc.filter(\.lit).map(\.label) == ["Med"])
        // USB's presets from the Core, lit where the slice's 100 to 3000 Hz is one.
        let catalog = try #require(model.main.catalogFeed.catalog)
        let usb = try #require(catalog.filterPresets["USB"])
        #expect(rx.presets.map(\.name) == usb.map(\.label))
        #expect(rx.presets.filter(\.lit).map(\.lowHz) == usb.filter { $0.lowHz == 100 && $0.highHz == 3000 }.map { Int64($0.lowHz) })
        #expect(rx.noise.map(\.label) == ["NB", "NR1", "NR2", "NR3", "NR4", "DFNR", "MNR", "NNR", "ANF", "SNB"])
        #expect(rx.noise.allSatisfy { !$0.lit })
        #expect(rx.squelchRange == StationCatalog.Range(min: 0, max: 100, step: 1))
        #expect(rx.squelch == 16)
        #expect(!rx.squelchOn)
        await model.disconnect()
    }

    @Test("NR3 stays on the panel, greyed with the Core's reason, and sends nothing")
    func nr3IsGreyedWithTheReason() async throws {
        let (model, station) = try await connected()
        let nr3 = try #require(model.main.rx.noise.first { $0.label == "NR3" })
        #expect(nr3.reason == "No NR3 model file was found on this Core, so NR3 cannot run.")
        let before = station.messages.count
        model.main.rx.tap(nr3)
        for _ in 0..<1_000 {
            await Task.yield()
        }
        #expect(station.messages.count == before)
        // Once the Core can run it, it is live.
        await station.deliver(.delta(LinkMessage.Delta(key: "dspAssets", properties: [
            .init(ordinal: 7, name: "nr3Runnable", value: .bool(true)),
        ])))
        #expect(await settle { model.main.rx.noise.first { $0.label == "NR3" }?.reason == nil })
        await model.disconnect()
    }

    @Test("DFNR and MNR are greyed with the Core's words from its DspAssetService, and send nothing")
    func dfnrAndMnrFollowTheCore() async throws {
        let (model, station) = try await connected()
        let dfnr = try #require(model.main.rx.noise.first { $0.label == "DFNR" })
        let mnr = try #require(model.main.rx.noise.first { $0.label == "MNR" })
        // The suite's Core sends dfnrRunnable false with no status, and mnrStatus.
        #expect(dfnr.reason == "DFNR cannot run on this Core.")
        #expect(mnr.reason == "MNR runs only on a Mac, and this Core is not a Mac, so MNR cannot run.")
        let before = station.messages.count
        model.main.rx.tap(dfnr)
        model.main.rx.tap(mnr)
        for _ in 0..<1_000 {
            await Task.yield()
        }
        #expect(station.messages.count == before)
        // The Core's own status, as sent, then live once it can run them.
        await station.deliver(.delta(LinkMessage.Delta(key: "dspAssets", properties: [
            .init(ordinal: 8, name: "dfnrModelStatus", value: .utf8("The DeepFilterNet model did not load.")),
        ])))
        #expect(await settle {
            model.main.rx.noise.first { $0.label == "DFNR" }?.reason == "The DeepFilterNet model did not load."
        })
        await station.deliver(.delta(LinkMessage.Delta(key: "dspAssets", properties: [
            .init(ordinal: 9, name: "dfnrRunnable", value: .bool(true)),
            .init(ordinal: 11, name: "mnrRunnable", value: .bool(true)),
        ])))
        #expect(await settle {
            model.main.rx.noise.filter { ["DFNR", "MNR"].contains($0.label) }.allSatisfy { $0.reason == nil }
        })
        await model.disconnect()
    }

    @Test("a Core that does not say which noise reduction it runs greys DFNR and MNR with that reason")
    func anOlderCoreGreysDfnrAndMnr() async throws {
        let (model, station) = try await connected(withoutCapabilities: ["dspAssetVersion"])
        let notSaid = "This Core does not say which noise reduction it can run. Updating the Core may help."
        for label in ["DFNR", "MNR"] {
            #expect(model.main.rx.noise.first { $0.label == label }?.reason == notSaid, "\(label)")
        }
        // Even when its objects say otherwise, the version rules.
        await station.deliver(.delta(LinkMessage.Delta(key: "dspAssets", properties: [
            .init(ordinal: 9, name: "dfnrRunnable", value: .bool(true)),
            .init(ordinal: 11, name: "mnrRunnable", value: .bool(true)),
        ])))
        for _ in 0..<1_000 {
            await Task.yield()
        }
        let before = station.messages.count
        for label in ["DFNR", "MNR"] {
            let button = try #require(model.main.rx.noise.first { $0.label == label })
            #expect(button.reason == notSaid, "\(label)")
            model.main.rx.tap(button)
        }
        for _ in 0..<1_000 {
            await Task.yield()
        }
        #expect(station.messages.count == before)
        await model.disconnect()
    }

    @Test("NNR sits in the desktop flag's order without BNR, and follows what the slice says about it")
    func nnrFollowsTheSlice() async throws {
        let (model, station) = try await connected()
        let rx = model.main.rx
        // The desktop flag's noise reductions (Off, NR1 to NR4, DFNR, BNR,
        // MNR, NNR), BNR left out as on the desktop.
        #expect(RxPanelModel.reductions.map(\.label) == ["NR1", "NR2", "NR3", "NR4", "DFNR", "MNR", "NNR"])
        #expect(RxPanelModel.reductions.map(\.slot) == [1, 2, 3, 4, 5, 7, 8])
        #expect(!rx.noise.contains { $0.label == "BNR" })
        // The suite's Core says NNR can run: it is live, and writes activeNr 8.
        let nnr = try #require(rx.noise.first { $0.label == "NNR" })
        #expect(nnr.reason == nil)
        rx.tap(nnr)
        #expect(await answer(station, "activeNr") == .enumeration(8))
        #expect(await settle { rx.noise.first { $0.label == "NNR" }?.lit == true })

        // The Core says it cannot: greyed with the Core's words, sending nothing.
        let words = "NNR receiver is not available."
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 69, name: "nnrAvailable", value: .bool(false)),
            .init(ordinal: 84, name: "nnrStatus", value: .utf8(words)),
        ])))
        #expect(await settle { rx.noise.first { $0.label == "NNR" }?.reason == words })
        let before = station.messages.count
        rx.tap(try #require(rx.noise.first { $0.label == "NNR" }))
        for _ in 0..<1_000 {
            await Task.yield()
        }
        #expect(station.messages.count == before)
        // With no words of its own, plain ones.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 84, name: "nnrStatus", value: .utf8("")),
        ])))
        #expect(await settle { rx.noise.first { $0.label == "NNR" }?.reason == RxPanelModel.nnrFallback })
        // The Modes tab shows the same list, from the same model.
        #expect(model.main.modes.rx.noise.map(\.label) == rx.noise.map(\.label))

        // A slice that does not say whether NNR runs greys it with that reason.
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel",
                                                                    properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(14_225_000)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(1)),
            .init(ordinal: 3, name: "filterLow", value: .i64(100)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(3000)),
            .init(ordinal: 11, name: "active", value: .bool(true)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
            .init(ordinal: 59, name: "activeNr", value: .enumeration(0)),
        ])))
        #expect(await settle {
            rx.noise.first { $0.label == "NNR" }?.reason == RxPanelModel.noiseReductionNotSaidText
        })
        await model.disconnect()
    }

    @Test("DFNR needs dspAssetVersion 3 to be said, MNR 4")
    func theVersionEachPairNeeds() {
        let notSaid = RxPanelModel.noiseReductionNotSaidText
        func reasons(_ version: Int64) -> [String?] {
            [RxPanelModel.dfnrPair, RxPanelModel.mnrPair].map {
                RxPanelModel.cannotRun($0, assets: nil, dspAssetVersion: version)
            }
        }
        #expect(reasons(0) == [notSaid, notSaid])
        #expect(reasons(2) == [notSaid, notSaid])
        #expect(reasons(3) == [nil, notSaid])
        #expect(reasons(4) == [nil, nil])
        // NR3 carries no version rule.
        #expect(RxPanelModel.cannotRun(RxPanelModel.nr3Pair, assets: nil, dspAssetVersion: 0) == nil)
    }

    @Test("each RX panel control's write reaches the Core and the control shows the Core's answer")
    func writesReachTheCore() async throws {
        let (model, station) = try await connected()
        let rx = model.main.rx

        rx.setAfGain(62)
        #expect(await answer(station, "afGain") == .i64(62))
        #expect(await settle { rx.afGain == 62 })

        let fast = try #require(rx.agc.first { $0.label == "Fast" })
        rx.selectAgc(fast.id)
        #expect(await answer(station, "agcMode") == .enumeration(Int64(fast.id)))
        #expect(await settle { rx.agc.first(where: \.lit)?.label == "Fast" })

        let narrow = try #require(rx.presets.last)
        rx.selectPreset(narrow)
        #expect(await answer(station, "filterLow") == .i64(narrow.lowHz))
        #expect(await answer(station, "filterHigh") == .i64(narrow.highHz))
        #expect(await settle { rx.presets.first(where: \.lit)?.slot == narrow.slot })

        for (label, property, value) in [("NR2", "activeNr", LinkMessage.PropertyValue.enumeration(2)),
                                         ("NB", "nbMode", .enumeration(1)), ("ANF", "anfEnabled", .bool(true)),
                                         ("SNB", "snbEnabled", .bool(true))] {
            let button = try #require(rx.noise.first { $0.label == label })
            rx.tap(button)
            #expect(await answer(station, property) == value, "\(label)")
            #expect(await settle { rx.noise.first { $0.id == button.id }?.lit == true }, "\(label)")
        }
        // A lit noise reduction's second tap turns it off; NB steps to NB2.
        rx.tap(try #require(rx.noise.first { $0.label == "NR2" }))
        #expect(await answer(station, "activeNr") == .enumeration(0))
        rx.tap(try #require(rx.noise.first { $0.id == "NB" }))
        #expect(await answer(station, "nbMode") == .enumeration(2))
        #expect(await settle { rx.noise.first { $0.id == "NB" }?.label == "NB2" })

        rx.toggleSquelch()
        #expect(await answer(station, "ssqlEnabled") == .bool(true))
        #expect(await settle { rx.squelchOn })
        rx.setSquelch(20)
        #expect(await answer(station, "ssqlThresh") == .f64(20))
        #expect(await settle { rx.squelch == 20 })
        await model.disconnect()
    }

    @Test("the control shows the value the Core keeps, and a refusal's reason as sent")
    func coresAnswerWins() async throws {
        let (model, station) = try await connected()
        let rx = model.main.rx
        rx.setAfGain(90)
        await answer(station, "afGain", keep: .i64(80))
        #expect(await settle { rx.afGain == 80 })
        rx.setAfGain(95)
        // A refusal carries the value the Core kept.
        await answer(station, "afGain", keep: .i64(80), refuse: "The Core kept AF gain where it was.")
        #expect(await settle { rx.note == "The Core kept AF gain where it was." })
        #expect(rx.afGain == 80)
        await model.disconnect()
    }

    @Test("a slider's values reach the Core in order, the last one kept")
    func sliderWritesStayInOrder() async throws {
        let (model, station) = try await connected()
        let rx = model.main.rx
        rx.setAfGain(10)
        rx.setAfGain(20)
        rx.setAfGain(30)
        #expect(await answer(station, "afGain") == .i64(10))
        // 20 was replaced by 30 while 10 waited for its answer.
        #expect(await answer(station, "afGain") == .i64(30))
        #expect(await settle { rx.afGain == 30 })
        await model.disconnect()
    }
}
