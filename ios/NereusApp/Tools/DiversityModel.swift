// NereusSDR for iOS: authoritative Diversity context, guarded coordinated actions and per-band phone memories
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror

@MainActor
final class DiversityModel: ObservableObject {
    static let phaseRange: ClosedRange<Double> = 0...360
    static let gainRange: ClosedRange<Double> = -20...20
    static let step = 0.1
    static let memoryCount = 8
    static let notConnectedReason = SpotsModel.notConnectedReason
    static let noSliceReason = "The Core has no slice open."
    static let notSentReason = "This Core does not send diversity. Updating the Core may help."
    static let pausedText = "PureSignal is calibrating on the air, so the Core holds diversity until it finishes."
    static let patternNotSentReason = "This Core does not send the diversity pattern. Updating the Core may help."
    static let patternUnreadableReason = "The Core has not sent a pattern this app can draw."
    static let memoryNote = "Memories are kept on this phone for each band. Turn on Store, then tap a memory to keep the phase and gain there."
    static let sliceClass = "SliceModel"

    @Published private(set) var sliceKey: String?
    @Published private(set) var enabled: Bool?
    @Published private(set) var phaseDeg: Double?
    @Published private(set) var gainDb: Double?
    @Published private(set) var fineNullEnabled: Bool?
    @Published private(set) var paused = false
    @Published private(set) var pauseReason: String?
    @Published private(set) var band: Int64?
    @Published private(set) var reason: String?
    @Published var storing = false
    @Published private(set) var memories: [Int: Memory] = [:]
    @Published private(set) var note: String?
    @Published private(set) var sensitivity: DiversitySensitivity?
    @Published private(set) var patternSent = false
    @Published private(set) var state: DiversityState?
    @Published private(set) var changing = false
    @Published private(set) var actionNotConfirmed = false

    @Published private(set) var blendGeneration: UInt64 = 0

    struct Memory: Equatable { let phaseDeg: Double; let gainDb: Double }
    private struct Participant {
        let id: Int
        let identity: DiversityState.Identity?
        let sliceObject: ObjectIdentifier
        let accessObject: ObjectIdentifier?
        let access: SliceAccess.State?
    }
    /// Captured before any Task or actor suspension. Its permit crosses physical route handoff.
    @MainActor private final class Touch {
        let snapshot: UInt64
        let radioSchema: UInt64?
        let sliceSchema: UInt64?
        let radioObject: ObjectIdentifier?
        let live: DiversityState.Identity?
        let revision: Int64?
        let participants: [Participant]
        let freeze: Bool
        let me: String?
        let permit = CommandSendPermit()
        var edit: UInt64?
        var property: String?
        init(snapshot: UInt64, radioSchema: UInt64?, sliceSchema: UInt64?, radioObject: ObjectIdentifier?,
             live: DiversityState.Identity?, revision: Int64?, participants: [Participant], freeze: Bool, me: String?) {
            self.snapshot = snapshot; self.radioSchema = radioSchema; self.sliceSchema = sliceSchema
            self.radioObject = radioObject; self.live = live; self.revision = revision
            self.participants = participants; self.freeze = freeze; self.me = me
        }
    }
    private struct Overlay { let identity: DiversityState.Identity?; let key: String; let value: MirrorValue }
    private struct DragContext: Equatable {
        let snapshot: UInt64
        let key: String?
        let live: DiversityState.Identity?
        let access: SliceAccess.State?
        let sliceObject: ObjectIdentifier?
        let accessObject: ObjectIdentifier?
        let radioObject: ObjectIdentifier?
        let sliceSchema: UInt64?
        let radioSchema: UInt64?
        let ready: Bool
        let me: String?
    }

    private let mirror: MirrorStore
    private let phone: PhoneSettings
    private let commands: CommandClient?
    private let slices: BandSlicesModel?
    private let captureSender: CommandClient.CaptureSender?
    private var watch: ToolMirrorWatch?
    private var fenceWatches: Set<AnyCancellable> = []
    private var objectWatches: [String: (ObjectIdentifier, AnyCancellable)] = [:]
    private var touches: [String: Touch] = [:]
    private var overlays: [String: Overlay] = [:]
    private var drags: [String: DragContext] = [:]
    private var lastBlendContext: DragContext?
    private var pendingEnabled: Bool?
    private var actionTouch: UInt64 = 0
    private var lastSummary: String?
    private var summaryEpoch: UInt64 = 0
    private lazy var writes = PropertyWriteQueue(store: mirror) { [weak self] property, outcome in
        self?.noteOutcome(outcome, property)
    }
    private lazy var actionQueue = CommandHoldQueue(store: mirror)

