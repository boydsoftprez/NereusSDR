// NereusSDR for iOS: PA Values' six received readings and extrema on this viewer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror

/// The original Setup description contract, section PA Values (514–525):
/// collection begins with a changed reading after the page opens. Rendering,
/// clocks and unit choices do not create samples. Both reset buttons share
/// this current viewer/radio/session state; nothing here writes to the Core.
@MainActor
final class PaValuesModel: ObservableObject {
    struct Extrema: Equatable {
        var peak: Double
        var minimum: Double
    }

    struct Action {
        fileprivate let admission: SetupAdmission
        fileprivate let epoch: UInt64
    }

    private struct Owner: Equatable {
        let snapshot: UInt64
        let radio: [String: MirrorValue]
        let setup: ObjectIdentifier?
        let transmit: ObjectIdentifier?
        let setupSchema: UInt64?
        let transmitSchema: UInt64?
        let description: [SetupDescription.Control]?
    }

    static let showPageKey = "display/showPaValuesPage"
    static let resetAction = "resetPaValues"
    @Published private(set) var showPage: Bool
    @Published private(set) var extrema: [String: Extrema] = [:]
    private let store: MirrorStore
    private let feed: SetupDescriptionFeed
    private let defaults: UserDefaults
    private let now: () -> Int64
    private weak var dispatcher: SetupControlDispatcher?
    private var owner: Owner?
    private var describedBytes: MirrorValue?
    private var describedControls: [SetupDescription.Control]?
    private var epoch: UInt64 = 0
    private var presentation: UUID?
    private var baseline: [String: Double] = [:]
    private struct ReceivedOwner: Equatable {
        let snapshot: UInt64
        let radio: [String: MirrorValue]
        let object: ObjectIdentifier?
        let schema: UInt64?
    }
    private struct ReceivedPower {
        let owner: ReceivedOwner
        let value: Double?
    }
    private var receivedPower: [String: ReceivedPower] = [:]
    private var receivedTelemetry: ReceivedOwner?
    private var lastTelemetry: StationTelemetryReceipt?
    private var watches: Set<AnyCancellable> = []
    private var setupWatch: AnyCancellable?
    private weak var watchedSetup: MirrorObject?

    /// Exact six IDs and their raw sources from the Core's PA description.
    private static let sources: [String: String] = [
        "pa.values.forwardCalibrated": "forwardPowerWatts",
        "pa.values.reflectedPower": "reflectedPowerWatts",
        "pa.values.swr": "swr",
        "pa.values.paCurrent": "paCurrentAmps",
        "pa.values.paTemperature": "paTemperatureCelsius",
        "pa.values.dcVoltage": "supplyVolts"
    ]
    private static let radioKeys = ["macAddress", "radioConnected", "setupDescriptionVersion",
                                    "txReadingsVersion", "stationTelemetryVersion"]

    init(store: MirrorStore, feed: SetupDescriptionFeed, defaults: UserDefaults = .standard,
         now: @escaping () -> Int64) {
        self.store = store
        self.feed = feed
        self.defaults = defaults
        self.now = now
        showPage = defaults.object(forKey: Self.showPageKey) as? Bool ?? true
        // These publishers retire authority synchronously, including an A-B-A
        // replacement before a queued feed refresh. They never collect samples.
        store.$isSnapshotComplete.dropFirst().sink { [weak self] complete in
            if !complete {
                self?.invalidateReceipts()
                self?.retire()
            }
        }.store(in: &watches)
        store.$isStale.dropFirst().sink { [weak self] stale in
            if stale {
                self?.invalidateReceipts()
                self?.retire()
            }
        }.store(in: &watches)
        store.$capabilities.dropFirst().sink { [weak self] incoming in
            guard let self else { return }
            if Self.radio(incoming) != Self.radio(self.store.capabilities) {
                self.invalidateReceipts()
                self.retire()
            }
        }.store(in: &watches)
    }

    func configure(_ dispatcher: SetupControlDispatcher) {
        self.dispatcher = dispatcher
        // Use the receipt's arrival clock for all existing generic PA readouts.
        dispatcher.telemetryNow = now
    }

    private static func radio(_ values: [String: MirrorValue]) -> [String: MirrorValue] {
        values.filter { radioKeys.contains($0.key) }
    }

    private func currentOwner() -> Owner? {
        guard store.isSnapshotComplete, !store.isStale else { return nil }
        let setup = store.object("setup")
        let transmit = store.object("txState")
        return Owner(snapshot: store.snapshotIdentity, radio: Self.radio(store.capabilities),
                     setup: setup.map(ObjectIdentifier.init), transmit: transmit.map(ObjectIdentifier.init),
                     setupSchema: store.currentSessionSchemaIdentity(ofClass: "SetupDescription"),
                     transmitSchema: store.currentSessionSchemaIdentity(ofClass: "TransmitState"),
                     description: describedControls)
    }

