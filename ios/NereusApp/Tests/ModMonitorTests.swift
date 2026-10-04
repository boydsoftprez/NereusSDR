// NereusSDR for iOS: the AM Mod Monitor against a fake Core: each state the board draws, RESET, the shared feedback receiver, watching
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

/// R-IOS-13, R-IOS-18, D102 (link document sections 6.3, 7.7, 8.1, 9.1):
/// the AM Mod Monitor on the TX panel against a fake Core, in each state the
/// approved board draws: keyed with the carrier OK, the carrier low with
/// both flashers latched, in AM but not transmitting, an older Core, not
/// connected, and USB with no monitor. RESET sends `txModMonitor.reset`
/// with the chosen source; the feedback receiver is written to the Core;
/// the Core's stream is watched only while the monitor is on screen.
@Suite("AM Mod Monitor", .serialized)
@MainActor
struct ModMonitorTests {
    private let center = NotificationCenter()

    // MARK: The board's example readings

    /// Keyed in AM, carrier OK: 112% / 94% / +18% / -6.2 dBFS.
    static func carrierOk() -> [String: LinkJSON] {
        reading(pos: 104, neg: 88, posHold: 112.3, negHold: 94.4, level: 0.49, dbfs: -6.2)
    }

    /// Carrier low, peaks over both flashers: 131% / 99% / +32% / -30.5 dBFS.
    static func carrierLow() -> [String: LinkJSON] {
        reading(pos: 128, neg: 97, posHold: 131, negHold: 99, level: 0.03, dbfs: -30.5, low: true)
    }

    static func reading(pos: Double, neg: Double, posHold: Double, negHold: Double, level: Double, dbfs: Double,
                        present: Bool = true, low: Bool = false, high: Bool = false) -> [String: LinkJSON] {
        [
            "atMs": .number(1_790_000_000_000), "posPeakPct": .number(pos), "negPeakPct": .number(neg),
            "posHoldPct": .number(posHold), "negHoldPct": .number(negHold), "carrierLevel": .number(level),
            "carrierDbfs": .number(dbfs), "carrierPresent": .bool(present), "carrierLow": .bool(low),
            "carrierHigh": .bool(high), "scopeRateHz": .number(1500),
            "scopePctTenths": .string(trace(posHold: posHold, negHold: negHold)),
        ]
    }

    /// A voice-like envelope whose peaks meet the holds (synthetic, D4).
    static func trace(posHold: Double, negHold: Double, points: Int = 256) -> String {
        var raw: [Double] = []
        for index in 0..<points {
            let t = Double(index) / Double(points)
            let envelope = 0.55 + 0.45 * pow(sin(Double.pi * 1.7 * t + 0.4), 2)
            raw.append(envelope * (0.62 * sin(2 * .pi * 9.1 * t) + 0.28 * sin(2 * .pi * 23.3 * t + 1.1)
                                   + 0.12 * sin(2 * .pi * 41 * t + 2)))
        }
        let top = raw.max() ?? 1
        let bottom = raw.min() ?? -1
        var bytes: [UInt8] = []
        for value in raw {
            let percent = value >= 0 ? value / top * posHold : -value / bottom * negHold
            let tenths = UInt16(bitPattern: Int16((percent * 10).rounded()))
            bytes.append(UInt8(tenths & 0xff))
            bytes.append(UInt8(tenths >> 8))
        }
        return Data(bytes).base64EncodedString()
    }

    static let pureSignalRunning = """
    {"schema":1,"psEnabled":true,"mox":true,"engineState":7,"feedbackLevel":152,\
    "successfulCalibrations":9,"attemptedCalibrations":11,"correctionsApplied":true}
    """

    // MARK: The states

