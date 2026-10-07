// NereusSDR for iOS: the Core's CAT setup (the stationCat object and catLog record), as read and as written
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// The `stationCat` object (`stationCatVersion` 1): the four channels, the
/// global settings, the Core computer's limits and the last test, each a
/// JSON text; the `catLog` record; and the configs the phone sends back.
@MainActor
@Suite struct StationCatTests {
    static let channelText = """
    {"config":{"channel":2,"primarySliceId":0,"secondarySliceId":-1,"tcpEnabled":true,"serialEnabled":false,\
    "ptyEnabled":true,"rigctldEnabled":false,"tcpBindAddress":"0.0.0.0","rigctldBindAddress":"127.0.0.1",\
    "tcpPort":13014,"rigctldPort":4533,"serialDevice":"/dev/cu.usbserial-1","serialBaud":38400,\
    "serialParity":"Even","serialDataBits":7,"serialStopBits":"2","ptyDialect":"Rigctld","futureField":5},\
    "primaryValid":false,"secondaryValid":true,"status":{"state":"Listening","tcp":"Listening","serial":"Disabled",\
    "pty":"Open","rigctld":"Disabled","tcpBoundAddress":"0.0.0.0","tcpBoundPort":13014,"rigctldBoundAddress":"",\
    "rigctldBoundPort":0,"tcpClients":2,"rigctldClients":0,"ptyPath":"/dev/ttys004"}}
    """

    static let globalText = """
    {"config":{"aiEnabled":true,"aiSerial1":false,"aiSerial2":true,"aiSerial3":false,"aiSerial4":false,"aiTcp":true,\
    "allowKenwoodAi":true,"digitalReportsSideband":true,"limitReportedPower":false,"pttChannel":3,\
    "pttDeviceSource":"Physical","pttEnabled":true,"pttSerialBaud":9600,"pttSerialDataBits":8,\
    "pttSerialDevice":"/dev/cu.ptt","pttSerialParity":"None","pttSerialStopBits":"1","pttUseCts":true,\
    "pttUseDsr":false,"recenterVfo":false,"rigIdentity":"TS-2000","rttyDiglHz":-2125,"rttyDiguHz":2125,\
    "rttyOffsetAEnabled":true,"rttyOffsetBEnabled":false,"sendWelcome":true,"serialNumber":"0000-0000"},\
    "aiActive":true,"pttState":"Idle"}
    """

    @Test("the names the Core sends, and the log's bound")
    func names() {
        #expect(StationCat.featureName == "stationCat" && StationCat.capabilityName == "stationCatVersion")
        #expect(StationCat.objectKey == "stationCat" && StationCat.className == "StationCatModel")
        #expect(StationCat.logStream == "catLog" && StationCat.logCapacity == 10000)
        #expect(RecordStreamClient.capacity(of: StationCat.logStream) == 10000)
        #expect(StationCat.setChannelVerb == "setStationCatChannel" && StationCat.setGlobalVerb == "setStationCatGlobal")
        #expect(StationCat.testVerb == "testStationCatCommand" && StationCat.refreshDevicesVerb == "refreshStationCatDevices")
        #expect(StationCat.channelProperty(3) == "channel3" && StationCat.channels == 1...4)
    }

