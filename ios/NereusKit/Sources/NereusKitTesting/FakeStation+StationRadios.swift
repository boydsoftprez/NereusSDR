// NereusSDR for iOS: the fake Core's radios: the ones it can see, choosing, scanning and forgetting, and its radio readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's radios for the Radio tab. With ``Additions/stationRadios``
/// it plays a Core that chooses its own radio at `stationRadiosVersion` 1
/// (link document sections 6.3, 7.7 and 9.1): `records.subscribe` for
/// `stationRadios` answers with a reset of the scene's radios,
/// `station.selectRadio` makes a radio in sight the Core's and sends the
/// two changed records (the real Core then restarts its run and ends every
/// session; the fake stays up so a test can read the list),
/// `station.rescanRadios` is taken, and `station.forgetRadio` takes a radio
/// off the list, refusing the Core's own radio. Each answers a refusal
/// queued with ``refuseNext(_:reason:)`` and refuses a radio it cannot
/// see or a request it cannot read with the Core's words. With
/// ``SceneRadios/asksFirst`` a choice is answered "Waiting for you to
/// confirm.", as when another device is on the Core.
///
/// With ``Additions/radioTelemetry`` it advertises `stationTelemetryVersion`
/// 6, and ``deliverRadioTelemetry(_:)`` sends one sample. The radios and
/// readings are made up for the tests.
extension FakeStation {
    /// One radio the fake Core can see.
    public struct SceneRadio: Sendable, Equatable {
        public var mac: String
        public var name: String
        public var model: Int64
        public var address: String
        public var protocolVersion: Int64
        public var inUse: Bool
        /// The name Setup shows for `model`, sent with ``Additions/radioModels``.
        public var modelLabel: String
        /// The models the radio's board can run as, sent with ``Additions/radioModels``.
        public var models: [(model: Int64, label: String)]

        public init(mac: String, name: String, model: Int64, address: String = "", protocolVersion: Int64,
                    inUse: Bool = false, modelLabel: String = "", models: [(model: Int64, label: String)] = []) {
            self.mac = mac
            self.name = name
            self.model = model
            self.address = address
            self.protocolVersion = protocolVersion
            self.inUse = inUse
            self.modelLabel = modelLabel
            self.models = models
        }

        public static func == (left: SceneRadio, right: SceneRadio) -> Bool {
            left.mac == right.mac && left.name == right.name && left.model == right.model
                && left.address == right.address && left.protocolVersion == right.protocolVersion
                && left.inUse == right.inUse && left.modelLabel == right.modelLabel
                && left.models.map(\.model) == right.models.map(\.model)
                && left.models.map(\.label) == right.models.map(\.label)
        }
    }

    /// The radios the fake Core can see, its own first.
    public struct SceneRadios: Sendable, Equatable {
        public var radios: [SceneRadio]
        /// A choice is answered "Waiting for you to confirm.", as when
        /// another device is on the Core.
        public var asksFirst: Bool

        public init(radios: [SceneRadio], asksFirst: Bool = false) {
            self.radios = radios
            self.asksFirst = asksFirst
        }

        /// Two radios at documentation addresses: the Core runs the first.
        public static let bench = SceneRadios(radios: [
            SceneRadio(mac: "AA:BB:CC:DD:EE:01", name: "Bench HL2", model: 14, address: "192.0.2.21",
                       protocolVersion: 1, inUse: true, modelLabel: "Hermes Lite 2",
                       models: [(14, "Hermes Lite 2")]),
            SceneRadio(mac: "AA:BB:CC:DD:EE:02", name: "Bench G2", model: 12, address: "192.0.2.22",
                       protocolVersion: 2, modelLabel: "ANAN-G2 1K", models: [(11, "ANAN-G2"), (12, "ANAN-G2 1K")]),
        ])
    }

    /// One telemetry sample's radio and host readings; nil leaves one out.
    public struct RadioReadings: Sendable, Equatable {
        public var paVolts: Double?
        public var sampleRateHz: Int64?
        /// Each ADC as `(adc, overloaded)`, nil overloaded for a status not known; nil sends no list.
        public var adcOverloads: [ADCReading]?
        public var systemCpuPercent: Double?

