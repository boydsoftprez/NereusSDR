// NereusSDR for iOS: the Modes tab against a Core: each radio's lists and ranges, and each control's write
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18, R-IOS-27, D15, D17: the Modes tab draws each radio's modes,
/// presets, attenuator range and antennas from the Core's catalogue, and
/// each control's change sends exactly the write, verb or setting its
/// owner expects, the control then showing what the Core kept. The Core is
/// `FakeStation`; its catalogues are the conformance suite's ANAN-G2 and
/// Hermes Lite 2 ones, read at run time and never bundled (D4). Its step
/// attenuator and Alex antennas are the objects a Core at
/// `radioHardwareVersion` 7 adds to its snapshot.
@Suite("Modes tab", .serialized)
@MainActor
struct ModesTabBindingTests {
    /// The writes and commands already answered, so each answer finds the next.
    final class Answered: @unchecked Sendable {
        private let lock = NSLock()
        private var ids: Set<UInt32> = []

        var all: Set<UInt32> { lock.withLock { ids } }

        func insert(_ id: UInt32) {
            _ = lock.withLock { ids.insert(id) }
        }

        /// A new session numbers its writes and commands from the start again.
        func removeAll() {
            lock.withLock { ids.removeAll() }
        }
    }

    private let answered = Answered()
    private let commandsAnswered = Answered()

    nonisolated static let anan = "catalog-anan-g2"
    nonisolated static let hermesLite = "catalog-hermes-lite-2"

    @Test("High SWR needs a known limit and a valid fresh transmit reading, and its toast lasts only on onset")
    func swrWarningAndToastLifetime() throws {
        for reading in [nil, 0, 0.5, -400, Double.nan, Double.infinity, 1.5, 2] as [Double?] {
            #expect(TxSwrWarning.evaluate(reading, limit: 2) == nil)
        }
        #expect(TxSwrWarning.evaluate(3.2, limit: nil) == nil)
        #expect(TxSwrWarning.evaluate(3.2, limit: .nan) == nil)
        #expect(TxSwrWarning.evaluate(3.2, limit: 0) == nil)
        let warning = try #require(TxSwrWarning.evaluate(3.2, limit: 2))
        #expect(warning.reading == 3.2 && warning.limit == 2)
        var state = TxSwrWarningState()
        state.update(warning, nowMilliseconds: 100)
        #expect(state.toastVisible(nowMilliseconds: 100))
        #expect(state.toastVisible(nowMilliseconds: 3_099))
        #expect(!state.toastVisible(nowMilliseconds: 3_100))
        state.update(warning, nowMilliseconds: 4_000)
        #expect(!state.toastVisible(nowMilliseconds: 4_000), "continued readings never restart the onset toast")
        state.update(nil, nowMilliseconds: 4_100)
        #expect(!state.toastVisible(nowMilliseconds: 4_100))
        state.update(warning, nowMilliseconds: 4_200)
        #expect(state.toastVisible(nowMilliseconds: 4_200), "a new onset starts a new toast")
        state.update(nil, nowMilliseconds: 4_300)
        #expect(!state.toastVisible(nowMilliseconds: 4_300), "an absent or expired sample clears the toast immediately")
    }