    @Test("a channel's settings, its slices' state and its status")
    func channel() {
        let channel = StationCat.Channel(number: 2, text: Self.channelText)
        #expect(channel.number == 2 && channel.received)
        #expect(channel.config.primarySliceId == 0 && channel.config.secondarySliceId == -1)
        #expect(channel.config.tcpEnabled && !channel.config.serialEnabled && channel.config.ptyEnabled)
        #expect(!channel.config.rigctldEnabled)
        #expect(channel.config.tcpBindAddress == "0.0.0.0" && channel.config.rigctldBindAddress == "127.0.0.1")
        #expect(channel.config.tcpPort == 13014 && channel.config.rigctldPort == 4533)
        #expect(channel.config.serialDevice == "/dev/cu.usbserial-1" && channel.config.serialBaud == 38400)
        #expect(channel.config.serialParity == "Even" && channel.config.serialDataBits == 7)
        #expect(channel.config.serialStopBits == "2" && channel.config.ptyDialect == "Rigctld")
        #expect(!channel.primaryValid && channel.secondaryValid)
        #expect(channel.status.state == "Listening" && channel.status.tcp == "Listening")
        #expect(channel.status.serial == "Disabled" && channel.status.pty == "Open" && channel.status.rigctld == "Disabled")
        #expect(channel.status.tcpBoundAddress == "0.0.0.0" && channel.status.tcpBoundPort == 13014)
        #expect(channel.status.tcpClients == 2 && channel.status.rigctldClients == 0)
        #expect(channel.status.ptyPath == "/dev/ttys004")
    }

