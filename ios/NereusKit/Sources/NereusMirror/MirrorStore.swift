// NereusSDR for iOS: the app's copy of the Core's objects, capabilities and telemetry
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusLink
import os

/// The Core's state as the app sees it (link document sections 5 to 7 and
/// 10): the classes it described, the objects it created with their latest
/// values, its capabilities, its telemetry, and whether all of it is
/// current. It is fed the session's events with `handle(_:)`, or its
/// messages with `apply(_:)`.
///
/// Each control writes to its owner (R-IOS-18): a write goes to the Core,
/// which decides it. The store never refuses a write on its own, because
/// the wire schema carries only each property's ordinal, name and kind, not
/// who sets it; a write to a property the Core sets itself comes back
/// refused, with the Core's reason.
///
/// A write shows the operator's value at once and keeps it until the Core
/// answers that write, as the desktop's remote window does (JJ, 2026-10-01;
/// `StationClient.cpp:1040-1068`, the outbound watcher's `m_pendingWrites`,
/// "the operator's value, held until the Core answers this write"). A delta
/// of a held property is not shown meanwhile, only kept as the Core's value
/// (`StationClient::handleDelta`, `:4196-4206`). The answer decides: the
/// Core's kept value shows, and a refusal without a value puts the Core's
/// last value back (`handlePropertyResult`, `:4216-4268`). A newer value
/// of the same property replaces an older one still waiting, and the older
/// one's answer only updates the Core's value behind it. A lost link puts
/// the Core's value back on every held property (`:2219` clears the
/// pending writes) and resolves their writes as not confirmed; nothing is
/// sent again. A write with no answer after answerDeadline keeps the
/// latest Core value, marked in unconfirmedWrites, and its late answer
/// goes to lateAnswers.
@MainActor
public final class MirrorStore: ObservableObject {
    /// Sends one message to the Core.
    public typealias Sender = @Sendable (LinkMessage) async throws -> Void
    public typealias BoundSender = @Sendable (LinkMessage, CommandSendPermit) async throws -> Void

    /// The link minor from which the Core answers a write with a `writeId`
    /// with `property.result` (link document section 7.3).
    public static let propertyResultMinor: UInt16 = 5

    /// True once the Core has sent `snapshot.complete` in this session.
    @Published public private(set) var isSnapshotComplete = false
    /// True from a lost link until the next snapshot completes; the values
    /// stay readable meanwhile.
    @Published public private(set) var isStale = false
    /// The Core's capabilities, the last set it sent, by name.
    @Published public private(set) var capabilities: [String: MirrorValue] = [:]
    /// The keys of the mirrored objects, in the order the Core created them.
    @Published public private(set) var objectKeys: [String] = []
    /// The Core's latest telemetry sample that the link allows the app to read.
    @Published public private(set) var metrics: StationMetrics?
    /// Last accepted sample with phone arrival time, retained as history across
    /// a lost link or a new snapshot. Use `currentTelemetryReceipt` for live data.
    @Published public private(set) var latestTelemetryReceipt: StationTelemetryReceipt?
    /// Advances at each accepted authentication, including a reconnect to a
    /// different Core. A receipt from an earlier identity is historical.
    public private(set) var snapshotIdentity: UInt64 = 0
    /// Advances when a schema arrives or a new snapshot retires the prior
    /// session's schema identities. Consumers can observe schema-only changes.
    @Published public private(set) var schemaRevision: UInt64 = 0

    /// Advances for each actual SWR sample, including an unchanged reading.
    @Published public private(set) var txSwrSampleSerial: UInt64 = 0
    /// A valid, fresh transmit SWR reading from this session, otherwise nil.
    public var currentTxSwr: Double? {
        guard isSnapshotComplete, !isStale, (!hasManagedLinkState || linkUp),
              let sample = txSwrSample, sample.identity == snapshotIdentity,
              let state = objectsByKey["txState"], Self.txOnAir(state) else { return nil }
        let age = clock.nowMilliseconds - sample.observedAt
        return age >= 0 && age <= 2_000 ? sample.value : nil
    }

    private struct TxSwrSample {
        let value: Double
        let observedAt: Int64
        let identity: UInt64
    }
    private var txSwrSample: TxSwrSample?

