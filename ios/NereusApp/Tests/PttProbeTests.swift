// NereusSDR for iOS: the PTT button test records every event and cannot reach the transmit path
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
@testable import NereusSDR
import Testing

/// The PTT button test (plan Task 65 step 1). The source checks run in
/// every configuration; the log's own tests only where the test is built
/// (Debug, where PTT_PROBE is set).
@Suite("PttProbe")
@MainActor
struct PttProbeTests {
    private static var ios: URL {
        URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
    }

    private static var probeFolder: URL {
        ios.appendingPathComponent("NereusApp/PTT/PttProbe")
    }

    private static func swiftFiles(under root: URL) throws -> [URL] {
        try #require(FileManager.default.enumerator(at: root, includingPropertiesForKeys: nil))
            .compactMap { $0 as? URL }
            .filter { $0.pathExtension == "swift" }
            .sorted { $0.path < $1.path }
    }

    /// The source without its comments and string literals, so only code
    /// is searched for names.
    private static func code(_ text: String) throws -> String {
        let comment = try Regex(#"//[^\n]*"#)
        let literal = try Regex(#""(\\.|[^"\\\n])*""#)
        return text.replacing(comment, with: "").replacing(literal, with: "\"\"")
    }

    /// The only modules the probe may use: Apple's, never the app's link,
    /// mirror or media.
    static let allowedImports: Set<String> = [
        "AVFoundation", "AppIntents", "Combine", "CoreBluetooth", "Foundation", "MediaPlayer", "PushToTalk",
        "SwiftUI", "UIKit",
    ]

    /// The app's transmit path and everything that reaches it.
    static let transmitPath = [
        "PttController", "PttButton", "TransmitModel", "TransmitVerb", "TransmitAnswer", "TransmitHeartbeatGate",
        "TransmitMicrophoneDemand", "TransmitDisplayModel", "TransmitTimeOutModel", "Transmit", "MicCapture",
        "Microphone", "MicrophoneSender", "MicrophoneRing", "MicrophoneStart", "AppModel", "ConnectionFlow",
        "SessionController", "ActivityAction", "UnkeyIntent", "AudioSessionController", "StationSession",
        "LinkClient",
    ]

    @Test("the probe's sources import only Apple's modules and name nothing on the transmit path")
    func probeCannotReachTheTransmitPath() throws {
        let files = try Self.swiftFiles(under: Self.probeFolder)
        #expect(files.map(\.lastPathComponent) == [
            "PttProbe.swift", "PttProbeActionIntent.swift", "PttProbeBluetooth.swift", "PttProbeLog.swift",
            "PttProbeMicrophone.swift", "PttProbePage.swift",
        ])
        let importLine = try Regex(#"^\s*(@\w+\s+)*import\s+(\w+)"#)
        for file in files {
            let text = try String(contentsOf: file, encoding: .utf8)
            let name = file.lastPathComponent
            for line in text.split(separator: "\n") {
                if let match = String(line).firstMatch(of: importLine), let module = match.output[2].substring {
                    #expect(Self.allowedImports.contains(String(module)), "\(name) imports \(module)")
                }
            }
            let code = try Self.code(text)
            for type in Self.transmitPath {
                let word = try Regex("\\b\(type)\\b")
                #expect(code.firstMatch(of: word) == nil, "\(name) names \(type)")
            }
            #expect(!text.contains("\"tx."), "\(name) holds a transmit verb")
            #expect(!code.contains("tap(trigger:"), "\(name) taps the PTT")
        }
    }

    @Test("every probe file is built only with PTT_PROBE, and only Debug sets it")
    func probeIsDebugOnly() throws {
        for file in try Self.swiftFiles(under: Self.probeFolder) {
            let lines = try String(contentsOf: file, encoding: .utf8).split(separator: "\n").map(String.init)
            #expect(lines.count > 3)
            #expect(lines[2] == "#if PTT_PROBE", "\(file.lastPathComponent)")
            #expect(lines.last == "#endif", "\(file.lastPathComponent)")
        }
        let project = try String(contentsOf: Self.ios.appendingPathComponent("project.yml"), encoding: .utf8)
            .split(separator: "\n").map(String.init)
        let flagged = project.indices.filter { project[$0].contains("PTT_PROBE") && !project[$0].contains("#") }
        // The app and its unit tests, each under a Debug configuration.
        #expect(flagged.count == 2)
        for index in flagged {
            #expect(index > 0 && project[index - 1].trimmingCharacters(in: .whitespaces) == "Debug:")
        }
    }

    @Test("outside its folder only Setup's PTT buttons page opens the probe, inside PTT_PROBE")
    func onlyTheDebugEntryNamesTheProbe() throws {
        let app = Self.ios.appendingPathComponent("NereusApp")
        let naming = try Self.swiftFiles(under: app)
            .filter { !$0.pathComponents.contains("Tests") && !$0.path.contains("/PTT/PttProbe/") }
            .filter { try String(contentsOf: $0, encoding: .utf8).contains("PttProbe") }
        #expect(naming.map(\.lastPathComponent) == ["PttButtonsPage.swift"])
        let page = try String(contentsOf: app.appendingPathComponent("Setup/PttButtonsPage.swift"), encoding: .utf8)
        let flag = try #require(page.range(of: "#if PTT_PROBE"))
        let end = try #require(page.range(of: "#endif", range: flag.upperBound..<page.endIndex))
        let use = try #require(page.range(of: "PttProbe"))
        #expect(use.lowerBound > flag.upperBound && use.upperBound < end.lowerBound)
    }

    #if PTT_PROBE
    private static let start = Date(timeIntervalSince1970: 1_790_000_000)
    private static let unlocked = PttProbeMoment(locked: false, appState: .active)
    private static let locked = PttProbeMoment(locked: true, appState: .background)

    @Test("each event carries its source, begin or end, the lock and its time")
    func recordsAnEvent() {
        let log = PttProbeLog(now: Self.start)
        let event = log.record(.headset, .begin, moment: Self.locked, detail: "x",
                               at: Self.start.addingTimeInterval(1.5))
        #expect(event.id == 0)
        #expect(event.sinceStart == 1.5)
        #expect(event.line == "2026-09-21T14:13:21.500Z | +1.500 s | Headset button | BEGIN | locked | background | x")
        let next = log.record(.phone, .note, moment: Self.unlocked, at: Self.start.addingTimeInterval(2))
        #expect(next.id == 1)
        #expect(next.line.hasSuffix("| Phone | note | unlocked | active"))
        #expect(log.events.count == 2)
    }

    @Test("a press with no channel asks for nothing")
    func pressWithoutAChannel() {
        let log = PttProbeLog(now: Self.start)
        #expect(log.toggle(by: .actionButton, joined: false, moment: Self.locked, at: Self.start) == .nothing)
        #expect(log.events.last?.kind == .press)
        #expect(log.events.last?.detail == "not joined to a channel, nothing to begin")
        #expect(!log.beginPending)
    }

    @Test("a toggling press begins, the next ends, and each is put down to the button that pressed")
    func togglesThroughBeginAndEnd() {
        let log = PttProbeLog(now: Self.start)
        #expect(log.toggle(by: .bluetooth, joined: true, moment: Self.locked, detail: "value 01",
                           at: Self.start) == .begin)
        #expect(log.beginPending)
        #expect(log.events.last?.detail == "value 01, asks to begin")
        let began = log.began(from: .developerRequest, moment: Self.locked, at: Self.start.addingTimeInterval(0.2))
        #expect(began.source == .bluetooth)
        #expect(began.kind == .begin)
        #expect(log.talking && !log.beginPending)
        #expect(log.talkingSource == .bluetooth)
        #expect(log.toggle(by: .bluetooth, joined: true, moment: Self.locked,
                           at: Self.start.addingTimeInterval(5)) == .end)
        let ended = log.ended(from: .developerRequest, moment: Self.locked, microphoneBuffers: 48,
                              at: Self.start.addingTimeInterval(5.2))
        #expect(ended.source == .bluetooth)
        #expect(ended.kind == .end)
        #expect(ended.detail == "lasted 5.000 s, microphone buffers 48")
        #expect(!log.talking)
    }

    @Test("a second press before Push to Talk answers the first asks to end, not to begin again")
    func quickSecondPressEnds() {
        let log = PttProbeLog(now: Self.start)
        #expect(log.toggle(by: .actionButton, joined: true, moment: Self.unlocked, at: Self.start) == .begin)
        #expect(log.toggle(by: .actionButton, joined: true, moment: Self.unlocked, at: Self.start) == .end)
    }

    @Test("Push to Talk's own sources map to the system Talk button and the headset")
    func systemSources() {
        let log = PttProbeLog(now: Self.start)
        #expect(log.began(from: .handsfreeButton, moment: Self.locked, at: Self.start).source == .headset)
        let ended = log.ended(from: .userRequest, moment: Self.locked, microphoneBuffers: 0,
                              at: Self.start.addingTimeInterval(3))
        #expect(ended.source == .systemTalkButton)
        #expect(ended.detail == "lasted 3.000 s, microphone buffers 0, begun by Headset button")
        #expect(log.source(for: .developerRequest) == .unknown)
        #expect(log.source(for: .unknown) == .unknown)
    }

    @Test("a refused request and a lost channel clear what was pending")
    func failuresClear() {
        let log = PttProbeLog(now: Self.start)
        _ = log.toggle(by: .probeScreen, joined: true, moment: Self.unlocked, at: Self.start)
        log.requestFailed(moment: Self.unlocked, detail: "did not begin: refused", at: Self.start)
        #expect(!log.beginPending)
        #expect(log.events.last?.source == .probeScreen)
        #expect(log.events.last?.detail == "did not begin: refused")
        _ = log.began(from: .handsfreeButton, moment: Self.unlocked, at: Self.start)
        log.channelLeft(moment: Self.unlocked, detail: "left", at: Self.start)
        #expect(!log.talking)
        #expect(log.talkingSource == nil)
        #expect(log.events.last?.source == .channel)
    }

    @Test("the shared text holds a title, the column names and every line, oldest first")
    func sharedText() {
        let log = PttProbeLog(now: Self.start)
        log.record(.phone, .note, moment: Self.unlocked, detail: "first", at: Self.start)
        log.record(.headset, .end, moment: Self.locked, detail: "second", at: Self.start.addingTimeInterval(1))
        let lines = log.text().split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        #expect(lines.count == 5)
        #expect(lines[0] == "NereusSDR PTT button test")
        #expect(lines[1] == "time (UTC) | since start | source | event | lock | app | detail")
        #expect(lines[2].hasSuffix("first"))
        #expect(lines[3].hasSuffix("second"))
        #expect(lines[4].isEmpty)
        log.clear(now: Self.start.addingTimeInterval(10))
        #expect(log.events.isEmpty)
        #expect(log.record(.phone, .note, moment: Self.unlocked, at: Self.start.addingTimeInterval(11)).sinceStart == 1)
    }

    @Test("a Bluetooth change is a press by the chosen rule, and its value shows as hex")
    func bluetoothRule() {
        #expect(PttProbeBluetoothRule.everyChange.isPress(Data([0])))
        #expect(PttProbeBluetoothRule.nonZeroOnly.isPress(Data([0, 1])))
        #expect(!PttProbeBluetoothRule.nonZeroOnly.isPress(Data([0, 0])))
        #expect(!PttProbeBluetoothRule.nonZeroOnly.isPress(Data()))
        #expect(PttProbeBluetooth.hex(Data([0x01, 0xAB])) == "01 AB")
        #expect(PttProbeBluetooth.hex(Data()) == "empty")
    }

    @Test("an audio interruption is described in words")
    func interruptionLine() {
        #expect(PttProbe.interruption([AVAudioSessionInterruptionTypeKey: UInt(1)]).hasPrefix("audio interrupted"))
        #expect(PttProbe.interruption([AVAudioSessionInterruptionTypeKey: UInt(0),
                                       AVAudioSessionInterruptionOptionKey: UInt(1)])
            == "audio interruption ended, may resume")
        #expect(PttProbe.interruption(nil) == "audio interruption of an unknown kind")
    }
    #endif
}