        public struct ADCReading: Sendable, Equatable {
            public var adc: Int
            public var overloaded: Bool?

            public init(adc: Int, overloaded: Bool?) {
                self.adc = adc
                self.overloaded = overloaded
            }
        }

        public init(paVolts: Double? = nil, sampleRateHz: Int64? = nil, adcOverloads: [ADCReading]? = nil,
                    systemCpuPercent: Double? = nil) {
            self.paVolts = paVolts
            self.sampleRateHz = sampleRateHz
            self.adcOverloads = adcOverloads
            self.systemCpuPercent = systemCpuPercent
        }

        /// Readings as a connected ANAN on a Linux Core would send them.
        public static let bench = RadioReadings(paVolts: 13.8, sampleRateHz: 192_000,
                                                adcOverloads: [ADCReading(adc: 0, overloaded: false)],
                                                systemCpuPercent: 31)
    }

    final class StationRadiosState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = SceneRadios(radios: [])
        private var generation: Int64 = 1
        private var sequence = 0

        func set(_ scene: SceneRadios) {
            lock.withLock { self.scene = scene }
        }

        func read<Result>(_ body: (inout SceneRadios, Int64) -> Result) -> Result {
            lock.withLock { body(&scene, generation) }
        }

        func nextSequence() -> Int {
            lock.withLock {
                sequence += 1
                return sequence
            }
        }
    }

    public static let stationRadiosStream = "stationRadios"
    public static let cannotSeeRadioReason = "The Core cannot see that radio. Scan again, then choose it."
    public static let forgetInUseReason = "The Core is using this radio. Choose another radio first."
    public static let radioOnAirReason = "The radio is on the air. Try again when it stops."
    public static let unreadableRequestReason = "The Core could not read this request."
    public static let confirmWaitingReason = "Waiting for you to confirm."

    static func stationRadioCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        var versions: [(String, Int)] = []
        if additions.contains(.stationRadios) {
            versions.append(("recordStreamVersion", 1))
            versions.append(("stationRadiosVersion", 1))
        }
        if additions.contains(.radioTelemetry) {
            versions.append(("stationTelemetryVersion", 6))
        }
        return versions
    }

    /// Sets the radios the fake Core can see and sends them, as a reset,
    /// to an app that asked for them.
    public func setStationRadios(_ scene: SceneRadios = .bench) async {
        stationRadiosState.set(scene)
        let models = additions.contains(.radioModels)
        let reset = stationRadiosState.read { scene, generation in
            Self.stationRadiosReset(scene, generation: generation, models: models)
        }
        await deliver(reset)
    }

    /// Why the Core has no radio, in its words (`radio`'s
    /// `stationRadioWaiting`); empty once it has one.
    public func setRadioWaiting(_ words: String) async {
        await deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "stationRadioWaiting", value: .utf8(words)),
        ])))
    }

    /// One telemetry sample with `readings`, as a Core at telemetry version 6 sends it.
    public func deliverRadioTelemetry(_ readings: RadioReadings = .bench) async {
        let sequence = stationRadiosState.nextSequence()
        var radio: [String: LinkJSON] = ["connected": .bool(true)]
        radio["paVolts"] = readings.paVolts.map(LinkJSON.number)
        radio["sampleRateHz"] = readings.sampleRateHz.map { .number(Double($0)) }
        if let overloads = readings.adcOverloads {
            radio["adcOverloads"] = .array(overloads.map { reading in
                // An ADC in overload now has had at least one event (link section 10).
                var entry: [String: LinkJSON] = ["adc": .number(Double(reading.adc)),
                                                 "eventsSinceConnection": .number(reading.overloaded == true ? 1 : 0)]
                entry["overloaded"] = reading.overloaded.map(LinkJSON.bool)
                entry["statusAgeMs"] = reading.overloaded == nil ? nil : .number(20)
                return .object(entry)
            })
        }
        var payload: [String: LinkJSON] = [
            "sequence": .number(Double(sequence)), "sampledElapsedMs": .number(Double(sequence) * 1_000),
            "radio": .object(radio),
        ]
        if let cpu = readings.systemCpuPercent {
            payload["host"] = .object(["systemCpuPercent": .number(cpu)])
        }
        await deliver(.stationMetrics(.init(payload: payload)))
    }

    func stationRadioReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: values))
        }
        let radioVerbs = ["station.selectRadio", "station.rescanRadios", "station.forgetRadio"]
        let ownStream = ["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
            && text("stream") == Self.stationRadiosStream
        guard additions.contains(.stationRadios), radioVerbs.contains(invoke.verb) || ownStream else {
            return nil
        }
        let models = additions.contains(.radioModels)
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return stationRadiosState.read { scene, generation -> [LinkMessage] in
            switch invoke.verb {
            case "records.subscribe":
                return [result(true), Self.stationRadiosReset(scene, generation: generation, models: models)]
            case "records.unsubscribe", "station.rescanRadios":
                return [result(true)]
            case "station.selectRadio":
                guard let mac = text("mac"), invoke.args.count == 1 else {
                    return [result(false, Self.unreadableRequestReason)]
                }
                guard let chosen = scene.radios.firstIndex(where: { $0.mac == mac }) else {
                    return [result(false, Self.cannotSeeRadioReason)]
                }
                if scene.radios[chosen].inUse {
                    // Choosing the Core's radio again is taken and changes nothing.
                    return [result(true)]
                }
                if scene.asksFirst {
                    return [result(false, Self.confirmWaitingReason,
                                   values: [.init(name: "phase", value: .utf8("needsConfirmation"))])]
                }
                var changed: [SceneRadio] = []
                for index in scene.radios.indices where scene.radios[index].inUse != (index == chosen) {
                    scene.radios[index].inUse = index == chosen
                    changed.append(scene.radios[index])
                }
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: Self.stationRadiosStream, generation: generation, reset: false,
                    upserts: changed.map { Self.stationRadioRecord($0, models: models) }, removes: []))]
            case "station.forgetRadio":
                guard let mac = text("mac"), invoke.args.count == 1 else {
                    return [result(false, Self.unreadableRequestReason)]
                }
                guard let index = scene.radios.firstIndex(where: { $0.mac == mac }) else {
                    return [result(false, Self.cannotSeeRadioReason)]
                }
                guard !scene.radios[index].inUse else {
                    return [result(false, Self.forgetInUseReason)]
                }
                scene.radios.remove(at: index)
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: Self.stationRadiosStream, generation: generation, reset: false, upserts: [],
                    removes: [mac]))]
            default:
                return [result(true)]
            }
        }
    }

    private static func stationRadiosReset(_ scene: SceneRadios, generation: Int64, models: Bool) -> LinkMessage {
        .recordBatch(LinkMessage.RecordBatch(stream: stationRadiosStream, generation: generation, reset: true,
                                             upserts: scene.radios.map { stationRadioRecord($0, models: models) },
                                             removes: []))
    }

    /// One radio's record; with `models` (``Additions/radioModels`` at
    /// `radioModelsVersion` 1) its `modelLabel` and `models` too.
    static func stationRadioRecord(_ radio: SceneRadio, models: Bool) -> LinkMessage.RecordBatch.Record {
        var fields: [String: LinkJSON] = [
            "id": .string(radio.mac), "mac": .string(radio.mac), "name": .string(radio.name),
            "model": .number(Double(radio.model)), "address": .string(radio.address),
            "protocol": .number(Double(radio.protocolVersion)), "inUse": .bool(radio.inUse),
        ]
        if models {
            fields["modelLabel"] = .string(radio.modelLabel)
            fields["models"] = .array(radio.models.map { choice in
                .object(["model": .number(Double(choice.model)), "label": .string(choice.label)])
            })
        }
        return LinkMessage.RecordBatch.Record(id: radio.mac, fields: fields)
    }
}

extension FakeStation.Additions {
    /// The Core chooses its own radio: `stationRadiosVersion` 1 with record
    /// streams, the `stationRadios` stream and its verbs. Not in ``all``.
    public static let stationRadios = FakeStation.Additions(rawValue: 1 << 30)
    /// The Core's telemetry at version 6. Not in ``all``.
    public static let radioTelemetry = FakeStation.Additions(rawValue: 1 << 29)
}
