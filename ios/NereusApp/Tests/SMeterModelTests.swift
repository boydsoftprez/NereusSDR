// NereusSDR for iOS: the analog S-meter reads the Core's readings by the menu's choices, keeps them on this device, and lights modes as the Core sends them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import CoreGraphics
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing
import UIKit

/// D86, R-IOS-24, R-IOS-27: the S-meter's menu against a fake Core. Max
/// Bin needs this slice's pan trace, and transmit modes need the Core's
/// readings; the choices are kept on this device.
@Suite("S-meter", .serialized)
@MainActor
struct SMeterModelTests {
    @Test("Max Bin follows this slice's own live pan trace and retires with its display")
    func maxBinFromOwnPan() async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
        ])))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-2_900)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 11, name: "active", value: .bool(true)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        let defaults = try #require(UserDefaults(suiteName: "SMeterPanTests-\(UUID().uuidString)"))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in }),
                                   meterSettings: SMeterSettingsStore(defaults: defaults))
        _ = main.band.prepareToDraw(size: CGSize(width: 320, height: 480), scale: 2)
        main.receive(.mediaState(.connected))
        #expect(await settle { main.subscriber.acceptedDisplay != nil })
        let accepted = try #require(main.subscriber.acceptedDisplay)
        #expect(accepted.panKey == "pan-0")
        #expect(main.sMeter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)

        let request = accepted.subscription
        var payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("meter"),
            "endpointId": .number(Double(request.endpointId)), "revision": .number(Double(request.revision)),
            "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(7_236_400), "sampleRateHz": .number(192_000),
            "centreHz": .number(7_236_400), "spanHz": .number(48_000),
            "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(6), "waterfallSamples": .number(6), "wideSamples": .number(0),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: false))))
        main.receive(.displayFrame(DisplayFrame(endpointId: request.endpointId, contextGeneration: 1,
                                                encoderSequence: 1, producerTimestamp: 0, isKeyframe: true,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -90, -50, -100, -80, -100],
                                                waterfallDbm: [-100, -100, -100, -100, -100, -100], wideDbm: [])))
        #expect(main.sMeter.reason(for: .maxBin) == nil)
        main.sMeter.chooseRx(.maxBin)
        #expect(main.sMeter.display.right == "-50.0 dBm")

        // The same slice moves to another pan before its new subscription
        // answers: the old pan's cached frame must not remain a meter input.
        mirror.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-1")),
        ])))
        #expect(await settle { main.slices.active?.panKey == "pan-1" })
        #expect(await settle { main.sMeter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText })
        #expect(await settle { main.subscriber.acceptedDisplay?.panKey == "pan-1"
            && main.subscriber.acceptedDisplay?.subscription.revision != request.revision })
        let moved = try #require(main.subscriber.acceptedDisplay)
        payload["revision"] = .number(Double(moved.subscription.revision))
        payload["contextGeneration"] = .number(2)
        main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: false))))
        main.receive(.displayFrame(DisplayFrame(endpointId: moved.subscription.endpointId, contextGeneration: 2,
                                                encoderSequence: 2, producerTimestamp: 1, isKeyframe: true,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -90, -55, -100, -80, -100],
                                                waterfallDbm: [-100, -100, -100, -100, -100, -100], wideDbm: [])))
        #expect(main.sMeter.reason(for: .maxBin) == nil)
        #expect(main.sMeter.display.right == "-55.0 dBm")

        // An extras-enabled old frame can still be waiting when the media
        // closes. Drawing or a late matching extras packet after reconnect
        // must not turn that queued old frame into a fresh reading.
        main.band.gates = MediaFeatureGates(agreedMinor: 11, capabilityVersion: { name in
            name == "remoteMediaVersion" || name == "displayExtrasVersion" ? 1 : 0
        })
        #expect(main.band.state.expectsExtras)
        let oldSerial = main.band.state.committedSerial
        main.receive(.displayFrame(DisplayFrame(endpointId: moved.subscription.endpointId, contextGeneration: 2,
                                                encoderSequence: 3, producerTimestamp: 2, isKeyframe: false,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -90, -40, -100, -80, -100],
                                                waterfallDbm: [-100, -100, -100, -100, -100, -100], wideDbm: [])))
        #expect(main.band.state.committedSerial == oldSerial)

        main.receive(.mediaState(.closed))
        #expect(main.subscriber.acceptedDisplay == nil)
        #expect(main.sMeter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        main.receive(.mediaState(.connected))
        #expect(await settle { main.subscriber.acceptedDisplay != nil })
        #expect(main.sMeter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        let fresh = try #require(main.subscriber.acceptedDisplay)
        payload["endpointId"] = .number(Double(fresh.subscription.endpointId))
        payload["revision"] = .number(Double(fresh.subscription.revision))
        // The new media may reuse every identifier the cached old frame
        // had. Its context alone cannot revive that old frame.
        payload["contextGeneration"] = .number(2)
        main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: false))))
        _ = main.band.prepareToDraw(size: CGSize(width: 320, height: 480), scale: 2)
        main.receive(.displayExtras(DisplayExtras(endpointId: fresh.subscription.endpointId, contextGeneration: 2,
                                                  encoderSequence: 3, peakBlobs: nil, peakHoldDbm: nil,
                                                  noiseFloorDbm: nil, waterfallLevels: nil)))
        main.sMeter.refresh()
        #expect(main.band.state.committedSerial == oldSerial)
        #expect(main.sMeter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        main.receive(.displayFrame(DisplayFrame(endpointId: fresh.subscription.endpointId, contextGeneration: 2,
                                                encoderSequence: 1, producerTimestamp: 0, isKeyframe: true,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -90, -60, -100, -80, -100],
                                                waterfallDbm: [-100, -100, -100, -100, -100, -100], wideDbm: [])))
        _ = main.band.prepareToDraw(size: CGSize(width: 320, height: 480), scale: 2)
        main.sMeter.refresh()
        #expect(main.sMeter.reason(for: .maxBin) == nil)
        #expect(main.sMeter.display.right == "-60.0 dBm")
    }

    @Test("Max Bin is off without this slice's pan trace; a stray wire property cannot enable it")
    func maxBinNeedsPan() async throws {
        let (model, station, _) = try await connected()
        let meter = model.main.sMeter
        #expect(meter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        #expect(meter.reason(for: .signalAverage) == nil)

        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-80)),
            .init(ordinal: 17, name: "signalAverageDbm", value: .f64(-90)),
            .init(ordinal: 200, name: "signalMaxBinDbm", value: .f64(-70)),
        ])))
        #expect(await settle { meter.state.readings.peakDbm == -80 })
        #expect(meter.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        #expect(meter.display.right == "-80.0 dBm")
        meter.chooseRx(.signalAverage)
        #expect(meter.display.right == "-90.0 dBm")
        #expect(meter.display.caption == "Sig Avg")
        await model.disconnect()
    }

    @Test("the transmit modes are off without the Core's transmit readings; Compression waits for its reading")
    func transmitModes() async throws {
        let (plain, plainStation, _) = try await connected()
        for mode in SMeterTxMode.allCases {
            #expect(plain.main.sMeter.reason(for: mode) == SMeterReadings.transmitNotSentText)
        }
        _ = plainStation
        await plain.disconnect()

        let (model, station, _) = try await connected(additions: [.remoteTx])
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: TransmitScreenTests.holder(
                                                                        "", short: "", keyed: false))))
        let meter = model.main.sMeter
        #expect(await settle { meter.reason(for: .power) == nil })
        #expect(meter.reason(for: .swr) == nil)
        #expect(meter.reason(for: .level) == nil)
        #expect(meter.reason(for: .compression) == SMeterReadings.compressionNotSentText)

        // On the air, the needle reads the TX Mode's reading.
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 0, name: "keyed", value: .bool(true)),
            .init(ordinal: 9, name: "forwardPowerWatts", value: .f64(50)),
            .init(ordinal: 11, name: "swr", value: .f64(1.5)),
            .init(ordinal: 13, name: "micLevelDb", value: .f64(-12)),
        ]))
        #expect(await settle { meter.display.transmitting && meter.display.right == "50 W" })
        meter.chooseTx(.swr)
        #expect(meter.display.right.hasPrefix("1.5"))
        meter.chooseTx(.level)
        #expect(meter.display.right == "-12 dB")

        // Compression lights with the Core's transmit readings and the reading itself.
        var capabilities = model.mirror.capabilities
        capabilities[TransmitModel.txReadingsCapability] = .int(1)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 33, name: TransmitModel.compressionReading, value: .f64(-6)),
        ]))
        #expect(await settle { meter.reason(for: .compression) == nil })
        meter.chooseTx(.compression)
        #expect(meter.display.right == "-6 dB")
        await model.disconnect()
    }

    @Test("the menu's choices are kept on this device and come back with the app")
    func choicesKept() async throws {
        let (model, _, defaults) = try await connected()
        let meter = model.main.sMeter
        #expect(meter.state.settings == .desktopDefaults)
        meter.chooseFace(.carbon)
        meter.chooseRx(.signalPeak)
        meter.chooseTx(.swr)
        meter.setPeakHold(false)
        meter.choosePeakDecay(.fast)
        let chosen = SMeterSettings(rxMode: .signalPeak, txMode: .swr, peakHold: false, peakDecay: .fast,
                                    face: .carbon)
        #expect(SMeterSettingsStore(defaults: defaults).settings == chosen)
        await model.disconnect()

        let again = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             meterSettings: SMeterSettingsStore(defaults: defaults))
        #expect(again.main.sMeter.state.settings == chosen)
    }

    @Test("the Multimeter page's units and decimal point print the face's, the flag's and the Live Activity's reading, and come back with the app")
    func unitsAndDecimal() async throws {
        let (model, station, defaults) = try await connected()
        let meter = model.main.sMeter
        #expect(meter.state.settings.readout == .desktopDefaults)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-85.64)),
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-85.64)),
        ])))
        #expect(await settle { meter.state.readings.peakDbm == -85.64
            && model.main.slices.active?.signalDbm == -85.64 })
        let sMeter = try #require(meter.meters?.sMeter)
        func activityText() -> String? {
            LiveActivityController.inputs(app: model, flow: nil).slice?.signalText
        }
        // dBm with the decimal point, the desktop's default.
        #expect(meter.display.right == "-85.6 dBm")
        #expect(model.main.meterReadout == .desktopDefaults)
        #expect(activityText() == "-85.6 dBm")
        // Without it.
        meter.setShowDecimal(false)
        #expect(meter.display.right == "-86 dBm")
        #expect(meter.display.left == SMeterReadout(unit: .sUnits, showDecimal: false).sUnits(dbm: -85.64, meter: sMeter))
        #expect(await settle { model.main.meterReadout.showDecimal == false })
        #expect(activityText() == "-86 dBm")
        // Microvolts at 50 ohms.
        meter.chooseUnit(.microvolts)
        #expect(meter.display.right == "12 uV")
        #expect(await settle { model.main.meterReadout.unit == .microvolts })
        #expect(activityText() == "12 uV")
        meter.setShowDecimal(true)
        #expect(meter.display.right == "11.7 uV")
        #expect(LiveActivityController.inputs(app: model, flow: nil).slice?.signalSpoken == "11.7 microvolts")
        // S-units: the left readout says it, the flag and the card print it.
        meter.chooseUnit(.sUnits)
        let sText = SMeterReadout(unit: .sUnits, showDecimal: true).sUnits(dbm: -85.64, meter: sMeter)
        #expect(sText.contains("."))
        #expect(meter.display.left == sText && meter.display.right == "")
        #expect(await settle { model.main.meterReadout.unit == .sUnits })
        #expect(activityText() == sText)
        let kept = SMeterSettingsStore(defaults: defaults).settings.readout
        #expect(kept == SMeterReadout(unit: .sUnits, showDecimal: true))
        await model.disconnect()

        let again = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             meterSettings: SMeterSettingsStore(defaults: defaults))
        #expect(again.main.sMeter.state.settings.readout == kept)
        #expect(again.main.meterReadout == kept)
    }

    // MARK: The fake Core

    private func connected(additions: FakeStation.Additions = []) async throws
        -> (AppModel, FakeStation, UserDefaults) {
        let defaults = try #require(UserDefaults(suiteName: "SMeterModelTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             meterSettings: SMeterSettingsStore(defaults: defaults))
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.active != nil })
        return (model, station, defaults)
    }

    private func settle(seconds: Double = 5, _ condition: () -> Bool) async -> Bool {
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
