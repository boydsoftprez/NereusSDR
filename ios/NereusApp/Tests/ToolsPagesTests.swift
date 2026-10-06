// NereusSDR for iOS: the Tools tab's list and the Core's tool pages against a fake Core: order, tags, reads, writes, reasons
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// R-IOS-18, D41, D42, spec section 5.2 items 3 to 5: the Tools tab lists
/// the Core's tools in the desktop's order, marked where each runs, and
/// follows a new catalogue while it is open; each station tool page reads
/// and writes its owner at the Core (the transmit settings, PureSignal's
/// verbs, slice A's diversity, the station TCI server, the support bundle);
/// an older Core, a Core off the air or no Core leaves each control shown
/// and greyed with its reason; the support bundle writes files for the
/// share sheet and sends nothing anywhere itself.
@Suite("Tools pages", .serialized)
@MainActor
struct ToolsPagesTests {
    private let platform = TestPlatform()

    /// Every tool the suite's ANAN-G2 catalogue lists on the phone, in the
    /// desktop's order: all it offers, and TCI Server, which a Core without
    /// its own TCI server does not offer and the phone shows greyed.
    static let offeredIds = ["spotHub", "freedvReporter", "txEqualizer", "pureSignal", "diversity", "tciServer",
                             "vaxAudio", "networkDiagnostics", "supportBundle"]

    // MARK: The list

