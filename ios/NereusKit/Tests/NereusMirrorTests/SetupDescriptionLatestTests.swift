// NereusSDR for iOS: Setup description versions 18 to 24: the HL2 clock rows, TCI Forget, the on-air rows and TX Input's radio mic rows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18: the phone reads Setup description 22. Hardware 18 opens the
/// HL2 clock rows (CL2's frequency follows Enable CL2), DSP 19 adds the CFC
/// band editor, PA 20 puts the profile rows' on-air state on each row,
/// CAT & Network 21 greys TCI's Forget row while Duplicate is off, and DSP
/// 22 says why the RX buffer rows are locked on the air. Hardware 23 adds
/// Calibration's Rx1 6m LNA row; Audio 24 moves Line In Gain to 1.5 dB
/// steps and adds the Saturn G2's Mic Tip-Ring row. HL2 Swap audio channels
/// opens in place at 16 once the Core sends it without its closed reason.
@MainActor
@Suite struct SetupDescriptionLatestTests {
    // The Core's rows as its resources hold them (resources/setup).
    static let cl2Enable = #"{"id":"hardware.hl2Io.cl2Enable","label":"Enable CL2","tooltip":"Enable frequency output on CL2","kind":"toggle","binding":{"radioSetting":"hl2/cl2Enable"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"default":false}"#
    static let cl2Freq = #"{"id":"hardware.hl2Io.cl2Freq","label":"CL2 frequency","tooltip":"Output frequency on CL2 output","kind":"decimal","binding":{"radioSetting":"hl2/cl2FreqMHz"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"min":1,"max":200,"step":0.1,"decimals":3,"unit":"MHz","enabledWhen":{"radioSetting":"hl2/cl2Enable","oneOf":[true]},"default":116}"#
    static let ext10MHz = #"{"id":"hardware.hl2Io.ext10MHz","label":"External 10 MHz reference","tooltip":"Enable external 10 MHz input on CL1","kind":"toggle","binding":{"radioSetting":"hl2/ext10MHz"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"default":false}"#
    static let forget = #"{"id":"catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect","label":"Forget RX2 VFO B","tooltip":"t","kind":"toggle","binding":{"command":{"verb":"setStationTciSettings","valueProperty":{"object":"stationTci","name":"forgetRx2VfoBOnDisconnect"},"arguments":{"forgetRx2VfoBOnDisconnect":{"$controlValue":true}}}},"applies":"live","gate":{"capability":"stationTciSettingsVersion","min":1},"enabledWhen":{"property":{"object":"stationTci","name":"copyRx2VfobToVfoa"},"oneOf":[true]}}"#
    static let forgetDependency = #","enabledWhen":{"property":{"object":"stationTci","name":"copyRx2VfobToVfoa"},"oneOf":[true]}"#
    static let duplicate = #"{"id":"catNetwork.tciServer.core.copyRx2VfobToVfoa","label":"Duplicate RX2 VFO B to RX2 VFO A","tooltip":"t","kind":"toggle","binding":{"command":{"verb":"setStationTciSettings","valueProperty":{"object":"stationTci","name":"copyRx2VfobToVfoa"},"arguments":{"copyRx2VfobToVfoa":{"$controlValue":true}}}},"applies":"live","gate":{"capability":"stationTciSettingsVersion","min":1}}"#
    static let rxBuffer = #"{"id":"dsp.options.DspOptionsBufferSizePhoneRx","label":"RX:","tooltip":"t","kind":"choice","binding":{"setting":"DspOptionsBufferSizePhoneRx"},"applies":"live","choices":["64","128","256","512","1024"],"availability":{"enabled":false,"reason":"Can't change while transmitting."}}"#
    static let onAir = #","availability":{"enabled":false,"reason":"Can't change while transmitting."}"#

    static func category(_ id: String, version: Int, _ controls: [String]) -> String {
        #"{"version":\#(version),"category":{"id":"\#(id)","title":"T","where":"mixed"},"pages":[{"id":"\#(id).p","title":"P","where":"mixed","sections":[{"title":"S","controls":["#
            + controls.joined(separator: ",") + "]}]}]}"
    }

    static func controls(_ json: String) throws -> [SetupDescription.Control] {
        try SetupDescription.parse(json: json).pages.flatMap(\.sections).flatMap(\.controls)
    }

