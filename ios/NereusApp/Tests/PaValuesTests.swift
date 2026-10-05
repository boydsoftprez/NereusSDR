// NereusSDR for iOS: PA Values' local measurement history, reset ownership and native rows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import UIKit
import Testing

@Suite("PA Values on this viewer", .serialized)
@MainActor
struct PaValuesTests {
    final class Sent: @unchecked Sendable {
        private let lock = NSLock()
        private var messages: [LinkMessage] = []
        func record(_ message: LinkMessage) { lock.withLock { messages.append(message) } }
        var all: [LinkMessage] { lock.withLock { messages } }
    }

    /// Real mirror/description/dispatcher/local binder, with a captured transport
    /// only to detect an accidental station write. No session or radio runs here.
    @MainActor final class Rig {
        let suite = "PaValuesTests-\(UUID().uuidString)"
        let defaults: UserDefaults
        let clock = TestLinkClock()
        let sent = Sent()
        let app: AppModel
        let mirror: MirrorStore
        let feed: SetupDescriptionFeed
        let model: PaValuesModel
        let phone: PhoneSetupKeys
        let dispatcher: SetupControlDispatcher
        var caps: [LinkMessage.PropertyEntry]

        init() throws {
            defaults = try #require(UserDefaults(suiteName: suite))
            app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           meterSettings: SMeterSettingsStore(defaults: defaults), mirrorClock: clock)
            let sent = sent
            let clock = clock
            mirror = MirrorStore(send: { sent.record($0) }, clock: clock)
            feed = SetupDescriptionFeed(store: mirror)
            model = PaValuesModel(store: mirror, feed: feed, defaults: defaults,
                                  now: { clock.nowMilliseconds })
            phone = PhoneSetupKeys(main: app.main, defaults: defaults, paValues: model)
            let sender: MirrorStore.BoundSender = { message, permit in
                guard permit.handoff({ sent.record(message); return true }) else { throw LinkSendError.notConnected }
            }
            let settings = SettingsProxyClient(send: { sent.record($0) }, captureSender: { sender }, clock: clock)
            let commands = CommandClient(clock: clock, send: { sent.record($0) }, captureSender: { sender })
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                                                captureSender: { sender }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: phone)
            model.configure(dispatcher)
            caps = [.init(name: "setupDescriptionVersion", value: .i64(20)),
                    .init(name: "txReadingsVersion", value: .i64(2)),
                    .init(name: "stationTelemetryVersion", value: .i64(4)),
                    .init(name: "macAddress", value: .utf8("00:11:22:33:44:55")),
                    .init(name: "radioConnected", value: .bool(true))]
        }

        func deliver(_ message: LinkMessage) {
            mirror.apply(message)
            model.handle(.message(message))
        }

        static let powerFields: [LinkMessage.SchemaField] = [
            .init(ordinal: 0, name: "forwardPowerWatts", kind: .f64),
            .init(ordinal: 1, name: "reflectedPowerWatts", kind: .f64),
            .init(ordinal: 2, name: "swr", kind: .f64)
        ]

        func connect(transmitFields: [LinkMessage.SchemaField]? = Rig.powerFields) async throws {
            mirror.handle(.stateChanged(.receivingSnapshot))
            deliver(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
            deliver(.authResult(.init(accepted: true, reason: "", retryable: false)))
            deliver(.capabilities(.init(properties: caps)))
            deliver(.schema(.init(className: "SetupDescription", fields: [
                .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "pa", kind: .utf8)])))
            deliver(.objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                .init(name: "revision", value: .i64(1)),
                .init(name: "pa", value: .utf8(try PaProfilesPanelTests.description()))])))
            if let transmitFields {
                deliver(.schema(.init(className: "TransmitState", fields: transmitFields)))
            }
            deliver(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                .init(name: "forwardPowerWatts", value: .f64(10)),
                .init(name: "reflectedPowerWatts", value: .f64(1)), .init(name: "swr", value: .f64(1.1))])))
            deliver(.snapshotComplete)
            mirror.handle(.stateChanged(.ready))
            await MainQueue.drained()
            try #require(await ShotWait.until { self.feed.description(for: "pa") != nil })
            telemetry(1, amps: 2, temperature: 20, volts: 12)
        }

        func control(_ id: String) throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "pa"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
        }

        func power(_ fwd: Double, _ rev: Double, _ swr: Double) {
            deliver(.delta(.init(key: "txState", properties: [
                .init(name: "forwardPowerWatts", value: .f64(fwd)),
                .init(name: "reflectedPowerWatts", value: .f64(rev)), .init(name: "swr", value: .f64(swr))])))
        }

        func telemetry(_ sequence: Int, amps: Double?, temperature: Double?, volts: Double?) {
            var radio: [String: LinkJSON] = ["connected": .bool(true)]
            radio["paCurrentAmps"] = amps.map(LinkJSON.number)
            radio["paTemperatureCelsius"] = temperature.map(LinkJSON.number)
            radio["supplyVolts"] = volts.map(LinkJSON.number)
            deliver(.stationMetrics(.init(payload: ["sequence": .number(Double(sequence)),
                "sampledElapsedMs": .number(Double(sequence) * 1000), "radio": .object(radio)])))
        }

        func shown(_ id: String) throws -> String {
            let control = try control(id)
            let base: String
            if control.specialized == .paTelemetry {
                base = PaTelemetryPanel.text(dispatcher.telemetryReading(control, in: "pa", nowMilliseconds: clock.nowMilliseconds), control: control)
            } else {
                base = DescribedControl.reading(dispatcher.state(of: control, in: "pa").value, control: control)
            }
            return model.text(base: base, control: control, category: "pa")
        }

        func close() { defaults.removePersistentDomain(forName: suite) }
    }

    // Breaks if initial render, equal telemetry, unit changes or unrelated deltas seed history.
    @Test func onlyChangedReceivedMeasurementsSeedExactlySixTrackers() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        let presentation = rig.model.beginPresentation()
        for _ in 0..<3 { #expect(try rig.shown("pa.values.forwardCalibrated") == "10.00 W") }
        rig.power(10, 1, 1.1)
        rig.telemetry(2, amps: 2, temperature: 20, volts: 12)
        #expect(rig.model.extrema.isEmpty)
        rig.power(12, 2, 1.2)
        rig.telemetry(3, amps: 3, temperature: 30, volts: 13)
        #expect(rig.model.extrema.count == 6)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "12.00 W")
        rig.power(8, 0.5, 1.05)
        rig.telemetry(4, amps: 1, temperature: 10, volts: 11)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "8.00 W  (P 12.00 / M 8.00)")
        #expect(try rig.shown("pa.values.reflectedPower") == "0.50 W  (P 2.00 / M 0.50)")
        #expect(try rig.shown("pa.values.swr") == "1.05  (P 1.20 / M 1.05)")
        #expect(try rig.shown("pa.values.paCurrent") == "1.00 A  (P 3.00 / M 1.00)")
        #expect(try rig.shown("pa.values.paTemperature") == "10.0 °C  (P 30.0 / M 10.0)")
        #expect(try rig.shown("pa.values.dcVoltage") == "11.0 V  (P 13.0 / M 11.0)")
        let raw = try rig.control("pa.values.forwardRawPower")
        #expect(rig.model.text(base: "7.00 W", control: raw, category: "pa") == "7.00 W")
        let saved = rig.model.extrema
        #expect(rig.dispatcher.setTemperatureInFahrenheit(true, for: try rig.control("pa.values.paTemperature")))
        #expect(try rig.shown("pa.values.paTemperature") == "50.0 °F  (P 86.0 / M 50.0)")
        #expect(rig.model.extrema == saved)
        rig.model.endPresentation(presentation)
        rig.power(99, 9, 9)
        #expect(rig.model.extrema == saved)
    }

    // Breaks if reset uses calibration/profile transport, misses an alias, or needs an open page.
    @Test func bothResetAliasesRestartValidCurrentValuesLocallyBeforeAndAfterOpening() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        let reset = try rig.control("pa.wattMeter.resetPaValues")
        #expect(rig.dispatcher.state(of: reset, in: "pa").editable)
        #expect(await rig.dispatcher.edit(reset, in: "pa", to: nil) == .applied)
        #expect(rig.model.extrema.count == 6)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "10.00 W")
        let page = rig.model.beginPresentation()
        rig.power(30, 4, 2)
        rig.telemetry(2, amps: 5, temperature: 50, volts: 15)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "30.00 W  (P 30.00 / M 10.00)")
        let otherReset = try rig.control("pa.values.resetPeakMin")
        #expect(await rig.dispatcher.edit(otherReset, in: "pa", to: nil) == .applied)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "30.00 W")
        #expect(try rig.shown("pa.values.paTemperature") == "50.0 °C")
        #expect(rig.sent.all.isEmpty)
        rig.model.endPresentation(page)
    }

    // Breaks if nil/stale/NaN values become fallback zeros or render callbacks append samples.
    @Test func missingStaleNonfiniteAndEqualFormattedValuesDoNotInventReadings() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(10.001, 1, 1.1)
        rig.power(10.002, 1, 1.1)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "10.00 W")
        let saved = rig.model.extrema
        rig.power(.nan, .infinity, -.infinity)
        #expect(rig.model.extrema == saved)
        rig.telemetry(2, amps: nil, temperature: nil, volts: nil)
        #expect(try rig.shown("pa.values.paCurrent") == "Unavailable")
        rig.telemetry(3, amps: 4, temperature: 40, volts: 14)
        await rig.clock.advance(by: 3001)
        #expect(try rig.shown("pa.values.paCurrent") == "Unavailable")
        let reset = try rig.control("pa.values.resetPeakMin")
        #expect(await rig.dispatcher.edit(reset, in: "pa", to: nil) == .applied)
        #expect(rig.model.extrema.isEmpty)
        #expect(try rig.shown("pa.values.paCurrent") == "Unavailable")
    }

    // Breaks if radio A->B->A or descriptor A->B->A revives an already admitted gesture.
    @Test func obsoleteLocalActionsCannotMutateAReplacementOwner() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        let reset = try rig.control("pa.values.resetPeakMin")
        let action = try rig.model.admit(reset, category: "pa").get()
        let original = rig.caps
        rig.caps.removeAll { $0.name == "macAddress" }
        rig.caps.append(.init(name: "macAddress", value: .utf8("AA:BB:CC:DD:EE:FF")))
        rig.deliver(.capabilities(.init(properties: rig.caps)))
        rig.deliver(.capabilities(.init(properties: original)))
        #expect(await rig.model.perform(action, value: nil) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.model.extrema.isEmpty)
        let next = try rig.model.admit(reset, category: "pa").get()
        rig.deliver(.objectDestroy(.init(key: "txState", className: "TransmitState")))
        rig.deliver(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
            .init(name: "forwardPowerWatts", value: .f64(50)),
            .init(name: "reflectedPowerWatts", value: .f64(5)), .init(name: "swr", value: .f64(2))])))
        #expect(await rig.model.perform(next, value: nil) == .notSent(SetupControlDispatcher.changedFirstReason))
        let descriptorAction = try rig.model.admit(reset, category: "pa").get()
        let body = try PaProfilesPanelTests.description()
        let replacement = body.replacingOccurrences(of: "pa.values.forwardCalibrated",
                                                    with: "pa.values.forwardCalibrated.replacement")
        rig.deliver(.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(replacement))])))
        rig.deliver(.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(body))])))
        await MainQueue.drained()
        #expect(await rig.model.perform(descriptorAction, value: nil) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.sent.all.isEmpty)
    }

    // Breaks if visibility is shared/Core-owned, hides the restore route, or changes History support.
    @Test func visibilityPersistsOnlyLocallyAndLeavesWattMeterReachable() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        let show = try rig.control("pa.wattMeter.showPaValues")
        #expect(rig.dispatcher.state(of: show, in: "pa").value == .bool(true))
        #expect(await rig.dispatcher.edit(show, in: "pa", to: .bool(false)) == .applied)
        let description = try #require(rig.feed.description(for: "pa"))
        let tree = SetupTree.categories(described: ["pa": description], order: ["pa"], unreadable: [:],
                                        showPaValues: rig.model.showPage)
        #expect(tree.first { $0.id == "PA" }?.pages.map(\.title) == ["PA Gain", "Watt Meter"])
        let restored = PaValuesModel(store: rig.mirror, feed: rig.feed, defaults: rig.defaults,
                                     now: { rig.clock.nowMilliseconds })
        #expect(!restored.showPage)
        #expect(await rig.dispatcher.edit(show, in: "pa", to: .bool(true)) == .applied)
        #expect(SetupTree.categories(described: ["pa": description], order: ["pa"], unreadable: [:],
                                    showPaValues: rig.model.showPage)
                    .first { $0.id == "PA" }?.pages.map(\.title) == ["PA Gain", "Watt Meter", "PA Values"])
        #expect(rig.phone.unavailableReason(forPhoneKey: "MultimeterSignalHistoryDurationMs")
                == PhoneSetupKeys.noHistoryGraphReason)
        #expect(rig.phone.value(forPhoneKey: "MultimeterSignalHistoryDurationMs") == nil)
        #expect(rig.sent.all.isEmpty)
    }

    @Test func firstValidChangeUnknownReadingsAndPresentationReplacement() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        rig.telemetry(2, amps: nil, temperature: nil, volts: nil)
        let abandoned = rig.model.beginPresentation()
        let current = rig.model.beginPresentation()
        rig.model.endPresentation(abandoned)
        rig.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "forwardAdcRaw", value: .i64(900)),
            .init(name: "forwardRawPowerWatts", value: .f64(300))])))
        #expect(rig.model.extrema.isEmpty)
        rig.telemetry(3, amps: 0, temperature: 0, volts: 0)
        #expect(rig.model.extrema.count == 3)
        #expect(try rig.shown("pa.values.paCurrent") == "0.00 A")
        let saved = rig.model.extrema
        // Decoder rejects this duplicate sequence; it is not a new receipt.
        rig.telemetry(3, amps: 100, temperature: 100, volts: 100)
        #expect(rig.model.extrema == saved)
        rig.model.endPresentation(current)
        rig.telemetry(4, amps: 20, temperature: 30, volts: 40)
        #expect(rig.model.extrema == saved)
        #expect(rig.sent.all.isEmpty)
    }

    @Test func unrelatedPaGainMetadataKeepsReadingHistory() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        rig.power(8, 0.5, 1.05)
        let saved = rig.model.extrema
        let body = try PaProfilesPanelTests.description(keyed: true, holder: 5)
        rig.deliver(.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(body))])))
        #expect(rig.model.extrema == saved)
        try #require(await ShotWait.until { rig.feed.description(for: "pa") != nil })
        #expect(try rig.shown("pa.values.forwardCalibrated") == "8.00 W  (P 12.00 / M 8.00)")
    }

    @Test func radioReplacementResetCannotSeedRetainedMeasurements() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        let original = rig.caps
        rig.caps.removeAll { $0.name == "macAddress" }
        rig.caps.append(.init(name: "macAddress", value: .utf8("AA:BB:CC:DD:EE:FF")))
        rig.deliver(.capabilities(.init(properties: rig.caps)))
        rig.deliver(.capabilities(.init(properties: original)))
        #expect(rig.model.reset())
        #expect(rig.model.extrema.isEmpty)
        // A rejected old receipt cannot reacquire the replacement owner.
        rig.telemetry(1, amps: 9, temperature: 90, volts: 19)
        #expect(rig.model.reset())
        #expect(rig.model.extrema.isEmpty)
        rig.power(4, 0.2, 1.01)
        rig.telemetry(2, amps: 0.5, temperature: 15, volts: 11)
        #expect(rig.model.extrema.count == 6)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "4.00 W")
        #expect(rig.sent.all.isEmpty)
    }

    @Test func sessionAndSchemaReplacementRetireLocalActionsAndExtrema() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        let reset = try rig.control("pa.values.resetPeakMin")
        let schemaAction = try rig.model.admit(reset, category: "pa").get()
        rig.deliver(.schema(.init(className: "TransmitState", fields: [
            .init(ordinal: 0, name: "forwardPowerWatts", kind: .f64),
            .init(ordinal: 1, name: "reflectedPowerWatts", kind: .f64),
            .init(ordinal: 2, name: "swr", kind: .f64)])))
        #expect(await rig.model.perform(schemaAction, value: nil) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.model.extrema.isEmpty)
        let sessionAction = try rig.model.admit(reset, category: "pa").get()
        rig.mirror.handle(.stateChanged(.stopped))
        rig.model.handle(.stateChanged(.stopped))
        #expect(await rig.model.perform(sessionAction, value: nil) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.model.extrema.isEmpty)
        #expect(rig.sent.all.isEmpty)
    }

    private static let powerIds = ["pa.values.forwardCalibrated", "pa.values.reflectedPower", "pa.values.swr"]
    private static let telemetryIds = ["pa.values.paCurrent", "pa.values.paTemperature", "pa.values.dcVoltage"]

    @Test("supported actual zero power remains a received value, never missing")
    func supportedZeroPowerRemainsValid() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        rig.caps.removeAll { $0.name == "txReadingsVersion" }
        rig.caps.append(.init(name: "txReadingsVersion", value: .i64(1)))
        rig.deliver(.capabilities(.init(properties: rig.caps)))
        #expect(rig.mirror.capabilityVersion("txReadingsVersion") == 1)
        _ = rig.model.beginPresentation()
        rig.power(0, 0, 1)
        #expect(rig.model.extrema["pa.values.forwardCalibrated"]?.peak == 0)
        #expect(rig.model.extrema["pa.values.reflectedPower"]?.minimum == 0)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "0.00 W")
        rig.power(1.5, 0.2, 1.1)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "1.50 W  (P 1.50 / M 0.00)")
        rig.power(0, 0, 1)
        #expect(await rig.dispatcher.edit(try rig.control("pa.values.resetPeakMin"), in: "pa", to: nil) == .applied)
        #expect(rig.model.extrema["pa.values.forwardCalibrated"]?.peak == 0)
        #expect(rig.model.extrema["pa.values.forwardCalibrated"]?.minimum == 0)
        #expect(try rig.shown("pa.values.forwardCalibrated") == "0.00 W")
        #expect(rig.sent.all.isEmpty)
    }

    @Test("zero/missing power capability rejects actual received deltas and reset while telemetry stays valid",
          arguments: ["zero", "missing"])
    func unsupportedPowerCapabilityCannotReseedTrackers(_ capability: String) async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        rig.telemetry(2, amps: 3, temperature: 30, volts: 13)
        #expect(rig.model.extrema.count == 6)
        rig.caps.removeAll { $0.name == "txReadingsVersion" }
        if capability == "zero" { rig.caps.append(.init(name: "txReadingsVersion", value: .i64(0))) }
        rig.deliver(.capabilities(.init(properties: rig.caps)))
        #expect(rig.mirror.capabilityVersion("txReadingsVersion") == 0)
        try #require(rig.mirror.currentSessionSchemaIdentity(ofClass: "TransmitState") != nil)
        try #require(rig.feed.description(for: "pa") != nil)
        rig.power(14, 3, 1.4)
        rig.telemetry(3, amps: 4, temperature: 40, volts: 14)
        rig.power(8, 0.5, 1.05)
        rig.telemetry(4, amps: 1, temperature: 10, volts: 11)
        for id in Self.powerIds {
            #expect(rig.model.extrema[id] == nil, "Unsupported received power must not seed a tracker")
            #expect(!(try rig.shown(id)).contains("  (P "))
        }
        for id in Self.telemetryIds { #expect(rig.model.extrema[id] != nil) }
        #expect(try rig.shown("pa.values.paCurrent") == "1.00 A  (P 4.00 / M 1.00)")
        let reset = try rig.control("pa.values.resetPeakMin")
        #expect(await rig.dispatcher.edit(reset, in: "pa", to: nil) == .applied)
        for id in Self.powerIds { #expect(rig.model.extrema[id] == nil) }
        for id in Self.telemetryIds { #expect(rig.model.extrema[id] != nil) }
        #expect(rig.model.extrema.count == 3)
        #expect(try rig.shown("pa.values.paCurrent") == "1.00 A")
        #expect(rig.sent.all.isEmpty)
    }

    @Test("txState values without a current schema cannot seed power, including reset")
    func missingCurrentTransmitSchemaCannotSeedPower() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        try #require(rig.mirror.propertyNames(ofClass: "TransmitState") != nil)
        // An accepted new snapshot retires current schema identities, while
        // MirrorStore deliberately retains the older class-name cache.
        try await rig.connect(transmitFields: nil)
        try #require(rig.mirror.object("txState")?.className == "TransmitState")
        #expect(rig.mirror.propertyNames(ofClass: "TransmitState") == Rig.powerFields.map(\.name))
        #expect(rig.mirror.capabilityVersion("txReadingsVersion") == 2)
        #expect(rig.mirror.currentSessionSchemaIdentity(ofClass: "TransmitState") == nil)
        #expect(rig.mirror.currentSessionPropertyNames(ofClass: "TransmitState") == nil)
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        rig.telemetry(2, amps: 3, temperature: 30, volts: 13)
        rig.power(8, 0.5, 1.05)
        rig.telemetry(3, amps: 1, temperature: 10, volts: 11)
        for id in Self.powerIds {
            #expect(rig.model.extrema[id] == nil)
            #expect(!(try rig.shown(id)).contains("  (P "))
        }
        #expect(rig.model.extrema.count == 3)
        #expect(try rig.shown("pa.values.paCurrent") == "1.00 A  (P 3.00 / M 1.00)")
        let reset = try rig.control("pa.wattMeter.resetPaValues")
        #expect(await rig.dispatcher.edit(reset, in: "pa", to: nil) == .applied)
        for id in Self.powerIds { #expect(rig.model.extrema[id] == nil) }
        for id in Self.telemetryIds { #expect(rig.model.extrema[id] != nil) }
        #expect(rig.model.extrema.count == 3)
        #expect(rig.sent.all.isEmpty)
    }

    @Test("a nonnil unrelated TransmitState schema is insufficient for power readings/reset")
    func missingAllExpectedTransmitFieldsCannotSeedPower() async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect(transmitFields: [.init(ordinal: 0, name: "keyed", kind: .bool)])
        try #require(rig.mirror.currentSessionSchemaIdentity(ofClass: "TransmitState") != nil)
        #expect(rig.mirror.currentSessionPropertyNames(ofClass: "TransmitState") == ["keyed"])
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        rig.telemetry(2, amps: 3, temperature: 30, volts: 13)
        rig.power(8, 0.5, 1.05)
        rig.telemetry(3, amps: 1, temperature: 10, volts: 11)
        for id in Self.powerIds {
            #expect(rig.model.extrema[id] == nil)
            #expect(!(try rig.shown(id)).contains("  (P "))
        }
        #expect(rig.model.extrema.count == 3)
        #expect(try rig.shown("pa.values.dcVoltage") == "11.0 V  (P 13.0 / M 11.0)")
        #expect(await rig.dispatcher.edit(try rig.control("pa.values.resetPeakMin"), in: "pa", to: nil) == .applied)
        for id in Self.powerIds { #expect(rig.model.extrema[id] == nil) }
        for id in Self.telemetryIds { #expect(rig.model.extrema[id] != nil) }
        #expect(rig.model.extrema.count == 3)
        #expect(rig.sent.all.isEmpty)
    }

    @Test("each power input requires its own expected field; other power and telemetry inputs remain valid",
          arguments: ["forwardPowerWatts", "reflectedPowerWatts", "swr"])
    func missingOneExpectedTransmitFieldBlocksOnlyThatTracker(_ missing: String) async throws {
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect(transmitFields: Rig.powerFields.filter { $0.name != missing })
        try #require(rig.mirror.currentSessionSchemaIdentity(ofClass: "TransmitState") != nil)
        let fields = try #require(rig.mirror.currentSessionPropertyNames(ofClass: "TransmitState"))
        #expect(!fields.contains(missing))
        _ = rig.model.beginPresentation()
        rig.power(12, 2, 1.2)
        rig.telemetry(2, amps: 3, temperature: 30, volts: 13)
        let id = try #require(["forwardPowerWatts": "pa.values.forwardCalibrated",
                              "reflectedPowerWatts": "pa.values.reflectedPower", "swr": "pa.values.swr"][missing])
        #expect(rig.model.extrema[id] == nil)
        for other in Self.powerIds where other != id { #expect(rig.model.extrema[other] != nil) }
        for telemetry in Self.telemetryIds { #expect(rig.model.extrema[telemetry] != nil) }
        #expect(rig.model.extrema.count == 5)
        #expect(await rig.dispatcher.edit(try rig.control("pa.values.resetPeakMin"), in: "pa", to: nil) == .applied)
        #expect(rig.model.extrema[id] == nil)
        #expect(rig.model.extrema.count == 5)
        #expect(rig.sent.all.isEmpty)
    }

    /// Existing native DescribedPage/List/readout route with the real Core
    /// resource. These fixture readings are illustrative, not radio evidence.
    /// Optional PNGs use the existing NEREUS_MAIN_SHOTS route.
    @Test func nativeSixReadingsReuseExistingRowsAndWrapAcrossViewports() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = try Rig()
        defer { rig.close() }
        try await rig.connect()
        let pages = SetupDescribedPages(feed: rig.feed, store: rig.mirror,
                                        hello: Just(nil).eraseToAnyPublisher())
        pages.refresh()
        var panels = SetupSpecializedPanels { kind, control, category in
            guard kind == .paTelemetry else { return nil }
            return AnyView(PaTelemetryPanel(control: control, category: category, dispatcher: rig.dispatcher,
                                           now: { rig.clock.nowMilliseconds }, paValues: rig.model))
        }
        panels.paValues = rig.model
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let configurations: [(String, CGFloat, CGFloat, DynamicTypeSize)] = [
            ("phone-portrait", 402, 874, .large),
            ("phone-landscape", 874, 402, .large),
            ("tablet-portrait", 1024, 1366, .large),
            ("tablet-landscape", 1366, 1024, .large),
            ("phone-large-text", 402, 874, .accessibility2)
        ]
        let expected: [(String, String)] = [
            ("pa.values.forwardCalibrated", "8.00 W  (P 12.00 / M 8.00)"),
            ("pa.values.reflectedPower", "0.50 W  (P 2.00 / M 0.50)"),
            ("pa.values.swr", "1.05  (P 1.20 / M 1.05)"),
            ("pa.values.paCurrent", "1.00 A  (P 3.00 / M 1.00)"),
            ("pa.values.paTemperature", "10.0 °C  (P 30.0 / M 10.0)"),
            ("pa.values.dcVoltage", "11.0 V  (P 13.0 / M 11.0)")
        ]
        var sequence = 1
        for (name, width, height, type) in configurations {
            for scheme in [ColorScheme.light, .dark] {
                rig.power(10, 1, 1.1)
                sequence += 1
                rig.telemetry(sequence, amps: 2, temperature: 20, volts: 12)
                let window = UIWindow(windowScene: scene)
                window.frame = CGRect(x: 0, y: 0, width: width, height: height)
                window.windowLevel = .alert + 1
                let root = NavigationStack {
                    DescribedPage(pages: pages, dispatcher: rig.dispatcher, category: "pa",
                                  pageId: "pa.values", specialized: panels)
                }.environment(\.dynamicTypeSize, type).preferredColorScheme(scheme)
                let host = UIHostingController(rootView: root)
                window.rootViewController = host
                host.view.frame = window.bounds
                window.makeKeyAndVisible()
                defer { window.isHidden = true; window.rootViewController = nil }
                await ShotWait.laidOut(window)
                // Only production DescribedPage.onAppear starts collection.
                // Breaking that hook must fail the received-sample assertions.
                rig.power(12, 2, 1.2)
                sequence += 1
                rig.telemetry(sequence, amps: 3, temperature: 30, volts: 13)
                rig.power(8, 0.5, 1.05)
                sequence += 1
                rig.telemetry(sequence, amps: 1, temperature: 10, volts: 11)
                for (id, text) in expected {
                    #expect(try rig.shown(id) == text)
                    try #require(await reveal(id, in: window, root: host.view))
                    let row = try #require(SetupTypedEntryTests.element(id, in: window))
                    let nativeText = [row.accessibilityLabel, row.accessibilityValue].compactMap { $0 }.joined(separator: " ")
                    #expect(nativeText.contains(text), "The actual native row must display its annotated reading")
                    #expect(!row.accessibilityFrame.isEmpty)
                    #expect(row.accessibilityFrame.width <= window.bounds.width + 1)
                    try save("pa-values-\(name)-\(scheme == .dark ? "dark" : "light")-\(id)", window: window)
                }
                try #require(await reveal("pa.values.resetPeakMin", in: window, root: host.view))
                let reset = try #require(SetupTypedEntryTests.element("pa.values.resetPeakMin", in: window))
                #expect(reset.accessibilityActivate())
                await ShotWait.laidOut(window)
                try #require(await ShotWait.until { rig.model.extrema.count == 6
                    && rig.model.extrema.values.allSatisfy { $0.peak == $0.minimum } })
                #expect(try rig.shown("pa.values.forwardCalibrated") == "8.00 W")
                try #require(await reveal("pa.values.forwardCalibrated", in: window, root: host.view))
                let currentRow = try #require(SetupTypedEntryTests.element("pa.values.forwardCalibrated", in: window))
                let currentText = [currentRow.accessibilityLabel, currentRow.accessibilityValue].compactMap { $0 }.joined(separator: " ")
                #expect(currentText.contains("8.00 W") && !currentText.contains("(P "))
                // Removing the hosted page must run production onDisappear.
                window.isHidden = true
                window.rootViewController = nil
                await ShotWait.laidOut(window)
                let closed = rig.model.extrema
                rig.power(99, 9, 9)
                sequence += 1
                rig.telemetry(sequence, amps: 9, temperature: 90, volts: 19)
                #expect(rig.model.extrema == closed, "A dismissed native page must not collect measurements")
            }
        }
        #expect(rig.sent.all.isEmpty)
    }

    @Test("full Setup route activates both native resets and hides/restores only PA Values")
    func nativeSetupResetAliasesAndVisibilityRestore() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let oldShow = UserDefaults.standard.object(forKey: PaValuesModel.showPageKey)
        UserDefaults.standard.set(true, forKey: PaValuesModel.showPageKey)
        defer {
            if let oldShow { UserDefaults.standard.set(oldShow, forKey: PaValuesModel.showPageKey) }
            else { UserDefaults.standard.removeObject(forKey: PaValuesModel.showPageKey) }
        }
        let clock = TestLinkClock()
        let rig = try await SetupSpecializedPanelsTests.connected(mirrorClock: clock)
        defer { rig.defaults.removePersistentDomain(forName: rig.suite) }
        var caps = rig.app.mirror.capabilities.map { LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue) }
        caps.removeAll { ["setupDescriptionVersion", "txReadingsVersion", "stationTelemetryVersion"].contains($0.name) }
        caps += [.init(name: "setupDescriptionVersion", value: .i64(20)),
                 .init(name: "txReadingsVersion", value: .i64(2)),
                 .init(name: "stationTelemetryVersion", value: .i64(4))]
        await rig.station.deliver(.capabilities(.init(properties: caps)))
        await rig.station.deliverSetup(try SetupDescribedPagesTests.coreCategories(peer: 20))
        try #require(await ShotWait.until { rig.app.setupPages.isCurrent && rig.app.setupPages.categories["pa"]?.version == 20 })
        await rig.station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "forwardPowerWatts", value: .f64(5)),
            .init(name: "reflectedPowerWatts", value: .f64(0.2)), .init(name: "swr", value: .f64(1.01))])))
        await rig.station.deliverPaTelemetry(sequence: 1, paCurrentAmps: 0.5, supplyVolts: 11, paTemperatureCelsius: 15)
        try await LinkBarrier.roundTrip(rig.app.commands)
        let before = rig.station.messages.count
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 1300)
        window.windowLevel = .alert + 1
        rig.router.path = [.category("PA")]
        let host = UIHostingController(rootView: SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil))
        window.rootViewController = host
        host.view.frame = window.bounds
        window.makeKeyAndVisible()
        defer { window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        try #require(SetupTypedEntryTests.element("PA Gain", in: window))
        try #require(SetupTypedEntryTests.element("PA Values", in: window))
        #expect(try #require(SetupTypedEntryTests.element("Watt Meter", in: window)).accessibilityActivate())
        await ShotWait.laidOut(window)
        let wattReset = try #require(SetupTypedEntryTests.element("pa.wattMeter.resetPaValues", in: window))
        #expect(wattReset.accessibilityActivate())
        await ShotWait.laidOut(window)
        try #require(await ShotWait.until { rig.app.paValues.extrema.count == 6
            && rig.app.paValues.extrema.values.allSatisfy { $0.peak == $0.minimum } })
        let hide = try #require(SetupTypedEntryTests.element("pa.wattMeter.showPaValues", in: window))
        #expect(hide.accessibilityActivate())
        await ShotWait.laidOut(window)
        try #require(await ShotWait.until { !rig.app.paValues.showPage })
        rig.router.path = [.category("PA")]
        await ShotWait.laidOut(window)
        // SwiftUI retains removed cells during navigation/list transitions.
        // A retained hidden cell is not a visible or accessible menu choice.
        // GREEN v1's native diagnostic confirmed the obsolete PA Values node
        // has a hidden ListCollectionViewCell ancestor while the live tree
        // contains only PA Gain and Watt Meter.
        #expect(visibleElement("PA Values", in: window) == nil)
        try #require(visibleElement("PA Gain", in: window))
        try #require(visibleElement("Watt Meter", in: window))
        try save("pa-values-full-setup-hidden", window: window)
        #expect(try #require(SetupTypedEntryTests.element("Watt Meter", in: window)).accessibilityActivate())
        await ShotWait.laidOut(window)
        let restore = try #require(SetupTypedEntryTests.element("pa.wattMeter.showPaValues", in: window))
        #expect(restore.accessibilityActivate())
        await ShotWait.laidOut(window)
        try #require(await ShotWait.until { rig.app.paValues.showPage })
        rig.router.path = [.category("PA")]
        await ShotWait.laidOut(window)
        try #require(SetupTypedEntryTests.element("PA Gain", in: window))
        try #require(SetupTypedEntryTests.element("Watt Meter", in: window))
        #expect(try #require(SetupTypedEntryTests.element("PA Values", in: window)).accessibilityActivate())
        await ShotWait.laidOut(window)
        await rig.station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "forwardPowerWatts", value: .f64(12)),
            .init(name: "reflectedPowerWatts", value: .f64(2)), .init(name: "swr", value: .f64(1.2))])))
        await rig.station.deliverPaTelemetry(sequence: 2, paCurrentAmps: 3, supplyVolts: 13, paTemperatureCelsius: 30)
        await ShotWait.laidOut(window)
        try #require(await reveal("pa.values.forwardCalibrated", in: window, root: host.view))
        let reading = try #require(SetupTypedEntryTests.element("pa.values.forwardCalibrated", in: window))
        let text = [reading.accessibilityLabel, reading.accessibilityValue].compactMap { $0 }.joined(separator: " ")
        #expect(text.contains("12.00 W  (P 12.00 / M 5.00)"))
        try #require(await reveal("pa.values.resetPeakMin", in: window, root: host.view))
        #expect(try #require(SetupTypedEntryTests.element("pa.values.resetPeakMin", in: window)).accessibilityActivate())
        await ShotWait.laidOut(window)
        try #require(await ShotWait.until { rig.app.paValues.extrema.count == 6
            && rig.app.paValues.extrema.values.allSatisfy { $0.peak == $0.minimum } })
        try await LinkBarrier.roundTrip(rig.app.commands)
        let after = Array(rig.station.messages.dropFirst(before))
        #expect(!after.contains { message in
            switch message {
            case .propertyWrite, .settingsWrite, .settingsRemove: return true
            case .commandInvoke(let invoke): return invoke.verb != FakeStation.barrierVerb
            default: return false
            }
        }, "Both native reset aliases and hide/restore must send no settings/property/production command")
        try save("pa-values-full-setup-restored-and-reset", window: window)
        await rig.app.disconnect()
    }

    private func reveal(_ id: String, in window: UIWindow, root: UIView) async -> Bool {
        await ShotWait.laidOut(window)
        if let row = SetupTypedEntryTests.element(id, in: window),
           !row.accessibilityFrame.isEmpty,
           UIAccessibility.convertToScreenCoordinates(window.bounds, in: window).intersects(row.accessibilityFrame) {
            return true
        }
        guard let list = SetupDescribedPagesTests.firstScrollView(in: root) else {
            return false
        }
        list.contentOffset.y = -list.adjustedContentInset.top
        var previous: CGFloat?
        for _ in 0..<32 {
            await ShotWait.laidOut(window)
            if let row = SetupTypedEntryTests.element(id, in: window),
               !row.accessibilityFrame.isEmpty,
               UIAccessibility.convertToScreenCoordinates(window.bounds, in: window).intersects(row.accessibilityFrame) {
                return true
            }
            if let previous, list.contentOffset.y <= previous + 1 { return false }
            previous = list.contentOffset.y
            let end = max(-list.adjustedContentInset.top,
                          list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
            let next = min(end, list.contentOffset.y + max(44, list.bounds.height / 2))
            guard next > list.contentOffset.y + 1 else { return false }
            list.contentOffset.y = next
        }
        return false
    }

    /// Search live native UI only, excluding cells retained under hidden views.
    /// Keep the existing bounded public accessibility traversal; no private APIs.
    private func visibleElement(_ id: String, in window: UIWindow) -> NSObject? {
        let rectangle = UIAccessibility.convertToScreenCoordinates(window.bounds, in: window)
        let sceneRectangle = window.windowScene.map { $0.coordinateSpace.convert($0.coordinateSpace.bounds, to: $0.screen.coordinateSpace) } ?? rectangle
        let viewport = rectangle.intersection(sceneRectangle)
        var queue: [NSObject] = [window]
        var seen: Set<ObjectIdentifier> = []
        var count = 0
        while !queue.isEmpty, count < 20_000 {
            let node = queue.removeFirst()
            guard seen.insert(ObjectIdentifier(node)).inserted else { continue }
            count += 1
            if let view = node as? UIView, view.isHidden || view.alpha <= 0 {
                continue
            }
            if SetupTypedEntryTests.identifier(node) == id,
               !node.accessibilityFrame.isEmpty, viewport.intersects(node.accessibilityFrame) {
                return node
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let total = node.accessibilityElementCount()
                if total != NSNotFound, total > 0 {
                    queue += (0..<total).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return nil
    }

    private func save(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        try #require(image.pngData()).write(to: URL(fileURLWithPath: directory).appendingPathComponent(name + ".png"))
    }

}