    private func invalidateReceipts() {
        receivedPower = [:]
        receivedTelemetry = nil
    }

    private func retire() {
        epoch &+= 1
        owner = nil
        extrema = [:]
        baseline = [:]
    }

    private func synchronizeOwner() {
        let raw = store.object("setup")?["pa"]
        if raw != describedBytes {
            describedBytes = raw
            describedControls = Self.relevantDescription(raw)
        }
        let current = currentOwner()
        if current != owner {
            retire()
            owner = current
            // Replacement values establish the new baseline, not history.
            if presentation != nil { baseline = currentValues() }
        }
        let setup = store.object("setup")
        if setup !== watchedSetup {
            watchedSetup = setup
            setupWatch = setup?.$values.dropFirst().sink { [weak self, weak setup] incoming in
                guard let self, let setup else { return }
                if incoming["pa"] != setup.values["pa"],
                   Self.relevantDescription(incoming["pa"]) != self.describedControls {
                    self.retire()
                }
            }
        }
    }

    private static func relevantDescription(_ raw: MirrorValue?) -> [SetupDescription.Control]? {
        guard case .text(let json)? = raw, let description = try? SetupDescription.parse(json: json) else { return nil }
        return description.pages.flatMap(\.sections).flatMap(\.controls)
            .filter { sources[$0.id] != nil || owns($0, category: "pa") }
            .sorted { $0.id < $1.id }
    }

    private var controls: [SetupDescription.Control] {
        guard let description = feed.description(for: "pa"), description.version >= 13 else { return [] }
        return description.pages.first { $0.id == "pa.values" }?.sections.flatMap(\.controls) ?? []
    }

    private func receivedOwner(object: MirrorObject? = nil) -> ReceivedOwner {
        ReceivedOwner(snapshot: store.snapshotIdentity, radio: Self.radio(store.capabilities),
                      object: object.map(ObjectIdentifier.init),
                      schema: object.flatMap { store.currentSessionSchemaIdentity(ofClass: $0.className) })
    }

    private func validValue(_ control: SetupDescription.Control) -> Double? {
        guard store.capabilities["radioConnected"] != .bool(false),
              case .text(let mac)? = store.capabilities["macAddress"], !mac.isEmpty,
              let expected = Self.sources[control.id], let dispatcher,
              control.kind == .readout, control.metadataIssue == nil, controls.contains(control),
              dispatcher.state(of: control, in: "pa").reason == nil else { return nil }
        let value: Double?
        switch control.binding {
        case .property(let reference)? where reference.object == "txState" && reference.name == expected:
            // The canonical power rows require txReadingsVersion 1 and the
            // current TransmitState field (resources/setup/pa.json). Generic
            // readout state alone does not enforce that capability/schema.
            guard store.capabilityVersion("txReadingsVersion") >= 1,
                  store.currentSessionSchemaIdentity(ofClass: "TransmitState") != nil,
                  store.currentSessionPropertyNames(ofClass: "TransmitState")?.contains(expected) == true,
                  let received = receivedPower[expected],
                  received.owner == receivedOwner(object: store.object("txState")) else { return nil }
            value = received.value
        case .telemetry(let reference)? where reference.object == "radio" && reference.name == expected:
            guard receivedTelemetry == receivedOwner() else { return nil }
            value = dispatcher.telemetryReading(control, in: "pa", nowMilliseconds: now()).value
        default: value = nil
        }
        guard let value, value.isFinite else { return nil }
        return value
    }

    private func currentValues() -> [String: Double] {
        Dictionary(uniqueKeysWithValues: controls.compactMap { control in
            validValue(control).map { (control.id, $0) }
        })
    }

    func beginPresentation() -> UUID {
        synchronizeOwner()
        let token = UUID()
        presentation = token
        baseline = currentValues()
        lastTelemetry = store.currentTelemetryReceipt
        return token
    }

    func endPresentation(_ token: UUID) {
        guard presentation == token else { return }
        presentation = nil
        baseline = [:]
    }