    @Test("keyed in AM, carrier OK: the readings as the board shows them, PA feedback greyed with its reason")
    func carrierOkState() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        #expect(monitor.shown && monitor.availability == .available)
        #expect(monitor.chipText == "AM \u{00B7} Slice A")
        monitor.appear()
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulation"] })
        #expect(Self.subscribes(rig.station) == [.init(stream: "txAmModulation", backlog: 1)])
        await rig.station.deliverModMonitor(Self.carrierOk())
        #expect(await Self.settle { monitor.live })
        #expect(monitor.posText == "112%")
        #expect(monitor.negText == "94%")
        #expect(monitor.asymmetryText == "+18%")
        #expect(monitor.carrierText == "-6.2 dBFS")
        #expect(monitor.lamp == .ok && monitor.lamp.text == "CARRIER OK")
        #expect(!monitor.posLit && !monitor.negLit)
        #expect(monitor.posBar == 104 && monitor.negBar == 88)
        #expect(monitor.posHold == 112.3 && monitor.negHold == 94.4)
        #expect(monitor.scope.count == 256)
        #expect(!monitor.pureSignalRunning)
        #expect(monitor.reason == nil)
        // PA feedback cannot be chosen while PureSignal is off.
        monitor.choose(.paFeedback)
        #expect(monitor.source == .txIq)
        #expect(ModMonitorModel.pureSignalOffText
                == "PA feedback works only while PureSignal runs on the Core\u{2019}s radio.")
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("carrier low with both flashers latched on PA feedback; they stay lit until RESET, which the Core hears")
    func carrierLowState() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 3, name: "statusJson", value: .utf8(Self.pureSignalRunning)),
        ])))
        #expect(await Self.settle { monitor.pureSignalRunning })
        monitor.appear()
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulation"] })
        monitor.choose(.paFeedback)
        #expect(monitor.source == .paFeedback)
        // The desktop's source change: the new source's peaks start afresh, and only its stream is watched.
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulationFeedback"] })
        #expect(await Self.settle { Self.resets(rig.station) == [1] })
        #expect(Self.unsubscribes(rig.station) == ["txAmModulation"])
        await rig.station.deliverModMonitor(Self.carrierLow(), stream: "txAmModulationFeedback")
        #expect(await Self.settle { monitor.live && monitor.posLit && monitor.negLit })
        #expect(monitor.posText == "131%")
        #expect(monitor.negText == "99%")
        #expect(monitor.asymmetryText == "+32%")
        #expect(monitor.carrierText == "-30.5 dBFS")
        #expect(monitor.lamp == .low && monitor.lamp.text == "CARRIER LOW")
        // The Core's holds fall; the phone draws them as sent, and the flashers stay latched.
        await rig.station.deliverModMonitor(Self.reading(pos: 60, neg: 50, posHold: 90, negHold: 70, level: 0.03,
                                                         dbfs: -30.5, low: true),
                                            stream: "txAmModulationFeedback")
        #expect(await Self.settle { monitor.posText == "90%" })
        #expect(monitor.negText == "70%")
        #expect(monitor.posHold == 90 && monitor.posBar == 60)
        #expect(monitor.posLit && monitor.negLit)
        // RESET: the lights go out and the Core clears the feedback analyzer for everyone.
        monitor.reset()
        #expect(!monitor.posLit && !monitor.negLit)
        #expect(await Self.settle { Self.resets(rig.station) == [1, 1] })
        #expect(await Self.settle { monitor.note == "Peaks and flashers cleared." })
        // A record below this phone's thresholds leaves them out.
        await rig.station.deliverModMonitor(Self.reading(pos: 100, neg: 80, posHold: 100, negHold: 80, level: 0.4,
                                                         dbfs: -8), stream: "txAmModulationFeedback")
        #expect(await Self.settle { monitor.posText == "100%" })
        #expect(!monitor.posLit && !monitor.negLit)
        #expect(monitor.lamp == .ok)
        // A high carrier comes first in the lamp's order.
        await rig.station.deliverModMonitor(Self.reading(pos: 100, neg: 80, posHold: 100, negHold: 80, level: 0.99,
                                                         dbfs: -0.1, high: true), stream: "txAmModulationFeedback")
        #expect(await Self.settle { monitor.lamp == .high })
        #expect(monitor.lamp.text == "CARRIER HIGH")
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("in AM, not transmitting: bars empty, every reading --, NO CARRIER, and the waiting line")
    func notTransmittingState() async throws {
        let rig = try await Self.connected(mode: "SAM", center: center)
        let monitor = rig.monitor
        #expect(monitor.shown)
        monitor.appear()
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulation"] })
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .noCarrier && monitor.lamp.text == "NO CARRIER")
        #expect(monitor.enabled && !monitor.live)
        #expect(ModMonitorModel.waitingText == "Readings appear while the radio transmits in AM, SAM or DSB.")
        // A key comes and goes: at the unkey the Core removes the record.
        await rig.station.deliverModMonitor(Self.carrierOk())
        #expect(await Self.settle { monitor.live })
        await rig.station.endModMonitor()
        #expect(await Self.settle { !monitor.live })
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .noCarrier)
        // A record the Core sends without a carrier is shown as none.
        await rig.station.deliverModMonitor(Self.reading(pos: 50, neg: 40, posHold: 50, negHold: 40, level: 0,
                                                         dbfs: -120, present: false))
        #expect(await Self.settle { monitor.reading != nil })
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .noCarrier)
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("an older Core: greyed with the desktop's words, and never asked")
    func olderCoreState() async throws {
        let rig = try await Self.connected(mode: "AM", center: center, monitor: false)
        let monitor = rig.monitor
        #expect(await Self.settle { monitor.availability == .olderCore })
        #expect(monitor.shown)
        #expect(monitor.reason == "This Core does not send the modulation monitor. Updating the Core may help.")
        #expect(!monitor.enabled)
        monitor.appear()
        monitor.reset()
        monitor.setFeedbackReceiver(3)
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.subscribes(rig.station).isEmpty)
        #expect(Self.resets(rig.station).isEmpty)
        #expect(Self.receiverWrites(rig.station).isEmpty)
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .blank && monitor.lamp.text == "--")
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("not connected: greyed with the desktop's words, the readings dropped")
    func notConnectedState() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        monitor.appear()
        await rig.station.deliverModMonitor(Self.carrierOk())
        #expect(await Self.settle { monitor.live })
        await rig.model.disconnect()
        #expect(await Self.settle { monitor.availability == .notConnected })
        #expect(monitor.reason == "Connect to the Core to see the modulation monitor.")
        #expect(!monitor.enabled)
        Self.expectBlank(monitor)
        #expect(monitor.lamp == .blank)
        monitor.disappear()
    }

    @Test("in USB there is no monitor, and nothing is asked of the Core")
    func usbState() async throws {
        let rig = try await Self.connected(mode: "USB", center: center)
        let monitor = rig.monitor
        #expect(!monitor.shown)
        monitor.appear()
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.subscribes(rig.station).isEmpty)
        monitor.disappear()
        await rig.model.disconnect()
    }

    // MARK: The controls

    @Test("RESET sends txModMonitor.reset with the chosen source, and shows the Core's refusal as sent")
    func resetSendsTheSource() async throws {
        let rig = try await Self.connected(mode: "DSB", center: center)
        let monitor = rig.monitor
        monitor.appear()
        monitor.reset()
        #expect(await Self.settle { Self.resets(rig.station) == [0] })
        #expect(await Self.settle { monitor.note == ModMonitorModel.clearedText })
        rig.station.refuseNext("txModMonitor.reset", reason: "The Core could not read this request.")
        monitor.reset()
        #expect(await Self.settle { monitor.note == "The Core could not read this request." })
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("the PA feedback receiver is the Core's: a pick writes ModMon/FbStream, and the Core's value shows")
    func feedbackReceiverWrite() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        #expect(monitor.feedbackReceiver == 1)
        monitor.setFeedbackReceiver(3)
        #expect(await Self.settle { Self.receiverWrites(rig.station) == ["3"] })
        #expect(await Self.settle { monitor.feedbackReceiver == 3 })
        #expect(rig.model.settings.value("ModMon/FbStream") == "3")
        // Out of range is never sent.
        monitor.setFeedbackReceiver(7)
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.receiverWrites(rig.station) == ["3"])
        // The Core's refusal, in its words.
        rig.station.refuseNext("ModMon/FbStream", reason: "The Core could not store this setting.")
        monitor.setFeedbackReceiver(0)
        #expect(await Self.settle { monitor.receiverNote == "The Core could not store this setting." })
        #expect(ModMonitorModel.receiver("x") == 1 && ModMonitorModel.receiver("4") == 4)
        await rig.model.disconnect()
    }

    @Test("the Core's stream is watched only while the monitor is on screen and the slice is in AM, SAM or DSB")
    func watchedOnlyOnScreen() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        try await LinkBarrier.roundTrip(rig.model.commands)
        #expect(Self.subscribes(rig.station).isEmpty)
        // The real TX panel on screen: the monitor appears, and asks.
        let window = try Self.window(size: CGSize(width: TxPanel.width, height: 900))
        let host = UIHostingController(rootView: TxPanel(transmit: rig.model.main.transmit,
                                                         accessories: rig.model.main.accessories,
                                                         micLevel: rig.model.main.micLevel, modes: rig.model.main.modes, meters: nil,
                                                         modMonitor: monitor))
        host.view.frame = window.bounds
        window.rootViewController = host
        window.isHidden = false
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulation"] })
        #expect(Self.subscribes(rig.station) == [.init(stream: "txAmModulation", backlog: 1)])
        // The mode leaves the AM family: the monitor goes, and so does the stream.
        try await Self.setMode("USB", rig)
        #expect(await Self.settle { !monitor.shown && rig.station.modMonitorWatched.isEmpty })
        #expect(Self.unsubscribes(rig.station) == ["txAmModulation"])
        try await Self.setMode("AM", rig)
        #expect(await Self.settle { rig.station.modMonitorWatched == ["txAmModulation"] })
        // The panel closes: the stream stops.
        window.isHidden = true
        window.rootViewController = nil
        #expect(await Self.settle { rig.station.modMonitorWatched.isEmpty })
        #expect(Self.unsubscribes(rig.station) == ["txAmModulation", "txAmModulation"])
        await rig.model.disconnect()
    }

    @Test("thresholds and style are this phone's: held to their ranges, kept, and used by the flashers")
    func phoneSettings() async throws {
        let rig = try await Self.connected(mode: "AM", center: center)
        let monitor = rig.monitor
        #expect(monitor.posFlashPct == 125 && monitor.negFlashPct == 95 && monitor.meterStyle == .bars)
        monitor.setPosFlash(200)
        monitor.setNegFlash(10)
        #expect(monitor.posFlashPct == 160 && monitor.negFlashPct == 50)
        monitor.setPosFlash(130)
        monitor.setMeterStyle(.meters)
        #expect(rig.phone.integer(ModMonitorModel.posFlashKey, default: 0) == 130)
        #expect(rig.phone.string(ModMonitorModel.styleKey, default: "") == "meters")
        monitor.appear()
        await rig.station.deliverModMonitor(Self.reading(pos: 128, neg: 40, posHold: 128, negHold: 40, level: 0.4,
                                                         dbfs: -8))
        #expect(await Self.settle { monitor.posText == "128%" })
        #expect(!monitor.posLit && !monitor.negLit)
        // Nothing here is ever written to the Core.
        #expect(Self.receiverWrites(rig.station).isEmpty)
        #expect(!rig.station.messages.contains {
            if case .settingsWrite(let write) = $0 { return write.key.hasPrefix("ModMon/") && write.key != "ModMon/FbStream" }
            return false
        })
        monitor.disappear()
        await rig.model.disconnect()
    }

    @Test("every monitor word is plain: no em dash and no promise of later")
    func words() {
        let words = [ModMonitorModel.olderCoreText, ModMonitorModel.notConnectedText, ModMonitorModel.pureSignalOffText,
                     ModMonitorModel.waitingText, ModMonitorModel.resetFootText, ModMonitorModel.clearedText,
                     ModMonitorModel.defaultsText, ModMonitorModel.receiverText]
        for text in words {
            #expect(!text.contains("\u{2014}"))
            #expect(!text.lowercased().split(whereSeparator: { !$0.isLetter }).contains("yet"))
        }
        #expect(ModMonitorModel.signedPercentText(-3.4) == "-3%")
        #expect(ModMonitorModel.signedPercentText(0) == "+0%")
        #expect(ModMonitorModel.dbfsText(-30.54) == "-30.5 dBFS")
    }

    // MARK: The fake Core

    struct Subscribe: Equatable {
        var stream: String
        var backlog: Int64
    }

    @MainActor
    struct Rig {
        let model: AppModel
        let station: FakeStation
        let phone: PhoneSettings

        var monitor: ModMonitorModel {
            guard let monitor = model.main.modMonitor else {
                preconditionFailure("the app makes its monitor")
            }
            return monitor
        }
    }

    /// The app connected to a fake Core (with the monitor, or one before it)
    /// with the board's TX panel and slice A, the transmit slice, in `mode`.
    static func connected(mode: String, center: NotificationCenter, monitor: Bool = true) async throws -> Rig {
        let suite = "ModMonitorTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        UIApplication.shared.isIdleTimerDisabled = false
        let station = try FakeStation(additions: monitor ? [.remoteTx, .wideband, .modMonitor] : [.remoteTx, .wideband])
        let phone = PhoneSettings(defaults: defaults)
        let audio = AudioSessionController(session: FakeAudioSession(), output: FakePlaybackOutput(),
                                           notificationCenter: center)
        let model = AppModel(phoneSettings: phone, mediaPeerFactory: station.mediaPeerFactory, audio: audio,
                             microphone: { _ in FakeMicrophone() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        try await MainScreenShotTests.fill(station)
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle { model.main.slices.catalog != nil })
        let rig = Rig(model: model, station: station, phone: phone)
        try await setMode(mode, rig)
        #expect(await settle {
            model.connection == .connected && model.main.modMonitor?.modeLabel == mode
                && model.main.modMonitor?.availability != .notConnected
        })
        return rig
    }

    static func setMode(_ mode: String, _ rig: Rig) async throws {
        let id = try #require(rig.model.main.slices.catalog?.modes.first { $0.label == mode }?.id)
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .enumeration(Int64(id))),
        ])))
        #expect(await settle { rig.model.main.slices.entries.first { $0.id == 0 }?.modeLabel == mode })
    }

    static func expectBlank(_ monitor: ModMonitorModel, sourceLocation: SourceLocation = #_sourceLocation) {
        #expect(monitor.posText == "--", sourceLocation: sourceLocation)
        #expect(monitor.negText == "--", sourceLocation: sourceLocation)
        #expect(monitor.asymmetryText == "--", sourceLocation: sourceLocation)
        #expect(monitor.carrierText == "--", sourceLocation: sourceLocation)
        #expect(monitor.posBar == 0 && monitor.negBar == 0, sourceLocation: sourceLocation)
        #expect(monitor.posHold == nil && monitor.negHold == nil, sourceLocation: sourceLocation)
        #expect(monitor.scope.isEmpty, sourceLocation: sourceLocation)
        #expect(!monitor.posLit && !monitor.negLit, sourceLocation: sourceLocation)
    }

    static func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == verb }
    }

    static func text(_ invoke: LinkMessage.CommandInvoke, _ name: String) -> String? {
        if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
            return value
        }
        return nil
    }

    /// The monitor's subscriptions, in order.
    static func subscribes(_ station: FakeStation) -> [Subscribe] {
        invokes(station, "records.subscribe").compactMap { invoke in
            guard let stream = text(invoke, "stream"), stream.hasPrefix("txAmModulation"),
                  case .i64(let backlog)? = invoke.args.first(where: { $0.name == "backlog" })?.value else {
                return nil
            }
            return Subscribe(stream: stream, backlog: backlog)
        }
    }

    static func unsubscribes(_ station: FakeStation) -> [String] {
        invokes(station, "records.unsubscribe").compactMap { text($0, "stream") }.filter { $0.hasPrefix("txAmModulation") }
    }

    /// Each `txModMonitor.reset`'s source, in order.
    static func resets(_ station: FakeStation) -> [Int64] {
        invokes(station, "txModMonitor.reset").compactMap { invoke in
            guard invoke.args.count == 1, invoke.args[0].name == "source", case .i64(let source) = invoke.args[0].value else {
                return -1
            }
            return source
        }
    }

    static func receiverWrites(_ station: FakeStation) -> [String] {
        station.messages.compactMap { message in
            guard case .settingsWrite(let write) = message, write.key == "ModMon/FbStream",
                  case .utf8(let value)? = write.properties.first?.value else {
                return nil
            }
            return value
        }
    }

    static func window(size: CGSize) throws -> UIWindow {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        return window
    }

    static func settle(seconds: Double = 10, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }
}