    private static func txOnAir(_ state: MirrorObject) -> Bool {
        state["keyed"] == .bool(true) || state["tuning"] == .bool(true) || state["twoTone"] == .bool(true)
    }

    /// Receipt time belongs to the message, including equal-valued deltas;
    /// observing only MirrorObject.values would miss those fresh samples.
    private func recordTxSwr(_ key: String, changes: [String: MirrorValue]) {
        guard key == "txState", let state = objectsByKey[key] else { return }
        if changes["swr"] != nil { txSwrSampleSerial &+= 1 }
        guard Self.txOnAir(state) else { txSwrSample = nil; return }
        guard let value = changes["swr"] else { return }
        guard case .double(let reading) = value, reading.isFinite, reading >= 1 else {
            txSwrSample = nil
            return
        }
        txSwrSample = TxSwrSample(value: reading, observedAt: clock.nowMilliseconds, identity: snapshotIdentity)
    }

    /// The link minor agreed with the Core: the lower of the two.
    public private(set) var agreedMinor: UInt16?

    /// How long a held write waits for the Core's answer before it is
    /// reported not confirmed. The desktop has no such bound (its pending
    /// writes end only by an answer, `StationClient.cpp:4216-4268`, or a
    /// lost link, `:2219`), so this is the phone's own command timeout
    /// (`ModesTabModel.commandTimeout`, `BandSlicesModel.commandTimeout`).
    /// The pending identity stays; presentation returns to latest Core.
    public static let answerDeadline: Duration = .seconds(5)

    /// The answer to a held write that came after its outcome was handed
    /// back as not confirmed, or the lost link that ended it.
    public struct LateAnswer: Equatable, Sendable {
        public let key: String
        public let property: String
        public let outcome: PropertyWriteOutcome

        public init(key: String, property: String, outcome: PropertyWriteOutcome) {
            self.key = key
            self.property = property
            self.outcome = outcome
        }
    }

    /// Answers, and lost links, of writes already reported not confirmed.
    public let lateAnswers = PassthroughSubject<LateAnswer, Never>()

    /// The held properties, `key.property`, whose latest write has had no
    /// answer within answerDeadline.
    @Published public private(set) var unconfirmedWrites: Set<String> = []

    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.store")

    private let send: Sender
    private let clock: any LinkClock
    private var schemas: [String: [String: LinkMessage.SchemaField]] = [:]
    private var currentSchemaIdentities: [String: UInt64] = [:]
    private var nextSchemaIdentity: UInt64 = 0
    private var objectsByKey: [String: MirrorObject] = [:]
    /// Objects of an earlier session the new snapshot has not created yet.
    private var unconfirmed: Set<String> = []
    private var linkUp = false
    private var hasManagedLinkState = false
    private var telemetry = StationTelemetryDecoder()

