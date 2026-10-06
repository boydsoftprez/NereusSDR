// NereusSDR for iOS: current keyed meter samples without fabricated accessory readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

struct KeyedMeterSample {
    let name: String
    let value: Double?
    let unit: String
    let scale: LinearGauge.Scale
    var stage: TxStage? = nil
    var peak: Double? = nil
    var reason: String? = nil
    init(name: String, value: Double?, unit: String, scale: LinearGauge.Scale,
         minimum: Double? = 0, stage: TxStage? = nil, peak: Double? = nil, reason: String? = nil) {
        self.name = name
        let cleaned = stage == nil ? value : TxStage.reading(value)
        let bound = stage == nil ? minimum : nil
        if let value = cleaned, value.isFinite, bound.map({ value >= $0 }) ?? true {
            self.value = value
        } else {
            self.value = nil
        }
        self.unit = unit
        self.scale = scale
        self.stage = stage
        self.peak = peak
        self.reason = reason
    }
    var position: Double? {
        value.map { stage?.position($0) ?? linearPosition($0) }
    }
    var peakPosition: Double? {
        guard let peak, peak.isFinite else { return nil }
        return stage?.position(peak) ?? linearPosition(peak)
    }
    func linearPosition(_ value: Double) -> Double {
        guard scale.max > scale.min else { return 0 }
        return min(max((value - scale.min) / (scale.max - scale.min), 0), 1)
    }
    var readout: String {
        guard let value else { return "--" }
        switch unit {
        case "dB": return TxStage.readout(value)
        case "": return String(format: "%.2f:1", value)
        default: return String(format: "%.0f %@", value, unit)
        }
    }
}


/// Resolves display data only; choosing a meter never reaches a command client.
@MainActor
enum KeyedMeterReadings {
    static func usesPowerGenius(_ accessories: AccessoriesModel) -> Bool {
        if let amp = accessories.powerGenius,
           configured(amp.link, present: amp.present) { return true }
        return false
    }

    static func configured(_ link: AccessoriesModel.Link, present: Bool) -> Bool {
        present || !link.host.isEmpty || AccessoriesModel.isSetUp(link, present: present)
    }

    static func availability(transmit: TransmitModel, accessories: AccessoriesModel) -> KeyedMeterAvailability {
        let amp: KeyedMeterStatus
        if usesPowerGenius(accessories), let device = accessories.powerGenius {
            amp = status(device.link, present: device.present, current: accessories.readingsCurrent,
                         reason: AccessoriesModel.ampNotConnectedReason)
        } else if let device = accessories.rfKit {
            amp = status(device.link, present: device.present, current: accessories.readingsCurrent,
                         reason: AccessoriesModel.rfKitNotConnectedReason)
        } else { amp = .absent }
        let tuner = accessories.tunerGenius.map {
            status($0.link, present: $0.present, current: accessories.readingsCurrent,
                   reason: AccessoriesModel.tunerNotConnectedReason)
        } ?? .absent
        return KeyedMeterAvailability(amp: amp, tuner: tuner,
            stagesReason: transmit.txReadingsVersion < TxStage.readingsVersion ? TxStageMeters.olderCoreText : nil)
    }

    private static func status(_ link: AccessoriesModel.Link, present: Bool, current: Bool,
                               reason: String) -> KeyedMeterStatus {
        guard configured(link, present: present) else { return .absent }
        return current && present && link.connected ? .available : .offline(reason)
    }

