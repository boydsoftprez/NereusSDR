// NereusSDR for iOS: the SSB Mod Monitor against a fake Core: record to display, each state, its reasons, RESET and watching
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Link document sections 6.3, 7.7, 9.1: the SSB Mod Monitor on the TX panel
/// against a fake Core. Each value on screen is the record's; OVER is the
/// held peak against this phone's threshold, and does not latch. RESET sends
/// `txSsbModMonitor.reset` with the chosen source; the Core's stream is watched
/// only while the monitor is on screen.
@Suite("SSB Mod Monitor", .serialized)
@MainActor
struct SsbModMonitorTests {
    private let center = NotificationCenter()

    // MARK: Readings (synthetic)

    static func reading(peak: Double = 55, avg: Double = 22, hold: Double = 70.8, peakDb: Double = -5.2,
                        avgDb: Double = -13.2, holdDb: Double = -3, crest: Double? = 9.1,
                        present: Bool = true) -> [String: LinkJSON] {
        var fields: [String: LinkJSON] = [
            "atMs": .number(1_790_000_000_000), "peakPct": .number(peak), "avgPct": .number(avg),
            "peakHoldPct": .number(hold), "peakDbfs": .number(peakDb), "avgDbfs": .number(avgDb),
            "peakHoldDbfs": .number(holdDb), "signalPresent": .bool(present), "scopeRateHz": .number(1500),
            "scopePctTenths": .string(trace(top: hold)),
        ]
        if let crest {
            fields["crestDb"] = .number(crest)
        }
        return fields
    }

    /// A voice-like envelope, never below zero, whose top meets the hold.
    static func trace(top: Double, points: Int = 256) -> String {
        var raw: [Double] = []
        for index in 0..<points {
            let t = Double(index) / Double(points)
            let syllables = pow(sin(Double.pi * 3.3 * t + 0.4), 2)
            raw.append(syllables * (0.55 + 0.3 * abs(sin(2 * .pi * 19.1 * t)) + 0.15 * abs(sin(2 * .pi * 43 * t + 1))))
        }
        let highest = raw.max() ?? 1
        var bytes: [UInt8] = []
        for value in raw {
            // A record can claim more than the wire carries; its trace stays within it.
            let tenths = UInt16(min((value / highest * top * 10).rounded(), 32_767))
            bytes.append(UInt8(tenths & 0xff))
            bytes.append(UInt8(tenths >> 8))
        }
        return Data(bytes).base64EncodedString()
    }

    static func connected(mode: String, center: NotificationCenter, available: Bool = true) async throws -> ModMonitorTests.Rig {
        try await ModMonitorTests.connected(mode: mode, center: center, ssb: available ? [.ssbModMonitor] : [])
    }

    static func monitor(_ rig: ModMonitorTests.Rig) throws -> SsbModMonitorModel {
        try #require(rig.model.main.ssbModMonitor)
    }

