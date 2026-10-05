// NereusSDR for iOS: Mic Gain in the TX panel under the mic level meter, against a fake Core, with pictures
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

/// JJ, 2026-09-30: Mic Gain in the TX panel, directly under the mic level
/// meter, as the desktop's Phone/CW applet has it. The TX panel's row and
/// the Modes tab's are the one value in ``ModesTabModel``: they send the
/// same write, show the same value after the Core's answer, and grey
/// together. Each row is moved as VoiceOver moves it, through its
/// accessibility element. With `NEREUS_MAIN_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the panel's pictures are
/// written there.
@Suite("Mic Gain in the TX panel", .serialized)
@MainActor
struct TxMicGainTests {
    private let platform = TestPlatform()
    /// Writes already answered, so a later wait takes the next one.
    private let answered = AnsweredWrites()

    // MARK: Against a fake Core

    @Test("the TX panel's Mic Gain sends the Modes row's write, and both rows show the Core's answer")
    func sameWriteSameValue() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        let step = modes.micGainRange.step > 0 ? modes.micGainRange.step : 1
        try await onScreen(model) { window in
            let panelRow = try #require(SetupTypedEntryTests.element("txMicGain", in: window))
            let modesRow = try #require(SetupTypedEntryTests.element("modesMicGain", in: window))
            #expect(panelRow.accessibilityLabel == "Microphone gain")
            #expect(modesRow.accessibilityLabel == "Microphone gain")
            #expect(await rowsRead("-6", in: window))
            #expect(await hints("", in: window))

            // The TX panel's row moves one step: the Core's transmit object
            // is written, and after its answer both rows read the new value.
            panelRow.accessibilityIncrement()
            let fromPanel = try #require(await answer(station, "micGainDb"))
            #expect(fromPanel.key == "transmit")
            #expect(fromPanel.properties.map(\.name) == ["micGainDb"])
            #expect(fromPanel.properties.first?.value == .i64(Int64(-6 + step)))
            #expect(await ShotWait.until { modes.micGainDb == -6 + step })
            #expect(await rowsRead(Self.text(-6 + step), in: window))

            // The Modes row moves the same way and sends the same write.
            try #require(SetupTypedEntryTests.element("modesMicGain", in: window)).accessibilityIncrement()
            let fromModes = try #require(await answer(station, "micGainDb"))
            #expect(fromModes.key == fromPanel.key)
            #expect(fromModes.properties.map(\.name) == fromPanel.properties.map(\.name))
            #expect(fromModes.properties.map(\.ordinal) == fromPanel.properties.map(\.ordinal))
            #expect(fromModes.properties.first?.value == .i64(Int64(-6 + 2 * step)))
            #expect(await ShotWait.until { modes.micGainDb == -6 + 2 * step })
            #expect(await rowsRead(Self.text(-6 + 2 * step), in: window))

            // The Core's own change, from another device, moves both rows.
            await station.deliver(Self.micGain(12))
            #expect(await ShotWait.until { modes.micGainDb == 12 })
            #expect(await rowsRead("12", in: window))
        }
        await model.disconnect()
    }

    @Test("Mic Gain greys with the mute's hint while the Core's mic is muted, and sends nothing")
    func greyedWhileMuted() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        #expect(transmit.settingsVersion >= 10)
        #expect(MicGainRow.range(modes, transmit) != nil)
        await station.deliver(TxStageMetersTests.micMuted(true))
        #expect(await ShotWait.until { transmit.micMuted })
        #expect(MicGainRow.range(modes, transmit) == nil)
        try await onScreen(model) { window in
            #expect(await hints(TransmitModel.micMutedText, in: window))
            let before = station.messages.count
            try #require(SetupTypedEntryTests.element("txMicGain", in: window)).accessibilityIncrement()
            try #require(SetupTypedEntryTests.element("modesMicGain", in: window)).accessibilityIncrement()
            await idle()
            #expect(!Self.wroteMicGain(station.messages.dropFirst(before)))
            #expect(modes.micGainDb == -6)

            // Unmuted, the row is live again and the hint goes.
            await station.deliver(TxStageMetersTests.micMuted(false))
            #expect(await ShotWait.until { !transmit.micMuted })
            #expect(MicGainRow.range(modes, transmit) != nil)
            #expect(await hints("", in: window))
        }
        await model.disconnect()
    }

    /// Below transmitSettingsVersion 13 (the fixture's Core is at 15, so
    /// this one says 12); at 13 and above Mic Gain moves on the air
    /// (``movesOnTheAirAtVersion13()``).
    @Test("below transmitSettingsVersion 13, Mic Gain greys while the Core's radio is on the air, with the panel's reason, and sends nothing")
    func greyedWhileNotEditable() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        await Self.takesOnAir(station, model, version: Self.onAirVersion - 1)
        #expect(await ShotWait.until { transmit.settingsVersion == Self.onAirVersion - 1 })
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        #expect(await ShotWait.until { transmit.coreOnAir })
        #expect(!transmit.settingsEditable(ModesTabModel.chainVersion))
        #expect(MicGainRow.range(modes, transmit) == nil)
        try await onScreen(model) { window in
            // The panel says why above its settings; the row carries no mute hint.
            #expect(await ShotWait.until { SetupTypedEntryTests.element("txSettingsReason", in: window) != nil })
            #expect(await hints("", in: window))
            let before = station.messages.count
            try #require(SetupTypedEntryTests.element("txMicGain", in: window)).accessibilityIncrement()
            await idle()
            #expect(!Self.wroteMicGain(station.messages.dropFirst(before)))
            #expect(modes.micGainDb == -6)
        }
        await model.disconnect()
    }

    @Test("a greyed Mic Gain keeps its thumb where its value is, muted or on the air, as the live row draws it")
    func greyedThumbKeepsItsPlace() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        // On the air greys it below transmitSettingsVersion 13; the fixture's Core is at 15.
        await Self.takesOnAir(station, model, version: Self.onAirVersion - 1)
        #expect(await ShotWait.until { transmit.settingsVersion == Self.onAirVersion - 1 })
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        let row = { MicGainRow(model: modes, transmit: transmit, identifier: "txMicGain") }
        let live = try #require(Self.thumbCentre(row()))
        // -6 dB on the catalogue's range is well clear of the track's left end.
        #expect(live > Self.trackStart + Self.thumb / 2 + 20, "live thumb at \(live)")

        await station.deliver(TxStageMetersTests.micMuted(true))
        #expect(await ShotWait.until { transmit.micMuted && MicGainRow.range(modes, transmit) == nil })
        let muted = try #require(Self.thumbCentre(row()))
        #expect(abs(muted - live) <= 0.5, "muted thumb at \(muted), live at \(live)")

        await station.deliver(TxStageMetersTests.micMuted(false))
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        #expect(await ShotWait.until { !transmit.micMuted && transmit.coreOnAir })
        #expect(MicGainRow.range(modes, transmit) == nil)
        let onAir = try #require(Self.thumbCentre(row()))
        #expect(abs(onAir - live) <= 0.5, "on-air thumb at \(onAir), live at \(live)")
        await model.disconnect()
    }

    /// A Core at transmitSettingsVersion 13 takes the transmit settings
    /// while its radio is on the air, as a local window does, and the
    /// desktop's remote window sends them then (MainWindow.cpp:15254-15266,
    /// kTransmitSettingsOnAirVersion 13 in StationCapabilities.h:248). The
    /// phone does the same: keyed, Mic Gain stays live and is written.
    @Test("at transmitSettingsVersion 13 Mic Gain moves while the Core's radio is on the air, and is written")
    func movesOnTheAirAtVersion13() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        await Self.takesOnAir(station, model)
        #expect(await ShotWait.until { transmit.settingsVersion == Self.onAirVersion })
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        #expect(await ShotWait.until { transmit.coreOnAir })
        #expect(transmit.settingsEditable(ModesTabModel.chainVersion))
        #expect(transmit.settingsReason(1) == nil && transmit.settingsReason(ModesTabModel.chainVersion) == nil)
        #expect(MicGainRow.range(modes, transmit) != nil)
        let step = modes.micGainRange.step > 0 ? modes.micGainRange.step : 1
        try await onScreen(model) { window in
            #expect(SetupTypedEntryTests.element("txSettingsReason", in: window) == nil)
            try #require(SetupTypedEntryTests.element("txMicGain", in: window)).accessibilityIncrement()
            let fromPanel = try #require(await answer(station, "micGainDb"))
            #expect(fromPanel.properties.first?.value == .i64(Int64(-6 + step)))
            #expect(await ShotWait.until { modes.micGainDb == -6 + step })
            #expect(await rowsRead(Self.text(-6 + step), in: window))
            try #require(SetupTypedEntryTests.element("modesMicGain", in: window)).accessibilityIncrement()
            let fromModes = try #require(await answer(station, "micGainDb"))
            #expect(fromModes.properties.first?.value == .i64(Int64(-6 + 2 * step)))
            #expect(await rowsRead(Self.text(-6 + 2 * step), in: window))
        }
        // The rest of the chain the desktop changes on the air at 13.
        transmit.setVoxThreshold(-30)
        #expect(try #require(await answer(station, "voxThresholdDb")).properties.first?.value == .i64(-30))
        transmit.setRfPower(20)
        #expect(try #require(await answer(station, "power")).properties.first?.value == .i64(20))
        await model.disconnect()
    }

    /// Below version 13 the Core refuses them on the air, so the phone keeps
    /// them greyed with the reason then, as the desktop does.
    @Test("at transmitSettingsVersion 12 Mic Gain still waits while the Core's radio is on the air")
    func waitsOnTheAirBelowVersion13() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        await Self.takesOnAir(station, model, version: Self.onAirVersion - 1)
        #expect(await ShotWait.until { transmit.settingsVersion == Self.onAirVersion - 1 })
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        #expect(await ShotWait.until { transmit.coreOnAir })
        #expect(!transmit.settingsEditable(ModesTabModel.chainVersion))
        #expect(transmit.settingsReason(ModesTabModel.chainVersion) == TransmitModel.onAirText)
        #expect(MicGainRow.range(modes, transmit) == nil)
        let before = station.messages.count
        modes.setMicGain(0)
        await idle()
        #expect(!Self.wroteMicGain(station.messages.dropFirst(before)))
        await model.disconnect()
    }

    /// A Core that refuses a Mic Gain write says why, and the words show
    /// where the control is: under the Transmit section and in the TX
    /// panel, not only on the flag menus. The next accepted write clears them.
    @Test("a refused Mic Gain write shows the Core's words in the Transmit section and the TX panel")
    func refusalShowsWhereTheControlIs() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        let words = "Choose a mic level from -50 to 70 dB."
        try await onScreen(model) { window in
            try #require(SetupTypedEntryTests.element("txMicGain", in: window)).accessibilityIncrement()
            #expect(await answer(station, "micGainDb", refuse: words) != nil)
            #expect(await ShotWait.until {
                window.layoutIfNeeded()
                return ["txPanelNote", "modesTransmitNote"].allSatisfy {
                    SetupTypedEntryTests.element($0, in: window)?.accessibilityLabel == words
                }
            })
            #expect(await rowsRead("-6", in: window))
            try #require(SetupTypedEntryTests.element("modesMicGain", in: window)).accessibilityIncrement()
            #expect(await answer(station, "micGainDb") != nil)
            #expect(await ShotWait.until {
                window.layoutIfNeeded()
                return ["txPanelNote", "modesTransmitNote"].allSatisfy {
                    SetupTypedEntryTests.element($0, in: window) == nil
                }
            })
        }
        await model.disconnect()
    }

    /// The desktop's Phone/CW applet, before its remote window has the
    /// Core's mic gain, shows TransmitModel's -6 dB (TransmitModel.h:2789,
    /// PhoneCwApplet.cpp:913-917). The phone does the same: the box reads
    /// -6, the thumb sits at -6 dB on the range, greyed or not by the same
    /// rule as with the Core's value; the Core's own value replaces it.
    @Test("before the Core sends its mic gain the row shows the desktop's -6 dB, thumb and box, until the Core's value")
    func noValueFromTheCore() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let modes = model.main.modes
        let transmit = model.main.transmit
        #expect(await ShotWait.until { modes.micGainDb == -6 && transmit.settingsEditable(ModesTabModel.chainVersion) })
        let row = { MicGainRow(model: modes, transmit: transmit, identifier: "txMicGain") }
        let fromCore = try #require(Self.thumbCentre(row()))

        // The Core's transmit object again, without its mic gain.
        await TransmitScreenTests.fillTransmit(station)
        #expect(await ShotWait.until { modes.micGainDb == nil })
        #expect(MicGainRow.beforeCoreDb == -6)
        #expect(MicGainRow.shown(modes) == -6)
        let before = try #require(Self.thumbCentre(row()))
        #expect(abs(before - fromCore) <= 0.5, "thumb with no value at \(before), at the Core's -6 at \(fromCore)")
        // Not at the track's left end, where the row drew it before.
        #expect(before > Self.trackStart + Self.thumb / 2 + 20, "thumb with no value at \(before)")
        try await onScreen(model) { window in
            #expect(await rowsRead("-6", in: window))

            // Greyed by the same rule as with a value: muted, it keeps its place.
            await station.deliver(TxStageMetersTests.micMuted(true))
            #expect(await ShotWait.until { transmit.micMuted && MicGainRow.range(modes, transmit) == nil })
            let muted = try #require(Self.thumbCentre(row()))
            #expect(abs(muted - before) <= 0.5, "muted thumb with no value at \(muted)")
            #expect(await rowsRead("-6", in: window))
            #expect(await hints(TransmitModel.micMutedText, in: window))

            // The Core's value replaces the desktop's.
            await station.deliver(Self.micGain(12))
            #expect(await ShotWait.until { modes.micGainDb == 12 })
            #expect(await rowsRead("12", in: window))
        }
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("pictures: the TX panel light and dark, large type, a muted mic, and the iPad's applet column")
    func shots() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessoryOperate, .txStageReadings],
                                                   catalogue: false)
        let transmit = model.main.transmit
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await ShotWait.until { transmit.permitted })
        try await MainScreenShotTests.fill(station)
        #expect(await ShotWait.until { model.main.slices.entries.count == 2 })
        await TransmitScreenTests.fillTransmit(station)
        await station.deliver(Self.micGain(-6))
        #expect(await ShotWait.until {
            transmit.amp != nil && transmit.rfPower == 100 && model.main.modes.micGainDb == -6
        })
        await station.deliver(TransmitScreenTests.txStateDelta([.init(ordinal: 0, name: "keyed", value: .bool(false))]
            + TxStageMetersTests.stages(TxStageMetersTests.idleReadings)))
        #expect(await ShotWait.until { transmit.stageReadings[0] == -18.5 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        try await shootPanel("txmicgain-portrait-dark", model: model, scheme: .dark)
        try await shootPanel("txmicgain-portrait-light", model: model, scheme: .light)
        try await shootPanel("txmicgain-portrait-large-type", model: model, scheme: .dark, largeText: true)
        try await shootColumn("txmicgain-ipad-column", model: model)

        await station.deliver(TxStageMetersTests.micMuted(true))
        #expect(await ShotWait.until { transmit.micMuted })
        try await shootPanel("txmicgain-portrait-mic-muted", model: model, scheme: .dark)
        await model.disconnect()
    }

    /// Mic Gain moved while the radio is on the air, against a Core at
    /// transmitSettingsVersion 13: the row is live, no on-air reason shows,
    /// and the Core's answer moves the row while this phone holds a PTT key.
    @Test("pictures: Mic Gain moved while keyed, at transmitSettingsVersion 13, light and dark")
    func shotsKeyed() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessoryOperate, .txStageReadings],
                                                   catalogue: false)
        let transmit = model.main.transmit
        let modes = model.main.modes
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await ShotWait.until { transmit.permitted })
        try await MainScreenShotTests.fill(station)
        #expect(await ShotWait.until { model.main.slices.entries.count == 2 })
        await TransmitScreenTests.fillTransmit(station)
        await station.deliver(Self.micGain(-6))
        #expect(await ShotWait.until {
            transmit.amp != nil && transmit.rfPower == 100 && modes.micGainDb == -6
        })
        await Self.takesOnAir(station, model)
        #expect(await ShotWait.until { transmit.settingsVersion == Self.onAirVersion })
        // Key through the same PTT path as a tap on the phone; setting the
        // Core's keyed reading alone leaves the local PTT showing Tap.
        transmit.tapPtt()
        await TransmitScreenTests.barrier(model, station)
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 19, name: "transmitting", value: .bool(true)),
        ])))
        await station.deliver(TransmitScreenTests.txStateDelta([.init(ordinal: 0, name: "keyed", value: .bool(true))]
            + TxStageMetersTests.stages(TxStageMetersTests.idleReadings)))
        #expect(await ShotWait.until { transmit.coreOnAir && transmit.stageReadings[0] == -18.5 })
        #expect(station.keyed, "the shot's fake Core holds the phone's PTT key")
        #expect(transmit.ptt.pttKeyed && transmit.ptt.state.isKeyed)
        #expect(PttButton.look(transmit.ptt) == .keyed)
        #expect(MicGainRow.range(modes, transmit) != nil)
        // Without the catalogue the row's range is the fallback, up to 10 dB.
        modes.setMicGain(6)
        #expect(try #require(await answer(station, "micGainDb")).properties.first?.value == .i64(6))
        #expect(await ShotWait.until { modes.micGainDb == 6 })
        #expect(station.keyed && transmit.ptt.pttKeyed && transmit.ptt.state.isKeyed,
                "moving Mic Gain leaves the phone's PTT key held")
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        try await shootPanel("txmicgain-keyed-v13-portrait-dark", model: model, scheme: .dark)
        try await shootPanel("txmicgain-keyed-v13-portrait-light", model: model, scheme: .light)
        await model.disconnect()
    }

    // MARK: The fake Core

    /// The desktop's kTransmitSettingsOnAirVersion (StationCapabilities.h:248).
    static let onAirVersion: Int64 = 13

    /// The Core's capabilities again, at transmitSettingsVersion `version`.
    static func takesOnAir(_ station: FakeStation, _ model: AppModel, version: Int64 = onAirVersion) async {
        var capabilities = model.mirror.capabilities
        capabilities[TransmitModel.settingsCapability] = .int(version)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
    }

    static func micGain(_ db: Int64) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "transmit", properties: [.init(ordinal: 27, name: "micGainDb", value: .i64(db))]))
    }

    static func text(_ db: Double) -> String {
        String(Int(db.rounded()))
    }

    static func wroteMicGain<S: Sequence>(_ messages: S) -> Bool where S.Element == LinkMessage {
        messages.contains { message in
            guard case .propertyWrite(let write) = message else {
                return false
            }
            return write.properties.contains { $0.name == "micGainDb" }
        }
    }

    /// A model connected to a fake Core, with the Modes suite's ANAN
    /// catalogue (Mic Gain's range) unless `catalogue` is false.
    private func connected(additions: FakeStation.Additions,
                           catalogue: Bool = true) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "TxMicGainTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected })
        if catalogue {
            let json = try #require(ModesTabBindingTests.catalogueJSON(ModesTabBindingTests.anan))
            await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(json)),
                .init(ordinal: 1, name: "revision", value: .i64(2)),
            ])))
            #expect(await ShotWait.until { !model.main.modes.modes.isEmpty })
        }
        return (model, station)
    }

    /// Waits for the app's next write of the transmit object's `property`,
    /// then answers it as the Core does: taken, with a delta of the value,
    /// or refused with `reason` and the value the Core kept (-6).
    private func answer(_ station: FakeStation, _ property: String,
                        refuse reason: String? = nil) async -> LinkMessage.PropertyWrite? {
        let done = answered.ids
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "transmit" && write.properties.first?.name == property
                    && !done.contains(write.writeId ?? 0)
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId,
              let entry = write.properties.first else {
            Issue.record("no write of transmit.\(property) reached the Core")
            return nil
        }
        answered.ids.insert(writeId)
        if let reason {
            let kept = LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: property, value: .i64(-6))
            await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "transmit", writeId: writeId, results: [
                .init(property: property, accepted: false, reason: reason, value: kept),
            ])))
            return write
        }
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "transmit", writeId: writeId, results: [
            .init(property: property, accepted: true, reason: "", value: entry),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [entry])))
        return write
    }

    /// Lets anything the app might send go.
    private func idle() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    // MARK: On screen

    /// Both rows read `value`, waited for: the Core's answer reaches the
    /// model, and the rows follow on the main queue's later turns.
    private func rowsRead(_ value: String, in window: UIWindow) async -> Bool {
        await ShotWait.until {
            window.layoutIfNeeded()
            return ["txMicGain", "modesMicGain"].allSatisfy {
                SetupTypedEntryTests.element($0, in: window)?.accessibilityValue == value
            }
        }
    }

    /// Both rows carry the hint `hint` ("" for none), waited for.
    private func hints(_ hint: String, in window: UIWindow) async -> Bool {
        await ShotWait.until {
            window.layoutIfNeeded()
            return ["txMicGain", "modesMicGain"].allSatisfy {
                (SetupTypedEntryTests.element($0, in: window)?.accessibilityHint ?? "") == hint
            }
        }
    }

    /// The TX panel and the Modes tab's Transmit section in one window,
    /// with application accessibility on, while `check` runs.
    private func onScreen(_ model: AppModel, _ check: (UIWindow) async throws -> Void) async throws {
        let main = model.main
        let window = try BandFlagShotTests.window(size: CGSize(width: TxPanel.width, height: 2_600))
        let root = VStack(spacing: 0) {
            TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel,
                    modes: main.modes, meters: nil, scrolls: false)
            TransmitSection(model: main.modes, transmit: main.transmit, micLevel: main.micLevel)
        }
        let host = UIHostingController(rootView: root)
        host.view.frame = window.bounds
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasOn)
        }
        await ShotWait.laidOut(window)
        try await check(window)
    }

    // MARK: Drawing

    /// The main screen at this simulator's size with the TX panel open,
    /// scrolled so Mic level, Mic Gain and the stage readings show.
    private func shootPanel(_ name: String, model: AppModel, scheme: ColorScheme,
                            largeText: Bool = false) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let size = scene.screen.bounds.size
        let window = try BandFlagShotTests.window(size: size)
        let root = MicGainShotRoot(model: model)
            .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
            .preferredColorScheme(scheme)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.view.frame = CGRect(origin: .zero, size: size)
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasOn)
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
        try await scrollToRow(window, panelWidth: TxPanel.width, lead: largeText ? 170 : 150)
        try write(name, render(window))
    }

    /// The iPad's applet column, as the column draws it, scrolled to Mic Gain.
    private func shootColumn(_ name: String, model: AppModel) async throws {
        let size = CGSize(width: AppletColumn.width, height: 834)
        let window = try BandFlagShotTests.window(size: size)
        let root = AppletColumn(main: model.main)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasOn)
        }
        await ShotWait.laidOut(window)
        try await scrollToRow(window, panelWidth: AppletColumn.width, lead: 200)
        try write(name, render(window))
    }

    /// Scrolls the panel's scroll view so the TX panel's Mic Gain sits
    /// `lead` points below the scroll view's top, with Mic level above it.
    private func scrollToRow(_ window: UIWindow, panelWidth: CGFloat, lead: CGFloat) async throws {
        await ShotWait.laidOut(window)
        let scrolls = Self.scrollViews(in: window).filter {
            $0.contentSize.height > $0.bounds.height + 1 && abs($0.contentSize.width - panelWidth) < 24
        }
        let all = Self.scrollViews(in: window).map { "\($0.bounds.size) in \($0.contentSize)" }
        let scroll = try #require(scrolls.first, "no scrolling panel \(panelWidth) wide among \(all)")
        let row = try #require(SetupTypedEntryTests.element("txMicGain", in: window))
        let frame = scroll.convert(scroll.bounds, to: nil)
        let furthest = max(scroll.contentSize.height - scroll.bounds.height, 0)
        let target = min(max(scroll.contentOffset.y + row.accessibilityFrame.minY - (frame.minY + lead), 0), furthest)
        scroll.setContentOffset(CGPoint(x: 0, y: target), animated: false)
        await ShotWait.laidOut(window)
        let moved = try #require(SetupTypedEntryTests.element("txMicGain", in: window))
        #expect(frame.contains(moved.accessibilityFrame), "Mic Gain at \(moved.accessibilityFrame), panel \(frame)")
    }

    /// The row's label column and the gap after it, where the track starts.
    static let trackStart: CGFloat = 62 + 8
    /// The slider's thumb diameter.
    static let thumb: CGFloat = 22

    /// Where `row`'s thumb is centred, in points from the row's left edge:
    /// drawn 300 points wide on black, the middle of the lit pixels on the
    /// line 6 points off the track's middle, where only the thumb is drawn.
    static func thumbCentre(_ row: some View) -> CGFloat? {
        let size = CGSize(width: 300, height: 30)
        let scale: CGFloat = 3
        let renderer = ImageRenderer(content: row.frame(width: size.width, height: size.height).background(Color.black))
        renderer.scale = scale
        guard let image = renderer.cgImage else {
            return nil
        }
        let width = image.width
        let height = image.height
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        let drawn = pixels.withUnsafeMutableBytes { buffer -> Bool in
            guard let context = CGContext(data: buffer.baseAddress, width: width, height: height, bitsPerComponent: 8,
                                          bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
                return false
            }
            context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        guard drawn else {
            return nil
        }
        // Clear of the 4-point track, inside the 22-point thumb.
        let line = Int(((size.height / 2 + 6) * scale).rounded())
        let first = Int(trackStart * scale)
        let last = width - Int(60 * scale)
        var lit: [Int] = []
        for x in first..<last {
            let index = (line * width + x) * 4
            if Int(pixels[index]) + Int(pixels[index + 1]) + Int(pixels[index + 2]) > 60 {
                lit.append(x)
            }
        }
        guard !lit.isEmpty else {
            return nil
        }
        return (CGFloat(lit.reduce(0, +)) / CGFloat(lit.count) + 0.5) / scale
    }

    private func render(_ window: UIWindow) -> UIImage {
        UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
    }

    private func write(_ name: String, _ image: UIImage) throws {
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        if let scroll = view as? UIScrollView {
            found.append(scroll)
        }
        for child in view.subviews {
            found += scrollViews(in: child)
        }
        return found
    }
}

/// The write ids a test has answered.
@MainActor
private final class AnsweredWrites {
    var ids: Set<UInt32> = []
}

/// The app's root as `RootView` lays it out, with the TX panel open.
private struct MicGainShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: true)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