    init(mirror: MirrorStore, phone: PhoneSettings, commands: CommandClient? = nil,
         slices: BandSlicesModel? = nil, captureSender: CommandClient.CaptureSender? = nil) {
        self.mirror = mirror; self.phone = phone; self.commands = commands
        self.slices = slices; self.captureSender = captureSender
        watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        if let slices {
            watch?.watch(slices.$activeSliceId); watch?.watch(slices.$entries)
            slices.$entries.dropFirst().sink { [weak self] _ in self?.validateTouches() }.store(in: &fenceWatches)
        }
        mirror.$isSnapshotComplete.dropFirst().sink { [weak self] value in
            if !value { self?.revokeAll() }
        }.store(in: &fenceWatches)
        mirror.$isStale.dropFirst().sink { [weak self] value in
            if value { self?.revokeAll() }
        }.store(in: &fenceWatches)
        mirror.$schemaRevision.dropFirst().sink { [weak self] _ in self?.revokeAll() }.store(in: &fenceWatches)
        mirror.$capabilities.dropFirst().sink { [weak self] values in
            guard let self else { return }
            if values[DiversityState.capabilityName] != mirror.capabilities[DiversityState.capabilityName]
                || values[SliceAccess.capability] != mirror.capabilities[SliceAccess.capability] { self.revokeAll() }
        }.store(in: &fenceWatches)
        mirror.$objectKeys.dropFirst().sink { [weak self] keys in
            // Removal is visible here before recreation can reuse an object identifier.
            // Queued offers have no Touch to revoke, so retire their context synchronously too.
            self?.synchronizeBlendContext()
            self?.validateTouches(keys: Set(keys))
        }.store(in: &fenceWatches)
        refresh()
    }

    var coordinated: Bool { DiversityState.available(in: mirror) }
    private var ready: Bool { mirror.isSnapshotComplete && !mirror.isStale }
    private var currentState: DiversityState? {
        guard coordinated, let json = mirror.object("radio")?[DiversityState.propertyName]?.text else { return nil }
        return DiversityState(json: json)
    }
    private var activeId: Int? {
        if let slices { return slices.activeSliceId }
        return mirror.objects(ofClass: Self.sliceClass).first { $0["active"]?.flag == true }
            .flatMap { $0["sliceIndex"]?.whole }.flatMap { Int(exactly: $0) }
    }
    private func slice(_ id: Int) -> MirrorObject? {
        guard let object = mirror.object("slice:\(id)"), object.className == Self.sliceClass,
              object["sliceIndex"]?.whole == Int64(id) else { return nil }; return object
    }
    private var contextId: Int? {
        if coordinated {
            if let live = currentState?.live { return live.identity.sliceId }
            guard let id = activeId, let row = currentState?.targets.first(where: { $0.identity.sliceId == id }),
                  row.eligible, participantReason(id, identity: row.identity, freeze: false) == nil else { return nil }
            return id
        }
        return slice(0) == nil ? nil : 0
    }
    var contextLetter: String? {
        if coordinated, let live = state?.live { return live.letter }
        return contextId.map(SliceAccess.letter)
    }
    var contextTitle: String { contextLetter.map { "Slice " + $0 } ?? "Diversity off" }
    var frequencyText: String? {
        let hz = coordinated ? state?.live?.frequencyHz ?? contextId.flatMap { slice($0)?["frequency"]?.number }
                             : slice(0)?["frequency"]?.number
        guard let hz, hz.isFinite, hz >= 0 else { return nil }
        // Readonly summaries may cover transverters and any finite nonnegative f64, not Int64 admission.
        return String(format: hz < 1e18 ? "%.6f MHz" : "%.3g MHz", hz / 1_000_000)
    }
    var activeTargetLetter: String? { activeId.map(SliceAccess.letter) }
    var useReason: String? {
        guard let id = activeId else { return Self.noSliceReason }
        return actionReason(id, enabled: true)
    }
    var hasPendingAction: Bool { actionQueue.isSending("diversity.target") }
    var canUse: Bool { coordinated && useReason == nil && !changing }
    var switchReason: String? { actionReason(contextId, enabled: !(state?.requested ?? enabled ?? false)) }
    var canSwitch: Bool { switchReason == nil && !changing }
    var moveUpdateReason: String? { coordinated ? nil : DiversityState.updateReason }
    var patternReason: String? {
        if !ready { return Self.notConnectedReason }
        guard patternSent else { return Self.patternNotSentReason }
        return sensitivity == nil ? Self.patternUnreadableReason : nil
    }
    func isUnconfirmed(_ property: String) -> Bool {
        property == "diversityEnabled" && coordinated ? actionNotConfirmed
            : sliceKey.map { mirror.isUnconfirmed($0, property: property) } ?? false
    }

