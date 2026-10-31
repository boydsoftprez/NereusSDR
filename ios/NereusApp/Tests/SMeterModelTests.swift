// NereusSDR for iOS: the analog S-meter reads the Core's readings by the menu's choices, keeps them on this device, and lights modes as the Core sends them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-31: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   passive MAX diagnostic format, bounds, raw holding and lifecycle coverage.

import Foundation
import CoreGraphics
import NereusBand
import NereusModels
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

    @Test("The flag's more menu sets the S-meter source: every flag reads Peak, Average or Max Bin, kept on this device")
    func flagMeterSource() async throws {
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
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-90)),
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-80)),
            .init(ordinal: 17, name: "signalAverageDbm", value: .f64(-90)),
            .init(ordinal: 27, name: "panKey", value: .utf8("pan-0")),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        let defaults = try #require(UserDefaults(suiteName: "SMeterFlagTests-\(UUID().uuidString)"))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in }),
                                   meterSettings: SMeterSettingsStore(defaults: defaults))
        _ = main.band.prepareToDraw(size: CGSize(width: 320, height: 480), scale: 2)
        main.receive(.mediaState(.connected))
        #expect(await settle { main.subscriber.acceptedDisplay != nil && main.slices.active != nil })
        let entry = try #require(main.slices.active)

        // The desktop's default, S-Meter: the flag reads the peak, with no tag.
        #expect(main.meterRxMode == .signal)
        #expect(FlagMeterChoice(main.meterRxMode).tag == nil)
        #expect(main.sMeter.flagLevel(for: entry) == -80)
        // Max Bin is greyed, with the reason, until this slice's pan is drawn.
        #expect(main.sMeter.flagMaxBinReason(for: entry) == SMeterReadings.maxBinUnavailableText)

        main.flagControls.pickFlagMeter(.average)
        #expect(await settle { main.meterRxMode == .signalAverage })
        #expect(FlagMeterChoice(main.meterRxMode).tag == "AV")
        #expect(main.sMeter.flagLevel(for: entry) == -90)

        let request = try #require(main.subscriber.acceptedDisplay).subscription
        let payload: [String: LinkJSON] = [
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
        #expect(main.sMeter.flagMaxBinReason(for: entry) == nil)
        main.flagControls.pickFlagMeter(.maxBin)
        #expect(await settle { main.meterRxMode == .maxBin })
        #expect(FlagMeterChoice(main.meterRxMode).tag == "MAX")
        #expect(main.sMeter.flagLevel(for: entry) == -50)

        // The pan is drawn, but tuned away from the slice: Max Bin says the
        // passband is outside the span, not that the display is closed.
        var away = payload
        away["contextGeneration"] = .number(2)
        away["centreHz"] = .number(7_300_000)
        away["sourceCentreHz"] = .number(7_300_000)
        main.receive(.context(try #require(MediaControlDecoder.context(away, wideband: false, grant: false))))
        main.receive(.displayFrame(DisplayFrame(endpointId: request.endpointId, contextGeneration: 2,
                                                encoderSequence: 2, producerTimestamp: 0, isKeyframe: true,
                                                waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                                traceDbm: [-100, -90, -50, -100, -80, -100],
                                                waterfallDbm: [-100, -100, -100, -100, -100, -100], wideDbm: [])))
        #expect(await settle { main.sMeter.flagMaxBinReason(for: entry) == SMeterReadings.maxBinOutsideSpanText })
        #expect(main.sMeter.flagLevel(for: entry) == nil)

        // One of the more menu's lists is open at a time.
        main.flagControls.toggleRateList()
        main.flagControls.toggleMeterList()
        #expect(main.flagControls.meterListOpen && !main.flagControls.rateListOpen)
        main.flagControls.toggleRateList()
        #expect(main.flagControls.rateListOpen && !main.flagControls.meterListOpen)

        // Kept on this device, with the meter's other choices.
        #expect(SMeterSettingsStore(defaults: defaults).settings.rxMode == .maxBin)
        main.flagControls.pickFlagMeter(.peak)
        #expect(await settle { main.meterRxMode == .signal })
        #expect(main.sMeter.flagLevel(for: entry) == -80)
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

    @Test("Max Bin diagnostics retain signed geometry and use only bounded scalar fields")
    func maxBinDiagnosticFormat() {
        var snapshot = SMeterModel.MaxBinDiagnostic(sliceId: 0, gate: .panMismatch)
        snapshot.slicePanPresent = true
        snapshot.acceptedPresent = true
        snapshot.acceptedSliceId = 1
        snapshot.acceptedEndpoint = 7
        snapshot.acceptedRevision = 12
        snapshot.panMatches = false
        snapshot.bandEndpoint = 7
        snapshot.frameEndpoint = 7
        snapshot.frameGeneration = 4
        snapshot.frameRevision = 12
        snapshot.coveragePresent = true
        snapshot.traceCount = 402
        snapshot.frequencyHz = 14_261_000
        snapshot.passbandOriginHz = 14_261_000
        snapshot.filterLowHz = -2_900
        snapshot.filterHighHz = -100
        snapshot.coverageCenterHz = 14_261_000
        snapshot.coverageSpanHz = 48_000
        snapshot.subscriberState = .init(mediaUp: true,
            accepted: .init(sliceId: 0, endpointId: 7, revision: 12),
            requested: .init(sliceId: 1, endpointId: 7, revision: 13), outstandingOperations: 1, asking: false)
        let text = snapshot.logText(repeated: false)
        #expect(text.contains("gate=panMismatch slice=0"))
        #expect(text.contains("acceptedEndpoint=7 acceptedRevision=12"))
        #expect(text.contains("mediaUp=1 rawAccepted=1 rawAcceptedSlice=0 rawAcceptedEndpoint=7 rawAcceptedRevision=12 requested=1 requestedSlice=1 requestedEndpoint=7 requestedRevision=13 outstanding=1 asking=0"))
        #expect(text.contains("panMatch=0"))
        #expect(text.contains("filterLowHz=-2900.000 filterHighHz=-100.000"))
        #expect(text.contains("valuePresent=0"))
        #expect(!text.contains("panKey"))
        snapshot.coverageSpanHz = .nan
        #expect(snapshot.logText(repeated: true).contains("coverageSpanHz=nonfinite"))
        #expect(snapshot.logText(repeated: true).contains("repeated=1"))
    }

    @Test("Max Bin diagnostics throttle per slice, report one persistent failure, and recover")
    func maxBinDiagnosticRate() {
        var limiter = SMeterModel.MaxBinDiagnosticLimiter()
        let missing = SMeterModel.MaxBinDiagnostic(sliceId: 0, gate: .acceptedDisplayMissing)
        #expect(limiter.record(missing, at: 10) == false)
        #expect(limiter.record(missing, at: 10.99) == nil)
        #expect(limiter.record(missing, at: 11) == true)
        #expect(limiter.record(missing, at: 12) == nil)
        var ready = missing
        ready.gate = .eligible
        ready.valuePresent = true
        #expect(limiter.record(ready, at: 12) == false)
        #expect(limiter.record(ready, at: 13) == nil)
        var changed = ready
        changed.frameRevision = 2
        #expect(limiter.record(changed, at: 12.5) == nil)
        #expect(limiter.record(changed, at: 13) == false)
        let otherSlice = SMeterModel.MaxBinDiagnostic(sliceId: 1, gate: .acceptedDisplayMissing)
        #expect(limiter.record(otherSlice, at: 13) == false)
        #expect(limiter.record(otherSlice, at: .nan) == nil)
        var malformed = SMeterModel.MaxBinDiagnostic(sliceId: 2, gate: .measurementUnavailable)
        malformed.coverageSpanHz = .nan
        #expect(limiter.record(malformed, at: 13) == false)
        #expect(limiter.record(malformed, at: 14) == true)
        #expect(limiter.record(malformed, at: 15) == nil)
    }

    @Test("The passive flag snapshot names the first unchanged MAX guard and observes the fake clock")
    func maxBinDiagnosticFirstFailure() async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [
            .init(name: "sliceIndex", value: .i64(0)), .init(name: "active", value: .bool(true)),
            .init(name: "frequency", value: .f64(14_261_000)),
            .init(name: "filterLow", value: .i64(100)), .init(name: "filterHigh", value: .i64(2_900)),
            .init(name: "panKey", value: .utf8("private-pan-name-never-logged")),
        ])))
        let defaults = try #require(UserDefaults(suiteName: "MaxBinDiagnostic-\(UUID().uuidString)"))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
            operations: .init(subscribe: { _ in Issue.record("No diagnostic display request") },
                              unsubscribe: { _ in Issue.record("No diagnostic display release") }),
            displaySettings: BandDisplaySettingsStore(defaults: defaults),
            meterSettings: SMeterSettingsStore(defaults: defaults))
        try #require(await settle { main.slices.active != nil })
        var now = 0.0
        let meter = SMeterModel(slices: main.slices, band: main.band, subscriber: main.subscriber,
            transmit: main.transmit, catalogFeed: main.catalogFeed,
            store: SMeterSettingsStore(defaults: defaults), clock: { now })
        var captures: [(SMeterModel.MaxBinDiagnostic, Bool)] = []
        meter.maxBinDiagnostic = { captures.append(($0, $1)) }
        var entry = try #require(main.slices.active)
        entry.panKey = nil
        #expect(meter.flagMaxBinReason(for: entry) == SMeterReadings.maxBinUnavailableText)
        #expect(captures.last?.0.gate == .slicePanMissing)
        entry.panKey = "private-pan-name-never-logged"
        now = 1
        #expect(meter.flagMaxBinReason(for: entry) == SMeterReadings.maxBinUnavailableText)
        #expect(captures.last?.0.gate == .acceptedDisplayMissing)
        #expect(captures.last?.0.slicePanPresent == true && captures.last?.0.acceptedPresent == false)
        #expect(captures.last?.0.valuePresent == false)
        #expect(captures.last?.0.logText(repeated: false).contains("private-pan-name") == false)
        now = 2
        _ = meter.flagMaxBin(for: entry)
        #expect(captures.last?.1 == true)
        now = 3
        _ = meter.flagMaxBin(for: entry)
        #expect(captures.count == 3)
    }

    @Test("Max Bin recovers its accepted pan after a jump, take and sound-only return",
          arguments: [false, true])
    func maxBinAssociationAfterJumpAndResume(frameBeforeAllocation: Bool) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(.init(properties: [
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "remoteDisplayBudgetVersion", value: .i64(1)),
            .init(name: "displayApplicationBytesPerSecond", value: .i64(50_000_000)),
            .init(name: "spectrumSampleUnitsPerSecond", value: .i64(50_000_000)),
            .init(name: "displayBudgetGeneration", value: .i64(1)),
            .init(name: "displayBudgetReason", value: .utf8("none")),
        ])))
        for id in 0 ... 1 {
            mirror.apply(.objectCreate(.init(key: "slice:\(id)", className: "SliceModel", properties: [
                .init(name: "frequency", value: .f64(id == 0 ? 14_261_000 : 7_236_400)),
                .init(name: "filterLow", value: .i64(100)),
                .init(name: "filterHigh", value: .i64(2_900)),
                .init(name: "active", value: .bool(id == 0)),
                .init(name: "sliceIndex", value: .i64(Int64(id))),
                .init(name: "signalPeakDbm", value: .f64(-89.3)),
                .init(name: "signalAverageDbm", value: .f64(-95)),
                .init(name: "panKey", value: .utf8("pan-\(id)")),
                .init(name: "streamIndex", value: .i64(Int64(id))),
                .init(name: "sampleRateHz", value: .i64(192_000)),
            ])))
        }
        var subscriptions: [DisplaySubscription] = []
        var releases: [UInt32] = []
        let defaults = try #require(UserDefaults(suiteName: "MaxBinAssociation-\(UUID().uuidString)"))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
            operations: .init(subscribe: { subscriptions.append($0) }, unsubscribe: { releases.append($0) }),
            meterSettings: SMeterSettingsStore(defaults: defaults))
        var diagnostics: [SMeterModel.MaxBinDiagnostic] = []
        main.sMeter.maxBinDiagnostic = { snapshot, _ in diagnostics.append(snapshot) }
        _ = main.band.prepareToDraw(size: CGSize(width: 402, height: 800), scale: 3)
        main.receive(.mediaState(.connected))
        try #require(await settle { subscriptions.last?.sliceId == 0 })

        func allocation(_ request: DisplaySubscription, held: Bool = true) throws -> MediaControlEvent {
            let payload: [String: LinkJSON] = [
                "op": .string("allocation-result"), "connectionId": .string("meter-association"),
                "endpointId": .number(Double(request.endpointId)), "revision": .number(Double(request.revision)),
                "accepted": .bool(true), "reason": .string(""), "budgetGeneration": .number(1),
                "acceptedRevision": .number(held ? Double(request.revision) : 0),
                "applicationBytesPerSecond": .number(held ? 1000 : 0),
                "spectrumSampleUnitsPerSecond": .number(held ? 1000 : 0),
                "messagesPerSecond": .number(held ? 30 : 0),
            ]
            return .allocationResult(try #require(MediaControlDecoder.allocationResult(payload)))
        }

        func acceptAndDraw(_ request: DisplaySubscription, generation: UInt32) throws {
            let entry = try #require(main.slices.entries.first { $0.id == request.sliceId })
            let payload: [String: LinkJSON] = [
                "op": .string("context"), "connectionId": .string("meter-association"),
                "endpointId": .number(Double(request.endpointId)), "revision": .number(Double(request.revision)),
                "contextGeneration": .number(Double(generation)), "sourceStream": .number(Double(request.sliceId)),
                "sourceCentreHz": .number(entry.slice.frequencyHz), "sampleRateHz": .number(192_000),
                "centreHz": .number(entry.slice.frequencyHz), "spanHz": .number(12_000),
                "wideCentreHz": .number(0), "wideSpanHz": .number(0),
                "traceSamples": .number(13), "waterfallSamples": .number(13), "wideSamples": .number(0),
                "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
            ]
            if !frameBeforeAllocation { main.receive(try allocation(request)) }
            main.receive(.context(try #require(MediaControlDecoder.context(payload, wideband: false, grant: false))))
            main.receive(.displayFrame(.init(endpointId: request.endpointId, contextGeneration: generation,
                encoderSequence: 1, producerTimestamp: 0, isKeyframe: true, waterfallAdvance: true,
                minDbm: -160, maxDbm: 0, traceDbm: Array(repeating: -83, count: 13),
                waterfallDbm: Array(repeating: -100, count: 13), wideDbm: [])))
            if frameBeforeAllocation { main.receive(try allocation(request)) }
            #expect(main.subscriber.acceptedDisplay?.panKey == entry.panKey)
            #expect(main.subscriber.acceptedDisplay?.subscription.revision == main.band.frameRevision)
            #expect(main.band.state.frame?.endpointId == request.endpointId)
            #expect(main.sMeter.flagMaxBinReason(for: entry) == nil)
            #expect(main.sMeter.flagMaxBin(for: entry).value == -83)
        }

        try acceptAndDraw(try #require(subscriptions.last), generation: 1)
        main.sMeter.chooseFlagMeter(.maxBin)
        #expect(main.sMeter.flagLevel(for: try #require(main.slices.active)) == -83)

        main.slices.show(1)
        try #require(await settle { subscriptions.last?.sliceId == 1 })
        // A new requested slice hides acceptedDisplay before its allocation
        // answer, but diagnostics must retain the previous raw holding.
        #expect(main.subscriber.acceptedDisplay == nil)
        let transitioning = try #require(main.slices.active)
        #expect(main.sMeter.flagMaxBinReason(for: transitioning) == SMeterReadings.maxBinUnavailableText)
        let transitionSnapshot = try #require(diagnostics.last)
        #expect(transitionSnapshot.gate == .acceptedDisplayMissing && !transitionSnapshot.acceptedPresent)
        #expect(transitionSnapshot.subscriberState.mediaUp)
        #expect(transitionSnapshot.subscriberState.accepted?.sliceId == 0)
        #expect(transitionSnapshot.subscriberState.requested?.sliceId == 1)
        #expect(transitionSnapshot.subscriberState.accepted?.endpointId
            == transitionSnapshot.subscriberState.requested?.endpointId)
        #expect(transitionSnapshot.subscriberState.accepted?.revision == 1)
        #expect(transitionSnapshot.subscriberState.requested?.revision == 2)
        #expect(transitionSnapshot.subscriberState.outstandingOperations == 1)
        #expect(transitionSnapshot.logText(repeated: false).contains("accepted=0 acceptedSlice=absent"))
        #expect(transitionSnapshot.logText(repeated: false).contains("mediaUp=1 rawAccepted=1 rawAcceptedSlice=0"))
        try acceptAndDraw(try #require(subscriptions.last), generation: 2)
        // The accepted outcome's local bookkeeping retains the previous band.
        // SliceListTests separately covers the actual take command/access delta.
        main.slices.tookControl(1, revision: 13)
        #expect(main.slices.jumped)
        #expect(main.sMeter.flagLevel(for: try #require(main.slices.active)) == -83)

        main.slices.back()
        try #require(await settle { subscriptions.last?.sliceId == 0 })
        try acceptAndDraw(try #require(subscriptions.last), generation: 3)
        #expect(!main.slices.jumped)

        let beforeAway = try #require(subscriptions.last)
        main.subscriber.session = SessionPolicy.Mode.audioOnly.request
        try #require(await settle { releases.contains(beforeAway.endpointId) })
        var released = beforeAway
        released.revision &+= 1
        main.receive(try allocation(released, held: false))
        #expect(main.sMeter.flagMaxBinReason(for: try #require(main.slices.active))
            == SMeterReadings.maxBinUnavailableText)
        main.subscriber.session = SessionPolicy.Mode.full.request
        try #require(await settle { subscriptions.last?.endpointId != beforeAway.endpointId })
        try acceptAndDraw(try #require(subscriptions.last), generation: 4)
        #expect(main.sMeter.flagLevel(for: try #require(main.slices.active)) == -83)
    }

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

@MainActor
extension SMeterModelTests {
    @Test("Shared amplifier source and display readings")
    func sharedAmplifierMeterRule() {
        let main = TransmitMeterFixture.main()
        #expect(main.sMeter.display.right == "1236 W")
        #expect(main.sMeter.display.needle == 1236.0 / 2000)
        #expect(main.sMeter.display.transmitScale?.redFrom == 0.75)
        let absent = TransmitMeterFixture.main(present: false, radioWatts: 98)
        #expect(absent.sMeter.display.right == "98 W")
        let idle = TransmitMeterFixture.main(keyed: false)
        #expect(!idle.sMeter.display.transmitting)
    }
}

@MainActor
extension SMeterModelTests {
    @Test("A late catalog restarts a visible needle already settled without a power scale")
    func lateCatalogRestartsNeedle() async throws {
        let txMirror = MirrorStore(send: { _ in Issue.record("The meter sends no message") })
        let main = TransmitMeterFixture.main(operate: false, radioWatts: 87, store: txMirror)
        try #require(await ShotWait.until { main.catalogFeed.catalog != nil })
        main.transmit.refresh()
        let mirror = MirrorStore(send: { _ in Issue.record("The meter sends no message") })
        let catalog = CatalogFeed(store: mirror)
        let defaults = try #require(UserDefaults(suiteName: "LateMeterCatalog-\(UUID().uuidString)"))
        let meter = SMeterModel(slices: main.slices, band: main.band, subscriber: main.subscriber,
            transmit: main.transmit, catalogFeed: catalog, store: SMeterSettingsStore(defaults: defaults))
        meter.appeared()
        defer { meter.disappeared() }
        // A distinct published reading drains constructor refresh work without a timed sleep.
        txMirror.apply(.delta(.init(key: "txState", properties: [
            .init(name: "forwardPowerWatts", value: .f64(98))])))
        try #require(await ShotWait.until { meter.display.right == "98 W" })
        meter.frame()
        #expect(meter.display.right == "98 W")
        #expect(meter.display.needle == 0 && meter.needle.settled(on: 0))
        let json = try #require(MainScreenTests.catalogueJSON())
        mirror.apply(.objectCreate(.init(key: "catalog", className: "StationCatalog", properties: [
            .init(name: "json", value: .utf8(json)), .init(name: "revision", value: .i64(1))])))
        mirror.apply(.snapshotComplete)
        try #require(await ShotWait.until {
            meter.meters != nil && meter.display.needle > 0 && meter.needle.settled(on: meter.display.needle)
        }, "A current nonzero reading must move the needle when its scale arrives")
        #expect(meter.display.right == "98 W")
        #expect(meter.display.needle == 98 / (try #require(meter.meters)).rfPower.maxW)
    }
}
