// NereusSDR for iOS: dedicated ephemeral Diversity UI fixture, no transport or credentials
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#if DEBUG
import Combine
import Foundation
import NereusLink
import NereusMedia
import NereusMirror
import SwiftUI

@MainActor
final class UITestDiversityFixture {
    static let argument = "-NereusDiversityFixture"
    static let lightArgument = "-NereusDiversityLightValidation"
    static func isEnabled(_ arguments: [String]) -> Bool { arguments.contains(argument) }
    static func scheme(_ arguments: [String]) -> ColorScheme {
        isEnabled(arguments) && arguments.contains(lightArgument) ? .light : .dark
    }
    // Separate DEBUG probe for an actual environment assertion, never public product controls.
    static func schemeProbe(_ arguments: [String]) -> Bool {
        isEnabled(arguments) && arguments.contains("-NereusDiversityEnvironmentProbe")
    }

    /// Ephemeral receipts from the synthetic Core, exposed only by the double-guarded DEBUG probe.
    @MainActor final class BlendReceipts: ObservableObject {
        @Published private(set) var phaseWrites = 0
        @Published private(set) var gainWrites = 0
        @Published private(set) var phase = 123.4
        @Published private(set) var gain = -2.0
        var value: String {
            "phaseWrites=\(phaseWrites);gainWrites=\(gainWrites);phase="
                + String(format: "%.1f", phase) + ";gain=" + String(format: "%.1f", gain)
        }
        func reset() { phaseWrites = 0; gainWrites = 0; phase = 123.4; gain = -2 }
        func confirmed(_ property: String, value: Double) {
            if property == "diversityPhaseDeg" { phase = value; phaseWrites += 1 }
            else if property == "diversityGainDb" { gain = value; gainWrites += 1 }
        }
    }
    static let blendReceipts = BlendReceipts()