    /// Called immediately after MirrorStore handles the event, before AppModel
    /// awaits other consumers. Raw received changes are the sole sample source.
    func handle(_ event: StationSession.Event) {
        if case .message(let message) = event {
            switch message {
            case .objectCreate(let created) where created.key == "setup" || created.key == "txState":
                if created.key == "txState" { receivedPower = [:] }
                retire()
            case .objectDestroy(let destroyed) where destroyed.key == "setup" || destroyed.key == "txState":
                if destroyed.key == "txState" { receivedPower = [:] }
                retire()
            default: break
            }
        }
        // Keep only receipt provenance, even while closed. This is not a
        // history buffer and cannot seed extrema without a visible page.
        var newTelemetry = false
        switch event {
        case .message(.objectCreate(let created)) where created.key == "txState":
            receivedPower = [:]
            for property in created.properties where Self.sources.values.contains(property.name) {
                recordPower(property)
            }
        case .message(.delta(let delta)) where delta.key == "txState":
            for property in delta.properties where Self.sources.values.contains(property.name) {
                recordPower(property)
            }
        case .message(.stationMetrics):
            if let receipt = store.currentTelemetryReceipt, receipt != lastTelemetry {
                receivedTelemetry = receivedOwner()
                lastTelemetry = receipt
                newTelemetry = true
            }
        default: break
        }
        synchronizeOwner()
        guard presentation != nil, showPage, owner != nil else { return }
        let names: Set<String>
        switch event {
        case .message(.delta(let delta)) where delta.key == "txState":
            names = Set(delta.properties.map(\.name)).intersection(["forwardPowerWatts", "reflectedPowerWatts", "swr"])
        case .message(.stationMetrics):
            guard newTelemetry else { return }
            names = ["paCurrentAmps", "paTemperatureCelsius", "supplyVolts"]
        default: return
        }
        var next = extrema
        for control in controls {
            guard let source = Self.sources[control.id], names.contains(source) else { continue }
            let value = validValue(control)
            let previous = baseline[control.id]
            baseline[control.id] = value
            guard let value, value != previous else { continue }
            if var tracked = next[control.id] {
                tracked.peak = max(tracked.peak, value)
                tracked.minimum = min(tracked.minimum, value)
                next[control.id] = tracked
            } else {
                next[control.id] = Extrema(peak: value, minimum: value)
            }
        }
        if next != extrema { extrema = next }
    }

    private func recordPower(_ property: LinkMessage.PropertyEntry) {
        guard let object = store.object("txState"), object.className == "TransmitState" else { return }
        let value: Double?
        switch property.value {
        case .f64(let number): value = number
        default: value = nil
        }
        receivedPower[property.name] = ReceivedPower(owner: receivedOwner(object: object), value: value)
    }

    var canReset: Bool { currentOwner() != nil && feed.description(for: "pa") != nil }

    func reset() -> Bool {
        synchronizeOwner()
        guard canReset else { return false }
        let current = currentValues()
        extrema = current.mapValues { Extrema(peak: $0, minimum: $0) }
        baseline = current
        lastTelemetry = store.currentTelemetryReceipt
        return true
    }

    func setShowPage(_ show: Bool) -> Bool {
        guard canReset else { return false }
        showPage = show
        defaults.set(show, forKey: Self.showPageKey)
        if !show {
            presentation = nil
            baseline = [:]
        }
        return true
    }

    static func owns(_ control: SetupDescription.Control, category: String) -> Bool {
        guard category == "pa" else { return false }
        if control.id == "pa.wattMeter.showPaValues" {
            return control.kind == .toggle && control.binding == .phone(showPageKey)
        }
        return ["pa.values.resetPeakMin", "pa.wattMeter.resetPaValues"].contains(control.id)
            && control.kind == .button && control.binding == .phone(resetAction)
    }

    func admit(_ control: SetupDescription.Control, category: String) -> Result<Action, SetupRefusal> {
        synchronizeOwner()
        guard Self.owns(control, category: category), canReset, let dispatcher else {
            return .failure(SetupRefusal(reason: SetupControlDispatcher.changedFirstReason))
        }
        return dispatcher.admit(control, in: category).map { Action(admission: $0, epoch: epoch) }
    }

    func perform(_ action: Action, value: SetupValue?) async -> SetupEditOutcome {
        synchronizeOwner()
        guard action.epoch == epoch, canReset, let dispatcher else {
            action.admission.revoke()
            return .notSent(SetupControlDispatcher.changedFirstReason)
        }
        return await dispatcher.perform(action.admission, value: value)
    }

    /// Preserves the native current reading and its reason. Celsius extrema
    /// are converted only for formatting, at the descriptor's existing places.
    func text(base: String, control: SetupDescription.Control, category: String) -> String {
        guard category == "pa", currentOwner() == owner, validValue(control) != nil,
              let tracked = extrema[control.id] else { return base }
        let fahrenheit = control.id == "pa.values.paTemperature"
            && (dispatcher?.temperatureInFahrenheit(control) ?? false)
        func formatted(_ number: Double) -> String {
            let value = fahrenheit ? number * 9 / 5 + 32 : number
            return String(format: "%.\(control.decimals ?? 2)f", value)
        }
        let peak = formatted(tracked.peak)
        let minimum = formatted(tracked.minimum)
        return peak == minimum ? base : "\(base)  (P \(peak) / M \(minimum))"
    }
}
