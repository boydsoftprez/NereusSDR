// NereusSDR for iOS: keyed meter missing values and the native stage scale contract
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import Testing

@Suite("Keyed meter readings")
struct KeyedMeterOverlayTests {
    @Test("missing and invalid readings stay missing, including the Core's mic sentinel")
    func missingReadings() {
        #expect(KeyedMeterSample(name: "Radio RF", value: nil, unit: "W", scale: .rfPower(nil)).readout == "--")
        #expect(KeyedMeterSample(name: "Radio RF", value: .nan, unit: "W", scale: .rfPower(nil)).value == nil)
        #expect(KeyedMeterSample(name: "Radio Mic", value: -400, unit: "dB", scale: .micLevel, minimum: -399).value == nil)
        #expect(KeyedMeterSample(name: "Amp SWR", value: 0, unit: "", scale: .ampSwr, minimum: 1).readout == "--")
        #expect(KeyedMeterSample(name: "Radio RF", value: 0, unit: "W", scale: .rfPower(nil)).readout == "0 W")
    }

    @Test("stage fill and peak use the nonlinear stage position and preserve the numeric reading")
    func stageScale() {
        let sample = KeyedMeterSample(name: "EQ", value: 0, unit: "dB", scale: .micLevel,
                                     stage: TxStage.all[0], peak: 12)
        #expect(abs((sample.position ?? -1) - 0.665) < 1e-9)
        #expect(abs((sample.peakPosition ?? -1) - 0.99) < 1e-9)
        #expect(sample.readout == "0.0 dB")
        let negative = KeyedMeterSample(name: "EQ", value: -4.2, unit: "dB", scale: .micLevel,
                                       stage: TxStage.all[0], peak: .nan)
        #expect(negative.readout == "-4.2 dB")
        #expect(negative.peakPosition == nil)
    }

    @Test("current RF-Kit zero samples are real, missing and disconnected samples remain unavailable")
    @MainActor
    func nullableSourceMappings() async throws {
        let platform = TestPlatform()
        let model = AppModel(platform: platform.platform)
        let station = try FakeStation(additions: [.remoteTx, .txStageReadings])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.mirror.isSnapshotComplete })
        let main = model.main
        model.mirror.apply(.objectCreate(.init(key: "rfkit", className: "RfKitState", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true))])))
        main.accessories.refresh()
        #expect(main.accessories.rfKitReadings.forwardW == nil)
        #expect(main.accessories.rfKitReadings.swr == nil)
        #expect(KeyedMeterReadings.sample(.ampPower, transmit: main.transmit, accessories: main.accessories,
                                         meters: nil).readout == "--")
        model.mirror.apply(.delta(.init(key: "rfkit", properties: [
            .init(name: "forwardPowerW", value: .f64(0)), .init(name: "swr", value: .f64(1)),
            .init(name: "temperatureC", value: .f64(0))])))
        main.accessories.refresh()
        #expect(KeyedMeterReadings.sample(.ampPower, transmit: main.transmit, accessories: main.accessories,
                                         meters: nil).readout == "0 W")
        #expect(KeyedMeterReadings.sample(.ampSwr, transmit: main.transmit, accessories: main.accessories,
                                         meters: nil).readout == "1.00:1")
        #expect(KeyedMeterReadings.sample(.ampTemperature, transmit: main.transmit, accessories: main.accessories,
                                         meters: nil).readout == "0 °C")
        model.mirror.apply(.delta(.init(key: "rfkit", properties: [
            .init(name: "forwardPowerW", value: .f64(-1)), .init(name: "swr", value: .f64(0)),
            .init(name: "temperatureC", value: .f64(.nan))])))
        main.accessories.refresh()
        #expect(main.accessories.rfKitReadings.forwardW == nil && main.accessories.rfKitReadings.swr == nil)
        #expect(main.accessories.rfKitReadings.temperatureC == nil)
        model.mirror.apply(.delta(.init(key: "rfkit", properties: [
            .init(name: "temperatureC", value: .f64(-5))])))
        main.accessories.refresh()
        #expect(main.accessories.rfKitReadings.temperatureC == -5)
        model.mirror.apply(.delta(.init(key: "rfkit", properties: [
            .init(name: "connectionPhase", value: .enumeration(5))])))
        main.accessories.refresh()
        #expect(main.accessories.rfKitReadings.forwardW == nil)
        #expect(KeyedMeterReadings.availability(transmit: main.transmit, accessories: main.accessories).amp
                == .offline(AccessoriesModel.rfKitNotConnectedReason))
        model.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties:
            [.init(name: "keyed", value: .bool(true)), .init(name: "forwardPowerWatts", value: .f64(0)),
             .init(name: "swr", value: .f64(1)), .init(name: "micLevelDb", value: .f64(-400))]
                + TxStageMetersTests.stages([-4.2, -2.6, 5.8, -3.1, 2.4, 4.1, -1.9]))))
        main.transmit.refresh()
        #expect(main.transmit.keyedForwardWatts == 0 && main.transmit.keyedSwr == 1)
        #expect(main.transmit.keyedMicLevelDb == nil)
        #expect(KeyedMeterReadings.sample(.eq, transmit: main.transmit, accessories: main.accessories,
                                         meters: nil).readout == "-4.2 dB")
        model.mirror.apply(.delta(.init(key: "rfkit", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "forwardPowerW", value: .f64(1500)),
            .init(name: "swr", value: .f64(1.1)), .init(name: "temperatureC", value: .f64(55))])))
        main.accessories.refresh()
        #expect(main.accessories.rfKitReadings.forwardW == 1500)
        // Link retirement preserves historical objects but never current keyed values or stage peaks.
        model.mirror.handle(.stateChanged(.stopped))
        main.transmit.refresh()
        main.accessories.refresh()
        #expect(main.transmit.keyedForwardWatts == nil && main.transmit.keyedSwr == nil)
        #expect(main.accessories.rfKitReadings.forwardW == nil && main.accessories.rfKitReadings.temperatureC == nil)
        let stale = KeyedMeterReadings.sample(.eq, transmit: main.transmit, accessories: main.accessories, meters: nil)
        #expect(stale.readout == "--" && stale.peakPosition == nil)
        #expect(main.transmit.stageReadings[0] == -4.2)
        await model.disconnect()
    }

    @Test("an older Core disables stage choices and suppresses any historical stage sample")
    @MainActor
    func oldCoreStageReadings() async throws {
        let platform = TestPlatform()
        let model = AppModel(platform: platform.platform)
        let station = try FakeStation(additions: [.remoteTx])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.mirror.isSnapshotComplete })
        model.main.transmit.refresh()
        let available = KeyedMeterReadings.availability(transmit: model.main.transmit, accessories: model.main.accessories)
        #expect(available.status(for: .eq) == .offline(TxStageMeters.olderCoreText))
        #expect(available.pickerMeters.contains(.eq))
        let sample = KeyedMeterReadings.sample(.eq, transmit: model.main.transmit, accessories: model.main.accessories, meters: nil)
        #expect(sample.readout == "--" && sample.reason == TxStageMeters.olderCoreText)
        await model.disconnect()
    }

}