    /// Only editing callbacks and confirmed synthetic writes, not a product state override.
    @MainActor final class SliderLifecycle: ObservableObject {
        @Published private(set) var events: [String] = []
        private var epoch = DispatchTime.now().uptimeNanoseconds
        func reset() { events = []; epoch = DispatchTime.now().uptimeNanoseconds }
        static func exact(_ value: Double) -> String {
            String(format: "%.17g", value) + "[bits=\(String(value.bitPattern, radix: 16))]"
        }
        private func append(_ event: String) {
            guard UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) else { return }
            let elapsed = DispatchTime.now().uptimeNanoseconds - epoch
            events.append("\(events.count + 1):tNs=\(elapsed):\(event)")
        }
        func record(_ event: String, model: DiversityModel) {
            guard UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) else { return }
            append("\(event):modelPhase=\(model.phaseDeg.map(Self.exact) ?? "nil"):modelGain=\(model.gainDb.map(Self.exact) ?? "nil")")
        }
        func origin(_ event: String, identifier: String, releaseEditing: Bool, draftEditing: Bool,
                    draft: Double?, model: Double?, offered: Double?) {
            guard UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) else { return }
            append("\(identifier).\(event):releaseEditing=\(releaseEditing):draftEditing=\(draftEditing):draft=\(draft.map(Self.exact) ?? "nil"):model=\(model.map(Self.exact) ?? "nil"):offered=\(offered.map(Self.exact) ?? "nil")")
        }
        /// Guarded numeric native-control measurements; shares the existing ordered trace.
        func native(_ event: String, identifier: String, fields: [(String, Double?)]) {
            guard UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) else { return }
            let measured = fields.map { "\($0.0)=\($0.1.map(Self.exact) ?? "nil")" }.joined(separator: ":")
            append("\(identifier).native.\(event):\(measured)")
        }
        var value: String { events.joined(separator: ";") }
    }
    static let sliderLifecycle = SliderLifecycle()

    private let app: AppModel
    private let commands: CommandClient
    private var revision = 12
    private var live: Int? = 1
    private let listening: Bool
    private let hugeReadonly: Bool
    private let separated: Bool
    private var phase = 123.4
    private var gain = -2.0

    static func makeApp(arguments: [String]) -> AppModel {
        blendReceipts.reset()
        sliderLifecycle.reset()
        let suite = "DiversityFixture-" + UUID().uuidString
        let defaults = UserDefaults(suiteName: suite)!
        defaults.removePersistentDomain(forName: suite)
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults))
        let fixture = UITestDiversityFixture(app: app, arguments: arguments)
        fixture.populate(arguments: arguments)
        app.diversity = DiversityModel(mirror: app.mirror, phone: app.phoneSettings,
                                      commands: fixture.commands, slices: app.main.slices,
                                      captureSender: { { message, permit in
            guard !permit.isRevoked else { throw LinkSendError.notConnected }
            try await fixture.receive(message, permit: permit)
        } })
        Task { await fixture.commands.handle(.stateChanged(.ready)) }
        return app
    }

    static func finishLaunch(app: AppModel) {
        app.main.slices.thisDeviceId = "phone"
    }

    private init(app: AppModel, arguments: [String]) {
        self.app = app
        commands = CommandClient(send: { _ in throw LinkSendError.notConnected })
        listening = arguments.contains("-NereusDiversityListening")
        hugeReadonly = arguments.contains("-NereusDiversityHugeReadonly")
        separated = arguments.contains("-NereusDiversitySeparated")
        if hugeReadonly { live = 4 }
    }

    private func populate(arguments: [String]) {
        let mirror = app.mirror
        mirror.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        mirror.apply(.capabilities(.init(properties: [
            .init(name: "diversityControlVersion", value: .i64(1)),
            .init(name: "sliceAccessVersion", value: .i64(3)),
            .init(name: "propertyResultVersion", value: .i64(1)),
        ])))
        mirror.apply(.schema(.init(className: "RadioModel", fields: [.init(ordinal: 37, name: "diversityState", kind: .utf8)])))
        mirror.apply(.schema(.init(className: "SliceModel", fields: [
            .init(ordinal: 29, name: "diversityEnabled", kind: .bool),
            .init(ordinal: 30, name: "diversityPhaseDeg", kind: .f64),
            .init(ordinal: 31, name: "diversityGainDb", kind: .f64),
        ])))
        mirror.apply(.objectCreate(.init(key: "radio", className: "RadioModel", properties: [
            .init(ordinal: 37, name: "diversityState", value: .utf8(summary())),
        ])))
        let active = arguments.contains("-NereusDiversityActiveC") ? 2 : arguments.contains("-NereusDiversityFullFlag") && !separated ? 1 : 0
        for id in 0...2 {
            mirror.apply(.objectCreate(.init(key: "slice:\(id)", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(separated && id == 1 ? 7_256_400 : 7_236_400 + Double(id) * 100)),
                .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
                .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
                .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
                .init(ordinal: 6, name: "stepHz", value: .i64(100)),
                .init(ordinal: 11, name: "active", value: .bool(id == active)),
                .init(ordinal: 12, name: "txSlice", value: .bool(id == 0)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(Int64(id))),
                .init(ordinal: 14, name: "band", value: .enumeration(5)),
                .init(ordinal: 29, name: "diversityEnabled", value: .bool(id == live)),
                .init(ordinal: 30, name: "diversityPhaseDeg", value: .f64(phase)),
                .init(ordinal: 31, name: "diversityGainDb", value: .f64(gain)),
            ])))
            mirror.apply(.objectCreate(.init(key: "access:\(id)", className: "SliceAccess", properties: [
                .init(ordinal: 0, name: "sliceId", value: .i64(Int64(id))),
                .init(ordinal: 1, name: "incarnation", value: .i64(Int64(100 + id))),
                .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(owner(id))),
                .init(ordinal: 3, name: "controlRevision", value: .i64(4)),
                .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8(LinkJSON.array(owner(id) == "phone" ? [.string("phone")] : [.string(owner(id)), .string("phone")]).compactText)),
                .init(ordinal: 6, name: "txSelected", value: .bool(id == 0)),
                .init(ordinal: 7, name: "onAir", value: .bool(false)),
            ])))
        }
        app.main.slices.thisDeviceId = "phone"
        mirror.apply(.snapshotComplete)
        app.main.band.endpointId = 1
        let context: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("diversity-fixture"), "endpointId": .number(1),
            "revision": .number(1), "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(7_244_500), "sampleRateHz": .number(192_000), "centreHz": .number(7_244_500),
            "spanHz": .number(48_000), "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(1206), "waterfallSamples": .number(1206), "wideSamples": .number(0),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        if let value = MediaControlDecoder.context(context, wideband: false, grant: false) { app.main.band.receive(.context(value)) }
    }

    private func owner(_ id: Int) -> String { id >= 3 || ((listening || hugeReadonly) && id == live) ? "desktop" : "phone" }
    private func summary() -> String {
        let liveObject: LinkJSON = live.map { id in .object([
            "sliceId": .number(Double(id)), "incarnation": .number(Double(100 + id)), "controlRevision": .number(4),
            "controllerDeviceId": .string(owner(id)), "letter": .string(SliceAccess.letter(id)), "band": .number(5),
            "frequencyHz": .number(hugeReadonly ? 1e300 : separated && id == 1 ? 7_256_400 : 7_236_400 + Double(id) * 100),
            "phaseDeg": .number(phase), "gainDb": .number(gain), "fineNullEnabled": .bool(false), "pattern": .null,
        ]) } ?? .null
        return LinkJSON.object([
            "version": .number(1), "revision": .number(Double(revision)), "requested": .bool(live != nil),
            "running": .bool(live != nil), "paused": .bool(false), "reasonCode": .string(""), "reason": .string(""),
            "live": liveObject, "targets": .array((0...4).map { id in .object([
                "sliceId": .number(Double(id)), "incarnation": .number(Double(100 + id)), "controlRevision": .number(4),
                "controllerDeviceId": .string(owner(id)), "eligible": .bool(true), "reasonCode": .string(""), "reason": .string(""),
            ]) }),
        ]).compactText
    }

    private func receive(_ message: LinkMessage, permit: CommandSendPermit) async throws {
        guard !permit.isRevoked else { throw LinkSendError.notConnected }
        switch message {
        case .commandInvoke(let request):
            guard request.verb == "diversity.setTarget", request.args.map(\.name) == ["enabled", "stateRevision", "sourceSliceId", "sourceIncarnation", "sourceControlRevision", "targetSliceId", "targetIncarnation", "targetControlRevision"] else { throw LinkSendError.notConnected }
            guard case .bool(let enabled) = request.args[0].value, case .i64(let target) = request.args[5].value else { throw LinkSendError.notConnected }
            guard !enabled || (0...4).contains(target) else { throw LinkSendError.notConnected }
            let source = live
            live = enabled ? Int(target) : nil
            revision += 1
            let state = summary()
            app.mirror.apply(.delta(.init(key: "radio", properties: [.init(ordinal: 37, name: "diversityState", value: .utf8(state))])))
            let notice = source != nil && live != nil && source != live ? "Diversity moved from \(SliceAccess.letter(source!)) to \(SliceAccess.letter(live!)). Both slices paused briefly." : ""
            await commands.receive(.commandResult(.init(verb: request.verb, id: request.id, accepted: true,
                reason: notice, affected: ["radio"], values: [.init(name: "diversityState", value: .utf8(state)), .init(name: "reasonCode", value: .utf8(""))])))
        case .propertyWrite(let request):
            // The actual write envelope and correlated result; never accept another fixture's participant.
            guard app.mirror.isSnapshotComplete, !app.mirror.isStale,
                  let live, request.key == "slice:\(live)", owner(live) == "phone",
                  app.main.slices.thisDeviceId == "phone",
                  let access = SliceAccess.states(in: app.mirror)[live],
                  access.incarnation == Int64(100 + live), access.controlRevision == 4,
                  access.controllerDeviceId == "phone", access.listenerDeviceIds.contains("phone"),
                  let object = app.mirror.object(request.key), object.className == "SliceModel",
                  object["sliceIndex"] == .int(Int64(live)),
                  let writeId = request.writeId, writeId > 0, request.properties.count == 1,
                  let entry = request.properties.first, case .f64(let value) = entry.value, value.isFinite
            else { throw LinkSendError.notConnected }
            switch (entry.ordinal, entry.name) {
            case (30, "diversityPhaseDeg") where DiversityModel.phaseRange.contains(value): phase = value
            case (31, "diversityGainDb") where DiversityModel.gainRange.contains(value): gain = value
            default: throw LinkSendError.notConnected
            }
            revision += 1
            app.mirror.apply(.delta(.init(key: request.key, properties: [entry])))
            app.mirror.apply(.delta(.init(key: "radio", properties: [
                .init(ordinal: 37, name: "diversityState", value: .utf8(summary())),
            ])))
            app.mirror.apply(.propertyResult(.init(key: request.key, writeId: writeId, results: [
                .init(property: entry.name, accepted: true, reason: "", value: entry),
            ])))
            Self.blendReceipts.confirmed(entry.name, value: value)
            Self.sliderLifecycle.record("confirmed.\(entry.name)=\(SliderLifecycle.exact(value)):writeId=\(writeId):ordinal=\(entry.ordinal)", model: app.diversity)
        default: throw LinkSendError.notConnected
        }
    }
}
/// Scene overlay only when fixture + explicit probe arguments are both present.
struct DiversityFixtureSchemeProbe: View {
    @Environment(\.colorScheme) private var scheme
    @ObservedObject private var receipts = UITestDiversityFixture.blendReceipts
    @ObservedObject private var lifecycle = UITestDiversityFixture.sliderLifecycle
    var body: some View {
        ZStack {
            Color.clear.frame(width: 1, height: 1)
                .allowsHitTesting(false)
                .accessibilityElement(children: .ignore)
                .accessibilityIdentifier("diversity.fixture.scheme")
                .accessibilityLabel("Diversity fixture colour scheme")
                .accessibilityValue(scheme == .light ? "light" : "dark")
            Color.clear.frame(width: 1, height: 1)
                .allowsHitTesting(false)
                .accessibilityElement(children: .ignore)
                .accessibilityIdentifier("diversity.fixture.blendReceipts")
                .accessibilityLabel("Synthetic Core confirmed blend receipts")
                .accessibilityValue(receipts.value)
            Color.clear.frame(width: 1, height: 1)
                .allowsHitTesting(false)
                .accessibilityElement(children: .ignore)
                .accessibilityIdentifier("diversity.fixture.sliderLifecycle")
                .accessibilityLabel("Synthetic Diversity slider lifecycle diagnostics")
                .accessibilityValue(lifecycle.value)
        }
    }
}
#endif