    static func sample(_ meter: KeyedMeter, transmit: TransmitModel, accessories: AccessoriesModel,
                       meters: StationCatalog.Meters?) -> KeyedMeterSample {
        let available = availability(transmit: transmit, accessories: accessories)
        let pgxl = usesPowerGenius(accessories)
        let rfKitConfigured = accessories.rfKit.map { configured($0.link, present: $0.present) } ?? false
        let ampName = pgxl ? "PGXL" : rfKitConfigured ? "RF-Kit" : "Amp"
        let amp = pgxl ? accessories.powerGeniusReadings : accessories.rfKitReadings
        let unavailable = available.status(for: meter) != .available
        let status = available.status(for: meter)
        let absentReason: String? = status == .absent ? (meter.source == .amp
            ? "No amplifier is configured." : "No tuner is configured.") : nil
        let reason = status.reason ?? absentReason
        let noRadioReadings = transmit.keyedForwardWatts == nil && transmit.keyedSwr == nil && transmit.keyedMicLevelDb == nil
        if let index = meter.stageIndex {
            let value = unavailable || !transmit.keyedReadingsCurrent ? nil : TxStage.at(transmit.stageReadings, index)
            return KeyedMeterSample(name: meter.name, value: value, unit: "dB", scale: .micLevel,
                stage: TxStage.all[index], peak: !unavailable && transmit.keyedReadingsCurrent && transmit.stagesOnAir ? TxStage.at(transmit.stagePeaks, index) : nil,
                reason: reason ?? (value == nil ? TxStageMeters.noReadingsText : nil))
        }
        switch meter {
        case .rfPower:
            return KeyedMeterSample(name: meter.name, value: transmit.keyedForwardWatts, unit: "W",
                scale: .rfPower(meters?.rfPower), reason: transmit.keyedForwardWatts == nil ? "The Core has no radio readings right now." : nil)
        case .swr:
            return KeyedMeterSample(name: meter.name, value: transmit.keyedSwr, unit: "", scale: .swr(meters?.swr),
                minimum: 1, reason: transmit.keyedSwr == nil ? "The Core has no radio readings right now." : nil)
        case .mic:
            return KeyedMeterSample(name: meter.name, value: transmit.keyedMicLevelDb, unit: "dB", scale: .micLevel,
                minimum: nil, reason: transmit.keyedMicLevelDb == nil ? (noRadioReadings ? "The Core has no radio readings right now." : "The Core has no microphone reading right now.") : nil)
        case .ampPower:
            return KeyedMeterSample(name: "\(ampName) output", value: unavailable ? nil : amp.forwardW, unit: "W",
                scale: .ampPower, reason: reason ?? (amp.forwardW == nil ? "\(ampName) has no output reading right now." : nil))
        case .ampSwr:
            return KeyedMeterSample(name: "\(ampName) SWR", value: unavailable ? nil : amp.swr, unit: "", scale: .ampSwr,
                minimum: 1, reason: reason ?? (amp.swr == nil ? "\(ampName) has no SWR reading right now." : nil))
        case .ampTemperature:
            return KeyedMeterSample(name: "\(ampName) temperature", value: unavailable ? nil : amp.temperatureC,
                unit: "°C", scale: .ampTemperature, minimum: nil,
                reason: reason ?? (amp.temperatureC == nil ? "\(ampName) has no temperature reading right now." : nil))
        case .tunerPower:
            let value = unavailable ? nil : accessories.tunerGeniusReadings.forwardW
            let shown = transmit.powerControl.shown
            let watts = shown?.unit == "W" ? shown?.max ?? 0 : 0
            let amplifying = pgxl ? accessories.powerGenius?.operate == true && available.amp.isAvailable
                : accessories.rfKit?.operate == true && available.amp.isAvailable
            return KeyedMeterSample(name: "TGXL power", value: value, unit: "W",
                scale: TxAccessoryMeterScale.tunerPower(maxWatts: watts, amplifying: amplifying),
                reason: reason ?? (value == nil ? "TGXL has no power reading right now." : nil))
        case .tunerSwr:
            let value = unavailable ? nil : accessories.tunerGeniusReadings.swr
            return KeyedMeterSample(name: "TGXL SWR", value: value, unit: "", scale: TxAccessoryMeterScale.tunerSwr,
                minimum: 1, reason: reason ?? (value == nil ? "TGXL has no SWR reading right now." : nil))
        default:
            return KeyedMeterSample(name: meter.name, value: nil, unit: "dB", scale: .micLevel, reason: reason)
        }
    }
}