    func refresh() {
        installObjectFences()
        for object in mirror.objects(ofClass: Self.sliceClass) + mirror.objects(ofClass: SliceAccess.accessClass) {
            _ = watch?.object(object.key)
        }
        _ = watch?.object("radio"); _ = watch?.object(SeveralDevices.connectedDevicesKey)
        let json = mirror.object("radio")?[DiversityState.propertyName]?.text
        if json != lastSummary { lastSummary = json; summaryEpoch &+= 1 }
        state = currentState
        synchronizeBlendContext()
        let id = contextId
        let object = id.flatMap(slice)
        sliceKey = id.map { "slice:\($0)" }
        if let touch = touches["target"], touch.permit.isRevoked { pendingEnabled = nil }
        if coordinated {
            enabled = pendingEnabled ?? state?.requested
            paused = state?.paused ?? false
            pauseReason = paused ? state?.reason : nil
            band = state?.live?.band ?? object?["band"]?.whole
            let live = state?.live
            let corePhase = live?.phaseDeg ?? object?["diversityPhaseDeg"]?.number
            let coreGain = live?.gainDb ?? object?["diversityGainDb"]?.number
            phaseDeg = presented("diversityPhaseDeg", core: corePhase)
            gainDb = presented("diversityGainDb", core: coreGain)
            fineNullEnabled = live?.fineNullEnabled ?? object?["diversityFineNullEnabled"]?.flag
        } else {
            enabled = object?["diversityEnabled"]?.flag
            phaseDeg = object?["diversityPhaseDeg"]?.number; gainDb = object?["diversityGainDb"]?.number
            fineNullEnabled = object?["diversityFineNullEnabled"]?.flag
            paused = object?["psPaused"]?.flag == true; pauseReason = paused ? Self.pausedText : nil
            band = object?["band"]?.whole
        }
        reason = !ready ? Self.notConnectedReason : coordinated && state == nil ? Self.notSentReason
            : id == nil ? (coordinated && activeId != nil ? useReason ?? Self.noSliceReason : Self.noSliceReason)
            : participantReason(id!, identity: state?.live?.identity, freeze: false)
                ?? (phaseDeg == nil || gainDb == nil ? Self.notSentReason : nil)
        memories = readMemories()
        patternSent = (mirror.agreedMinor ?? 0) >= DiversityPattern.minor
            && mirror.capabilityVersion(DiversityPattern.capabilityName) >= 1
        let patternJSON = coordinated ? state?.live?.pattern : object?[DiversityPattern.propertyName]?.text
        let pattern = ready && patternSent ? patternJSON.flatMap(DiversityPattern.init(json:)) : nil
        sensitivity = pattern.map { DiversitySensitivity(shares: $0.points, stepDeg: $0.stepDeg) }
    }
    private func presented(_ property: String, core: Double?) -> Double? {
        guard let overlay = overlays[property], overlay.key == sliceKey,
              overlay.identity == currentState?.live?.identity else { overlays[property] = nil; return core }
        if core == overlay.value.number { overlays[property] = nil; return core }
        return overlay.value.number
    }