    @Test("the list is the Core's tools in the desktop's order, each marked Core or Both, greyed where not offered")
    func listFromCatalogue() async throws {
        let (model, station) = try await connected()
        let list = ToolListModel(mirror: model.mirror, catalogFeed: model.main.catalogFeed)
        try await deliverCatalogue(station, revision: 2)
        // The feed takes the new catalogue at once; the list reads it on the
        // main queue's next turn (ToolMirrorWatch.queue). Before that read
        // the list is the one built before any catalogue, with the same ids
        // and every Core tool greyed, so wait for TCI Server's reason, which
        // only this catalogue's read gives it.
        #expect(await settle {
            model.main.catalogFeed.revision == 2 && list.entries.map(\.id) == Self.offeredIds
                && list.entries.first { $0.id == "tciServer" }?.reason == StationToolList.noTciServerReason
        })
        #expect(list.entries.map(\.title) == ["Spot Hub", "FreeDV Reporter", "TX Equalizer", "PureSignal",
                                              "Diversity", "TCI Server", "VAX Audio", "Connection and performance",
                                              "Support Bundle"])
        #expect(list.entries.map(\.tag) == [.both, .core, .core, .core, .core, .core, .core, .both, .both])
        #expect(list.entries.allSatisfy { $0.page != nil })
        // This Core runs no TCI server of its own: TCI Server is greyed with the reason; the rest open.
        let tci = try #require(list.entries.first { $0.id == "tciServer" })
        #expect(!tci.enabled && tci.reason == StationToolList.noTciServerReason && tci.detail == tci.reason)
        #expect(list.entries.filter { $0.id != "tciServer" }.allSatisfy { $0.enabled })
        // CWX, the Memory Manager and CAT Control are not built on the desktop: not listed (D41).
        #expect(!list.entries.contains { ["cwx", "memoryManager", "catControl"].contains($0.id) })
        await model.disconnect()
    }

    @Test("a new catalogue while the tab is open changes the list; a tool this app lacks is greyed with its reason")
    func listFollowsCatalogue() async throws {
        let (model, station) = try await connected()
        let list = ToolListModel(mirror: model.mirror, catalogFeed: model.main.catalogFeed)
        try await deliverCatalogue(station, revision: 2)
        #expect(await settle { model.main.catalogFeed.revision == 2 && list.entries.map(\.id) == Self.offeredIds })
        // The desktop builds CWX, and the Core stops offering PureSignal.
        try await deliverCatalogue(station, revision: 3) { tools in
            tools.map { tool in
                var tool = tool
                if tool["id"] as? String == "cwx" {
                    tool["offered"] = true
                }
                if tool["id"] as? String == "pureSignal" {
                    tool["offered"] = false
                }
                return tool
            }
        }
        #expect(await settle { list.entries.contains { $0.id == "cwx" } })
        #expect(list.entries.map(\.id) == ["spotHub", "freedvReporter", "txEqualizer", "diversity", "cwx",
                                           "tciServer", "vaxAudio", "networkDiagnostics", "supportBundle"])
        let cwx = try #require(list.entries.first { $0.id == "cwx" })
        #expect(cwx.title == "CWX" && cwx.tag == .core && cwx.page == nil)
        #expect(cwx.reason == StationToolList.unknownToolReason && !cwx.enabled)
        // The Core goes away: its own tools grey with the reason; this phone's stay.
        await model.disconnect()
        #expect(await settle { list.entries.first { $0.id == "txEqualizer" }?.enabled == false })
        for entry in list.entries {
            if entry.page == nil {
                #expect(entry.reason == StationToolList.unknownToolReason, "\(entry.id)")
            } else if entry.tag == .core {
                #expect(entry.reason == StationToolList.notConnectedReason, "\(entry.id)")
            } else if entry.page != nil {
                #expect(entry.enabled, "\(entry.id)")
            }
        }
    }

    @Test("a tool the Core does not offer: hidden for absent hardware and unbuilt tools, greyed with its reason otherwise")
    func notOfferedTools() async throws {
        let (model, station) = try await connected()
        let list = ToolListModel(mirror: model.mirror, catalogFeed: model.main.catalogFeed)
        // The Hermes Lite 2's catalogue: no diversity receiver, no TCI server of the Core's own.
        try await deliverCatalogue(station, revision: 2, fixture: "catalog-hermes-lite-2")
        #expect(await settle { list.entries.map(\.id) == ["spotHub", "freedvReporter", "txEqualizer", "pureSignal",
                                                          "tciServer", "vaxAudio", "networkDiagnostics",
                                                          "supportBundle"] })
        #expect(list.entries.first { $0.id == "tciServer" }?.reason == StationToolList.noTciServerReason)
        // A headless Core with its own TCI server: VAX Audio greyed, TCI Server opens; no PureSignal on this radio.
        try await deliverCatalogue(station, revision: 3) { tools in
            tools.map { tool in
                var tool = tool
                switch tool["id"] as? String {
                case "vaxAudio"?, "pureSignal"?: tool["offered"] = false
                case "tciServer"?: tool["offered"] = true
                default: break
                }
                return tool
            }
        }
        #expect(await settle { list.entries.map(\.id) == ["spotHub", "freedvReporter", "txEqualizer", "diversity",
                                                          "tciServer", "vaxAudio", "networkDiagnostics",
                                                          "supportBundle"] })
        let vax = try #require(list.entries.first { $0.id == "vaxAudio" })
        #expect(!vax.enabled && vax.reason == StationToolList.noVaxReason && vax.page == .vaxAudio)
        #expect(list.entries.first { $0.id == "tciServer" }?.enabled == true)
        // The catalogue offers VAX Audio again while the tab is open: it opens.
        try await deliverCatalogue(station, revision: 4)
        #expect(await settle { list.entries.first { $0.id == "vaxAudio" }?.enabled == true })
        await model.disconnect()
    }

    @Test("a Core that leaves Connection and performance unoffered still gets it: this phone measures it")
    func performanceAlwaysOpens() throws {
        let tools = try JSONDecoder().decode([StationCatalog.Tool].self, from: Data("""
        [{"id":"networkDiagnostics","label":"Network Diagnostics","where":"both","offered":false},
         {"id":"spotHub","label":"Spot Hub","where":"both","offered":false}]
        """.utf8))
        let entries = StationToolList.entries(tools: tools, connected: true, olderCore: false)
        #expect(entries.map(\.id) == ["networkDiagnostics", "spotHub"])
        #expect(entries[0].enabled)
        #expect(entries[1].reason == StationToolList.notOfferedReason)
    }

    @Test("before a Core lists its tools, this app's tools are listed; the Core's are greyed with the reason")
    func listWithoutCatalogue() throws {
        let away = StationToolList.entries(tools: nil, connected: false, olderCore: false)
        #expect(away.map(\.id) == Self.offeredIds)
        #expect(away.filter { $0.tag == .core }.allSatisfy { $0.reason == StationToolList.notConnectedReason })
        #expect(away.filter { $0.tag == .both }.allSatisfy { $0.enabled })
        let waiting = StationToolList.entries(tools: nil, connected: true, olderCore: false)
        #expect(waiting.filter { $0.tag == .core }.allSatisfy { $0.reason == StationToolList.notListedReason })
        let older = StationToolList.entries(tools: nil, connected: true, olderCore: true)
        #expect(older.filter { $0.tag == .core }.allSatisfy { $0.reason == StationToolList.olderCoreReason })
        #expect(older.first { $0.page == .performance }?.enabled == true)
        // A Core that leaves Connection and performance out still gets it: this phone measures it.
        let tools = try JSONDecoder().decode([StationCatalog.Tool].self, from: Data("""
        [{"id":"spotHub","label":"Spot Hub","where":"both","offered":true}]
        """.utf8))
        let bare = StationToolList.entries(tools: tools, connected: true, olderCore: false)
        #expect(bare.map(\.id) == ["spotHub", "networkDiagnostics"])
    }

    // MARK: TX Equalizer

    @Test("TX Equalizer reads the Core's transmit EQ and writes each change to it")
    func txEqualizer() async throws {
        let (model, station) = try await connected()
        let eq = TxEqualizerModel(mirror: model.mirror, transmit: model.main.transmit, commands: model.commands)
        #expect(await settle { eq.bands != nil && eq.editorReason == nil && eq.switchReason == nil })
        #expect(eq.enabled == false && eq.legacy == true && eq.preamp == 0)
        #expect(eq.bands == [-12, -12, -12, -1, 1, 4, 9, 12, -10, -10])
        #expect(eq.frequencies == [32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000])
        #expect(eq.size == 2048 && eq.minimumPhase == false && eq.cutoff == 0 && eq.window == 0 && !eq.hasCurve)
        eq.setEnabled(true)
        #expect(await answer(station, key: "transmit", "txEqEnabled") == .bool(true))
        #expect(await settle { eq.enabled == true })
        // One band's gain: the ten go together, held to the Core's range.
        eq.setBand(2, 40)
        #expect(await answer(station, key: "transmit", "txEqBandsJson") == .utf8("[-12,-12,15,-1,1,4,9,12,-10,-10]"))
        #expect(await settle { eq.bands?[2] == 15 })
        eq.setPreamp(-5)
        #expect(await answer(station, key: "transmit", "txEqPreamp") == .i64(-5))
        eq.setCutoff(1)
        #expect(await answer(station, key: "transmit", "txEqCtfmode") == .i64(1))
        eq.setWindow(1)
        #expect(await answer(station, key: "transmit", "txEqWintype") == .i64(1))
        eq.setMinimumPhase(true)
        #expect(await answer(station, key: "transmit", "txEqMp") == .bool(true))
        eq.setLegacy(false)
        #expect(await answer(station, key: "transmit", "txEqUseLegacy") == .bool(false))
        // A band's centre and the filter size, on the number pad.
        eq.openFrequencyPad(9)
        let pad = try #require(eq.pad)
        type(pad, "15000")
        async let entered = pad.enter()
        #expect(await answer(station, key: "transmit", "txEqFreqsJson")
                    == .utf8("[32,63,125,250,500,1000,2000,4000,8000,15000]"))
        #expect(await entered)
        eq.openSizePad()
        let size = try #require(eq.pad)
        type(size, "4096")
        async let sized = size.enter()
        // The Core refuses it: its words show, and the pad stays open.
        #expect(await answer(station, key: "transmit", "txEqNc", refuse: "Choose a filter size from 32 to 8192.")
                    == .i64(4096))
        #expect(await sized == false)
        #expect(eq.note == "Choose a filter size from 32 to 8192.")
        await model.disconnect()
        #expect(await settle { eq.editorReason == TxEqualizerModel.notConnectedReason })
    }

    @Test("TX Equalizer greys every setting on an older Core, on the air below version 13, and while another device holds transmit")
    func txEqualizerGreyed() async throws {
        let (older, oldStation) = try await connected(without: ["transmitSettingsVersion"])
        let eq = TxEqualizerModel(mirror: older.mirror, transmit: older.main.transmit, commands: older.commands)
        #expect(await settle { eq.bands != nil && eq.editorReason != nil })
        #expect(eq.editorReason == TransmitModel.settingsNotTakenText)
        #expect(eq.switchReason == TransmitModel.settingsNotTakenText)
        eq.setBand(0, 3)
        eq.setEnabled(true)
        #expect(!oldStation.messages.contains { message in
            if case .propertyWrite(let write) = message {
                return write.key == "transmit"
            }
            return false
        })
        await older.disconnect()

        let (model, station) = try await connected()
        let onAir = TxEqualizerModel(mirror: model.mirror, transmit: model.main.transmit, commands: model.commands)
        #expect(await settle { onAir.bands != nil && onAir.editorReason == nil })
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(true)),
        ])))
        // At transmitSettingsVersion 13 and later every TX EQ control stays live on the air, as on the desktop.
        #expect(await settle { onAir.transmitting })
        #expect(onAir.takesOnAir && onAir.editorReason == nil && onAir.switchReason == nil)
        #expect(onAir.profileReason == nil && onAir.saveReason == nil)
        // Below 13 the Core takes none of them on the air: greyed, the profile menu and Save included.
        SetupDescribedPagesTests.withCapabilities(model, ["transmitSettingsVersion": .i64(12)])
        #expect(await settle { onAir.editorReason == TransmitModel.onAirText })
        #expect(!onAir.takesOnAir)
        #expect(onAir.switchReason == TransmitModel.onAirText && onAir.profileReason == TransmitModel.onAirText)
        #expect(onAir.saveReason == TransmitModel.onAirText)
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(false)),
        ])))
        #expect(await settle { onAir.editorReason == nil })
        await model.disconnect()

        // Another device holding transmit: its words, at every version.
        let (heldApp, heldStation) = try await connected(additions: [.remoteTx])
        let held = TxEqualizerModel(mirror: heldApp.mirror, transmit: heldApp.main.transmit,
                                    commands: heldApp.commands)
        #expect(await settle { held.bands != nil && held.editorReason == nil })
        heldApp.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await heldStation.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("ipad-1", short: "JJ\u{2019}s iPad", keyed: false))))
        let words = TxEqualizerModel.holderReason("JJ\u{2019}s iPad")
        #expect(await settle { held.editorReason == words })
        #expect(held.switchReason == words && held.profileReason == words && held.saveReason == words)
        await heldApp.disconnect()
    }

    @Test("a greyed choice row shows its chosen choice in grey, never in the blue of one that can be pressed")
    func greyedChoiceIsNotBlue() {
        typealias Choices = ToolPageParts.Choices
        let chosen = Choices.look(chosen: true, enabled: true)
        #expect(chosen.fill == ChromeColours.buttonOnBlue && chosen.border == ChromeColours.buttonOnBlueBorder)
        let greyedChosen = Choices.look(chosen: true, enabled: false)
        let greyedOther = Choices.look(chosen: false, enabled: false)
        let blues = [ChromeColours.buttonOnBlue, ChromeColours.buttonOnBlueBorder]
        #expect(!blues.contains(greyedChosen.fill) && !blues.contains(greyedChosen.border))
        #expect(greyedChosen.text != .white)
        // Greyed like the rest of the row, and still told apart from the others.
        #expect(greyedChosen.fill == ChromeColours.buttonOff && greyedOther.fill == ChromeColours.buttonOff)
        #expect(greyedChosen.border != greyedOther.border && greyedChosen.text != greyedOther.text)
    }

    @Test("a greyed accessory choice and a greyed logging category keep their chosen one grey, as the choice rows do")
    func greyedChosenElsewhereIsNotBlue() {
        let blues = [ChromeColours.buttonOnBlue, ChromeColours.buttonOnBlueBorder]
        for enabled in [true, false] {
            for chosen in [true, false] {
                let rows = ToolPageParts.Choices.look(chosen: chosen, enabled: enabled)
                #expect(AccessoryChrome.ChoiceButton.look(lit: chosen, enabled: enabled) == rows)
                #expect(SupportBundlePage.CategoryButton.look(isOn: chosen, enabled: enabled) == rows)
            }
        }
        let greyed = AccessoryChrome.ChoiceButton.look(lit: true, enabled: false)
        #expect(!blues.contains(greyed.fill) && !blues.contains(greyed.border) && greyed.text != .white)
        let greyedCategory = SupportBundlePage.CategoryButton.look(isOn: true, enabled: false)
        #expect(!blues.contains(greyedCategory.fill) && !blues.contains(greyedCategory.border))
    }

    // MARK: PureSignal

    @Test("PureSignal turns on and off with the Core's verbs and shows its status; calibration stays at the Core")
    func pureSignal() async throws {
        let (model, station) = try await connected()
        let ps = PureSignalModel(mirror: model.mirror, transmit: model.main.transmit,
                                 catalogFeed: model.main.catalogFeed)
        #expect(await settle { ps.available && ps.onReason == nil })
        #expect(!ps.on && ps.radioHasIt)
        ps.set(on: true)
        #expect(await answerCommand(station, "ps3.automatic")?.args.isEmpty == true)
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignalSettings", properties: [
            .init(ordinal: 0, name: "autoCalEnabled", value: .bool(true)),
        ])))
        #expect(await settle { ps.on })
        let status = """
        {"schema":1,"psEnabled":true,"mox":true,"engineState":3,"feedbackLevel":180,\
        "successfulCalibrations":4,"attemptedCalibrations":6,"correctionsApplied":true}
        """
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 3, name: "statusJson", value: .utf8(status)),
        ])))
        #expect(await settle { ps.status?.feedbackLevel == 180 })
        #expect(ps.status?.calibrating == true && ps.status?.hearingFeedback == true)
        #expect(ps.stateText == "Calibrating")
        ps.set(on: false)
        #expect(await answerCommand(station, "ps3.off") != nil)
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignalSettings", properties: [
            .init(ordinal: 0, name: "autoCalEnabled", value: .bool(false)),
        ])))
        #expect(await settle { !ps.on })
        // Nothing here asks for a single calibration, a saved correction or the two-tone test.
        #expect(!station.messages.contains { message in
            guard let invoke = AccessoryPagesTests.invoke(message) else {
                return false
            }
            return ["ps3.single", "ps3.applyCurrent", "ps3.twoTone", "ps3.saveCorrection",
                    "ps3.restoreCorrection"].contains(invoke.verb)
        })
        // The Core's words for a failed action show.
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 4, name: "lastActionError", value: .utf8("PureSignal is unavailable until the radio is ready.")),
        ])))
        #expect(await settle { ps.actionError == "PureSignal is unavailable until the radio is ready." })
        await model.disconnect()
        #expect(await settle { ps.onReason == PureSignalModel.notConnectedReason && ps.status == nil })
    }

    @Test("PureSignal is greyed on an older Core, and a radio without it shows no controls")
    func pureSignalGreyed() async throws {
        let (older, _) = try await connected(without: ["psAlgorithmVersion"])
        let ps = PureSignalModel(mirror: older.mirror, transmit: older.main.transmit,
                                 catalogFeed: older.main.catalogFeed)
        #expect(await settle { ps.onReason == TransmitModel.psaNotOfferedText })
        await older.disconnect()

        let (model, station) = try await connected()
        let none = PureSignalModel(mirror: model.mirror, transmit: model.main.transmit,
                                   catalogFeed: model.main.catalogFeed)
        try await deliverCatalogue(station, revision: 2, board: ["pureSignal": false])
        #expect(await settle { !none.radioHasIt })
        await model.disconnect()
    }

    // MARK: Diversity

    @Test("Diversity writes slice A's diversity, and this phone's memories store and recall its phase and gain")
    func diversity() async throws {
        let defaults = try #require(UserDefaults(suiteName: "ToolsPagesTests-\(UUID().uuidString)"))
        let phone = PhoneSettings(defaults: defaults)
        let (model, station) = try await connected(phone: phone)
        let diversity = DiversityModel(mirror: model.mirror, phone: phone)
        #expect(await settle { diversity.reason == nil && diversity.sliceKey == "slice:0" })
        #expect(diversity.enabled == false && diversity.phaseDeg == 0 && diversity.gainDb == 0 && diversity.band == 5)
        diversity.setEnabled(true)
        #expect(await answer(station, key: "slice:0", "diversityEnabled") == .bool(true))
        diversity.setPhase(123.46)
        #expect(await answer(station, key: "slice:0", "diversityPhaseDeg") == .f64(123.5))
        diversity.setGain(-30)
        #expect(await answer(station, key: "slice:0", "diversityGainDb") == .f64(-20))
        #expect(await settle { diversity.phaseDeg == 123.5 && diversity.gainDb == -20 })
        // Store into M3, then move away and recall it.
        diversity.storing = true
        diversity.tapMemory(2)
        #expect(!diversity.storing)
        #expect(diversity.memories[2] == DiversityModel.Memory(phaseDeg: 123.5, gainDb: -20))
        #expect(phone.string(DiversityModel.memoryKey(band: 5, 2), default: "") == "123.5,-20.0")
        diversity.setPhase(0)
        #expect(await answer(station, key: "slice:0", "diversityPhaseDeg") == .f64(0))
        #expect(await settle { diversity.phaseDeg == 0 })
        diversity.tapMemory(2)
        #expect(await answer(station, key: "slice:0", "diversityPhaseDeg") == .f64(123.5))
        #expect(await answer(station, key: "slice:0", "diversityGainDb") == .f64(-20))
        // PureSignal calibrating on the air: the Core holds diversity, and the page says so.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: try ordinal("SliceModel", "psPaused"), name: "psPaused", value: .bool(true)),
        ])))
        #expect(await settle { diversity.paused })
        await model.disconnect()
        #expect(await settle { diversity.reason == DiversityModel.notConnectedReason })
        let before = station.messages.count
        diversity.setEnabled(false)
        #expect(station.messages.count == before)
    }

    // MARK: TCI Server

    @Test("TCI Server shows the Core's switch, port and apps, changes them and closes an app")
    func tciServer() async throws {
        let (model, station) = try await connected(additions: [.spots, .stationTci])
        let tci = TciServerModel(mirror: model.mirror, commands: model.commands, records: model.records)
        #expect(await settle { tci.reason == TciServerModel.noServerReason })
        try await station.deliverStationTci(.board)
        #expect(await settle { tci.reason == nil && tci.enabled == true })
        #expect(tci.port == 50001 && tci.listening && tci.stationAddress == "192.0.2.10")
        #expect(tci.stateText == "Listening on port 50001")
        // The apps are asked for only while the page shows.
        #expect(!station.messages.contains { AccessoryPagesTests.invoke($0)?.verb == "records.subscribe"
                && AccessoryPagesTests.invoke($0)?.args.first?.value == .utf8("tciClients") })
        tci.setOpen(true)
        #expect(await settle { tci.clients.count == 2 })
        #expect(tci.clients.map(\.name) == ["WSJT-X", "RF2K-S"])
        #expect(tci.clients.first?.subscriptions == ["audio", "sensors"])
        tci.setEnabled(false)
        #expect(await settle { tci.enabled == false })
        let set = try #require(await sent(station, "setStationTci"))
        #expect(set == [.init(name: "enabled", value: .bool(false)), .init(name: "port", value: .i64(50001))])
        tci.openPortPad()
        let pad = try #require(tci.pad)
        type(pad, "50555")
        #expect(await pad.enter())
        #expect(await settle { tci.port == 50555 })
        tci.disconnect(try #require(tci.clients.first))
        #expect(await settle { tci.clients.map(\.id) == ["2"] })
        station.refuseNext("disconnectStationTciClient", reason: "The radio is on the air. Try again when it stops.")
        tci.disconnect(try #require(tci.clients.first))
        #expect(await settle { tci.note == "The radio is on the air. Try again when it stops." })
        tci.setOpen(false)
        #expect(await settle { tci.clients.isEmpty })
        await model.disconnect()
        #expect(await settle { tci.reason == TciServerModel.notConnectedReason })
    }

    @Test("TCI Server's port pad keeps the Core's switch as it is now, a wordless refusal still says so, and on the air Disconnect waits")
    func tciServerChangesBehindAPad() async throws {
        let (model, station) = try await connected(additions: [.spots, .stationTci])
        let tci = TciServerModel(mirror: model.mirror, commands: model.commands, records: model.records)
        try await station.deliverStationTci(.board)
        tci.setOpen(true)
        #expect(await settle { tci.enabled == true && tci.clients.count == 2 })
        // The pad opens with the server on; it is turned off before Enter.
        tci.openPortPad()
        let pad = try #require(tci.pad)
        tci.setEnabled(false)
        #expect(await settle { tci.enabled == false && !tci.sending })
        type(pad, "50555")
        #expect(await pad.enter())
        #expect(await settle { tci.port == 50555 })
        let moved = station.messages.compactMap(AccessoryPagesTests.invoke).last { $0.verb == "setStationTci" }
        #expect(moved?.args == [.init(name: "enabled", value: .bool(false)), .init(name: "port", value: .i64(50555))])
        #expect(tci.enabled == false)
        // A refusal the Core gave no words for still shows.
        station.refuseNext("disconnectStationTciClient", reason: "")
        tci.disconnect(try #require(tci.clients.first))
        #expect(await settle { tci.note == BandSlicesModel.refusedText })
        // On the air the Core refuses a disconnect: the page says why and sends nothing.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { tci.optionsReason == TciServerModel.onAirReason })
        #expect(tci.disconnectReason == TciServerModel.onAirReason)
        tci.disconnect(try #require(tci.clients.first))
        #expect(tci.note == TciServerModel.onAirReason)
        await model.disconnect()
    }

    @Test("TCI Server is greyed with its reason on a Core without a TCI server, and sends nothing")
    func tciServerOlderCore() async throws {
        let (model, station) = try await connected()
        let tci = TciServerModel(mirror: model.mirror, commands: model.commands, records: model.records)
        tci.setOpen(true)
        #expect(await settle { tci.reason == TciServerModel.noServerReason })
        #expect(tci.clientsReason == TciServerModel.noServerReason && tci.enabled == nil)
        tci.setEnabled(true)
        tci.openPortPad()
        #expect(tci.pad == nil)
        #expect(!station.messages.contains { AccessoryPagesTests.invoke($0)?.verb == "setStationTci" })
        await model.disconnect()
    }

    @Test("TCI Server's four options read from the Core, change whole through its verb and grey on the air")
    func tciOptions() async throws {
        let (model, station) = try await connected(additions: [.spots, .stationTci])
        let tci = TciServerModel(mirror: model.mirror, commands: model.commands, records: model.records)
        #expect(await settle { tci.optionsReason == TciServerModel.noServerReason })
        #expect(tci.options == nil)
        try await station.deliverStationTci(.board)
        #expect(await settle { tci.optionsReason == nil && tci.options != nil })
        #expect(tci.options == StationTciOptions(emulateExpertSdr3: true, emulateSunSdr2Pro: true, cwluBecomesCw: false,
                                                 sendInitialState: true))
        #expect(TciServerModel.optionRows.map(\.title) == ["Emulate ExpertSDR3 protocol", "Emulate SunSDR2 PRO device",
                                                            "CWL/CWU becomes CW", "Send initial state on connect"])
        // One option changes; all four go, in the verb's order, and the Core's copy follows.
        tci.setOption(\.cwluBecomesCw, true)
        #expect(tci.options?.cwluBecomesCw == true)
        let sent = try #require(await sent(station, "setStationTciOptions"))
        #expect(sent == [.init(name: "emulateExpertSdr3", value: .bool(true)),
                         .init(name: "emulateSunSdr2Pro", value: .bool(true)),
                         .init(name: "cwluBecomesCw", value: .bool(true)),
                         .init(name: "sendInitialState", value: .bool(true))])
        #expect(await settle { model.mirror.object("stationTci")?["cwluBecomesCw"] == .bool(true) })
        #expect(tci.options?.cwluBecomesCw == true)
        // A refusal shows the Core's words and its options again.
        station.refuseNext("setStationTciOptions", reason: "The radio is on the air. Try again when it stops.")
        tci.setOption(\.sendInitialState, false)
        #expect(await settle { tci.note == "The radio is on the air. Try again when it stops." })
        #expect(await settle { tci.options?.sendInitialState == true })
        // On the air the options grey with the reason, and nothing is sent.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { tci.optionsReason == TciServerModel.onAirReason })
        let before = station.messages.filter { AccessoryPagesTests.invoke($0)?.verb == "setStationTciOptions" }.count
        tci.setOption(\.emulateExpertSdr3, false)
        #expect(tci.options?.emulateExpertSdr3 == true)
        #expect(station.messages.filter { AccessoryPagesTests.invoke($0)?.verb == "setStationTciOptions" }.count
                    == before)
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(false)),
        ])))
        #expect(await settle { tci.optionsReason == nil })
        await model.disconnect()
        #expect(await settle { tci.optionsReason == TciServerModel.notConnectedReason && tci.options == nil })
    }

    // MARK: Support Bundle

    @Test("Support Bundle writes this phone's log and the Core's bundle as files for the share sheet, sending nothing else")
    func supportBundle() async throws {
        let (model, station) = try await connected(additions: [.supportBundle])
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("ToolsPagesTests-\(UUID())")
        defer { try? FileManager.default.removeItem(at: directory) }
        let bundle = SupportBundleModel(mirror: model.mirror, commands: model.commands,
                                        phoneLog: PhoneLog { ["first line", "second line"] }, directory: directory,
                                        now: { Date(timeIntervalSince1970: 1_790_000_000) })
        #expect(await settle { bundle.coreReason == nil })
        await bundle.collect()
        #expect(bundle.state == .ready && bundle.coreNote == nil)
        #expect(bundle.files.map(\.lastPathComponent) == ["NereusSDR-phone-log-20260921-141320.txt",
                                                          "NereusSDR-Core-support-20260921-141320.zip"])
        let phone = try String(contentsOf: bundle.files[0], encoding: .utf8)
        #expect(phone.hasPrefix("NereusSDR for iPhone and iPad\n"))
        // The header's build number is the one About shows: the Info.plist's CFBundleVersion.
        let info = Bundle.main.infoDictionary
        let version = try #require(info?["CFBundleShortVersionString"] as? String)
        let build = try #require(info?["CFBundleVersion"] as? String)
        #expect(phone.contains("\nVersion \(version) (\(build))\n"))
        #expect(phone.hasSuffix("first line\nsecond line\n"))
        #expect(try Data(contentsOf: bundle.files[1]) == FakeStation.supportBundle)
        #expect(bundle.files.allSatisfy { $0.path.hasPrefix(directory.path) })
        // One request to the Core, with nothing of the files in it.
        let asked = station.messages.compactMap(AccessoryPagesTests.invoke).filter { $0.verb == "support.collect" }
        #expect(asked.count == 1 && asked.first?.args.isEmpty == true)
        // The Core refuses: the phone's log goes alone, with the Core's words.
        station.refuseNext("support.collect", reason: "The Core is already making a support bundle. Try again in a moment.")
        await bundle.collect()
        #expect(bundle.files.count == 1 && bundle.files.first?.lastPathComponent.hasSuffix(".txt") == true)
        #expect(bundle.coreNote == "The Core is already making a support bundle. Try again in a moment.")
        await model.disconnect()
        #expect(await settle { bundle.coreReason == SupportBundleModel.notConnectedReason })
    }

    @Test("Support Bundle on an older Core collects this phone's log alone and says why")
    func supportBundleOlderCore() async throws {
        let (model, station) = try await connected(without: ["supportBundleVersion"])
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("ToolsPagesTests-\(UUID())")
        defer { try? FileManager.default.removeItem(at: directory) }
        let bundle = SupportBundleModel(mirror: model.mirror, commands: model.commands,
                                        phoneLog: PhoneLog { throw CocoaError(.fileReadUnknown) }, directory: directory)
        #expect(await settle { bundle.coreReason == SupportBundleModel.olderCoreReason })
        await bundle.collect()
        #expect(bundle.files.count == 1 && bundle.coreNote == SupportBundleModel.olderCoreReason)
        #expect(bundle.phoneLogFailed)
        #expect(try String(contentsOf: bundle.files[0], encoding: .utf8).contains(SupportBundleModel.phoneLogUnreadable))
        #expect(!station.messages.contains { AccessoryPagesTests.invoke($0)?.verb == "support.collect" })
        await model.disconnect()
    }

    @Test("Support Bundle shows the Core's recent log while it is open, and copies it as shown")
    func coreLog() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let log = CoreLogModel(mirror: model.mirror, commands: model.commands, records: model.records)
        #expect(await settle { log.reason == nil })
        // The log is asked for only while the page shows.
        #expect(!station.messages.contains { Self.asksFor($0, "coreLog") })
        log.setOpen(true)
        #expect(await settle { log.lines.count == 3 })
        #expect(log.lines.map(\.line) == FakeStation.coreLogLines)
        let subscribe = try #require(station.messages.compactMap(AccessoryPagesTests.invoke)
            .first { $0.verb == "records.subscribe" && $0.args.first?.value == .utf8("coreLog") })
        #expect(subscribe.args.last == .init(name: "backlog", value: .i64(200)))
        await station.deliverCoreLog(["[18:35:00.000] INF: Slice A tuned"])
        #expect(await settle { log.lines.count == 4 })
        #expect(log.text == (FakeStation.coreLogLines + ["[18:35:00.000] INF: Slice A tuned"]).joined(separator: "\n"))
        // Reload asks again, and the Core's newest lines come back as a reset.
        log.reload()
        #expect(await settle {
            station.messages.filter { Self.asksFor($0, "coreLog") }.count == 2 && log.lines.count == 4
        })
        log.setOpen(false)
        #expect(await settle { log.lines.isEmpty })
        // The stop goes to the Core on its own task, after the copy is dropped.
        #expect(await settle {
            station.messages.contains { message in
                AccessoryPagesTests.invoke(message)?.verb == "records.unsubscribe"
                    && AccessoryPagesTests.invoke(message)?.args.first?.value == .utf8("coreLog")
            }
        })
        await model.disconnect()
        #expect(await settle { log.reason == CoreLogModel.notConnectedReason })
        #expect(log.text == CoreLogModel.notConnectedReason)
    }

    @Test("Support Bundle's refusal of the log goes when the Core sends it again; a wordless refusal still shows")
    func coreLogRefusalClears() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let log = CoreLogModel(mirror: model.mirror, commands: model.commands, records: model.records)
        #expect(await settle { log.reason == nil })
        station.refuseNext("records.subscribe", reason: "The Core is busy. Try again.")
        log.setOpen(true)
        #expect(await settle { log.note == "The Core is busy. Try again." })
        // Reload: the Core sends the log, and its refusal no longer shows.
        log.reload()
        #expect(await settle { log.lines.count == 3 })
        #expect(await settle { log.note == nil })
        station.refuseNext("support.setLogCategories", reason: "")
        log.setCategory("nereus.tci", true)
        #expect(await settle { log.note == BandSlicesModel.refusedText })
        await model.disconnect()
    }

    @Test("Support Bundle turns the Core's logging categories on and off, keeping one this app does not know")
    func coreLogCategories() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let log = CoreLogModel(mirror: model.mirror, commands: model.commands, records: model.records)
        #expect(await settle { log.reason == nil })
        #expect(log.on.isEmpty && log.categories == CoreLogModel.known)
        #expect(log.categories.map(\.title) == ["Discovery", "Connection", "Protocol", "Receiver", "Audio", "DSP",
                                                "Spectrum", "Container", "Meter", "MMIO", "TCI", "Spots"])
        log.setCategory("nereus.tci", true)
        #expect(log.on == ["nereus.tci"])
        #expect(try #require(await sent(station, "support.setLogCategories"))
                    == [.init(name: "categories", value: .utf8("nereus.tci"))])
        #expect(await settle { station.coreLogCategories == "nereus.tci" })
        #expect(await settle { model.mirror.object("radio")?["logCategories"] == .text("nereus.tci") })
        // All on, in the dialog's order.
        log.setAll(true)
        #expect(await settle { station.coreLogCategories == CoreLogModel.known.map(\.id).joined(separator: ",") })
        #expect(await settle { log.on.count == 12 })
        // The Core has one on that this app does not know: listed by its id, and kept when another changes.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "logCategories"), name: "logCategories",
                  value: .utf8("nereus.discovery,nereus.future")),
        ])))
        #expect(await settle { log.on == ["nereus.discovery", "nereus.future"] })
        #expect(log.categories.last == CoreLogModel.Category(id: "nereus.future", title: "nereus.future"))
        log.setCategory("nereus.discovery", false)
        #expect(await settle { station.coreLogCategories == "nereus.future" })
        // A refusal shows the Core's words and its categories again.
        station.refuseNext("support.setLogCategories", reason: "The Core could not read this request.")
        log.setAll(false)
        #expect(await settle { log.note == "The Core could not read this request." })
        #expect(await settle { log.on == ["nereus.future"] })
        await model.disconnect()
    }

    @Test("Support Bundle on an older Core greys the logging and shows the reason in place of the log")
    func coreLogOlderCore() async throws {
        let (model, station) = try await connected(additions: [.spots], without: ["supportBundleVersion"])
        let log = CoreLogModel(mirror: model.mirror, commands: model.commands, records: model.records)
        log.setOpen(true)
        #expect(await settle { log.reason == CoreLogModel.olderCoreReason })
        #expect(log.text == CoreLogModel.olderCoreReason)
        log.setCategory("nereus.tci", true)
        log.setAll(true)
        #expect(log.on.isEmpty)
        #expect(!station.messages.contains { AccessoryPagesTests.invoke($0)?.verb == "support.setLogCategories" })
        #expect(!station.messages.contains { Self.asksFor($0, "coreLog") })
        await model.disconnect()
    }

    @Test("with logCategoryList the Support Bundle's switches carry the Core's own ids and labels, in its order")
    func coreLogCategoryLabelsFromTheCore() async throws {
        let (model, station) = try await connected(additions: [.supportBundle, .logCategoryList])
        let log = model.coreLog
        let suite = try #require(LogCategoryList(json: try FakeStation.suiteLogCategoryList()))
        #expect(await settle { log.reason == nil })
        #expect(log.categories.map(\.id) == suite.categories.map(\.id))
        #expect(log.categories.map(\.title) == suite.categories.map(\.label))
        // A Core with other categories and words: the page lists the Core's,
        // not the phone's. (The Core sends its list in the snapshot only; this
        // delta stands in for a different Core's list.)
        let other = #"{"categories":[{"id":"nereus.tci","label":"TCI server"},{"id":"nereus.rade","label":"RADE"}]}"#
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "logCategoryList"), name: "logCategoryList", value: .utf8(other)),
        ])))
        #expect(await settle { log.categories.map(\.title) == ["TCI server", "RADE"] })
        log.setAll(true)
        #expect(await settle { station.coreLogCategories == "nereus.tci,nereus.rade" })
        await model.disconnect()

        // An older Core: the phone's own list, as before.
        let (older, _) = try await connected(additions: [.supportBundle])
        #expect(await settle { older.coreLog.reason == nil })
        #expect(older.coreLog.categories == PhoneHeldLogCategories.list)
        await older.disconnect()
    }

    @Test("the logging categories list the Core's own labels when it sends them, else the phone's, then other ids on")
    func coreLogCategoryList() {
        let phoneHeld = CoreLogModel.listed(offered: nil, on: ["nereus.tci", "nereus.future"])
        #expect(phoneHeld == CoreLogModel.known + [CoreLogModel.Category(id: "nereus.future", title: "nereus.future")])
        let coreOwn = [CoreLogModel.Category(id: "nereus.tci", title: "TCI server"),
                       CoreLogModel.Category(id: "nereus.future", title: "Future")]
        #expect(CoreLogModel.listed(offered: coreOwn, on: ["nereus.future", "nereus.other"])
            == coreOwn + [CoreLogModel.Category(id: "nereus.other", title: "nereus.other")])
    }

    // MARK: A choice the Core overtakes

    /// The Core's echo of a choice and a newer value land in one turn of the
    /// main queue, so no read ever sees the echo alone. The page shows the
    /// Core's newer value, whether the answer came before the two or after,
    /// and whether the newer value is another device's or the one from
    /// before the choice.
    @Test("the logging switches give way to the Core's value when its echo and a newer one land in one turn",
          arguments: [(true, "nereus.discovery,nereus.future"), (false, ""), (true, ""),
                      (false, "nereus.discovery,nereus.future")])
    func coreLogChoiceOvertaken(answerFirst: Bool, later: String) async throws {
        let (store, commands, outbox) = await handFed(CoreLogLine.capabilityName, version: 1, key: "radio",
                                                      className: "RadioModel",
                                                      values: [.init(ordinal: 0, name: "logCategories",
                                                                     value: .utf8(""))])
        let log = CoreLogModel(mirror: store, commands: commands, records: nil)
        #expect(await settle { log.reason == nil && log.on.isEmpty })
        log.setCategory("nereus.tci", true)
        #expect(log.on == ["nereus.tci"])
        #expect(await settle { outbox.invokes(CoreLogLine.setCategoriesVerb).count == 1 })
        func landTogether() {
            store.apply(.delta(LinkMessage.Delta(key: "radio", properties: [
                .init(ordinal: 0, name: "logCategories", value: .utf8("nereus.tci")),
            ])))
            store.apply(.delta(LinkMessage.Delta(key: "radio", properties: [
                .init(ordinal: 0, name: "logCategories", value: .utf8(later)),
            ])))
        }
        if !answerFirst {
            landTogether()
        }
        #expect(await accept(commands, outbox, CoreLogLine.setCategoriesVerb))
        if answerFirst {
            landTogether()
        }
        let expected = Set(later.split(separator: ",").map(String.init))
        #expect(await settle(seconds: 10) { log.on == expected })
        #expect(outbox.invokes(CoreLogLine.setCategoriesVerb).count == 1)
    }

    /// The same for TCI Server's options: the echo and a newer value in one
    /// turn leave the page on the Core's newer options.
    @Test("TCI Server's options give way to the Core's when its echo and a newer value land in one turn",
          arguments: [(true, false), (false, false), (true, true), (false, true)])
    func tciOptionsOvertaken(answerFirst: Bool, backToBefore: Bool) async throws {
        func flags(_ options: [Bool]) -> [LinkMessage.PropertyEntry] {
            StationTciOptions.names.enumerated().map { index, name in
                .init(ordinal: UInt16(5 + index), name: name, value: .bool(options[index]))
            }
        }
        let before = [true, true, false, true]
        let (store, commands, outbox) = await handFed(
            StationTciClient.capabilityName, version: StationTciOptions.version, key: StationTciClient.objectKey,
            className: FakeStation.tciClass,
            values: [.init(ordinal: 0, name: "enabled", value: .bool(true)),
                     .init(ordinal: 1, name: "port", value: .i64(40001)),
                     .init(ordinal: 2, name: "listening", value: .bool(true)),
                     .init(ordinal: 3, name: "stationAddress", value: .utf8("")),
                     .init(ordinal: 4, name: "error", value: .utf8(""))] + flags(before))
        let tci = TciServerModel(mirror: store, commands: commands, records: nil)
        #expect(await settle { tci.optionsReason == nil && tci.options?.cwluBecomesCw == false })
        tci.setOption(\.cwluBecomesCw, true)
        #expect(tci.options?.cwluBecomesCw == true)
        #expect(await settle { outbox.invokes(StationTciOptions.verb).count == 1 })
        let later = backToBefore ? before : [false, false, true, false]
        func landTogether() {
            store.apply(.delta(LinkMessage.Delta(key: StationTciClient.objectKey,
                                                 properties: flags([true, true, true, true]))))
            store.apply(.delta(LinkMessage.Delta(key: StationTciClient.objectKey, properties: flags(later))))
        }
        if !answerFirst {
            landTogether()
        }
        #expect(await accept(commands, outbox, StationTciOptions.verb))
        if answerFirst {
            landTogether()
        }
        let expected = StationTciOptions(emulateExpertSdr3: later[0], emulateSunSdr2Pro: later[1],
                                         cwluBecomesCw: later[2], sendInitialState: later[3])
        #expect(await settle(seconds: 10) { tci.options == expected })
        #expect(outbox.invokes(StationTciOptions.verb).count == 1)
    }

    // MARK: Coordinated Diversity summary (scoped synthetic v1, not the baseline fixture)

    @Test("Diversity follows the complete live summary even when that slice is unjoined")
    func diversitySummaryUnjoinedLive() throws {
        let store = diversitySummaryStore(live: 1)
        addDiversitySlice(store, id: 0, active: true, enabled: false, phase: 11, gain: 1)
        let defaults = UserDefaults(suiteName: "diversity-summary-unjoined")!
        defer { defaults.removePersistentDomain(forName: "diversity-summary-unjoined") }
        let diversity = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults))
        #expect(diversity.sliceKey == "slice:1")
        #expect(diversity.enabled == true)
        #expect(diversity.phaseDeg == 123.4 && diversity.gainDb == -2 && diversity.band == 5)
    }

    @Test("Diversity off with no active candidate never aliases B to missing A")
    func diversitySummaryOffWithoutActive() throws {
        let store = diversitySummaryStore(live: nil)
        addDiversitySlice(store, id: 1, active: false, enabled: false, phase: 31, gain: 3)
        let defaults = UserDefaults(suiteName: "diversity-summary-absent-a")!
        defer { defaults.removePersistentDomain(forName: "diversity-summary-absent-a") }
        let diversity = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults))
        #expect(diversity.sliceKey == nil)
        #expect(diversity.phaseDeg == nil && diversity.gainDb == nil)
    }

    @Test("Intermediate slice deltas cannot replace the complete Diversity owner or its blend")
    func diversitySummaryIgnoresIntermediateSliceDeltas() throws {
        let store = diversitySummaryStore(live: 1)
        addDiversitySlice(store, id: 0, active: true, enabled: false, phase: 11, gain: 1)
        addDiversitySlice(store, id: 1, active: false, enabled: true, phase: 77, gain: 7)
        let defaults = UserDefaults(suiteName: "diversity-summary-intermediate")!
        defer { defaults.removePersistentDomain(forName: "diversity-summary-intermediate") }
        let diversity = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults))
        #expect(diversity.sliceKey == "slice:1" && diversity.phaseDeg == 123.4)
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 29, name: "diversityEnabled", value: .bool(true)),
            .init(ordinal: 30, name: "diversityPhaseDeg", value: .f64(99)),
        ])))
        store.apply(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 29, name: "diversityEnabled", value: .bool(false)),
        ])))
        diversity.refresh()
        #expect(diversity.sliceKey == "slice:1" && diversity.enabled == true)
        #expect(diversity.phaseDeg == 123.4 && diversity.gainDb == -2)
        store.apply(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 37, name: "diversityState", value: .utf8(Self.diversitySummaryJSON(live: 0, revision: 2))),
        ])))
        diversity.refresh()
        #expect(diversity.sliceKey == "slice:0" && diversity.phaseDeg == 123.4)
        #expect(store.object("slice:1")?["diversityPhaseDeg"] == .double(77))
    }

    @Test("Legacy Diversity resolves stable id zero and never treats B as A")
    func diversityLegacyMissingA() throws {
        let store = diversitySummaryStore(live: nil, version: 0)
        addDiversitySlice(store, id: 1, active: false, enabled: false, phase: 31, gain: 3)
        let defaults = UserDefaults(suiteName: "diversity-legacy-missing-a")!
        defer { defaults.removePersistentDomain(forName: "diversity-legacy-missing-a") }
        let diversity = DiversityModel(mirror: store, phone: PhoneSettings(defaults: defaults))
        #expect(diversity.sliceKey == nil)
        #expect(diversity.reason == DiversityModel.noSliceReason)
    }

    /// Exact frozen radio property shape. Synthetic input stays local to this suite.
    static func diversitySummaryJSON(live: Int?, revision: Int = 1) -> String {
        let liveText = live.map { id in
            """
            {"sliceId":\(id),"incarnation":\(100 + id),"controlRevision":4,
             "controllerDeviceId":"phone","letter":"\(String(UnicodeScalar(65 + id)!))","band":5,
             "frequencyHz":14200000,"phaseDeg":123.4,"gainDb":-2,"fineNullEnabled":false,"pattern":null}
            """
        } ?? "null"
        return """
        {"version":1,"revision":\(revision),"requested":\(live != nil),"running":\(live != nil),"paused":false,
         "reasonCode":"","reason":"","live":\(liveText),
         "targets":[{"sliceId":0,"incarnation":100,"controlRevision":4,"controllerDeviceId":"phone",
                     "eligible":true,"reasonCode":"","reason":""},
                    {"sliceId":1,"incarnation":101,"controlRevision":4,"controllerDeviceId":"phone",
                     "eligible":true,"reasonCode":"","reason":""}]}
        """
    }

    private func diversitySummaryStore(live: Int?, version: Int64 = 1) -> MirrorStore {
        let store = MirrorStore(send: { _ in })
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        store.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "diversityControlVersion", value: .i64(version)),
        ])))
        store.apply(.schema(LinkMessage.Schema(className: "RadioModel", fields: [
            .init(ordinal: 37, name: "diversityState", kind: .utf8),
        ])))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 37, name: "diversityState", value: .utf8(Self.diversitySummaryJSON(live: live))),
        ])))
        store.apply(.snapshotComplete)
        return store
    }

    private func addDiversitySlice(_ store: MirrorStore, id: Int, active: Bool, enabled: Bool,
                                   phase: Double, gain: Double) {
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:\(id)", className: "SliceModel", properties: [
            .init(ordinal: 11, name: "active", value: .bool(active)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(Int64(id))),
            .init(ordinal: 14, name: "band", value: .enumeration(5)),
            .init(ordinal: 29, name: "diversityEnabled", value: .bool(enabled)),
            .init(ordinal: 30, name: "diversityPhaseDeg", value: .f64(phase)),
            .init(ordinal: 31, name: "diversityGainDb", value: .f64(gain)),
        ])))
    }

    // MARK: Diversity pattern

    @Test("the sensitivity pattern is greyed with its reason while the Core sends none, and the phase and gain stay shown")
    func diversityPatternNotSent() async throws {
        let (model, _) = try await connected()
        let defaults = try #require(UserDefaults(suiteName: "ToolsPagesTests-\(UUID().uuidString)"))
        let diversity = DiversityModel(mirror: model.mirror, phone: PhoneSettings(defaults: defaults))
        #expect(await settle { diversity.reason == nil && diversity.phaseDeg != nil && diversity.gainDb != nil })
        // The phone does not work the pattern out from the phase and gain (D4).
        #expect(diversity.sensitivity == nil)
        #expect(diversity.patternReason == "This Core does not send the diversity pattern. Updating the Core may help.")
        await model.disconnect()
        #expect(await settle { diversity.reason == DiversityModel.notConnectedReason })
        #expect(diversity.patternReason == DiversityModel.notConnectedReason)
    }

    @Test("slice A's pattern from the Core is drawn as sent, follows each new one, and an unreadable one says why")
    func diversityPatternFromTheCore() async throws {
        let (model, station) = try await connected(additions: [.diversityPattern])
        let defaults = try #require(UserDefaults(suiteName: "ToolsPagesTests-\(UUID().uuidString)"))
        let diversity = DiversityModel(mirror: model.mirror, phone: PhoneSettings(defaults: defaults))
        let suite = try #require(DiversityPattern(json: try FakeStation.suiteDiversityPattern()))
        #expect(await settle { diversity.reason == nil && diversity.sensitivity != nil })
        let drawn = try #require(diversity.sensitivity)
        // Fractions of the peak, as sent, at the Core's 3 degree steps from north.
        #expect(drawn.shares == suite.points && drawn.shares.count == 120 && drawn.stepDeg == 3)
        #expect(diversity.patternReason == nil)
        // A new pattern arrives with the change that moved it.
        var next = Array(repeating: 0.25, count: 120)
        next[30] = 1
        let json = "{\"crossFire\":false,\"points\":[" + next.map { String($0) }.joined(separator: ",")
            + "],\"spacingMeters\":5.5,\"stepDeg\":3}"
        try await station.deliverDiversityPattern(json)
        #expect(await settle { diversity.sensitivity?.strongestBearing == 90 })
        // A value that does not read as a pattern is not drawn, and the page says why.
        try await station.deliverDiversityPattern("{}")
        #expect(await settle { diversity.sensitivity == nil })
        #expect(diversity.patternReason == DiversityModel.patternUnreadableReason)
        // With the Core away the pattern is not shown as current.
        try await station.deliverDiversityPattern(json)
        #expect(await settle { diversity.sensitivity != nil })
        await model.disconnect()
        #expect(await settle { diversity.reason == DiversityModel.notConnectedReason && diversity.sensitivity == nil })
        #expect(diversity.patternReason == DiversityModel.notConnectedReason)
    }

    @Test("a pattern the Core sends is drawn at its own steps; the steering handle sits at the phase")
    func diversityPatternSteps() {
        var shares = Array(repeating: 0.0, count: 120)
        shares[30] = 1
        let pattern = DiversitySensitivity(shares: shares, stepDeg: 3)
        #expect(pattern.strongestBearing == 90)
        let points = pattern.points(centre: CGPoint(x: 100, y: 100), radius: 80)
        #expect(abs(points[30].x - 180) < 1e-9 && abs(points[30].y - 100) < 1e-9)
        let handle = DiversitySensitivity.handle(phaseDeg: 180, centre: CGPoint(x: 100, y: 100), radius: 80)
        #expect(abs(handle.x - 100) < 1e-9 && abs(handle.y - 180) < 1e-9)
    }

    @Test("a pattern the Core sends is drawn as it is: north up, clockwise, clamped to the plot")
    func diversityPatternDrawing() {
        let pattern = DiversitySensitivity(shares: [1, 0.5, 2, -1, .nan, 0.25, 0, 0.75])
        #expect(pattern.shares == [1, 0.5, 1, 0, 0, 0.25, 0, 0.75])
        #expect(pattern.strongestBearing == 0)
        let points = pattern.points(centre: CGPoint(x: 100, y: 100), radius: 80)
        #expect(points.count == 8)
        // North, a full share, sits straight up; east (index 2) straight right.
        #expect(abs(points[0].x - 100) < 1e-9 && abs(points[0].y - 20) < 1e-9)
        #expect(abs(points[2].x - 180) < 1e-9 && abs(points[2].y - 100) < 1e-9)
        #expect(DiversitySensitivity(shares: [0.2, 0.9, 0.1, 0.3]).strongestBearing == 90)
        #expect(DiversitySensitivity(shares: []).strongestBearing == nil)
    }

    // MARK: Helpers

    private func connected(additions: FakeStation.Additions = [], without: Set<String> = [],
                           phone: PhoneSettings? = nil) async throws -> (AppModel, FakeStation) {
        answered.removeAll()
        commandsAnswered.removeAll()
        let defaults = try #require(UserDefaults(suiteName: "ToolsPagesTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: phone ?? PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions, withoutCapabilities: without)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        return (model, station)
    }

    /// The suite's ANAN-G2 catalogue as a new revision, with its tools and board changed.
    private func deliverCatalogue(_ station: FakeStation, revision: Int64, board edits: [String: Any] = [:],
                                  fixture: String = "catalog-anan-g2",
                                  tools change: ([[String: Any]]) -> [[String: Any]] = { $0 }) async throws {
        var object = try #require(ModesTabBindingTests.catalogueObject(fixture))
        object["tools"] = change(try #require(object["tools"] as? [[String: Any]]))
        var board = try #require(object["board"] as? [String: Any])
        board.merge(edits) { $1 }
        object["board"] = board
        let data = try JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(String(decoding: data, as: UTF8.self))),
            .init(ordinal: 1, name: "revision", value: .i64(revision)),
        ])))
    }

    private func ordinal(_ className: String, _ property: String) throws -> UInt16 {
        try #require(try FakeStation.schema(ofClass: className).fields.first { $0.name == property }?.ordinal)
    }

    /// The writes and commands already answered, so each answer takes the next one.
    private final class Answered {
        var all: Set<UInt32> = []
        func removeAll() { all.removeAll() }
        func insert(_ id: UInt32) { all.insert(id) }
    }

    private let answered = Answered()
    private let commandsAnswered = Answered()

    /// Waits for the app's next write of `property` to `key`, answers it as
    /// the Core does (kept, or refused with `refuse`) and returns what was sent.
    @discardableResult
    private func answer(_ station: FakeStation, key: String, _ property: String,
                        refuse reason: String? = nil) async -> LinkMessage.PropertyValue? {
        let done = answered.all
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == key && write.properties.first?.name == property
                    && !done.contains(write.writeId ?? 0)
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId,
              let entry = write.properties.first else {
            Issue.record("no write of \(key).\(property) reached the Core")
            return nil
        }
        answered.insert(writeId)
        let value = LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: property, value: entry.value)
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: key, writeId: writeId, results: [
            .init(property: property, accepted: reason == nil, reason: reason ?? "", value: value),
        ])))
        if reason == nil {
            await station.deliver(.delta(LinkMessage.Delta(key: key, properties: [value])))
        }
        return entry.value
    }

    /// Waits for the app's next `verb`, answers it accepted and returns it.
    @discardableResult
    private func answerCommand(_ station: FakeStation, _ verb: String) async -> LinkMessage.CommandInvoke? {
        let done = commandsAnswered.all
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == verb && !done.contains(invoke.id)
            }
            return false
        }
        guard case .commandInvoke(let invoke)? = sent else {
            Issue.record("no \(verb) reached the Core")
            return nil
        }
        commandsAnswered.insert(invoke.id)
        await station.deliver(.commandResult(LinkMessage.CommandResult(verb: verb, id: invoke.id, accepted: true,
                                                                       reason: "", affected: [], values: nil)))
        return invoke
    }

    /// A Core fed by hand at agreed minor 11, with `capability` at
    /// `version` and one object: the mirror and the command client take
    /// only what the test gives them, so two changes can land in one turn.
    private func handFed(_ capability: String, version: Int64, key: String, className: String,
                         values: [LinkMessage.PropertyEntry]) async
        -> (MirrorStore, CommandClient, SliceListTests.Outbox) {
        let outbox = SliceListTests.Outbox()
        let store = MirrorStore(send: { message in outbox.record(message) })
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: capability, value: .i64(version)),
        ])))
        store.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: key, className: className, properties: values)))
        store.apply(.snapshotComplete)
        let commands = CommandClient(send: { message in outbox.record(message) })
        await commands.handle(.stateChanged(.ready))
        return (store, commands, outbox)
    }

    /// Answers the app's only `verb` accepted, by hand.
    private func accept(_ commands: CommandClient, _ outbox: SliceListTests.Outbox, _ verb: String) async -> Bool {
        guard let invoke = outbox.invokes(verb).last else {
            return false
        }
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: invoke.id, accepted: true, reason: "", affected: [], values: nil)))
        return true
    }

    /// The arguments of the first `verb` the app sent, or nil if none came.
    private func sent(_ station: FakeStation, _ verb: String) async -> [LinkMessage.PropertyEntry]? {
        await station.waitForMessage(within: .seconds(30)) { AccessoryPagesTests.invoke($0)?.verb == verb }
            .flatMap(AccessoryPagesTests.invoke)?.args
    }

    /// The message asks the Core for `stream`.
    static func asksFor(_ message: LinkMessage, _ stream: String) -> Bool {
        let invoke = AccessoryPagesTests.invoke(message)
        return invoke?.verb == "records.subscribe" && invoke?.args.first?.value == .utf8(stream)
    }

    /// Types `digits` on a number pad from an empty entry.
    private func type(_ pad: ValuePadModel, _ digits: String) {
        while !pad.entry.isEmpty {
            pad.press(.delete)
        }
        for digit in digits {
            pad.press(.digit(digit.wholeNumberValue ?? 0))
        }
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}