    /// The version 16 clock rows a Core below 18 sends, closed.
    static var closedClockRows: [String] {
        SetupDescriptionTests.hardwareV16ClockRows.sorted { $0.key < $1.key }.map { entry in
            let data = (try? JSONSerialization.data(withJSONObject: entry.value)) ?? Data()
            return String(decoding: data, as: UTF8.self)
        }
    }

    // MARK: What the phone asks for

    @Test func thePhoneAsksForTwentyFourAndReadsEachCategoryAtTheCoresVersion() {
        #expect(SetupDescription.highestVersion == 24)
        #expect(LinkFeatures.app["setupDescription"] == 24)
        #expect(LinkFeatures.app["radioMic"] == 1)
        #expect(LinkFeatures.app["cfcProfile"] == 1)
        #expect(LinkFeatures.app["levelCalibration"] == 1)
        #expect(LinkFeatures.app["adcAttenuators"] == 1)
        #expect(LinkFeatures.app["rx2Attenuator"] == 1)
        // The Core's SetupDescription::fitCategoryForVersion, for the caps
        // older and current Cores send.
        let expected: [String: [Int64: Int]] = [
            "hardware": [15: 13, 16: 16, 17: 17, 18: 18, 19: 18, 21: 18, 22: 18, 23: 23, 24: 23],
            "pa": [15: 14, 16: 14, 17: 14, 18: 14, 19: 14, 20: 20, 21: 20, 22: 20, 24: 20],
            "transmit": [15: 15, 16: 15, 17: 15, 18: 15, 19: 15, 21: 15, 22: 15, 24: 15],
            "dsp": [15: 15, 16: 15, 17: 15, 18: 15, 19: 19, 21: 19, 22: 22, 24: 22],
            "catNetwork": [15: 15, 16: 15, 17: 15, 18: 15, 19: 15, 20: 15, 21: 21, 22: 21, 24: 21],
            "audio": [14: 3, 15: 15, 16: 15, 17: 15, 18: 15, 19: 15, 21: 15, 22: 15, 23: 15, 24: 24],
            "diagnostics": [15: 15, 16: 15, 17: 15, 18: 15, 19: 15, 21: 15, 22: 15, 24: 15],
            "appearance": [15: 12, 22: 12, 24: 12],
            "display": [15: 12, 22: 12, 24: 12],
            "general": [15: 3, 22: 3, 24: 3],
        ]
        for (category, caps) in expected {
            for (cap, version) in caps {
                #expect(SetupDescriptionFeed.projectedVersion(maximum: cap, category: category) == version,
                        "\(category) at \(cap)")
            }
        }
        #expect(SetupDescriptionFeed.projectedVersion(maximum: 25, category: "pa") == nil)
        #expect(SetupDescriptionFeed.projectedVersion(maximum: 25, category: "audio") == nil)
    }

    // MARK: Reading the rows

    @Test func hardwareEighteensClockRowsAreLiveAndTheFrequencyFollowsEnableCL2() throws {
        let rows = try Self.controls(Self.category("hardware", version: 18, [Self.cl2Enable, Self.cl2Freq, Self.ext10MHz]))
        #expect(rows.allSatisfy { $0.metadataIssue == nil && $0.unavailableReason == nil && $0.availability == nil })
        let frequency = rows[1]
        #expect(frequency.kind == .decimal)
        #expect(frequency.range == SetupDescription.Range(minimum: 1, maximum: 200, step: 0.1))
        #expect(frequency.decimals == 3)
        #expect(frequency.unit == "MHz")
        #expect(frequency.defaultValue == .decimal(116))
        #expect(frequency.modern?.radioSettingDependency
                == SetupDescription.RadioSettingDependency(path: "hl2/cl2Enable", oneOf: [.bool(true)]))
        #expect(rows[0].defaultValue == .bool(false))
        #expect(rows[2].binding == .radioSetting("hl2/ext10MHz"))
    }

    /// Hardware 23 (Level Cal 2): Calibration's Rx1 6m LNA row, 0 to 25 dB
    /// with 13 by default, live; a row asking for 23 in an older category
    /// is not read.
    @Test func hardwareTwentyThreesRx1SixMetreLnaRowIsLive() throws {
        let rows = try Self.controls(Self.category("hardware", version: 23, [Self.rx1SixMetreLna]))
        let lna = try #require(rows.first)
        #expect(lna.id == "hardware.calibration.rx1_6mLna")
        #expect(lna.metadataIssue == nil && lna.unavailableReason == nil && lna.availability == nil)
        #expect(lna.kind == .decimal)
        #expect(lna.label == "Rx1 6m LNA:")
        #expect(lna.range == SetupDescription.Range(minimum: 0, maximum: 25, step: 1))
        #expect(lna.decimals == 1)
        #expect(lna.unit == "dB")
        #expect(lna.defaultValue == .decimal(13))
        #expect(lna.binding == .radioSetting("cal/rx1_6mLna"))
        let older = try Self.controls(Self.category("hardware", version: 18, [Self.rx1SixMetreLna]))
        #expect(older.first?.metadataIssue == "Unsupported Setup control version.")
    }

    /// Audio 24 (the radio codec lane): Line In Gain in the radio's 1.5 dB
    /// steps from -34.5 with one decimal, and the Saturn G2's Mic Tip-Ring,
    /// both live; a row asking for 24 in an older category is not read,
    /// and the version 15 row an older peer keeps still reads.
    @Test func audioTwentyFoursLineInGainStepsAndSaturnTipRingAreLive() throws {
        let rows = try Self.controls(Self.category("audio", version: 24, [Self.lineInGain, Self.saturnTipRing]))
        #expect(rows.allSatisfy { $0.metadataIssue == nil && $0.unavailableReason == nil && $0.availability == nil })
        let gain = rows[0]
        #expect(gain.id == "audio.txInput.hermesLineInGain" && gain.label == "Line In Gain:")
        #expect(gain.kind == .decimal)
        #expect(gain.range == SetupDescription.Range(minimum: -34.5, maximum: 12, step: 1.5))
        #expect(gain.decimals == 1)
        #expect(gain.unit == "dB")
        #expect(gain.binding == .property(.init(object: "transmit", name: "lineInBoost")))
        let tipRing = rows[1]
        #expect(tipRing.id == "audio.txInput.saturnMicTipRing" && tipRing.label == "Mic Tip-Ring (Tip is Mic)")
        #expect(tipRing.kind == .toggle)
        #expect(tipRing.binding == .property(.init(object: "transmit", name: "micTipRing")))
        let older = try Self.controls(Self.category("audio", version: 15, [Self.lineInGain, Self.saturnTipRing]))
        #expect(older.allSatisfy { $0.metadataIssue == "Unsupported Setup control version." })
        let fifteen = try #require(try Self.controls(Self.category("audio", version: 15, [Self.lineInGainV15])).first)
        #expect(fifteen.metadataIssue == nil)
        #expect(fifteen.range == SetupDescription.Range(minimum: -34, maximum: 12, step: 1))
        #expect(fifteen.decimals == nil)
    }

    /// HL2 Swap audio channels at 16: the Core now sends it open, with its
    /// tooltip and no availability, and the phone reads a live switch; an
    /// older Core's closed row still reads, greyed with that Core's reason.
    @Test func hl2SwapAudioChannelsReadsOpenAndItsOldClosedShapeStillReads() throws {
        let open = try #require(try Self.controls(Self.category("hardware", version: 23, [Self.swapAudioChannels])).first)
        #expect(open.id == "hardware.hl2Io.swapAudioChannels")
        #expect(open.metadataIssue == nil && open.unavailableReason == nil && open.availability == nil)
        #expect(open.kind == .toggle)
        #expect(open.tooltip == "Swap the audio channels sent to the HL2")
        #expect(open.binding == .radioSetting("hl2/swapAudioChannels"))
        #expect(open.defaultValue == .bool(false))
        let closed = try #require(try Self.controls(Self.category("hardware", version: 18, [Self.swapAudioChannelsClosed])).first)
        #expect(closed.metadataIssue == nil)
        #expect(closed.availability == SetupDescription.Availability(enabled: false, reason: Self.swapClosedReason))
        #expect(closed.tooltip.isEmpty)
    }

    /// The rows as the Core's resources/setup/audio.json and its version 24
    /// table (SetupDescriptionService.cpp's kAudioV24Controls) hold them.
    static let lineInGain = #"{"id":"audio.txInput.hermesLineInGain","label":"Line In Gain:","tooltip":"","kind":"decimal","binding":{"property":{"object":"transmit","name":"lineInBoost"}},"applies":"live","requiresDescriptionVersion":24,"gate":{"capability":"transmitSettingsVersion","min":3,"transmit":true},"min":-34.5,"max":12,"step":1.5,"decimals":1,"unit":"dB"}"#
    static let saturnTipRing = #"{"id":"audio.txInput.saturnMicTipRing","label":"Mic Tip-Ring (Tip is Mic)","tooltip":"","kind":"toggle","binding":{"property":{"object":"transmit","name":"micTipRing"}},"applies":"live","requiresDescriptionVersion":24,"gate":{"capability":"transmitSettingsVersion","min":3,"transmit":true}}"#
    /// Line In Gain as the Core fits it for a peer below 24: whole decibels from -34.
    static let lineInGainV15 = #"{"id":"audio.txInput.hermesLineInGain","label":"Line In Gain:","tooltip":"","kind":"decimal","binding":{"property":{"object":"transmit","name":"lineInBoost"}},"applies":"live","requiresDescriptionVersion":15,"gate":{"capability":"transmitSettingsVersion","min":3,"transmit":true},"min":-34,"max":12,"step":1,"unit":"dB"}"#
    /// Swap audio channels as the Core sends it from dc238c2d5, and as it did before.
    static let swapAudioChannels = #"{"id":"hardware.hl2Io.swapAudioChannels","label":"Swap audio channels","tooltip":"Swap the audio channels sent to the HL2","kind":"toggle","binding":{"radioSetting":"hl2/swapAudioChannels"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false}"#
    static let swapAudioChannelsClosed = #"{"id":"hardware.hl2Io.swapAudioChannels","label":"Swap audio channels","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2/swapAudioChannels"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false,"availability":{"enabled":false,"reason":"NereusSDR does not send the radio audio of its own, so there is nothing to swap."}}"#
    static let swapClosedReason = "NereusSDR does not send the radio audio of its own, so there is nothing to swap."

    /// The row as the Core's resources/setup/hardware.json holds it.
    static let rx1SixMetreLna = #"{"id":"hardware.calibration.rx1_6mLna","label":"Rx1 6m LNA:","tooltip":"","kind":"decimal","binding":{"radioSetting":"cal/rx1_6mLna"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":1},"requiresDescriptionVersion":23,"min":0,"max":25,"step":1,"decimals":1,"unit":"dB","default":13}"#

    @Test func catNetworkTwentyOnesForgetRowFollowsDuplicate() throws {
        let rows = try Self.controls(Self.category("catNetwork", version: 21, [Self.forget, Self.duplicate]))
        #expect(rows[0].metadataIssue == nil)
        #expect(rows[0].modern?.propertyDependency == SetupDescription.PropertyDependency(
            property: .init(object: "stationTci", name: "copyRx2VfobToVfoa"), oneOf: [.bool(true)]))
        #expect(rows[1].modern == nil)
        // Below 21 the Core sends no enabledWhen; one sent anyway, or a shape
        // other than a property's, is bad metadata.
        #expect(try Self.controls(Self.category("catNetwork", version: 15, [Self.forget]))[0].metadataIssue != nil)
        let other = Self.forget.replacingOccurrences(of: #""property":{"object":"stationTci","name":"copyRx2VfobToVfoa"}"#,
                                                     with: #""setting":"X""#)
        #expect(try Self.controls(Self.category("catNetwork", version: 21, [other]))[0].metadataIssue != nil)
    }

    @Test func dspTwentyTwosRxBufferRowsShowTheirOnAirStateAsSent() throws {
        let row = try Self.controls(Self.category("dsp", version: 22, [Self.rxBuffer]))[0]
        #expect(row.metadataIssue == nil)
        #expect(row.availability == SetupDescription.Availability(enabled: false, reason: "Can't change while transmitting."))
        #expect(row.unavailableReason == "Can't change while transmitting.")
        #expect(!row.isEditable)
        let offAir = Self.rxBuffer.replacingOccurrences(of: Self.onAir, with: "")
        #expect(try Self.controls(Self.category("dsp", version: 22, [offAir]))[0].isEditable)
    }

    @Test func paTwentysMalformedProfileMetadataStaysDisabled() throws {
        let profile = #"{"id":"pa.gain.profile","label":"Profile","tooltip":"t","kind":"choice","binding":{"paProfile":{"property":"activeName"}},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"availability":{"enabled":false,"reason":"Can't change while transmitting."}}"#
        let table = #"{"id":"pa.gain.table","label":"Gain","tooltip":"t","kind":"table","binding":{"paProfileGrid":{}},"applies":"live","gate":{"capability":"paProfileVersion","min":1},"requiresDescriptionVersion":14,"rows":[{"band":0,"availability":{"enabled":false,"reason":"Only the device that is transmitting can change this."}}]}"#
        let rows = try Self.controls(Self.category("pa", version: 20, [profile, table]))
        for row in rows {
            #expect(row.metadataIssue == "Invalid PA profile metadata.", "\(row.id)")
            #expect(!row.isEditable, "\(row.id)")
        }
    }

    // MARK: The dispatcher

    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher

        init() {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        func connect(maximum: Int64 = 22, settingsValues: [String: String] = [:], duplicate: Bool = false) async {
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let tests = SetupDescriptionLatestTests.self
            let hardware = maximum >= 18
                ? tests.category("hardware", version: 18, [tests.cl2Enable, tests.cl2Freq, tests.ext10MHz])
                : tests.category("hardware", version: 17, tests.closedClockRows)
            let catNetwork = tests.category("catNetwork", version: maximum >= 21 ? 21 : 15, [
                maximum >= 21 ? tests.forget : tests.forget.replacingOccurrences(of: tests.forgetDependency, with: ""),
                tests.duplicate,
            ])
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(maximum)),
                    .init(name: "transmitSettingsVersion", value: .i64(8)),
                    .init(name: "stationTciSettingsVersion", value: .i64(1)),
                    .init(name: "radioConnected", value: .bool(true)),
                    .init(name: "macAddress", value: .utf8("AA:BB:CC:DD:EE:01")),
                ])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "hardware", kind: .utf8),
                    .init(ordinal: 2, name: "catNetwork", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "hardware", value: .utf8(hardware)),
                    .init(ordinal: 2, name: "catNetwork", value: .utf8(catNetwork)),
                ])),
                .objectCreate(.init(key: "stationTci", className: "StationTciSettings", properties: [
                    .init(name: "copyRx2VfobToVfoa", value: .bool(duplicate)),
                    .init(name: "forgetRx2VfoBOnDisconnect", value: .bool(false)),
                ])),
                .settingsSnapshot(.init(properties: settingsValues.sorted { $0.key < $1.key }
                    .map { .init(name: $0.key, value: .utf8($0.value)) })),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control(_ id: String, in category: String = "hardware") throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: category))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
        }

        func state(_ id: String, in category: String = "hardware") throws -> SetupControlState {
            dispatcher.state(of: try control(id, in: category), in: category)
        }
    }

    static let enableKey = "hardware/AA:BB:CC:DD:EE:01/hl2/cl2Enable"
    static let frequencyKey = "hardware/AA:BB:CC:DD:EE:01/hl2/cl2FreqMHz"

    @Test func theFrequencyIsGreyedUntilEnableCL2IsOnAndReadsItsDefault() async throws {
        let rig = Rig()
        await rig.connect()
        #expect(try rig.state("hardware.hl2Io.cl2Enable") == .init(value: .bool(false), editable: true, reason: nil))
        #expect(try rig.state("hardware.hl2Io.ext10MHz") == .init(value: .bool(false), editable: true, reason: nil))
        #expect(try rig.state("hardware.hl2Io.cl2Freq")
                == .init(value: .decimal(116), editable: false, reason: SetupControlDispatcher.dependsReason))
        #expect(await rig.dispatcher.edit(try rig.control("hardware.hl2Io.cl2Freq"), in: "hardware", to: .decimal(120))
                == .notSent(SetupControlDispatcher.dependsReason))
        #expect(rig.route.sent.isEmpty)

        let on = Rig()
        await on.connect(settingsValues: [Self.enableKey: "True", Self.frequencyKey: "24.576"])
        #expect(try on.state("hardware.hl2Io.cl2Enable").value == .bool(true))
        #expect(try on.state("hardware.hl2Io.cl2Freq") == .init(value: .decimal(24.576), editable: true, reason: nil))
    }

    @Test func enableCL2WritesTrueAndTheFrequencyWritesTheCoresTextAndShowsItsRefusal() async throws {
        let rig = Rig()
        await rig.connect()
        let enable = try rig.control("hardware.hl2Io.cl2Enable")
        let turning = Task { await rig.dispatcher.edit(enable, in: "hardware", to: .bool(true)) }
        #expect(await rig.route.waitForCount(1, unless: turning))
        guard case .settingsWrite(let first) = rig.route.sent[0].message else { Issue.record("no write"); return }
        #expect(first.key == Self.enableKey)
        #expect(first.properties.first?.value == .utf8("True"))
        rig.settings.apply(.settingsValue(.init(key: first.key, origin: first.origin, properties: first.properties)))
        #expect(await turning.value == .applied)
        for _ in 0..<5 { await Task.yield() }
        #expect(try rig.state("hardware.hl2Io.cl2Freq").editable)

        let frequency = try rig.control("hardware.hl2Io.cl2Freq")
        var count = 1
        for (value, text) in [(24.576, "24.576"), (116.0, "116")] {
            let writing = Task { await rig.dispatcher.edit(frequency, in: "hardware", to: .decimal(value)) }
            count += 1
            #expect(await rig.route.waitForCount(count, unless: writing))
            guard case .settingsWrite(let write) = rig.route.sent[count - 1].message else { Issue.record("no write"); return }
            #expect(write.key == Self.frequencyKey)
            #expect(write.properties.first?.value == .utf8(text))
            rig.settings.apply(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
            #expect(await writing.value == .applied)
        }

        // The Core's refusal is shown as it words it.
        let refusal = "Choose a CL2 frequency from 1 to 200 MHz."
        let refused = Task { await rig.dispatcher.edit(frequency, in: "hardware", to: .decimal(150.5)) }
        count += 1
        #expect(await rig.route.waitForCount(count, unless: refused))
        guard case .settingsWrite(let write) = rig.route.sent[count - 1].message else { Issue.record("no write"); return }
        rig.settings.apply(.settingsReject(.init(key: write.key, properties: [.init(name: "value", value: .utf8("116"))],
                                                 reason: refusal)))
        #expect(await refused.value == .refused(refusal))
    }

    @Test func aSeventeenCoreKeepsTheClockRowsClosedWithItsReason() async throws {
        let rig = Rig()
        await rig.connect(maximum: 17)
        let reason = "NereusSDR does not change the radio's clock settings."
        for id in ["hardware.hl2Io.cl2Enable", "hardware.hl2Io.cl2Freq", "hardware.hl2Io.ext10MHz"] {
            let state = try rig.state(id)
            #expect(!state.editable, "\(id)")
            #expect(state.reason == reason, "\(id)")
        }
        #expect(rig.feed.description(for: "hardware")?.version == 17)
    }

    @Test func forgetIsGreyedWhileDuplicateIsOffAtTwentyOneAndAlwaysLiveBelow() async throws {
        let forget = "catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect"
        let rig = Rig()
        await rig.connect()
        #expect(rig.feed.description(for: "catNetwork")?.version == 21)
        #expect(try rig.state(forget, in: "catNetwork")
                == .init(value: .bool(false), editable: false, reason: SetupControlDispatcher.dependsReason))
        await rig.event(.message(.delta(.init(key: "stationTci", properties: [
            .init(name: "copyRx2VfobToVfoa", value: .bool(true))]))))
        for _ in 0..<5 { await Task.yield() }
        #expect(try rig.state(forget, in: "catNetwork") == .init(value: .bool(false), editable: true, reason: nil))

        let older = Rig()
        await older.connect(maximum: 17)
        #expect(older.feed.description(for: "catNetwork")?.version == 15)
        #expect(try older.state(forget, in: "catNetwork") == .init(value: .bool(false), editable: true, reason: nil))
    }
}