    /// The badge supplies exact identity; opening never activates, tunes, joins or sets a target.
    func badgeMatches(_ id: Int, incarnation: Int64?) -> Bool {
        guard ready else { return false }
        if coordinated {
            guard let live = currentState?.live, live.identity.sliceId == id else { return false }
            return incarnation == nil || live.identity.incarnation == incarnation
        }
        return id == 0 && slice(0)?["diversityEnabled"]?.flag == true
    }
    func useActiveSlice() { if let id = activeId { setTarget(id, enabled: true) } }
    func setEnabled(_ on: Bool) {
        if coordinated { setTarget(on ? contextId : nil, enabled: on) }
        else { write("diversityEnabled", .bool(on)) }
    }
    func actionReason(_ id: Int?, enabled: Bool) -> String? {
        guard ready else { return Self.notConnectedReason }
        if !coordinated {
            guard id == nil || id == 0 else { return DiversityState.updateReason }
            guard slice(0) != nil else { return Self.noSliceReason }
            return participantReason(0, identity: nil, freeze: true)
        }
        guard let state = currentState else { return Self.notSentReason }
        if let source = state.live?.identity,
           let reason = participantReason(source.sliceId, identity: source, freeze: true) { return reason }
        if enabled {
            guard let id, let target = state.targets.first(where: { $0.identity.sliceId == id }) else { return Self.noSliceReason }
            if let reason = participantReason(id, identity: target.identity, freeze: true) { return reason }
            if !target.eligible { return target.reason.isEmpty ? Self.notSentReason : target.reason }
        }
        guard commands != nil, captureSender != nil else { return Self.notConnectedReason }
        return nil
    }

    func setTarget(_ id: Int?, enabled on: Bool) {
        guard coordinated else { if id == nil || id == 0 { setEnabled(on) }; return }
        guard actionReason(id, enabled: on) == nil, let state = currentState,
              let action = DiversityTargetAction(state: state, enabled: on, targetSliceId: on ? id : nil),
              let commands, let sender = captureSender?(),
              let touch = captureTouch(identities: [action.source, action.target].compactMap { $0 },
                                       revision: action.revision, freeze: true) else { refresh(); return }
        replaceTouch("target", touch)
        actionTouch &+= 1
        let serial = actionTouch, epoch = summaryEpoch
        pendingEnabled = on; changing = true; actionNotConfirmed = false; enabled = on; note = nil
        actionQueue.send("diversity.target", shows: [], invokeWithLate: { [weak self] late in
            guard let self, self.touchAllowed(touch), !touch.permit.isRevoked else { throw CommandError.notSent }
            return try await commands.invokeBound(DiversityTargetAction.verb, arguments: action.arguments,
                timeout: .seconds(5), sender: sender, stillAllowed: { [weak self] in
                    await self?.touchAllowed(touch) ?? false
                }, authority: touch.permit, onLateOutcome: { outcome in await late(outcome) })
        }, onOutcome: { [weak self] outcome in
            guard let self, self.actionTouch == serial, self.participantsCurrent(touch) else { return }
            self.pendingEnabled = nil; self.changing = false
            if case .success(let result) = outcome {
                guard case .text(let json)? = result.values[DiversityState.propertyName],
                      let answer = DiversityState(json: json) else {
                    self.note = result.reason.isEmpty ? Self.notSentReason : result.reason; self.refresh(); return
                }
                if let current = self.currentState, answer.revision < current.revision { self.refresh(); return }
                // Equal summary revisions can carry newer blend samples. Never replace a later radio value with an older result.
                if answer.revision > (self.currentState?.revision ?? -1) || self.summaryEpoch == epoch {
                    self.mirror.apply(.delta(.init(key: "radio", properties: [
                        .init(ordinal: DiversityState.ordinal, name: DiversityState.propertyName, value: .utf8(json)),
                    ])))
                }
                self.actionNotConfirmed = false
                self.note = result.reason.isEmpty ? nil : result.reason
            } else {
                let change = PropertyWriteOutcome(outcome)
                self.note = change.noteText(refused: Self.notSentReason)
                self.actionNotConfirmed = change == .notConfirmed
            }
            self.refresh()
        })
    }