    @Test("a channel missing, not JSON, or with fields of another kind reads as empty, never as a failure")
    func channelTolerant() {
        let missing = StationCat.Channel(number: 1, text: nil)
        #expect(!missing.received && missing.config.primarySliceId == -1 && missing.config.secondarySliceId == -1)
        #expect(missing.primaryValid && missing.secondaryValid && missing.status.state.isEmpty)
        let garbled = StationCat.Channel(number: 1, text: "not json")
        #expect(!garbled.received)
        let odd = StationCat.Channel(number: 1, text: #"{"config":{"tcpPort":"loud","tcpEnabled":1},"status":[]}"#)
        #expect(odd.received && odd.config.tcpPort == 0 && !odd.config.tcpEnabled && odd.status.tcpClients == 0)
    }

    @Test("the channel command is the Core's whole config, the change and the rebind flags, unknown fields kept")
    func channelCommand() throws {
        var config = StationCat.Channel(number: 2, text: Self.channelText).config
        config.tcpPort = 14000
        config.secondarySliceId = 1
        let sent = try LinkJSON.parse(config.commandText(primaryRebind: false, secondaryRebind: true))
        guard case .object(let fields) = sent else {
            Issue.record("not an object")
            return
        }
        #expect(fields["tcpPort"] == .number(14000) && fields["secondarySliceId"] == .number(1))
        #expect(fields["primaryRebind"] == .bool(false) && fields["secondaryRebind"] == .bool(true))
        #expect(fields["channel"] == .number(2) && fields["serialStopBits"] == .string("2"))
        #expect(fields["tcpBindAddress"] == .string("0.0.0.0") && fields["ptyDialect"] == .string("Rigctld"))
        #expect(fields["futureField"] == .number(5))
        #expect(fields.count == 20)
    }

    @Test("a config the Core never sent writes only what the phone set")
    func channelCommandFromNothing() throws {
        var config = StationCat.ChannelConfig()
        config.tcpEnabled = true
        let sent = try LinkJSON.parse(config.commandText(primaryRebind: false, secondaryRebind: false))
        #expect(sent == .object(["tcpEnabled": .bool(true), "primaryRebind": .bool(false),
                                 "secondaryRebind": .bool(false)]))
    }

    @Test("the global settings, AI's state and PTT's state")
    func global() {
        let global = StationCat.Global(text: Self.globalText)
        #expect(global.received && global.aiActive && global.pttState == "Idle")
        let config = global.config
        #expect(config.aiEnabled && !config.aiSerial1 && config.aiSerial2 && config.aiTcp && config.allowKenwoodAi)
        #expect(config.digitalReportsSideband && !config.limitReportedPower && !config.recenterVfo)
        #expect(config.pttChannel == 3 && config.pttDeviceSource == "Physical" && config.pttEnabled)
        #expect(config.pttSerialBaud == 9600 && config.pttSerialDataBits == 8 && config.pttSerialDevice == "/dev/cu.ptt")
        #expect(config.pttSerialParity == "None" && config.pttSerialStopBits == "1" && config.pttUseCts && !config.pttUseDsr)
        #expect(config.rigIdentity == "TS-2000" && config.rttyDiglHz == -2125 && config.rttyDiguHz == 2125)
        #expect(config.rttyOffsetAEnabled && !config.rttyOffsetBEnabled && config.sendWelcome)
        #expect(config.serialNumber == "0000-0000")
        #expect(config.aiSerial(2) && !config.aiSerial(4))
    }

    @Test("the global command is the Core's whole config with the change")
    func globalCommand() throws {
        var config = StationCat.Global(text: Self.globalText).config
        config.rigIdentity = "TS-480"
        config.setAiSerial(4, true)
        let sent = try LinkJSON.parse(config.text)
        guard case .object(let fields) = sent else {
            Issue.record("not an object")
            return
        }
        #expect(fields["rigIdentity"] == .string("TS-480") && fields["aiSerial4"] == .bool(true))
        #expect(fields["rttyDiglHz"] == .number(-2125) && fields["pttChannel"] == .number(3))
        #expect(fields.count == 27)
    }

    @Test("the Core computer's limits, its serial devices and its PTY dialects")
    func platform() {
        let platform = StationCat.Platform(text: """
        {"serial":true,"pty":true,"markSpaceParity":false,"oneAndHalfStop":false,\
        "serialDevices":["/dev/cu.a","/dev/cu.b",3],\
        "ptyDialects":[{"value":"Native","label":"Kenwood and ZZ commands"},{"value":"Rigctld","label":"Hamlib rigctld"},5]}
        """)
        #expect(platform.received && platform.serial && platform.pty)
        #expect(!platform.markSpaceParity && !platform.oneAndHalfStop)
        #expect(platform.serialDevices == ["/dev/cu.a", "/dev/cu.b"])
        #expect(platform.ptyDialects == [StationCat.Dialect(value: "Native", label: "Kenwood and ZZ commands"),
                                         StationCat.Dialect(value: "Rigctld", label: "Hamlib rigctld")])
        let missing = StationCat.Platform(text: "")
        #expect(!missing.received && !missing.serial && !missing.pty && missing.serialDevices.isEmpty)
        #expect(missing.ptyDialects.isEmpty)
    }

    @Test("the last test the Core ran")
    func lastTest() {
        let test = StationCat.LastTest(text: """
        {"requestId":7,"channel":1,"command":"FA;","reply":"FA00014074000;","accepted":true}
        """)
        #expect(test.requestId == 7 && test.channel == 1 && test.command == "FA;")
        #expect(test.reply == "FA00014074000;" && test.accepted)
        #expect(StationCat.LastTest(text: nil).requestId == 0)
    }

    @Test("the object's seven properties, read by name")
    func object() {
        let cat = StationCat(values: ["global": .text(Self.globalText), "channel2": .text(Self.channelText),
                                      "platform": .text(#"{"pty":true}"#), "lastTest": .int(4)])
        #expect(cat.channels.map(\.number) == [1, 2, 3, 4])
        #expect(cat.channels.map(\.received) == [false, true, false, false])
        #expect(cat.channel(2).config.tcpPort == 13014 && cat.global.received && cat.platform.pty)
        #expect(!cat.lastTest.received)
    }

    @Test("one catLog record: channel, direction, text and time")
    func logLine() {
        let line = StationCat.LogLine(record: .init(id: "12", fields: [
            "channel": .number(3), "inbound": .bool(true), "text": .string("FA;"), "time": .number(1_700_000_000_123),
        ]))
        #expect(line.id == "12" && line.channel == 3 && line.inbound && line.text == "FA;")
        #expect(line.timeMs == 1_700_000_000_123)
        let odd = StationCat.LogLine(record: .init(id: "13", fields: ["channel": .string("x"), "inbound": .number(1)]))
        #expect(odd.channel == 0 && !odd.inbound && odd.text.isEmpty && odd.timeMs == nil)
    }
}