    private struct PendingWrite {
        let key: String
        let property: String
        let requested: MirrorValue
        /// Answered by `property.result`; otherwise by the next `delta`.
        let byResult: Bool
        /// Order of sending, for the oldest write a `delta` answers.
        let sequence: UInt64
        /// Nil once the outcome was handed back as not confirmed.
        var resolve: CheckedContinuation<PropertyWriteOutcome, Never>?
        /// The operator's value shows until the answer (`holds`).
        let held: Bool
        let edit: UInt64?
        let onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)?
        var deadline: (any LinkTimer)?
        let deadlineAuthority: CommandSendPermit?
    }

    /// The operator's value for one property, shown from the touch until the
    /// Core answers the latest write of it (the desktop's `PendingWrite`).
    private struct Hold {
        var value: MirrorValue
        var edit: UInt64
        /// The latest write sent of this property.
        var latestWriteId: UInt32?
        /// True while `value` waits its turn behind a write not answered.
        var unsent: Bool
        /// The Core's last value of the property, nil when it has none:
        /// shown again on a refusal without a value, an unsent write or a
        /// lost link.
        var core: MirrorValue?
        /// A value a command changes, which the Core took (or did not
        /// answer in time): it stays until the Core's next value of it.
        var untilDelta = false
    }

    private var nextEdit: UInt64 = 0
    /// Current edits remain known after their hold settles, so an older
    /// result cannot become current again (StationClient.cpp:4229-4243).
    private var currentEdits: [String: UInt64] = [:]
    private var nextWriteId: UInt32 = 1
    private var nextSequence: UInt64 = 0
    /// Writes waiting for their answer, by `writeId`.
    private var pendingWrites: [UInt32: PendingWrite] = [:]
    /// The operator's values shown over the Core's, by key and property.
    private var holds: [String: [String: Hold]] = [:]

    public init(send: @escaping Sender, clock: any LinkClock = SystemLinkClock()) {
        self.send = send
        self.clock = clock
    }

    /// A store that sends through `session`. Feed it the session's events
    /// with `handle(_:)`.
    public convenience init(session: StationSession) {
        self.init(send: { message in try await session.send(message) })
    }

    // MARK: Reading

    /// The receipt from this completed, connected snapshot, if it has one.
    /// Synchronous guards also cover the interval before a queued UI refresh.
    public var currentTelemetryReceipt: StationTelemetryReceipt? {
        guard isSnapshotComplete, !isStale, (!hasManagedLinkState || linkUp),
              let receipt = latestTelemetryReceipt,
              receipt.snapshotIdentity == snapshotIdentity else { return nil }
        return receipt
    }

    /// The object with `key`, if the Core has created it.
    public func object(_ key: String) -> MirrorObject? {
        objectsByKey[key]
    }

    /// Every object of one class, in the order the Core created them.
    public func objects(ofClass className: String) -> [MirrorObject] {
        objectKeys.compactMap { objectsByKey[$0] }.filter { $0.className == className }
    }

    /// The names of a class's properties, from its schema.
    public func propertyNames(ofClass className: String) -> [String]? {
        schemas[className].map { $0.values.sorted { $0.ordinal < $1.ordinal }.map(\.name) }
    }

    /// Identity of a class schema received during this snapshot/session.
    /// The retained `propertyNames` cache may still describe an older session.
    public func currentSessionSchemaIdentity(ofClass className: String) -> UInt64? {
        currentSchemaIdentities[className]
    }

    /// Ordered fields only when the Core sent this class's schema in the
    /// current session; nil after a new snapshot until it does so again.
    public func currentSessionPropertyNames(ofClass className: String) -> [String]? {
        guard currentSchemaIdentities[className] != nil else { return nil }
        return propertyNames(ofClass: className)
    }

    /// A capability's whole-number value, or 0 when the Core did not send it.
    public func capabilityVersion(_ name: String) -> Int64 {
        switch capabilities[name] {
        case .int(let value)?, .enumeration(let value)?:
            return value
        default:
            return 0
        }
    }

    /// True when the Core answers writes with `property.result`: agreed
    /// minor 5 or more and `propertyResultVersion` 1 or more.
    public var propertyResultsAvailable: Bool {
        (agreedMinor ?? 0) >= Self.propertyResultMinor && capabilityVersion("propertyResultVersion") >= 1
    }

    // MARK: Feeding

    /// One session event: a message, or a change of state (a lost link
    /// makes the mirror stale).
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(let message):
            apply(message)
        case .stateChanged(let state):
            hasManagedLinkState = true
            let up = state == .receivingSnapshot || state == .ready
            let wasUp = linkUp
            linkUp = up
            if wasUp && !up {
                linkLost()
            }
        case .refused:
            break
        }
    }

    /// One message from the Core.
    public func apply(_ message: LinkMessage) {
        switch message {
        case .hello(let hello):
            agreedMinor = min(LinkVersionPolicy.minor, hello.minor)
        case .authResult(let result):
            if result.accepted {
                beginSnapshot()
            }
        case .capabilities(let set):
            // Every capabilities message is a whole new set (section 5.2).
            var next: [String: MirrorValue] = [:]
            for entry in set.properties {
                next[entry.name] = MirrorValue(entry.value)
            }
            if next != capabilities {
                capabilities = next
            }
        case .schema(let schema):
            var fields: [String: LinkMessage.SchemaField] = [:]
            for field in schema.fields {
                fields[field.name] = field
            }
            schemas[schema.className] = fields
            nextSchemaIdentity &+= 1
            currentSchemaIdentities[schema.className] = nextSchemaIdentity
            schemaRevision &+= 1
        case .objectCreate(let create):
            self.create(create)
        case .objectDestroy(let destroy):
            remove(destroy.key)
        case .delta(let delta):
            applyDelta(delta)
        case .propertyResult(let result):
            applyResult(result)
        case .snapshotComplete:
            for key in unconfirmed {
                remove(key)
            }
            unconfirmed = []
            isSnapshotComplete = true
            isStale = false
        case .stationMetrics(let sample):
            if let decoded = telemetry.decode(sample, agreedMinor: agreedMinor ?? 0,
                                              telemetryVersion: capabilityVersion("stationTelemetryVersion")) {
                metrics = decoded
                if snapshotIdentity != 0 {
                    latestTelemetryReceipt = StationTelemetryReceipt(
                        metrics: decoded, observedAtMilliseconds: clock.nowMilliseconds,
                        snapshotIdentity: snapshotIdentity)
                }
            }
        default:
            break
        }
    }

    // MARK: Writing

    /// Asks the Core to set one property of one object, and returns its
    /// answer. When property results were negotiated (agreed minor 5 or more
    /// and `propertyResultVersion` 1 or more) the write carries a fresh
    /// nonzero `writeId` and its outcome comes from the matching
    /// `property.result`. Otherwise it carries no `writeId`, since an older
    /// Core may not read one (link document section 7.3), and its outcome
    /// comes from the next `delta` for that property.
    ///
    /// With `holdsOperatorValue` the mirror shows the value at once and keeps
    /// it until the answer (the class comment), and an answer that does not
    /// come within ``answerDeadline`` resolves as not confirmed. Without it
    /// the mirror changes only when the Core says so and the write waits for
    /// its answer however long it takes: the transmit keying path's own way.
    public func write(_ key: String, property: String, value: MirrorValue,
                      holdsOperatorValue: Bool = true, edit: UInt64? = nil,
                      onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        await writePrepared(key, property: property, value: value, held: holdsOperatorValue, send: self.send,
                            edit: edit, onLateOutcome: onLateOutcome)
    }

    /// VOX writes with a captured session and revocable authority. It keeps
    /// ordinary write IDs, result matching, and pending-write retirement.
    public func writeBound(_ key: String, property: String, value: MirrorValue,
                           sender: @escaping BoundSender, authority: CommandSendPermit,
                           holdsOperatorValue: Bool = true, edit: UInt64? = nil,
                           expiresAuthorityAtDeadline: Bool = false,
                           onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        await writePrepared(key, property: property, value: value, held: holdsOperatorValue,
                            send: { message in try await sender(message, authority) }, edit: edit,
                            deadlineAuthority: expiresAuthorityAtDeadline ? authority : nil,
                            onLateOutcome: onLateOutcome)
    }

    /// Retires one authority-bound touch after its participant/session was revoked.
    /// Its late answer remains matched, but cannot overwrite a newer Core presentation or touch.
    public func retireBoundEdit(_ key: String, property: String, edit: UInt64) {
        guard isCurrent(key, property: property, edit: edit) else { return }
        if let hold = holds[key]?[property], hold.edit == edit {
            restoreCore(key, property: property, hold: hold)
        }
        currentEdits[Self.slot(key, property)] = nil
        setUnconfirmed(key, property, false)
    }

    /// Shows the operator's value of one property before its write is sent:
    /// a newer value waiting its turn behind a write the Core has not
    /// answered. It replaces any older value shown and stays until its own
    /// write is answered or the link drops.
    @discardableResult
    public func hold(_ key: String, property: String, value: MirrorValue) -> UInt64? {
        guard let object = objectsByKey[key] else {
            return nil
        }
        nextEdit &+= 1
        currentEdits[Self.slot(key, property)] = nextEdit
        setUnconfirmed(key, property, false)
        let shown = MirrorValue(entry(key, property, value).value)
        var hold = holds[key]?[property] ?? Hold(value: shown, edit: nextEdit, latestWriteId: nil, unsent: true, core: object[property])
        hold.value = shown
        hold.edit = nextEdit
        hold.unsent = true
        hold.untilDelta = false
        holds[key, default: [:]][property] = hold
        object.assign([property: shown])
        return nextEdit
    }

    public func isCurrent(_ key: String, property: String, edit: UInt64) -> Bool {
        currentEdits[Self.slot(key, property)] == edit
    }

    /// The Core answered the command that sends a value
    /// ``hold(_:property:value:)`` showed, for a value that changes by
    /// command rather than by property write: refused, the Core's value
    /// shows again; taken, the operator's value stays until the Core's next value of the property arrives.
    public func commandAnswered(_ key: String, property: String, accepted: Bool) {
        setUnconfirmed(key, property, false)
        guard var hold = holds[key]?[property], hold.unsent, hold.latestWriteId == nil else {
            return
        }
        setUnconfirmed(key, property, false)
        guard accepted else {
            releaseUnsent(key, property: property)
            return
        }
        if hold.core == hold.value {
            // The Core's value came while the command waited and is the
            // operator's: nothing is left to hold.
            holds[key]?[property] = nil
            if holds[key]?.isEmpty == true {
                holds[key] = nil
            }
            return
        }
        hold.untilDelta = true
        holds[key]?[property] = hold
    }

    /// The Core did not answer, in time, the command that sends a value
    /// ``hold(_:property:value:)`` showed: the latest Core value shows,
    /// marked not confirmed (``isUnconfirmed(_:property:)``), until the
    /// command answer or session retirement. Deltas cannot answer it.
    public func commandNotConfirmed(_ key: String, property: String) {
        guard let hold = holds[key]?[property], hold.unsent, hold.latestWriteId == nil else { return }
        restoreCore(key, property: property, hold: hold)
        setUnconfirmed(key, property, true)
    }

    /// Expiry ends the presentation hold, not the pending answer's identity.
    private func restoreCore(_ key: String, property: String, hold: Hold) {
        holds[key]?[property] = nil
        if holds[key]?.isEmpty == true { holds[key] = nil }
        objectsByKey[key]?.assign([property: hold.core])
    }

    /// Lets go of a value hold(_:property:value:) showed that will not
    /// be sent: the latest write of it still waiting shows again, else the
    /// Core's value.
    public func releaseUnsent(_ key: String, property: String) {
        guard var hold = holds[key]?[property], hold.unsent else {
            return
        }
        setUnconfirmed(key, property, false)
        if let id = hold.latestWriteId, let pending = pendingWrites[id], pending.held {
            hold.unsent = false
            hold.value = pending.requested
            holds[key]?[property] = hold
            setUnconfirmed(key, property, pending.resolve == nil)
            objectsByKey[key]?.assign([property: hold.value])
            return
        }
        holds[key]?[property] = nil
        if holds[key]?.isEmpty == true {
            holds[key] = nil
        }
        objectsByKey[key]?.assign([property: hold.core])
    }

    /// True while the latest write of a held property has had no answer
    /// within ``answerDeadline``.
    public func isUnconfirmed(_ key: String, property: String) -> Bool {
        unconfirmedWrites.contains(Self.slot(key, property))
    }

    private func writePrepared(_ key: String, property: String, value: MirrorValue, held: Bool,
                               send: @escaping Sender, edit: UInt64? = nil, deadlineAuthority: CommandSendPermit? = nil,
                               onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        let byResult = propertyResultsAvailable
        // Also the key a delta-answered write waits under; it goes on the
        // wire only when results were negotiated.
        let writeId = allocateWriteId()
        let entry = entry(key, property, value)
        let requested = MirrorValue(entry.value)
        let message = LinkMessage.propertyWrite(LinkMessage.PropertyWrite(key: key,
                                                                          writeId: byResult ? writeId : nil,
                                                                          properties: [entry]))
        let holding = held && objectsByKey[key] != nil
        let admittedEdit: UInt64?
        if holding {
            // A queue carries the identity made at the touch across Task
            // admission. A direct write makes its own identity here.
            admittedEdit = edit ?? (holds[key]?[property].flatMap {
                $0.unsent && $0.value == requested ? $0.edit : nil
            }) ?? hold(key, property: property, value: requested)
            holdSent(key, property, requested, writeId: writeId, edit: admittedEdit!)
        } else {
            admittedEdit = nil
        }
        return await withCheckedContinuation { continuation in
            nextSequence += 1
            pendingWrites[writeId] = PendingWrite(key: key, property: property, requested: requested,
                                                  byResult: byResult, sequence: nextSequence, resolve: continuation,
                                                  held: holding, edit: admittedEdit,
                                                  onLateOutcome: onLateOutcome, deadline: nil, deadlineAuthority: deadlineAuthority)
            if holding {
                pendingWrites[writeId]?.deadline = clock.schedule(after: Self.answerDeadline) { [weak self] in
                    await self?.deadlinePassed(writeId)
                }
            }
            Task { @MainActor in
                do {
                    try await send(message)
                } catch {
                    Self.logger.info("A property write was not sent: \(String(describing: error), privacy: .public)")
                    self.notSent(writeId)
                }
            }
        }
    }

    // MARK: Inside

    private static func slot(_ key: String, _ property: String) -> String {
        key + "." + property
    }

    /// One property as the Core's schema types it.
    private func entry(_ key: String, _ property: String, _ value: MirrorValue) -> LinkMessage.PropertyEntry {
        let field = objectsByKey[key].flatMap { schemas[$0.className]?[property] }
        return LinkMessage.PropertyEntry(ordinal: field?.ordinal ?? 0, name: property,
                                         value: field.map { value.wireValue(as: $0.kind) } ?? value.wireValue)
    }

    private func allocateWriteId() -> UInt32 {
        while pendingWrites[nextWriteId] != nil {
            nextWriteId = nextWriteId == UInt32.max ? 1 : nextWriteId + 1
        }
        let id = nextWriteId
        nextWriteId = nextWriteId == UInt32.max ? 1 : nextWriteId + 1
        return id
    }

    private func beginSnapshot() {
        txSwrSample = nil
        // A new session: whatever this app wrote in the last one goes
        // unconfirmed (the desktop's lost link, StationClient.cpp:2219).
        if !pendingWrites.isEmpty || !holds.isEmpty {
            retireWrites()
        }
        snapshotIdentity &+= 1
        isSnapshotComplete = false
        currentSchemaIdentities = [:]
        schemaRevision &+= 1
        unconfirmed = Set(objectKeys)
        telemetry.reset()
    }

    private func create(_ create: LinkMessage.ObjectCreate) {
        if create.key == "txState" { txSwrSample = nil }
        var values: [String: MirrorValue] = [:]
        for entry in create.properties {
            values[entry.name] = MirrorValue(entry.value)
        }
        unconfirmed.remove(create.key)
        if let existing = objectsByKey[create.key], existing.className == create.className {
            // The operator's values stay over the Core's until their writes
            // are answered (the desktop's restoreOperatorValues,
            // StationClient.cpp:4468-4525).
            var shown = values
            for (name, hold) in holds[create.key] ?? [:] {
                holds[create.key]?[name]?.core = values[name]
                shown[name] = hold.value
            }
            existing.replace(shown)
            recordTxSwr(create.key, changes: values)
            return
        }
        dropHolds(create.key)
        let object = MirrorObject(key: create.key, className: create.className, values: values)
        if objectsByKey[create.key] == nil {
            objectKeys.append(create.key)
        }
        objectsByKey[create.key] = object
        recordTxSwr(create.key, changes: values)
    }

    private func remove(_ key: String) {
        if key == "txState" { txSwrSample = nil }
        guard objectsByKey.removeValue(forKey: key) != nil else {
            return
        }
        dropHolds(key)
        objectKeys.removeAll { $0 == key }
    }

    private func dropHolds(_ key: String) {
        guard let properties = holds.removeValue(forKey: key) else {
            return
        }
        for name in properties.keys {
            setUnconfirmed(key, name, false)
        }
    }

    private func applyDelta(_ delta: LinkMessage.Delta) {
        var changes: [String: MirrorValue] = [:]
        for entry in delta.properties {
            changes[entry.name] = MirrorValue(entry.value)
        }
        guard let object = objectsByKey[delta.key] else {
            Self.logger.info("Ignoring a change to \(delta.key, privacy: .public), which the Core has not created")
            return
        }
        var shown = changes
        for (name, value) in changes {
            let waiting = pendingWrites.filter { !$0.value.byResult && $0.value.key == delta.key && $0.value.property == name }
            if let oldest = waiting.min(by: { $0.value.sequence < $1.value.sequence }) {
                // Without results, this delta is the oldest write's answer.
                let pending = oldest.value
                pendingWrites.removeValue(forKey: oldest.key)
                pending.deadline?.cancel()
                let outcome = value == pending.requested
                    ? PropertyWriteOutcome(accepted: true, reason: "", value: value)
                    : .keptOther(value)
                if isCurrent(pending) { setUnconfirmed(delta.key, name, false) }
                if holds[delta.key]?[name] != nil {
                    shown[name] = nil
                    settle(pending, writeId: oldest.key, outcome: outcome)
                }
                finish(pending, outcome)
            } else if holds[delta.key]?[name]?.untilDelta == true {
                // The Core's value after the command that sent it.
                holds[delta.key]?[name] = nil
                setUnconfirmed(delta.key, name, false)
                if holds[delta.key]?.isEmpty == true {
                    holds[delta.key] = nil
                }
            } else if holds[delta.key]?[name] != nil {
                // Skipped while the operator's write holds it, kept as the
                // Core's value (StationClient::handleDelta, :4196-4206).
                holds[delta.key]?[name]?.core = value
                shown[name] = nil
            }
        }
        object.merge(shown)
        recordTxSwr(delta.key, changes: changes)
    }

    private func applyResult(_ result: LinkMessage.PropertyResult) {
        guard let pending = pendingWrites[result.writeId], pending.byResult, pending.key == result.key else {
            Self.logger.info("Ignoring an answer to a write this app is not waiting for")
            return
        }
        pendingWrites.removeValue(forKey: result.writeId)
        pending.deadline?.cancel()
        var kept: [String: MirrorValue] = [:]
        for entry in result.results {
            if let value = entry.value {
                kept[entry.property] = MirrorValue(value.value)
            }
        }
        let outcome: PropertyWriteOutcome
        if let answer = result.results.first(where: { $0.property == pending.property }) {
            outcome = PropertyWriteOutcome(accepted: answer.accepted, reason: answer.reason,
                                           value: answer.value.map { MirrorValue($0.value) })
        } else {
            outcome = PropertyWriteOutcome(accepted: false, reason: "The Core did not answer for this setting.",
                                           value: nil)
        }
        guard isCurrent(pending) else {
            // The waiter still receives its answer, marked obsolete, but
            // it cannot replace a newer completed value or current note.
            finish(pending, outcome.superseded())
            return
        }
        setUnconfirmed(pending.key, pending.property, false)
        var shown = kept
        for (name, value) in kept where holds[result.key]?[name] != nil {
            shown[name] = nil
            if name != pending.property {
                holds[result.key]?[name]?.core = value
            }
        }
        if holds[result.key]?[pending.property] != nil {
            shown[pending.property] = nil
            settle(pending, writeId: result.writeId, outcome: outcome)
        }
        objectsByKey[result.key]?.merge(shown)
        finish(pending, outcome)
    }

    /// Shows the operator's value of a write that is leaving.
    private func holdSent(_ key: String, _ property: String, _ value: MirrorValue, writeId: UInt32, edit: UInt64) {
        guard let object = objectsByKey[key] else {
            return
        }
        var hold = holds[key]?[property] ?? Hold(value: value, edit: edit, latestWriteId: nil, unsent: false, core: object[property])
        guard hold.edit == edit else { return }
        hold.value = value
        hold.latestWriteId = writeId
        hold.unsent = false
        holds[key, default: [:]][property] = hold
        setUnconfirmed(key, property, false)
        object.assign([property: value])
    }

    /// The Core answered write `writeId` of a held property. The answer to
    /// the latest write decides what shows: the Core's kept value, else the
    /// operator's when accepted and the Core's last when not. An older
    /// write's answer only updates the Core's value behind a newer one.
    private func settle(_ pending: PendingWrite, writeId: UInt32, outcome: PropertyWriteOutcome) {
        guard isCurrent(pending), var hold = holds[pending.key]?[pending.property] else {
            return
        }
        if let value = outcome.value {
            hold.core = value
        }
        guard hold.latestWriteId == writeId, !hold.unsent else {
            holds[pending.key]?[pending.property] = hold
            return
        }
        if outcome.heldForQuestion {
            // Held for this phone's question, not refused: the operator's
            // value stays until the Core's next value of it.
            hold.latestWriteId = nil
            hold.untilDelta = true
            holds[pending.key]?[pending.property] = hold
            setUnconfirmed(pending.key, pending.property, false)
            return
        }
        let shown = outcome.value ?? (outcome.accepted ? hold.value : hold.core)
        holds[pending.key]?[pending.property] = nil
        if holds[pending.key]?.isEmpty == true {
            holds[pending.key] = nil
        }
        setUnconfirmed(pending.key, pending.property, false)
        objectsByKey[pending.key]?.assign([pending.property: shown])
    }

    /// Hands a write's outcome back, or to ``lateAnswers`` when it was
    /// already handed back as not confirmed.
    private func isCurrent(_ pending: PendingWrite) -> Bool {
        !pending.held || pending.edit == currentEdits[Self.slot(pending.key, pending.property)]
    }

    private func finish(_ pending: PendingWrite, _ outcome: PropertyWriteOutcome) {
        let current = isCurrent(pending)
        if let resolve = pending.resolve {
            resolve.resume(returning: current ? outcome : outcome.superseded())
        } else if current {
            pending.onLateOutcome?(outcome)
            lateAnswers.send(LateAnswer(key: pending.key, property: pending.property, outcome: outcome))
        }
    }

    private func notSent(_ writeId: UInt32) {
        guard let pending = pendingWrites.removeValue(forKey: writeId) else {
            return
        }
        pending.deadline?.cancel()
        // Expired authority may fail a held physical handoff after the five-second UI outcome.
        // That is no new Core answer and must leave the current not-confirmed marker intact.
        if pending.resolve == nil && pending.deadlineAuthority?.isRevoked == true { return }
        if pending.held {
            settle(pending, writeId: writeId, outcome: .notSent)
        }
        finish(pending, .notSent)
    }

    /// No answer within ``answerDeadline``: the latest Core value shows and
    /// the unresolved operation is marked; the answer, if it comes, goes to ``lateAnswers``.
    private func deadlinePassed(_ writeId: UInt32) {
        guard let pending = pendingWrites[writeId], let resolve = pending.resolve else {
            return
        }
        pending.deadlineAuthority?.revoke()
        pendingWrites[writeId]?.resolve = nil
        pendingWrites[writeId]?.deadline = nil
        Self.logger.info("""
            The Core has not answered a write of \(pending.key, privacy: .public).\(pending.property, privacy: .public) \
            within 5 s; the control returns to the Core's value, not confirmed
            """)
        if isCurrent(pending), let hold = holds[pending.key]?[pending.property], hold.latestWriteId == writeId, !hold.unsent {
            restoreCore(pending.key, property: pending.property, hold: hold)
            setUnconfirmed(pending.key, pending.property, true)
        }
        resolve.resume(returning: isCurrent(pending) ? .notConfirmed : .notConfirmed.superseded())
    }

    private func setUnconfirmed(_ key: String, _ property: String, _ on: Bool) {
        let slot = Self.slot(key, property)
        if on != unconfirmedWrites.contains(slot) {
            if on {
                unconfirmedWrites.insert(slot)
            } else {
                unconfirmedWrites.remove(slot)
            }
        }
    }

    private func linkLost() {
        txSwrSample = nil
        isSnapshotComplete = false
        if !objectKeys.isEmpty || !capabilities.isEmpty {
            isStale = true
        }
        retireWrites()
    }

    /// Every write still waiting ends unconfirmed and every held property
    /// shows the Core's last value again, as the desktop clears its pending
    /// writes on a lost link (StationClient.cpp:2219). Nothing is sent again.
    private func retireWrites() {
        let held = holds
        holds = [:]
        for (key, properties) in held {
            objectsByKey[key]?.assign(properties.mapValues { $0.core })
        }
        if !unconfirmedWrites.isEmpty {
            unconfirmedWrites = []
        }
        let waiting = pendingWrites.values.sorted { $0.sequence < $1.sequence }
        pendingWrites = [:]
        for pending in waiting {
            pending.deadline?.cancel()
            finish(pending, .linkLost)
        }
        currentEdits = [:]
    }
}