    static func pureSignal(_ rig: ModMonitorTests.Rig, on: Bool = true) async {
        let json = on ? ModMonitorTests.pureSignalRunning
            : ModMonitorTests.pureSignalRunning.replacingOccurrences(of: "\"psEnabled\":true", with: "\"psEnabled\":false")
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 3, name: "statusJson", value: .utf8(json)),
        ])))
    }

    // MARK: The states

    @Test("keyed in USB: every reading is the record's, as the desktop shows it, the needles first")
    func keyedInUsb() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        #expect(monitor.shown && monitor.availability == .available && monitor.reason == nil)
        #expect(monitor.chipText == "USB \u{00B7} Slice A")
        #expect(monitor.meterStyle == .meters)
        monitor.appear()
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulation"] })
        #expect(Self.subscribes(rig.station) == [.init(stream: "txSsbModulation", backlog: 1)])
        await rig.station.deliverSsbModMonitor(Self.reading())
        #expect(await ModMonitorTests.settle { monitor.live })
        #expect(monitor.peakText == "-3.0 dBFS")
        #expect(monitor.avgText == "-13.2 dBFS")
        #expect(monitor.crestText == "9.1 dB")
        #expect(monitor.percentText == "71%")
        #expect(monitor.lamp == .on && monitor.lamp.text == "ON")
        #expect(!monitor.overLit)
        #expect(monitor.peakBar == 70.8 && monitor.avgBar == 22 && monitor.peakHold == 70.8)
        #expect(monitor.scope.count == 256)
        // The window's peak above the hold takes the needle.
        await rig.station.deliverSsbModMonitor(Self.reading(peak: 90, hold: 70.8))
        #expect(await ModMonitorTests.settle { monitor.peakBar == 90 })
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("LSB shows it too; OVER lights at the default -1 dBFS and follows the Core's hold down")
    func overLamp() async throws {
        let rig = try await Self.connected(mode: "LSB", center: center)
        let monitor = try Self.monitor(rig)
        #expect(monitor.shown && monitor.chipText == "LSB \u{00B7} Slice A")
        #expect(monitor.overDbfs == -1)
        monitor.appear()
        await rig.station.deliverSsbModMonitor(Self.reading(hold: 89, holdDb: -1))
        #expect(await ModMonitorTests.settle { monitor.overLit })
        #expect(monitor.lamp == .over && monitor.lamp.text == "OVER")
        await rig.station.deliverSsbModMonitor(Self.reading(hold: 88, holdDb: -1.1))
        #expect(await ModMonitorTests.settle { !monitor.overLit })
        #expect(monitor.lamp == .on)
        // This phone's threshold.
        monitor.setOver(-2)
        #expect(monitor.overLit)
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("crest with no value reads -- while the rest stay live; the phone never works it out")
    func crestAbsent() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.appear()
        await rig.station.deliverSsbModMonitor(Self.reading(crest: nil))
        #expect(await ModMonitorTests.settle { monitor.live })
        #expect(monitor.crestText == "--")
        #expect(monitor.peakText == "-3.0 dBFS")
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("in USB, not transmitting: empty, every reading --, NO RF, and the waiting line")
    func notTransmitting() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.appear()
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulation"] })
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .noSignal && monitor.lamp.text == "NO RF")
        #expect(monitor.enabled)
        #expect(SsbModMonitorModel.waitingText == "Readings appear while the radio transmits in USB or LSB.")
        // A key comes and goes: at the unkey the Core removes the record.
        await rig.station.deliverSsbModMonitor(Self.reading())
        #expect(await ModMonitorTests.settle { monitor.live })
        await rig.station.endSsbModMonitor()
        #expect(await ModMonitorTests.settle { !monitor.live })
        Self.expectBlank(monitor)
        // A record that says there is no signal is shown as none, even at a level that would light OVER.
        await rig.station.deliverSsbModMonitor(Self.reading(holdDb: 0, present: false))
        #expect(await ModMonitorTests.settle { monitor.reading != nil })
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .noSignal && !monitor.overLit)
        // A record out of range is refused, as the desktop refuses it.
        await rig.station.deliverSsbModMonitor(Self.reading())
        #expect(await ModMonitorTests.settle { monitor.live })
        await rig.station.deliverSsbModMonitor(Self.reading(hold: 9_000))
        #expect(await ModMonitorTests.settle { monitor.reading == nil })
        Self.expectBlank(monitor)
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("an older Core: disabled with the desktop's words, still shown, never asked")
    func olderCore() async throws {
        let rig = try await Self.connected(mode: "USB", center: center, available: false)
        let monitor = try Self.monitor(rig)
        #expect(await ModMonitorTests.settle { monitor.availability == .olderCore })
        #expect(monitor.shown && !monitor.enabled)
        #expect(monitor.reason == "This Core does not send the SSB modulation monitor. Updating the Core may help.")
        monitor.appear()
        monitor.reset()
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.subscribes(rig.station).isEmpty)
        #expect(Self.resets(rig.station).isEmpty)
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .blank && monitor.lamp.text == "--")
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("not connected: disabled with the desktop's words, the readings dropped")
    func notConnected() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.appear()
        await rig.station.deliverSsbModMonitor(Self.reading())
        #expect(await ModMonitorTests.settle { monitor.live })
        await rig.model.disconnect()
        #expect(await ModMonitorTests.settle { monitor.availability == .notConnected })
        #expect(monitor.reason == "Connect to the Core to see the SSB modulation monitor.")
        #expect(!monitor.enabled)
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .blank)
        monitor.disappear()
    }

    @Test("outside USB and LSB there is no monitor, and nothing is asked of the Core", arguments: ["AM", "DIGU", "DIGL"])
    func otherModes(mode: String) async throws {
        let rig = try await Self.connected(mode: mode, center: center)
        let monitor = try Self.monitor(rig)
        #expect(!monitor.shown)
        monitor.appear()
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.subscribes(rig.station).isEmpty)
        monitor.disappear()
        await rig.model.disconnect()
    }

    // MARK: The controls

    @Test("PA FB is disabled with the desktop's words while PureSignal is off, and watches its own stream when on")
    func paFeedback() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.appear()
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulation"] })
        #expect(!monitor.pureSignalRunning)
        monitor.choose(.paFeedback)
        #expect(monitor.source == .txIq)
        #expect(monitor.paFeedbackReason
                == "PA FB reads PureSignal\u{2019}s feedback. PureSignal is off, or this radio does not have it.")
        await Self.pureSignal(rig)
        #expect(await ModMonitorTests.settle { monitor.pureSignalRunning })
        #expect(monitor.paFeedbackReason == nil)
        monitor.choose(.paFeedback)
        #expect(monitor.source == .paFeedback)
        // Only its stream is watched. No reset: other devices may be reading.
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulationFeedback"] })
        #expect(Self.resets(rig.station).isEmpty)
        #expect(Self.unsubscribes(rig.station) == ["txSsbModulation"])
        #expect(monitor.note == nil)
        await rig.station.deliverSsbModMonitor(Self.reading(), stream: "txSsbModulationFeedback")
        #expect(await ModMonitorTests.settle { monitor.live })
        // PureSignal stops: a chosen PA FB stays chosen and the phone can go back.
        await Self.pureSignal(rig, on: false)
        #expect(await ModMonitorTests.settle { !monitor.pureSignalRunning })
        #expect(monitor.source == .paFeedback)
        monitor.choose(.txIq)
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulation"] })
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("RESET sends txSsbModMonitor.reset with the chosen source, and shows the Core's refusal as sent")
    func resetSendsTheSource() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.appear()
        monitor.reset()
        #expect(await ModMonitorTests.settle { Self.resets(rig.station) == [0] })
        #expect(await ModMonitorTests.settle { monitor.note == SsbModMonitorModel.clearedText })
        rig.station.refuseNext("txSsbModMonitor.reset", reason: "The Core could not read this request.")
        monitor.reset()
        #expect(await ModMonitorTests.settle { monitor.note == "The Core could not read this request." })
        // The AM monitor's verb is never used.
        #expect(ModMonitorTests.resets(rig.station).isEmpty)
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("a source chosen with no monitor on screen sends nothing to the Core; shown again, it watches that source")
    func noResetUntilShown() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        await Self.pureSignal(rig)
        #expect(await ModMonitorTests.settle { monitor.pureSignalRunning })
        monitor.choose(.paFeedback)
        monitor.reset()
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(monitor.source == .paFeedback)
        #expect(Self.resets(rig.station).isEmpty)
        #expect(Self.subscribes(rig.station).isEmpty)
        monitor.appear()
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulationFeedback"] })
        #expect(Self.resets(rig.station).isEmpty)
        monitor.reset()
        #expect(await ModMonitorTests.settle { Self.resets(rig.station) == [1] })
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("the Core's stream is watched only while the monitor is on the real TX panel in USB or LSB")
    func watchedOnlyOnScreen() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        try await LinkBarrier.roundTrip(rig.model.commands)
        // Connected, the panel closed: no link traffic for it at all.
        #expect(Self.subscribes(rig.station).isEmpty)
        let main = rig.model.main
        let window = try ModMonitorTests.window(size: CGSize(width: TxPanel.width, height: 900))
        let host = UIHostingController(rootView: TxPanel(transmit: main.transmit, accessories: main.accessories,
                                                         micLevel: main.micLevel, modes: main.modes, meters: nil,
                                                         modMonitor: main.modMonitor, ssbModMonitor: monitor))
        host.view.frame = window.bounds
        window.rootViewController = host
        window.isHidden = false
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched == ["txSsbModulation"] })
        #expect(rig.station.modMonitorWatched.isEmpty)
        // USB to LSB keeps it; to AM the SSB monitor goes and the AM monitor comes.
        try await ModMonitorTests.setMode("LSB", rig)
        #expect(rig.station.ssbModMonitorWatched == ["txSsbModulation"])
        try await ModMonitorTests.setMode("AM", rig)
        #expect(await ModMonitorTests.settle {
            !monitor.shown && rig.station.ssbModMonitorWatched.isEmpty && rig.station.modMonitorWatched == ["txAmModulation"]
        })
        #expect(Self.unsubscribes(rig.station) == ["txSsbModulation"])
        try await ModMonitorTests.setMode("USB", rig)
        #expect(await ModMonitorTests.settle {
            rig.station.ssbModMonitorWatched == ["txSsbModulation"] && rig.station.modMonitorWatched.isEmpty
        })
        // The panel closes: the stream stops.
        window.isHidden = true
        window.rootViewController = nil
        #expect(await ModMonitorTests.settle { rig.station.ssbModMonitorWatched.isEmpty })
        #expect(Self.unsubscribes(rig.station) == ["txSsbModulation", "txSsbModulation"])
        await rig.model.disconnect()
    }

    @Test("the threshold, style and source are this phone's: held to range, kept, and never written to the Core")
    func phoneSettings() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = try Self.monitor(rig)
        monitor.setOver(5)
        #expect(monitor.overDbfs == 0)
        monitor.setOver(-50)
        #expect(monitor.overDbfs == -20)
        monitor.setOver(-6)
        monitor.setMeterStyle(.bars)
        #expect(rig.phone.integer(SsbModMonitorModel.overKey, default: 0) == -6)
        #expect(rig.phone.string(SsbModMonitorModel.styleKey, default: "") == "bars")
        // The AM monitor's own settings are untouched.
        #expect(rig.model.main.modMonitor?.meterStyle == .bars)
        #expect(rig.phone.string(ModMonitorModel.styleKey, default: "unset") == "unset")
        #expect(!rig.station.messages.contains {
            if case .settingsWrite(let write) = $0 { return write.key.hasPrefix("SsbModMon") }
            return false
        })
        await rig.model.disconnect()
    }

    @Test("every monitor word is plain: no em dash and no promise of later")
    func words() {
        let words = [SsbModMonitorModel.olderCoreText, SsbModMonitorModel.notConnectedText,
                     SsbModMonitorModel.pureSignalOffText, SsbModMonitorModel.waitingText,
                     SsbModMonitorModel.crestFootText, SsbModMonitorModel.resetFootText,
                     SsbModMonitorModel.clearedText, SsbModMonitorModel.defaultsText]
        for text in words {
            #expect(!text.contains("\u{2014}"))
            #expect(!text.lowercased().split(whereSeparator: { !$0.isLetter }).contains("yet"))
        }
    }

    // MARK: The fake Core

    static func expectBlank(_ monitor: SsbModMonitorModel, sourceLocation: SourceLocation = #_sourceLocation) {
        #expect(monitor.peakText == "--", sourceLocation: sourceLocation)
        #expect(monitor.avgText == "--", sourceLocation: sourceLocation)
        #expect(monitor.crestText == "--", sourceLocation: sourceLocation)
        #expect(monitor.percentText == "--", sourceLocation: sourceLocation)
        #expect(monitor.peakBar == 0 && monitor.avgBar == 0 && monitor.peakHold == nil, sourceLocation: sourceLocation)
        #expect(monitor.scope.isEmpty && !monitor.overLit, sourceLocation: sourceLocation)
    }

    static func subscribes(_ station: FakeStation) -> [ModMonitorTests.Subscribe] {
        ModMonitorTests.invokes(station, "records.subscribe").compactMap { invoke in
            guard let stream = ModMonitorTests.text(invoke, "stream"), stream.hasPrefix("txSsbModulation"),
                  case .i64(let backlog)? = invoke.args.first(where: { $0.name == "backlog" })?.value else {
                return nil
            }
            return ModMonitorTests.Subscribe(stream: stream, backlog: backlog)
        }
    }

    static func unsubscribes(_ station: FakeStation) -> [String] {
        ModMonitorTests.invokes(station, "records.unsubscribe").compactMap { ModMonitorTests.text($0, "stream") }
            .filter { $0.hasPrefix("txSsbModulation") }
    }

    /// Each `txSsbModMonitor.reset`'s source, in order.
    static func resets(_ station: FakeStation) -> [Int64] {
        ModMonitorTests.invokes(station, "txSsbModMonitor.reset").compactMap { invoke in
            guard invoke.args.count == 1, invoke.args[0].name == "source", case .i64(let source) = invoke.args[0].value else {
                return -1
            }
            return source
        }
    }
}