    @Test("a High SWR warning uses the current Core limit and disappears when the settings session becomes stale")
    func highSwrUsesCurrentSettings() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        model.settings.apply(.settingsValue(.init(key: "SwrProtectionLimit", origin: "", properties: [
            .init(name: "SwrProtectionLimit", value: .utf8("2.0")),
        ])))
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 11, name: "swr", value: .f64(3.2)),
        ])))
        model.main.transmit.refresh()
        try #require(model.main.transmit.highSwr == TxSwrWarning(reading: 3.2, limit: 2))
        model.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(model.main.transmit.highSwr == nil, "an earlier settings session cannot supply the limit")
        await model.disconnect()
    }

    @Test("anti-VOX mirrors real Run, Gain and Time fields and sends exact bounded writes")
    func antiVoxMirrorAndWire() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let transmit = model.main.transmit
        model.mirror.apply(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 13, name: "antiVoxRun", value: .bool(false)),
            .init(ordinal: 12, name: "antiVoxTauMs", value: .i64(20)),
            .init(ordinal: 76, name: "antiVoxGainDb", value: .i64(-12)),
        ])))
        transmit.refresh()
        try #require(transmit.antiVoxRun == false)
        try #require(transmit.antiVoxTauMs == 20)
        try #require(transmit.antiVoxGainDb == -12)
        transmit.toggleAntiVox()
        #expect(await answer(station, key: "transmit", "antiVoxRun") == .bool(true))
        transmit.setAntiVoxGain(100)
        #expect(await answer(station, key: "transmit", "antiVoxGainDb") == .i64(60))
        transmit.setAntiVoxTime(0)
        #expect(await answer(station, key: "transmit", "antiVoxTauMs") == .i64(1))
        await TransmitScreenTests.barrier(model, station)
        transmit.refresh()
        #expect(transmit.antiVoxRun == true && transmit.antiVoxGainDb == 60 && transmit.antiVoxTauMs == 1)
        #expect(model.main.modes.transmit === transmit, "Modes and TX keep one model")
        #expect(!station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb.hasPrefix("tx.") })
        await model.disconnect()
    }

    @Test("anti-VOX Run and Time remain available without Gain; v13 permits settings on air independently of TX ownership",
          arguments: [Int64(0), 1, 4, 5, 12, 13])
    func antiVoxIndependentGates(_ version: Int64) async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let transmit = model.main.transmit
        var capabilities = model.mirror.capabilities
        capabilities["transmitSettingsVersion"] = .int(version)
        capabilities["txPermitted"] = .bool(false)
        model.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            .init(name: $0, value: capabilities[$0]!.wireValue)
        })))
        model.mirror.apply(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 13, name: "antiVoxRun", value: .bool(false)),
            .init(ordinal: 12, name: "antiVoxTauMs", value: .i64(20)),
            .init(ordinal: 76, name: "antiVoxGainDb", value: .i64(-12)),
        ])))
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties:
            TransmitScreenTests.holder(TransmitScreenTests.otherDeviceId, short: "MacBook", keyed: false))))
        transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        transmit.refresh()
        #expect(transmit.report.heldElsewhere)
        let before = station.messages.count
        transmit.toggleAntiVox()
        transmit.setAntiVoxTime(250)
        transmit.setAntiVoxGain(-20)
        if version >= 1 {
            #expect(await answer(station, key: "transmit", "antiVoxRun") == .bool(true))
            #expect(await answer(station, key: "transmit", "antiVoxTauMs") == .i64(250))
        }
        if version >= 5 { #expect(await answer(station, key: "transmit", "antiVoxGainDb") == .i64(-20)) }
        await TransmitScreenTests.barrier(model, station)
        let offAir = station.messages.dropFirst(before).compactMap { message -> String? in
            if case .propertyWrite(let write) = message { return write.properties.first?.name }
            return nil
        }
        #expect(Set(offAir) == (version >= 5 ? Set(["antiVoxRun", "antiVoxTauMs", "antiVoxGainDb"])
                               : version >= 1 ? Set(["antiVoxRun", "antiVoxTauMs"]) : []))
        model.mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        transmit.refresh()
        let onAirStart = station.messages.count
        transmit.setAntiVoxTime(300)
        if version >= 13 { #expect(await answer(station, key: "transmit", "antiVoxTauMs") == .i64(300)) }
        await TransmitScreenTests.barrier(model, station)
        let writes = station.messages.dropFirst(onAirStart).filter { if case .propertyWrite = $0 { return true }; return false }
        #expect(writes.count == (version >= 13 ? 1 : 0))
        #expect(!transmit.permitted && !transmit.ptt.voxArmed)
        await model.disconnect()
    }

    @Test("a gain-only mirror does not invent Run or Time, and missing settings send nothing")
    func antiVoxMissingFields() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        model.mirror.apply(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
            .init(ordinal: 76, name: "antiVoxGainDb", value: .i64(12)),
        ])))
        model.main.transmit.refresh()
        try #require(model.main.transmit.antiVoxGainDb == 12)
        #expect(model.main.transmit.antiVoxRun == nil && model.main.transmit.antiVoxTauMs == nil)
        let before = station.messages.count
        model.main.transmit.toggleAntiVox()
        model.main.transmit.setAntiVoxTime(20)
        await TransmitScreenTests.barrier(model, station)
        #expect(!station.messages.dropFirst(before).contains { if case .propertyWrite = $0 { return true }; return false })
        model.main.transmit.setAntiVoxGain(15)
        #expect(await answer(station, key: "transmit", "antiVoxGainDb") == .i64(15))
        await model.disconnect()
    }

    @Test("monitor volume uses the Core's fractional mirrored value and bounded floating-point write")
    func monitorVolumeBinding() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        model.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(ordinal: 19, name: "monitorVolume", value: .f64(0.42)),
        ])))
        model.main.transmit.refresh()
        try #require(model.main.transmit.monitorVolume == 0.42)
        model.main.transmit.setMonitorVolume(0.75)
        #expect(await answer(station, key: "transmit", "monitorVolume") == .f64(0.75))
        model.main.transmit.setMonitorVolume(2)
        #expect(await answer(station, key: "transmit", "monitorVolume") == .f64(1))
        await model.disconnect()
    }

    @Test("Settings and Back focus Transmit and preserve the panel without arming or keying")
    func transmitSettingsNavigation() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let router = TxSettingsRouter()
        var destinations: [String] = []
        router.selectModes = { destinations.append("Modes") }
        router.selectPanadapter = { destinations.append("Panadapter") }
        router.openTransmit()
        #expect(router.returnsToPanel && router.focusSerial == 1)
        #expect(destinations == ["Modes"])
        router.backToPanel()
        #expect(!router.returnsToPanel)
        #expect(destinations == ["Modes", "Panadapter"])
        await TransmitScreenTests.barrier(model, station)
        #expect(!model.main.transmit.vox && !model.main.transmit.ptt.voxArmed && !station.keyed)
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.properties.contains { $0.name == "voxEnabled" } }
            return TransmitScreenTests.invoke(message)?.verb.hasPrefix("tx.") == true
        })
        await model.disconnect()
    }

    // MARK: Each radio from its own catalogue

    @Test("an ANAN-G2 and a Hermes Lite 2 each show their own modes, presets, attenuator range and antennas",
          arguments: [anan, hermesLite])
    func eachRadioFromItsCatalogue(_ fixture: String) async throws {
        let (model, _) = try await connected(catalogue: fixture, frontEnd: true)
        let modes = model.main.modes
        let catalog = try #require(model.main.catalogFeed.catalog)
        let wire = try #require(Self.catalogueObject(fixture))
        let board = try #require(wire["board"] as? [String: Any])

        // The modes, in the Core's order, the slice's USB lit.
        #expect(modes.modes.map(\.label) == catalog.modes.map(\.label))
        #expect(modes.modes.count == 14)
        #expect(modes.modes.filter(\.lit).map(\.label) == ["USB"])
        #expect(modes.modeLabel == "USB")
        // USB's presets, as the Core lists them.
        let usb = try #require((wire["filterPresets"] as? [String: [[String: Any]]])?["USB"])
        #expect(modes.rx.presets.map(\.name) == usb.map { $0["label"] as? String ?? "" })
        #expect(modes.filterLowHz == 100 && modes.filterHighHz == 3000)

        // The front end, from the board the catalogue names.
        let attenuator = try #require(board["attenuator"] as? [String: Double])
        #expect(modes.attenuatorRange == StationCatalog.Range(min: attenuator["min"] ?? .nan,
                                                             max: attenuator["max"] ?? .nan,
                                                             step: attenuator["step"] ?? .nan))
        #expect(modes.attenuatorReason == nil)
        #expect(modes.preamp.map(\.label)
            == (board["preampItems"] as? [[String: Any]] ?? []).map { $0["label"] as? String ?? "" })
        // The Core's step attenuator is on (S-ATT): the preamp choices wait for ATT.
        #expect(modes.attenuatorWay == .stepAtt)
        #expect(modes.preampReason == ModesTabModel.preampWithStepAttText)
        #expect(modes.rxAntennas.map(\.label) == board["rxAntennas"] as? [String])
        #expect(modes.rxOnlyInputs.map(\.label) == board["rxOnlyInputs"] as? [String])
        #expect(modes.txAntennas.map(\.label) == board["txAntennas"] as? [String])
        #expect(modes.rxAntennas.filter(\.lit).map(\.label) == ["ANT1"])
        #expect(modes.txAntennas.filter(\.lit).map(\.label) == ["ANT1"])
        #expect(modes.rxAntennaReason == nil && modes.txAntennaReason == nil)

        // The radios differ where the suite's catalogues say they do.
        if fixture == Self.anan {
            #expect(modes.attenuatorRange?.min == 0 && modes.attenuatorRange?.max == 31)
            #expect(modes.rxOnlyInputs.count == 3)
        } else {
            #expect(modes.attenuatorRange?.min == -28 && modes.attenuatorRange?.max == 31)
            #expect(modes.rxAntennas.count == 1 && modes.rxOnlyInputs.isEmpty && modes.txAntennas.count == 1)
        }
        // AGC-T over the catalogue's range, the slice's values.
        #expect(modes.agcThresholdRange == catalog.agc.thresholdDb)
        #expect(modes.agcThreshold == -20 && modes.autoAgc == false)
        #expect(modes.stepHz == 100 && modes.ritHz == 0 && modes.xitHz == 0)
        await model.disconnect()
    }

    @Test("the step attenuator's arrows stay within the Core's range for each radio")
    func attenuatorArrowsStayInRange() async throws {
        for (fixture, low, high) in [(Self.anan, 0.0, 31.0), (Self.hermesLite, -28.0, 31.0)] {
            let (model, station) = try await connected(catalogue: fixture, frontEnd: true)
            let modes = model.main.modes
            // At the bottom, less attenuation sends nothing.
            await deliver(station, key: "stepAtt", property: "attenuationDb", ordinal: 1, value: .i64(Int64(low)))
            #expect(await settle { modes.attenuationDb == Int64(low) })
            let before = station.messages.count
            modes.stepAttenuator(up: false)
            await idle()
            #expect(station.messages.count == before, "\(fixture)")
            // One step up is one write of the next value to stepAtt.
            modes.stepAttenuator(up: true)
            let sent = await answer(station, key: "stepAtt", "attenuationDb")
            #expect(sent == .i64(Int64(low) + 1), "\(fixture)")
            #expect(await settle { modes.attenuationDb == Int64(low) + 1 })
            // At the top, more attenuation sends nothing.
            await deliver(station, key: "stepAtt", property: "attenuationDb", ordinal: 1, value: .i64(Int64(high)))
            #expect(await settle { modes.attenuationDb == Int64(high) })
            let atTop = station.messages.count
            modes.stepAttenuator(up: true)
            await idle()
            #expect(station.messages.count == atTop, "\(fixture)")
            await model.disconnect()
        }
    }

    // MARK: Each control's write

    @Test("the slice switch activates the other slice by the Core's verb")
    func sliceSwitch() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        await station.deliver(BandFlagShotTests.slice(1, active: false))
        let modes = model.main.modes
        #expect(await settle { modes.sliceChoices.count == 2 })
        #expect(modes.sliceChoices.map(\.letter) == ["A", "B"])
        #expect(modes.sliceChoices.filter(\.active).map(\.letter) == ["A"])
        modes.selectSlice(1)
        let invoke = await answerCommand(station, "setActiveSliceById")
        #expect(invoke?.args.map(\.name) == ["sliceId"])
        #expect(invoke?.args.first?.value == .i64(1))
        await model.disconnect()
    }

    @Test("mode, AGC-T, AUTO, pan, MUTE, BIN, RIT and XIT each write their one slice property")
    func sliceWrites() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes

        modes.selectMode(0)
        #expect(await answer(station, key: "slice:0", "dspMode") == .enumeration(0))
        #expect(await settle { modes.modes.first(where: \.lit)?.label == "LSB" && modes.modeLabel == "LSB" })

        modes.setAgcThreshold(-45.4)
        #expect(await answer(station, key: "slice:0", "agcThreshold") == .i64(-45))
        #expect(await settle { modes.agcThreshold == -45 })

        modes.toggleAutoAgc()
        #expect(await answer(station, key: "slice:0", "autoAgcEnabled") == .bool(true))
        #expect(await settle { modes.autoAgc == true })

        modes.setPan(-0.5)
        #expect(await answer(station, key: "slice:0", "audioPan") == .f64(-0.5))
        #expect(await settle { modes.pan == -0.5 })
        #expect(AudioSection.panText(-0.5) == "L50" && AudioSection.panText(0) == "C" && AudioSection.panText(1) == "R100")

        modes.toggleMute()
        #expect(await answer(station, key: "slice:0", "muted") == .bool(true))
        #expect(await settle { modes.muted == true })

        modes.toggleBinaural()
        #expect(await answer(station, key: "slice:0", "binauralEnabled") == .bool(true))

        modes.toggleRit()
        #expect(await answer(station, key: "slice:0", "ritEnabled") == .bool(true))
        #expect(await settle { modes.ritOn == true })
        // The arrows move by the slice's own step (100 Hz here).
        modes.stepRit(up: true)
        #expect(await answer(station, key: "slice:0", "ritHz") == .i64(100))
        #expect(await settle { modes.ritHz == 100 })

        modes.toggleXit()
        #expect(await answer(station, key: "slice:0", "xitEnabled") == .bool(true))
        modes.stepXit(up: false)
        #expect(await answer(station, key: "slice:0", "xitHz") == .i64(-100))
        #expect(await settle { modes.xitHz == -100 })
        await model.disconnect()
    }

    @Test("APF switches in every mode as on the desktop flag; its tune moves in CW with APF on, -500 to 500 Hz (M2, I8)")
    func apfInEveryModeAndItsTune() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        // USB: APF switches, its tune is greyed with the reason and sends nothing.
        #expect(modes.apfReason == nil)
        #expect(modes.apfTuneReason == ModesTabModel.apfTuneText)
        modes.toggleApf()
        #expect(await answer(station, key: "slice:0", "apfEnabled") == .bool(true))
        #expect(await settle { modes.apf == true })
        let before = station.messages.count
        modes.setApfTune(100)
        await idle()
        #expect(station.messages.count == before)
        // CWU with APF on: the tune moves, within the flag's range.
        let cwu = try #require(model.main.catalogFeed.catalog?.modes.first { $0.label == "CWU" })
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(cwu.id)))
        #expect(await settle { modes.apfTuneReason == nil })
        modes.setApfTune(120.4)
        #expect(await answer(station, key: "slice:0", "apfTuneHz") == .i64(120))
        #expect(await settle { modes.apfTuneHz == 120 })
        modes.setApfTune(900)
        #expect(await answer(station, key: "slice:0", "apfTuneHz") == .i64(500))
        await model.disconnect()
    }

    /// The Core's ten preamp modes (5cdb9a1ab): -10, -20 and -30 dB are the
    /// step attenuator's modes 7, 8 and 9 on these boards. The buttons are
    /// the catalogue's ids and labels, as sent, and a choice writes its id;
    /// the app keeps no preamp table of its own.
    @Test("the preamp buttons are the catalogue's, -10, -20 and -30 dB its ids 7, 8 and 9", arguments: [anan, hermesLite])
    func preampIdsFromTheCatalogue(_ fixture: String) async throws {
        let (model, station) = try await connected(catalogue: fixture, frontEnd: true)
        let modes = model.main.modes
        // The fixture's own catalogue, as the Core sends it.
        let wire = try #require(Self.catalogueObject(fixture))
        let items = try #require((wire["board"] as? [String: Any])?["preampItems"] as? [[String: Any]])
        #expect(modes.preamp.map(\.id) == items.map { ($0["id"] as? NSNumber)?.intValue ?? -1 })
        #expect(modes.preamp.map(\.label) == items.map { $0["label"] as? String ?? "" })
        #expect(modes.preamp.map(\.id) == [1, 7, 8, 9])
        #expect(modes.preamp.map(\.label) == ["0dB", "-10dB", "-20dB", "-30dB"])
        await deliver(station, key: "stepAtt", property: "enabled", ordinal: 0, value: .bool(false))
        #expect(await settle { modes.attenuatorWay == .att && modes.preampReason == nil })
        for (id, label) in [(7, "-10dB"), (8, "-20dB"), (9, "-30dB")] {
            modes.selectPreamp(id)
            #expect(await answer(station, key: "stepAtt", "preampMode") == .i64(Int64(id)))
            #expect(await settle { modes.preamp.first(where: \.lit)?.label == label })
        }
        await model.disconnect()
    }

    @Test("the preamp and step attenuator write the Core's stepAtt, the RX antenna the slice")
    func frontEndWrites() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true)
        let modes = model.main.modes
        // The preamp choices apply in ATT, with the step attenuator off.
        #expect(modes.attenuatorWay == .stepAtt && modes.preampReason == ModesTabModel.preampWithStepAttText)
        await deliver(station, key: "stepAtt", property: "enabled", ordinal: 0, value: .bool(false))
        #expect(await settle { modes.attenuatorWay == .att && modes.preampReason == nil })
        #expect(modes.attenuatorReason == ModesTabModel.attOffText)
        let minus20 = try #require(modes.preamp.first { $0.label == "-20dB" })
        modes.selectPreamp(minus20.id)
        #expect(await answer(station, key: "stepAtt", "preampMode") == .i64(Int64(minus20.id)))
        #expect(await settle { modes.preamp.first(where: \.lit)?.label == "-20dB" })

        let ext1 = try #require(modes.rxOnlyInputs.first { $0.label == "EXT1" })
        modes.selectRxAntenna(ext1)
        #expect(await answer(station, key: "slice:0", "rxAntenna") == .utf8("EXT1"))
        #expect(await settle { modes.rxOnlyInputs.first(where: \.lit)?.label == "EXT1" })
        #expect(modes.rxAntennas.allSatisfy { !$0.lit })

        // The lit antenna sends nothing again.
        let before = station.messages.count
        if let lit = modes.rxOnlyInputs.first(where: \.lit) {
            modes.selectRxAntenna(lit)
        }
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()
    }

    @Test("a TX antenna goes by setAlexTxAntenna for the slice's band on a Core that takes it")
    func txAntennaByTheCoresVerb() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true)
        let modes = model.main.modes
        #expect(modes.txAntennaByBand)
        let ant2 = try #require(modes.txAntennas.first { $0.label == "ANT2" })
        modes.selectTxAntenna(ant2)
        // The slice's band (20 m, band 5) and the antenna's number.
        let invoke = await answerCommand(station, ModesTabModel.txAntennaVerb)
        #expect(invoke?.args.map(\.name) == ["band", "antenna"])
        #expect(invoke?.args.map(\.value) == [.i64(5), .i64(2)])
        // No slice write goes with it.
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message {
                return write.properties.contains { $0.name == "txAntenna" }
            }
            return false
        })
        // The Core's refusal shows in its words.
        let ant3 = try #require(modes.txAntennas.first { $0.label == "ANT3" })
        modes.selectTxAntenna(ant3)
        await answerCommand(station, ModesTabModel.txAntennaVerb,
                            refuse: "An antenna blocked for transmit cannot be a band's TX antenna.")
        #expect(await settle { modes.note == "An antenna blocked for transmit cannot be a band's TX antenna." })
        await model.disconnect()
    }

    @Test("a TX antenna, TX profile and PS-A light at the touch while the Core has not answered; refused, the Core's light returns")
    func commandControlsActAtTheTouch() async throws {
        // JJ, 2026-10-01: every control acts at the touch (StationClient.cpp:1040-1068).
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true, additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(modes.txAntennaByBand)
        let lit = try #require(modes.txAntennas.first(where: \.lit)?.label)
        let other = try #require(modes.txAntennas.first { !$0.lit })
        modes.selectTxAntenna(other)
        #expect(await settle { modes.txAntennas.first(where: \.lit)?.label == other.label })
        await answerCommand(station, ModesTabModel.txAntennaVerb, refuse: "Not that one.")
        #expect(await settle { modes.note == "Not that one." })
        #expect(await settle { modes.txAntennas.first(where: \.lit)?.label == lit })

        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 37, name: "activeTxProfile", value: .utf8("Default")),
            .init(ordinal: 38, name: "txProfilesJson", value: .utf8("[\"Default\",\"DX\"]")),
        ])))
        #expect(await settle { transmit.activeProfile == "Default" })
        transmit.selectProfile("DX")
        #expect(await settle { transmit.activeProfile == "DX" })
        await answerCommand(station, TransmitModel.txProfileVerb, refuse: "No such profile.")
        #expect(await settle { transmit.activeProfile == "Default" && transmit.note == "No such profile." })
        // A change the Core holds for this phone's question is not refused:
        // the touched profile stays lit and no new refusal note shows.
        transmit.selectProfile("DX")
        #expect(await settle { transmit.activeProfile == "DX" })
        await answerCommand(station, TransmitModel.txProfileVerb, refuse: SeveralDevices.waitingReason)
        await idle()
        #expect(transmit.activeProfile == "DX")
        #expect(transmit.note == "No such profile.")

        #expect(await settle { transmit.psaReason == nil && !transmit.psa })
        transmit.togglePsa()
        #expect(await settle { transmit.psa })
        _ = await answerCommand(station, TransmitModel.psaOnVerb)
        await idle()
        #expect(transmit.psa)
        await model.disconnect()
    }

    @Test("a TX profile held for this phone's question stays shown and creates no refusal note")
    func heldProfileCreatesNoRefusalNote() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true, additions: [.remoteTx])
        let transmit = model.main.transmit
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 37, name: "activeTxProfile", value: .utf8("Default")),
            .init(ordinal: 38, name: "txProfilesJson", value: .utf8("[\"Default\",\"DX\"]")),
        ])))
        #expect(await settle { transmit.activeProfile == "Default" })
        #expect(transmit.note == nil)
        transmit.selectProfile("DX")
        #expect(await settle { transmit.activeProfile == "DX" })
        await answerCommand(station, TransmitModel.txProfileVerb, refuse: SeveralDevices.waitingReason)
        await idle()
        #expect(transmit.activeProfile == "DX")
        #expect(transmit.note == nil)
        await model.disconnect()
    }

    @Test("with radio-bound antenna rows the TX antenna goes by setAlexTxAntennaForRadio with the radio's address (R1)")
    func txAntennaForTheConnectedRadio() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true)
        let modes = model.main.modes
        // A Core that declares radio-bound antenna rows, without saying
        // which radio is connected: the desktop's words, and nothing sent.
        await Self.deliverCapabilities(station, model: model, ["radioAntennaRowsVersion": .int(1)])
        #expect(await settle { model.mirror.capabilityVersion("radioAntennaRowsVersion") == 1 })
        let ant2 = try #require(modes.txAntennas.first { $0.label == "ANT2" })
        modes.selectTxAntenna(ant2)
        #expect(modes.note == "The Core's connected radio is not ready for this antenna change.")
        await idle()
        #expect(!station.messages.contains { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb.hasPrefix("setAlexTxAntenna")
            }
            return false
        })
        // Now it names the radio: the change carries its address first, as
        // the desktop's StationClient::requestAlexTxAntenna sends it.
        let mac = "AA:BB:CC:DD:EE:01"
        await Self.deliverCapabilities(station, model: model, ["macAddress": .text(mac)])
        #expect(await settle { model.mirror.capabilities["macAddress"] == .text(mac) })
        modes.selectTxAntenna(ant2)
        let invoke = await answerCommand(station, "setAlexTxAntennaForRadio")
        #expect(invoke?.args.map(\.name) == ["mac", "band", "antenna"])
        #expect(invoke?.args.map(\.value) == [.utf8(mac), .i64(5), .i64(2)])
        #expect(!station.messages.contains { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == ModesTabModel.txAntennaVerb
            }
            return false
        })
        await model.disconnect()
    }

    @Test("a change the Core holds for this phone's question leaves no note on the tab and closes its pad (R2)")
    func heldForQuestionLeavesNoNote() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        await deliver(station, key: "slice:0", property: "ritHz", ordinal: 55, value: .i64(0))
        #expect(await settle { modes.ritHz == 0 })
        modes.openRitPad()
        let pad = try #require(modes.pad)
        type(pad, "120")
        async let kept = pad.enter()
        await answer(station, key: "slice:0", "ritHz", keep: .i64(0), refuse: SeveralDevices.waitingReason)
        #expect(await kept == false)
        #expect(modes.note == nil)
        // The question sheet asks; the pad does not stay open with the typed value.
        #expect(modes.pad == nil)
        // Held, not refused: the touched value stays until the Core's next
        // value of it (the liveui rule, StationClient.cpp:1040-1068).
        await idle()
        #expect(modes.ritHz == 120)
        await deliver(station, key: "slice:0", property: "ritHz", ordinal: 55, value: .i64(120))
        await deliver(station, key: "slice:0", property: "ritHz", ordinal: 55, value: .i64(40))
        #expect(await settle { modes.ritHz == 40 })
        await model.disconnect()
    }

    @Test("a late answer to a RIT pad closed meanwhile leaves the XIT pad opened after it (R3)")
    func staleRitAnswerLeavesTheNewerPad() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        await deliver(station, key: "slice:0", property: "ritHz", ordinal: 55, value: .i64(0))
        #expect(await settle { modes.ritHz == 0 && modes.xitHz != nil })
        modes.openRitPad()
        let rit = try #require(modes.pad)
        type(rit, "300")
        async let kept = rit.enter()
        // Establish the admitted RIT write before replacing its presentation
        // owner; an async-let child need not have entered on this actor yet.
        let admitted = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.first?.name == "ritHz"
            }
            return false
        }
        #expect(admitted != nil)
        // The operator opens XIT before the Core has answered RIT.
        modes.openXitPad()
        let xit = try #require(modes.pad)
        #expect(xit.title == "XIT offset")
        #expect(await answer(station, key: "slice:0", "ritHz") == .i64(300))
        #expect(await kept)
        #expect(modes.pad === xit)
        await model.disconnect()
    }

    @Test("on a Core before setAlexTxAntenna, a TX antenna is the slice's txAntenna")
    func txAntennaOnAnOlderCore() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 5)
        let modes = model.main.modes
        #expect(!modes.txAntennaByBand)
        let ant3 = try #require(modes.txAntennas.first { $0.label == "ANT3" })
        modes.selectTxAntenna(ant3)
        #expect(await answer(station, key: "slice:0", "txAntenna") == .utf8("ANT3"))
        #expect(await settle { modes.txAntennas.first(where: \.lit)?.label == "ANT3" })
        #expect(!station.messages.contains { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == ModesTabModel.txAntennaVerb
            }
            return false
        })
        await model.disconnect()
    }

    @Test("without the Core's step attenuator the preamp and step att are greyed with the reason and send nothing")
    func frontEndGreyedWithoutStepAtt() async throws {
        // The suite's Core sends radioHardwareVersion 0: no attenuator ready.
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        #expect(await settle { modes.preampReason == ModesTabModel.frontEndNotReadyText })
        #expect(modes.attenuatorReason == ModesTabModel.frontEndNotReadyText)
        #expect(!modes.preamp.isEmpty)
        let before = station.messages.count
        modes.selectPreamp(modes.preamp[0].id)
        modes.stepAttenuator(up: true)
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()

        // A Core that does not know the capability needs updating.
        let (older, _) = try await connected(catalogue: Self.anan, frontEnd: false,
                                             withoutCapabilities: ["radioHardwareVersion"])
        #expect(await settle { older.main.modes.preampReason == CatalogFeed.needsNewerCoreText })
        #expect(older.main.modes.attenuatorReason == CatalogFeed.needsNewerCoreText)
        await older.disconnect()
    }

    @Test("the transmit settings write the Core's transmit object while this phone may transmit")
    func transmitWrites() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let modes = model.main.modes
        #expect(await settle { model.main.transmit.settingsEditable })
        #expect(modes.txFilterLowHz == 100 && modes.txFilterHighHz == 2900)
        #expect(modes.micGainDb == -6 && modes.procLevelDb == 2)
        #expect(modes.leveler == true && modes.eq == false && modes.cfc == false)

        modes.setMicGain(-12.2)
        #expect(await answer(station, key: "transmit", "micGainDb") == .i64(-12))
        #expect(await settle { modes.micGainDb == -12 })
        modes.setProcLevel(7)
        #expect(await answer(station, key: "transmit", "cpdrLevelDb") == .i64(7))
        modes.toggleLeveler()
        #expect(await answer(station, key: "transmit", "txLevelerOn") == .bool(false))
        #expect(await settle { modes.leveler == false })
        modes.toggleEq()
        #expect(await answer(station, key: "transmit", "txEqEnabled") == .bool(true))
        modes.toggleCfc()
        #expect(await answer(station, key: "transmit", "cfcEnabled") == .bool(true))
        #expect(await settle { modes.cfc == true })
        // PROC is the TX panel's own switch.
        model.main.transmit.toggleProc()
        #expect(await answer(station, key: "transmit", "cpdrOn") == .bool(true))
        await model.disconnect()
    }

    @Test("without transmit permission the transmit settings still change, as on the desktop (I1)")
    func transmitSettingsWithoutPermission() async throws {
        // The suite's Core is receive only for this phone, at transmitSettingsVersion 8.
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await settle { transmit.settingsEditable })
        #expect(!transmit.permitted)
        #expect(transmit.settingsReason(1) == nil && transmit.settingsReason(5) == nil)
        modes.setMicGain(3)
        #expect(await answer(station, key: "transmit", "micGainDb") == .i64(3))
        modes.toggleEq()
        #expect(await answer(station, key: "transmit", "txEqEnabled") == .bool(true))
        transmit.toggleProc()
        #expect(await answer(station, key: "transmit", "cpdrOn") == .bool(true))
        transmit.setVoxThreshold(-55.3)
        #expect(await answer(station, key: "transmit", "voxThresholdDb") == .i64(-55))
        transmit.setVoxHang(3000)
        #expect(await answer(station, key: "transmit", "voxHangTimeMs") == .i64(2000))
        transmit.toggleDexp()
        #expect(await answer(station, key: "transmit", "dexpEnabled") == .bool(true))
        transmit.setAmCarrier(40)
        #expect(await answer(station, key: "transmit", "amCarrierLevel") == .i64(40))
        // VOX keys, so it follows remote transmit, which this Core does not offer.
        #expect(TxPanel.voxReason(transmit) == TransmitModel.noRemoteTransmitText)
        await model.disconnect()
    }

    /// Below transmitSettingsVersion 13 (the desktop's
    /// kTransmitSettingsOnAirVersion) the Core refuses the transmit settings
    /// on the air, so they wait; at 13 and above they move then
    /// (TxMicGainTests.movesOnTheAirAtVersion13). The fixture's Core is at
    /// 15, so this Core says 12.
    @Test("below transmitSettingsVersion 13, while the Core's radio is on the air every transmit setting is greyed with the reason and sends nothing")
    func transmitSettingsWaitOnTheAir() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        let transmit = model.main.transmit
        await TxMicGainTests.takesOnAir(station, model, version: TxMicGainTests.onAirVersion - 1)
        #expect(await settle { transmit.settingsVersion == TxMicGainTests.onAirVersion - 1 })
        #expect(await settle { transmit.settingsEditable })
        for (key, property, ordinal) in [("radio", "transmitting", UInt16(19)), ("transmit", "tune", UInt16(1))] {
            await deliver(station, key: key, property: property, ordinal: ordinal, value: .bool(true))
            #expect(await settle { transmit.coreOnAir }, "\(property)")
            #expect(!transmit.settingsEditable)
            #expect(transmit.settingsReason(1) == TransmitModel.onAirText)
            #expect(transmit.settingsReason(2) == "The radio is on the air. Try again when it stops.")
            let before = station.messages.count
            modes.setMicGain(0)
            modes.setProcLevel(5)
            modes.toggleLeveler()
            modes.toggleEq()
            modes.toggleCfc()
            modes.matchRx()
            modes.openTxFilterPad(low: true)
            transmit.setRfPower(20)
            transmit.toggleProc()
            transmit.setVoxThreshold(-30)
            transmit.selectProfile("DX")
            await idle()
            #expect(station.messages.count == before, "\(property)")
            #expect(modes.pad == nil)
            await deliver(station, key: key, property: property, ordinal: ordinal, value: .bool(false))
            #expect(await settle { transmit.settingsEditable }, "\(property)")
        }
        await model.disconnect()
    }

    @Test("a Core without transmit settings greys them with the reason, version by version")
    func transmitSettingsByVersion() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false,
                                                   withoutCapabilities: ["transmitSettingsVersion"])
        let transmit = model.main.transmit
        #expect(await settle { model.mirror.isSnapshotComplete })
        #expect(!transmit.settingsEditable)
        #expect(transmit.settingsReason(1) == TransmitModel.settingsNotTakenText)
        let before = station.messages.count
        model.main.modes.setMicGain(0)
        transmit.setRfPower(10)
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()

        // At version 2 the chain changes; the profile (3) and power by band (5) wait.
        let (second, secondStation) = try await connected(catalogue: Self.anan, frontEnd: false)
        var capabilities = second.mirror.capabilities
        capabilities[TransmitModel.settingsCapability] = .int(2)
        await secondStation.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        let tx = second.main.transmit
        #expect(await settle { tx.settingsVersion == 2 })
        #expect(tx.settingsEditable(2) && !tx.settingsEditable(3) && !tx.settingsEditable(5))
        #expect(tx.settingsReason(3) == TransmitModel.settingsNotTakenText)
        tx.setRfPower(30)
        #expect(await answer(secondStation, key: "transmit", "power") == .i64(30))
        await idle()
        #expect(!secondStation.messages.contains { message in
            if case .propertyWrite(let write) = message {
                return write.properties.contains { $0.name == "powerByBandJson" || $0.name == "tuneDrivePowerSource" }
            }
            return false
        })
        await second.disconnect()
    }

    // MARK: Desktop parity, fix round C-a

    @Test("the slice's filter edges are typed in, in order and at least 10 Hz apart, on the Modes tab and RX panel (C2)")
    func typedReceiveFilterEdges() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        // USB, 100 to 3000 Hz. The low edge typed to 250 Hz.
        modes.openFilterEdgePad(low: true)
        let low = try #require(modes.pad)
        #expect(low.title == "Filter low edge" && low.entry == "100")
        type(low, "250")
        #expect(low.canEnter && low.enterLabel == "Set to 250 Hz")
        async let lowKept = low.enter()
        #expect(await answer(station, key: "slice:0", "filterLow") == .i64(250))
        #expect(await lowKept)
        #expect(await settle { modes.filterLowHz == 250 && modes.pad == nil })
        // A low edge above the high one is turned down before it is sent.
        modes.openFilterEdgePad(low: true)
        let crossed = try #require(modes.pad)
        type(crossed, "2995")
        #expect(crossed.problem == RxPanelModel.filterEdgeOrderText && !crossed.canEnter)
        let before = station.messages.count
        #expect(await crossed.enter() == false)
        #expect(station.messages.count == before)
        crossed.cancel()
        #expect(modes.pad == nil)
        // The RX panel's high edge, refused by the Core: its words, and the pad stays.
        let rx = model.main.rx
        rx.openFilterEdgePad(low: false)
        let high = try #require(rx.pad)
        type(high, "2700")
        async let highKept = high.enter()
        await answer(station, key: "slice:0", "filterHigh", keep: .i64(3000),
                     refuse: "The Core could not set that filter.")
        #expect(await highKept == false)
        #expect(high.refusal == "The Core could not set that filter." && rx.pad != nil)
        await model.disconnect()
    }

    @Test("the TX filter's edges are typed in within the desktop's ranges, on the Modes tab and TX panel (C1)")
    func typedTransmitFilterEdges() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await settle { transmit.settingsEditable && modes.txFilterHighHz == 2900 })
        modes.openTxFilterPad(low: false)
        let high = try #require(modes.pad)
        #expect(high.range == 200...10000 && !high.signed)
        type(high, "3100")
        async let kept = high.enter()
        #expect(await answer(station, key: "transmit", "filterHigh") == .i64(3100))
        #expect(await kept)
        #expect(await settle { modes.txFilterHighHz == 3100 && modes.pad == nil })
        transmit.openTxFilterPad(low: true)
        let low = try #require(transmit.pad)
        #expect(low.range == 0...5000)
        type(low, "6000")
        #expect(low.problem == "Choose a value from 0 to 5000 Hz." && !low.canEnter)
        type(low, "150")
        async let lowKept = low.enter()
        #expect(await answer(station, key: "transmit", "filterLow") == .i64(150))
        #expect(await lowKept)
        #expect(await settle { transmit.txFilterLowHz == 150 && transmit.pad == nil })
        await model.disconnect()
    }

    @Test("NNR held back by the Core shows its words in the warning colour, Try again and the model (C3)")
    func nnrStepBack() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let rx = model.main.rx
        let reason = "Noise reduction is using the Standard model. The Core computer could not keep up with Premium."
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 59, name: "activeNr", value: .enumeration(8)),
            .init(ordinal: 60, name: "nnrModelSlot", value: .i64(1)),
            .init(ordinal: 69, name: "nnrAvailable", value: .bool(true)),
            .init(ordinal: 72, name: "nnrStandardAvailable", value: .bool(true)),
            .init(ordinal: 73, name: "nnrPremiumAvailable", value: .bool(true)),
            .init(ordinal: 84, name: "nnrStatus", value: .utf8(reason)),
            .init(ordinal: 86, name: "nnrLimit", value: .i64(1)),
        ])))
        #expect(await settle { rx.nnr?.limit == 1 })
        let nnr = try #require(rx.noise.first { $0.label == "NNR" })
        #expect(nnr.lit && nnr.reason == nil && nnr.warning == reason)
        #expect(rx.nnr?.limitText == reason && rx.nnr?.tryAgainReason == nil)
        #expect(rx.nnr?.modelSlot == 1)
        rx.tryNnrAgain()
        let invoke = await answerCommand(station, RxPanelModel.nnrTryAgainVerb)
        #expect(invoke?.args == [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0))])
        rx.selectNnrModel(0)
        #expect(await answer(station, key: "slice:0", "nnrModelSlot") == .i64(0))
        // The limit lifted: no warning.
        await deliver(station, key: "slice:0", property: "nnrLimit", ordinal: 86, value: .i64(0))
        #expect(await settle { rx.noise.first { $0.label == "NNR" }?.warning == nil })
        await model.disconnect()
    }

    @Test("RF Power writes power and the drive slider source at version 5, never a per-band power table (I3)")
    func rfPowerLeavesPerBandTablesToTheCore() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let transmit = model.main.transmit
        let keys = ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "GEN", "WWV", "XVTR"]
        let bands = Dictionary(uniqueKeysWithValues: keys.map { ($0, 50) })
        let json = String(decoding: try JSONSerialization.data(withJSONObject: bands, options: [.sortedKeys]),
                          as: UTF8.self)
        await deliver(station, key: "transmit", property: "powerByBandJson", ordinal: 64, value: .utf8(json))
        #expect(await settle { transmit.settingsEditable(5) })
        transmit.setRfPower(60)
        #expect(await answer(station, key: "transmit", "power") == .i64(60))
        #expect(await answer(station, key: "transmit", "tuneDrivePowerSource") == .enumeration(0))
        await idle()
        // The Core alone keeps the per-band tables; it refuses them from the phone.
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message {
                return write.properties.contains { $0.name == "powerByBandJson" || $0.name == "tunePowerByBandJson" }
            }
            return false
        })
        await model.disconnect()
    }

    @Test("ATT, S-ATT and A-ATT, the Core's live range, a typed attenuation and RX1 preamp write stepAtt (I5, I6)")
    func attenuatorWays() async throws {
        let (model, station) = try await connected(catalogue: Self.hermesLite, frontEnd: true)
        let modes = model.main.modes
        #expect(modes.attenuatorWay == .stepAtt && modes.attenuatorReason == nil)
        modes.selectAttenuatorWay(.autoAtt)
        #expect(await answer(station, key: "stepAtt", "autoAttEnabled") == .bool(true))
        #expect(await settle { modes.attenuatorWay == .autoAtt })
        modes.selectAttenuatorWay(.att)
        #expect(await answer(station, key: "stepAtt", "enabled") == .bool(false))
        #expect(await settle { modes.attenuatorWay == .att && modes.attenuatorReason == ModesTabModel.attOffText })
        modes.selectAttenuatorWay(.stepAtt)
        #expect(await answer(station, key: "stepAtt", "enabled") == .bool(true))
        #expect(await answer(station, key: "stepAtt", "autoAttEnabled") == .bool(false))
        #expect(await settle { modes.attenuatorWay == .stepAtt })
        // The Core's live range replaces the catalogue's.
        await deliver(station, key: "stepAtt", property: "minDb", ordinal: 9, value: .i64(-10))
        await deliver(station, key: "stepAtt", property: "maxDb", ordinal: 10, value: .i64(20))
        #expect(await settle { modes.attenuatorRange?.min == -10 && modes.attenuatorRange?.max == 20 })
        modes.openAttenuatorPad()
        let pad = try #require(modes.pad)
        #expect(pad.range == -10...20 && pad.signed)
        type(pad, "7", negative: true)
        async let kept = pad.enter()
        #expect(await answer(station, key: "stepAtt", "attenuationDb") == .i64(-7))
        #expect(await kept)
        // The ANAN-G2 has no second ADC preamp: the control is left out.
        #expect(!modes.rx1PreampPresent && modes.rx1Preamp == nil && modes.rx1PreampReason == nil)
        let before = station.messages.count
        modes.toggleRx1Preamp()
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()

        // On a board that has it, RX1 preamp, as the Core mirrors it, switches.
        let (dual, dualStation) = try await connected(catalogue: Self.anan, frontEnd: true,
                                                      board: ["rx1Preamp": true])
        #expect(await settle { dual.main.modes.rx1PreampPresent && dual.main.modes.rx1Preamp == false })
        #expect(dual.main.modes.rx1PreampReason == nil)
        dual.main.modes.toggleRx1Preamp()
        #expect(await answer(dualStation, key: "stepAtt", "rx1Preamp") == .bool(true))
        await dual.disconnect()
    }

    @Test("RIT and XIT stay within -10000 to 10000 Hz, clear with 0 and take a typed offset (I7)")
    func ritXitClampClearAndType() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        await deliver(station, key: "slice:0", property: "ritHz", ordinal: 55, value: .i64(9950))
        #expect(await settle { modes.ritHz == 9950 })
        modes.stepRit(up: true)
        #expect(await answer(station, key: "slice:0", "ritHz") == .i64(10000))
        #expect(await settle { modes.ritHz == 10000 })
        let atTop = station.messages.count
        modes.stepRit(up: true)
        await idle()
        #expect(station.messages.count == atTop)
        modes.clearRit()
        #expect(await answer(station, key: "slice:0", "ritHz") == .i64(0))
        modes.openXitPad()
        let pad = try #require(modes.pad)
        type(pad, "250", negative: true)
        #expect(pad.shown == "\u{2212}250")
        async let kept = pad.enter()
        #expect(await answer(station, key: "slice:0", "xitHz") == .i64(-250))
        #expect(await kept)
        #expect(await settle { modes.xitHz == -250 })
        modes.clearXit()
        #expect(await answer(station, key: "slice:0", "xitHz") == .i64(0))
        await model.disconnect()
    }

    @Test("the DIG offset works in DIGL and DIGU, RTTY mark and shift in DIGL, each by the flag's step (I9)")
    func digitalOffsets() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        let catalog = try #require(model.main.catalogFeed.catalog)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 138, name: "diglOffsetHz", value: .i64(0)),
            .init(ordinal: 139, name: "diguOffsetHz", value: .i64(1500)),
            .init(ordinal: 140, name: "rttyMarkHz", value: .i64(2295)),
            .init(ordinal: 141, name: "rttyShiftHz", value: .i64(170)),
        ])))
        // USB: both greyed with their reasons, sending nothing.
        #expect(await settle { modes.rttyMarkHz == 2295 })
        #expect(modes.digOffsetReason == ModesTabModel.digOffsetText && modes.rttyReason == ModesTabModel.rttyText)
        let before = station.messages.count
        modes.stepDigOffset(up: true)
        modes.stepRtty(mark: true, up: true)
        await idle()
        #expect(station.messages.count == before)
        // DIGU: its own offset, 10 Hz a step; RTTY still waits for DIGL.
        let digu = try #require(catalog.modes.first { $0.label == "DIGU" })
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(digu.id)))
        #expect(await settle { modes.digOffsetReason == nil && modes.digOffsetHz == 1500 })
        #expect(modes.rttyReason == ModesTabModel.rttyText)
        modes.stepDigOffset(up: true)
        #expect(await answer(station, key: "slice:0", "diguOffsetHz") == .i64(1510))
        // DIGL: DIGL's offset, and RTTY mark by 25 Hz and shift by 5 Hz.
        let digl = try #require(catalog.modes.first { $0.label == "DIGL" })
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(digl.id)))
        #expect(await settle { modes.rttyReason == nil && modes.digOffsetHz == 0 })
        modes.stepDigOffset(up: false)
        #expect(await answer(station, key: "slice:0", "diglOffsetHz") == .i64(-10))
        modes.stepRtty(mark: true, up: true)
        #expect(await answer(station, key: "slice:0", "rttyMarkHz") == .i64(2320))
        modes.stepRtty(mark: false, up: false)
        #expect(await answer(station, key: "slice:0", "rttyShiftHz") == .i64(165))
        modes.openRttyPad(mark: true)
        let pad = try #require(modes.pad)
        #expect(pad.range == 1000...3500)
        await model.disconnect()
    }

    @Test("NNR's settings write the slice; the other reducers' settings are greyed with the reason (I10)")
    func nnrSettings() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        #expect(await settle { modes.nnrSettings["nnrMaskFloorDb"] != nil })
        let suppression = try #require(NnrSettingsSection.settings.first { $0.property == "nnrMaskFloorDb" })
        modes.setNnrSetting(suppression, -30.04)
        #expect(await answer(station, key: "slice:0", "nnrMaskFloorDb") == .f64(-30))
        let position = try #require(NnrSettingsSection.settings.first { $0.property == "nnrPosition" })
        modes.setNnrSetting(position, 1)
        #expect(await answer(station, key: "slice:0", "nnrPosition") == .enumeration(1))
        #expect(NnrSettingsSection.undescribed == ["NR1", "NR2", "NR3", "NR4", "DFNR", "MNR"])
        await model.disconnect()
    }

    @Test("AUTO says what it chose, the Core's output route switches, and RX bypass writes the Alex antennas (M1, M4, M5)")
    func autoInfoRouteAndBypass() async throws {
        // A board with the RX bypass relay (the suite's two have none).
        let relay: [String: Any] = ["relays": ["rxOutOnTx": true, "ext1OutOnTx": NSNull(), "ext2OutOnTx": NSNull(),
                                               "rxOutOverride": false] as [String: Any]]
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true, board: relay)
        let modes = model.main.modes
        #expect(modes.autoAgcInfo == nil)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 49, name: "autoAgcEnabled", value: .bool(true)),
        ])))
        #expect(await settle { modes.autoAgcInfo == ModesTabModel.noiseFloorWaitingText })
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 18, name: "stationAutoAgcNoiseFloorDbm", value: .f64(-110.6)),
            .init(ordinal: 19, name: "stationAutoAgcNoiseFloorValid", value: .bool(true)),
            .init(ordinal: 50, name: "autoAgcOffset", value: .f64(8)),
        ])))
        #expect(await settle { modes.autoAgcInfo == "NF \u{2212}110 dB \u{00B7} offset +8" })

        #expect(modes.outputRoute == ModesTabModel.speakersRoute)
        modes.selectOutputRoute(ModesTabModel.phonesRoute)
        #expect(await answer(station, key: "slice:0", "outputRoute") == .enumeration(1))

        // The Core's Alex antennas at radioHardwareVersion 7: RX bypass switches.
        #expect(modes.bypassReason == nil && modes.bypass == false)
        modes.toggleBypass()
        #expect(await answer(station, key: "alexAntennas", "rxOutOnTx") == .bool(true))
        await model.disconnect()

        // A Core before RX bypass on transmit (radioHardwareVersion 4) says so.
        let (older, _) = try await connected(catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 4,
                                             board: relay)
        #expect(await settle { older.main.modes.bypassReason == ModesTabModel.bypassOlderCoreText })
        await older.disconnect()
    }

    @Test("TX profiles, 2-Tone and PS-A ask the Core by its verbs (I12, M6, M7)")
    func profileTwoToneAndPsa() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let transmit = model.main.transmit
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 37, name: "activeTxProfile", value: .utf8("Default")),
            .init(ordinal: 38, name: "txProfilesJson", value: .utf8("[\"Default\",\"DX\"]")),
        ])))
        #expect(await settle { transmit.profiles == ["Default", "DX"] && transmit.activeProfile == "Default" })
        transmit.selectProfile("DX")
        let select = await answerCommand(station, TransmitModel.txProfileVerb)
        #expect(select?.args == [LinkMessage.PropertyEntry(name: "name", value: .utf8("DX"))])
        #expect(TransmitModel.profileNames("[\"A\",3]").isEmpty && TransmitModel.profileNames("x").isEmpty)

        transmit.toggleTwoTone()
        let twoTone = await station.waitForMessage(within: .seconds(30)) { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == "tx.twoTone"
            }
            return false
        }
        #expect(twoTone.flatMap(TransmitScreenTests.invoke)?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(true))])

        #expect(await settle { transmit.psaReason == nil })
        transmit.togglePsa()
        _ = await answerCommand(station, TransmitModel.psaOnVerb)
        await deliver(station, key: "pureSignalSettings", property: "autoCalEnabled", ordinal: 0, value: .bool(true))
        #expect(await settle { transmit.psa })
        transmit.togglePsa()
        _ = await answerCommand(station, TransmitModel.psaOffVerb)
        await model.disconnect()
    }

    @Test("PS-A is greyed with the reason on a radio without PureSignal and while the Core cannot run it")
    func psaReasons() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let transmit = model.main.transmit
        // Receive only for this phone, but a version 8 Core arms PS-A off the air.
        #expect(await settle { model.mirror.isSnapshotComplete && transmit.psaReason == nil })
        await deliver(station, key: "pureSignal", property: "canActuate", ordinal: 1, value: .bool(false))
        #expect(await settle { transmit.psaReason == TransmitModel.psaCannotRunText })
        await deliver(station, key: "pureSignal", property: "available", ordinal: 0, value: .bool(false))
        #expect(await settle { transmit.psaReason == TransmitModel.psaNoRadioText })
        let before = station.messages.count
        transmit.togglePsa()
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()
    }

    // MARK: Match RX and MON

    @Test("Match RX writes the TX filter converted from the receive filter for USB, LSB and AM (D81)")
    func matchRxConvertsTheReceiveFilter() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await settle { transmit.settingsEditable })
        #expect(transmit.txFilterLowHz == 100 && transmit.txFilterHighHz == 2900)
        let catalog = try #require(model.main.catalogFeed.catalog)
        let lsb = try #require(catalog.modes.first { $0.label == "LSB" })
        let am = try #require(catalog.modes.first { $0.label == "AM" })

        // USB, 100 to 3000 Hz: the same edges.
        modes.matchRx()
        #expect(await answer(station, key: "transmit", "filterLow") == .i64(100))
        #expect(await answer(station, key: "transmit", "filterHigh") == .i64(3000))
        #expect(await settle { modes.txFilterLowHz == 100 && modes.txFilterHighHz == 3000 })
        #expect(transmit.txFilterLowHz == 100 && transmit.txFilterHighHz == 3000)

        // LSB, -2800 to -150 Hz: the edges' magnitudes, smaller first.
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(lsb.id)))
        await deliver(station, key: "slice:0", property: "filterLow", ordinal: 3, value: .i64(-2800))
        await deliver(station, key: "slice:0", property: "filterHigh", ordinal: 4, value: .i64(-150))
        #expect(await settle { modes.modeLabel == "LSB" && modes.filterLowHz == -2800 && modes.filterHighHz == -150 })
        transmit.matchRxFilter()
        #expect(await answer(station, key: "transmit", "filterLow") == .i64(150))
        #expect(await answer(station, key: "transmit", "filterHigh") == .i64(2800))
        #expect(await settle { transmit.txFilterLowHz == 150 && transmit.txFilterHighHz == 2800 })

        // AM, -4000 to 4000 Hz: 0 to the high edge's magnitude.
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(am.id)))
        await deliver(station, key: "slice:0", property: "filterLow", ordinal: 3, value: .i64(-4000))
        await deliver(station, key: "slice:0", property: "filterHigh", ordinal: 4, value: .i64(4000))
        #expect(await settle { modes.modeLabel == "AM" && modes.filterLowHz == -4000 && modes.filterHighHz == 4000 })
        modes.matchRx()
        #expect(await answer(station, key: "transmit", "filterLow") == .i64(0))
        #expect(await answer(station, key: "transmit", "filterHigh") == .i64(4000))
        #expect(await settle { modes.txFilterLowHz == 0 && modes.txFilterHighHz == 4000 })

        // A new filter wholly above the TX filter now sends its high edge
        // first, so the Core never sees the low edge above the high one.
        let usb = try #require(catalog.modes.first { $0.label == "USB" })
        await deliver(station, key: "slice:0", property: "dspMode", ordinal: 2, value: .enumeration(Int64(usb.id)))
        await deliver(station, key: "slice:0", property: "filterLow", ordinal: 3, value: .i64(4500))
        await deliver(station, key: "slice:0", property: "filterHigh", ordinal: 4, value: .i64(5200))
        #expect(await settle { modes.modeLabel == "USB" && modes.filterLowHz == 4500 && modes.filterHighHz == 5200 })
        let start = station.messages.count
        modes.matchRx()
        #expect(await answer(station, key: "transmit", "filterHigh") == .i64(5200))
        #expect(await answer(station, key: "transmit", "filterLow") == .i64(4500))
        let order = station.messages.dropFirst(start).compactMap { message -> String? in
            if case .propertyWrite(let write) = message, write.key == "transmit" {
                return write.properties.first?.name
            }
            return nil
        }
        #expect(order == ["filterHigh", "filterLow"])
        await model.disconnect()
    }

    @Test("the desktop's rule, edge by edge, for each mode family")
    func theMatchRule() {
        #expect(TxFilterMatch.audioEdges(lowHz: 100, highHz: 2900, modeLabel: "USB") == (100, 2900))
        #expect(TxFilterMatch.audioEdges(lowHz: -2900, highHz: -100, modeLabel: "LSB") == (100, 2900))
        #expect(TxFilterMatch.audioEdges(lowHz: -1000, highHz: -300, modeLabel: "DIGL") == (300, 1000))
        #expect(TxFilterMatch.audioEdges(lowHz: -500, highHz: 500, modeLabel: "CWU") == (500, 500))
        for label in ["AM", "SAM", "DSB", "FM", "DRM"] {
            #expect(TxFilterMatch.audioEdges(lowHz: -5000, highHz: 3000, modeLabel: label) == (0, 3000), "\(label)")
        }
        #expect(TxFilterMatch.audioEdges(lowHz: -5000, highHz: 3000, modeLabel: "SPEC") == (3000, 5000))
        #expect(TxFilterMatch.writes((100, 2900), currentHighHz: 3000).map(\.0) == ["filterLow", "filterHigh"])
        #expect(TxFilterMatch.writes((3000, 3500), currentHighHz: 3000).map(\.0) == ["filterHigh", "filterLow"])
        #expect(TxFilterMatch.writes((100, 2900), currentHighHz: nil).map(\.0) == ["filterLow", "filterHigh"])
    }

    @Test("Match RX sends nothing while the Core does not take transmit settings from this phone")
    func matchRxGreyedWithoutSettings() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false,
                                                   withoutCapabilities: ["transmitSettingsVersion"])
        #expect(await settle { model.mirror.isSnapshotComplete })
        #expect(!model.main.transmit.settingsEditable)
        let before = station.messages.count
        model.main.modes.matchRx()
        model.main.transmit.matchRxFilter()
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()
    }

    @Test("MON is greyed with the first reason until the Core sends the transmit monitor, then with the second off headphones")
    func monGreyedForEachReason() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let transmit = model.main.transmit
        #expect(await settle { transmit.settingsEditable })
        #expect(TransmitModel.monNotSentText
                == "This Core does not send the transmit monitor. Updating the Core may help.")
        #expect(transmit.monReason == TransmitModel.monNotSentText)
        // Headphones alone do not lift it, and a tap sends nothing.
        transmit.headphonesChanged(true)
        #expect(transmit.monReason == TransmitModel.monNotSentText)
        transmit.toggleMon()
        await idle()
        #expect(Self.monEnabledWrites(station).isEmpty)

        // A Core that sends the transmit monitor lifts it while the sound is in headphones.
        var capabilities = model.mirror.capabilities
        capabilities[TransmitModel.monitorCapability] = .int(1)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        #expect(await settle { transmit.monReason == nil })
        // Off headphones (the loudspeaker or the earpiece) the second reason.
        transmit.headphonesChanged(false)
        #expect(TransmitModel.monNeedsHeadphonesText
                == "Plug in headphones to hear your transmit. The loudspeaker would feed back into the microphone.")
        #expect(transmit.monReason == TransmitModel.monNeedsHeadphonesText)
        transmit.toggleMon()
        await idle()
        #expect(Self.monEnabledWrites(station).isEmpty)
        await model.disconnect()
    }

    @Test("MON on writes monEnabled on exactly once, off writes it off, as the desktop's MON does")
    func monWritesMonEnabled() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false, additions: [.remoteTx])
        let transmit = model.main.transmit
        #expect(await settle { transmit.settingsEditable })
        var capabilities = model.mirror.capabilities
        capabilities[TransmitModel.monitorCapability] = .int(1)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        transmit.headphonesChanged(true)
        #expect(await settle { transmit.monReason == nil })

        transmit.toggleMon()
        #expect(await answer(station, key: "transmit", "monEnabled") == .bool(true))
        // The Core's answer to the route lights it, with its monEnabled on.
        model.main.receive(.monitorAudioContext(.init(revision: 1, route: .speakers)))
        #expect(await settle { transmit.mon })
        await idle()
        #expect(Self.monEnabledWrites(station) == [.bool(true)])

        transmit.toggleMon()
        #expect(await answer(station, key: "transmit", "monEnabled") == .bool(false))
        #expect(await settle { !transmit.mon })
        await idle()
        #expect(Self.monEnabledWrites(station) == [.bool(true), .bool(false)])
        await model.disconnect()
    }

    @Test("every phone write of monEnabled goes through MON's one property name")
    func monEnabledWrittenOnlyByMon() throws {
        let app = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
        let files = try #require(FileManager.default.enumerator(at: app, includingPropertiesForKeys: nil))
            .compactMap { $0 as? URL }
            .filter { $0.pathExtension == "swift" && !$0.pathComponents.contains("Tests") }
        let lines = try files.flatMap { file in
            try String(contentsOf: file, encoding: .utf8).split(separator: "\n").filter { $0.contains("\"monEnabled\"") }
        }
        #expect(lines.count == 1)
        #expect(lines.allSatisfy { $0.contains("static let monEnabledProperty = \"monEnabled\"") })
    }

    /// The values of every `monEnabled` write the app sent, in order.
    static func monEnabledWrites(_ station: FakeStation) -> [LinkMessage.PropertyValue] {
        station.messages.compactMap { message -> LinkMessage.PropertyValue? in
            guard case .propertyWrite(let write) = message else {
                return nil
            }
            return write.properties.first { $0.name == "monEnabled" }?.value
        }
    }

    @Test("no on-screen string of the phone's own carries the word \"yet\" (D41)")
    func noStringPromisesYet() throws {
        let ios = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        // None kept: the Core dropped the word from every reason it sends
        // (trunk 40b089c9a), and the phone's own words follow it.
        let kept: [String] = []
        let literal = try Regex(#""[^"\n]*\byet\b[^"\n]*""#).ignoresCase()
        var found: [String] = []
        for folder in ["NereusApp", "NereusKit/Sources", "NereusActivity", "Shared"] {
            let root = ios.appendingPathComponent(folder)
            guard let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: nil) else {
                continue
            }
            for case let file as URL in walk where file.pathExtension == "swift" {
                if file.pathComponents.contains("Tests") || file.pathComponents.contains("UITests") {
                    continue
                }
                for line in try String(contentsOf: file, encoding: .utf8).split(separator: "\n") {
                    let text = line.trimmingCharacters(in: .whitespaces)
                    // Comments, and log lines no operator reads.
                    if text.hasPrefix("//") || text.contains("logger.") {
                        continue
                    }
                    for match in text.matches(of: literal) {
                        let words = String(text[match.range].dropFirst().dropLast())
                        if !kept.contains(words) {
                            found.append("\(file.lastPathComponent): \(words)")
                        }
                    }
                }
            }
        }
        #expect(found.isEmpty, "\(found)")
    }

    @Test("a refused slice write shows the Core's words, and the next accepted one clears them")
    func refusalShowsTheCoresWords() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        modes.toggleMute()
        // The Core keeps the slice unmuted and says why.
        await answer(station, key: "slice:0", "muted", keep: .bool(false),
                     refuse: "Tablet B is on the air. Try again when they stop.")
        #expect(await settle { modes.note == "Tablet B is on the air. Try again when they stop." })
        #expect(modes.muted == false)
        modes.toggleMute()
        await answer(station, key: "slice:0", "muted")
        #expect(await settle { modes.note == nil && modes.muted == true })
        await model.disconnect()
    }

    // MARK: No radio tables in the app

    @Test("the app's source holds no radio tables: no numeric lists and no radio labels")
    func noRadioTablesInTheApp() throws {
        let app = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
        let skipped: Set<String> = ["Tests", "UITests", "Resources"]
        let files = try #require(FileManager.default.enumerator(at: app, includingPropertiesForKeys: nil))
            .compactMap { $0 as? URL }
            .filter { $0.pathExtension == "swift" }
            .filter { url in !url.pathComponents.contains { skipped.contains($0) } }
            // This DEBUG-only UI fixture supplies mock Core data to UI tests.
            .filter { $0.lastPathComponent != "UITestQuestion.swift" }
        #expect(files.count > 50)
        // A list of three or more numbers, `[0, 31, 1]`. A list of zeros
        // only, `[0, 0, 0]`, holds no radio's values: it is a start state
        // the Core's values replace (the Tuner Genius's three relays before
        // the Core sends relayC1, relayL and relayC2), so it is not a table.
        let numericList = try Regex(#"\[\s*-?\d+(\.\d+)?\s*(,\s*-?\d+(\.\d+)?\s*){2,}\]"#)
        let number = try Regex(#"-?\d+(\.\d+)?"#)
        // Two lists are not a radio's: the transmit time-out menu's times in
        // seconds (PTT buttons page), the same on every radio, the Core's
        // own limits, not the list, bounding what it keeps; and the
        // connection charts' grid lines at the bottom, middle and top of
        // each chart (Tools, Connection and performance); and the AM Mod
        // Monitor's positive scale ticks, percent of carrier drawn under
        // its bar (TX panel, the approved board), the same on every radio.
        // Those ticks are uneven, so no range and step says them plainly.
        let notRadioLists: Set<String> = ["[30, 60, 120, 180, 300, 600, 900, 1200, 1800]", "[0.0, 0.5, 1.0]",
                                          "[0, 50, 100, 125, 160]",
                                          // The TX EQ curve's Bands choices (the desktop's 5, 10 and
                                          // 18-band) and its drawing's Hz scale steps.
                                          "[5, 10, 18]", "[50.0, 100, 200, 250, 500, 1000, 2000, 2500, 5000]",
                                          // The flag's step cycle: the desktop's step ladder in hertz,
                                          // the same on every radio.
                                          "[1, 10, 100, 500, 1000, 10000]"]
        #expect(TransmitTimeOutModel.choices == [30, 60, 120, 180, 300, 600, 900, 1200, 1800])
        #expect(ModMonitorModel.posScale.ticks == [0, 50, 100, 125, 160])
        #expect(TxEqualizerModel.curveCounts == [5, 10, 18])
        #expect(FlagControls.stepLadder == [1, 10, 100, 500, 1000, 10000])
        #expect(TxEqCurveParts.tickStep(250) == 50 && TxEqCurveParts.tickStep(40_000) == 10000)
        func tables(in text: String) -> [Substring] {
            text.matches(of: numericList).map { text[$0.range] }.filter { list in
                !notRadioLists.contains(String(list))
                    && list.matches(of: number).contains { Double(list[$0.range]) != 0 }
            }
        }
        #expect(tables(in: "let steps = [0, 31, 1]").count == 1)
        #expect(tables(in: "let ranges = [-28, 0, 0.5]").count == 1)
        #expect(tables(in: "var relays: [Int64] = [0, 0, 0]").isEmpty)
        #expect(tables(in: "var start = [0.0, -0, 0]").isEmpty)
        // A radio's own labels: antenna ports, receive-only inputs, preamp items.
        let radioLabel = try Regex(#""(ANT\d|BYPS|EXT\d|XVTR|RX\d|0dB|-[123]0dB)""#)
        // The step attenuator's ranges, as numbers.
        let attenuatorRange = try Regex(#"-28|\b31\b"#)
        // The DEBUG-only UI-test fixtures under Shared, which stand in for the Core.
        let debugFixtures: Set<String> = ["UITestQuestion.swift", "UITestBand.swift"]
        for file in files {
            let text = try String(contentsOf: file, encoding: .utf8)
            let name = file.lastPathComponent
            #expect(tables(in: text).isEmpty, "\(name) holds a list of numbers: \(tables(in: text))")
            if file.pathComponents.suffix(2).first == "Shared", debugFixtures.contains(name) {
                // These app-source fixtures inject a fake Core question, or
                // the Core's band and one slice, for UI tests. Their antenna
                // labels must remain DEBUG-only: the whole file is #if DEBUG.
                let code = text.split(separator: "\n").map(String.init).filter { line in
                    let trimmed = line.trimmingCharacters(in: .whitespaces)
                    return !trimmed.isEmpty && !trimmed.hasPrefix("//")
                }
                #expect(code.first == "#if DEBUG" && code.last == "#endif")
            } else {
                // The desktop flag's name for its bypass switch, one constant, the same on every radio.
                let bypassTitle = #"static let bypassTitle = "BYPS""#
                let checked = name == "FlagControls.swift" ? text.replacingOccurrences(of: bypassTitle, with: "") : text
                #expect(checked.firstMatch(of: radioLabel) == nil, "\(name) holds a radio's label")
            }
            if file.pathComponents.contains("Modes") {
                #expect(text.firstMatch(of: attenuatorRange) == nil, "\(name) holds an attenuator range")
            }
        }
    }

    // MARK: The catalogue's transmit ranges, presence flags and noise-reduction settings

    @Test("each radio's RF Power, Tune Pwr and mic gain take its catalogue's ranges and readouts",
          arguments: [anan, hermesLite])
    func transmitRangesFromTheCatalogue(_ fixture: String) async throws {
        let (model, station) = try await connected(catalogue: fixture, frontEnd: false)
        let transmit = model.main.transmit
        let modes = model.main.modes
        let catalog = try #require(model.main.catalogFeed.catalog)
        let ranges = try #require(catalog.board.transmit)
        let power = try #require(ranges.power)
        let tune = try #require(ranges.tunePowerForTxBand)
        #expect(await settle { transmit.powerControl == power && transmit.tuneControl == tune })
        #expect(await settle { modes.micGainRange == ranges.micGainDb })
        #expect(await settle { transmit.settingsEditable(2) })
        // Half way between two steps goes to the nearer step of the board's own.
        let asked = power.min + power.step * 8.4
        transmit.setRfPower(asked)
        #expect(await answer(station, key: "transmit", "power") == .i64(Int64(power.min + power.step * 8)))
        transmit.setTunePower(tune.min + tune.step * 5.4)
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .commandInvoke(let invoke) = message {
                return invoke.verb == "setTunePowerForTxBand"
            }
            return false
        }
        guard case .commandInvoke(let invoke)? = sent else {
            Issue.record("no setTunePowerForTxBand reached the Core")
            return
        }
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "watts", value: .i64(Int64(tune.min + tune.step * 5)))])
        // The mic gain goes no further than the board's range.
        let mic = try #require(ranges.micGainDb)
        modes.setMicGain(mic.max + 20)
        #expect(await answer(station, key: "transmit", "micGainDb") == .i64(Int64(mic.max)))
        // The readouts: the number on a 100 W board, dB on a Hermes Lite 2.
        if fixture == Self.hermesLite {
            #expect(transmit.powerControl.text(power.max) == "0.0 dB")
            #expect(transmit.tuneControl.text(tune.min).hasSuffix(" dB"))
        } else {
            #expect(transmit.powerControl.text(power.max) == String(Int(power.max)))
        }
        await model.disconnect()
    }

    @Test("a Core whose catalogue has no transmit ranges keeps 0 to 100 and the Core's own mic range")
    func transmitRangesFromAnOlderCore() async throws {
        let (model, station) = try await connected(catalogue: Self.hermesLite, frontEnd: false,
                                                   removing: ["noiseReduction"],
                                                   removingFromBoard: ["transmit", "rx1Preamp", "relays"])
        let transmit = model.main.transmit
        let modes = model.main.modes
        #expect(await settle { model.main.catalogFeed.catalog != nil && model.main.catalogFeed.catalog?.board.transmit == nil })
        #expect(await settle { transmit.powerControl == TransmitModel.powerFallback })
        #expect(transmit.tuneControl == TransmitModel.powerFallback)
        #expect(modes.micGainRange == ModesTabModel.micGainFallback)
        #expect(transmit.powerControl.text(57) == "57")
        #expect(await settle { transmit.settingsEditable(1) })
        transmit.setRfPower(57)
        #expect(await answer(station, key: "transmit", "power") == .i64(57))
        // Without the presence flags the controls stay, greyed where they cannot change.
        #expect(modes.rx1PreampPresent && modes.bypassPresent)
        // Without the descriptions NR1 to MNR stay greyed.
        #expect(modes.noiseReduction == nil)
        await model.disconnect()
    }

    @Test("hardware the radio lacks is left out: the second ADC's preamp and the RX bypass relay",
          arguments: [anan, hermesLite])
    func absentHardwareIsHidden(_ fixture: String) async throws {
        let (model, station) = try await connected(catalogue: fixture, frontEnd: true)
        let modes = model.main.modes
        #expect(await settle { !modes.rx1PreampPresent && !modes.bypassPresent })
        #expect(modes.rx1Preamp == nil && modes.rx1PreampReason == nil)
        #expect(modes.bypass == nil && modes.bypassReason == nil)
        let before = station.messages.count
        modes.toggleRx1Preamp()
        modes.toggleBypass()
        await idle()
        #expect(station.messages.count == before)
        await model.disconnect()
    }

    @Test("the Core's noise-reduction descriptions give each reducer its settings, which write the slice")
    func noiseReductionSettingsFromTheCatalogue() async throws {
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: false)
        let modes = model.main.modes
        #expect(await settle { modes.noiseReduction != nil && modes.nrValues["nr1Taps"] != nil })
        let nr = try #require(modes.noiseReduction)
        for slot in StationCatalog.NoiseReduction.slotKeys {
            let controls = try #require(nr[slot], "\(slot)")
            #expect(!controls.isEmpty, "\(slot)")
            for control in controls {
                #expect(modes.nrValues[control.property] != nil, "\(slot) \(control.property)")
            }
        }
        // NR1's taps: a whole number on the slice.
        let taps = try #require(nr["nr1"]?.first { $0.property == "nr1Taps" })
        modes.setNrControl(taps, 128.4)
        #expect(await answer(station, key: "slice:0", "nr1Taps") == .i64(128))
        // NR1's gain: the slider's position times its scale.
        let gain = try #require(nr["nr1"]?.first { $0.property == "nr1Gain" })
        guard case .slider(let gainSlider) = gain.kind else {
            Issue.record("NR1 gain is not a slider")
            return
        }
        modes.setNrControl(gain, 200)
        let written = await answer(station, key: "slice:0", "nr1Gain")
        guard case .f64(let value)? = written else {
            Issue.record("NR1 gain was not written as a number: \(String(describing: written))")
            return
        }
        #expect(abs(value - gainSlider.propertyValue(200)) < 1e-15)
        // NR2's gain method, a choice; its AE filter, a switch.
        let method = try #require(nr["nr2"]?.first { $0.property == "nr2GainMethod" })
        modes.setNrControl(method, 1)
        #expect(await answer(station, key: "slice:0", "nr2GainMethod") == .enumeration(1))
        let aeFilter = try #require(nr["nr2"]?.first { $0.property == "nr2AeFilter" })
        let aeWas = RxPanelModel.flag(modes.nrValues["nr2AeFilter"])
        modes.toggleNrControl(aeFilter)
        #expect(await answer(station, key: "slice:0", "nr2AeFilter") == .bool(!(aeWas ?? false)))
        // MNR's Reset puts Aggressiveness back to the value its Reset names.
        let oversub = try #require(nr["mnr"]?.first { $0.property == "mnrOversub" })
        guard case .slider(let oversubSlider) = oversub.kind, let reset = oversubSlider.reset else {
            Issue.record("MNR aggressiveness has no Reset")
            return
        }
        await deliver(station, key: "slice:0", property: "mnrOversub", ordinal: 115, value: .f64(9))
        #expect(await settle { RxPanelModel.number(modes.nrValues["mnrOversub"]) == 9 })
        modes.resetNrSlot("mnr")
        #expect(await answer(station, key: "slice:0", "mnrOversub") == .f64(oversubSlider.propertyValue(reset)))
        await model.disconnect()
    }

    // MARK: RX2's own input (Level Cal 2)

    @Test("a slice on the other input sets RX2's attenuator as the pair, and the Core's refusal above 31 shows as sent")
    func rx2AttenuatorPairAndRefusal() async throws {
        // A catalogue that states more than the Core keeps, so the phone
        // sends 32 and the Core says why it keeps 31.
        let (model, station) = try await connected(catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 12,
                                                   board: ["rx2Attenuator": ["min": 0, "max": 40, "step": 1]])
        try await onRx2(station, model: model, values: [
            ("rx2AttenuationDb", 18, .i64(31)), ("rx2StepAttEnabled", 20, .bool(true)),
            ("rx2AutoAttEnabled", 21, .bool(false)), ("rx2PreampMode", 24, .i64(0)),
        ])
        let modes = model.main.modes
        #expect(await settle { modes.onRx2Input && modes.attenuationDb == 31 && modes.attenuatorWay == .stepAtt })
        #expect(modes.attenuatorRange == StationCatalog.Range(min: 0, max: 40, step: 1))
        #expect(modes.attenuatorReason == nil)
        modes.stepAttenuator(up: true)
        #expect(await answer(station, key: "stepAtt", "rx2StepAttEnabled") == .bool(true))
        #expect(await answer(station, key: "stepAtt", "rx2AttenuationDb", keep: .i64(31),
                             refuse: "RX2's attenuator goes from 0 to 31 dB.") == .i64(32))
        #expect(await settle { modes.note == "RX2's attenuator goes from 0 to 31 dB." })
        #expect(modes.attenuationDb == 31)
        try await shootFrontEnd("modes-rx2-attenuator-refused", modes)
        // One step down is taken, and clears the Core's words.
        modes.stepAttenuator(up: false)
        #expect(await answer(station, key: "stepAtt", "rx2StepAttEnabled") == .bool(true))
        #expect(await answer(station, key: "stepAtt", "rx2AttenuationDb") == .i64(30))
        #expect(await settle { modes.note == nil && modes.attenuationDb == 30 })
        // Slice A's attenuator was never written.
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message {
                return write.key == "stepAtt" && write.properties.contains { $0.name == "attenuationDb" }
            }
            return false
        })
        // A-ATT and ATT set RX2's own enable and auto-attenuate.
        modes.selectAttenuatorWay(.autoAtt)
        #expect(await answer(station, key: "stepAtt", "rx2AutoAttEnabled") == .bool(true))
        modes.selectAttenuatorWay(.att)
        #expect(await answer(station, key: "stepAtt", "rx2StepAttEnabled") == .bool(false))
        #expect(await settle { modes.attenuatorWay == .att && modes.attenuatorReason == ModesTabModel.attOffText })
        await model.disconnect()
    }

    @Test("RX2's range and preamp list come only from the catalogue, each missing one greyed with the Core's reason")
    func rx2InputFromTheCatalogue() async throws {
        // ANAN-G2: RX2's own 0 to 31 dB attenuator, no RX2 preamp list.
        let (g2, g2Station) = try await connected(catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 12)
        try await onRx2(g2Station, model: g2, values: [
            ("rx2AttenuationDb", 18, .i64(12)), ("rx2StepAttEnabled", 20, .bool(true)),
        ])
        let modes = g2.main.modes
        #expect(await settle { modes.onRx2Input && modes.attenuationDb == 12 })
        #expect(modes.attenuatorRange == StationCatalog.Range(min: 0, max: 31, step: 1))
        #expect(modes.attenuatorReason == nil && modes.attenuatorWayReason == nil)
        #expect(modes.preamp.isEmpty && modes.preampReason == ModesTabModel.rx2NoPreampText)
        // The row's note names the preamp once (JJ, 2026-09-30).
        #expect(FrontEndSection.preampNote(try #require(modes.preampReason)) == "Preamp: none on this receiver input.")
        #expect(!FrontEndSection.preampCaptionShown(modes))
        try await shootFrontEnd("modes-rx2-g2-slice-b", modes, looks: true)
        // Back on slice A: slice A's own list and attenuator.
        await deliver(g2Station, key: "slice:1", property: "active", ordinal: 11, value: .bool(false))
        await deliver(g2Station, key: "slice:0", property: "active", ordinal: 11, value: .bool(true))
        #expect(await settle { !modes.onRx2Input && modes.attenuationDb == 0 && !modes.preamp.isEmpty })
        await g2.disconnect()

        // An HPSDR's second Mercury: two preamp states, no attenuator, and a
        // radio whose RX2 input the Core cannot set says so.
        let reason = "NereusSDR cannot set RX2's input on this radio."
        let (hpsdr, hpsdrStation) = try await connected(
            catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 12,
            board: ["rx2Attenuator": NSNull(), "rx2AttenuatorReason": NSNull(),
                    "rx2PreampItems": [["id": 1, "label": "0dB"], ["id": 0, "label": "-20dB"]]])
        try await onRx2(hpsdrStation, model: hpsdr, values: [
            ("rx2StepAttEnabled", 20, .bool(false)), ("rx2PreampMode", 24, .i64(1)),
        ])
        let second = hpsdr.main.modes
        #expect(await settle { second.onRx2Input && second.preamp.map(\.label) == ["0dB", "-20dB"] })
        #expect(second.preamp.filter(\.lit).map(\.label) == ["0dB"] && second.preampReason == nil)
        #expect(second.attenuatorReason == ModesTabModel.noAttenuatorText)
        second.selectPreamp(0)
        #expect(await answer(hpsdrStation, key: "stepAtt", "rx2PreampMode") == .i64(0))
        #expect(await settle { second.preamp.filter(\.lit).map(\.label) == ["-20dB"] })
        try await shootFrontEnd("modes-rx2-hpsdr-slice-b", second)
        await hpsdr.disconnect()

        let (none, noneStation) = try await connected(
            catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 12,
            board: ["rx2Attenuator": NSNull(), "rx2PreampItems": [] as [Any], "rx2AttenuatorReason": reason])
        try await onRx2(noneStation, model: none, values: [("rx2StepAttEnabled", 20, .bool(true))])
        let greyed = none.main.modes
        #expect(await settle { greyed.onRx2Input && greyed.attenuatorReason == reason })
        #expect(greyed.attenuatorWayReason == reason && greyed.preampReason == reason && greyed.preamp.isEmpty)
        try await shootFrontEnd("modes-rx2-no-input-slice-b", greyed)
        let before = noneStation.messages.count
        greyed.stepAttenuator(up: true)
        greyed.selectAttenuatorWay(.att)
        await idle()
        #expect(noneStation.messages.count == before)
        await none.disconnect()
    }

    @Test("a Core without RX2's catalogue items greys a slice on the other input with the older-Core words, sending nothing")
    func rx2OnAnOlderCore() async throws {
        let (model, station) = try await connected(
            catalogue: Self.anan, frontEnd: true, radioHardwareVersion: 12,
            removingFromBoard: ["rx2Attenuator", "rx2PreampItems", "rx2AttenuatorReason"])
        try await onRx2(station, model: model, capability: false, values: [
            ("rx2AttenuationDb", 18, .i64(5)), ("rx2StepAttEnabled", 20, .bool(true)),
        ])
        let modes = model.main.modes
        #expect(await settle { modes.onRx2Input && modes.preampReason == ModesTabModel.rx2PreampOlderCoreText })
        // The first input's list stays in its place, greyed and unlit.
        #expect(!modes.preamp.isEmpty && modes.preamp.allSatisfy { !$0.lit })
        #expect(modes.attenuatorReason == CatalogFeed.needsNewerCoreText)
        #expect(modes.attenuatorWayReason == CatalogFeed.needsNewerCoreText)
        try await shootFrontEnd("modes-rx2-older-core-slice-b", modes)
        let before = station.messages.count
        modes.stepAttenuator(up: true)
        modes.selectPreamp(modes.preamp[0].id)
        modes.openAttenuatorPad()
        await idle()
        #expect(station.messages.count == before && modes.pad == nil)
        await model.disconnect()
    }

    /// The Front end section alone, as the Modes tab draws it, into
    /// `NEREUS_MAIN_SHOTS`: dark and upright, and with `looks` also light,
    /// sideways and in large type.
    private func shootFrontEnd(_ name: String, _ modes: ModesTabModel, looks: Bool = false) async throws {
        var takes: [(String, ColorScheme, CGSize, DynamicTypeSize)] = [("", .dark, CGSize(width: 402, height: 760), .large)]
        if looks {
            takes += [("-light", .light, CGSize(width: 402, height: 760), .large),
                      ("-landscape", .dark, CGSize(width: 874, height: 560), .large),
                      ("-landscape-light", .light, CGSize(width: 874, height: 560), .large),
                      ("-large-type", .dark, CGSize(width: 402, height: 1_300), .accessibility2)]
        }
        for (suffix, scheme, size, type) in takes {
            let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
            let window = UIWindow(windowScene: scene)
            window.frame = CGRect(origin: .zero, size: size)
            window.windowLevel = .alert + 1
            let root = ScrollView { FrontEndSection(model: modes) }
                .background(ChromeColours.page)
                .preferredColorScheme(scheme)
                .environment(\.dynamicTypeSize, type)
            let host = UIHostingController(rootView: root)
            host.view.frame = CGRect(origin: .zero, size: size)
            window.rootViewController = host
            window.isHidden = false
            await ShotWait.laidOut(window)
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            window.isHidden = true
            window.rootViewController = nil
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image.pngData() {
                let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name)\(suffix).png")
                try data.write(to: url)
                print("Wrote \(url.path)")
            }
        }
    }

    /// Slice B, active, on the other input: the Core's RX2 capability
    /// (unless `capability` is false), then `stepAtt`'s `rx2SliceMask` with
    /// slice B's bit and RX2's `values`.
    private func onRx2(_ station: FakeStation, model: AppModel, capability: Bool = true,
                       values: [(String, UInt16, LinkMessage.PropertyValue)]) async throws {
        if capability {
            var capabilities = model.mirror.capabilities
            capabilities[ModesTabModel.rx2AttenuatorCapability] = .int(1)
            await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
                LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
            })))
        }
        await station.deliver(BandFlagShotTests.slice(1, active: true))
        await deliver(station, key: "slice:0", property: "active", ordinal: 11, value: .bool(false))
        await station.deliver(.delta(LinkMessage.Delta(key: "stepAtt", properties:
            [.init(ordinal: 19, name: "rx2SliceMask", value: .i64(2))]
            + values.map { LinkMessage.PropertyEntry(ordinal: $0.1, name: $0.0, value: $0.2) })))
        #expect(await settle { model.main.slices.activeSliceId == 1 })
    }

    // MARK: Inside

    /// A model connected to a fake Core with the suite's `catalogue`; with
    /// `frontEnd`, the Core adds its step attenuator and Alex antennas at
    /// `radioHardwareVersion`, as a current Core's snapshot does.
    private func connected(catalogue: String, frontEnd: Bool, radioHardwareVersion: Int64 = 7,
                           additions: FakeStation.Additions = [],
                           withoutCapabilities: Set<String> = [],
                           board: [String: Any] = [:], removing: Set<String> = [],
                           removingFromBoard: Set<String> = []) async throws -> (AppModel, FakeStation) {
        answered.removeAll()
        commandsAnswered.removeAll()
        let station = try FakeStation(additions: additions, withoutCapabilities: withoutCapabilities)
        let defaults = try #require(UserDefaults(suiteName: "ModesTabBindingTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        let json = try #require(Self.catalogueJSON(catalogue, board: board, removing: removing,
                                                   removingFromBoard: removingFromBoard))
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { !model.main.modes.modes.isEmpty && !model.main.rx.presets.isEmpty })
        if frontEnd {
            try await Self.addFrontEnd(station, model: model, catalogue: catalogue,
                                       radioHardwareVersion: radioHardwareVersion)
            #expect(await settle { model.main.modes.attenuationDb != nil && model.main.modes.attenuatorWay != nil })
        }
        return (model, station)
    }

    /// The Core's capabilities again, with `changes` set over them.
    static func deliverCapabilities(_ station: FakeStation, model: AppModel,
                                    _ changes: [String: MirrorValue]) async {
        var capabilities = model.mirror.capabilities
        for (name, value) in changes {
            capabilities[name] = value
        }
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
    }

    /// The Core's step attenuator and Alex antennas, as a Core at
    /// `radioHardwareVersion` adds them: its capabilities again with that
    /// version, then each object's schema and its object, the attenuator at
    /// 0 dB with the catalogue's first preamp item.
    static func addFrontEnd(_ station: FakeStation, model: AppModel, catalogue: String,
                            radioHardwareVersion: Int64) async throws {
        var capabilities = model.mirror.capabilities
        capabilities["radioHardwareVersion"] = .int(radioHardwareVersion)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        let board = try #require(catalogueObject(catalogue)?["board"] as? [String: Any])
        let attenuator = board["attenuator"] as? [String: Int] ?? [:]
        for className in ["StepAttenuatorFacade", "AlexAntennaFacade"] {
            await station.deliver(.schema(try FakeStation.schema(ofClass: className)))
        }
        let preamp = (board["preampItems"] as? [[String: Any]])?.first?["id"] as? Int ?? 0
        await station.deliver(.objectCreate(try FakeStation.objectCreate(
            key: "stepAtt", className: "StepAttenuatorFacade",
            values: ["enabled": .bool(true), "attenuationDb": .i64(0), "preampMode": .i64(Int64(preamp)),
                     "minDb": .i64(Int64(attenuator["min"] ?? 0)), "maxDb": .i64(Int64(attenuator["max"] ?? 0))])))
        await station.deliver(.objectCreate(try FakeStation.objectCreate(
            key: "alexAntennas", className: "AlexAntennaFacade", values: [:])))
    }

    /// Delivers one property's new value, as the Core's delta.
    private func deliver(_ station: FakeStation, key: String, property: String, ordinal: UInt16,
                         value: LinkMessage.PropertyValue) async {
        await station.deliver(.delta(LinkMessage.Delta(key: key, properties: [
            .init(ordinal: ordinal, name: property, value: value),
        ])))
    }

    /// Waits for the app's write of `property` to `key`, holding only that
    /// property, then answers it as the Core does: `property.result` by the
    /// write's id with the value it kept (`kept`, else the one asked for)
    /// and, when taken, a delta with it.
    @discardableResult
    private func answer(_ station: FakeStation, key: String, _ property: String,
                        keep kept: LinkMessage.PropertyValue? = nil,
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
        #expect(write.properties.count == 1, "the write of \(property) carries only it")
        let value = LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: property, value: kept ?? entry.value)
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: key, writeId: writeId, results: [
            .init(property: property, accepted: reason == nil, reason: reason ?? "", value: value),
        ])))
        if reason == nil {
            await station.deliver(.delta(LinkMessage.Delta(key: key, properties: [value])))
        }
        return entry.value
    }

    /// Waits for the app's next `verb`, answers it (accepted, or refused
    /// with `refuse`) and returns what the app sent.
    @discardableResult
    private func answerCommand(_ station: FakeStation, _ verb: String,
                               refuse reason: String? = nil) async -> LinkMessage.CommandInvoke? {
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
        await station.deliver(.commandResult(LinkMessage.CommandResult(verb: verb, id: invoke.id,
                                                                       accepted: reason == nil,
                                                                       reason: reason ?? "", affected: [],
                                                                       values: nil)))
        return invoke
    }

    /// Types `digits` on a number pad from an empty entry, with the sign asked for.
    private func type(_ pad: ValuePadModel, _ digits: String, negative: Bool = false) {
        while !pad.entry.isEmpty {
            pad.press(.delete)
        }
        if pad.negative != negative {
            pad.press(.minus)
        }
        for digit in digits {
            pad.press(.digit(digit.wholeNumberValue ?? 0))
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

    /// Lets anything the app might send go.
    private func idle() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    /// A suite catalogue's `json`, or nil outside the checkout.
    static func catalogueJSON(_ fixture: String) -> String? {
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
            .appendingPathComponent("tests/data/link/v1/sessions/\(fixture).json")
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

    /// A suite catalogue with its board's members set from `board` and the
    /// sections in `removing` and board members in `removingFromBoard` left
    /// out (an older Core, or a synthetic board with other hardware).
    static func catalogueJSON(_ fixture: String, board edits: [String: Any], removing: Set<String>,
                              removingFromBoard: Set<String>) -> String? {
        guard !edits.isEmpty || !removing.isEmpty || !removingFromBoard.isEmpty else {
            return catalogueJSON(fixture)
        }
        guard var object = catalogueObject(fixture), var board = object["board"] as? [String: Any] else {
            return nil
        }
        board.merge(edits) { $1 }
        for key in removingFromBoard {
            board.removeValue(forKey: key)
        }
        object["board"] = board
        for key in removing {
            object.removeValue(forKey: key)
        }
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return nil
        }
        return String(decoding: data, as: UTF8.self)
    }

    /// A suite catalogue, read as plain JSON for comparing with what the tab shows.
    static func catalogueObject(_ fixture: String) -> [String: Any]? {
        guard let json = catalogueJSON(fixture) else {
            return nil
        }
        return (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any]
    }
}