    func blendEditing(_ property: String, _ editing: Bool) {
        if editing { drags[property] = dragContext }
        else { drags[property] = nil }
    }
    private var dragContext: DragContext {
        let id = contextId
        return DragContext(snapshot: mirror.snapshotIdentity, key: id.map { "slice:\($0)" },
                    live: currentState?.live?.identity, access: id.flatMap { SliceAccess.states(in: mirror)[$0] },
                    sliceObject: id.flatMap(slice).map(ObjectIdentifier.init),
                    accessObject: id.flatMap { mirror.object("access:\($0)") }.map(ObjectIdentifier.init),
                    radioObject: mirror.object("radio").map(ObjectIdentifier.init),
                    sliceSchema: mirror.currentSessionSchemaIdentity(ofClass: Self.sliceClass),
                    radioSchema: mirror.currentSessionSchemaIdentity(ofClass: "RadioModel"),
                    ready: ready, me: slices?.thisDeviceId)
    }
    private func synchronizeBlendContext() {
        let context = dragContext
        if let lastBlendContext, lastBlendContext != context { blendGeneration &+= 1 }
        lastBlendContext = context
    }
    func capturePhaseCommit(_ value: Double) -> ToolPageParts.SliderCommitOffer {
        captureBlendCommit(value, property: "diversityPhaseDeg")
    }
    func captureGainCommit(_ value: Double) -> ToolPageParts.SliderCommitOffer {
        captureBlendCommit(value, property: "diversityGainDb")
    }
    private func captureBlendCommit(_ value: Double, property: String) -> ToolPageParts.SliderCommitOffer {
        synchronizeBlendContext()
        let generation = blendGeneration, context = dragContext
        let sendValue: (Double) -> Void = { [weak self] value in
            guard let self, self.blendGeneration == generation, self.dragContext == context,
                  self.sliceKey == context.key else { return }
            // This fence precedes write(), which captures a new bound Touch/session permit.
            if property == "diversityPhaseDeg" { self.setPhase(value) }
            else { self.setGain(value) }
        }
        return ToolPageParts.SliderCommitOffer(value: value, generation: generation, revalue: { final in
            ToolPageParts.SliderCommitOffer(value: final, generation: generation) { sendValue(final) }
        }) { sendValue(value) }
    }
    func setPhase(_ value: Double) {
        guard value.isFinite else { return }
        write("diversityPhaseDeg", .double(Self.tenths(min(max(value, Self.phaseRange.lowerBound), Self.phaseRange.upperBound))))
    }
    func setGain(_ value: Double) {
        guard value.isFinite else { return }
        write("diversityGainDb", .double(Self.tenths(min(max(value, Self.gainRange.lowerBound), Self.gainRange.upperBound))))
    }
    func setFineNull(_ value: Bool) { write("diversityFineNullEnabled", .bool(value)) }
    private func write(_ property: String, _ value: MirrorValue) {
        guard reason == nil, let id = contextId, let key = sliceKey,
              drags[property].map({ $0 == dragContext }) ?? true else { return }
        if property == "diversityEnabled", actionReason(0, enabled: value.flag == true) != nil { return }
        if let captureSender {
            guard let sender = captureSender(), let touch = captureTouch(ids: [id], revision: nil, freeze: property == "diversityEnabled") else { return }
            replaceTouch(property, touch); touch.property = property
            if coordinated { overlays[property] = Overlay(identity: currentState?.live?.identity, key: key, value: value) }
            touch.edit = writes.writeBound(key, property, value, sender: sender, authority: touch.permit,
                              stillAllowed: { [weak self] in self?.touchAllowed(touch) ?? false })
            refresh()
        } else if !coordinated {
            // Existing standalone legacy consumers have their own MirrorStore sender. The app always supplies a captured SessionRoute.
            writes.write(key, property, value)
        }
    }
    private func noteOutcome(_ outcome: PropertyWriteOutcome, _ property: String) {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return }
        if let touch = touches[property] {
            guard touchAllowed(touch), touch.participants.first?.id == contextId else { return }
        }
        if !outcome.accepted { overlays[property] = nil }
        note = outcome.noteText(refused: Self.notSentReason)
        refresh()
    }

    private func participantReason(_ id: Int, identity: DiversityState.Identity?, freeze: Bool) -> String? {
        guard ready else { return Self.notConnectedReason }
        let states = SliceAccess.states(in: mirror), access = states[id]
        if SliceAccess.available(in: mirror) || coordinated {
            guard let me = slices?.thisDeviceId, !me.isEmpty else { return Self.notConnectedReason }
            if let identity, identity.controllerDeviceId != me {
                let owner = SliceAccess.State(sliceId: id, incarnation: identity.incarnation,
                    controllerDeviceId: identity.controllerDeviceId, controlRevision: identity.controlRevision)
                return SliceAccess.ownerLine(owner, devices: SeveralDevices.connectedDevices(in: mirror))
            }
            guard let access else { return Self.notConnectedReason }
            if access.controllerDeviceId != me { return SliceAccess.ownerLine(access, devices: SeveralDevices.connectedDevices(in: mirror)) }
            guard access.listenerDeviceIds.contains(me), access.incarnation > 0, access.controlRevision > 0 else { return Self.notConnectedReason }
            if let identity, identity.incarnation != access.incarnation || identity.controlRevision != access.controlRevision { return Self.notConnectedReason }
        }
        guard slice(id) != nil else { return Self.notConnectedReason }
        if freeze, access?.onAir == true { return SliceAccess.transmittingText(id) }
        return nil
    }
    private func captureTouch(identities: [DiversityState.Identity], revision: Int64?, freeze: Bool) -> Touch? {
        captureTouch(ids: Array(Set(identities.map(\.sliceId))).sorted(), identities: identities, revision: revision, freeze: freeze)
    }
    private func captureTouch(ids: [Int], identities: [DiversityState.Identity] = [], revision: Int64?, freeze: Bool) -> Touch? {
        var participants: [Participant] = []
        for id in ids {
            let identity = identities.first { $0.sliceId == id }
                ?? currentState?.live.flatMap { $0.identity.sliceId == id ? $0.identity : nil }
                ?? currentState?.targets.first { $0.identity.sliceId == id }?.identity
            guard participantReason(id, identity: identity, freeze: freeze) == nil, let object = slice(id) else { return nil }
            let accessObject = mirror.object("access:\(id)")
            participants.append(Participant(id: id, identity: identity, sliceObject: ObjectIdentifier(object),
                accessObject: accessObject.map(ObjectIdentifier.init), access: accessObject.flatMap(SliceAccess.state)))
        }
        return Touch(snapshot: mirror.snapshotIdentity, radioSchema: mirror.currentSessionSchemaIdentity(ofClass: "RadioModel"),
            sliceSchema: mirror.currentSessionSchemaIdentity(ofClass: Self.sliceClass),
            radioObject: mirror.object("radio").map(ObjectIdentifier.init), live: currentState?.live?.identity,
            revision: revision, participants: participants, freeze: freeze, me: slices?.thisDeviceId)
    }
    private func replaceTouch(_ slot: String, _ touch: Touch) {
        if let old = touches[slot] { revoke(old) }
        touches[slot] = touch
    }
    private func revoke(_ touch: Touch) {
        touch.permit.revoke()
        if let property = touch.property, let edit = touch.edit, let id = touch.participants.first?.id {
            mirror.retireBoundEdit("slice:\(id)", property: property, edit: edit)
            overlays[property] = nil
        }
    }
    private func revokeAll() {
        for touch in touches.values { revoke(touch) }
        pendingEnabled = nil; changing = false; actionTouch &+= 1
    }
    private func validateTouches(overrides: [String: [String: MirrorValue]] = [:], keys: Set<String>? = nil) {
        for touch in touches.values where !touch.permit.isRevoked {
            if !touchAllowed(touch, overrides: overrides, keys: keys) {
                revoke(touch)
                if touch === touches["target"] { pendingEnabled = nil; changing = false }
            }
        }
    }
    private func touchAllowed(_ touch: Touch, overrides: [String: [String: MirrorValue]] = [:], keys: Set<String>? = nil) -> Bool {
        guard ready, mirror.snapshotIdentity == touch.snapshot, slices?.thisDeviceId == touch.me,
              mirror.currentSessionSchemaIdentity(ofClass: "RadioModel") == touch.radioSchema,
              mirror.currentSessionSchemaIdentity(ofClass: Self.sliceClass) == touch.sliceSchema,
              mirror.object("radio").map(ObjectIdentifier.init) == touch.radioObject else { return false }
        if coordinated {
            let json: String?
            if let values = overrides["radio"] { json = values[DiversityState.propertyName]?.text }
            else { json = mirror.object("radio")?[DiversityState.propertyName]?.text }
            guard let json, let state = DiversityState(json: json), state.live?.identity == touch.live,
                  touch.revision.map({ $0 == state.revision }) ?? true else { return false }
        }
        return participantsCurrent(touch, overrides: overrides, keys: keys)
    }
    private func participantsCurrent(_ touch: Touch, overrides: [String: [String: MirrorValue]] = [:], keys: Set<String>? = nil) -> Bool {
        guard ready, mirror.snapshotIdentity == touch.snapshot, slices?.thisDeviceId == touch.me,
              mirror.currentSessionSchemaIdentity(ofClass: "RadioModel") == touch.radioSchema,
              mirror.currentSessionSchemaIdentity(ofClass: Self.sliceClass) == touch.sliceSchema,
              mirror.object("radio").map(ObjectIdentifier.init) == touch.radioObject else { return false }
        for participant in touch.participants {
            let key = "slice:\(participant.id)", accessKey = "access:\(participant.id)"
            if let keys, !keys.contains(key) || (participant.accessObject != nil && !keys.contains(accessKey)) { return false }
            guard let object = slice(participant.id), ObjectIdentifier(object) == participant.sliceObject else { return false }
            if let values = overrides[key], values["sliceIndex"]?.whole != Int64(participant.id) { return false }
            if let captured = participant.access {
                guard let object = mirror.object(accessKey), ObjectIdentifier(object) == participant.accessObject else { return false }
                let values = overrides[accessKey] ?? object.values
                guard values["incarnation"]?.whole == captured.incarnation,
                      values["controlRevision"]?.whole == captured.controlRevision,
                      values["controllerDeviceId"]?.text == captured.controllerDeviceId,
                      captured.controllerDeviceId == touch.me,
                      let listeners = values["listenerDeviceIds"]?.text,
                      case .array(let ids)? = try? LinkJSON.parse(listeners),
                      ids.contains(.string(captured.controllerDeviceId)),
                      !touch.freeze || values["onAir"]?.flag == false else { return false }
            } else if SliceAccess.available(in: mirror) || coordinated { return false }
        }
        return true
    }
    private func installObjectFences() {
        let objects = mirror.objects(ofClass: Self.sliceClass) + mirror.objects(ofClass: SliceAccess.accessClass)
            + [mirror.object("radio")].compactMap { $0 }
        let keys = Set(objects.map(\.key))
        for key in objectWatches.keys where !keys.contains(key) { objectWatches[key] = nil }
        for object in objects {
            let id = ObjectIdentifier(object)
            guard objectWatches[object.key]?.0 != id else { continue }
            let key = object.key
            let observer = object.$values.dropFirst().sink { [weak self] values in
                // @Published publishes before assignment; compare the incoming authoritative dictionary now.
                self?.validateTouches(overrides: [key: values])
            }
            objectWatches[key] = (id, observer)
        }
    }

    func tapMemory(_ slot: Int) {
        refresh() // Resolve the current admitted context before explicit recall replaces drag ownership.
        guard reason == nil, (0..<Self.memoryCount).contains(slot), let band else { return }
        if storing {
            guard let phaseDeg, let gainDb else { return }
            phone.setString("\(Self.tenths(phaseDeg)),\(Self.tenths(gainDb))", for: Self.memoryKey(band: band, slot))
            storing = false; memories = readMemories(); return
        }
        guard let memory = memories[slot] else { return }
        // Explicit recall supersedes earlier offers before either direct write captures its Touch.
        blendGeneration &+= 1
        drags.removeAll()
        setPhase(memory.phaseDeg); setGain(memory.gainDb)
    }
    static func memoryKey(band: Int64, _ slot: Int) -> String { "diversity.memory.\(band).\(slot)" }
    private func readMemories() -> [Int: Memory] {
        guard let band else { return [:] }; var read: [Int: Memory] = [:]
        for slot in 0..<Self.memoryCount {
            let parts = phone.string(Self.memoryKey(band: band, slot), default: "").split(separator: ",")
            if parts.count == 2, let phase = Double(parts[0]), let gain = Double(parts[1]),
               Self.phaseRange.contains(phase), Self.gainRange.contains(gain) { read[slot] = Memory(phaseDeg: phase, gainDb: gain) }
        }
        return read
    }
    static func tenths(_ value: Double) -> Double { (value * 10).rounded() / 10 }
}

/// The same typed Tools readers used by the accepted model, kept local to this consumer.
private extension MirrorValue {
    var number: Double? { ToolValue.number(self) }
    var whole: Int64? { ToolValue.whole(self) }
    var flag: Bool? { ToolValue.flag(self) }
    var text: String? { ToolValue.text(self) }
}
